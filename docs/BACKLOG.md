# Backlog duy nhất — Kindle Alpine

Lịch sử được giữ nguyên theo các nguồn nhập bên dưới; các mốc NO-GO cũ có thể
đã được bằng chứng về sau thay thế. Trạng thái hiện hành nằm ở
[guides/CURRENT.vi.md](guides/CURRENT.vi.md), không suy từ checkbox lịch sử.
Ngày ghi trong từng nguồn là ngày gốc; thứ tự nhập không phải thứ tự thời gian.
Các đường dẫn trong transcript/lệnh cũ là snapshot lịch sử, không phải lệnh để
chạy lại. Hướng dẫn thao tác hiện hành ở guides/.

## Công việc còn mở

| ID | Trạng thái | Việc cần làm |
| --- | --- | --- |
| B01 | Chưa làm | Release mới gồm full refresh và dọn locale; xác minh gói/ảnh trước triển khai |
| B02 | Chờ kiểm chứng | Đọc nhiều trang thực tế sau đổi chính sách refresh; kiểm tra ghost sau các lần cuộn và sau khi idle-clean chạy |
| B03 | Một phần | Wi-Fi native + auto-reconnect đã đạt; Tailscale/SSH sau xác thực còn mở |
| B04 | Chưa triển khai | Web UI upload tài liệu có xác thực qua VPN |
| B05 | Một phần | Mem suspend/RTC wake/đường nút đã đạt; bấm nút thật và đo điện năng còn mở |
| B06 | Chưa kiểm chứng | Boot Amazon fallback sau dọn; không tự thử chỉ để đóng backlog |
| B07 | Một phần | App runtime: package/installer/UI đã đạt; còn generic launch, UID/limit/fd handoff, crash/OOM, data image và checklist `AS-*`. Contract: [2026-10-31-app-session-contract.md](technical/superpowers/specs/2026-10-31-app-session-contract.md); kế hoạch: [2026-10-31-app-runtime.md](technical/superpowers/plans/2026-10-31-app-runtime.md) |
| B08 | Một phần | App manager + signed USB installer/update UI đã đạt; OS A/B, catalog online và UID riêng còn mở |
| B09 | Đã deploy, chờ nghiệm thu thiết bị | UI polish Phase 0–7 host đạt 2504 check/0 fail; đã push ARM 3.064.832 byte (sha256 `6020c6fe…`) lên `/usr/sbin/gw-shell`, rollback `/usr/sbin/gw-shell.before-ui-polish-20261101`, `/usr/sbin/gw-shell.before-network-scroll` và `/usr/sbin/gw-shell.before-tap-flash`. Ảnh framebuffer xác nhận Home mới; còn cảm ứng/ghosting và paint-quality regression trên PW3 |
| B10 | Đã implement + device-verified, đã được thay một phần | Gộp phản hồi flash sau mỗi tap/scroll với review UI 2026-11-01 thành [PW3-UI-REFRESH-UX-PLAN-2026-11-01.md](technical/PW3-UI-REFRESH-UX-PLAN-2026-11-01.md). Đã implement workstream A–E: `GC16_FAST` partial cho scene/scroll/content, GC16 full chỉ ở lock/unlock hoặc budget idle, scroll live-preview + final on release, incremental To-dos/Network/Apps, toast/Back/lock time, network/update state, Home/tray/calendar/app sheet/due options; host suite 2629 check 0 failed, ARM build OK. Bản `149ae024…` đã deploy và giữ rollback `6020c6fe…`; 9-action log + 20 tap cho **0 GC16 ngoài lock**. Chính sách scroll sau đó được tinh chỉnh lại theo B11 (6 preview/160 ms + idle GC16), nên số preview/release trong mục này là lịch sử của build 149ae, không phải cấu hình đang chạy. Bằng chứng: `docs/evidence/pw3-ui-refresh-2026-11-01/` |
| B11 | **Hoàn tất + đã deploy** | Tối ưu dung lượng, sửa Wi-Fi list và ghost khi cuộn (2026-11-01). Userstore 871 MiB → 2.87 GiB trống; root image 256 → 176 MiB, giữ bản gốc trên host. Wi-Fi Scan sửa bằng `gw-system` mới `aeedb481…` (service cũ trả hex thiếu signal/secure). Cuộn: `gw-shell` cuối `df2c9bd7…`, 6 preview/160 ms, union vùng dirty, full GC16 sau khi dừng cuộn 1.2 giây (throttle 3 giây, chỉnh bằng `GW_UI_SCROLL_CLEAN_MS`/`MIN_MS`), full GC16 một lần ở Home đầu tiên sau boot. Log xác nhận `scroll cleanup scheduled` → `scroll idle clean fired` → full update; người dùng xác nhận cuộn mượt, dừng tay một nháy dọn, Home sạch bóng boot. Evidence: `docs/evidence/PW3-STORAGE-OPTIMIZATION-2026-11-01.md`, `docs/evidence/pw3-wifi-scroll-fix-2026-11-01.md` |
| B12 | **Hoàn tất + đã deploy** | Repository hardening sau review toàn repo 2026-11-01: capture git, installer/signature TOCTOU + setuid, quyền `gw-system`, lỗi UI, an toàn tooling, build tái lập, public source, test/CI. Phase 0–6 đã implement, `make check` pass; đã build và deploy ARM `gw-system` `e6ca00b9…`, `gw-appctl` `6361dbb5…`, `gw-shell` `ea4a0631…` lên PW3, giữ rollback `*.before-hardening-20261101`. Kế hoạch và giới hạn còn lại: [2026-11-01-repository-hardening.md](technical/superpowers/plans/2026-11-01-repository-hardening.md) |

| B13 | **Đã sửa + deploy** | Nhấn giảm độ sáng gây nhấp nháy đen/trắng toàn màn: `RELEASED` trên trang Settings cuộn được bị hiểu là scroll, rồi idle cleanup gọi full GC16. Đã chỉ cho release sau khi thực sự kéo mới đánh dấu scroll, và tắt full-screen scroll cleanup mặc định và đặt `GW_UI_SCROLL_CLEAN_MS=0` trong `/etc/conf.d/gw-system` (bản cũ còn export 1200). `gw-shell` `74864cdb…` đã deploy; ghost cuộn nhẹ chấp nhận được. |

| B14 | **Đã implement + deploy** | Engine hiển thị chung + profile theo màn hình/loại nội dung: Settings/Notes (text) không preview khi kéo, Network/Apps/Update (list) 2 preview/160 ms; cleanup GC16 partial theo vùng; `GW_UI_FULL_REFRESH_AFTER` cho refresh định kỳ (mặc định tắt). `gw-shell` `3083b5de…`. Đo `GW_UI_PERF` + panel wait và so sánh waveform: [PW3-PAINT-PERF-2026-11-01.md](evidence/PW3-PAINT-PERF-2026-11-01.md). |

| B15 | **Đã deploy + nghiệm thu** | Tối ưu visibility tĩnh cho trang chữ: Settings/Notes chuyển page-snap sang một lần GC16 (không cleanup thứ hai), full refresh định kỳ mỗi 15 page-snap (`GW_UI_FULL_REFRESH_AFTER=15`). `gw-shell` `5fef3b22…`; rollback `before-quality-text-20261101`. Người dùng xác nhận chất lượng hiện tại chấp nhận được (2026-11-01). |

| B16 | **Đã deploy, chờ cảm nhận tốc độ** | Giảm độ trễ trang chữ: Settings/Notes page-snap dùng REAGL thay GC16 (một lần/trang), giữ full refresh mỗi 15 trang. `gw-shell` `ef922f25…`; rollback `before-reagl-text-20261101`. |

| B17 | **Đã thử, đã revert** | DU4 nhanh hơn nhưng mất nét/mất chữ trên trang text; đã quay lại GC16 một lần/trang (`gw-shell` `5fef3b22…`). Giới hạn: waveform 16 mức xám trên PW3 mất ~0,66 s/cập nhật lớn. |

Đây là danh sách theo dõi, không tự cấp phép thay đổi boot hay mở rộng task.

## Mục lục lịch sử nhập từ repo

- [ours/linux/EXECUTION.md](history/BACKLOG.md#history-001)
- [ours/APPLIANCE-PLAN.md](history/BACKLOG.md#history-002)
- [ours/EXPANSION-PLAN.md](history/BACKLOG.md#history-003)
- [ours/PLAN.md](history/BACKLOG.md#history-004)
- [ours/VERIFICATION.md](history/BACKLOG.md#history-005)
- [ours/docs/superpowers/plans/2026-10-29-boot-gate-batch.md](history/BACKLOG.md#history-006)
- [ours/docs/superpowers/plans/2026-10-29-independent-linux.md](history/BACKLOG.md#history-007)
- [ours/docs/superpowers/plans/2026-10-29-popup-errors.md](history/BACKLOG.md#history-008)
- [ours/docs/superpowers/plans/2026-10-30-alpine-safe-native-boot.md](history/BACKLOG.md#history-009)
- [ours/docs/superpowers/plans/2026-10-30-stock-kernel-alpine.md](history/BACKLOG.md#history-010)
- [ours/linux/boot-gate-batch/TEST-MATRIX.md](history/BACKLOG.md#history-011)
- [ours/linux/bootstrap/PLAN.md](history/BACKLOG.md#history-012)
- [ours/linux/native-display/PLAN.md](history/BACKLOG.md#history-013)
- [ours/linux/native-handoff/PLAN.md](history/BACKLOG.md#history-014)
- [ours/linux/native-stage/VERIFICATION.md](history/BACKLOG.md#history-015)
