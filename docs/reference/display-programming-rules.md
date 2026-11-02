---
diataxis: reference
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# PW3 display programming rules

Date: 2026-11-01  
Scope: mọi thay đổi UI/refresh trong `os/ui` chạy trên Kindle Paperwhite 3
(1072×1448, framebuffer Y8, pitch 1088, EPDC i.MX6).  
Trạng thái: **bắt buộc**. Không merge thay đổi vi phạm các quy tắc dưới đây.

Mục tiêu kép, không được đánh đổi:

1. **Anti-ghost:** không để residue nhìn thấy sau thao tác.
2. **Responsive:** scroll/feedback phải cập nhật đủ nhanh để không có cảm giác
   “đứng hình”, trong giới hạn vật lý của e-ink.

---

## 1. Kiến trúc bắt buộc

```text
gw-shell.c  ->  LVGL widgets  ->  lv_port_epdc.h  ->  /dev/fb0 + MXCFB_SEND_UPDATE
                     ^
             lv_port_input.h (touch)
```

Quy tắc:

- **Chỉ `lv_port_epdc.h` được gọi `MXCFB_SEND_UPDATE`.**
- `gw-shell.c` không được tự ioctl, không tự đọc/ghi `/dev/fb0`.
- Mọi chuyển màn hình phải đi qua `gw_scene_show()` (hoặc `gw_ui_show()`).
- Mọi thay đổi text phải đi qua `label_set_if_changed()`.
- Danh sách dài phải cập nhật incremental; không `lv_obj_clean()` toàn list.

---

## 2. Ma trận waveform / intent

| Intent | Waveform | Mode | Ghi chú |
| --- | --- | --- | --- |
| Scene / modal / unlock | `GC16_FAST` (3) | partial | một submit cho cả màn |
| Content (text, clock, row) | `GC16_FAST` (3) | partial | không rebuild list |
| Feedback (pressed, toast) | `GC16_FAST` (3) | partial | chỉ vùng nhỏ |
| Scroll preview | `GC16_FAST` (3) | partial | 6 preview, mỗi 160 ms/lần kéo (PW3 overlay) |
| Scroll release | `GC16` (2) | partial | union toàn bộ vùng dirty của cú kéo |
| Idle scroll clean | `GC16` (2) | full | sau khi dừng cuộn 1.2 giây; throttle 3 giây |
| Deep clean | `GC16` (2) | full | lock/unlock, boot Home đầu tiên, hoặc opt-in budget |

Tuyệt đối không dùng `DU`, `A2`, `DU4` cho nội dung 16 mức xám (text, ảnh,
anti-alias). Các waveform này chỉ phù hợp feedback 1-bit và để lại ghost.

### Quy tắc một batch

- Mỗi lần present chỉ dùng **một waveform + một update mode**.
- Không trộn A2 cho key với REAGL/GC16_FAST cho text trong cùng batch.
- Không để diện tích dirty tự promote thành GC16.
- Log phải có `merge_risk=0`; nếu khác, tách batch.

---

## 3. Scroll specification (bắt buộc)

Scroll phải vừa nhanh vừa sạch:

```text
finger down + drag:
  - coalesce touch, chỉ vẽ frame mới nhất
  - preview mỗi 160 ms, tối đa 6 preview cho một lần kéo
  - waveform GC16_FAST partial

finger up:
  - submit vùng đã cuộn một lần cuối (GC16 partial)
  - chưa dọn ngay: chờ 1.2 giây không cuộn rồi mới full-screen GC16
  - cuộn tiếp thì hoãn; tối thiểu 3 giây giữa hai lần dọn
  - chỉnh bằng GW_UI_SCROLL_CLEAN_MS / GW_UI_SCROLL_CLEAN_MIN_MS
```

Cấu hình runtime:

| Biến | Mặc định | Ý nghĩa |
| --- | --- | --- |
| `GW_UI_SCROLL_PREVIEW_MS` | `160` | khoảng cách preview; `0` = tắt preview |
| `GW_UI_SCROLL_PREVIEW_MAX` | `6` | số preview tối đa mỗi lần kéo; `0` = tắt preview |
| `GW_UI_SCROLL_WAVEFORM` | scene waveform | waveform preview, nên `gc16_fast` |
| `GW_UI_SCROLL_RELEASE_WAVEFORM` | `gc16` | waveform submit cuối |

PW3 overlay (`/etc/conf.d/gw-system`) đặt 6 preview/160 ms để cuộn mượt. Ghost
được dọn bằng một full-screen GC16 **sau khi dừng cuộn 1.2 giây**, không phải
sau mỗi lần nhả. Không đặt `PREVIEW_MS=0` trên binary cũ: code cũ hiểu 0 là
“không throttle” và vẽ mọi frame; source hiện đã sửa để 0 = tắt preview.

File tương ứng (nếu cần cấu hình trên máy):
`/etc/gleworks/scroll-waveform`, `/etc/gleworks/scroll-release-waveform`,
`/etc/conf.d/gw-system`.

### Khi ghost vẫn còn

Thứ tự xử lý:

1. Kiểm tra log `paint policy=... preview_ms=... preview_max=...`.
   `preview_max=0` mới là tắt preview; `preview_ms=0` trên binary cũ lại vẽ
   mọi frame.
2. Giảm preview (`6 → 4 → 2`) nếu ghost tạm quá nhiều trong lúc kéo.
3. Kiểm tra full GC16 khi idle có chạy không (log/nháy sau 1.2 giây dừng).
4. Nếu vẫn ghost: dùng page-based scroll thay vì continuous.

### Khi vẫn chậm

1. Giảm `GW_UI_SCROLL_PREVIEW_MS` nhưng **không dưới 160 ms**.
2. Kiểm tra log: mỗi preview phải là một submit, không queue nhiều frame.
3. Không hứa 60 fps; e-ink chỉ settle khoảng 3–5 Hz.
4. Fallback hợp lệ: nút Page up/down hoặc on-release.

---

## 4. Anti-ghost rules

- GC16 full chỉ được phát ra từ:
  - lock/unlock (U3, điểm dọn ghost chính);
  - `GW_UI_SCENE_WAVEFORM=gc16` (opt-in rõ ràng);
  - budget idle `GW_UI_DEEP_CLEAN=1` sau 30 partial.
- Không GC16 cho tap, nav, checkbox, scan, keyboard, modal, scroll preview.
  Ngoại lệ: một full-screen GC16 khi kết thúc một drag có di chuyển, để dọn
  di ảnh trước khi người dùng đọc tiếp.
- `paint-policy=quality` không được ép GC16 mỗi scene; policy legacy map sang
  `GC16_FAST`.
- Sau mỗi lần kéo phải có một submit `REAGL` để dọn residue.
- Không dùng nền xám lớn; surface mặc định trắng + viền đen.
- Không animation, gradient, shadow, dark mode.

### Gate ghost

- 20 tap điều hướng: **0 GC16, 0 full**.
- Sau 20 tap: ghost nhìn thấy ≤ 1/5.
- Ngay sau lock/unlock: ghost = 0/5 (full GC16).
- Drag 600 ms: tối đa 6 preview `GC16_FAST` (160 ms) + 1 release GC16 partial; full GC16 chỉ sau khi dừng 1.2 giây.

---

## 5. Layout / UI rules

- Panel 1072×1448; touch target tối thiểu **96×96 px**.
- Không label/row bị cắt; dùng `LV_LABEL_LONG_DOT` + chiều cao đủ line-height.
- Danh sách scroll phải có gutter cho scrollbar (`pad_right` ≥ 16 px).
- Topbar chỉ có **một** indicator network (vạch tín hiệu). Không thêm icon
  Wi-Fi trùng.
- Icon USB (`LV_SYMBOL_USB USB`) chỉ hiện khi status `usb=host-class`; ẩn với
  `usb=none`/`usb=power`. Cập nhật ngay từ event `USB CONNECTED`/`USB
  DISCONNECTED`, không chờ poll 30 s.
- Status tray: network bars, Light icon + %, pin theo mức + sạc.
- Không lồng clickable trong clickable.
- Modal là inset sheet, không phải full-screen page.
- Nút primary giữ nền đen khi pressed; chỉ đổi border/label, không đảo trắng.
- “Storage” trong Settings đọc `/mnt/us` (user data), không đọc rootfs;
  hiển thị `free of total` (MB khi dưới 1 GB, GB khi từ 1 GB).

---

## 6. Incremental list rules

- To-dos: diff theo `id`; toggle/delete/add chỉ chạm một row.
- Network: diff theo SSID; rescan chỉ thêm/xóa/update row thay đổi.
- Apps: diff theo app id; Remove xóa một row, không rebuild list.
- Không gọi `lv_obj_clean()` trên list lớn sau mỗi thao tác.
- Nếu buộc rebuild, phải chạy trong content intent và không cho area promote
  thành GC16.

---

## 7. Diagnostics bắt buộc

Mỗi submit EPDC phải log được:

```text
waveform=<n> mode=<0|1> area=<px> intent=<scene|content|feedback|scroll|deep_clean>
reason=<intent|scroll_preview|scroll_release|lock|budget>
merge_risk=<0|1>
```

Build chẩn đoán:

```sh
GW_UI_PERF=1 GW_UI_PERF_LOG=1 GW_UI_DIAG=1
touch /run/gw-ui-perf
```

Công cụ:

```sh
gwctl ui-nav HOME|SETTINGS|CALENDAR|TODOS|NETWORK|APPS|UPDATE
gwctl ui-tap X Y
gwctl ui-screen
gwctl ui-scroll DY      # scroll object, một lần
gwctl ui-drag DY        # giữ 600 ms, kiểm tra preview + release
```

---

## 8. Test gates trước khi deploy

1. Host suite: `./os/ui/sim/run-preview.sh` → **0 failed**.
2. `make check` với `EPDC_ACCEPTED=1 TOUCH_ACCEPTED=1 DIAG=1 PERF=1`.
3. ARM build OK; giữ rollback bản đang chạy.
4. Trên PW3:
   - 9 action: chỉ lock có GC16.
   - 20 tap: 0 GC16.
   - drag: preview `GC16_FAST` 160 ms, release GC16 partial trên union, `merge_risk=0`; idle 1.2 giây → full GC16.
   - lock: đúng một GC16 full, label `Locked`, không re-lock sau unlock.
5. Chụp framebuffer + lưu log vào `docs/evidence/`.

Không đạt gate anti-ghost hoặc responsive thì **không merge**.

---

## 9. Anti-patterns

- Dùng `lv_obj_clean()` trên list 8–10 row sau mỗi checkbox.
- Submit từng key A2 và text DU trong cùng frame.
- Để diện tích dirty > 50% tự thành GC16.
- Dùng DU4 cho text anti-alias.
- Bật preview scroll 10–20 Hz để “cho mượt”.
- Thêm icon network thứ hai vào topbar.
- Đặt GC16 trên mỗi scene vì “cho sạch ghost”.
- Đổi waveform im lặng khi ioctl fail mà không log.

---

## 10. Checklist cho mọi PR UI/refresh

- [ ] Intent rõ ràng: scene / content / feedback / scroll / deep clean.
- [ ] Một waveform + một mode mỗi batch.
- [ ] Không GC16 ngoài lock/unlock, idle scroll clean, boot Home đầu tiên hoặc opt-in.
- [ ] Scroll: preview ≤ 6 lần, mỗi 160 ms; release GC16 partial trên union; full GC16 khi idle 1.2 giây.
- [ ] List cập nhật incremental.
- [ ] Text qua `label_set_if_changed`.
- [ ] Layout không cắt, target ≥ 96 px, scrollbar có gutter.
- [ ] Host suite 0 failed.
- [ ] Device log đạt gate ghost + responsive.
