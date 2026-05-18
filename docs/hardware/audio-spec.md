# Audio I/O Specification

> INMP441 I2S MEMS mic + MAX98357A I2S class-D amp + 3W 40mm speaker. Pattern Security Bot wiring + XiaoZhi firmware base.
>
> **Phase 7 scope**: hardware audio chain hoạt động (record + play + loopback). **Voice pipeline** (wake word + cloud STT + LLM + TTS) là một dự án phụ thuộc cloud — sẽ làm sau khi hardware verified.
>
> **Version**: 1.0 — 2026-05-18

---

## 1. I2S audio chain

```
[INMP441 mic] ──DIN──► ESP32-S3 ──DOUT──► [MAX98357A amp] ──► [Speaker 3W 8Ω]
                       I2S_NUM_0
                       16 kHz / 16-bit / mono / Philips standard

      Shared clock signals:
         BCLK  GPIO14 ──► both mic SCK and amp BCLK
         LRCLK GPIO12 ──► both mic WS and amp LRCLK
         DOUT  GPIO16  → only to amp DIN  (TX path)
         DIN   GPIO17  ← only from mic SD (RX path)
```

Đây là I2S full-duplex — TX và RX share BCLK + LRCLK nhưng dùng data line riêng. ESP32-S3 I2S peripheral hỗ trợ native (1 controller, 2 channel TX/RX).

---

## 2. Pin mapping

Theo `pin-mapping.md`:

| ESP32-S3 GPIO | Signal | Direction | Connected to | Note |
|---------------|--------|-----------|--------------|------|
| GPIO12 | LRCLK / WS | Output | INMP441 WS + MAX98357A LRCLK | **Strapping pin — pull-down 10kΩ required** |
| GPIO14 | BCLK / SCK | Output | INMP441 SCK + MAX98357A BCLK | — |
| GPIO16 | DOUT / SDOUT | Output | MAX98357A DIN | TX audio |
| GPIO17 | DIN / SDIN | Input | INMP441 SD | RX audio |

### 2.1 ⚠️ GPIO12 strapping note

GPIO12 vừa là **I2S LRCLK** vừa là **strapping pin** chọn flash voltage. Khi reset, nếu GPIO12 không có pull-down 10kΩ → chip có thể boot vào sai mode.

**Sau khi boot xong** (firmware đã chạy), GPIO12 chỉ là I/O bình thường — set HIGH/LOW không sao.

Pull-down R 10kΩ external **không ảnh hưởng** I2S LRCLK (LRCLK chạy mạnh override 10k pull-down). Cứ giữ trong design.

---

## 3. INMP441 — MEMS mic

### 3.1 Wiring

```
INMP441 module Shopee 6-pin:

VCC ────── +3.3V rail
GND ────── GND
SCK ────── BCLK GPIO14
WS  ────── LRCLK GPIO12
SD  ────── DIN GPIO17
L/R ────── GND (chọn left channel; data on WS LOW phase)
```

### 3.2 Output format

INMP441 outputs **24-bit signed PCM** in 32-bit slot, big-endian:
- 8 bit leading zeros
- 24 bit signed sample (MSB first)

Khi I2S configured 16-bit mode, ESP32 chỉ đọc 16 bit MSB của slot → bỏ 8 bit LSB. Quality enough cho voice (16-bit @ 16kHz = ~6dB headroom vs 24-bit).

Nếu muốn full 24-bit: dùng `I2S_DATA_BIT_WIDTH_32BIT` + shift right 8 in software.

### 3.3 Decoupling

| Component | Value | Mục đích |
|-----------|-------|----------|
| C_VDD | 100nF X7R 0603 | HF bypass — sát chân VDD |
| C_bulk | 10µF X7R 0603 | Mid-frequency bulk |

**Đặt mic XA motor + buck**. Trace VDD riêng từ LDO output, không share với MPU6050 hay OLED (xem `pcb-layout-power.md`).

### 3.4 Beam pattern

INMP441 omnidirectional, sensitivity -26 dBFS / Pa. Nghe được giọng nói rõ trong khoảng cách 1-3m.

---

## 4. MAX98357A — Class-D amplifier

### 4.1 Wiring

```
MAX98357A module Shopee 7-pin:

VIN ────── +5V rail (3.3V cũng work nhưng power thấp)
GND ────── GND
LRC ────── LRCLK GPIO12
BCLK ───── BCLK GPIO14
DIN ────── DOUT GPIO16
GAIN ───── GND (9 dB gain — moderate)
SD  ────── +3.3V (always-on; hoặc GPIO để mute từ firmware)
```

### 4.2 GAIN pin options

| GAIN connection | Gain |
|-----------------|------|
| GND | 9 dB |
| 100kΩ to GND | 6 dB |
| Floating (NC) | 12 dB |
| 100kΩ to VDD | 15 dB |
| VDD | 18 dB |

**Chọn GND = 9 dB** cho phòng 4×4m (đủ to để người già nghe, không quá ồn).

### 4.3 SD pin (shutdown)

- HIGH = active (output enabled)
- LOW = mute + low-power sleep
- Floating = follow internal logic (output channel select based on DIN bits)

Tie SD lên +3.3V để always-on. **Optional**: nối SD đến GPIO ESP32 để mute từ firmware (giảm hiss khi không phát).

### 4.4 Output stage

- Class-D PWM 1.4MHz
- 3.2W vào 4Ω, 2.5W vào 8Ω (5V VIN)
- No external filter cần — speaker tự lọc PWM

### 4.5 Decoupling

| Component | Value | Mục đích |
|-----------|-------|----------|
| C_VIN | 100µF aluminium low-ESR | Bulk cho switching burst |
| C_HF | 100nF X7R 0603 | HF bypass |
| C_mid | 10µF X7R 0603 | Mid-freq |

Class-D switching tạo nhiễu — đặt amp **xa mic ≥ 30mm**.

---

## 5. Speaker

### 5.1 Spec

- Diameter: 40mm
- Impedance: 8Ω
- Power: 3W max
- Frequency response: 200Hz - 8kHz (đủ cho voice)
- Mount: 4-hole flange hoặc adhesive

### 5.2 Vị trí

Đặt phía trước hoặc bên dưới chassis (mặt loa hướng người dùng). Tránh đặt sát mic — sẽ bị acoustic loopback.

Khoảng cách mic ↔ speaker ≥ 30mm. Nếu < 30mm → cần echo cancellation phức tạp.

### 5.3 Vibration isolation

Loa rung → có thể làm rung MPU6050. Đặt loa trên foam pad mỏng (3mm EVA) để giảm rung truyền vào PCB.

---

## 6. I2S configuration (firmware)

```c
// ESP-IDF v5 new i2s_std API
i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan);  // full-duplex

i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                    I2S_DATA_BIT_WIDTH_16BIT,
                    I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = 14,
        .ws   = 12,
        .dout = 16,
        .din  = 17,
        .invert_flags = {0, 0, 0},
    },
};
i2s_channel_init_std_mode(tx_chan, &std_cfg);
i2s_channel_init_std_mode(rx_chan, &std_cfg);
```

### 6.1 Sample rate choice

16 kHz đủ cho voice (Nyquist 8kHz > giọng nói 4kHz):
- Bandwidth: 16kHz × 16bit × 1ch = 32 KB/s (mono PCM)
- 5s audio = 160 KB → fit trong PSRAM dễ dàng
- Whisper API native chấp nhận 16 kHz

22kHz hoặc 44.1kHz là overkill cho voice, tăng dữ liệu mà không tăng quality nhận biết.

### 6.2 Mono vs stereo

INMP441 single chip → mono. MAX98357A mono output (hoặc avg L+R nếu input stereo).

Cấu hình `I2S_SLOT_MODE_MONO` → ESP32 RX/TX sample mono, BCLK chạy chậm hơn (1 channel thay 2) → power saving + bus simpler.

---

## 7. Voice pipeline plan (Phase 7+)

Phase 7 chỉ làm hardware verified. Voice pipeline đầy đủ là **post-Phase 7 effort**:

```
┌────────────────────────────────────────────────────────────┐
│                                                            │
│  Mic ─► I2S RX ─► VAD ─► Wake word detect (on-device)      │
│                          │                                 │
│                  (wake)  ▼                                 │
│              Buffer 5s ─► Upload via WebSocket             │
│                          │                                 │
│                          ▼                                 │
│              Cloud STT (Whisper-large or ElevenLabs)       │
│                          │                                 │
│                          ▼                                 │
│              LLM (Claude Sonnet, system prompt elderly)    │
│                          │                                 │
│                          ▼                                 │
│              TTS (Elevenlabs voice clone hoặc gtts)        │
│                          │                                 │
│                          ▼                                 │
│              MP3 stream → on-device decode ─► I2S TX       │
│                                                ▼           │
│                                            Speaker         │
└────────────────────────────────────────────────────────────┘
```

### 7.1 Components cần (phase tiếp)

| Component | Purpose | Source |
|-----------|---------|--------|
| ESP-SR (wake word) | On-device "Hey bot" detection | Espressif lib free |
| WebSocket client | Upload audio realtime | ESP-IDF built-in |
| MP3 decoder | TTS playback | `esp-adf` mp3 decoder |
| Claude API client | LLM brain | HTTPS POST JSON |
| ElevenLabs / OpenAI Whisper | STT | HTTPS POST audio |

### 7.2 Latency target (end-to-end)

- Wake → STT first token: 800 ms
- LLM first token: 600 ms
- TTS first audio: 400 ms
- **Total**: ~2 seconds (tolerable)

### 7.3 Cost ước

| Service | Cost / month (typical) |
|---------|------------------------|
| OpenAI Whisper API | $0.006 × 60 min = $0.36 |
| Claude Sonnet API | $3 / M input + $15 / M output tokens |
| ElevenLabs TTS | $5 / 30 min audio |
| **Total đoán** | **~$15-30/month** cho 1 user active 2h/day |

---

## 8. Schematic verification

Kiểm tra `audio.kicad_sch`:

- [ ] INMP441 connector 6-pin (VCC=3.3V, GND, SCK, WS, SD, L/R=GND)
- [ ] 100nF + 10µF decoupling sát chân INMP441 VDD
- [ ] MAX98357A module 7-pin connector
- [ ] +5V → MAX98357A VIN với 100µF + 100nF
- [ ] GAIN pin → GND (silk label "9dB GAIN")
- [ ] SD pin → +3.3V (hoặc GPIO nếu muốn mute control)
- [ ] Speaker terminal 2-pin (output + GND của MAX98357A)
- [ ] Test point cho BCLK, LRCLK, DOUT, DIN
- [ ] Layout note: amp ≥ 30mm xa mic

---

## 9. BOM

| Item | Qty | Source | Cost |
|------|-----|--------|------|
| INMP441 MH-ET-LIVE module | 1 | Đã có (Shopee) | 0 (đã sở hữu) |
| MAX98357A I2S amp combo XiaoZhi + speaker 3W 40mm | 1 set | Đã có (Shopee) | 0 (đã sở hữu) |
| Connector header 6-pin 2.54mm | 1 | Shopee | 1k |
| Connector header 7-pin 2.54mm | 1 | Shopee | 1k |
| Cap 100nF/10µF/100µF decoupling | (đã trong stock chung) | — | — |
| **TOTAL** | | | **~2k VND** (chỉ connector) |

---

## 10. Next phase

Sau Phase 7 pass (mic ghi rõ giọng, amp phát loop-back, không noise):
→ **Phase 8**: SIM800L cellular SOS (UART2, PWRKEY, gọi điện + SMS).
