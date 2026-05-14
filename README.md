# Elderly Companion Robot

> Wheeled companion robot for monitoring elderly living alone — camera PTZ, voice interaction, auto-docking charge station.

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│           ELDERLY COMPANION ROBOT                │
├─────────────────────────────────────────────────┤
│ MCU:      ESP32-S3 N16R8 (16MB flash, 8MB PSRAM)│
│ Camera:   ESP32-CAM + 2x SG90 servo (pan-tilt)  │
│ Audio:    INMP441 I2S MEMS mic + MAX98357A amp   │
│           + 3W 40mm speaker                      │
│ Motors:   L298N driver + 2x BO motors + wheels  │
│ Power:    TP4056 + 3x 18650 battery pack         │
│ Dock:     IR beacon + copper contact charging    │
│ Display:  SSD1306 OLED (status indicators)       │
│ Sensors:  MPU6050 IMU + 4x HC-SR04 ultrasonic   │
│ Comms:    WiFi (ESP32-S3) + MQTT → Home Asst.    │
└─────────────────────────────────────────────────┘
```

## Key Features

- **PTZ Camera**: Pan-tilt servo mount for elderly monitoring
- **Voice I/O**: I2S MEMS microphone + amplified speaker for 2-way communication
- **Auto-Dock & Charge**: IR beacon navigation + copper contact charging station
- **Fall Detection**: MPU6050 IMU + camera-based human pose estimation
- **Obstacle Avoidance**: 4x ultrasonic sensors (front, back, left, right)
- **Home Assistant Integration**: MQTT-based control and monitoring
- **OLED Status Display**: Battery, WiFi, charging state

## Project Structure

```
elderly-companion-robot/
├── .github/workflows/     # CI/CD workflows
├── config/                # Configuration files (MQTT, WiFi, thresholds)
├── docs/
│   ├── hardware/          # Schematics, pin mapping, BOM
│   ├── firmware/           # Architecture, API docs
│   └── mechanical/        # Assembly guides, dock design
├── firmware/
│   ├── main/              # Main application entry
│   ├── drivers/            # Hardware drivers (motors, sensors, audio)
│   ├── tasks/              # FreeRTOS tasks (vision, voice, nav, dock)
│   └── utils/              # Utilities (MQTT, WiFi, OTA)
├── hardware/kicad/        # KiCad schematic & PCB
├── mechanical/             # 3D models, dock design
└── scripts/                # Build, flash, monitoring scripts
```

## Reference Projects

| Project | Feature Used | Link |
|---------|-------------|------|
| Watney Rover | Auto-dock chassis + copper contacts | https://github.com/nikivanov/watney |
| Rudra | PTZ camera mount + elderly care AI | https://github.com/aceta-minophen/Rudra |
| Caretaker | ESP32-CAM dock charging design | https://github.com/positron48/robot |
| osrf/autodock | ROS auto-docking algorithm | https://github.com/osrf/autodock |
| Desk Buddy | ESP32-S3 voice interaction | https://www.hackster.io/roboattic_Lab |

## Hardware BOM

| Component | Qty | Source/Notes |
|-----------|-----|-------------|
| ESP32-S3 N16R8 | 1 | Main MCU (already owned) |
| ESP32-CAM OV2640 | 1 | Camera module with PTZ |
| SG90 micro servo | 2 | Pan & tilt for camera |
| INMP441 I2S MEMS mic | 1 | Voice input |
| MAX98357A I2S amp | 1 | Speaker driver |
| 3W 40mm speaker | 1 | Voice output |
| L298N motor driver | 1 | Dual DC motor control |
| BO motor + wheel | 2 | Drive motors |
| TP4056 charge module | 1 | 18650 charging |
| 18650 battery | 3 | Power source |
| MPU6050 GY-521 | 1 | IMU (already owned) |
| HC-SR04 ultrasonic | 4 | Obstacle avoidance |
| SSD1306 OLED 128x64 | 1 | Status display (already owned) |
| IR LED + receiver | 2+2 | Dock beacon navigation |
| Copper contact plates | 2 | Dock charging contacts |

## Pin Mapping (ESP32-S3)

See `docs/hardware/pin-mapping.md` for detailed pin assignments.

## License

MIT