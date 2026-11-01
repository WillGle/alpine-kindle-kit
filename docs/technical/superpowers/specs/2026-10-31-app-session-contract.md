# Hợp đồng phiên ứng dụng (APP-SESSION) — PW3

- Trạng thái: **DRAFT cập nhật 2026-10-31**. Lớp package/app manager đã
  triển khai và nghiệm thu trên PW3; lớp session/launch tổng quát **chưa**
  triển khai. Tài liệu này không thay đổi mã, thiết bị hay boot.
- Ngày: 2026-10-31.
- Phạm vi: PW3 `G090KB`, firmware/kernel tham chiếu `5.16.2.1.1` /
  `3.0.35-lab126`. Không hứa hỗ trợ model khác.
- Liên quan:
  - [Thiết kế OS v1](2026-10-30-minimal-alpine-reader-os-design.md)
  - [Kế hoạch OS](../plans/2026-10-30-minimal-alpine-reader-os.md)
  - [APP-PACKAGE](../../../../os/system/APP-PACKAGE.md)
  - [IPC](../../../../os/system/IPC.md)
  - [Khảo sát prior art](../../../ref/EINK-LINUX-PRIOR-ART.vi.md)
- Định nghĩa "đạt" của tài liệu này: toàn bộ checklist `AS-*` bên dưới có bằng
  chứng trên máy thật, cùng boot ID, không thay đổi bootloader/kernel, và đường
  cứu hộ USB độc lập vẫn hoạt động.

## 0. Cập nhật triển khai — 2026-10-31

Đợt cập nhật lớn đã làm thay đổi giả định của contract. Trạng thái thực tế:

| Hạng mục | Trạng thái | Bằng chứng |
| --- | --- | --- |
| Signed bundle `.gwapp` + `.gwapp.sig` | **Đã có, CORE** | `docs/evidence/pw3-app-installer-2026-10-31.md` |
| Cài/update/remove, giữ version cũ + app data | **Đã có** | cùng evidence; `os/system/gw-apps.c` |
| Từ chối tamper/unsigned/link/`..` | **Đã có** | cùng evidence |
| UI Applications + App updates | **Đã có** | `docs/evidence/pw3-app-ui-2026-10-31/` |
| Mem suspend + RTC wake + đường evdev | **Đã có** (uinput; nút vật lý thật đã xác nhận lock → suspend → wake) | `docs/evidence/pw3-power-suspend-2026-10-31.md`, `docs/evidence/pw3-power-physical-button-2026-10-31.md` |
| Paint/transition policy (DU4, section isolation) | **Đã có, đo trên máy** | `docs/technical/PW3-PAINT-POLICY.md` |
| Touch/power injection để test lặp | **Đã có** (`uinput`) | `tools/maintenance/pw3-uinput-touch.py`, `pw3-uinput-power.py` |
| Wi-Fi lưu cấu hình + auto-reconnect | **Đã có** | `docs/evidence/pw3-wifi-autoconnect-2026-10-31.md` |
| Idle log/refresh | **Đã sửa** | `docs/evidence/PW3-PHASE1-OPTIMIZATION.md` |
| Launch app theo `id` (app ngoài Reader) | **Chưa có** | `APP LAUNCH` chỉ chấp nhận `koreader` |
| UID riêng, `RLIMIT_*`, watchdog | **Chưa có** | mọi app hiện chạy root; ACCEPTANCE ghi rõ |
| Bàn giao fb/input qua fd + `libgwdisplay` | **Chưa có** | Reader tự mở `/dev/fb0` |
| Crash → Home, OOM, treo, fd leak | **Chưa kiểm chứng** | TS. `CURRENT.vi.md` |
| Data image riêng | **Chưa nghiệm thu thiết bị** | `tools/build/pw3-data-image.py` |

Hệ quả cho contract:

1. Mục 11 (package) không còn là kế hoạch; nó là mô tả hiện trạng, và định
   dạng v1 là `.gwapp`, không phải `.apk`.
2. Trọng tâm còn lại là **runtime**: launch app ngoài Reader, UID/giới hạn,
   bàn giao hiển thị/input, lỗi và power.
3. Các checklist `AS-*` đã có bằng chứng được đánh dấu; phần lớn `AS-13`…
   `AS-21` vẫn là khoảng trống lớn nhất.
4. Chưa được phép gọi là "app platform" đầy đủ khi app đã cài vẫn không có
   đường chạy ngoài built-in Reader.

## 1. Mục đích

Chốt cách OS bàn giao màn hình, cảm ứng, nút nguồn và tài nguyên cho **một ứng
dụng foreground** tại một thời điểm, để có thể thêm app mà không phá trải nghiệm
đọc sách, không tạo boot loop và không cần compositor.

Tài liệu này **không** định nghĩa:

- Định dạng gói cài đặt (đã có `APP-PACKAGE.md`, đang là đề xuất).
- Giao diện Home, thư viện hay màn hình khóa.
- Cơ chế A/B update.
- Sandbox mạnh, namespace, seccomp hoặc container.

## 2. Nguyên tắc bất biến

1. **Một foreground app.** Không đa cửa sổ, không overlay, không chạy hai app
   cùng sở hữu màn hình.
2. **OS sở hữu phần cứng.** Chỉ dịch vụ hệ thống được điều khiển nút nguồn,
   frontlight, suspend và mạng. App không gọi sysfs trực tiếp cho các việc này.
3. **Bàn giao độc quyền.** Shell nhường framebuffer/input cho app; khi app đóng,
   OS lấy lại và vẽ Home. Không có app nào vẽ nền.
4. **Không compositor.** Không X11, Wayland, broker daemon đa cửa sổ trong v1.
5. **Refresh policy tập trung.** Một nơi quyết định partial/full và chống
   ghosting; app chỉ gửi vùng thay đổi và gợi ý.
6. **App là phần mềm tin cậy, không phải sandbox.** Kernel không có
   namespace/seccomp/cgroup; chỉ có DAC bằng UID và giới hạn tài nguyên.
7. **Lỗi app không được thành lỗi OS.** Crash/treo/OOM phải quay về Home, không
   boot loop, không kéo PID1.
8. **Đường lui luôn tồn tại.** Rescue USB độc lập rootfs; app không được đụng
   p1, boot gate hoặc kernel.
9. **Không hứa quá bằng chứng.** Ảnh framebuffer, process còn sống hay build
   host không phải nghiệm thu.
10. **Đọc sách là ưu tiên.** Mọi quyết định tài nguyên phải bảo vệ đường
    Home → Reader → Home.

## 3. Thành phần và quyền sở hữu

| Thành phần | Sở hữu | Không được làm |
| --- | --- | --- |
| `gw-system` (dịch vụ hệ thống) | Nút nguồn, frontlight, suspend, trạng thái pin/sạc, mạng, vòng đời app foreground, IPC | Vẽ UI; chạy logic đọc sách |
| `gw-shell` (Home/Lock, LVGL) | Nội dung Home/Lock; yêu cầu launch/stop app qua IPC | Tự mở nút nguồn; tự suspend; tự cài app |
| App foreground | Nội dung toàn màn hình trong phiên của nó | Giữ framebuffer sau khi thoát; điều khiển power/mạng; mở nút nguồn |
| Reader (KOReader + adapter) | Nội dung đọc, lưu vị trí, nút Return to Home | Quản lý Wi-Fi/power/update |
| Rescue p1 | Cứu hộ độc lập | Bị app hoặc installer sửa |

Trong v1, logic phiên nằm **trong cùng tiến trình `gw-system`** (hoặc thư viện
liên kết vào nó), không tạo daemon thứ hai cùng sở hữu phần cứng. CLI
`gwctl app ...` chỉ là client IPC để kiểm thử.

**Hiện trạng 2026-10-31:** `gw-system` đã sở hữu nút nguồn, frontlight,
suspend, Wi-Fi, app install/update/remove và vòng đời Reader cố định. Nó
**chưa** có launch app tổng quát: `APP LAUNCH` chỉ chấp nhận `koreader`, và
`APP EXIT`/`APP CRASH` vẫn trả `not-accepted`. Shell đã có màn hình
Applications nhưng app do người dùng cài chưa có nút Open; chỉ built-in Reader
được mở.

## 4. Trạng thái phiên

```text
BOOT
  └─> HOME ──launch──> LAUNCHING ──ok──> FOREGROUND
        ^                   │                │
        │                   └──fail──┐      ├─exit 0──────┐
        └───────────────────────────┴──────┴─crash/hang───┤
                                                            v
HOME <──────────────── STOPPING <──────────────────────────┘

HOME/FOREGROUND ──short power──> LOCKED ──unlock──> HOME/FOREGROUND
HOME/FOREGROUND ──sleep policy──> SUSPENDING ──> RESUMING ──> HOME/FOREGROUND
```

Quy tắc:

- `LAUNCHING` có timeout (đề xuất 5 giây). Quá hạn → kill, log, về `HOME`.
- `STOPPING` có timeout (đề xuất 3 giây cho `SIGTERM`, sau đó `SIGKILL`).
- `FOREGROUND` chỉ có một app ID; service từ chối launch thứ hai.
- `LOCKED` vẫn giữ app trong bộ nhớ nhưng app không nhận input và không được vẽ.
- `SUSPENDING` chỉ vào sau khi refresh hoàn tất; app nhận tín hiệu chuẩn bị
  nhưng OS không chờ vô hạn.

## 5. Trình tự bàn giao

1. Home nhận yêu cầu mở app (chạm hoặc IPC).
2. Service kiểm tra app ID, manifest, trạng thái phiên, dung lượng/tiến trình cũ.
3. Service dừng vẽ Home; hoàn tất refresh cuối; đóng hoặc chuyển fd màn hình.
4. Service tạo tiến trình app với UID riêng, giới hạn tài nguyên, env và fd.
5. App vẽ toàn màn hình; service theo dõi `waitpid`, fd, RSS và thời gian.
6. App thoát: service thu hồi fd, đóng input, ghi log, chạy full refresh, vẽ Home.
7. Nếu app crash/treo/OOM: cùng bước 6 nhưng kèm thông báo lỗi đọc được trên
   Home; không tự chạy lại app.

## 6. Hợp đồng dành cho app

### 6.1 Danh tính

- `id`: theo manifest hiện có; không phải đường dẫn.
- UID riêng `gwapp-<id>` (>= 1000), không phải root; GID riêng hoặc group tối
  thiểu. Không dùng chung UID giữa các app.
  - **Hiện trạng:** chưa có UID riêng; app/Reader chạy root. Đây là mục tiêu
    của giai đoạn runtime, không phải tính năng đã có.
- Payload: `/var/lib/gleworks/apps/<id>/<version>/`, root sở hữu, `0700` hiện
  tại (đích: `0555`), không ghi.
- `active` là symlink tới version đang chọn; chỉ `gw-appctl`/service đổi.
- Data: `/var/lib/gleworks/appdata/<id>/`, hiện root `0700`, đích là UID app
  `0700`. **Không** đặt data trên FAT/p4. Khi có data image riêng, đường dẫn
  này là điểm mount, không đổi contract.

### 6.2 Tiến trình và tín hiệu

- App chạy foreground, không daemonize, không fork giữ tiến trình sau khi exit.
- `SIGTERM`: lưu trạng thái và thoát trong 3 giây.
- `SIGUSR1`: chuẩn bị ngủ (lưu vị trí, ngừng vẽ); không được chặn quá 2 giây.
- `SIGUSR2`: đã thức; phục hồi trạng thái và vẽ lại.
- Nút nguồn **không** được app đọc/giữ độc quyền. App nhận sự kiện pause/lock
  qua IPC hoặc tín hiệu, không tự quyết định ngủ.

### 6.3 Exit code

| Code | Nghĩa | Hành vi OS |
| --- | --- | --- |
| 0 | Thoát bình thường | Về Home, không báo lỗi |
| 1 | Lỗi app tự báo | Về Home, hiện thông báo ngắn |
| 64 | Dùng sai | Về Home, log |
| 70 | Lỗi nội bộ | Về Home, log |
| 75 | Tạm thời không sẵn sàng | Về Home, log |
| tín hiệu | Crash/kill | Về Home, hiện lỗi; không respawn |

## 7. Hợp đồng hiển thị

- Framebuffer tham chiếu: 1072×1448, 8 bpp, stride 1088 (đã quan sát trên
  candidate). Không hard-code nếu đọc được từ `FBIOGET_VSCREENINFO`.
- App mới **không tự mở `/dev/fb0`**. Service mở và truyền fd:
  - `GW_FB_FD` (mặc định 3)
  - `GW_DISPLAY_WIDTH`, `GW_DISPLAY_HEIGHT`, `GW_DISPLAY_STRIDE`,
    `GW_DISPLAY_BPP`
- App dùng `libgwdisplay` (sẽ tạo trong `os/system/` hoặc `os/ui/`) để:
  - `gw_display_acquire()`
  - `gw_display_commit(rect, hint)`
  - `gw_display_release()`
- `hint`: `AUTO`, `FAST`, `FULL`. Service quyết định mode thật, đếm ghosting,
  full refresh định kỳ và full refresh sau mỗi lần bàn giao.
- Chính sách đã đo cho shell hiện tại: DU4 cho transition, DU/A2 cho typing,
  REAGL cho cập nhật nhỏ, deferred REAGL cleanup, GC16 chỉ khi yêu cầu — xem
  `docs/technical/PW3-PAINT-POLICY.md`. `libgwdisplay` phải dùng lại policy này,
  không tạo policy thứ hai.
- Policy runtime hiện có `quality` (mặc định, một GC16 mỗi scene), `balanced`
  (DU4 + GC16 mỗi 4 scene) và `fast` (DU4 + REAGL mỗi scene). Xem
  `docs/technical/EINK-UI-RENDERING.vi.md` trước khi chọn mặc định.
- App không được đổi refresh policy toàn cục, không tự chạy full refresh liên
  tục, không animation theo FPS.
- **Ngoại lệ KOReader v1:** KOReader hiện tự mở fb và tự điều khiển refresh.
  Được phép tiếp tục như ngoại lệ có ghi rõ; adapter chỉ lo launch/exit/return,
  không cố bọc lại ioctl của KOReader. Nếu port sang `libgwdisplay` thì phải
  qua checklist riêng.
- **Hiện trạng:** chưa có app nào chạy theo đường fd/`libgwdisplay`; thư viện
  này chưa tồn tại. Đây là mục tiêu của app runtime plan, không phải tính năng
  đã có.

## 8. Hợp đồng input

- Service mở touch evdev và truyền fd `GW_TOUCH_FD` (mặc định 4) cho app mới.
- App không tự `EVIOCGRAB` thiết bị touch; không mở nút nguồn.
- Service giữ độc quyền `KEY_POWER` trong suốt phiên.
- Khi khóa: service ngừng chuyển touch cho app; app không được vẽ.
- Sau resume: service drain sự kiện cũ trước khi chuyển lại input.

## 9. Hợp đồng nguồn

- Bấm nguồn ngắn: OS khóa, tắt frontlight, hoàn tất refresh; suspend chỉ sau
  khi gate power được nghiệm thu.
- App nhận `SIGUSR1`/pause trước khi khóa; OS chờ tối đa 2 giây rồi tiếp tục.
- Không app nào được giữ wake lock hoặc đăng ký wakeup source.
- Cắm USB quản trị: khóa màn hình/tắt đèn nhưng giữ kernel thức cho console và
  chép file; không suspend trong lúc cáp còn kết nối (theo thiết kế v1).
- RTC/suspend/resume dùng các đường đã có trong `os/system/gw-power.h`; app
  không gọi trực tiếp.

## 10. Giới hạn tài nguyên

### 10.1 Cơ chế có và không có

Từ `os/devices/pw3/stock-kernel.config`:

- **Không có** `CGROUPS`, `NAMESPACES`, `SECCOMP`.
- **Không có** `EXT4`, `TUN`, `KEXEC`.
- **Có** `SQUASHFS` (chỉ zlib), `BLK_DEV_LOOP`, `FUSE` (module), `SWAP`,
  `TMPFS`.

Hệ quả: không thể giới hạn RSS bằng cgroup. Dùng:

- `RLIMIT_AS` (trần địa chỉ ảo — **không phải RSS**, phải test với KOReader),
- `RLIMIT_DATA`, `RLIMIT_NOFILE`, `RLIMIT_NPROC` nếu khả dụng,
- `nice`, `oom_score_adj`,
- watchdog đọc RSS từ `/proc/<pid>/status` và kill khi vượt trần.

### 10.2 Ngân sách đề xuất (đã có số đo một phần, chốt bằng AS-02/AS-03)

Số đo hiện có (2026-10-31):

| Hạng mục | Số đo | Nguồn |
| --- | --- | --- |
| RAM tổng | 514,724 KiB (~503 MiB) | baseline Phase 0 |
| `gw-shell` idle | RSS 656 KiB, CPU 0.10% | Phase 0 + Phase 1 |
| Reader peak | EPUB nhỏ 52 MiB, EPUB lớn 70 MiB, PDF phức tạp 33 MiB | Phase 2 active perf |
| Boot → service/shell | ~16 giây | Phase 2 |
| Boot → Home diag trả lời | ~23.1 giây | Phase 2 |
| Reader launch → ready | 7.0 s (EPUB nhỏ), 18.2 s (EPUB lớn), 6.2 s (PDF) | Phase 2 |
| Idle EPDC | 1 update/phút (đồng hồ phút) | Phase 1 |
| Idle log | 0 B/s sau P0 | Phase 1 |

| Hạng mục | Trần đề xuất | Ghi chú |
| --- | --- | --- |
| Kernel + init + network | 64 MiB | Đo sau boot, không có app |
| `gw-shell` Home/Lock | 16 MiB | Đo thực tế thấp hơn nhiều; giữ đệm LVGL |
| Reader (KOReader) | 96 MiB peak | Peak đo 70 MiB; đệm cho PDF/CJK |
| App foreground | 96 MiB mặc định, 128 MiB tối đa | Giới hạn theo profile, không theo manifest |
| Dự phòng | ≥ 48 MiB | Không được để OOM khi lật trang |

Tổng RAM khả dụng ~503 MiB. Đây vẫn là **trần mục tiêu**; chỉ được coi là
chốt sau khi chạy lại AS-02/AS-03 trên build hiện tại.

Giới hạn do OS policy quyết định theo `id` + `profile`; app không được tự đặt
trần của mình trong manifest.

## 11. Dữ liệu và cài đặt

**Hiện trạng 2026-10-31 — đã triển khai:**

- Bundle `.gwapp` (gzip tar: `manifest` + `payload/...`) + chữ ký detached
  `.gwapp.sig`, RSA-2048 PKCS#1 v1.5 SHA-256.
- Trust root `/etc/gleworks/app-keys/release.pem`; private key chỉ trên host.
- `gw-appctl verify/install/remove/list/scan`; service chỉ cho supervised shell
  gọi install/update/remove.
- Thư mục USB: `/mnt/us/gleworks/apps/`.
- Cài: verify chữ ký trước khi extract → validate từng tên tar → từ chối
  link/special file → parse manifest → kiểm arch/profile/min_os → extract vào
  version mới → kiểm entrypoint → đổi symlink `active`.
- Update giữ version cũ; remove giữ appdata; built-in `koreader` không cho gỡ.
- UI Applications/App updates đã dùng thật trên máy.

**Còn mở:**

- Data image riêng (`/var/lib/gleworks/appdata` hiện nằm trên root ext3);
  `pw3-data-image.py` mới tạo ảnh rỗng, **chưa** nghiệm thu mount/shutdown/
  rescue trên thiết bị.
- Hết chỗ và mất điện giữa lúc cài chưa được kiểm chứng.
- Chưa có catalog online; v1 vẫn chỉ cài offline từ USB.
- App cài xong **chưa có đường launch**; đây là khoảng trống lớn nhất.
- Payload hiện root `0700`, chưa phải read-only `0555`; app chạy root nên
  "read-only" chưa được enforce bằng DAC.
- Không chạy maintainer script, không ghi p1/boot gate/kernel/rescue.

Log app: `/run/gw-apps/<id>.log`, tối đa 64 KiB, không chứa mật khẩu/tên sách
nhạy cảm.

## 12. Bảo mật và ranh giới trung thực

- App là **trusted code**. UID riêng + payload read-only + DAC là ranh giới
  duy nhất; không có namespace/seccomp.
  - **Hiện trạng:** app và Reader chạy root, chưa có UID riêng; mục tiêu này
    thuộc giai đoạn runtime.
- Chỉ foreground app nhận display/touch.
  - **Hiện trạng:** chỉ Reader được foreground; app ngoài chưa launch được.
- Frontlight/mạng/power đi qua IPC có tập lệnh cố định, không phải shell text.
- IPC xác thực `SO_PEERCRED`; chỉ UID 0 hoặc shell PID đã đăng ký được gửi lệnh
  đặc quyền.
- Không endpoint exec tùy ý, không cài package từ xa.
- Root bị compromise nằm ngoài ranh giới UID; không tuyên bố chống được.

## 13. Quan sát và chẩn đoán

`gwctl status` (hoặc IPC tương đương) phải trả được:

- `session=home|launching|foreground|stopping|locked|suspending|resuming|failed`
- `app=<id>|none`, `app_pid`, `app_uid`, `app_started_ms`, `app_exit`,
  `app_reason`
- `display=on|off`, `refresh_mode`, `refresh_count`
- `light`, `battery`, `charging`, `net`
- `mem_available_kb`, `app_rss_kb`, `app_fd_count`
- `boot_id`

Không ghi mật khẩu Wi-Fi, token, tên sách nhạy cảm hoặc nội dung người dùng.

## 14. Checklist acceptance

Điều kiện chung trước mỗi nhóm:

- [ ] Rescue USB độc lập vừa được kiểm tra trước khi thử.
- [ ] Có backup root/data và hash; biết cách quay lại ảnh đang chạy.
- [ ] Không thay đổi bootloader/kernel/p1 trong suốt phiên thử.
- [ ] Pin đủ (đề xuất ≥ 50%) và có console USB.
- [ ] Ghi `boot_id` trước/sau; mọi bước dùng cùng một boot trừ khi ghi rõ.

### Trạng thái bằng chứng tại 2026-10-31

Checkbox bên dưới chỉ được tick khi chạy lại trên build hiện tại và lưu bằng
chứng. Bảng này ghi cái đã có, không thay thế lần chạy lại.

| Trạng thái | AS | Ghi chú |
| --- | --- | --- |
| Đã có bằng chứng | AS-04, AS-06, AS-07, AS-11, AS-22, AS-24, AS-25, AS-28, AS-30, AS-31, AS-32, AS-34, AS-37, AS-39, AS-40, AS-41, AS-42, AS-47, AS-48 | Power đã xác nhận bằng nút vật lý thật (lock → suspend → wake → Unlock); paint policy đo transition ~76 ms. AS-37 mới xác nhận update, rollback đầy đủ ở AS-38 |
| Một phần | AS-01, AS-02, AS-05, AS-26, AS-27, AS-29, AS-33, AS-38, AS-45, AS-46 | AS-26/27 mới đo UI, chưa đủ ghosting đọc sách và double-event sau resume; AS-29 mới test lock trong Reader, chưa test suspend khi app lạ foreground |
| Chưa có | AS-03, AS-08, AS-09, AS-10, AS-12, AS-13, AS-14, AS-15, AS-16, AS-17, AS-18, AS-19, AS-20, AS-21, AS-23, AS-35, AS-36, AS-43, AS-44 | Tập trung ở generic launch, crash/OOM/limit, mất điện và rollback |

Bằng chứng chính: `docs/evidence/pw3-app-installer-2026-10-31.md`,
`docs/evidence/pw3-power-suspend-2026-10-31.md`,
`docs/evidence/pw3-power-physical-button-2026-10-31.md`,
`docs/evidence/pw3-wifi-autoconnect-2026-10-31.md`,
`docs/evidence/PW3-PHASE1-OPTIMIZATION.md`,
`docs/evidence/PW3-PHASE2-ACTIVE-PERF.md`,
`docs/technical/PW3-PAINT-POLICY.md`.

### A. Baseline và ngân sách

- [ ] **AS-01 — Đo nền không app.** Boot → Home, không mở app; ghi
  `MemAvailable`, RSS của PID1/shell/network, `free`, fd count.
  *Bằng chứng:* `docs/evidence/app-session/as-01.json` + log.
- [ ] **AS-02 — Đo reader peak.** Mở PDF nặng nhất có sẵn, lật ≥ 20 trang; ghi
  `VmHWM`, thời gian mở, thời gian về Home.
  *Bằng chứng:* `as-02.json`.
- [ ] **AS-03 — Chốt trần.** Ghi bảng trần cuối cùng vào mục 10.2 và cấu hình
  policy; nêu rõ `RLIMIT_AS` có phá KOReader hay không.
- [ ] **AS-04 — Đo baseline suspend.** Trước khi app platform, xác nhận
  suspend/resume hiện tại đạt hay chưa; nếu chưa, ghi rõ app platform không
  được coi là shippable.
- [ ] **AS-05 — Đo thời gian.** Boot → Home, Home → app, app → Home, lock/unlock.

### B. Launch, exit, return

- [ ] **AS-06 — Launch KOReader từ Home.** Đúng app ID, đúng UID, không có
  process cũ; `foreground=koreader`; touch hoạt động.
- [ ] **AS-07 — Return to Home.** Thoát từ menu KOReader → Home, không respawn
  reader, không mất vị trí đọc đã lưu.
- [ ] **AS-08 — Launch lần hai.** Mở lại reader sau khi về Home; không rò fd,
  không chồng process.
- [ ] **AS-09 — Chặn launch thứ hai.** Khi đang foreground, yêu cầu launch app
  khác bị từ chối với lỗi rõ ràng; app hiện tại không bị dừng.
- [ ] **AS-10 — App ID sai.** Launch ID không tồn tại bị từ chối; Home không
  đổi trạng thái.
- [ ] **AS-11 — Exit code 0.** Không có thông báo lỗi; full refresh; input trả
  lại Home.
- [ ] **AS-12 — Exit không mong đợi.** App tự exit code 1 → Home có thông báo
  ngắn, log đúng, không respawn.

### C. Lỗi, treo, OOM

- [ ] **AS-13 — Crash SIGSEGV.** App mẫu crash; về Home ≤ 3 giây; không boot
  loop; log có signal.
- [ ] **AS-14 — Treo lúc launch.** App mẫu không vẽ/không phản hồi; bị kill sau
  timeout; Home trở lại bình thường.
- [ ] **AS-15 — Treo lúc exit.** App bỏ qua `SIGTERM`; bị `SIGKILL` sau 3 giây;
  fd được thu hồi.
- [ ] **AS-16 — OOM do app.** App cấp phát vượt trần; bị kill; reader/PID1 vẫn
  sống; Home hiện lỗi.
- [ ] **AS-17 — OOM toàn hệ thống.** Tạo áp lực bộ nhớ có kiểm soát; xác nhận
  `oom_score_adj` bảo vệ PID1/reader; ghi lại ai bị chọn.
- [ ] **AS-18 — Rò fd.** Chu kỳ launch/exit 100 lần; fd count của service và
  shell không tăng đơn điệu; không rò socket.
- [ ] **AS-19 — App không thể làm sập OS.** Crash lặp 20 lần; boot ID không đổi;
  Home vẫn mở app khác được.

### D. Tài nguyên và dữ liệu

- [ ] **AS-20 — Trần bộ nhớ có hiệu lực.** App mẫu vượt 96 MiB bị chặn/kill
  đúng cách; thông báo không gây hiểu nhầm là lỗi OS.
- [ ] **AS-21 — Trần fd/process.** App mở quá giới hạn bị lỗi `EMFILE`/`EAGAIN`
  và thoát sạch, không treo Home.
- [ ] **AS-22 — Data tách biệt.** Ghi dữ liệu app; nâng version/gỡ app; data
  còn nguyên; xóa data chỉ khi xác nhận riêng.
- [ ] **AS-23 — Payload read-only.** App thử ghi vào payload thất bại; không
  ảnh hưởng chạy.
- [ ] **AS-24 — Không data trên FAT.** Kiểm tra không có app/data/executable
  mới trên p4/FAT; không mount FAT khi root đang dùng.

### E. Hiển thị, input, power

- [ ] **AS-25 — Bàn giao màn hình.** Home → app → Home: không còn pixel cũ,
  không nhấp nháy bất thường, full refresh sau bàn giao.
- [ ] **AS-26 — Refresh policy.** Partial cho thay đổi nhỏ, full định kỳ; đếm
  ghosting qua 30 lần lật trang; policy không do app tự đổi.
- [ ] **AS-27 — Input độc quyền.** App foreground nhận touch; Home không nhận
  cùng lúc; không có double event sau resume.
- [ ] **AS-28 — Nút nguồn thuộc OS.** Trong app, bấm ngắn → khóa/tắt đèn theo
  policy; app không thoát ngoài ý muốn; giữ 40 giây vẫn là reset phần cứng.
- [ ] **AS-29 — Suspend trong app.** `SIGUSR1` → lưu → suspend → wake → vẽ lại;
  lặp ≥ 5 vòng; không wake loop.
- [ ] **AS-30 — USB quản trị.** Cắm cáp khi đang foreground: khóa màn hình,
  giữ kernel thức, console chép file được; rút cáp chuyển lại policy.
- [ ] **AS-31 — Không nhiễu nền.** Sau 10 phút ở Home và 10 phút trong app,
  CPU idle không tăng bất thường; không app nào đánh thức máy.

### F. Đóng gói và cài đặt (chỉ sau khi A–E đạt)

- [ ] **AS-32 — Cài app mẫu offline.** Staging → verify → extract → entrypoint
  → active; Home thấy app; chạy được.
- [ ] **AS-33 — Từ chối sai arch/profile.** Cài gói sai bị từ chối trước khi
  đổi active; app cũ vẫn chạy.
- [ ] **AS-34 — Từ chối sai chữ ký/hash.** Bị từ chối; log rõ; không ghi đè.
- [ ] **AS-35 — Hết chỗ.** Gói vượt dung lượng bị từ chối trước khi ghi; không
  làm hỏng root.
- [ ] **AS-36 — Mất điện khi cài.** Cắt điện mô phỏng/khôi phục; boot lại vẫn
  vào app cũ hoặc Home, không boot loop.
- [ ] **AS-37 — Update.** Cài version mới; data giữ; version cũ còn để rollback.
- [ ] **AS-38 — Gỡ và rollback.** Gỡ giữ data; rollback về version trước chạy
  được; không đụng p1/boot gate.

### G. Hồi quy và phục hồi

- [ ] **AS-39 — Reboot sau mọi thay đổi.** Boot vào Home, app đã cài còn đó,
  reader mở được, boot ID mới hợp lệ.
- [ ] **AS-40 — Rescue sau thử nghiệm.** Chọn rescue, khôi phục ảnh, quay lại
  native; hash khớp.
- [ ] **AS-41 — Không hồi quy đọc sách.** Mở EPUB/PDF, lật trang, về Home, mở
  lại đúng vị trí; người dùng xác nhận.
- [ ] **AS-42 — Không hồi quy Wi-Fi/USB.** Các đường đã đạt trước đó vẫn đạt
  hoặc được ghi rõ là chưa thử.
- [ ] **AS-43 — Boot Amazon fallback.** Chỉ thử nếu đã có gate riêng; không
  gộp vào acceptance app platform.
- [ ] **AS-44 — Dữ liệu người dùng.** Không mất sách/cấu hình sau crash, update,
  rollback hoặc gỡ app.

### H. Vận hành và bảo mật

- [ ] **AS-45 — IPC xác thực.** Client lạ/không đúng UID bị từ chối; không
  command injection; dòng quá dài bị từ chối.
- [ ] **AS-46 — Không lộ bí mật.** Log không chứa mật khẩu Wi-Fi, token, tên
  sách nhạy cảm.
- [ ] **AS-47 — Không exec tùy ý.** Không có đường từ app/Home/mạng tới shell
  hoặc cài package ngoài luồng đã duyệt.
- [ ] **AS-48 — Trạng thái đúng.** `gwctl status` phản ánh đúng session/app/
  power; giá trị không đo được ghi `unknown`, không đoán.

## 15. Bằng chứng và cách ghi

Mỗi mục `AS-*` khi đạt phải có:

- ngày giờ UTC, `boot_id`, model/firmware, hash ảnh đang chạy;
- lệnh đã chạy và output thô (hoặc file log);
- kết quả trước/sau (RSS, fd, thời gian, ảnh framebuffer nếu cần);
- người xác nhận nếu là trải nghiệm người dùng;
- giới hạn của phép thử (ví dụ: chỉ một lần, chưa thử mất điện).

Lưu tại `docs/evidence/app-session/` (public, đã lọc bí mật) và
`work/private/` cho dữ liệu nhạy cảm. Không sửa lịch sử để đánh dấu đạt.

## 16. Không được coi là đạt

- Ảnh framebuffer PNG thay cho trải nghiệm thật.
- Tiến trình còn sống sau 90 giây.
- Boot thẳng vào KOReader.
- Build host hoặc syntax check.
- "Người dùng thấy có vẻ ổn" mà không có checklist.
- Chạy được một lần, không lặp lại.
- Có rescue nhưng chưa thử sau thay đổi.

## 17. Điều kiện dừng khẩn cấp

Dừng thử, vào rescue, không cố sửa tại chỗ nếu:

- boot loop hoặc Home không mở được;
- màn hình kẹt, không refresh được sau 2 lần thử;
- fd/RSS tăng đơn điệu qua nhiều chu kỳ;
- nút nguồn không còn phản hồi;
- USB console mất và không tự trở lại;
- có dấu hiệu ghi vào p1/boot gate ngoài ý muốn.

## 18. Quyết định còn mở

| ID | Câu hỏi | Trạng thái 2026-10-31 |
| --- | --- | --- |
| D1 | Logic phiên nằm trong `gw-system` hay module riêng? | Giữ trong `gw-system`; hiện chỉ có nhánh Reader cố định |
| D2 | KOReader chạy UID riêng hay root? | **Mở**: hiện root; ưu tiên UID riêng cho app mới, KOReader có thể là ngoại lệ |
| D3 | Data app nằm trên root ext3 hay data image riêng? | **Mở, ưu tiên cao**: tool tạo ảnh đã có nhưng chưa nghiệm thu; root ext3 hiện gánh appdata |
| D4 | App bundle dùng `.apk` hay squashfs? | **Đã chốt:** `.gwapp` + `.sig` (gzip tar), đã chạy thật |
| D5 | Có cần broker hiển thị không? | Không trong v1; Reader vẫn tự điều khiển fb |
| D6 | `RLIMIT_AS` có phá KOReader? | **Chưa thử**; peak đo 70 MiB, đặt trần 96 MiB trước |

## 19. Thứ tự triển khai sau cập nhật 2026-10-31

Lớp package/UI/power cơ bản đã có. Thứ tự còn lại:

1. **Chốt lưu trữ (D3) và đo lại baseline** — AS-01…AS-05 trên build hiện tại;
   xác nhận dung lượng root/appdata trước khi mở launch tổng quát.
2. **Generic launch tối thiểu** — chỉ một app `gwtest` (không phải Reader):
   service launch theo `id`, entrypoint cố định, timeout, exit → Home;
   AS-08…AS-12.
3. **UID + giới hạn + fd handoff** — app chạy UID riêng, `RLIMIT_*`, watchdog,
   nhận fb/touch fd qua `libgwdisplay`; AS-20…AS-27.
4. **Lỗi và tài nguyên** — crash, treo, OOM, fd leak, boot loop; AS-13…AS-19.
5. **Power trong app** — pause/lock/suspend/resume có giới hạn chờ; AS-28,
   AS-29.
6. **Đóng gói nâng cao** — hết chỗ, mất điện, rollback; AS-35, AS-36, AS-38.
7. **UI và hồi quy** — nút Open cho app đã cài, reboot/rescue/reader regression;
   AS-39…AS-48.
8. Chỉ sau đó mới thêm app thứ ba, catalog hoặc A/B OS update.
