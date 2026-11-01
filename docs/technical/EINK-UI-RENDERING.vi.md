> **Cập nhật 2026-11-01:** chính sách scroll đang chạy là 6 preview/160 ms,
> release GC16 partial trên union, và một full GC16 sau khi dừng cuộn 1.2 giây
> (xem [PW3-DISPLAY-PROGRAMMING-RULES.md](PW3-DISPLAY-PROGRAMMING-RULES.md)).
> Các mô tả REAGL/4 preview bên dưới là lịch sử của build cũ `149ae024…`.

# Vẽ và làm mượt UI trên E-ink PW3

- Ngày: 2026-10-31.
- Phạm vi: LVGL + EPDC trên PW3 (`1072×1448`, 8 bpp, stride 1088).
- Mục đích: giải thích ghosting/flicker đang thấy, đối chiếu cơ chế waveform
  của E-ink, và chốt cách vẽ UI ít flash nhưng vẫn phản hồi.
- Đây là ghi chú kỹ thuật + khảo sát; chưa phải acceptance trên panel.

## 1. Hiện tượng

Sau paint policy 2026-10-31, chuyển màn nhanh hơn nhưng:

- **Ghosting rõ**: chữ/khối của màn trước còn lưu lại.
- **Flickering rõ**: màn chuyển xong rồi còn một nhịp sáng/tối nữa.

Chuỗi đo được của policy hiện tại:

```text
epdc submit marker=7 waveform=7 (DU4)  input_to_submit_us=75890
epdc submit marker=8 waveform=8 (REAGL) input_to_submit_us=719634
```

Nghĩa là mỗi lần chuyển màn có **hai lần vẽ toàn màn hình**: DU4 ngay, rồi
REAGL full sau ~600 ms. Lần thứ hai chính là nhịp flicker nhìn thấy; DU4 để lại
residue vì chỉ 4 mức xám và không có pha xoá ghost.

## 2. Cơ chế E-ink

E-ink là bistable: hạt mực giữ vị trí sau khi mất điện. Muốn đổi màu phải áp
một chuỗi xung (waveform) từ LUT của panel. Nhiều xung hơn → nhiều mức xám và
sạch hơn, nhưng chậm hơn. Không có mode nào vừa nhanh vừa sạch.

Bảng tham chiếu (số liệu từ tài liệu E Ink qua Inkframe; **PW3 có waveform
vendor riêng, nhiệt độ và LUT có thể khác**, nên chỉ dùng để so sánh tương đối):

| Mode | Thời gian tham chiếu | Mức xám | Flash | Ghost | Dùng cho |
| --- | ---: | ---: | --- | --- | --- |
| A2 | ~120 ms | 2 | Có | Nhiều | Feedback, scrolling, animation |
| DU | ~260 ms | 2 | Không | Có | Menu, chữ đen/trắng |
| DU4 | ~290 ms | 4 | Ít | Có | Text anti-alias trong menu |
| GC16 | ~450 ms | 16 | Có (một pha xoá) | Gần như sạch | Ảnh, màn mới, cleanup |
| GL16 | ~450 ms | 16 | Vừa | Ít hơn DU4 | Text trên nền trắng |
| REAGL/Regal | Vendor-specific | 16 | Thấp | Giảm ghost | Partial “sạch hơn” của vendor |

Nguyên tắc vật lý:

- **Partial update** chỉ đổi vùng chọn, không xoá toàn panel → nhanh nhưng
  residue tích tụ.
- **Full update** (GC16/INIT) đi qua pha đen/trắng để xoá residue → sạch nhưng
  chậm và thấy flash.
- E-reader thực tế dùng DU/DU4/A2 cho thao tác, rồi **GC16 định kỳ** để dọn
  ghost; flash định kỳ đó là chủ ý, không phải lỗi.
- Nhiệt độ thấp làm hạt mực chậm; cùng mode có thể lâu hơn.

Nguồn: Inkframe glossary (bảng mode), Pervasive Displays (fast/partial), E Ink
spec references; xem §7.

## 3. Vì sao UI hiện tại bị cả hai

1. **DU4 cho cả màn hình có chữ.** DU4 chỉ 4 mức xám; chữ anti-alias và nền
   trắng dễ để lại bóng. DU4 phù hợp text menu nhỏ, không phù hợp thay toàn
   màn hình.
2. **REAGL full sau mỗi lần chuyển.** Đây là lần vẽ thứ hai, gây nhịp sáng/tối.
   REAGL là partial ghost-compensating, không tương đương GC16.
3. **Nhiều rect được submit riêng.** Khi transition, LVGL có thể trả nhiều
   vùng (status bar, nội dung, label); mỗi vùng là một EPDC submit. Nếu không
   gộp thành **một** update toàn màn, panel nhận nhiều lần đổi liên tiếp → cảm
   giác flicker/tearing.
4. **`fast_nav`/DU trong 700 ms sau tap** áp lên cả vùng lớn, không chỉ feedback
   nhỏ. Chữ trên vùng lớn bị 1-bit và để ghost.
5. **Bộ đếm partial không tính DU4**, nên fast mode có thể chạy rất lâu mà
   không có GC16 dọn ghost.

## 4. Cách vẽ đúng cho UI đọc sách

### 4.1 Ba loại cập nhật, ba policy

| Loại | Ví dụ | Mode | Phạm vi |
| --- | --- | --- | --- |
| Feedback | nhấn nút, highlight | A2/DU | rất nhỏ, không full |
| Content | đồng hồ, pin, text field | REAGL/DU | vùng nhỏ, đếm budget |
| Scene | đổi Home ↔ Settings | **một** GC16 hoặc **một** DU4 | toàn màn, một submit |

Không trộn: scene change không được “DU4 trước + cleanup sau”.

### 4.2 Policy đề xuất

- **Quality (mặc định):** mỗi scene = một GC16 full, không cleanup sau. Sạch
  ghost, chấp nhận một nhịp flash ngắn. Đây là kiểu Kindle menu/reader.
- **Balanced:** scene = một DU4 full, **không** cleanup từng lần; GC16 sau mỗi
  4 scene. Nhanh, ít flash, có ghost nhẹ.
- **Fast:** scene = DU4 + REAGL mỗi lần (policy cũ). Chỉ dùng để thử nghiệm,
  không làm mặc định.

### 4.3 Quy tắc UI

1. **Một ý định → một update.** Không vẽ lại cả màn chỉ vì một label đổi.
2. **Một scene → một submit toàn màn.** Gộp dirty rect thành một vùng cho
   transition; chỉ partial cho thay đổi nhỏ.
3. **Không animation liên tục.** E-ink không hợp fade/slide; dùng chuyển màn
   tức thời, hoặc “on-release” cho scroll.
4. **Đen/trắng là chính.** KOReader cố ý dùng UI đơn sắc vì gradient/nhiều xám
   gây flash và ghost. Hạn chế ảnh xám, bóng đổ, anti-alias nặng.
5. **Tránh chồng lấn giữa hai màn.** Nếu phần tử ở cùng vị trí nhưng nội dung
   khác, ghost dễ thấy; thiết kế layout khác nhau hoặc xoá vùng đó bằng GC16.
6. **Pre-render rồi commit.** Dựng xong scene trong RAM, gọi `lv_refr_now()`,
   rồi một `present()`. Không để người dùng thấy các bước trung gian.
7. **Feedback phải tức thì.** A2/DU cho vùng nhỏ ngay khi chạm; không chờ
   update trước hoàn tất.
8. **Debounce và ngưỡng.** Gộp nhiều tap/scroll trong ~50–100 ms thành một
   update; bỏ update nếu nội dung không đổi (`label_set_if_changed`).
9. **Budget GC16 theo số update, không theo thời gian.** Ví dụ 1 GC16 mỗi 4
   scene, hoặc mỗi 20 partial nhỏ; tránh flash giữa lúc đang đọc.
10. **Đo bằng mắt + đếm.** Ghost score, số flash mỗi 10 thao tác, thời gian
    press→visible; không chỉ nhìn “có vẻ nhanh”.

## 5. Đối chiếu với code hiện tại

Đã thêm runtime policy trong `os/ui/gw-shell.c`:

```text
/etc/gleworks/paint-policy   hoặc   GW_UI_PAINT_POLICY=quality|balanced|fast
```

- `quality`: `gw_ui_show()` gọi `gw_epdc_request_full()` (GC16) sau khi render;
  không request fast/transition; không cleanup sau.
- `balanced`: DU4 cho scene; main loop GC16 sau mỗi 4 scene; không cleanup từng
  lần.
- `fast`: policy cũ (DU4 + REAGL sau 600 ms).

Còn phải sửa để đúng hoàn toàn:

- [x] Transition coalesce thành **một** rect toàn màn trước `present()`.
- [x] `fast_nav` chỉ áp DU cho vùng nhỏ; vùng lớn dùng REAGL/DU4.
- [x] Đếm cả DU4/DU vào budget partial, không chỉ REAGL.
- [x] Hạ `GW_EPDC_PARTIAL_LIMIT` (60 → 20).
- [ ] Cân nhắc GL16 cho scene có chữ nếu panel hỗ trợ; test A/B với GC16.
- [ ] Không dùng REAGL full thay GC16 khi cần xoá ghost.

## 6. Cách test

Dùng `tools/maintenance/pw3-uinput-touch.py` và `gwctl ui-nav/ui-tap`:

1. Cùng build, chạy 20 lần Home ↔ Settings.
2. Đếm số flash nhìn thấy; chấm ghost 1–5 sau mỗi 5 lần.
3. Đo press→visible bằng log `input_to_submit_us` + cảm nhận thực.
4. Lặp với `quality`, `balanced`, `fast`; ghi bảng so sánh.
5. Chỉ chấp nhận mode có ghost thấp **và** không có flash thứ hai sau chuyển
   màn.

## 7. Nguồn

- Inkframe — E-Paper Waveform Modes Glossary (A2, DU4, GC16):
  <https://inkframe.net/en/guides/e-paper-waveform-modes-glossary>
- Pervasive Displays — Fast & Partial Refresh:
  <https://www.pervasivedisplays.com/how-e-paper-works/fast-update-refresh/>
- KOReader User Guide — E-ink settings, full refresh, on-release scrolling:
  <https://koreader.rocks/user_guide/>
- KOReader UI ghi chú “chủ yếu đen/trắng vì gradient gây flash/ghosting”:
  cùng User Guide, mục Learning the User Interface.
- ACM — A Dedicated E-Paper Design System for Mobile Phones (quản lý ghosting,
  tránh chồng lấn component giữa màn):
  <https://dl.acm.org/doi/full/10.1145/3772318.3791459>
- Modos Glider — waveform/low-latency EPD:
  <https://github.com/Modos-Labs/Glider>
- E Ink waveform spec (được dẫn lại bởi Inkframe; không trích trực tiếp):
  <https://www.eink.com>
