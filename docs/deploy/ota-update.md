# OTA Firmware Update

> Cập nhật firmware từ xa (qua WiFi) — không cần USB cable. Critical sau khi robot đã deploy tại nhà người thân: tránh phải đi đến tận nhà mỗi lần fix bug.
>
> **Tiền đề**: partition table có 2 OTA slot (đã setup từ Phase 3, xem `firmware/partitions.csv`).
>
> **Phase**: 12 (scaffolding documented; Phase 13+ implement endpoint)

---

## 1. Partition layout

```
0x10000   app0     3MB    ← active app (đang chạy)
0x310000  app1     3MB    ← OTA target slot
0xf000    otadata  8KB    ← which slot is active
```

ESP32 boot loader đọc `otadata` để biết boot app0 hay app1. OTA process:
1. Robot download new firmware → ghi vào slot kia (app1 nếu đang chạy app0)
2. Verify image hash OK
3. Update `otadata` → set boot slot = app1
4. Reboot
5. (Optional safety) Sau N giây nếu app1 không "phone home" → rollback đến app0

---

## 2. Implementation (Phase 13+ — scaffold only)

ESP-IDF cung cấp:
- `esp_https_ota` — high-level OTA over HTTPS
- `esp_ota_*` — low-level API

Phase 12 chưa implement endpoint, chỉ document. Mục tiêu Phase 13:

### 2.1 Endpoint POST /ota

```c
static esp_err_t ota_handler(httpd_req_t *req) {
    esp_http_client_config_t cfg = {
        .url = "http://release.elderly-bot.local/firmware.bin",
        .cert_pem = NULL,
    };
    esp_https_ota_config_t ota_cfg = {
        .http_config = &cfg,
    };
    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err == ESP_OK) {
        esp_restart();
    } else {
        // return error
    }
    return ESP_OK;
}
```

### 2.2 MQTT trigger

Subscribe `elderly_robot/cmd/ota` với payload là URL:
```
mosquitto_pub -t 'elderly_robot/cmd/ota' -m 'http://192.168.1.50:8080/firmware.bin'
```

Robot tự download + apply.

### 2.3 Anti-brick rollback

Sau khi boot vào image mới, app phải `esp_ota_mark_app_valid_cancel_rollback()` trong vòng 5 phút. Nếu không (= crash / hang), bootloader tự rollback về app cũ.

```c
// In task_behavior_start() after init:
if (esp_ota_get_app_partition() != esp_ota_get_running_partition()) {
    // First boot of new image — start a 5-minute timer
    xTimerStart(...);
}

// Health check fires after 5 min healthy → mark valid
esp_ota_mark_app_valid_cancel_rollback();
```

---

## 3. OTA workflow (target end-state)

```
[Dev laptop]
   │
   │ pio run -e esp32-s3-cam
   │
   ▼
[Build artifact: .pio/build/esp32-s3-cam/firmware.bin]
   │
   │ scp → HTTP server tại nhà (Hassio addon "Local Backup" or nginx)
   │
   ▼
[release.local/firmware.bin]
   │
   │ MQTT publish elderly_robot/cmd/ota = URL
   │
   ▼
[Robot] → download → flash slot inactive → reboot → run new app
   │
   │ if healthy 5 min: mark_valid
   │ else: bootloader rollback to old slot
   │
   ▼
[Robot uptime healthy with new firmware]
```

---

## 4. Manual OTA (no MQTT, via web UI)

Phase 12 cũng chưa có. Plan cho Phase 13:

UI panel:
```
<input type=file id=fw accept=".bin">
<button onclick='uploadFw()'>Upload firmware</button>
```

```js
async function uploadFw() {
    const file = document.getElementById('fw').files[0];
    const buf = await file.arrayBuffer();
    await fetch('/ota/upload', {
        method: 'POST',
        body: buf,
        headers: {'Content-Type': 'application/octet-stream'}
    });
    // Robot reboot trong vài giây
}
```

Endpoint `/ota/upload` nhận stream binary, ghi vào partition tiếp theo qua `esp_ota_write()`.

---

## 5. Version tracking

App image có version trong `app_descriptor`:
```c
// In CMakeLists.txt project root:
PROJECT_VER "1.2.3"
```

Read tại runtime:
```c
const esp_app_desc_t *desc = esp_app_get_description();
ESP_LOGI(TAG, "version: %s", desc->version);
```

Publish vào `/status` JSON:
```json
{"phase": 12, "version": "1.2.3", ...}
```

HASS có thể track version → notify khi mismatch.

---

## 6. Safety considerations

### 6.1 Don't OTA while moving

OTA download takes 30-60s, write takes ~10s. Robot KHÔNG nên drive trong lúc đó.

Force precondition: chỉ accept OTA khi `behavior == IDLE` hoặc `behavior == DOCKED`.

```c
if (task_behavior_state() != BHV_IDLE && task_behavior_state() != BHV_DOCKED) {
    return ESP_FAIL;   // refuse
}
```

### 6.2 Don't OTA on low battery

Brick risk nếu mất nguồn giữa OTA write.

```c
if (battery_percent() < 30) return ESP_FAIL;
```

### 6.3 Signed images (production hardening)

ESP-IDF support secure boot + flash encryption. Phase 14+:
- Generate signing key
- Build with `CONFIG_SECURE_BOOT_V2=y`
- Burn eFuse → robot only boots signed image
- Anti-tamper

---

## 7. Rollout strategy

Multi-robot fleet (sau khi build > 1 robot):

1. **Canary**: deploy update lên 1 robot test trước, monitor 1 tuần
2. **Staged**: nếu OK, rollout 25% / 50% / 100% trong vòng 1 tháng
3. **Telemetry**: track version trong MQTT state, dashboard hiển thị fleet status

---

## 8. Phase 12 OTA status

| Capability | Phase 12 | Phase 13+ target |
|------------|----------|-------------------|
| 2 OTA partition slot | ✓ (partitions.csv) | — |
| Version trong status | partial (phase number) | + git commit hash |
| HTTPS OTA endpoint | ✗ | ✓ |
| MQTT trigger | ✗ | ✓ |
| Anti-brick rollback | ✗ | ✓ |
| Signed images | ✗ | Phase 14+ |
| Fleet dashboard | ✗ | Phase 14+ |

→ Phase 12 = production-ready core. OTA là **operational convenience** sau khi ship.
