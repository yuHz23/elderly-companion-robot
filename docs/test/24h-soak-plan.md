# 24h Soak Test

> Stress test dài hạn — robot tự chạy 24 giờ không can thiệp. Verify memory leak, thermal drift, accumulated error, charge cycle.
>
> **Pre-requirement**: All Test 1-7 trong `test-plan.md` đã pass.

---

## 1. Setup

### 1.1 Environment

- Phòng có dock station hoạt động
- Vùng patrol ~ 2×2m, có vật chắn (cố định, không di chuyển)
- WiFi stable, AC dock cắm điện
- Nhiệt độ phòng 22-28°C (không bật điều hòa lên/xuống — gây nhiễu IMU)

### 1.2 Initial state

- [ ] Battery 100% sạc đầy
- [ ] Robot ở dock, state CHARGED
- [ ] WiFi connected
- [ ] SOS contacts đã configured (test mode: dùng số chính bạn, đừng làm phiền người nhà)

### 1.3 Logging setup

Capture serial log ra file:
```bash
pio device monitor -e esp32-s3-cam | tee soak-$(date +%Y%m%d).log
```

Hoặc parallel với HTTP polling mỗi 60s:
```bash
#!/bin/bash
# poll-status.sh
while true; do
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] $(curl -s http://<ip>/status)"
    sleep 60
done > metrics-$(date +%Y%m%d).log
```

---

## 2. Test script — Random patrol + dock cycle

24 giờ chia thành **4 chu kỳ 6h**, mỗi chu kỳ:
- 3h patrol (random nav)
- ~1h return home + dock
- 2h charge to full → idle on dock

### 2.1 Patrol script

```bash
#!/bin/bash
# soak-patrol.sh — gọi mỗi 3 giây trong 3h
END=$(($(date +%s) + 10800))
while [ $(date +%s) -lt $END ]; do
    lin=$((RANDOM % 60 - 20))    # -20..40
    ang=$((RANDOM % 80 - 40))    # -40..40
    curl -s "http://<ip>/drive/velocity?linear=$lin&angular=$ang" > /dev/null
    sleep 3
done
curl -s "http://<ip>/drive/stop"
echo "patrol done — sending dock command"
curl -s "http://<ip>/behavior/dock"
```

Sau script này robot tự dock.

### 2.2 Auto-cycle full 24h

```bash
#!/bin/bash
# soak-24h.sh — 4 cycles × 6h
for cycle in 1 2 3 4; do
    echo "=== Cycle $cycle/4 start at $(date) ==="

    # Wait until robot is CHARGED + idle
    while [ "$(curl -s http://<ip>/behavior/state | jq -r '.name')" != "DOCKED" ] &&
          [ "$(curl -s http://<ip>/dock/state | jq -r '.state_name')" != "CHARGED" ]; do
        sleep 30
    done

    # Leave dock
    curl -s http://<ip>/behavior/leave
    sleep 5

    # Patrol 3h
    ./soak-patrol.sh

    # Return home (script already calls /behavior/dock)
    # Wait until DOCKED + CHARGED (max 3h)
    timeout 10800 bash -c 'while [ "$(curl -s http://<ip>/dock/state | jq -r .state_name)" != "CHARGED" ]; do sleep 60; done'

    echo "=== Cycle $cycle done at $(date) ==="
done
```

---

## 3. Metrics to capture

Mỗi 60s (qua `poll-status.sh`):

| Metric | Source | Acceptance |
|--------|--------|------------|
| `free_heap` | `/status` | > 100KB ổn định, không giảm dần |
| `min_free_heap` | `/status` | > 80KB tổng thể |
| `uptime_s` | `/status` | tăng đều đặn (không reboot) |
| `battery.v` | `/dock/state` | giảm patrol, tăng charging |
| `battery.pct` | `/dock/state` | cycle 0% ↔ 100% mỗi 6h |
| `behavior.name` | `/behavior/state` | thay đổi theo cycle |
| `dock.state_name` | `/dock/state` | IDLE → SEARCH → CHARGING → CHARGED |
| `sim800.state` | `/sim800/status` | READY xuyên suốt (không tự reboot) |
| WiFi RSSI | log esp_wifi | giữ > -75 dBm |

---

## 4. Pass/fail criteria

### 4.1 Critical (any fail = test fail)

- [ ] **0 reboot trong 24h** (uptime tăng đều, không reset to 0)
- [ ] **0 panic / TWDT / brownout** trong serial log
- [ ] **Memory không leak**: `min_free_heap` cuối > 70% giá trị ban đầu
- [ ] **Charge cycle complete** 4/4 lần (battery về > 95%)
- [ ] **0 fall false positive** trong 24h

### 4.2 Quality (≤ 1 fail acceptable)

- [ ] Dock success rate 4/4 cycles
- [ ] SIM800 không tự reboot
- [ ] WiFi disconnect/reconnect < 3 lần
- [ ] Thermal: ESP32 < 65°C, motor driver < 60°C

### 4.3 Informational

- [ ] Đếm số lần obstacle gate brake — báo cáo trung bình/cycle
- [ ] Đếm số lần fall candidate (spike > 2.5g) — false candidate có thể nhiều
- [ ] Đếm số lần WiFi reconnect

---

## 5. Common 24h failures

| Triệu chứng | Nguyên nhân | Fix |
|-------------|-------------|-----|
| Reboot mỗi vài giờ | Slow memory leak | Run heap_caps_check trong task_behavior |
| Battery drift (full → not full sau 1 cycle) | BMS calibration | Reset BMS bằng deep-discharge + full charge |
| SIM800L lose reg lặp lại | Power dip + slow restart | Tăng cap output buck #2 lên 1000µF |
| WiFi keep disconnect | Router DHCP lease ngắn | Set static IP cho robot |
| Dock success giảm cycle 4 vs 1 | IR LED hot drift | Heatsink cho LED, hoặc giảm duty |
| OLED freeze sau 12h | I2C bus locked | Add I2C recovery routine in i2c_bus |
| Motor weak after 24h | Brush wear | Bình thường — BO motor cheap; vài tháng cần thay |

---

## 6. Post-soak analysis

Sau 24h, archive:
- Serial log → `docs/test/soak-results/soak-YYYYMMDD.log.gz`
- Status metrics → `metrics-YYYYMMDD.log.gz`
- Plot battery voltage over time (python + matplotlib)

```python
# plot-soak.py — quick visualization
import json, matplotlib.pyplot as plt
lines = open('metrics-20260518.log').read().splitlines()
ts, v, pct = [], [], []
for ln in lines:
    if '"battery":' in ln:
        j = json.loads(ln.split('] ', 1)[1])
        # ... parse and plot
plt.plot(ts, v); plt.show()
```

Identify anomalies: voltage step (= dock failed mid-charge), heap dips (= ephemeral leak).

---

## 7. Sign-off

| Item | Pass/Fail | Comment |
|------|-----------|---------|
| 24h no reboot | | |
| Memory stable | | |
| 4 charge cycles | | |
| No fall FP | | |
| Dock 4/4 success | | |

**Tester**: ____________________  **Date**: _______________

→ Sau 24h soak pass → **ship**. Phase 12 (deploy + Home Assistant integration) là production hardening.
