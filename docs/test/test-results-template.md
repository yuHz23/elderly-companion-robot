# Test Results — Elderly Companion Robot

> Copy file này thành `test-results-YYYYMMDD.md` cho mỗi run test. Ghi đầy đủ kết quả + photo + serial log archive.

---

## Test session info

- **Date**: __________________
- **Tester**: __________________
- **Firmware commit**: ___________________  (git rev-parse --short HEAD)
- **Hardware revision**: _______________
- **Battery age**: ____ tháng (degraded cell có thể fail Test 4)

---

## Test 1 — Smoke test (1 giờ)

### 1.1 Boot
- [ ] All 9 tasks init OK (no ERROR in serial)
- [ ] `/status` JSON: phase=10, free_heap=______ KB, psram=8MB
- [ ] OLED displays 8 lines

### 1.2 Self-test endpoint
```
Output of GET /diag/selftest?motor_pulse=0:
______________________________________________
______________________________________________
______________________________________________
```

### 1.3 1-hour idle
- Start free_heap: _______ bytes
- End free_heap:   _______ bytes
- Δ: _______ bytes (target: ≤ 5000 leak)
- Reboot count: _______ (target: 0)

**Test 1 result**: ☐ PASS  ☐ FAIL

---

## Test 2 — Drive train (30 phút)

### 2.1 Straight line 2m
- Lệch ngang đo: _______ cm (target: < 20cm)

### 2.2 Rotation 360°
- Góc lệch: _______ ° (target: < 30°)

### 2.3 Obstacle response time
- Đo (lặp 5×): _____, _____, _____, _____, _____ ms (target: < 200ms avg)

### 2.4 Watchdog brake
- Tắt WiFi đến brake: _______ ms (target: < 600ms)

### 2.5 E-stop button
- Bấm đến brake: _______ ms (target: < 50ms)

**Test 2 result**: ☐ PASS  ☐ FAIL

---

## Test 3 — Camera + audio (1 giờ)

### 3.1 Camera FPS
- 30-min run, sample FPS: ___, ___, ___, ___ → avg: ____ (target: ≥ 15)
- Drop frames count: _____

### 3.2 PTZ accuracy
- Pan 10→170 time: _____ s
- Tilt 30→150 time: _____ s

### 3.3 Audio
- Loopback "1 2 3" intelligible (Y/N): ____
- Tone 440Hz pitch correct (Y/N): ____
- Tone 1kHz pitch correct (Y/N): ____

### 3.4 Combined load 5 phút
- Lag/freeze observed (Y/N): ____

**Test 3 result**: ☐ PASS  ☐ FAIL

---

## Test 4 — Docking 20×

| # | Start angle | Success | Time(s) | Notes |
|---|-------------|---------|---------|-------|
| 1 | 0°   |  |  |  |
| 2 | +15° |  |  |  |
| 3 | -15° |  |  |  |
| 4 | +30° |  |  |  |
| 5 | -30° |  |  |  |
| 6 | 0°   |  |  |  |
| 7 | +15° |  |  |  |
| 8 | -15° |  |  |  |
| 9 | +30° |  |  |  |
| 10 | -30° |  |  |  |
| 11 | 0°  |  |  |  |
| 12 | +15° |  |  |  |
| 13 | -15° |  |  |  |
| 14 | +30° |  |  |  |
| 15 | -30° |  |  |  |
| 16 | 0°  |  |  |  |
| 17 | +15° |  |  |  |
| 18 | -15° |  |  |  |
| 19 | +30° |  |  |  |
| 20 | -30° |  |  |  |

- Success rate: ______ / 20 (target: ≥ 16)
- Avg time: ______ s (target: < 60)

**Test 4 result**: ☐ PASS  ☐ FAIL

---

## Test 5 — Fall detection (10 lần)

### 5.1 True positive (drop 30cm)
| # | Trigger | Time to SMS(s) |
|---|---------|----------------|
| 1 |  |  |
| 2 |  |  |
| 3 |  |  |
| 4 |  |  |
| 5 |  |  |
| 6 |  |  |
| 7 |  |  |
| 8 |  |  |
| 9 |  |  |
| 10 |  |  |

TP rate: ______ / 10 (target: ≥ 9)

### 5.2 False positive

| # | Stimulus | Trigger? (phải N) |
|---|----------|-------------------|
| 1 | Đẩy mạnh forward |  |
| 2 | Đập tay xuống bàn |  |
| 3 | Quay 360° nhanh |  |
| 4 | Sàn xóc |  |
| 5 | Cầm lên giữ thẳng |  |
| 6 | Lắc bên 15° |  |
| 7 | Va nhẹ |  |
| 8 | Đụng tường cruise |  |
| 9 | Reverse đụng |  |
| 10 | PATROL 5 phút |  |

FP rate: ______ / 10 (target: 0)

**Test 5 result**: ☐ PASS  ☐ FAIL

---

## Test 6 — SOS (3 lần)

| # | Trigger | SMS p1 | SMS p2 | Call | Time |
|---|---------|--------|--------|------|------|
| 1 | /sos/trigger |  |  |  |  |
| 2 | Drop test |  |  |  |  |
| 3 | UI button |  |  |  |  |

**Test 6 result**: ☐ PASS  ☐ FAIL

---

## Test 7 — Edge cases

| Case | Pass? | Notes |
|------|-------|-------|
| WiFi flap recovery |  |  |
| Brownout safe reboot |  |  |
| Motor stall recovery |  |  |
| Fall during dock |  |  |
| Battery critical auto-dock |  |  |

**Test 7 result**: ☐ PASS  ☐ FAIL

---

## Test 8 — 24h soak

Theo `24h-soak-plan.md`. Archive:
- Serial log → `soak-results/serial-YYYYMMDD.log.gz`
- Metrics → `soak-results/metrics-YYYYMMDD.log.gz`
- Plot → `soak-results/voltage-YYYYMMDD.png`

- [ ] 0 reboot
- [ ] Memory stable
- [ ] 4/4 charge cycles complete
- [ ] 0 fall FP in 24h

**Test 8 result**: ☐ PASS  ☐ FAIL

---

## Final verdict

☐ **SHIP** — all 8 tests pass
☐ **NEEDS REWORK** — list issues below

### Issues
1. ___________________________
2. ___________________________
3. ___________________________

**Tester signature**: ____________________
**Date**: _______________
