# Mechanical — Elderly Companion Robot

Cơ khí chassis cho robot. Phase 1 của build guide (`docs/HDSD-Lap-Rap-Robot.md`).

## Files

| File | Mục đích |
|------|----------|
| `chassis-spec.md` | **Source of truth** — toạ độ, tolerance, BOM, lý do thiết kế |
| `chassis-layer1.svg` | Tầng trên (PCB + PTZ + slot) — laser-cut |
| `chassis-layer2.svg` | Tầng dưới (motor + battery + caster) — laser-cut |
| `chassis-layer1.dxf` | DXF tương đương layer 1 (cho shop chỉ nhận DXF) |
| `chassis-layer2.dxf` | DXF tương đương layer 2 |
| `generate_dxf.py` | Script regenerate DXF từ source code (cần `pip install ezdxf`) |
| `assembly-phase1.md` | Hướng dẫn lắp ráp 8 bước |
| `3d-models/` | (TBD) STL cho PTZ mount, dock station |
| `dock-design/` | (TBD) Phase 9 dock charging station |

## Workflow khi sửa chassis

1. Sửa `chassis-spec.md` (text — dễ review).
2. Đồng bộ sửa toạ độ trong `generate_dxf.py` (constants ở đầu file).
3. Sửa SVG tương ứng (thủ công, hoặc viết SVG generator nếu cần).
4. Chạy `python generate_dxf.py` để regenerate DXF.
5. Verify bằng cách mở SVG/DXF trong Inkscape/LibreCAD.
6. Commit cả 3 (spec + svg + dxf + py).

## Đặt laser-cut

- Material: **acrylic 3mm** (trong hoặc mờ — tuỳ thẩm mỹ)
- Số lượng: 2 tấm (mỗi tấm 200×150mm)
- Cost: ~30-50k VND/tấm tại VN
- Lead time: 1-2 ngày

Gửi file `chassis-layer1.svg` + `chassis-layer2.svg` hoặc bản DXF tương ứng.

## Sau khi nhận hàng

Đọc `chassis-spec.md` section 10 (verify checklist) trước khi lắp ráp.
Sau đó theo `assembly-phase1.md` để lắp ráp 60-90 phút.
