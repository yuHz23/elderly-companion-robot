# Firmware — Elderly Companion Robot

ESP-IDF v5 application chạy trên ESP32-S3-CAM N16R8.

## Build

Cài PlatformIO:
```bash
pip install -U platformio
```

Build + flash + monitor:
```bash
cd firmware/
pio run -e esp32-s3-cam              # build
pio run -e esp32-s3-cam -t upload    # flash via USB-C
pio device monitor -e esp32-s3-cam   # serial console
```

Hoặc dùng ESP-IDF native:
```bash
. ~/esp-idf/export.sh
idf.py build flash monitor
```

## Cấu trúc

```
firmware/
├── platformio.ini          # PlatformIO env config
├── partitions.csv          # 16MB flash layout (app0/app1/spiffs/coredump)
├── sdkconfig.defaults      # ESP-IDF config (PSRAM Octal, partition, brownout)
├── CMakeLists.txt          # ESP-IDF project root
├── main/                   # App entry
│   ├── main.c              #   app_main(), banner, NVS init
│   ├── smoke_test.{c,h}    #   Phase 3: WiFi + camera + MJPEG server
│   └── CMakeLists.txt
├── drivers/                # (Phase 4+) HAL — motor, servo, IMU, audio
├── tasks/                  # (Phase 10) FreeRTOS task modules
└── utils/                  # Shared utilities
    └── wifi_manager/       #   NVS creds + softAP portal
```

## Phase progression (theo HDSD)

| Phase | Trạng thái firmware |
|-------|---------------------|
| 3 | Smoke test (WiFi + camera + MJPEG) ← **HIỆN TẠI** |
| 4 | + Driver `servo_pwm` (LEDC 50Hz pan/tilt) |
| 5 | + Driver `motor_l298n` (LEDC + GPIO direction) |
| 6 | + Driver `mpu6050`, `hcsr04`, task `sensor_fusion` |
| 7 | + Driver `audio_i2s`, voice pipeline (XiaoZhi-style) |
| 8 | + Driver `sim800_uart`, SOS task |
| 9 | + Task `task_dock` (state machine) |
| 10 | Toàn bộ 8 FreeRTOS task khởi chạy từ `app_main` |
| 11 | Tests + calibration |
| 12 | + Driver `mqtt_client`, Home Assistant integration |

## WiFi setup lần đầu

Lần build đầu, edit `platformio.ini` để truyền credentials qua flag:

```ini
build_flags =
  -DWIFI_FALLBACK_SSID=\"YourHomeSSID\"
  -DWIFI_FALLBACK_PSK=\"YourHomePassword\"
```

Hoặc, sau khi Phase 4+ enable `wifi_manager_start_portal()`:
1. Robot mở softAP `elderly-bot-XXXX`
2. Connect từ phone → mở `http://192.168.4.1/`
3. Nhập SSID/PSK → robot lưu vào NVS, reboot vào STA

Credentials NVS sau persist qua nhiều lần flash app (chỉ mất khi `nvs_flash_erase`).

## Documentation liên quan

- Hardware spec MCU: `docs/hardware/mcu-core-spec.md`
- Bring-up procedure: `docs/hardware/mcu-bringup.md`
- Power tree: `docs/hardware/power-tree-spec.md`
- Pin mapping: `docs/hardware/pin-mapping.md`
- Master HDSD: `docs/HDSD-Lap-Rap-Robot.md`
