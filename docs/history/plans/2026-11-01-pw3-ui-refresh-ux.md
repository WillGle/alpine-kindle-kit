---
status: historical
date: 2026-11-01
superseded_by: ../../status/CURRENT.md
---

# PW3 UI refresh + UX remediation plan

Date: 2026-11-01  
Status: **implemented + device-verified (2026-11-01)**  
Scope: `os/ui`, `theme/tokens.json`, host simulator và quy trình kiểm chứng trên PW3.
Quy tắc hiển thị bắt buộc: [PW3-DISPLAY-PROGRAMMING-RULES.md](PW3-DISPLAY-PROGRAMMING-RULES.md).
Không đổi boot/kernel/recovery, không đổi KOReader engine.

Nguồn đầu vào:

- Review UI ngày 2026-11-01: 8 lỗi hiển thị, các điểm IA/UX và polish.
- Phản hồi thiết bị: “sau mỗi tap, màn hình flash đen/trắng; scroll không mượt”.
- Source hiện tại: commit `08ba4e4` (`ui: remove GC16 flashes from taps and
  scrolling`) và bản đã deploy được ghi là `6020c6fe…` trong
  `docs/evidence/pw3-ui-polish-2026-11-01/README.md`.

### Trạng thái triển khai (2026-11-01)

Đã implement toàn bộ workstream A–E trong source:

- refresh: `GC16_FAST` partial cho scene/scroll/content; GC16 full chỉ ở
  lock/unlock (budget 30 partial khi idle là opt-in `GW_UI_DEEP_CLEAN=1`);
  mọi submit một waveform/mode;
  scroll live-preview ~5 Hz + final on release; input coalesce frame mới nhất;
- UI: 8 lỗi hiển thị, toast, Back context, lock time, network/update states,
  Home/status/tray, calendar `Today`/`+N more`, app detail sheet, due options;
- test: host suite 2631 check, 0 failed; ARM build OK;
- device: deployed `149ae024…`; 9-action + 20-tap capture pass on
  `61b78d09…` (same UI, before preview), scroll preview verified on
  `149ae024…`; one GC16 only at lock/unlock.

### Revision sau tuning trên máy (2026-11-01, muộn hơn)

Bản `149ae024…` ở trên là mốc lịch sử. Sau khi dùng thật, chính sách scroll được
chốt lại và deploy `df2c9bd7…`:

- 6 preview/160 ms khi kéo (`GW_UI_SCROLL_PREVIEW_MAX=6`,
  `GW_UI_SCROLL_PREVIEW_MS=160`).
- Gộp toàn bộ vùng dirty của cú kéo; khi nhả tay submit GC16 partial trên union.
- Chỉ full-screen GC16 khi người dùng **dừng cuộn 1.2 giây**; tối thiểu 3 giây
  giữa hai lần dọn (`GW_UI_SCROLL_CLEAN_MS`,
  `GW_UI_SCROLL_CLEAN_MIN_MS`). Không full GC16 mỗi lần nhả.
- `preview_ms=0`/`preview_max=0` giờ thực sự tắt preview (trước đó 0 = không
  throttle, vẽ mọi frame).
- Full GC16 một lần ở Home đầu tiên sau boot để xoá bóng ảnh boot
  (`/run/gw-first-home-clean`).
- Wi-Fi Scan: `gw-system` mới `aeedb481…` trả đủ signal/secure; shell decode hex
  đúng tên + dBm.

Task A3 cũ được đánh dấu hoàn tất theo nghĩa “đã có live preview + final”; hành
vi cuối khác ở chỗ dọn ghost dời sang idle 1.2 giây và preview nhiều hơn. Chi
tiết và log: `docs/evidence/pw3-wifi-scroll-fix-2026-11-01.md`.

**Verify (2026-11-01):**

- `./os/ui/sim/run-preview.sh docs/evidence/pw3-ui-refresh-2026-11-01`
  → **2631 check, 0 failed**; LVGL heap used **191,560 / 262,144 bytes**;
  screenshots + `geometry.json` trong cùng thư mục.
- `make check ... GW_UI_EPDC_ACCEPTED=1 GW_UI_TOUCH_ACCEPTED=1`
  → no error/warning.
- ARM release build (`EPDC_accepted=1 touch_accepted=1`, `PERF=0`, `DIAG=0`)
  → 3,073,048 bytes, sha256
  `149ae0243ded075cfe16ab44e8fa3beabf9f3cf45024442177184899e5c36a05`.
- PW3 deployed test build
  `149ae0243ded075cfe16ab44e8fa3beabf9f3cf45024442177184899e5c36a05`
  (EPDC/touch/perf/diag enabled), rollback `6020c6fe…` retained.
- Device capture: 9 actions + 20 taps + lock/unlock in
  `docs/evidence/pw3-ui-refresh-2026-11-01/`.
  - 20 taps: 20 submits, **0 GC16, 0 full**.
  - 9 actions: GC16 only at lock (`mode=1 intent=deep_clean reason=lock`).
  - scroll: one `waveform=3 mode=0 intent=scroll`, no GC16.
  - `merge_risk=0` on every submit.
- Device cleanup: chỉ giữ rollback gốc `6020c6fe…`; perf log tắt; tài liệu
  trên máy ở `/etc/gleworks/UI-REFRESH-2026-11-01.txt`.

**Ghost note:** framebuffer capture cannot measure physical e-ink ghost.
Logs prove no GC16/black-white inversion outside lock and one full cleanup at
lock; a human panel observation after 20 taps is still the final visual score.

## 0. Kết luận ngắn

1. **Flash sau tap không phải lỗi layout.** Đó là vấn đề waveform/refresh
   policy. GC16 có pha xoá đen/trắng toàn màn hình; DU4/GC16_FAST/REAGL không
   có pha đó nhưng có trade-off ghosting/tốc độ.
2. Commit `08ba4e4` đã bỏ GC16 khỏi `gw_ui_show()`, modal, đổi tháng và scroll,
   nhưng **vẫn còn các đường có thể rơi vào GC16**:
   - Lock screen được load trực tiếp bằng `lv_scr_load()` ở 3 chỗ, không đi
     qua scene helper;
   - đóng bàn phím đặt `g_settle_refresh_pending` rồi gọi
     `gw_epdc_request_full()` → GC16;
   - `gw_epdc_present()` vẫn tự promote một vùng dirty lớn (`total * 2 >=
     screen`) thành GC16, kể cả khi đó là một tap/scene;
   - policy `balanced`/`fast` vẫn có full cleanup định kỳ.
3. **Không thể kết luận bản đang chạy trên máy là bản nào** chỉ từ source.
   Phải đọc hash, policy và log `epdc submit` trước khi sửa tiếp. Nếu log vẫn
   ra `waveform=2` thì đó là GC16; nếu ra `waveform=7` mà mắt vẫn thấy flash
   đen/trắng thì DU4 toàn màn hình trên PW3 đang bị cảm nhận như flash và phải
   thử `GC16_FAST` (mode 3) hoặc `REAGL`.
4. Nên gộp việc sửa refresh với việc sửa UI thành **một release gate**, nhưng
   phải làm theo thứ tự: chặn flash trước, sửa layout/state sau, rồi polish.

### Quyết định đã chốt (2026-11-01)

| # | Quyết định | Hệ quả kỹ thuật |
| --- | --- | --- |
| U1 | **Giảm flashing tối thiểu, không chấp nhận ghost** | Không dùng DU/A2/DU4 cho nội dung 16 mức xám. Scene dùng `GC16_FAST` partial; scroll dùng live preview 250 ms (tối đa 4 lần) + `GC16_FAST`, release `REAGL`; deep clean `GC16` đặt ở lock/unlock và/hoặc sau N partial, không đặt trên mỗi tap |
| U2 | **Scroll phải giống hành vi e-reader tốt (Amazon/KOReader), không flash mỗi frame** | Default **live preview 250 ms (~4 Hz, tối đa 4 lần)** khi kéo + **final on release**. Preview `GC16_FAST`, release `REAGL`; không dùng GC16 |
| U3 | **Được phép full refresh cho lock/unlock** | Dùng GC16 full khi lock (và/hoặc unlock) làm “điểm dọn ghost” tự nhiên; không cần GC16 giữa các tap thường |

> Không có waveform nào đồng thời “zero flash” và “zero ghost” trên mọi thời
> điểm. Plan đạt cả hai theo nghĩa thực dụng: nội dung dùng `GC16_FAST`/`GL16`
> (16 mức, không pha xoá), scroll live-preview ~5 Hz + final on release, và dồn GC16 full vào lock/unlock
> (được phép) cùng budget an toàn 20–30 partial. Nếu cần zero ghost tuyệt đối
> sau từng thao tác thì bắt buộc phải quay lại GC16 mỗi scene và chấp nhận
> flash.

## 1. Mục tiêu và non-goals

### Mục tiêu

- Một tap điều hướng không tạo pha đen/trắng toàn màn hình.
- Scroll không bị promote thành GC16; mỗi frame scroll chỉ một partial update.
- Scroll dùng **live preview 250 ms (~4 Hz, tối đa 4 lần)** khi kéo; preview
  `GC16_FAST`, submit cuối khi nhả tay dùng `REAGL`; không dùng GC16.
- Mọi scene repaint đi qua một helper duy nhất, có intent rõ ràng:
  scene / scroll / feedback / deep cleanup.
- Không có ghost nhìn thấy sau 20 scene/tap; deep clean tập trung ở lock/unlock
  và sau một ngưỡng partial an toàn.
- Hết các lỗi hiển thị đã thấy trong ảnh host-sim.
- Các trạng thái quan trọng đúng: lock time, lỗi hiện tại màn, Back đúng nơi
  xuất phát, network selection/connected, update feedback.
- Giữ touch target ≥ 96 px và không tăng LVGL heap quá 240 KiB.

### Non-goals

- Không làm scroll mượt như LCD; mục tiêu là “ổn định, không flash, trễ thấp
  nhất có thể trên e-ink”.
- Không thêm animation, gradient, shadow, dark mode.
- Không đổi IPC trừ khi cần cho trạng thái UI (ví dụ lỗi/toast).
- Không đổi boot, power, recovery, KOReader internals.

## 2. Nguyên tắc refresh

| Loại | Ví dụ | Waveform | Update | Ghi chú |
| --- | --- | --- | --- | --- |
| Feedback | nhấn nút, highlight | A2/DU | partial, vùng nhỏ | không full; chỉ vùng nút, không đảo cả nền primary |
| Content | giờ, pin, một row | REAGL/DU | partial | đếm budget; không rebuild cả list |
| Scene | Home ↔ Settings, modal | **một** `GC16_FAST` partial | whole-screen partial | không GC16 mặc định; 16 mức xám |
| Scroll | kéo list | preview `GC16_FAST`, release `REAGL` | 250 ms, tối đa 4 preview + final | không GC16 |
| Deep clean | lock/unlock, sau N partial | GC16 | full | được phép; đặt ở điểm không làm gián đoạn tap |

Quy tắc bắt buộc:

1. Một tap = một intent = tối đa một EPDC submit.
2. Scene/scroll **không được** bị promote thành GC16 bởi heuristic diện tích.
3. GC16 chỉ được phát ra từ deep-clean explicit: lock/unlock,
   `GW_UI_SCENE_WAVEFORM=gc16`, hoặc budget opt-in `GW_UI_DEEP_CLEAN=1`.
   `paint-policy=quality` không còn ép GC16 mỗi scene.
4. Mọi `lv_scr_load()` phải đi qua cùng một scene helper.
5. Mọi thay đổi text phải dùng `label_set_if_changed()`.
6. Không rebuild toàn bộ list khi chỉ một row thay đổi; `lv_obj_clean()` trên
   list lớn là một nguồn GC16/ghost thực tế.
7. **Không cập nhật nhanh hơn panel settle.** Với DU4/GL16/GC16_FAST trên PW3,
   giới hạn thực tế khoảng 3–4 Hz; queue nhiều update chỉ làm panel “mid-
   transition” và tăng ghost.
8. **Ưu tiên bề mặt đen/trắng.** Nền xám lớn buộc waveform 16 mức và làm mọi
   thao tác scroll/repaint đắt hơn; dùng viền/không gian thay cho fill xám.

## 3. Workstream A — refresh, flash, scroll (P0)

### A0. Reproduce và instrument trước khi sửa

> **Device capture done (2026-11-01).** Log và ảnh nằm ở
> `docs/evidence/pw3-ui-refresh-2026-11-01/`. Ghost vật lý vẫn cần mắt người
> xác nhận; log chỉ chứng minh không có GC16 ngoài lock/unlock.

- [x] Đọc hash `/usr/sbin/gw-shell`; xác nhận đang chạy `6020c6fe…` hay bản cũ.
- [x] Đọc `/etc/gleworks/paint-policy` và log `GW_SHELL: paint policy=…`.
- [x] Chạy build `GW_UI_PERF=1`, bật `GW_UI_PERF_LOG=1` hoặc
      `/run/gw-ui-perf`.
- [x] Ghi log cho từng hành động riêng:
  - 1 tap bottom nav;
  - 1 tap Home card;
  - 1 tap checkbox To-dos;
  - 1 tap Scan Wi-Fi;
  - 1 tap Remove app;
  - 1 tap Lock;
  - 1 tap mở/đóng keyboard;
  - 1 tap mở/đóng modal;
  - 1 lần scroll Settings/Apps.
- [x] Đếm `waveform=2` (GC16), `waveform=3` (GC16_FAST), `waveform=7` (DU4),
      `waveform=8` (REAGL), `mode` (0=partial, 1=full), `area`.
- [x] Chụp/ghi lại mắt thấy: flash đen/trắng hay chỉ nháy xám.
- [x] Kiểm tra log driver nếu có: một update bị merge thành `mode=1` hoặc
      waveform `AUTO` (257) sau một tap nhỏ.

**Gate A0:** có log trước/sau cho ít nhất 9 hành động trên. Không sửa
waveform khi chưa có log này.

### A1. Tập trung scene intent

- [x] Thêm helper duy nhất, ví dụ `gw_scene_begin(gw_screen_t)`:
  - set `hold_partial`;
  - set `scene_waveform` theo policy;
  - clear `g_force_full_update` nếu đang có;
  - tăng counter cho deep clean.
- [x] Thay mọi `lv_scr_load()` trực tiếp bằng helper:
  - `lock_screen_cb()`;
  - `lock_apply_status()`;
  - `evt_handle_line()` khi nhận lock/wake/unlock.
- [x] `gw_ui_request_full()` dùng cùng helper, không tự gọi
      `gw_epdc_request_full()`.
- [x] Thêm test host: sau tap giả lập, không có request GC16; chỉ có scene
      intent.

### A2. Chặn heuristic promote GC16

Trong `gw_epdc_present()` hiện có:

```c
full = g_force_full_update ||
       (!no_full && !text && !popup && !scrolling && total * 2 >= screen) ||
       (... partial_updates ... >= LIMIT);
```

- [x] Tách “scene/scroll intent” ra khỏi nhánh `full`; scene/scroll phải thắng.
- [x] Không auto-promote một frame có touch thành GC16. Chỉ promote khi:
  - explicit `gw_epdc_request_full()`, hoặc
  - idle cleanup theo policy, hoặc
  - budget deep-clean đã tới hạn.
- [x] Log lý do full (`full_reason=force|area|budget`) để không đoán.
- [x] Test: tap một card lớn/Home không tạo `waveform=2`.

### A3. Scroll

> Các mục tick bên dưới ghi lại bản `149ae024…` (release REAGL, 4 preview).
> Hành vi đang chạy là revision cuối ở đầu tài liệu: 6 preview/160 ms, release
> GC16 partial trên union, full GC16 khi idle 1.2 giây.

Hiện `attach_scroll_events()` chỉ gắn ở Settings, To-dos, Network, Apps và
đặt `gw_eink_request_scroll(600)` từ `LV_EVENT_SCROLL_BEGIN/SCROLL`. Cách này
vẫn cho LVGL cập nhật liên tục, trong khi panel chỉ settle được ~3–4 Hz.

- [x] **Live preview + final on-release**: khi kéo, submit partial tối đa mỗi
      250 ms và tối đa 4 lần; khi `LV_EVENT_RELEASED` submit `REAGL` cuối.
      Đây là cách KOReader mô tả là phù hợp e-ink: “doesn’t refresh your page
      continuously while scrolling”.
- [x] Preview throttle 250 ms (`GW_UI_SCROLL_PREVIEW_MS`), tối đa 4
      (`GW_UI_SCROLL_PREVIEW_MAX`), release `REAGL`; override waveform bằng
      `GW_UI_SCROLL_WAVEFORM`/`GW_UI_SCROLL_RELEASE_WAVEFORM`.
- [x] Không dùng DU/A2/DU4 cho list có chữ anti-alias; 1-bit/4-level làm mất
      mức xám và để ghost.
- [x] Gắn scroll intent sớm hơn: trong input port, khi phát hiện di chuyển
      > ~8 px, gọi `gw_eink_request_scroll(1500)`; không chờ LVGL event.
- [x] Cho nhánh scroll ưu tiên trước nhánh `full` trong `gw_epdc_present()`.
- [x] Gắn scroll events cho mọi container scrollable, kể cả dropdown list.
- [x] Khi có nhiều event trong queue, coalesce về vị trí mới nhất; không replay
      từng frame cũ (nguồn gây giật và update dồn).
- [x] Đo: 10 lần kéo list → 0 `waveform=2`; số submit ≤ số lần nhả tay + preview;
      không frame nào > 250 ms.
- [x] Nếu vẫn giật vì EPDC ~250–400 ms/frame: giữ on-release hoặc thêm nút
      Page up/down; không cố đạt 60 fps.
**Revision đã deploy (2026-11-01, muộn hơn):**

- [x] Preview 6 lần/160 ms; gộp union vùng dirty của cả cú kéo.
- [x] Full GC16 chỉ sau khi dừng cuộn 1.2 giây; throttle tối thiểu 3 giây.
- [x] `preview_ms=0`/`preview_max=0` thực sự tắt preview.
- [x] Full GC16 một lần ở Home đầu tiên sau boot để xoá bóng boot.
- [x] Log xác nhận `scroll cleanup scheduled` → `scroll idle clean fired` →
      `full update`; người dùng xác nhận cuộn mượt, dừng tay một nháy dọn.

### A4. Chọn scene waveform bằng A/B trên máy

Thứ tự thử, cùng một build có runtime policy:

1. `gc16_fast` — mode 3 trong header Kindle, KOReader dùng cho UI; 16 mức xám,
   **có thể dùng partial mà không có pha xoá GC16**. Đây là default đề xuất.
2. `gl16` — mode 5; 16 mức xám, không flash, nhưng có thể để residue.
3. `reagl` — full region; ít flash hơn nhưng ghost/reverse-ghost nhiều hơn.
4. `du4` — hiện tại; nhanh nhưng 4 mức xám, chỉ nên dùng cho preview/feedback.
5. `quality` (GC16) — fallback sạch tuyệt đối, nhưng flash mỗi scene; không
   default.

- [x] Thêm policy `smooth`: `GC16_FAST` mỗi scene, `GC16` full khi lock/unlock,
      không deep-clean mỗi tap.
- [x] Nếu `GC16_FAST`/`GL16` không được panel/kernel hỗ trợ, fallback DU4 và
      ghi rõ; không im lặng đổi waveform.
- [x] Deep-clean ngoài lock/unlock chỉ sau **20–30 partial** cộng dồn (đo trên
      panel; web ref gợi ý 10–50), và chạy khi idle ≥ 600 ms.
- [x] Deep-clean định kỳ **mặc định tắt** để ưu tiên không flash; bật bằng
      `GW_UI_DEEP_CLEAN=1` nếu ghost > 1/5. Lock/unlock vẫn là điểm dọn chính.
- [x] Lưu ý: full refresh khi lock/unlock được phép (U3), nên đây là điểm dọn
      ghost chính; không cần full giữa các tap.

**Gate A4:** 20 tap điều hướng liên tiếp:
- 0 lần `waveform=2` giữa các tap;
- 1 submit/tap;
- flash mắt thấy = 0 hoặc chỉ nháy xám nhẹ;
- ghost score ≤ 1/5 sau 20 tap, và = 0 ngay sau lock/unlock.

### A5. Pressed state không đảo cả nút

`gw_style_btn_pressed_s` hiện đổi nền primary từ đen sang trắng, border 4 px.
Với nút lớn (Add, Connect, Unlock, Lock now) đây là một mảng trắng lớn, dễ bị
nhìn như flash.

- [x] Primary pressed: giữ nền đen, chỉ đổi border/inner outline hoặc label;
      không đảo toàn bộ diện tích.
- [x] Secondary pressed: giữ trắng + border 4 px như hiện tại.
- [x] Card pressed: giữ border 2→4 px, không fill.
- [x] Kiểm tra lại trên panel: tap Add/Unlock/Connect không thấy nháy trắng.

### A6. Keyboard/modal/lock

- [x] Đóng keyboard hiện gọi `gw_epdc_request_full()` → GC16. Đổi sang
      `GC16_FAST`/GL16 partial, hoặc deep clean chỉ khi rời màn.
- [x] Modal mở/đóng dùng scene helper, không GC16.
- [x] Lock/unlock dùng scene helper; **cho phép GC16 full ở lock/unlock** theo
      U3. Đề xuất: GC16 khi lock (màn sắp tắt), `GC16_FAST` khi unlock để
      tránh flash lúc mở máy.
- [x] Kiểm tra lock screen time label được cập nhật trước khi full update.

### A7. Không rebuild cả list khi một row đổi

Audit code cho thấy đây là nguồn flash lớn chưa được xử lý:

- `todo_rebuild_list()` gọi `lv_obj_clean(g_todo_list)` rồi dựng lại toàn bộ
  list sau mỗi checkbox/add/delete/filter.
- `network_build_rows()` gọi `lv_obj_clean(g_net_list)` sau scan.
- `apps_apply_entries()` gọi `lv_obj_clean(g_apps_list)` khi count/id đổi
  (remove/install/refresh).

Với list 8–10 row, vùng dirty vượt 50% màn hình → `gw_epdc_present()` promote
thành GC16, hoặc driver merge thành `UPDATE_MODE_FULL` + `WAVEFORM_MODE_AUTO`
(xem A8).

- [x] To-dos: giữ row widget; toggle chỉ đổi checkbox/text/row style; add/delete
      chèn/xóa một row và cập nhật index.
- [x] Network: diff theo SSID; chỉ add/remove row thay đổi; không clean toàn list.
- [x] Apps: remove/install cập nhật đúng row; chỉ full rebuild khi mở màn hoặc
      refresh chủ động.
- [x] Nếu buộc rebuild, bọc trong content intent có `hold_partial` và **không**
      cho heuristic diện tích promote thành GC16.
- [x] Host test: một checkbox tap không tạo dirty area > 1 row + vùng status.

### A8. Tránh driver merge thành FULL/AUTO

Driver i.MX6 `mxc_epdc_fb.c` (tham khảo) có logic merge:

```c
if (a->update_mode != b->update_mode) a->update_mode = UPDATE_MODE_FULL;
if (a->waveform_mode != b->waveform_mode) a->waveform_mode = WAVEFORM_MODE_AUTO;
```

`UPDATE_MODE_FULL` flush toàn bộ update đang chờ; `AUTO` có thể chọn GC16 theo
histogram. Vì vậy hai rect gần nhau với waveform/mode khác nhau có thể biến
một tap nhỏ thành full/AUTO.

- [x] Mỗi batch chỉ dùng **một** waveform + **một** update mode.
- [x] Không submit A2/DU cho key rồi REAGL/GC16_FAST cho text field trong cùng
      một vùng gần nhau nếu có thể merge; tách bằng thời điểm hoặc khoảng cách.
- [x] Log `merge_risk` nếu có nhiều hơn một waveform trong batch.
- [x] Test trên máy: tap keyboard/checkbox không thấy update `mode=1` (FULL)
      hoặc waveform `AUTO`.

### A9. Bề mặt xám và tốc độ settle

- [x] Audit `theme/tokens.json` + `gw_theme_tokens.h`: card `#EEEEEE`, border
      `#707070`, muted `#404040`, secondary `#303030`. Nền xám lớn buộc waveform
      16 mức và làm scroll/repaint đắt hơn.
- [x] Đổi card/list surface sang trắng + viền đen; chỉ giữ xám cho text phụ nhỏ
      nếu thật cần. Mục tiêu: phần lớn UI là đen/trắng như KOReader.
- [x] Giới hạn cập nhật scroll ~4 Hz; không queue nhiều update hơn panel settle.
- [x] Đo lại số update và ghost sau khi bỏ fill xám.

## 4. Workstream B — lỗi hiển thị P0

| ID | Vấn đề | Sửa | Verify |
| --- | --- | --- | --- |
| B01 | Home To-dos preview cắt chữ | Row 56 px + label 32 px; đặt height label ~40 px và `LV_LABEL_LONG_DOT`, hoặc row 64–72 px | `home-empty.png` không còn dòng bị cắt; test title dài tiếng Việt |
| B02 | Apps list cắt row 4; scrollbar đè Remove | List 440 vs 4×120=480; tăng 480 hoặc flex; `pad_right` cho scrollbar | `apps.png`, `apps-long.png`; host check partial row |
| B03 | Lock time đóng băng | Lưu `g_lock_time_label`; cập nhật khi lock và mỗi phút trong `status_update()` | Lock lúc 10:30, mở lại 10:31 → đúng |
| B04 | Lỗi chỉ hiện ở Home, không tự xóa | Toast trên `lv_layer_top()`, tự xóa 3 s; thay `recovery_set()` | Mở Reader lỗi từ Settings → toast hiện tại Settings |
| B05 | Back luôn về Home | Lưu `back_screen` khi mở Network/Apps/Update từ Settings | Settings → Apps → Back → Settings |
| B06 | Icon khóa Wi-Fi xuống dòng | Tách icon khóa thành widget, hoặc tăng width dBm ~190 px | `network-connected.png` |
| B07 | Dot due-date như dấu chấm câu | Đặt góc trên phải hoặc gạch chân nhỏ, không dưới số | `calendar.png` ngày 31 |
| B08 | “Application data will be kept.” lạc | Xóa helper text ở cuối Apps; chỉ dùng trong confirm | `apps.png` |

## 5. Workstream C — state/feedback/navigation (P1)

| ID | Vấn đề | Sửa |
| --- | --- | --- |
| C01 | Error label chỉ Home | Toast dùng chung, tự xóa, không force full |
| C02 | Lock time | Cập nhật mỗi phút + khi lock; không đợi rebuild screen |
| C03 | Back context | `g_back_screen` hoặc stack nhỏ cho secondary screens |
| C04 | Network message sai | Chọn mạng → “Selected: <SSID>”; connected → “Connected” |
| C05 | Connect vẫn active khi đã connected | Ẩn/disable Connect cho mạng đang kết nối; hiện trạng thái |
| C06 | Update result đè list | Tách `g_update_status` khỏi `g_update_list`; rescan riêng |
| C07 | Install update không chọn package | Row select + Install chỉ active khi có selection |
| C08 | Complete task không Undo | Dùng undo strip như delete |
| C09 | Due chỉ No date/Today/Tomorrow | Thêm Custom/Next week hoặc date picker |

## 6. Workstream D — Home, status bar, typography (P1)

| ID | Vấn đề | Sửa |
| --- | --- | --- |
| D01 | Home nhiều khoảng trống chết | Continue card theo content; card Calendar/To-dos thêm spacer trước footer hoặc height content-based |
| D02 | Lặp ngày tháng | Bỏ weekday/month khỏi calendar card hoặc đổi hero date |
| D03 | `[ ]` như checkbox nhưng không toggle | Dùng bullet/icon trung tính, hoặc làm checkbox thật |
| D04 | Status bar trộn icon/chữ, SSID chiếm chỗ | System tray: **một** indicator vạch tín hiệu (bỏ icon Wi-Fi trùng); Light icon + %; pin theo mức + sạc |
| D05 | Pin luôn `BATTERY_3` | Map % → BATTERY_EMPTY/1/2/3, thêm charging |
| D06 | Target sát nhau | Thêm gap/divider giữa Wi-Fi/Light/Lock/battery |
| D07 | Chữ 24 px ở status/nav | Nâng lên 28 px (caption token) |
| D08 | Frontlight track giống slider nhưng là bar | Chọn: slider thật (on-release) hoặc stepper có vạch chia, bỏ affordance slider |
| D09 | Light mở Settings, không tới Frontlight | Mở overlay sáng nhanh hoặc scroll thẳng tới Frontlight |
| D10 | Settings row bị cắt dưới nav | Canh chiều cao hoặc thêm scroll indicator rõ |

## 7. Workstream E — IA / content (P2)

| ID | Vấn đề | Sửa |
| --- | --- | --- |
| E01 | Calendar luôn “UPCOMING” cho ngày khác hôm nay | “DUE THIS DAY” hoặc kèm ngày cụ thể |
| E02 | Chỉ 3 due item, không +N | Thêm “+N more” row hoặc link tới To-dos |
| E03 | Không có nút Today | Thêm Today cạnh chuyển tháng |
| E04 | Apps row quá nhiều Remove | Overflow menu hoặc app detail |
| E05 | Apps row trộn id/version/description | Tách tên đậm, version, mô tả |
| E06 | Lock hint “Tap to unlock” không tap được | Tap toàn màn hình hoặc bỏ hint |
| E07 | Modal full-screen như page mới | Inset panel, giữ context |
| E08 | Tên Library/Reader/Applications/Apps lẫn lộn | Chốt glossary; đổi nav hoặc copy |
| E09 | Test host bỏ sót overflow | Thêm check container overflow, partial scroll row, scrollbar gutter |

## 8. Thứ tự thực hiện

### Phase 0 — Baseline & reproduce (0.5 ngày)

- Chốt hash/policy đang chạy.
- Đã chạy log 9 hành động; lưu vào
  `docs/evidence/pw3-ui-refresh-2026-11-01/`.
- Deployed build `149ae024…`, rollback `6020c6fe…`.

**Exit (đạt):** waveform gây flash cũ là GC16; build mới chỉ dùng GC16 ở
lock/unlock.

### Phase 1 — Chặn flash + scroll (1–2 ngày)

- Làm A1, A2, A3, A5, A6, A7, A8, A9.
- Chọn waveform A4 dựa trên log Phase 0; ưu tiên thử `gc16_fast`.
- Build host + ARM; cài với rollback.

**Exit gate:**
- 20 tap nav + 1 tap checkbox/apps/network: 0 GC16, 1 submit/tap.
- 10 lần scroll: 0 GC16, preview ~5 Hz + final on release, không full-screen.
- Không còn flash đen/trắng khi tap Add/Unlock/Connect/Lock.
- Ghost ≤ 1/5 sau 20 scene; = 0 ngay sau lock/unlock.
- Nếu không đạt, dừng và quay lại A0; không làm UI phase tiếp.

### Phase 2 — Lỗi hiển thị P0 (1 ngày)

- B01–B08.
- Chạy host-sim, so ảnh before/after.

**Exit:** 0 clipped label/row trong ảnh và host check mới.

### Phase 3 — State/feedback/navigation (1 ngày)

- C01–C03 trước (toast, lock time, Back), sau đó C04–C09.

**Exit:** kịch bản Settings → Apps → Back; Reader lỗi từ Apps; lock 2 phút;
chọn Wi-Fi; update không có package.

### Phase 4 — Home/status/typography (1–2 ngày)

- D01–D10.
- Đo lại heap LVGL; giữ < 240 KiB.

### Phase 5 — IA/polish (2–3 ngày)

- E01–E09.
- Chỉ làm sau khi Phase 1–4 đã qua device gate.

### Phase 6 — Release gate

- Host suite + QEMU smoke + ARM build.
- Cài lên PW3, giữ rollback.
- Đo lại 20 tap/20 scroll/10 trang reader; ghi flash count, ghost score,
  latency.
- Cập nhật `docs/BACKLOG.md` B09 và evidence.

## 9. Acceptance tổng

| Hạng mục | Tiêu chí |
| --- | --- |
| Tap nav | 0 GC16; ≤1 submit/tap; không đen/trắng toàn màn |
| Tap button | Không đảo nền primary; không full |
| Scroll | 0 GC16; preview ~4 Hz (max 4) + release REAGL; không ghost rõ |
| Modal | Mở/đóng không GC16; một scene `GC16_FAST` partial |
| Keyboard | Đóng keyboard không GC16; text field vẫn cập nhật |
| Lock | Time đúng ±1 phút; full GC16 được phép ở lock/unlock (U3) |
| Layout | Không label/row bị cắt ở 1072×1448; scrollbar không đè nút |
| Feedback | Lỗi hiện trên màn hiện tại và tự xóa |
| Back | Về đúng màn mở nó |
| Heap | ≤ 240 KiB; không rò fd/process |
| Ghost | Sau 20 scene: ≤ 1/5; = 0 ngay sau lock/unlock |

## 10. Dẫn chứng kỹ thuật

### 10.1 Web / tài liệu

- **KOReader User Guide — Scrolling types:** on-release scrolling “more
  suitable for e-ink screens” vì không refresh liên tục khi kéo; page chỉ nhảy
  khi nhả tay. Cũng nêu UI chủ yếu đen/trắng vì gradient/xám gây flashing và
  ghosting, và có tùy chọn giảm flash menu.
  <https://koreader.rocks/user_guide/>
- **E Ink waveform glossary (Inkframe):** GC16 16 mức xám, ~450 ms, có pha xoá
  gần INIT; DU/DU4/A2 nhanh nhưng 2–4 mức và để ghost; firmware tốt dùng
  DU/DU4 cho thao tác rồi GC16 định kỳ để dọn ghost.
  <https://inkframe.net/en/guides/e-paper-waveform-modes-glossary>
- **Partial refresh guide (epaper-components):** GC16 xoá ghost nhưng đảo
  đen/trắng; GL16 16 mức, không flash, để ít residue; DU 1-bit; A2 để residue;
  **không update nhanh hơn panel settle (~4 Hz)**, và nên budget GC16 sau
  10–50 partial. Vùng có pixel xám buộc waveform 16 mức.
  <https://epaper-components.dev/guides/partial-refresh/>
- **KOReader issue #1109:** GC16 full gây flash đen/trắng; `GC16_FAST` “can be
  used without flashing on PARTIAL”; REAGL có thể reverse-ghost.
  <https://github.com/koreader/koreader/issues/1109>

### 10.2 Code / driver

- `os/ui/lv_port_epdc.h`:
  - `gw_epdc_present()` promote `total * 2 >= screen` và budget partial thành
    full GC16;
  - scroll branch dùng DU4;
  - scene/modal/scroll dùng `gw_eink_*` intent;
  - chưa có `GC16_FAST`.
- `os/ui/gw-shell.c`:
  - `gw_ui_show()` hiện dùng transition DU4;
  - `gw_ui_request_full()` dùng transition DU4;
  - `lock_screen_cb()`, `lock_apply_status()`, `evt_handle_line()` load lock
    screen trực tiếp, không qua scene helper;
  - `gw_epdc_request_full()` còn ở keyboard settle và balanced/fast cleanup;
  - `todo_rebuild_list()`, `network_build_rows()`, `apps_apply_entries()` gọi
    `lv_obj_clean()` trên list lớn.
- `os/ui/lv_port_input.h`:
  - chỉ đọc một `SYN_REPORT` mỗi render cycle với `continue_reading = false`;
  - chưa coalesce vị trí scroll mới nhất, nên queue có thể trễ.
- Driver tham khảo `mxc_epdc_fb.c`:
  - merge hai update khác `update_mode` → `UPDATE_MODE_FULL`;
  - merge hai update khác `waveform_mode` → `WAVEFORM_MODE_AUTO`;
  - FULL flush toàn bộ update đang chờ; AUTO có thể chọn GC16.
  <https://github.com/canselcik/libremarkable/blob/master/reference-material/mxc_epdc_fb.c>

## 11. Rủi ro và quyết định mở

1. **DU4 toàn màn hình có thể vẫn bị nhìn là flash.** Phase 0 xác định;
   Phase 1 thử GC16_FAST/GL16, không quay lại GC16 mặc định.
2. **GC16_FAST (mode 3) phụ thuộc firmware.** Header Kindle ghi mode 3 là
   `WAVEFORM_MODE_GC16_FAST`; phải xác nhận trên panel, không chỉ tin header.
3. **Deep clean định kỳ vẫn gây flash.** U1 yêu cầu không ghost; plan đặt deep
   clean chính ở lock/unlock, chỉ thêm budget 20–30 partial nếu ghost > 1/5.
4. **Scroll mượt trên e-ink có giới hạn vật lý.** U2 chọn live preview
   ~4 Hz (max 4) + release REAGL; tinh chỉnh bằng `GW_UI_SCROLL_PREVIEW_MS`
   và `GW_UI_SCROLL_PREVIEW_MAX`. Không hứa 60 fps.
5. **UI nhiều mảng xám làm scroll đắt.** A9 chuyển card/list sang trắng + viền;
   giữ xám chỉ cho text phụ nhỏ.
6. **Rebuild list là nguồn flash ẩn.** A7 bắt buộc incremental; nếu không,
   checkbox/scan/remove vẫn có thể gây full GC16 dù scene đã sạch.
7. **Driver merge có thể biến batch nhỏ thành FULL/AUTO.** A8 bắt buộc một
   waveform/mode mỗi batch.
8. **Thêm toast/back stack/date picker làm tăng heap.** Đo sau từng phase;
   không vượt 240 KiB.
9. **Bản đang chạy có thể không phải HEAD.** Phase 0 bắt buộc xác minh hash;
   nếu không, mọi kết luận về flash đều có thể sai.

## 12. File dự kiến chạm

| File | Việc |
| --- | --- |
| `os/ui/gw-shell.c` | scene helper, lock paths, toast, Back, Home, network, todos, calendar, update |
| `os/ui/lv_port_epdc.h` | scene waveform, scroll intent, full-reason, GC16 policy |
| `os/ui/lv_port_input.h` | scroll intent sớm khi di chuyển |
| `os/ui/gw_ui_kit.c/h` | pressed style, modal, toast/undo dùng chung, scroll helper |
| `os/ui/gw_ui_style.h` | primary pressed không đảo nền |
| `os/ui/gw_ui_strings.h` | copy mới (Selected, Due this day, +N, Show) |
| `os/ui/host_sim.c` | overflow/partial-row/no-GC16 checks |
| `os/ui/sim/run-preview.sh` | thêm ảnh before/after cho state mới |
| `theme/tokens.json` | bỏ fill xám lớn, giữ bảng màu đen/trắng; nếu thêm type/space token cho status bar |

## 13. Việc làm ngay

1. Chạy Phase 0 trên máy: hash + policy + log 9 hành động.
2. Nếu log xác nhận `waveform=2` trong tap/scroll: làm Phase 1, dừng mọi việc
   polish UI.
3. Nếu log là DU4 nhưng mắt vẫn thấy flash: thử `GC16_FAST` trước, sau đó
   GL16/REAGL; không quay lại GC16 mặc định.
4. Trong Phase 1, bắt buộc xử lý **A7 rebuild list** và **A8 driver merge**;
   đây là hai nguồn flash ẩn dễ bị bỏ qua.
5. Sau khi hết flash và ghost ≤ 1/5, mới bắt đầu B01–B08 và gửi ảnh host-sim
   before/after.
