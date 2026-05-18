# Firmware Architecture — Elderly Companion Robot

> Tổng quan kiến trúc firmware sau Phase 10. Tất cả driver + task được wired lại quanh một top-level FSM (task_behavior) điều phối hành vi cấp cao.
>
> **Version**: 1.0 — 2026-05-18
> **Phase**: 10 (HDSD)

---

## 1. 4-Layer Stack

```
┌────────────────────────────────────────────────────────────┐
│ Layer 4 — Cloud / Home Assistant (over MQTT, WiFi)          │
├────────────────────────────────────────────────────────────┤
│ Layer 3 — task_behavior                                     │
│           top-level FSM: IDLE / PATROL / RETURN_HOME /      │
│           DOCKED / SOS_ACTIVE / FAULT                       │
├────────────────────────────────────────────────────────────┤
│ Layer 2 — Domain tasks                                      │
│   nav  ptz  sensors  dock  audio  sos  oled  mqtt  vision   │
├────────────────────────────────────────────────────────────┤
│ Layer 1 — Drivers (HAL)                                     │
│   motor servo i2c hcsr04 mpu6050 audio_i2s sim800 battery   │
│   ir_dock ssd1306                                            │
├────────────────────────────────────────────────────────────┤
│ Layer 0 — ESP-IDF + FreeRTOS                                │
└────────────────────────────────────────────────────────────┘
```

**Direction of control**: lệnh đi xuống (behavior → tasks → drivers); event đi lên (sensor → fusion → events → behavior).

---

## 2. Task table (final)

| Task | File | Priority | Stack | Period | Vai trò |
|------|------|----------|-------|--------|---------|
| `task_behavior` | tasks/task_behavior | 6 | 4096 | 200ms | Top FSM, điều phối |
| `task_sensor_fusion` | tasks/task_sensor_fusion | 5 | 4096 | 50ms | IMU + 4× ultrasonic |
| `task_audio` | tasks/task_audio | 5 | 8192 | event | I2S record/play |
| `task_navigation` | tasks/task_navigation | 4 | 4096 | 20ms | Drive train + watchdog |
| `task_dock` | tasks/task_dock | 4 | 4096 | 100ms | Auto-dock FSM |
| `task_ptz` | tasks/task_ptz | 4 | 4096 | 20ms | Pan-tilt smooth motion |
| `task_sos` | tasks/task_sos | 3 | 6144 | event | SIM800L SOS dispatcher |
| `task_mqtt` | tasks/task_mqtt | 2 | 6144 | 1000ms | Publish state to broker |
| `task_oled` | tasks/task_oled | 1 | 3072 | 500ms | Status display |
| (HTTP server) | main/smoke_test | 5 | 8192 | event | Web UI + API |

Total: **9 FreeRTOS tasks + 1 HTTP worker**. CPU usage typical < 15% (mostly idle waiting on events).

---

## 3. Inter-task communication

### 3.1 Queues (latest-snapshot pattern)

| Queue | Producer | Consumers | Depth | Type |
|-------|----------|-----------|-------|------|
| `sensor_state` | task_sensor_fusion | task_navigation, task_oled, task_behavior, task_mqtt | 1 (overwrite) | `sensor_state_t` |

Single-slot overwrite queue — consumers peek the latest reading, no history. Avoids backlog when consumers are slow.

### 3.2 Event group bits

```c
EventGroupHandle_t robot_events = sensors_event_group();

// Sensor-fusion side raises these:
EVT_FALL_DETECTED   BIT0
EVT_OBSTACLE_FRONT  BIT1
EVT_OBSTACLE_BACK   BIT2
EVT_OBSTACLE_LEFT   BIT3
EVT_OBSTACLE_RIGHT  BIT4
EVT_FALL_CLEAR      BIT5

// Behavior side consumes them:
xEventGroupWaitBits(robot_events, EVT_FALL_DETECTED, ...)
```

### 3.3 NVS namespaces

| Namespace | Owner | Keys | Persistence |
|-----------|-------|------|-------------|
| `wifi` | wifi_manager | ssid, psk | per-device WiFi config |
| `servo` | servo_pwm | pan_off, tilt_off | mechanical trim |
| `nav` | task_navigation | ltrim, rtrim | per-side motor balance |
| `mpu6050` | mpu6050 | ax/ay/az/gx/gy/gz | IMU bias offsets |
| `sos` | task_sos | phone1, phone2, sms_text | emergency contacts |
| `mqtt` | mqtt_client | broker_uri, client_id | broker config (optional) |

---

## 4. Task spawn order (in `smoke_test_run`)

```
1.  WiFi STA          — depends on nothing
2.  Camera init       — depends on PSRAM (Phase 3)
3.  task_ptz_start    — servo_init() inside
4.  task_sensor_fusion_start  — must precede nav so obstacle gate has data
5.  task_navigation_start     — motor_init() inside; reads sensor queue
6.  task_audio_start          — i2s_init() inside
7.  task_sos_start            — SIM800L power-on takes 10-30s, runs background
8.  task_dock_start           — battery + ir_dock init inside
9.  task_oled_start           — ssd1306_init() over i2c_bus
10. task_mqtt_start  (only if broker_uri configured in NVS)
11. task_behavior_start       — must be LAST: depends on all sources above
12. HTTP server start         — exposes /behavior/* + everything below
```

Order rationale: each task is started only after the resources it depends on are ready. The behavior FSM is last because it issues commands to every other task.

---

## 5. Top-level behavior FSM (task_behavior)

```
                    ┌─────────────────────────────┐
                    │                              │
                    ▼                              │
              ┌──────────┐  /patrol or schedule    │
              │  IDLE    │ ──────────────────►  PATROL
              └─┬──────┬─┘                     ┌─┴───────────┐
                │      │                       │ random walk │
       battery_is_low()│                       │ + obstacle  │
                │      │                       │ avoidance   │
                ▼      │                       └─┬───────────┘
         ┌────────────┐│        battery_is_low() │
         │RETURN_HOME │◄────────────────────────┘
         └─────┬──────┘
               │ dock_state == CHARGED
               ▼
         ┌────────────┐
         │  DOCKED    │
         └─────┬──────┘
               │ user /behavior/leave or battery_is_full() + user
               ▼
              IDLE

  ────────────────────────────────────────────────────────────
  Any state  ◄── EVT_FALL_DETECTED ──► SOS_ACTIVE
                                       │
                                       │ wait 30s + EVT_FALL_CLEAR
                                       ▼
                                  previous state
```

### 5.1 Behavior priorities

1. **SOS_ACTIVE** preempts everything (fall = life-critical)
2. **RETURN_HOME** preempts PATROL (battery preservation)
3. **DOCKED** stays until battery full OR user manual

### 5.2 PATROL implementation

Simple random walk with obstacle-gated nav:
- Every 5s: pick random `linear` (-30..+50) and `angular` (-40..+40)
- Send velocity command
- Camera continues streaming
- Ultrasonic obstacle gate (Phase 6) handles avoidance

Phase 10 keeps PATROL deliberately dumb — future phases can add vision-based path planning.

---

## 6. MQTT topics (Layer 4 — Home Assistant)

Robot publishes:
```
elderly_robot/state          (1Hz)  JSON full state
elderly_robot/event/fall     (event) "1" when triggered
elderly_robot/event/battery  (event) "low" / "full"
elderly_robot/event/dock     (event) state transition
```

Robot subscribes:
```
elderly_robot/cmd/behavior   "idle" / "patrol" / "dock" / "leave"
elderly_robot/cmd/ptz        JSON {pan, tilt}
elderly_robot/cmd/say        text (TTS via voice pipeline — Phase 7+)
elderly_robot/cmd/sos        "trigger"
```

QoS = 1 for events (at-least-once delivery), QoS = 0 for state heartbeat (acceptable to drop).

Phase 10 ships MQTT **scaffolded but disabled** — set NVS `mqtt/broker_uri` to activate.

---

## 7. OLED status display

128×64 SSD1306 via shared I2C bus (0x3C). Refresh 2Hz:

```
┌────────────────────────┐
│ Elderly Bot         12s│  ← title + uptime
│ wifi: 192.168.1.123    │  ← IP
│ batt: 11.85V  68%      │  ← battery
│ state: PATROL          │  ← behavior FSM state
│ dock: IDLE             │  ← dock FSM state
│ sim800: READY          │  ← SOS subsystem state
│                        │
│ F  45  B  ---  L  120  │  ← ultrasonic distances
└────────────────────────┘
```

Visible without phone/laptop — useful for field debugging and elderly users to see "ai robot vẫn sống".

---

## 8. Health monitoring

`task_behavior` mỗi 10s check health của subsystems:

```c
typedef struct {
    bool sensor_fresh;    // sensor_state age < 200ms
    bool wifi_ok;
    bool sim800_ready;
    bool battery_critical; // < 10%
    bool oled_responding;
} health_t;
```

Health failure → log warning, publish event to MQTT, OLED flash. Critical failure (battery < 5%) → emergency-stop + force dock.

---

## 9. Memory budget (estimated)

| Region | Use |
|--------|-----|
| **PSRAM 8MB** | |
| 320KB | Audio record buffer |
| 60KB | Camera frame buffers (2× VGA JPEG) |
| ~50KB | WiFi/LWIP buffers |
| **rest free** | |
| | |
| **Internal RAM 512KB** | |
| ~50KB | All task stacks summed |
| ~30KB | Driver state structs |
| ~80KB | ESP-IDF system |
| | |
| **Flash 16MB** | |
| ~700KB | App image (.text + .rodata) |
| ~3MB | OTA slot B (currently unused) |
| ~10MB | SPIFFS |
| ~100KB | NVS + coredump |

Free heap typical: > 150KB internal RAM (verified in `/status` endpoint).

---

## 10. Failure modes & recovery

| Failure | Detection | Recovery |
|---------|-----------|----------|
| WiFi disconnect | event handler | auto-reconnect, watchdog brakes motor |
| MQTT broker down | task_mqtt heartbeat fail | exponential backoff retry |
| Sensor stale (>500ms) | task_behavior health check | log + flash OLED, continue cautiously |
| SIM800L lost reg | next AT command fails | re-power on next SOS trigger |
| Camera crash | webserver stream returns NULL fb | warning logged, stream resumes on next frame |
| Battery critical | battery_is_low() = true | force RETURN_HOME state |
| Dock fail | task_dock → DOCK_FAULT | OLED shows fault; user manual |

---

## 11. Build & flash

```bash
cd firmware/
pio run -e esp32-s3-cam              # build
pio run -e esp32-s3-cam -t upload    # flash
pio device monitor                   # serial console
```

OTA update via MQTT (Phase 12+): firmware blob streamed to partition app1, then `esp_ota_set_boot_partition`.

---

## 12. Where Phase 10 stops, Phase 11 picks up

Phase 10 = **all tasks running, behavior FSM coordinating**.

Phase 11 = **test & calibration** — verify all the cross-cutting scenarios:
- Fall during PATROL → SOS → recover
- Battery low during PATROL → auto-dock
- WiFi drop → motors brake, OLED shows offline
- 24h soak test

Phase 12 = **deploy & Home Assistant integration** — MQTT live, schedule patrols, family Telegram alerts.
