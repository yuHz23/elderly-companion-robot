# MCU Bring-up Procedure

> Quy trình bring-up ESP32-S3-CAM N16R8 sau khi PCB power tree đã pass (Phase 2). Đến cuối Phase 3 phải xem được camera stream qua browser.
>
> **Tiền đề**: PCB power đã pass theo `power-bringup.md` (rail 5V/3.3V/4V đều ổn định).
>
> **Dụng cụ**: PC có port USB-C, cáp USB-C ↔ USB-C (data cable, không phải charge-only), terminal serial (`pio device monitor` hoặc `idf.py monitor`).

---

## Pre-flight

### Verify hardware (5 phút)

- [ ] Power tree đã pass Phase 2 (4 rail OK)
- [ ] ESP32-S3-CAM module đã hàn lên PCB hoặc cắm socket
- [ ] R_pullup GPIO0 (10kΩ) hiện diện
- [ ] **R_pulldown GPIO12 (10kΩ) hiện diện** — critical
- [ ] R_pullup EN (10kΩ) + tụ 100nF
- [ ] CC1 và CC2 trên USB-C đều có 5.1kΩ pull-down
- [ ] Nút BOOT + RESET (nếu có) hoạt động tactile

### Verify dev environment (10 phút)

```bash
# Cài PlatformIO Core
pip install -U platformio

# Verify
pio --version       # ≥ 6.x
```

Hoặc dùng ESP-IDF native (nếu thích):
```bash
# Cài ESP-IDF v5
git clone -b v5.3 https://github.com/espressif/esp-idf.git ~/esp-idf
~/esp-idf/install.sh
. ~/esp-idf/export.sh
idf.py --version
```

---

## Phase 3.A — First flash (10 phút)

### A.1 Connect board

- [ ] Cắm USB-C từ PC → ESP32-S3-CAM USB connector
- [ ] Đèn power LED trên module sáng (báo VBUS 5V OK)

### A.2 Verify USB enumeration

**Windows**:
```powershell
Get-PnpDevice -Class Ports | Where-Object Status -eq OK
# Phải thấy "USB Serial Device (COMx)"
```

**Linux/Mac**:
```bash
ls /dev/ttyACM*    # Linux
ls /dev/tty.usbmodem*  # macOS
```

**Nếu không thấy**:
- Kiểm tra cáp USB-C có hỗ trợ data (không phải charge-only)
- Đo điện áp CC1, CC2 — phải có ~1.6V (pull-up từ PC + pull-down 5.1kΩ)
- Manual boot mode: giữ nút BOOT + bấm RESET, nhả RESET, nhả BOOT → enter download mode

### A.3 Build & flash

Trong `firmware/`:

```bash
pio run -e esp32-s3-cam              # build
pio run -e esp32-s3-cam -t upload    # flash
```

Lần đầu build sẽ download esp-idf framework + esp32-camera lib — mất 5-10 phút.

**Output thành công**:
```
Building in release mode
Linking .pio/build/esp32-s3-cam/firmware.elf
Retrieving maximum program size .pio/build/esp32-s3-cam/firmware.elf
Checking size .pio/build/esp32-s3-cam/firmware.elf
RAM:   [=         ]   8.2% (used 26876 bytes from 327680 bytes)
Flash: [==        ]  18.4% (used 565392 bytes from 3145728 bytes)

Configuring upload protocol...
Connecting...
Writing at 0x00010000... (100%)
Hash of data verified.
Leaving... Hard resetting via RTS pin...
```

---

## Phase 3.B — Serial monitor (5 phút)

### B.1 Connect monitor

```bash
pio device monitor -e esp32-s3-cam
# Hoặc với baud rate explicit:
pio device monitor -b 115200 -p COM5
```

### B.2 Mong đợi output

```
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x1900
load:0x403c9700,len:0x4
load:0x403c9704,len:0xc28
load:0x403cc700,len:0x2f30
entry 0x403c9908

I (37) boot: ESP-IDF 5.3.0 2nd stage bootloader
I (37) boot: chip revision: v0.2
I (39) boot.esp32s3: Boot SPI Speed : 80MHz
I (43) boot.esp32s3: SPI Mode       : DIO
I (47) boot.esp32s3: SPI Flash Size : 16MB
...
I (210) cpu_start: Pro cpu up.
I (211) cpu_start: Application information:
I (216) cpu_start: Project name:     elderly_companion_robot
I (221) cpu_start: App version:      ...
I (240) heap_init: At 3FCEEE60 len 0001E2A0 (120 KiB): RAM
I (246) heap_init: At 3FD0D100 len 00002F00 (11 KiB): RAM
I (252) spi_flash: detected chip: gd
I (255) spi_flash: flash io: dio
I (260) psram: Found 8MB PSRAM device
I (264) psram: Speed: 80MHz
I (269) cpu_start: Starting scheduler on PRO CPU.

I (321) main: =========================================
I (321) main:   Elderly Companion Robot — Phase 3
I (321) main:   Build: May 18 2026 ...
I (321) main: =========================================
I (321) main: PSRAM initialized: 8 MB
W (321) smoke: No WiFi creds in NVS — using compile-time fallback.
I (321) smoke: Connecting to SSID: elderly-bot-setup
...
I (5450) smoke: Got IP: 192.168.1.123
I (5455) smoke: Camera sensor PID: 0x36 (OV3660)
I (5460) smoke: HTTP server up — visit http://<ip>/
I (5465) smoke: smoke test ready
```

### B.3 Kiểm tra critical lines

- [ ] `Pro cpu up.` — chip booted OK
- [ ] `Found 8MB PSRAM` — Octal PSRAM detect đúng
- [ ] `psram: Speed: 80MHz` — high speed mode
- [ ] `Camera sensor PID: 0x36` — OV3660 (hoặc 0x26 = OV2640)
- [ ] `Got IP: x.x.x.x` — WiFi connected

---

## Phase 3.C — Troubleshooting boot fails

### C.1 Reboot loop ngay sau "Pro cpu up"

```
rst:0x10 (RTCWDT_RTC_RESET)
```

**Nguyên nhân**: GPIO12 không pull-down → flash voltage select sai.
**Fix**: Hàn R 10kΩ giữa GPIO12 và GND. Test point GPIO12 → đo phải có liên kết 10kΩ với GND khi power off.

### C.2 PSRAM not found

```
I (xxx) psram: PSRAM ID read error: 0xffffffff
```

**Nguyên nhân**: sdkconfig sai mode (Quad thay vì Octal cho N16R8).
**Fix**: Verify `sdkconfig.defaults` có:
```
CONFIG_SPIRAM_MODE_OCT=y
```
Rebuild với `pio run -t fullclean && pio run`.

### C.3 Camera init failed

```
E (xxx) smoke: Camera init failed: 0x105
```

Code error 0x105 = ESP_ERR_NOT_FOUND (camera I2C không response).

**Nguyên nhân**:
- Module XiaoZhi variant khác (OV2640 thay vì OV3660) → pin DVP khác
- Camera ribbon cable lỏng (nếu module có)
- 5V VIN không đủ áp (< 4.8V) → camera analog die brownout

**Fix**:
- Đo 5V tại chân module = 4.95-5.05V
- Reseat ribbon cable
- Thử PIXFORMAT_RGB565 thay vì JPEG để loại trừ JPEG encoder issue

### C.4 WiFi không connect

```
W (xxx) smoke: WiFi disconnected, retry 1/5
...
E (xxx) smoke: WiFi failed — check SSID/PSK
```

**Nguyên nhân**:
- SSID/PSK fallback trong code không match home WiFi
- Anten yếu (chip antenna PCB cần ground plane đủ)
- 2.4GHz channel quá nhiễu

**Fix**:
- Build với SSID/PSK đúng: `pio run -e esp32-s3-cam --build-flag="-DWIFI_FALLBACK_SSID=\\\"YourSSID\\\"" --build-flag="-DWIFI_FALLBACK_PSK=\\\"YourPass\\\""`
- Hoặc dùng wifi_manager portal (cần code Phase 4+ enable)
- Đặt robot gần router để test

### C.5 USB-CDC không enumerate

PC không thấy COM port mới khi cắm.

**Nguyên nhân**:
- Cáp charge-only (chỉ VBUS+GND, không có D+/D-)
- CC1/CC2 thiếu 5.1kΩ pull-down → PC nghĩ là charger
- USB role wrong (host thay vì device)

**Fix**:
- Đổi cáp USB-C ↔ USB-C **data cable** (thường có "data" hoặc "3A" trong tên)
- Verify R 5.1k trên CC pins
- Build với `CONFIG_TINYUSB_CDC_ENABLED=y`

---

## Phase 3.D — Web UI verification

### D.1 Mở browser

URL: `http://<ip-from-log>/`

Mong đợi:
- Trang HTML đen với title "Elderly Bot — Phase 3 smoke test"
- Camera stream live (640×480 MJPEG)
- Link "JSON status"

### D.2 Test stream stability

- [ ] Stream chạy 5 phút liên tục không drop frame
- [ ] FPS ước tính ≥ 15 (visual smoothness)
- [ ] Latency < 500ms (di chuyển tay trước camera, đối chiếu với stream)

### D.3 Test status JSON

Mở `http://<ip>/status`:
```json
{
  "phase": 3,
  "free_heap": 230544,
  "min_free_heap": 198332,
  "psram_size_mb": 8,
  "uptime_s": 42
}
```

- [ ] `psram_size_mb` = 8
- [ ] `free_heap` > 150000 (đủ headroom)
- [ ] `uptime_s` tăng theo thời gian

---

## Phase 3.E — Stress test (15 phút)

### E.1 Long-run stream

- [ ] Mở browser stream
- [ ] Để chạy **30 phút liên tục**
- [ ] Cuối 30 phút:
  - Stream vẫn live
  - Module không quá nóng (< 60°C đo bằng tay)
  - Status JSON: `min_free_heap` > 100000 (không memory leak)
  - Không có log lỗi nghiêm trọng trong serial monitor

### E.2 Heat soak

- [ ] Đo nhiệt độ module bằng IR thermometer hoặc K-type:
  - ESP32-S3 die: < 70°C
  - PSRAM chip: < 60°C
  - Module overall: < 55°C ambient

Nếu > giới hạn → thiếu copper pour bottom dưới module, hoặc 5V rail sụt khi load → thêm cap.

### E.3 WiFi reconnect

- [ ] Tắt WiFi router 30 giây → bật lại
- [ ] Robot tự reconnect (xem serial log)
- [ ] Stream resume sau ~10s

---

## Phase 3 — Sign-off

Pass ALL của những điều sau là đủ:

- [ ] Module boot không loop
- [ ] PSRAM 8MB detected
- [ ] Camera PID hợp lệ (OV3660 = 0x36 hoặc OV2640 = 0x26)
- [ ] WiFi connect và got IP
- [ ] HTTP / camera stream / status JSON đều work
- [ ] 30 phút stream không drop / no leak
- [ ] Heat ổn định < 60°C
- [ ] WiFi reconnect tự động

→ **Tiếp theo**: Phase 4 — PTZ servo pan-tilt (2× SG90 PWM 50Hz, GPIO 44/45).
