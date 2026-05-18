# Hướng dẫn lắp ráp Phase 1 — Chassis

> Bước-by-bước để biến 2 tấm acrylic + linh kiện thành robot chassis có 3 điểm tiếp đất, sẵn sàng cho Phase 2 (power tree).
>
> **Thời gian dự kiến**: 60-90 phút (lần đầu)
> **Dụng cụ cần**: tua vít Phillips PH0+PH1, cờ-lê 5.5mm (cho đai ốc M3), pince mỏ nhọn, thước kẹp digital

---

## Trước khi bắt đầu

### Kiểm tra hàng nhận

- [ ] 2 tấm acrylic đã laser-cut theo `chassis-layer1.svg` và `chassis-layer2.svg`
- [ ] Đã verify theo mục 10 của `chassis-spec.md` (đo outline, thử vít, chồng tấm)
- [ ] 6 standoff M3×30mm đầy đủ
- [ ] 12 vít M3×6 + 12 đai ốc M3
- [ ] 4 vít M2×6 + 4 đai ốc M2
- [ ] 2 BO motor + bracket + bánh xe 65mm
- [ ] 1 caster wheel + 4 vít kèm theo
- [ ] 1 holder 18650 3-cell

### Bóc film bảo vệ acrylic

Acrylic mới giao có lớp giấy/film 2 mặt. **Để nguyên khi lắp ráp** — chống xước. Chỉ bóc khi đã hoàn thành toàn bộ robot.

---

## Bước 1 — Cột đồng vào layer 1 (5 phút)

1. Đặt **layer 1** (tấm có lỗ PTZ servo 22×12mm) lên bàn, mặt có vết khắc/cắt **úp xuống**.
2. Cắm 6 cột đồng M3×30 từ **trên xuống** qua 6 lỗ standoff.
3. Vặn 6 vít M3×6 từ **mặt dưới** (mặt đang ngửa lên) vào đầu cái của standoff.
4. Vặn chặt vừa phải bằng tay (không cần lực mạnh — sẽ làm nứt acrylic).

**Kiểm tra**: 6 cột đồng đứng thẳng vuông góc với tấm. Nếu nghiêng → vít quá chặt, nới ra.

---

## Bước 2 — Lắp motor + bánh xe vào layer 2 (15 phút)

1. Đặt **layer 2** (tấm có notch khoét 2 bên cạnh) lên bàn.
2. Lấy 2 motor BO + bracket. Tháo bracket khỏi motor tạm (2 vít).
3. Đặt bracket vào vị trí: 2 lỗ M3 của bracket phải khớp với 2 lỗ (14, 65) và (14, 85) bên trái.
4. Vặn 2 vít M3×6 từ **trên** layer 2 xuống, có đai ốc bên dưới.
5. Gắn motor vào bracket (2 vít kèm bracket).
6. Lắp bánh xe vào trục motor — đẩy mạnh đến hết, bánh khít trục.
7. Lặp lại cho motor phải.

**Kiểm tra**: 2 bánh xe quay tự do không cọ vào edge notch. Nếu cọ → kiểm tra notch có đủ rộng 8×34mm không.

---

## Bước 3 — Lắp caster wheel (5 phút)

1. Đặt caster wheel mount plate ở vị trí 4 lỗ caster (X=88-113, Y=20-45) trên layer 2.
2. Vặn 4 vít M3×6 + đai ốc.
3. Caster quay 360° tự do.

**Kiểm tra**: Đặt cả layer 2 lên bàn — 2 bánh BO + caster cùng chạm bàn, không kênh. Nếu kênh → caster quá ngắn hoặc bánh BO quá lớn.

---

## Bước 4 — Lắp battery holder (5 phút)

1. Đặt holder 18650 3-cell lên layer 2, 4 lỗ holder khớp với 4 lỗ battery (65/135, 58/123).
2. Vặn 4 vít M3×6 + đai ốc.
3. **Chưa cắm pin** — để Phase 2 (sau khi đo điện áp các rail).

**Kiểm tra**: Holder không lung lay, các tab kim loại của holder không chạm vào vít/đai ốc (gây chập).

---

## Bước 5 — Lắp PTZ servo Pan vào layer 1 (10 phút)

1. Lấy 1 con SG90 (chọn từ 8 con MG90S sẵn có).
2. Đẩy body servo qua lỗ 22×12 từ **trên xuống**. Body nằm dưới chassis, 2 flange (cánh) tựa trên mặt chassis.
3. Khớp 2 lỗ M2 trên flange với 2 lỗ (86, 31) và (114, 31).
4. Vặn 2 vít M2×6 + đai ốc M2.

**Kiểm tra**: Servo cố định chắc, không xoay được. Cấp 5V vào servo và gửi PWM 1.5ms → horn xoay đến 90° (center).

---

## Bước 6 — Lắp servo Tilt + camera (15 phút)

1. Servo tilt gắn lên **horn của servo pan** (không gắn lên chassis). Dùng plastic mount nhỏ kèm SG90 servo set hoặc tự in 3D.
2. Cố định ESP32-S3-CAM lên **horn của servo tilt** sao cho ống kính nằm dọc trục tilt.
3. Định vị: khi pan=90° tilt=90° → camera nhìn thẳng phía trước robot.

**Quan trọng**: Để dây cáp servo + camera đủ dài, có vòng dự phòng để khi pan-tilt quay không kéo căng dây.

**Kiểm tra**: Quay pan 0° → 180° và tilt 30° → 150° bằng tay. Cáp không bị căng, không đụng PCB.

---

## Bước 7 — Lắp 2 layer lại với nhau (10 phút)

1. Đặt **layer 2** (đã có motor + caster + battery holder) lên bàn.
2. Hạ **layer 1** (đã có 6 standoff lủng lẳng phía dưới + servo PTZ phía trên) xuống. 6 đầu cái của standoff phải khớp với 6 lỗ tương ứng trên layer 2.
3. Lật cả robot lên (layer 2 ở trên, layer 1 ở dưới).
4. Vặn 6 vít M3×6 từ mặt ngoài layer 2 vào đầu cái standoff.
5. Lật lại robot — layer 1 ở trên, layer 2 ở dưới (vị trí cuối cùng).

**Kiểm tra**:
- 2 tầng vuông góc 90° (dùng eke nếu khó tính bằng mắt)
- Khoảng cách 2 tầng = 30mm ± 1mm
- Lắc thử — không lung lay

---

## Bước 8 — Cân bằng & vận hành thử cơ học (5 phút)

1. Đặt robot lên mặt phẳng (bàn / sàn).
2. **Kiểm tra**:
   - Cả 3 điểm (2 bánh + caster) đều chạm sàn
   - Robot không nghiêng > 5° về bất cứ hướng nào (dùng MPU6050 sau hoặc dùng level)
   - Đẩy nhẹ → robot lăn tự do không kẹt
3. **Lắc test**:
   - Cầm robot lên, lắc nhẹ 5 lần
   - Mọi linh kiện vẫn cố định, không có tiếng kêu loẹt xoẹt
   - Vít không lỏng

---

## Common pitfalls

| Lỗi | Nguyên nhân | Cách sửa |
|-----|-------------|---------|
| Nứt acrylic gần lỗ vít | Vặn quá chặt | Nới lỏng 1/2 vòng, dùng washer cao su |
| 2 layer không vuông góc | Standoff không vặn chặt | Vặn lại cả 6 con đều tay |
| Bánh xe cọ chassis | Notch không đủ sâu | Đặt lại tấm, hoặc dùng mũi khoan nới rộng notch (cẩn thận) |
| Caster kênh | Caster ngắn hơn bánh BO | Thêm washer M3 dưới mount caster cho cao lên |
| Servo PTZ lỏng | Cắt PTZ slot rộng quá | Dán băng dính 2 mặt 1mm vào body trước khi lắp |
| Robot nghiêng về 1 bên | Trọng lượng không đối xứng | Đổi vị trí pin / linh kiện cho đối xứng |

---

## Hoàn thành Phase 1 — Kết quả mong đợi

Sau khi hoàn thành 8 bước trên, bạn có:

✓ Robot chassis 2 tầng, kích thước 210×150×130mm (tính cả bánh)
✓ 3-point stable: 2 bánh BO + 1 caster
✓ PTZ servo pan-tilt + camera ESP32-S3 lắp sẵn (chưa wiring)
✓ Battery holder 3-cell 18650 (chưa cắm pin)
✓ Khung sẵn sàng cho Phase 2 (lắp PCB + power tree)

**Chụp ảnh chassis hoàn thành** để theo dõi tiến độ, lưu vào `docs/photos/phase1-chassis.jpg`.

→ **Tiếp theo**: Phase 2 trong `docs/HDSD-Lap-Rap-Robot.md`.
