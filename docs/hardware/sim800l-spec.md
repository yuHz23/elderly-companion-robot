# SIM800L Cellular SOS Specification

> Module GSM/GPRS 2G để gửi SMS + gọi điện báo động khi robot phát hiện té ngã. Đường dây cấp cứu **không phụ thuộc WiFi** — vẫn hoạt động khi mất internet.
>
> **Version**: 1.0 — 2026-05-18
> **Phase**: 8 (HDSD)

---

## 1. Topology

```
                          +4V rail (LM2596S #2, 2A peak)
                                       │
                                       ▼
                            ┌──────────────────────┐
                            │   SIM800L Module     │
                            │   (Shopee variant)   │
                            │                      │
                            │ VCC ◄────────────────┘
                            │ GND ─────► robot GND
                            │
       GPIO48 (TX) ────► RXD│
       GPIO46 (RX) ◄──── TXD│
       GPIO47 (out) ───► PWRKEY (active LOW pulse > 1s)
                            │
                            │ ANT ─────► spring antenna (kèm module)
                            │ SIM slot ─► nano SIM trả trước
                            │
                            └──────────────────────┘
```

---

## 2. Pin mapping

Theo `pin-mapping.md`:

| ESP32-S3 GPIO | Signal | Direction | SIM800L pin |
|---------------|--------|-----------|-------------|
| GPIO48 | UART TX | Output | RXD |
| GPIO46 | UART RX | Input | TXD |
| GPIO47 | PWRKEY ctrl | Output (open-drain hoặc push-pull) | PWRKEY |

### 2.1 ⚠️ UART controller

Tên "UART2" trong `pin-mapping.md` là **logical** (UART thứ hai cho SIM800L), không phải literal `UART_NUM_2`. Firmware dùng `UART_NUM_1`:
- `UART_NUM_0` = USB-CDC console
- `UART_NUM_1` = **SIM800L** (Phase 8) ← driver dùng
- `UART_NUM_2` = available cho future expansion

ESP32-S3 GPIO matrix flexible nên pin nào cũng route được, không quan trọng physical controller nào.

### 2.2 Logic level

| Direction | SIM800L pin | Voltage | ESP32-S3 |
|-----------|-------------|---------|----------|
| ESP → SIM800L | RXD input | Accept 2.5V+ | 3.3V output ✓ |
| SIM800L → ESP | TXD output | 2.8V typical | 3.3V input min 1.65V ✓ |

**Không cần level shifter**. Vì TXD chỉ 2.8V (không phải 3.3V), tín hiệu vẫn đủ HIGH cho ESP32-S3 nhận.

### 2.3 PWRKEY behavior

- Module **không tự bật** khi cấp nguồn — phải pulse PWRKEY
- Pulse > 1 giây để bật
- Pulse > 1 giây lần nữa để tắt
- Sau pulse, đợi 3-5s cho module register network

```c
// Power on sequence
gpio_set_level(PWRKEY, 1);      // default HIGH
vTaskDelay(pdMS_TO_TICKS(100));
gpio_set_level(PWRKEY, 0);      // pulse LOW
vTaskDelay(pdMS_TO_TICKS(1500));
gpio_set_level(PWRKEY, 1);      // release
vTaskDelay(pdMS_TO_TICKS(5000)); // wait network
```

### 2.4 PWRKEY level translator

Một số SIM800L module có PWRKEY input chấp nhận 0V/3.3V trực tiếp. Một số khác cần level translator (3.3V → 2.8V). **Module Shopee phổ biến (CWS-SIM800L)** chấp nhận trực tiếp 3.3V — không cần thêm gì.

Verify spec module trước khi đặt PCB.

---

## 3. Power supply

### 3.1 Tại sao cần rail riêng 4V?

SIM800L spec input **3.4 - 4.4V**. **Không dùng 3.3V** rail của MCU (quá thấp) và **không dùng 5V** rail (quá cao + GSM TX burst sẽ phá MCU).

Phase 2 power tree đã dành rail riêng:
- **LM2596S-ADJ #2**: 12V → 4.0V, 2A peak
- Cap output 470µF aluminum + 100µF tantalum sát chân VCC SIM800L
- Cap đủ lớn để hấp thụ burst 2A trong 577µs TX

### 3.2 Current profile

```
  Idle:      0.05 A
  Network:   0.30 A (active idle)
  GPRS:      0.50 A
  GSM TX:    2.00 A burst (577us / 4.6ms = 12% duty)
  Average:   ~0.40 A
```

Tổng năng lượng cho 1 phút gọi điện: 0.4A × 4V × 60s ≈ 96 J = ~3% battery 3S 3Ah.

### 3.3 Power-on dip

Khi bật SIM800L lần đầu, module kéo 1-1.5A trong 1 giây đầu (initialize PLL, register). Verify cap output buck #2 không sụt > 0.5V.

---

## 4. AT command reference

SIM800L hiểu AT command qua UART. Default baud 9600 (có thể đổi). Mỗi command kết thúc `\r\n`. Response cũng kết thúc `\r\n`.

### 4.1 Essential commands

| Command | Mục đích | Expected response |
|---------|----------|-------------------|
| `AT` | Test connection | `OK` |
| `ATE0` | Tắt echo (giảm bytes) | `OK` |
| `AT+CREG?` | Network registration status | `+CREG: 0,1` (home) hoặc `0,5` (roaming) |
| `AT+CSQ` | Signal quality | `+CSQ: 15,99` (RSSI 0-31, BER 0-7) |
| `AT+COPS?` | Current operator | `+COPS: 0,0,"Viettel"` |
| `AT+CBC` | Battery (module's, not robot's) | `+CBC: 0,80,3950` |
| `AT+GMR` | Firmware version | `Revision:1418B05SIM800L24` |

### 4.2 SMS — text mode

| Step | Command | Response |
|------|---------|----------|
| 1 | `AT+CMGF=1` (text mode) | `OK` |
| 2 | `AT+CMGS="+84909123456"` | `>` (prompt) |
| 3 | (gửi message text) | (no immediate response) |
| 4 | (gửi Ctrl+Z = 0x1A) | `+CMGS: nnn\r\nOK` |

### 4.3 Voice call

| Step | Command | Response |
|------|---------|----------|
| Dial | `ATD+84909123456;` | `OK` (ngay), `NO CARRIER` nếu không bắt máy, `BUSY` nếu busy |
| Hang up | `ATH` | `OK` |
| Answer incoming | `ATA` | `OK` |

**Lưu ý**: dấu `;` cuối số là quan trọng — báo voice call, không có sẽ thành data call.

### 4.4 Signal quality interpretation

`AT+CSQ` → `+CSQ: <rssi>,<ber>`

| RSSI | dBm | Quality |
|------|-----|---------|
| 0 | -113 | Marginal |
| 1 | -111 | Marginal |
| 2-9 | -109 to -95 | Poor |
| 10-14 | -93 to -85 | OK |
| **15-19** | **-83 to -75** | **Good** |
| 20-30 | -73 to -53 | Excellent |
| 99 | — | Unknown / no signal |

**Acceptance**: ≥ 15 = OK gọi/SMS reliable. Dưới 15 → cần đổi anten hoặc vị trí.

---

## 5. SIM card requirements

### 5.1 Yêu cầu

- **Nano SIM** (15×12mm) — đa số SIM800L module dùng nano size
- **2G GSM** — SIM800L chỉ support 2G. Đảm bảo nhà mạng còn duy trì 2G:
  - **Việt Nam**: Viettel, MobiFone, VinaPhone — vẫn còn 2G nhưng có kế hoạch tắt 2G/3G sau 2026-09. Verify trước khi mua module.
  - **Mỹ/EU**: 2G shutdown — không dùng được SIM800L
- Trả trước **đủ tiền cho SMS + gọi** (10k VND/tháng đủ cho ~50 SMS)
- **Không** PIN lock (hoặc disable PIN trong điện thoại trước khi cắm vào SIM800L)

### 5.2 APN config (cho GPRS, optional Phase 8)

Phase 8 chỉ dùng SMS + voice → **không cần APN**. Khi cần data (Phase 12 nâng cao):

```
AT+CSTT="v-internet","",""        // Viettel
AT+CIICR
AT+CIFSR
```

---

## 6. Antenna

### 6.1 Module Shopee

Đa số module SIM800L bán kèm **spring antenna** (lò xo nhỏ hàn lên pad ANT). OK trong nhà có sóng tốt.

### 6.2 Upgrade khi tín hiệu yếu

Nếu CSQ < 15 trong nhà:
- **Anten dán LTE/GSM**: kết nối qua dây IPEX → tăng 5-10 dB
- Đặt anten **bên ngoài** vỏ robot (acrylic OK nhưng kim loại block)
- Tránh sát motor (rung + EMI)

---

## 7. Schematic verification

Kiểm tra trong KiCad (`camera_dock.kicad_sch` hoặc tạo `sim800l.kicad_sch`):

- [ ] SIM800L module 8-pin connector:
  - VCC → +4V rail (sau buck #2)
  - GND → GND
  - RXD → GPIO48
  - TXD → GPIO46
  - PWRKEY → GPIO47
  - DTR, RST, RI → NC (không dùng)
- [ ] Decoupling cap sát VCC:
  - 470µF aluminium 6.3V low-ESR
  - 100µF tantalum 6.3V
  - 100nF X7R 0603
- [ ] Anten pad (IPEX hoặc U.FL connector)
- [ ] Spring antenna nối qua dây ngắn (< 50mm)
- [ ] Test point: TX, RX, PWRKEY
- [ ] Layout note: anten cách kim loại lớn ≥ 15mm

---

## 8. BOM

| Item | Qty | Source | Cost |
|------|-----|--------|------|
| SIM800L module (CWS hoặc Quectel) | 1 | Đã có | 0 |
| Spring antenna (kèm module) | 1 | Đã có | 0 |
| Nano SIM 2G trả trước | 1 | Viettel/MobiFone | 50,000 |
| Connector header 8-pin 2.54mm | 1 | Shopee | 1,000 |
| Cap 470µF/100µF tantalum/100nF | (đã trong stock) | — | — |
| (Optional) Anten LTE dán có IPEX | 1 | Shopee | 50,000 |
| **TOTAL** | | | **~50-100k VND** (chủ yếu SIM) |

---

## 9. Emergency contacts storage

Lưu vào NVS namespace `sos`:

| Key | Value | Mục đích |
|-----|-------|---------|
| `phone1` | `+84909123456` | Primary (gọi + SMS) |
| `phone2` | `+84909987654` | Secondary (SMS only) |
| `sms_text` | (template) | Custom SOS message |

Default `sms_text`:
```
[ELDERLY ROBOT SOS] Phat hien NGUOI NHA BI TE NGA tai phong khach. Vui long lien lac ngay.
```

(Tránh dấu tiếng Việt vì SMS 7-bit GSM không support — sẽ thành Unicode → giới hạn ký tự ít hơn)

---

## 10. Common pitfalls

| Lỗi | Nguyên nhân | Fix |
|-----|-------------|----|
| Module không bật | PWRKEY pulse không đủ dài | Đảm bảo > 1 giây pulse LOW |
| AT command no response | Baud rate sai (default 9600, đôi khi 115200) | Try `AT` ở 9600, nếu fail thử 115200 |
| CSQ = 99 (no signal) | Anten không tốt | Đổi anten LTE ngoài |
| SMS gửi báo "OK" nhưng người nhận không nhận | Số điện thoại sai format | Phải có `+` prefix (`+84909...`) |
| Module restart liên tục | Power dip do thiếu bulk cap | Tăng cap output buck #2 lên 1000µF |
| Voice call fail "NO CARRIER" | Số máy đầu kia không bắt máy | Bình thường — retry sau 30s |
| ESP32 reboot khi SIM800L TX | Rail 4V không tách riêng (share với MCU) | Verify Phase 2 design — rail riêng |

---

## 11. Next phase

Sau Phase 8 pass (SOS test: drop robot → SMS đến + gọi điện trong 15s):
→ **Phase 9** — Auto-dock + charging (IR beacon + copper contact + state machine).
