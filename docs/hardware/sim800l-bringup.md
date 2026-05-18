# SIM800L Bring-up & SOS Verification

> Quy trình test SIM800L sau khi Phase 7 (audio) đã pass.
>
> **Tiền đề**: PCB power working, rail 4V đã verify (xem `power-bringup.md` Phase D), nano SIM 2G còn tiền + đã disable PIN.
>
> **Dụng cụ**: Điện thoại để nhận SMS + gọi, oscilloscope (optional), nano SIM 2G trả trước.

---

## Phase 8.A — SIM card preparation

⚠️ **Disable PIN lock** trước khi cắm:
1. Cắm SIM vào điện thoại bình thường
2. Settings → Security → SIM Lock → **Tắt**
3. Verify: rút SIM rồi cắm lại, gọi/nhắn không hỏi PIN
4. Sau đó cắm SIM vào module SIM800L (slot ở mặt sau)

Verify còn tiền:
- Gọi `*101#` (Viettel) hoặc tương đương để check balance
- Đảm bảo ≥ 10k VND cho test

---

## Phase 8.B — First power-on (5 phút)

### B.1 Build & flash

```bash
cd firmware/
pio run -e esp32-s3-cam -t upload
pio device monitor
```

Log mong đợi sau ~30s:
```
I (xxx) sim800: uart installed — TX=48 RX=46 PWRKEY=47 @ 9600 baud
I (xxx) sos: powering on SIM800L (this can take 10-30s)
I (xxx) sim800: pulsing PWRKEY...
I (1234) sim800: module responsive after 5 s
I (5678) sim800: registered, RSSI=18
I (xxx) sos: SIM800 not ready — retrying power-on   <-- chỉ xuất hiện nếu fail
```

### B.2 Verify signal quality

URL `http://<ip>/sim800/status`:
```json
{
  "state": 2,
  "rssi": 18,
  "registered": true,
  "phone1": "",
  "phone2": "",
  "trigger_count": 0
}
```

- `state` = 2 = READY
- `rssi` ≥ 15 = signal đủ
- `registered` = true

Web UI panel "Status" sẽ hiện:
```
sim800     READY rssi=18 registered
sos        (chưa config)
```

### B.3 Nếu fail

| Triệu chứng | Fix |
|-------------|-----|
| `state=3 FAULT` | Module chết — kiểm tra VCC, PWRKEY, đo điện áp 4V rail |
| `state=2 READY rssi=99` | Anten yếu — đổi anten LTE bên ngoài |
| `state=2 NOT registered` | SIM không có sóng 2G nhà mạng, hoặc SIM lock | Verify nhà mạng còn 2G + PIN disabled |
| No response sau 30s | UART wiring sai TX/RX | Swap GPIO46/48, verify scope thấy data |

---

## Phase 8.C — Manual AT command test (optional, 10 phút)

Một số test ban đầu kiểm tra module healthy. Dùng curl + raw AT (yêu cầu thêm endpoint custom, hoặc dùng serial monitor + ESP serial bridge — skip nếu không cần).

Đơn giản nhất: dùng `/sim800/sms` và `/sim800/dial` test trực tiếp (Phase D).

---

## Phase 8.D — Test SMS (5 phút)

### D.1 Manual SMS

```bash
curl "http://<ip>/sim800/sms?to=%2B84909123456&text=Hello%20from%20robot"
```

(`%2B` là URL-encode của `+`; `%20` là space)

Response: `{"ok":true,"sent":true}`

- [ ] Điện thoại nhận SMS trong **15-30 giây**
- [ ] Nội dung đúng "Hello from robot"
- [ ] Số gửi đến hiển thị đúng số trong SIM của module

### D.2 SMS không có ký tự đặc biệt

SMS 7-bit GSM chỉ support ASCII + một số ký tự châu Âu. Tiếng Việt dấu sẽ thành Unicode → giới hạn ký tự còn 70.

Best practice: **dùng không dấu** trong SMS_TEXT (default đã không dấu).

---

## Phase 8.E — Test voice call (5 phút)

### E.1 Dial test

```bash
curl "http://<ip>/sim800/dial?to=%2B84909123456"
```

- [ ] Điện thoại đổ chuông trong **10-20 giây**
- [ ] Nhấc máy → kết nối được (nhưng **không có audio 2 chiều** vì chưa có speaker/mic bridged qua SIM800L — chỉ verify dial work)
- [ ] Để chuông 5-10s

### E.2 Hangup

Trong khi đang gọi:
```bash
curl "http://<ip>/sim800/hangup"
```

- [ ] Cuộc gọi cắt ngay
- [ ] Hoặc tự cắt sau 30s nếu không nhấc máy (driver timeout)

---

## Phase 8.F — Configure emergency contacts (3 phút)

### F.1 Lưu contacts qua web UI

1. Mở `http://<ip>/`
2. Panel **SOS — emergency contacts**:
   - Phone 1: `+84909123456` (số người nhà chính)
   - Phone 2: `+84909987654` (optional — số dự phòng)
3. Bấm "Save contacts"
4. Reload page → ô input đã pre-fill (NVS persist)

### F.2 Verify lưu NVS

```bash
curl "http://<ip>/sim800/status"
# Phải có "phone1":"+84909123456","phone2":"+84909987654"
```

Reboot ESP32 → contacts vẫn còn.

---

## Phase 8.G — SOS trigger (manual) — END-TO-END

⚠️ **Báo trước người nhận** rằng đây là test, không phải khẩn cấp thật.

### G.1 Test trigger button

1. Web UI → bấm nút đỏ "TRIGGER SOS (test)"
2. Confirm dialog → OK
3. Log serial:
   ```
   W (xxx) sos: trigger source: MANUAL
   E (xxx) sos: *** SOS DISPATCH ***
   I (xxx) sos: SMS → +84909123456
   I (xxx) sos: SMS send result: OK
   I (xxx) sos: SMS → +84909987654
   I (xxx) sos: SMS send result: OK
   I (xxx) sos: dialing → +84909123456
   ```
4. Trong **30 giây**:
   - [ ] Phone1 nhận SMS với SOS text
   - [ ] Phone2 nhận SMS với SOS text
   - [ ] Phone1 đổ chuông từ số SIM của module
5. Hangup tự động sau 30s (nếu không nhấc máy)
6. Web UI `trigger_count` tăng lên 1

### G.2 Nếu fail

| Triệu chứng | Nguyên nhân | Fix |
|-------------|-------------|----|
| `state=3 FAULT` ngay khi trigger | SIM lost signal đột ngột | Retry sau 1 phút, đợi reconnect |
| SMS OK nhưng phone không nhận | Số sai format hoặc nhà mạng chặn | Verify số `+84xxx` đúng |
| Dial báo OK nhưng phone không chuông | "NO CARRIER" — số tắt máy hoặc trên 30s không bắt | Bình thường, retry |
| Module reboot khi dial | Power dip do bulk cap thiếu | Tăng cap output buck 470µF → 1000µF |

---

## Phase 8.H — Fall → SOS end-to-end (10 phút)

⚠️ **Cần đệm xốp dưới robot** trước khi drop.

### H.1 Drop test

1. Đặt robot trên đệm xốp 30cm
2. Đảm bảo IMU đã calibrate (Phase 6)
3. Đảm bảo SOS contacts đã save (Phase F)
4. Cầm robot lên cao 30cm
5. **Báo trước người nhận**: "Sắp test fall detection"
6. Thả robot → rơi xuống xốp

Log:
```
W (xxx) sensors: spike 3.20g — fall candidate
E (xxx) sensors: FALL DETECTED — tilt 82.1°
W (xxx) sos: trigger source: FALL
E (xxx) sos: *** SOS DISPATCH ***
...
```

Trong **30-60 giây**:
- [ ] Phone1 nhận SMS SOS
- [ ] Phone1 đổ chuông
- [ ] Web UI báo `⚠ FALL` trong sensors panel
- [ ] Sensor task ghi `trigger_count` = 1

### H.2 Cooldown verify

Drop lại lần thứ 2 trong vòng 30 giây sau lần đầu:
- [ ] Log: `sensors: spike X.XXg — fall candidate` xuất hiện nhưng KHÔNG dispatch SOS lần 2
- [ ] `trigger_count` vẫn = 1

Đợi 30s, đặt robot đứng dậy → log `sensors: fall cleared`. Sau đó drop lại → trigger lần 2 work.

---

## Phase 8.I — Long-run reliability (1 giờ)

### I.1 Idle test

Để robot bật, không action gì trong 1 giờ.

- [ ] SIM800L `state=READY` xuyên suốt
- [ ] RSSI ổn định (± 5 từ baseline)
- [ ] Module không tự reboot
- [ ] Dòng tiêu thụ từ rail 4V: ~50-100mA (idle)

### I.2 Periodic test

Trigger SOS 5 lần, cách nhau 5 phút:
- [ ] Cả 5 lần SOS đến phone trong < 60s
- [ ] Không có lỗi UART timeout
- [ ] `trigger_count` = 5

---

## Common pitfalls

| Triệu chứng | Nguyên nhân | Fix |
|-------------|-------------|-----|
| Module không boot | PWRKEY pulse không đủ 1s | Verify `sim800_power_on()` pulse 1500ms |
| Booted nhưng AT no response | Baud rate sai | Try 115200 thay 9600 trong driver |
| Registered nhưng SMS fail | Nhà mạng tắt 2G | Verify số trên TT giữa 2026-2027 (VN), 2017 (US) |
| Robot reboot khi SIM TX | Rail 4V share với 5V | Verify Phase 2 — rail riêng |
| Test trigger không gửi SMS | Contacts chưa save NVS | Verify `/sim800/status` show contacts |
| Cooldown 30s không work | sensor_fusion bị bug timestamp | Verify reading `esp_timer_get_time` đúng |

---

## Phase 8 — Sign-off

- [ ] SIM800L power-on, state=READY, registered
- [ ] RSSI ≥ 15 trong vị trí thực tế đặt robot
- [ ] SMS test gửi đến phone trong < 30s
- [ ] Voice dial test ring trong < 20s
- [ ] Contacts save NVS, persist qua reboot
- [ ] Manual SOS trigger → SMS×2 + call trong < 60s
- [ ] **Fall detection (drop test) → SOS auto-trigger end-to-end**
- [ ] Cooldown 30s chống spam
- [ ] 1-hour idle: module không reboot, không lose registration

→ **Tiếp theo**: Phase 9 — Auto-dock + charging (IR beacon homing + copper contact + state machine docking 7 trạng thái).
