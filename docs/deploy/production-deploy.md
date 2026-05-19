# Production Deployment Guide

> Quy trình mang robot từ bench (đã pass Test 1-8) đến nhà người thân và để vận hành tự động.
>
> **Tiền đề**: Phase 11 sign-off (`test-results-YYYYMMDD.md` đã PASS).

---

## 1. Final hardware checklist (1 ngày trước deploy)

### 1.1 Physical inspection

- [ ] Vít chassis chặt (lắc nhẹ, không có gì rơi rớt)
- [ ] Bánh xe xoay tự do, không vênh
- [ ] Spring contact ở dock side: stroke 3mm, gold-plated sạch
- [ ] PTZ servo chuyển động mượt, không buzz lớn
- [ ] OLED hiển thị rõ ràng, không pixel chết
- [ ] Camera lens sạch (lau bằng vải microfiber)
- [ ] Antenna SIM800L gắn chắc

### 1.2 Battery health

- [ ] 3 cell 18650 đo voltage no-load: 12.5-12.6V (sạc đầy)
- [ ] Internal resistance < 50mΩ mỗi cell (đo bằng đồng hồ chuyên dụng)
- [ ] Không phồng / rò rỉ
- [ ] Capacity test: 18650 còn ≥ 80% nominal (test bằng bộ load)

Nếu battery > 18 tháng tuổi → **thay mới** trước deploy.

### 1.3 SIM card

- [ ] SIM 2G còn ≥ 100k VND
- [ ] PIN disabled
- [ ] Test gọi/SMS từ điện thoại khác → nhận được
- [ ] Đã đăng ký thông tin thuê bao (Việt Nam yêu cầu)

### 1.4 Calibration verify

Mở web UI, run `/diag/selftest`:
- [ ] Tất cả 10/10 check pass
- [ ] IMU tilt < 1° khi robot đứng phẳng (đã calibrate)
- [ ] Drive trim đảm bảo straight < 20cm/2m
- [ ] PTZ offset chính giữa = 90°

---

## 2. Deployment kit

Mang theo:

| Item | Mục đích |
|------|----------|
| Robot (full charged) | — |
| Dock station + AC adapter | Lắp tường nhà người thân |
| Laptop với firmware nguồn | Re-flash nếu cần |
| Cáp USB-C data | Reflash + serial debug |
| Thước đo | Đo vị trí dock đúng cao 80mm |
| Vít M3 + drill tay + plug nhựa | Lắp dock vào tường |
| Đệm xốp | Test fall detection lần cuối |
| Hồ sơ in: số điện thoại family + WiFi của family | Setup |
| Sample card visit + hướng dẫn dùng (1 trang) | Để lại cho gia đình |

---

## 3. On-site setup (60-90 phút)

### 3.1 WiFi setup (10 phút)

WiFi tại nhà người thân thường khác phòng test:

```
Robot mở softAP "elderly-bot-XXXX" (default khi chưa có cred)
→ Phone connect → http://192.168.4.1/
→ Nhập SSID + password nhà người thân
→ Save → reboot → robot connect home WiFi
```

Verify:
- OLED hiện IP địa chỉ (192.168.x.x)
- Lap pop laptop cùng mạng → http://<robot-ip>/

### 3.2 Lắp dock (20 phút)

- Chọn vị trí: tường gần ổ cắm điện, không vướng đồ đạc, không nắng trực tiếp
- Khoan 4 lỗ M3 vào tường (dùng plug nhựa)
- Bắt dock plate vít M3
- Cắm AC adapter
- LED dock xanh sáng → dock ON
- Đo voltage giữa 2 copper plate: 12.6V

### 3.3 Test docking thực địa (10 phút)

⚠️ **Phòng người thân khác layout với phòng test** — verify lại.

- Đặt robot cách dock 1m, mặt hướng dock
- Trigger `/dock/start`
- Robot tìm dock + về sạc
- Repeat 3 lần đảm bảo reliable

Nếu fail:
- IR beacon yếu trong phòng sáng → đặt dock nơi tối hơn
- Spring/copper alignment lệch → đo lại

### 3.4 SOS contacts (5 phút)

- Mở UI panel "SOS"
- Phone 1: số con trai/gái lớn (primary contact)
- Phone 2: số người nhà khác (backup)
- Save
- Test trigger: bấm nút đỏ "TRIGGER SOS"
  - Phone 1 nhận SMS trong 30s
  - Phone 2 nhận SMS
  - Phone 1 đổ chuông

### 3.5 MQTT + HASS (15 phút, nếu HASS đã setup tại nhà)

Theo `home-assistant.md`:
- Robot point đến HASS broker
- Verify entities discover
- Test 1 fall (drop trên đệm) → Telegram alert đến family

### 3.6 Schedule daily ops (5 phút)

Default schedule đã seed (06:00 leave, 08:00 patrol, etc.). Adjust qua UI:
```
GET /schedule        # xem hiện tại
GET /schedule/set?i=0&h=7&m=30&cmd=leave   # đổi entry 0
```

Tùy nhịp sống người thân:
- Người ngủ muộn → leave 09:00 thay vì 06:00
- Có người giúp việc → tắt patrol khi họ đang làm

### 3.7 Voice greeting custom (optional, 5 phút)

Tạo audio chào buổi sáng:
```bash
# Record từ phone: "Chào bác, chúc bác một ngày khỏe mạnh"
# Upload qua POST /audio/play (chưa implement Phase 12)
```

Phase 12 ship scaffold thôi — voice integration đầy đủ là Phase 13+.

---

## 4. Train người dùng (15 phút)

Đối với người già, để robot **tự chạy** là chính. Nhưng họ cần biết:

### 4.1 OLED reading

```
state:PATROL   ← robot đang đi tuần
state:DOCKED   ← robot đang ngồi sạc
state:SOS_*    ← BẤT THƯỜNG, gọi con cháu
```

### 4.2 Manual stop button

Trên chassis có nút "Emergency Stop" lớn (red). Bấm = robot dừng ngay. Bấm lại = robot tiếp tục.

(Nếu chưa có nút vật lý → Phase 12+ thêm pull-up trên GPIO0 = phục vụ BOOT nhưng cũng làm e-stop khi đang chạy.)

### 4.3 Nếu mất điện

- Dock mất điện → robot tự xả pin → sau ~3h dock thất bại → dừng
- WiFi mất → mất control từ xa nhưng robot vẫn auto-dock + SOS qua SIM800L

### 4.4 Khi cần gọi support

In thẻ giấy có:
- Số điện thoại "kỹ thuật viên" (chính bạn)
- Hướng dẫn 3 bước: rút pin → đợi 30s → cắm lại
- Mã QR đến hướng dẫn online (optional)

---

## 5. Post-deploy verification (sau 24h)

Lúc về kiểm tra remote:
- HASS dashboard: robot uptime > 24h, charge cycles complete
- Telegram: có notification nào bất thường không
- SMS bot bạn: không có SOS giả

Cuối tuần ghé thăm:
- Pin có sạc đầy không
- Có vết va đập ở chassis không
- Hỏi người dùng có gì khó khăn

---

## 6. Maintenance lịch

| Frequency | Task |
|-----------|------|
| Hàng tuần | Check HASS dashboard, xem có alert bất thường |
| Hàng tháng | Đến thăm, lau camera lens, kiểm tra vít, run /diag/selftest |
| Hàng quý | Re-calibrate IMU, đo battery internal resistance |
| 1 năm | Thay battery 18650 nếu capacity < 70%, thay brush motor BO |
| 2 năm | Đánh giá tổng thể, có nên upgrade firmware/hardware |

---

## 7. Common deploy issues

| Triệu chứng | Nguyên nhân | Fix |
|-------------|-------------|-----|
| WiFi không connect | Nhà có 5GHz only, robot 2.4GHz | Yêu cầu router enable dual-band |
| SIM800L no signal trong nhà | Tường dày | Đặt anten ngoài hoặc gần cửa sổ |
| Dock không reliable | Sàn không phẳng | Đặt thảm hoặc tinh chỉnh chassis level |
| Robot patrol đụng đồ đạc liên tục | Phòng đông đồ | Giảm patrol area, di dời đồ trong "safe zone" |
| Pin xả nhanh hơn dự đoán | Cold weather hoặc battery degraded | Thay cell, hoặc tăng schedule dock từ 2 lần/ngày lên 3 |

---

## 8. Sign-off

| Item | Verified by | Date |
|------|-------------|------|
| Hardware checklist | | |
| Battery health | | |
| WiFi setup | | |
| Dock alignment | | |
| SOS test (real SMS+call) | | |
| HASS dashboard | | |
| Schedule customized | | |
| User trained | | |

**Deployed by**: ____________________
**Deploy date**: 2026-05-19 (Phase 12 release)
**Re-visit**: ________________ (1 month follow-up)

→ Hệ thống production-ready. Lưu lại file này tại `docs/deploy/deployments/HOUSEHOLD_NAME-YYYYMMDD.md`.
