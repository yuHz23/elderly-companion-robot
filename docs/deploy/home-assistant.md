# Home Assistant Integration

> Tích hợp robot với Home Assistant (HASS) qua MQTT để family theo dõi và nhận thông báo qua Telegram.
>
> **Yêu cầu**: HASS chạy local (Hassio / Container / Supervised), Mosquitto broker.
> **Phase**: 12 (HDSD)

---

## 1. Topology

```
┌─────────────┐   MQTT     ┌──────────────┐   Telegram   ┌─────────┐
│   Robot     │ ─────────► │ Mosquitto    │              │ Family  │
│ ESP32-S3    │ ◄───────── │  broker      │              │ phone   │
└─────────────┘            └──────┬───────┘              └────▲────┘
                                  │                            │
                                  ▼                            │
                          ┌──────────────┐                     │
                          │ Home         │  ───── automation ──┘
                          │ Assistant    │
                          └──────────────┘
```

---

## 2. Mosquitto broker setup

### 2.1 Install (Hassio)

Settings → Add-ons → "Mosquitto broker" → Install → Start.

### 2.2 Create MQTT user

Configuration → People → Users → Add user:
- Username: `robot`
- Password: (strong random)
- Note: this user is for the MQTT broker, NOT for the HA UI

Settings → Add-ons → Mosquitto → "Configuration" tab:
```yaml
logins:
  - username: robot
    password: <strong-password>
anonymous: false
```

Restart broker.

### 2.3 Verify

From any device on LAN:
```bash
mosquitto_sub -h <hass-ip> -u robot -P <password> -t 'elderly_robot/#' -v
```

Phải im lặng (chưa có message). Subscribe OK.

---

## 3. Configure robot to use broker

Trên web UI robot, hoặc qua curl:

```bash
curl "http://<robot-ip>/mqtt/config?uri=mqtt://robot:<password>@<hass-ip>:1883"
```

Response: `{"ok":true,"reboot_required":true}`

Reboot robot:
```bash
# Manual: rút pin → cắm lại
# Or: chờ task_behavior reboot từ next OTA
```

Sau reboot, serial log:
```
I (xxx) mqtt: connecting to mqtt://robot:****@<hass-ip>:1883
I (xxx) mqtt: connected to broker
I (xxx) task_mqtt: publisher task started — 1 Hz
```

Verify trên broker:
```bash
mosquitto_sub -h <hass-ip> -u robot -P <password> -t 'elderly_robot/#' -v
# Should see: elderly_robot/state {"uptime_s":15,...}  every 1s
```

---

## 4. Home Assistant entities

### 4.1 `configuration.yaml`

```yaml
# Enable MQTT (Settings → Devices & Services → MQTT first, then this YAML)
mqtt:
  sensor:
    - name: "Robot Battery"
      unique_id: elderly_bot_battery
      state_topic: "elderly_robot/state"
      value_template: "{{ value_json.battery.pct }}"
      unit_of_measurement: "%"
      device_class: battery
      icon: mdi:battery

    - name: "Robot Battery Voltage"
      unique_id: elderly_bot_battery_v
      state_topic: "elderly_robot/state"
      value_template: "{{ value_json.battery.v }}"
      unit_of_measurement: "V"
      device_class: voltage

    - name: "Robot Uptime"
      unique_id: elderly_bot_uptime
      state_topic: "elderly_robot/state"
      value_template: "{{ value_json.uptime_s }}"
      unit_of_measurement: "s"

    - name: "Robot Behavior"
      unique_id: elderly_bot_behavior
      state_topic: "elderly_robot/state"
      value_template: "{{ value_json.behavior }}"

    - name: "Robot Dock State"
      unique_id: elderly_bot_dock
      state_topic: "elderly_robot/state"
      value_template: "{{ value_json.dock }}"

    - name: "Robot Tilt"
      unique_id: elderly_bot_tilt
      state_topic: "elderly_robot/state"
      value_template: "{{ value_json.sensors.tilt }}"
      unit_of_measurement: "°"

  binary_sensor:
    - name: "Robot Fall Detected"
      unique_id: elderly_bot_fall
      state_topic: "elderly_robot/event/fall"
      payload_on: "1"
      payload_off: "0"
      device_class: safety
      icon: mdi:run-fast

    - name: "Robot Charging"
      unique_id: elderly_bot_charging
      state_topic: "elderly_robot/state"
      value_template: "{{ value_json.battery.charging }}"
      payload_on: "true"
      payload_off: "false"
      device_class: battery_charging

  button:
    - name: "Robot Patrol"
      unique_id: elderly_bot_cmd_patrol
      command_topic: "elderly_robot/cmd/behavior"
      payload_press: "patrol"

    - name: "Robot Return Home"
      unique_id: elderly_bot_cmd_dock
      command_topic: "elderly_robot/cmd/behavior"
      payload_press: "dock"

    - name: "Robot Idle"
      unique_id: elderly_bot_cmd_idle
      command_topic: "elderly_robot/cmd/behavior"
      payload_press: "idle"

# Camera stream — robot's MJPEG endpoint
camera:
  - platform: mjpeg
    mjpeg_url: http://<robot-ip>/stream
    name: Elderly Robot Camera
```

Restart HASS (Settings → System → Restart).

### 4.2 Verify entities

Settings → Devices & Services → MQTT → Devices. Phải thấy 11 entity của `elderly_bot_*`.

Mở Lovelace, drag các entity vào dashboard:

```yaml
# Example dashboard card
type: entities
title: Elderly Robot
entities:
  - sensor.robot_battery
  - sensor.robot_battery_voltage
  - sensor.robot_behavior
  - sensor.robot_dock_state
  - binary_sensor.robot_charging
  - binary_sensor.robot_fall_detected
  - button.robot_patrol
  - button.robot_return_home
  - button.robot_idle
```

---

## 5. Telegram bot setup

### 5.1 Create bot

1. Mở Telegram, search `@BotFather`
2. `/newbot` → đặt tên (vd "Elderly Bot Alerts")
3. BotFather trả về **bot token** dạng `123456789:AAEx...`
4. Lưu token

### 5.2 Get your chat_id

1. Chat với bot mới (gửi `/start`)
2. Mở: `https://api.telegram.org/bot<TOKEN>/getUpdates`
3. Tìm `"chat":{"id":<NUMBER>` → đó là chat_id của bạn
4. (Optional) Tạo group cho family, add bot, lấy group chat_id (negative number)

### 5.3 HASS Telegram notify

`configuration.yaml`:
```yaml
telegram_bot:
  - platform: polling
    api_key: !secret telegram_token
    allowed_chat_ids:
      - !secret family_chat_id

notify:
  - name: family
    platform: telegram
    chat_id: !secret family_chat_id
```

`secrets.yaml`:
```yaml
telegram_token: "123456789:AAEx..."
family_chat_id: -987654321   # negative for groups
```

Restart HASS. Test:
- Developer Tools → Services → `notify.family`
- Service data: `{"message": "Test from HASS"}`
- Phone notification?

---

## 6. Automations

`automations.yaml`:

```yaml
# Critical: Fall detected
- alias: "Elderly Robot — Fall Alert"
  description: "Notify family via Telegram + voice"
  trigger:
    platform: state
    entity_id: binary_sensor.robot_fall_detected
    to: "on"
  action:
    - service: notify.family
      data:
        message: |
          ⚠️ NGƯỜI NHÀ BỊ NGÃ
          Thời gian: {{ now().strftime('%H:%M:%S %d/%m/%Y') }}
          Camera: http://<external-ip>/stream
          Robot battery: {{ states('sensor.robot_battery') }}%
    - service: tts.cloud_say  # optional — speak through HASS speaker
      data:
        entity_id: media_player.living_room
        message: "Cảnh báo, đã phát hiện ngã. Người nhà vui lòng kiểm tra."

# Battery low warning
- alias: "Elderly Robot — Battery Low"
  trigger:
    platform: numeric_state
    entity_id: sensor.robot_battery
    below: 20
  action:
    - service: notify.family
      data:
        message: "🔋 Robot pin còn {{ states('sensor.robot_battery') }}%, đang tự về sạc"

# Charging started — informational
- alias: "Elderly Robot — Started Charging"
  trigger:
    platform: state
    entity_id: binary_sensor.robot_charging
    to: "on"
  action:
    - service: persistent_notification.create
      data:
        title: "Robot docking"
        message: "Đã về dock và bắt đầu sạc"

# Robot stuck (RETURN_HOME quá 5 phút)
- alias: "Elderly Robot — Stuck Going Home"
  trigger:
    platform: state
    entity_id: sensor.robot_behavior
    to: "RETURN_HOME"
    for:
      minutes: 5
  action:
    - service: notify.family
      data:
        message: "Robot đang về dock 5 phút rồi mà chưa tới — có thể bị kẹt"

# Daily morning greeting (08:00)
- alias: "Elderly Robot — Morning Greeting"
  trigger:
    platform: time
    at: "08:00:00"
  action:
    - service: mqtt.publish
      data:
        topic: "elderly_robot/cmd/behavior"
        payload: "patrol"
```

---

## 7. Dashboard example

`lovelace-elderly-bot.yaml`:

```yaml
title: Elderly Care
views:
  - title: Robot
    cards:
      - type: picture-glance
        title: Live Camera
        camera_image: camera.elderly_robot_camera
        entities:
          - binary_sensor.robot_fall_detected
          - binary_sensor.robot_charging

      - type: gauge
        entity: sensor.robot_battery
        min: 0
        max: 100
        severity:
          green: 50
          yellow: 20
          red: 0

      - type: entities
        title: Control
        entities:
          - button.robot_patrol
          - button.robot_return_home
          - button.robot_idle
          - sensor.robot_behavior
          - sensor.robot_dock_state

      - type: history-graph
        title: Battery Today
        hours_to_show: 24
        entities:
          - sensor.robot_battery_voltage
```

---

## 8. Sanity checklist

- [ ] Mosquitto running, robot user authenticated
- [ ] Robot connected, `mosquitto_sub` shows `elderly_robot/state` every 1s
- [ ] HASS Settings → MQTT → 11 entities discovered
- [ ] Camera feed plays in HASS dashboard
- [ ] Button "Patrol" actually starts robot patrol
- [ ] Telegram bot responds to manual `notify.family` call
- [ ] Test fall (drop on cushion) → Telegram notification arrives < 30s

---

## 9. Maintenance

- **Backup HASS config** weekly (Settings → System → Backups)
- **Update HASS** monthly via OS supervisor
- **Battery degradation**: replace 18650 cells every 18-24 months (cycle count)
- **Re-calibrate** IMU mỗi 3 tháng (drift)

---

## 10. Next phase

Phase 12 hoàn thành = robot deployed at home. Long-term improvements (separate effort):
- Voice pipeline (wake word + Claude API + TTS) — xem `audio-spec.md` §7
- Vision-based path planning (skip random walk)
- Multi-user authentication trong HASS
- Encrypted MQTT (mqtts://) over Tailscale VPN
