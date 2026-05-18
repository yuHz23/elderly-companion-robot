# Chassis Specification — Elderly Companion Robot

> Authoritative dimensions cho chassis 2 tầng acrylic. Mọi file CAD/laser-cut phải khớp với spec này.
>
> **Version**: 1.0 — 2026-05-18
> **Material**: Acrylic 3mm (đúc hoặc đùn — đúc cắt sạch hơn)
> **Tool**: CO2 laser 40W+ (laser cut tại Shopee ~25-50k VND/tấm)

---

## 1. Sheet specifications

| Tham số | Giá trị |
|---------|---------|
| Kích thước tổng (L × W) | 200 × 150 mm |
| Vật liệu | Acrylic trong/mờ, **3mm** dày |
| Số lượng | 2 tấm (layer 1 + layer 2) |
| Diện tích | 0.03 m² mỗi tấm |
| Số file | `chassis-layer1.svg`, `chassis-layer2.svg` |

## 2. Hệ trục toạ độ

- Gốc (0, 0) tại **góc trên trái** của SVG
- Trục X tăng sang phải (0 → 200)
- Trục Y tăng xuống dưới (0 → 150)
- **FRONT của robot** = cạnh Y=0 (phía trên SVG)
- **BACK của robot** = cạnh Y=150 (phía dưới SVG)

## 3. Standoff hole pattern (CHUNG cả 2 layer)

6 lỗ M3 (đường kính 3.2mm) — **bắt buộc khớp toạ độ chính xác** giữa 2 layer để 6 cột đồng xuyên qua được.

| # | X (mm) | Y (mm) | Đường kính | Mục đích |
|---|--------|--------|------------|----------|
| 1 | 10  | 10  | 3.2 | Góc back-left |
| 2 | 100 | 10  | 3.2 | Giữa cạnh back |
| 3 | 190 | 10  | 3.2 | Góc back-right |
| 4 | 10  | 140 | 3.2 | Góc front-left |
| 5 | 100 | 140 | 3.2 | Giữa cạnh front |
| 6 | 190 | 140 | 3.2 | Góc front-right |

**Standoff dùng**: cột đồng hexagonal M3 × 30mm cao, đực-cái hoặc cái-cái (tuỳ chọn).
**Vít**: M3 × 6mm đầu phẳng (4 con — bắt từ trên xuống standoff, và 4 con từ dưới lên).

---

## 4. Layer 1 (TOP) — Chi tiết features

### 4.1 PTZ servo mount

Lắp servo SG90/MG90S pan ở **phía trước-trung tâm** robot.

| Feature | X (mm) | Y (mm) | Kích thước | Note |
|---------|--------|--------|------------|------|
| Body cutout | 89 → 111 | 25 → 37 | 22 × 12 mm rect | Body servo SG90 đút lọt |
| Flange hole L | 86 | 31 | dia 2.2mm | M2 mount |
| Flange hole R | 114 | 31 | dia 2.2mm | M2 mount |

**Khoảng cách 2 lỗ M2**: 28mm (chuẩn SG90/MG90S).
**Hướng servo**: trục pan vuông góc với mặt chassis, motor body cắm xuống dưới chassis, horn quay phía trên.

### 4.2 Main PCB mount

4 lỗ M3 cho custom KiCad PCB.

| Feature | X (mm) | Y (mm) | Đường kính |
|---------|--------|--------|------------|
| PCB hole NW | 64  | 54  | 3.2 |
| PCB hole NE | 136 | 54  | 3.2 |
| PCB hole SW | 64  | 106 | 3.2 |
| PCB hole SE | 136 | 106 | 3.2 |

**Pattern**: 72 × 52mm (PCB ~80×60mm với margin 4mm mỗi cạnh).

**Nếu PCB cuối khác kích thước**: re-export SVG với toạ độ mới, đừng khoan bù.

### 4.3 Cable pass-through slot

| Feature | X (mm) | Y (mm) | Kích thước |
|---------|--------|--------|------------|
| Slot | 15 → 35 | 125 → 135 | 20 × 10 mm với góc R2mm |

Vị trí back-left để luồn dây từ PCB xuống motor + battery ở layer 2.

---

## 5. Layer 2 (BOTTOM) — Chi tiết features

### 5.1 Wheel clearance notches

Cắt khoét 2 bên cạnh để bánh xe quay không cọ vào acrylic.

| Cạnh | X range | Y range | Độ sâu × Chiều dài |
|------|---------|---------|---------------------|
| Trái | 0 → 8 | 58 → 92 | 8 × 34 mm |
| Phải | 192 → 200 | 58 → 92 | 8 × 34 mm |

Bánh BO 65mm dia, motor mount cách trục bánh ~12mm. Notch 8mm đảm bảo clearance 6-8mm với bánh xe.

### 5.2 BO motor brackets

Bracket nhựa hình L bắt từ trên xuống chassis. 2 lỗ M3 mỗi bracket.

| Bracket | Hole 1 (X, Y) | Hole 2 (X, Y) | Spacing |
|---------|---------------|---------------|---------|
| Trái | (14, 65) | (14, 85) | 20mm |
| Phải | (186, 65) | (186, 85) | 20mm |

**Vị trí trục bánh xe** (tham chiếu): X ≈ -5 (trái), X ≈ 205 (phải) — tức là bánh nằm ngoài footprint chassis 5mm mỗi bên.

**Robot width tổng** (kể cả bánh): ~210mm.

### 5.3 Caster wheel mount

4 lỗ M3 vuông 25mm × 25mm ở **front-center**.

| Hole | X (mm) | Y (mm) |
|------|--------|--------|
| NW | 88  | 20 |
| NE | 113 | 20 |
| SW | 88  | 45 |
| SE | 113 | 45 |

**Caster dùng**: bánh xoay 360° đường kính 25mm hoặc bi tròn (ball caster). Mount plate 25×25mm.

### 5.4 18650 battery holder

Holder 3-cell 18650 in series. Body ~80×70mm.

| Hole | X (mm) | Y (mm) |
|------|--------|--------|
| NW | 65  | 58 |
| NE | 135 | 58 |
| SW | 65  | 123 |
| SE | 135 | 123 |

**Pattern**: 70 × 65mm.
**Vị trí**: chính giữa chassis (đối xứng X), hơi lệch về back để dồn trọng lượng phía sau (giúp caster wheel ấn xuống tốt).

### 5.5 Cable slot

Cùng vị trí với layer 1 (15-35, 125-135). Để dây xuyên 2 tầng.

---

## 6. Cân bằng trọng lực & ổn định

### 6.1 Trọng lượng dự kiến

| Component | Vị trí | Mass (g) |
|-----------|--------|----------|
| Tấm acrylic ×2 | spread | 100 |
| Cột đồng ×6 | corners | 30 |
| ESP32-S3-CAM | layer 1 center | 15 |
| OLED + spkr + mic | layer 1 left | 30 |
| PTZ + 2 servo | layer 1 front | 30 |
| Camera lens | PTZ tilt | 5 |
| 3× 18650 | layer 2 center | 150 |
| Holder + BMS | layer 2 center | 50 |
| L298N + motors + brackets | layer 2 sides | 120 |
| Caster | layer 2 front | 20 |
| Bánh xe ×2 | sides | 60 |
| Wire + misc | distributed | 50 |
| **TOTAL** | | **~660g** |

### 6.2 Trọng tâm

- X centroid ≈ 100mm (đối xứng)
- Y centroid ≈ 80mm (gần giữa) — biased toward back nhờ vị trí battery
- Z centroid: pin nặng nhất → ở layer 2 → trọng tâm thấp → ổn định tốt

### 6.3 Khoảng cách 2 tầng

Standoff M3 × **30mm** → khoảng trống giữa 2 tầng ~30mm (đủ chứa 18650 cao 19mm + holder 5mm + margin 6mm).

---

## 7. Tolerance & dung sai

| Feature | Nominal | Tolerance | Lý do |
|---------|---------|-----------|-------|
| Lỗ M3 | 3.2 mm | ±0.1 mm | M3 screw clearance |
| Lỗ M2 | 2.2 mm | ±0.1 mm | M2 screw clearance |
| Outer outline | 200×150 mm | ±0.5 mm | Không critical |
| Standoff XY | exact | ±0.2 mm | **Cả 2 layer phải khớp** |
| Wheel notch | 8×34 mm | ±0.5 mm | Bánh quay không cọ |

Laser CO2 thường có kerf ~0.15mm. Đặt design ở **nominal** — không bù kerf. Lỗ sẽ nhỏ hơn nominal ~0.15mm, ổn vì M3 clearance vẫn pass.

---

## 8. Order & cost

**Đặt laser-cut tại**: Shopee shop "Laser cắt CNC" (search keyword "laser cắt acrylic theo file"), hoặc tiệm địa phương.

| Item | Spec | Số lượng | Giá ước (VND) |
|------|------|----------|---------------|
| Acrylic 3mm trong, cắt theo file | 200×150mm | 2 | 30,000-50,000 |
| Phí thiết kế phụ (nếu có) | — | — | 0 (đã có file) |

**Format file**: gửi SVG hoặc DXF (xem `generate_dxf.py` nếu cần DXF).

**Lead time**: 1-2 ngày làm việc.

---

## 9. Mechanical BOM

Hardware cần để lắp ráp Phase 1:

| Item | Spec | Qty | Source | Note |
|------|------|-----|--------|------|
| Tấm acrylic 3mm cắt sẵn | 200×150mm | 2 | Laser shop | Theo SVG |
| Standoff đồng M3 | M3×30mm cái-cái | 6 | Shopee | "cột đồng M3 cái cái 30mm" |
| Vít M3 phẳng | M3×6mm | 12 | Shopee | Mua hộp ~50 con |
| Đai ốc M3 | — | 12 | Shopee | |
| Vít M2 mặt phẳng | M2×6mm | 4 | Shopee | Lắp servo SG90 |
| Đai ốc M2 | — | 4 | Shopee | |
| BO motor + bracket + bánh 65mm | DC 6V có encoder | 2 | Shopee "BO motor có encoder" | Combo motor+bracket+wheel |
| Caster wheel | dia 25mm + plate 25×25 | 1 | Shopee | "bánh xe omni caster mini" |
| 18650 battery holder | 3-cell series + leads | 1 | Shopee | Đảm bảo có lỗ M3 mount |
| 18650 cells | 3000mAh có protection | 3 | Shopee | Pin Samsung/LG/Sony chính hãng |

**Tổng cost Phase 1**: ~200,000-300,000 VND (chưa kể pin).

---

## 10. Quy trình verify khi nhận hàng

1. **Đo outline 2 tấm**: 200 ± 1mm × 150 ± 1mm
2. **Đo lỗ M3**: cắm thử vít M3 → vào lọt, không lỏng quá
3. **Đo vị trí standoff**: đặt 2 tấm chồng lên nhau, chiếu sáng từ dưới → 6 lỗ phải xuyên qua nhau (sai lệch < 0.5mm)
4. **Đo notch bánh xe**: lắp thử motor + bánh → bánh quay 360° không chạm acrylic
5. **Đo PTZ slot**: cắm thử SG90 vào lỗ 22×12 → vừa khít, không quá lỏng

Nếu fail bất kỳ điểm nào → **đặt lại**, đừng cố sửa bằng giũa (mỏng dần, gãy dễ).

---

## 11. Next phase

Sau khi hoàn thành Phase 1 (chassis assembled, đứng cân bằng), chuyển sang:

→ **Phase 2** — Power tree (xem `docs/HDSD-Lap-Rap-Robot.md` section "Phase 2")
