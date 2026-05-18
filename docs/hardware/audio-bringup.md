# Audio I/O Bring-up

> Quy trình test mic + amp + loa sau khi Phase 6 (sensor) đã pass.
>
> **Tiền đề**: PCB power working, MCU bootup OK, INMP441 + MAX98357A combo lắp đặt theo `audio-spec.md`.
>
> **Dụng cụ**: PC với loa, oscilloscope (optional cho I2S verify), software audio player (Audacity hoặc browser).

---

## Phase 7.A — Build verify (5 phút)

### A.1 Rebuild

```bash
cd firmware/
pio run -e esp32-s3-cam -t upload
pio device monitor
```

Log mong đợi mới:
```
I (xxx) audio: init OK — BCLK=14 WS=12 DOUT=16 DIN=17, 16kHz/16b/mono
I (xxx) task_audio: task started — record buffer 320000 bytes in PSRAM
```

Nếu thấy `no PSRAM for record buf` → sdkconfig PSRAM chưa enabled. Verify Phase 3.

### A.2 Mở Web UI

URL `http://<ip>/` — phải thấy panel "Audio test" có 4 button:
- Tone 440Hz
- Tone 1kHz
- Loopback 3s
- Record 3s

Cộng link "Download WAV".

---

## Phase 7.B — TX (amplifier + speaker) test

### B.1 Tone 440Hz

- [ ] Bấm "Tone 440Hz"
- [ ] Nghe **note A4 chuẩn** từ loa, ~800ms
- [ ] Âm thanh sạch, không méo, không click

Nếu không nghe:
- Verify VIN MAX98357A = 5V (đo trực tiếp)
- Verify SD pin = HIGH (3.3V)
- Verify GAIN pin = GND
- Verify loa nối đúng 2 terminal output của amp

Nếu nghe nhưng nhiễu/giật:
- Cap 100µF input của amp thiếu hoặc kém ESR
- Trace VIN từ buck quá dài → sụt áp khi class-D switching

### B.2 Tone 1kHz

- [ ] Bấm "Tone 1kHz"
- [ ] Note cao hơn, vẫn rõ ràng, không buzz

### B.3 Multiple frequencies

Test bằng curl:
```bash
for hz in 200 500 1000 2000 4000; do
  curl "http://<ip>/audio/tone?freq=$hz&ms=500&vol=60"
  sleep 1
done
```

- [ ] Mỗi frequency phát ra đúng pitch
- [ ] Volume tương đối đều ở 200-4000Hz (loa 40mm cắt thấp ở < 200Hz)

### B.4 Volume sweep

```bash
for v in 10 30 50 70 90; do
  echo "vol=$v"
  curl "http://<ip>/audio/tone?freq=1000&ms=500&vol=$v"
  sleep 1
done
```

- [ ] Volume tăng dần khi vol parameter tăng
- [ ] Tại vol=90: loa to nhưng không méo
- [ ] Tại vol=100: có thể nghe clipping/distortion → bình thường

---

## Phase 7.C — I2S signal verification (optional, oscilloscope)

Bấm "Tone 1kHz" liên tục (lặp):
```bash
while true; do curl -s "http://<ip>/audio/tone?freq=1000&ms=500" > /dev/null; sleep 0.6; done
```

Probe oscilloscope:

| Pin | Mong đợi |
|-----|----------|
| GPIO14 (BCLK) | Sóng vuông ~512 kHz (16 bit × 16 kHz × 2 phase) |
| GPIO12 (LRCLK / WS) | Sóng vuông 16 kHz, duty 50% |
| GPIO16 (DOUT) | PCM data sync với BCLK, modulated by sine wave |

Tất cả 3 signal phải đồng pha, logic 0-3.3V sạch, không ringing > 0.5V.

---

## Phase 7.D — RX (microphone) test

### D.1 Record 3s

1. Nói vào mic: "Test một hai ba"
2. Bấm "Record 3s" trong web UI
3. Log:
   ```
   I (xxx) task_audio: recording 48000 samples (3000 ms)
   I (xxx) task_audio: recorded 48000 samples
   ```
4. Bấm link "Download WAV" → file `recording.wav` download

### D.2 Verify WAV trên PC

Mở `recording.wav` bằng:
- **Audacity** (free) — view waveform + listen
- **Browser**: kéo file vào tab Chrome
- **VLC media player**

- [ ] File play được, nghe rõ giọng "Test một hai ba"
- [ ] Waveform có biên độ ~50-80% (-3 to -6 dBFS) khi nói gần mic 20cm
- [ ] Không có click/pop ở đầu file (bug DMA frame đầu — đã skip trong driver)
- [ ] Noise floor < -50 dBFS (im lặng phải im)

### D.3 Distance sensitivity

- [ ] Record từ 1m: vẫn nghe được giọng, nhưng nhỏ hơn
- [ ] Record từ 3m: rất nhỏ — gần ngưỡng noise
- [ ] Record gần loa khi loa đang phát (cảnh báo: acoustic loopback — feedback) → bypass test này

### D.4 Frequency response check

Record khi phát tone từ phone:
- [ ] Phát tone 1kHz từ phone, để gần mic 20cm
- [ ] Record 2s
- [ ] WAV waveform là sine wave 1kHz rõ ràng
- [ ] FFT analysis trong Audacity: peak chính ở 1kHz, không có harmonic > -30dB

---

## Phase 7.E — Loopback test (mic → amp → speaker)

Đây là test **end-to-end** của audio chain.

### E.1 Quiet room

Đặt robot trong phòng yên tĩnh.

- [ ] Bấm "Loopback 3s"
- [ ] Đèn LED PSRAM busy nhấp nháy
- [ ] Trong 3s đầu: nói "Test loopback một hai ba"
- [ ] Sau 3s: loa phát lại chính giọng nói đó
- [ ] Quality: hiểu được, nhưng có thể hơi nhỏ + có chút EMI noise

### E.2 Echo cancellation

Loopback có **không có** echo cancellation. Khi loa phát to + mic gần loa, audio sẽ tự loop về mic → feedback (rít).

**Solution Phase 7+ (voice pipeline)**:
- Khi TTS phát → mute mic (gpio control hoặc software gate)
- Hoặc dùng adaptive echo cancellation (esp-sr library hỗ trợ)

Phase 7 cơ bản không cần — chỉ verify chain hoạt động.

### E.3 Quality regression test

Sau 1 giờ idle, lặp lại E.1:
- [ ] Loopback quality không tệ đi
- [ ] Mic noise floor không tăng (DC bias drift)
- [ ] Loa không bị méo (cap output thoái hóa)

---

## Phase 7.F — Stress test (15 phút)

### F.1 Continuous loopback

```bash
for i in {1..30}; do
  curl -s "http://<ip>/audio/loopback?ms=3000"
  sleep 4
done
```

30 lần × 3s record + 3s play + 1s gap = 7 phút.

- [ ] Tất cả 30 lần loopback work
- [ ] Không có DMA underrun trong serial log
- [ ] MAX98357A ấm nhẹ (< 50°C)
- [ ] Robot vẫn responsive (PTZ, drive, sensor vẫn work) — audio không block tasks khác

### F.2 Tone burst stress

```bash
for i in {1..50}; do
  freq=$((RANDOM % 3950 + 50))
  curl -s "http://<ip>/audio/tone?freq=$freq&ms=200&vol=70"
  sleep 0.3
done
```

- [ ] Cả 50 tone phát ra (có thể nghe như nhạc rất tạp)
- [ ] Amp không thermal shutdown
- [ ] Mic recordings vẫn work sau test

---

## Common issues

| Triệu chứng | Nguyên nhân | Fix |
|-------------|-------------|-----|
| No sound from speaker | Amp SD pin LOW or floating | Pull SD HIGH to 3.3V |
| Sound very quiet | GAIN pin = GND (9dB) thấp | Try GAIN = NC (12dB) or VCC (15dB) — không quá vol vẫn được |
| Sound distorted at low vol | VIN của amp dao động | Tăng cap input 100µF → 220µF |
| Sound clicks/pops | DMA underrun | Increase dma_desc_num in audio_i2s.c |
| Mic records only static | L/R pin của INMP441 floating | Tie L/R = GND |
| Mic records but with constant tone | I2S clock conflict với camera | Verify Timer 0 only used by camera XCLK |
| WAV not playable | Header sai endianness | Check WAV format spec section 6 |
| Loud rít khi loopback | Acoustic feedback (mic gần loa) | Move mic + speaker xa nhau ≥ 30mm |
| ESP32 reboot khi loopback | Memory leak PSRAM | Check task_audio frees buffer if reused |
| Audio quality nghe stuttering | Other task hogging CPU | Increase task_audio priority (5 → 6) |

---

## Phase 7 — Sign-off

- [ ] Tone 440Hz + 1kHz phát đúng cao độ
- [ ] Volume parameter scale linear 10..90
- [ ] Record 3s tạo file WAV download được
- [ ] WAV play trên PC nghe rõ giọng
- [ ] Loopback 3s record + play back end-to-end
- [ ] 30 lần loopback liên tiếp không lỗi
- [ ] Audio không block các task khác (PTZ, drive, sensor)

→ **Tiếp theo**: Phase 8 — SIM800L cellular SOS (gọi điện + SMS qua AT command, UART2 trên GPIO46/47/48).

## Voice pipeline future plan

Phase 7 chỉ verify hardware. Voice pipeline đầy đủ (wake word → STT → LLM → TTS) là **separate effort**:

1. Integrate **esp-sr** library cho wake word "Hey bot" on-device
2. Add WebSocket client gửi audio sau wake → cloud STT
3. Add Claude API client với system prompt elderly care
4. Add MP3 decoder (esp-adf) để play TTS response
5. Implement mute control để chống acoustic loopback
6. Tune latency end-to-end < 2s

Tham khảo `docs/HDSD-Lap-Rap-Robot.md` Phase 7 cho pipeline diagram.
