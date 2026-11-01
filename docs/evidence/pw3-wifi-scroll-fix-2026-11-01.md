# PW3 Wi-Fi list + scroll ghost fix — 2026-11-01

## Triệu chứng

1. Màn Network → Scan chỉ hiện chuỗi hex (ví dụ `4e686120426172726f6e206631`)
   thay vì tên Wi-Fi, mọi mạng đều `0 dBm`.
2. Cuộn ở bất kỳ trang nào để lại di ảnh (ghost) rất nhiều, chữ không đọc được;
   cảm giác ban đầu là “flick/tear”.

## Nguyên nhân 1 — lệch phiên bản service/shell

- `/usr/sbin/gw-shell` trên máy là `149ae024…` (bản UI refresh), đã có parser
  hex và đọc 3 trường.
- `/usr/sbin/gw-system` trên máy là `f9025e0b…` (build 2026-10-31 20:15), cũ
  hơn source `gw-system.c` (2026-11-01 00:24). Service cũ trả
  `NET SSID <hex>` không có signal/secure.
- Shell thấy thiếu trường nên rơi vào nhánh “old service: plain SSID”, in nguyên
  chuỗi hex và gán signal 0 → đúng hiện tượng ảnh chụp.

Sửa: build lại `gw-system` từ source với các flag đang dùng
(`GW_POWER_LIGHT_ACCEPTED=1`, `GW_POWER_SUSPEND_ACCEPTED=1`,
`GW_LIFECYCLE_SPIKE=1`, `GW_WIFI_SPIKE=1`), strip, deploy.

| | Hash |
| --- | --- |
| gw-system cũ | `f9025e0b7d7869e855b20d50be756913b6ef4657439146d931d095e90cb7d211` |
| gw-system mới | `aeedb4811f4ce7172c8b308841b8af17c334496b85783a0b9a7ebae1b02ecfa2` |
| backup trên máy | `/usr/sbin/gw-system.before-netscan-20261101` |

Kết quả: Scan hiện đúng tên (`Olak Coffee`, `FPT Telecom-2CA1-IOT`, …), dBm thật
(−58, −50, −76, −74) và icon khoá. Ảnh chứng minh:
`work/private/screens/wifi-fixed-20261101.png`; ảnh lỗi:
`work/private/screens/wifi-issue-20261101.png`.

Ghi chú build: toolchain 4.9.4 không có `libssp.a` cho ARM, nên service mới build
với `-fno-stack-protector` (bản cũ có SSP). Cần build lại bằng toolchain musl
đầy đủ khi có, hoặc chấp nhận giảm hardening này.

## Nguyên nhân 2 — preview scroll và ghost

Cấu hình ban đầu: 4 preview/250 ms, `GC16_FAST`; release `REAGL`. Các lần thử:

1. `PREVIEW_MS=0` trên binary cũ bị hiểu là "không throttle" nên vẽ mọi frame
   kéo → di ảnh rất nặng. Đã sửa source để `preview_max == 0 ||
   preview_ms == 0` trả về sớm.
2. GC16 **partial** trên vùng cuộn không đủ xoá ghost trên panel này.
3. Full-screen GC16 ngay khi nhả tay thì sạch nhưng nháy đen-trắng cả màn sau
   mỗi lần cuộn → không chấp nhận.
4. Chốt: 6 preview/160 ms để cuộn mượt; khi người dùng **dừng cuộn 1.2 giây**
   mới chạy một full-screen GC16. Nếu cuộn tiếp thì hoãn; tối thiểu 3 giây giữa
   hai lần dọn. Có thể chỉnh qua `GW_UI_SCROLL_CLEAN_MS` và
   `GW_UI_SCROLL_CLEAN_MIN_MS` trong `/etc/conf.d/gw-system`. Cuối cùng thêm một full GC16 khi Home được vẽ lần
   đầu sau boot để xoá bóng ảnh boot (chỉ một lần mỗi boot, marker
   `/run/gw-first-home-clean`).

Thay đổi source:

- `os/ui/lv_port_epdc.h`: gộp vùng dirty của cả cú kéo (`g_scroll_union`),
  lên lịch dọn khi idle, và guard `preview_ms/max == 0`.
- `os/ui/gw-shell.c`: render loop gọi full GC16 khi hết 1.5 giây idle sau cuộn;
  full GC16 một lần ở lần vẽ Home đầu tiên sau boot.
- `os/devices/pw3/overlay/etc/conf.d/gw-system`: preview 6 frame/160 ms;
  `scroll-release-waveform=gc16`.

| | Hash |
| --- | --- |
| gw-shell trước | `149ae0243ded075cfe16ab44e8fa3beabf9f3cf45024442177184899e5c36a05` |
| gw-shell cuối | `df2c9bd7b24cf595d908ece87ceecfa154e8dda9c65530c5fcdaf29dc6bbe0a5` |
| backup trên máy | `/usr/sbin/gw-shell.before-scrollclean-20261101` (149ae) |

Log trên máy xác nhận chuỗi `scroll cleanup scheduled` → `scroll idle clean
fired` → `full update reason=lock` (GC16 FULL). Người dùng xác nhận cuộn mượt và
dừng tay thấy một nháy dọn; ghost trong lúc kéo là bản chất preview e-ink. Khoá/mở
vẫn là cách dọn tức thì nếu cần.

## Đánh đổi

- Không thể đồng thời "mượt, sạch, không nháy": panel cần full GC16 để xoá hết
  di ảnh. Chế độ chốt là mượt khi kéo, một nháy dọn sau khi dừng.
- Trong lúc kéo vẫn thấy ghost tạm; nó được dọn khi dừng.
- Full GC16 chỉ chạy khi idle, không chạy khi KOReader đang foreground (shell bị
  service dừng trước khi reader chạy).

## Trạng thái Wi-Fi

Đã bật lại Wi-Fi (`wifi-enabled.conf=1`). Mạng đã lưu hiện không kết nối được
(`wifi-state=FAILED`) nên service vẫn retry; người dùng cần chọn mạng trong
Network → Scan → Connect. Danh sách quét đã hiển thị đúng.
