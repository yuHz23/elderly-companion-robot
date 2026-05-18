# MCU Core Hardware Specification

> Hardware spec cho **ESP32-S3-CAM N16R8** core: boot/strapping pin, USB-C programmer interface, EN circuit, camera DVP.
>
> **Version**: 1.0 — 2026-05-18
> **Module**: ESP32-S3-CAM N16R8 (XiaoZhi variant, có sẵn OV3660 DVP)
> **Phase**: 3 (HDSD)

---

## 1. Module overview

ESP32-S3-CAM N16R8 module tích hợp:
- ESP32-S3 SoC dual-core 240MHz LX7
- **16MB Quad/Octal SPI flash** (N16 = 16MB)
- **8MB PSRAM Octal** (R8 = 8MB) — cần thiết cho frame buffer camera
- OV3660 / OV2640 camera kết nối qua DVP **nội bộ** (không expose pin DVP ra ngoài)
- LDO 3.3V on-board (input 5V)

**Khác biệt với ESP32-S3 generic**: pin DVP camera bị "claim" sẵn (GPIO 4, 5, 18, 19, 21, 22, 23, 25-27, 34-39 thường dùng cho DVP). Phải tham khảo datasheet module cụ thể để biết pin nào available.

### XiaoZhi ESP32-S3-CAM pinout (đã verify với memory)

GPIO available for external use (sau khi DVP claim camera):
- GPIO 0, 4, 5 — boot + dock IR
- GPIO 6-11 — L298N motor + UART2
- GPIO 12 (strapping — cần pull-down)
- GPIO 14, 16, 17, 12, 18-19, 21-22 (I2S, I2C, DVP — share-able where datasheet allows)
- GPIO 35-43 — HC-SR04 + servo
- GPIO 44-48 — servo + UART2 cho SIM800L

Xem `pin-mapping.md` cho danh sách đầy đủ.

---

## 2. Boot & Strapping pins

ESP32-S3 dùng 3 strapping pin để chọn boot mode khi chip reset:

| Pin | Pull default | Function khi RESET |
|-----|--------------|---------------------|
| GPIO0 | Pull-up nội bộ | LOW → USB download mode; HIGH → SPI boot từ flash |
| GPIO3 | Pull-up nội bộ | Liên quan JTAG/USB role select |
| GPIO46 | Pull-up nội bộ | Bộ flash voltage select (3.3V vs 1.8V) |

### 2.1 GPIO0 — Boot button

**Required circuit**:

```
    +3V3 ──┬── R_pullup 10kΩ ──┬── GPIO0 (ESP32-S3)
           │                    │
           │                ┌───┴───┐
           │                │ BOOT  │
           │                │ Button│
           │                └───┬───┘
           │                    │
           └────── GND ─────────┘
```

- Default: pull-up 10kΩ giữ GPIO0 HIGH → SPI boot
- Bấm BOOT: kéo GPIO0 LOW → vào USB download mode
- Nút BOOT tactile switch 4-pin

### 2.2 GPIO12 — **CRITICAL pull-down**

**LÝ DO**: ESP32-S3 N16R8 dùng Octal PSRAM ở 1.8V. GPIO12 là pin strapping ảnh hưởng flash voltage select. Nếu GPIO12 float → có thể chọn nhầm 3.3V flash → chip không boot.

**Required circuit**:

```
    GPIO12 ──┬── R_pulldown 10kΩ ── GND
             │
             │
            (route đến chức năng khác trong firmware:
             trong project này là I2S LRCLK)
```

**ĐÂY LÀ LỖI #1 PHỔ BIẾN** khi tự thiết kế ESP32-S3 board → reboot loop liên tục.

Khi firmware boot xong → set GPIO12 high không sao (strapping chỉ check tại moment reset).

### 2.3 EN pin — Reset circuit

```
    +3V3 ──┬── R_en 10kΩ ──┬── EN (ESP32-S3)
           │                │
           │            C_en 100nF
           │                │
           │            ┌───┴───┐
           │            │ RESET │  (optional, có thể bỏ)
           │            │ Button│
           │            └───┬───┘
           │                │
           └────── GND ─────┘
```

- Pull-up 10kΩ kéo EN HIGH (chip enabled)
- Tụ 100nF lọc noise + tạo RC delay reset (~1ms)
- Nút RESET (optional) → kéo EN LOW → reset chip

Tụ 100nF rất quan trọng — không có nó chip có thể bị "phantom reset" khi noise trên rail 3.3V.

---

## 3. USB-C programmer & console

ESP32-S3 có USB-OTG built-in (D+/D- pin), không cần CP210x/CH340 USB-to-serial converter như ESP32 generic.

### 3.1 USB-C connector wiring

```
USB-C connector:
    VBUS (5V) ──── đến rail 5V của robot (qua diode protect)
    GND       ──── GND
    D+        ──── ESP32-S3 GPIO20 (USB D+)
    D-        ──── ESP32-S3 GPIO19 (USB D-)
    CC1, CC2  ──── R_cc 5.1kΩ xuống GND (báo device mode)

    SBU1, SBU2 — NC (không dùng)
    Shield — connect GND qua ferrite bead BLM18 (filter EMI)
```

### 3.2 Tại sao R_cc 5.1kΩ?

USB-C spec: device side phải có 5.1kΩ pull-down trên CC1, CC2. Nếu thiếu → host PC không nhận diện device. Nhiều board DIY sai chỗ này.

### 3.3 USB role

ESP32-S3 USB-OTG có thể đóng vai trò:
- **Device** (đa số case): nối với PC để programming + Serial
- **Host** (advanced): kết nối thiết bị USB khác (không dùng trong project này)

Set trong sdkconfig:
```
CONFIG_TINYUSB_CDC_ENABLED=y
CONFIG_TINYUSB_CDC_RX_BUFSIZE=512
```

ESP32-S3 sẽ enumerate là **CDC ACM Serial device** trên PC — không cần driver, hiển thị là `/dev/ttyACM0` (Linux) hoặc COMx (Windows).

### 3.4 Auto-reset circuit (optional)

PlatformIO/idf.py tự reset chip để flash:

```
DTR ────────┬── transistor Q1 base
            │
        R1 10kΩ
            │
            └── EN pin

RTS ────────┬── transistor Q2 base
            │
        R2 10kΩ
            │
            └── GPIO0

Khi DTR=0 RTS=0 → reset & boot mode
Khi DTR=1 RTS=1 → boot SPI bình thường
```

**Nhưng** ESP32-S3 USB-OTG nội bộ thường tự handle reset → không cần transistor. PlatformIO tự gửi USB control packet.

Nếu không tự reset được: dùng nút BOOT + RESET manual (bấm RESET trước, nhả → bấm BOOT, nhả).

---

## 4. Power input

ESP32-S3-CAM N16R8 module có LDO 3.3V on-board. Input acceptable:

| Input | Source | Note |
|-------|--------|------|
| 5V VIN pin | Buck #1 output (5V rail) | Module có LDO 3.3V → tự cấp |
| 3.3V pin | KHÔNG dùng (skip) | Tránh bypass LDO on-board |
| USB-C VBUS | Khi cắm USB | Tự động override 5V VIN |

**Decoupling cần thiết** (xem `decoupling-network.md`):
- 470µF aluminium low-ESR sát chân 5V VIN của module
- 100nF X7R 0603 song song
- Nếu chân 3.3V có expose: thêm 10µF + 100nF (không bắt buộc)

---

## 5. Camera DVP (nội bộ module)

ESP32-S3-CAM module tích hợp camera nên DVP pin không expose ra ngoài. **Tuyệt đối không cố nối gì vào** GPIO sau:

| GPIO | DVP signal | Cảnh báo |
|------|------------|----------|
| 4 | SIOD (camera I2C SDA) | NHƯNG datasheet XiaoZhi tận dụng được — verify cụ thể |
| 5 | SIOC (camera I2C SCL) | NHƯNG datasheet — verify |
| 15 | Y2 (DVP data) | KHÔNG dùng |
| 18 | PCLK (DVP pixel clock) | KHÔNG dùng |
| 19 | HSYNC | KHÔNG dùng |
| 36-39 | YH/Yx data | KHÔNG dùng |

**Pin-mapping.md** đã ghi rõ pin nào avail/không avail trong XiaoZhi module.

### 5.1 Camera I2C control (SCCB)

Camera điều khiển qua I2C 100kHz (gọi là SCCB). Internal module đã wire sẵn.

Firmware không cần config pin SCCB → dùng API `esp_camera.h`:

```c
camera_config_t config = {
    .pin_pwdn = -1,           // No power-down (XiaoZhi không có)
    .pin_reset = -1,          // No reset pin
    .pin_xclk = -1,           // Module clock internal
    .pin_sccb_sda = -1,       // Internal
    .pin_sccb_scl = -1,       // Internal
    // ... pin DVP cũng đặt -1, module tự handle
    .xclk_freq_hz = 20000000, // 20MHz XCLK
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_VGA, // 640x480
    .jpeg_quality = 12,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
};
```

### 5.2 PSRAM cần thiết cho camera

VGA JPEG frame buffer ~30KB, 2 buffer = 60KB. SRAM internal ESP32-S3 chỉ 512KB → đủ cho VGA nhưng không đủ cho HD.

**Bật PSRAM** trong sdkconfig:
```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y      # Octal PSRAM (8MB N8R8 variant)
CONFIG_SPIRAM_SPEED_80M=y     # Max speed
CONFIG_SPIRAM_USE_MALLOC=y    # heap_caps_malloc(MALLOC_CAP_SPIRAM)
```

---

## 6. Battery voltage sensing

Theo `power-tree-spec.md` section 8, battery monitor qua ADC1_CH0 (GPIO1).

| Component | Value | Note |
|-----------|-------|------|
| R1 | 100kΩ 1% 0603 | Top of divider, từ VBAT |
| R2 | 33kΩ 1% 0603 | Bottom, đến GND |
| C_filter | 100nF X7R | Lọc nhiễu ADC, song song R2 |

ADC input range 0-3.3V (ADC1 attenuation 11dB).

**Lưu ý**: ADC2 (GPIO11-20) **không dùng được** khi WiFi đang TX. Bắt buộc ADC1.

---

## 7. Reset & power-good sequencing

Power-up sequence mong muốn:

```
0ms   ── Battery cắm vào
0-5ms ── Buck #1 → 5V đạt 4.5V
5-10ms ── LDO 3.3V đạt 3.0V
10ms  ── Tụ EN 100nF charge xong → EN HIGH
12ms  ── Chip ESP32-S3 release reset, bắt đầu boot ROM
50ms  ── Boot ROM check strapping pin (GPIO0=HIGH → SPI boot)
        Read flash → load bootloader
200ms ── Bootloader init PSRAM → load app từ partition
500ms ── app_main() chạy, WiFi init bắt đầu
2000ms ── WiFi connected (nếu cred OK)
```

Nếu thấy reboot loop ngay sau 12ms → strapping pin sai (GPIO12 float).
Nếu reboot loop ở ~200ms → PSRAM init fail (Octal/Quad mismatch trong sdkconfig).
Nếu reboot ở 500ms → brownout (rail 3.3V drop trong WiFi TX init) → tăng cap.

---

## 8. Test point cho debug

PCB nên có test point cho:

| Signal | Test point | Mục đích |
|--------|-----------|----------|
| EN | TP_EN | Đo điện áp + scope reset timing |
| GPIO0 | TP_BOOT | Verify boot button work |
| GPIO12 | TP_GPIO12 | Verify pull-down 10kΩ effective (đo R xuống GND) |
| RX/TX UART0 | TP_UART | Debug nếu USB-C không enumerate |
| 3.3V module pin | TP_3V3M | Verify LDO on-board của module |

---

## 9. Schematic verification checklist

Kiểm tra `mcu_core.kicad_sch` trong KiCad có:

- [ ] R_pullup 10kΩ trên GPIO0
- [ ] Nút BOOT giữa GPIO0 và GND
- [ ] R_pulldown 10kΩ trên GPIO12 xuống GND ← **critical**
- [ ] R_pullup 10kΩ trên EN
- [ ] Tụ 100nF trên EN xuống GND
- [ ] Nút RESET giữa EN và GND (optional)
- [ ] USB-C connector với:
  - VBUS → 5V rail (qua diode SS54 protect ngược)
  - GND → ground
  - D+ → GPIO20
  - D- → GPIO19
  - **CC1 và CC2 đều có R 5.1kΩ xuống GND**
  - Shield → ferrite bead → GND
- [ ] 470µF + 100nF decoupling sát 5V VIN module
- [ ] Voltage divider 100k+33k cho VBAT_SENSE → GPIO1 + 100nF filter
- [ ] Test point đầy đủ (EN, GPIO0, GPIO12, 3.3V, UART)

Nếu schematic chưa có những thành phần này → bổ sung trước khi đặt PCB.

---

## 10. Common pitfalls — ESP32-S3-CAM design

| Lỗi | Triệu chứng | Cách fix |
|-----|-------------|---------|
| GPIO12 không pull-down | Boot loop 100% sau reset | Hàn R 10kΩ giữa GPIO12 và GND |
| GPIO0 không pull-up | Boot loop ngẫu nhiên | Hàn R 10kΩ giữa GPIO0 và 3.3V |
| Tụ EN thiếu | Phantom reset khi load thay đổi | Hàn 100nF giữa EN và GND |
| CC1/CC2 không có 5.1k | PC không thấy USB device | Hàn 2× 5.1kΩ trên CC1, CC2 |
| Decoupling 5V VIN thiếu | Reset khi WiFi TX | Hàn 470µF aluminium sát chân |
| sdkconfig PSRAM Quad mode | Octal PSRAM không boot | Sửa thành Oct mode, rebuild |
| 5V rail < 4.5V dưới load | Chip brownout reset | Verify Buck #1 output đủ áp |
| Camera init fail | OV3660 không trả I2C | Verify module XiaoZhi đúng variant + reset cycling |

---

## 11. Next phase prep

Sau khi Phase 3 pass:
- MCU boot OK, serial log thấy "Pro cpu up"
- PSRAM detect 8MB
- Camera stream MJPEG 30fps qua web example
- WiFi connect và ping được

→ **Phase 4**: cắm 2 servo SG90 vào pin 44, 45 → quay PTZ thử
