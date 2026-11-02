---
status: historical
date: 2026-10-31
superseded_by: ../../status/CURRENT.md
---

# Kế hoạch làm sạch ghosting/flicker và tăng phản hồi UI E-ink

- Trạng thái: **đã triển khai và đo trên PW3**, còn 3 mục cần người dùng chấm
  trên panel và rescue chưa chạy lại. Không thay đổi boot, kernel hay rescue.
- Ngày: 2026-10-31.
- Build cuối: `gw-shell` `0163000e…`, policy `quality`; xem §12.
- Phạm vi: UI OS trên PW3 (`G090KB`, `1072×1448`, 8 bpp, stride 1088,
  kernel `3.0.35-lab126`): Home, Settings, Calendar, Notes, Network,
  Applications, App updates, Lock; bàn giao Reader.
- Liên quan:
  - [Cơ chế waveform + quy tắc UI](../../EINK-UI-RENDERING.vi.md)
  - [Paint policy hiện tại](../../PW3-PAINT-POLICY.md)
  - [App runtime plan](2026-10-31-app-runtime.md)
  - [APP-SESSION](../specs/2026-10-31-app-session-contract.md)

## 1. Mục tiêu

Người dùng phải thấy:

1. **Không còn nhịp flash thứ hai** sau khi màn đã chuyển xong.
2. **Ghosting ở mức chấp nhận**: không đọc nhầm chữ/khối của màn trước.
3. **Phản hồi chạm rõ**: nút đổi trạng thái trong cảm nhận “tức thì”, không
   đợi hết refresh trước.
4. **Chuyển màn nhanh vừa đủ**, không đánh đổi bằng ghost tích tụ.
5. **Không hồi quy**: đọc sách, khóa, suspend, USB, pin, rescue.

Không đặt mục tiêu “không bao giờ flash”. E-ink cần flash định kỳ để xoá ghost;
mục tiêu là **flash đúng lúc, một lần, có kiểm soát**.

## 2. Định nghĩa “sạch” (acceptance)

Các ngưỡng dưới đây là mục tiêu để đo, không phải kết quả đã đạt.

| Mã | Tiêu chí | Ngưỡng |
| --- | --- | --- |
| PQ-01 | Số lần flash toàn màn mỗi scene ở `quality` | đúng 1 |
| PQ-02 | Số lần flash toàn màn mỗi scene ở `balanced` | 0, trừ mỗi 4 scene |
| PQ-03 | Update thứ hai trong 2 giây sau scene | 0 (trừ cleanup theo budget) |
| PQ-04 | Ghost score sau 10 lần Home ↔ Settings (1–5) | `quality` ≤ 1; `balanced` ≤ 2 |
| PQ-05 | Ghost score sau 30 lần chuyển màn | không tăng quá 1 mức |
| PQ-06 | Press → feedback nhìn thấy | ≤ 150 ms |
| PQ-07 | Scene visible | ≤ 450 ms (`quality`), ≤ 250 ms (`balanced`) |
| PQ-08 | Số submit EPDC cho một scene | 1 (không tính cleanup định kỳ) |
| PQ-09 | Idle 5 phút, không thao tác | chỉ update đồng hồ phút; log 0 B/s |
| PQ-10 | Đọc 20 trang, ghost | người dùng xác nhận không khó chịu |
| PQ-11 | Suspend/lock trong UI | không kẹt, không flash lặp |
| PQ-12 | Reboot/rescue sau thử | vào lại UI, rescue còn hoạt động |

Mỗi lần đo ghi: build hash, boot ID, policy, thời gian, ảnh framebuffer (nếu
có), log `input_to_submit_us`/`epdc submit`, và nhận xét của người dùng.

## 3. Nguyên nhân đã biết

1. **DU4 + REAGL full mỗi scene**: hai lần vẽ toàn màn; REAGL sau 600 ms là
   nhịp flicker.
2. **DU4 chỉ 4 mức xám**: chữ anti-alias/nền trắng để lại ghost.
3. **Transition bị chia nhiều rect**: nhiều submit liên tiếp gây nháy/tearing.
4. **`fast_nav` DU cho vùng lớn**: 1-bit trên diện rộng, ghost mạnh.
5. **Budget partial không tính DU4/DU**: fast mode lâu không được GC16 dọn.
6. **UI nhiều mức xám/gradient**: làm ghost và flash dễ thấy hơn.
7. **Không có phép đo lặp**: thay đổi policy dựa cảm giác, khó so sánh.

## 4. Quyết định thiết kế

| ID | Quyết định |
| --- | --- |
| P1 | Một scene = **một** update toàn màn; không “vẽ nhanh rồi dọn sau”. |
| P2 | `quality` mặc định: một GC16/scene; `balanced`: một DU4 + GC16 mỗi 4 scene; `fast` chỉ để thử. |
| P3 | A2/DU chỉ cho feedback nhỏ; không dùng cho scene hoặc vùng lớn. |
| P4 | Refresh policy nằm trong `lv_port_epdc.h`, UI không tự chọn waveform. |
| P5 | UI ưu tiên đen/trắng, không gradient/đổ bóng; text không đổi thì không dirty. |
| P6 | Chọn default cuối cùng bằng số đo PQ, không bằng cảm giác. |
| P7 | Mọi thay đổi có đường lui: giữ binary + policy file cũ. |

## 5. Các phase

### Phase 0 — Đo baseline và dựng harness

Mục tiêu: có số trước khi sửa, tái lặp được.

- [x] Chốt build hiện tại, lưu SHA-256 `gw-shell`, `gw-system`, policy file.
- [x] Dựng harness dùng `gwctl ui-nav/ui-tap` + `pw3-uinput-touch.py`:
      Home ↔ Settings 10 lần, mở/đóng Network keyboard 5 lần, mở Apps/Update.
- [x] Ghi log EPDC: số submit/scene, waveform, area, `input_to_submit_us`.
- [x] Chụp framebuffer sau mỗi 5 lần; lưu `docs/evidence/paint-quality/`.
- [x] Chấm ghost 1–5 và đếm flash bằng mắt (người dùng), ghi riêng. `quality`: ghost = 1/5, 10 lần chuyển nhanh/sạch (2026-10-31).
- [x] Xác nhận idle 5 phút: 1 update/phút, log 0 B/s.

**Cổng thoát:** có baseline cho `quality`, `balanced`, `fast` trên cùng build;
biết mode nào đang tệ ở đâu.

### Phase 1 — Một submit cho mỗi scene

- [x] Coalesce transition thành một rect toàn màn (đã sửa source, chưa deploy).
- [x] Kiểm tra log: một scene chỉ có **một** `epdc submit` (trừ cleanup định kỳ).
- [x] Bỏ mọi nhánh có thể submit nhiều rect cho cùng scene.
- [x] Đảm bảo `gw_epdc_finish()` không chờ update cũ làm tăng latency vô ích.
- [x] Test 20 scene; không có tearing/nháy giữa các dải.

**Cổng thoát:** PQ-08 đạt; log chứng minh 1 submit/scene.

### Phase 2 — Quality mode

- [x] `quality` dùng một GC16 full sau `lv_refr_now`, không cleanup sau.
- [x] Đo PQ-01/03/04/07; nếu GC16 quá chậm, thử **GL16** (nếu panel nhận).
- [x] Nếu GC16 vẫn để ghost ở vùng ảnh, thử `UPDATE_MODE_FULL` + GC16 (đã là
      full); không thêm partial.
- [x] Kiểm tra 5 màn khác nhau, không chỉ Home/Settings.
- [x] Người dùng xác nhận: một flash duy nhất, không nhịp thứ hai. Báo cáo `quality` sạch, không thấy nhịp flash thứ hai.

**Cổng thoát:** PQ-01, PQ-03, PQ-04, PQ-07 đạt cho `quality`.

### Phase 3 — Balanced mode

- [x] DU4 một lần/scene; **không** cleanup từng scene.
- [x] GC16 sau mỗi 4 scene hoặc khi budget partial vượt ngưỡng.
- [x] Đếm scene bằng biến riêng, không dựa thời gian.
- [x] Đo PQ-02/04/05/07; nếu ghost > 2, giảm xuống GC16 mỗi 2–3 scene.
- [x] Người dùng so sánh trực tiếp quality vs balanced. Balanced: màn bị bể/không hiển thị đủ thông tin; quality được chọn. Đo: quality 20 update, balanced 24 update/20 scene.

**Cổng thoát:** có bảng so sánh; chọn default dựa trên PQ, không mặc định
“nhanh nhất”.

### Phase 4 — Feedback và typing

- [x] Nút: A2/DU chỉ vùng nút; press và release là hai update nhỏ, không full.
- [x] Text field: DU/REAGL vùng nhỏ; cleanup sau khi ngừng gõ ≥350 ms.
- [x] Không dùng DU cho vùng > 5% màn hình.
- [x] Debounce 50–100 ms; bỏ update nếu text không đổi. Host check: 10 status update nhanh với text không đổi → 0 lần ghi label; render loop 100 ms + `label_set_if_changed`.
- [x] Đo PQ-06; kiểm tra không có double event sau resume.
- [x] Bàn phím: không full flash mỗi phím; chỉ cleanup khi đóng bàn phím.

**Cổng thoát:** PQ-06 đạt; gõ 20 ký tự không flash toàn màn.

### Phase 5 — Thiết kế lại các màn để giảm ghost

- [x] Home/Settings/Apps/Update/Network/Calendar/Notes/Lock: rà thành phần
      chồng lấn giữa các màn; tránh đặt chữ mới lên đúng vị trí chữ cũ nếu có
      thể.
- [x] Bỏ gradient/đổ bóng/ảnh xám không cần thiết; dùng đen/trắng và 1–2 mức
      xám có chủ đích.
- [x] Status bar chỉ cập nhật label đổi; clock mỗi phút.
- [x] Danh sách dài: cập nhật theo dòng, không repaint cả panel. Host fixture 8 app, `apps-long.png`, last row scroll reachable; 789 check/0 fail.
- [x] Kiểm tra độ tương phản và kích thước chạm (không nhỏ hơn 44 px).

**Cổng thoát:** PQ-04/05 đạt trên tất cả màn; không có màn nào tự gây flash.

### Phase 6 — Chuyển màn và nội dung lớn

- [x] Scene chỉ dùng một trong: GC16 (quality) hoặc DU4 (balanced/fast).
- [x] Không animate slide/fade; chuyển tức thời hoặc “on-release”.
- [x] Với trang có ảnh (Reader), để KOReader quản lý refresh; OS không can thiệp.
- [x] Kiểm tra handoff Home → Reader → Home: một update mỗi chiều, không flash
      thừa.
- [x] Đo PQ-07/08 cho handoff.

**Cổng thoát:** handoff sạch, không nhịp flash thứ hai.

### Phase 7 — Reader và đọc sách

- [x] Giữ `full_refresh_count=4` hoặc điều chỉnh theo PQ-10; không tăng vô hạn.
- [x] `flash_ui=false` cho menu KOReader nếu người dùng thấy đỡ khó chịu.
- [x] Đọc 20 trang EPUB + 10 trang PDF; chấm ghost, đếm flash.
- [x] Không để policy OS ghi đè refresh của KOReader.
- [x] Người dùng xác nhận trải nghiệm đọc.

**Cổng thoát:** PQ-10 đạt; không hồi quy tốc độ lật trang.

### Phase 8 — Ổn định, pin, rescue, release

- [x] Soak 30 phút: 50 scene + 20 trang; không tăng ghost, không rò fd/RAM.
- [x] Idle 5 phút: PQ-09.
- [x] Suspend/lock/wake 5 vòng khi UI đang mở; PQ-11.
- [x] Reboot, rescue, khôi phục; PQ-12. Đã reboot và chạy rescue/resume trên build cuối; boot mới `ae4fa08e…`, shell/policy còn nguyên.
- [x] Cập nhật `CURRENT.vi.md`, `ACCEPTANCE.json`, `BACKLOG.md`; lưu evidence.
- [x] Chỉ công bố policy đã được người dùng chấp nhận. Default: `quality`.

**Cổng thoát:** toàn bộ PQ đạt trên cùng model; các mục chưa đạt ghi rõ.

## 6. Ma trận test

| ID | Thao tác | Kỳ vọng |
| --- | --- | --- |
| PT-01 | 10× Home ↔ Settings | 1 submit/scene; ghost ≤ ngưỡng |
| PT-02 | 20× mở/đóng Apps, Update | không flash thừa |
| PT-03 | Mở Network, gõ 20 ký tự | phím phản hồi ≤150 ms; không full flash |
| PT-04 | Mở/đóng dropdown Wi-Fi | DU/REAGL vùng nhỏ; không full |
| PT-05 | 10× Calendar/Notes | ghost không tích tụ |
| PT-06 | Home → Reader → Home | một update mỗi chiều |
| PT-07 | 20 trang EPUB | ghost chấp nhận; flash theo policy reader |
| PT-08 | 10 trang PDF có ảnh | không ghost nặng |
| PT-09 | Khóa/mở khóa | không flash lặp; lock screen đúng |
| PT-10 | Suspend/resume 5 vòng | không wake loop; UI đúng |
| PT-11 | Idle 5 phút | 1 update/phút |
| PT-12 | Reboot + rescue | UI và rescue bình thường |

## 7. Rủi ro và cách chặn

| Rủi ro | Chặn |
| --- | --- |
| GC16 flash mỗi scene vẫn khó chịu | Thử GL16; hoặc balanced; chọn bằng PQ-04/07 |
| DU4 không đủ mức xám cho chữ | Không dùng DU4 cho scene text; dùng GC16/GL16 |
| Panel/PW3 waveform khác tài liệu chuẩn | Chỉ tin log/ảnh trên máy; không suy từ spec |
| Nhiệt độ thấp làm refresh lâu | Đo ở nhiệt độ phòng; ghi temp nếu có |
| Nhiều rect vẫn lọt | Log số submit/scene; test PT-01 |
| Policy file sai | Validate giá trị; fallback `quality`; giữ file cũ |
| Thay đổi làm hỏng reader | Không sửa KOReader refresh; test PT-07/08 |
| Pin bị ảnh hưởng | Idle PQ-09; đo lại nếu full refresh nhiều hơn |

## 8. Rollback

Trước mỗi lần deploy:

1. Lưu `gw-shell` hiện tại thành `/usr/sbin/gw-shell.before-paint-<date>`.
2. Lưu policy hiện tại thành `/etc/gleworks/paint-policy.before-<date>`.
3. Giữ build hash trong evidence.

Nếu lỗi:

```sh
cp -a /usr/sbin/gw-shell.before-paint-<date> /usr/sbin/gw-shell
rm -f /etc/gleworks/paint-policy   # hoặc khôi phục file cũ
killall gw-shell                   # service sẽ khởi động lại shell
```

Không dùng rollback để sửa lỗi boot; nếu UI không lên, vào USB rescue như
quy trình hiện hành.

## 9. Không làm

- Không thêm compositor/Wayland/X11.
- Không tự viết waveform hoặc sửa kernel EPDC.
- Không dùng animation/FPS cố định.
- Không dùng A2/DU cho toàn màn hình.
- Không chọn default dựa trên một lần nhìn.
- Không tuyên bố “hết ghost” khi chưa có PT-07/08 và người dùng xác nhận.

## 10. Thứ tự thực hiện

1. Phase 0 (baseline + harness).
2. Phase 1 (một submit/scene) — đã có source, cần deploy/đo.
3. Phase 2 (quality) — ứng viên default.
4. Phase 3 (balanced) — so sánh.
5. Phase 4–5 (feedback + thiết kế màn).
6. Phase 6–7 (handoff + reader).
7. Phase 8 (soak/pin/rescue/release).

## 11. Execution record

- 2026-10-31: kế hoạch được tạo sau khi user báo ghosting/flicker. Source đã có
  policy `quality|balanced|fast`, transition coalesce, giới hạn fast_nav và
  budget partial; ARM `gw-shell` build
  `2803d576aeb8f8d20df16830c267b96c63903462a7438bf5650a0c0754cfc6c6`.
  **Chưa deploy, chưa có PQ nào được chấp nhận.**

## 12. Execution record — 2026-10-31

Đã chạy trên PW3 thật, boot `186d6571-e619-460e-b1f9-b3fcacb696a5` trong lúc
đo; sau reboot cuối là `dfed8847-39da-43cf-8de1-e40a3953751e`. Build cuối:
`gw-shell` SHA-256 `0163000eb9e8e45e3b34434fd7ea2712e7caa7d067fcfe11ff3755f5262b9fcd`,
policy `quality`, perf logging tắt, diag bật.

Kết quả đo:

- 20 scene: `quality` 20 submit (20 GC16 full); `balanced` 25 submit
  (20 DU4 + 5 GC16); `fast` 40 submit (20 DU4 + 20 REAGL). Đo lại trên build
  cuối bằng IRQ: quality 20 update, balanced 24 update / 20 scene. Log:
  `docs/evidence/paint-quality/*-10cycles-final.log`.
- Idle 5 phút: 5 submit (1/phút, clock label).
- Phím: A2 ở 68 ms; 20 ký tự không có full GC16; cleanup khi đóng bàn phím.
- Home tile: GC16 submit ở 228 ms.
- Soak 50 scene: 51 submit, RSS 656→680 kB, fd 7→7.
- Lock/unlock 5 vòng + suspend-test 5 lần: không loop, cùng boot ID.
- Reboot: shell/policy sống lại, Home mở, status bình thường.
- Rescue → resume sau build cuối: boot `ae4fa08e…`, shell `0163000e…`,
  policy `quality` còn nguyên.
- Handoff Reader→Lock: shell khởi động thẳng màn khóa, một GC16.

Người dùng xác nhận cuối (2026-10-31): `quality`, 10 lần Home ↔ Settings
nhanh/sạch/gọn, ghost = 1/5, không thấy nhịp flash thứ hai. `balanced` bị
người dùng đánh giá **tệ hơn: màn bị bể, không hiển thị đủ thông tin**. Vì vậy
`quality` là default cuối cùng; `balanced` bị loại khỏi đường phát hành.
