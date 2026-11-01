# Prior art: Linux nhẹ cho thiết bị E-ink

- Ngày khảo sát: 2026-10-31.
- Phương pháp: tìm kiếm web + đọc README/tài liệu công khai của từng dự án.
- Phạm vi: **chưa clone và chưa đọc source** trừ khi ghi rõ; chưa chạy thử trên
  PW3. Đây là ghi chú tham chiếu, không phải bằng chứng.
- Mục đích: tránh làm lại phần đã có người làm; chọn hướng kiến trúc và lớp
  trừu tượng phù hợp cho GleWorks OS (Alpine, giữ kernel/bootloader gốc, vẽ
  trực tiếp framebuffer/EPDC, LVGL).

## Kết luận ngắn

Đã có rất nhiều người làm "Linux nhẹ cho E-ink", nhưng gần như **không ai viết
một distro desktop từ đầu**. Các dự án thực tế chia thành 5 nhóm:

1. Dùng OS nhúng sẵn của hãng (Kindle OS, Kobo Nickel, reMarkable OS,
   PocketBook).
2. Chroot một distro ARM lên firmware gốc (Alpine/Debian/Arch trên Kindle).
3. Chỉ thay UI đọc sách, chạy trên framebuffer có sẵn (KOReader, Plato, FBInk).
4. Thay toàn bộ rootfs/OS (InkBox trên Kobo, postmarketOS, Arch/Debian trên
   PineNote).
5. Tự làm phần cứng + controller EPD (Modos Glider/Caster, TRMNL).

Điểm chung quyết định trải nghiệm: **framebuffer + ioctl của driver E-ink**,
không GPU, RAM ít, và **waveform/refresh policy** quan trọng hơn việc chọn
distro nào.

## Bảng prior art

| Nhóm | Dự án tiêu biểu | Cách làm | Liên quan tới PW3 |
| --- | --- | --- | --- |
| OS nhúng của hãng | Kindle OS, Kobo Nickel, reMarkable OS ("Codex"), PocketBook | Rootfs Linux + app đọc sách độc quyền; reMarkable build bằng Yocto, UI Xochitl đóng | Nền tảng hiện tại; PW3 trong dự án này dùng kernel `3.0.35-lab126`, GUI X11 |
| Chroot distro trên firmware gốc | `alpine_kindle`, `kindle-linux-chroot`, Linux Deploy cho KUAL | Không đụng kernel/OS gốc; mount file `.ext3` rồi `chroot`, dùng chung `/dev`, `/proc`, `/sys` | Hướng an toàn nhất, đã có người chạy trên PW3 |
| Thay UI đọc sách | KOReader, Plato, FBInk, kindle-dash | Chạy trên framebuffer có sẵn, chỉ vẽ nội dung | Tham chiếu event loop, refresh, đóng gói |
| Thay toàn bộ OS | InkBox OS (Kobo), postmarketOS (Kobo Clara, PineNote), Arch/Debian trên PineNote, custom distro cho reMarkable | Rootfs riêng + init riêng, thường vẫn dùng kernel vendor hoặc mainline đã port | Khó hơn trên Kindle vì bootloader/recovery |
| Phần cứng/controller riêng | Modos Glider/Caster, TRMNL | FPGA/controller riêng để giảm latency, firmware cực nhỏ | Tham chiếu giới hạn vật lý của E-ink |

## Từng dự án

### reMarkable OS

- Nền: Yocto; UI `xochitl` là thành phần đóng, kernel patch được công bố.
- `rm2fb` là **framebuffer server + client shim `LD_PRELOAD`**, vì rM2 không có
  fb driver "chuẩn" cho app thường; màn hình do app UI điều khiển.
- `libremarkable` (Rust) cung cấp truy cập framebuffer mức thấp, ví dụ cho app
  tự vẽ.
- Bài học: khi driver không cho app ngoài vẽ trực tiếp, mô hình
  "server giữ fb + shim cho client" là cách đã được dùng thật.

### PineNote

- RK3566, 4 GB RAM, panel 1404×1872; bản Community Edition ship Debian.
- Cộng đồng chạy Arch + Sway (`hrdl-github/pinenote-arch`) và postmarketOS.
- Phải reverse-engineer driver EBC của RK3566 và **khôi phục file waveform**
  (`waveform_firmware_recovered`).
- Bài học: ngay cả thiết bị mở vẫn cần waveform đúng; Sway/Wayland chỉ khả thi
  khi SoC đủ mạnh và driver đã ổn.

### Kobo

- Firmware gốc là Linux; **InkBox OS** là OS thay thế hoàn chỉnh (Qt), chạy trên
  Glo/Touch/Mini/Nia.
- PostmarketOS Kobo Clara HD dùng kernel mainline, framebuffer `mxc_epdc` hoạt
  động, thậm chí chạy được X server.
- Bài học: Kobo có thẻ SD/recovery dễ hơn Kindle, nên thay rootfs thực tế hơn.

### Kindle

- Firmware gốc đã là Linux; kernel **thay đổi theo model/firmware**: PW3 trong dự
  án này dùng `3.0.35-lab126`, một số model khác (ví dụ Kindle 10th gen) dùng
  `4.1.15`. GUI chạy X11.
- `alpine_kindle` (schuhumi) và `kindle-linux-chroot` (Sqrilizz): image
  Debian/Alpine/Arch/Void ARMv7 nằm trong một file `.ext3` trên `/mnt/us`,
  KUAL + kterm mount rồi chroot. Không dual-boot, không flash.
- Giới hạn đã được ghi nhận: kernel cố định, ~237 MB RAM khả dụng (trên model
  được test), không systemd, không GPU, eMMC chậm, không nạp module kernel mới.
- Rủi ro: mount `/mnt/us` trong khi USB mass-storage đang bật có thể phá
  partition.
- Bài học: với Kindle, "OS nhẹ" thực tế là **userspace mới trên kernel gốc**;
  chroot là đường ít rủi ro nhất và giữ được OS gốc làm fallback.

### App/dashboard

- KOReader/Plato: event loop, xử lý input, partial refresh, đóng gói app.
- FBInk: lớp vẽ text/ảnh/progress bar trên nhiều loại fb E-ink.
- kindle-dash/kindle-dashboard: mô hình "tải ảnh rồi vẽ" bằng `eips`, hữu ích
  cho dashboard tĩnh nhưng không phải OS.

## Cách họ giải quyết từng lớp

### 1. Hiển thị

- Màn E-ink hiện ra như `/dev/fb0` (Kindle: `mxc_epdc_fb`, Kobo: `mxc_epdc`,
  PineNote: `EBC`). App mmap framebuffer rồi gọi ioctl để commit.
- Kindle có ioctl riêng:
  - `FBIO_EINK_UPDATE_DISPLAY` = `0x46db`; tham số `0` = refresh nhanh có
    ghosting, `1` = flash đen/trắng đầy đủ.
  - `FBIO_EINK_CLEAR_SCREEN` = `0x46e1`.
  - `eips` chỉ là công cụ gọi các ioctl này.
- **FBInk** (NiLuJe) trừu tượng hoá fbdev, i.MX EPDC, MTK (Kindle), sunxi
  (Kobo); hỗ trợ 4/8/16/24/32 bpp, xoay, vẽ text/ảnh/progress bar. Đây là lớp
  được KOReader và nhiều dự án dùng lại.
- **Waveform** là tài sản đóng của nhà sản xuất. Không có waveform đúng thì
  không thể tự do làm refresh nhanh/grayscale đẹp. Vì vậy đa số dự án dùng
  driver vendor có sẵn và chỉ thay userspace; Glider/Caster là ngoại lệ vì làm
  cả controller.
- Chiến lược refresh thực dụng: cập nhật vùng nhỏ (partial), thỉnh thoảng full
  refresh để xoá ghost, tránh animation, ưu tiên nội dung tĩnh.

### 2. Input và nguồn

- Touch/pen đọc qua **evdev** (`/dev/input/event*`), không cần X/Wayland.
- E-ink giữ ảnh khi mất điện → cho phép **suspend sâu** gần như toàn thời gian,
  đánh thức bằng RTC/nút bấm rồi vẽ lại một lần. Đây là mô hình pin của
  reader/dashboard.
- Đèn nền (frontlight) đi qua sysfs/LIPC tuỳ model; cần bảng map theo thiết bị.

### 3. Boot / rootfs / UI stack

- **Kindle**: bootloader khoá, không nạp kernel tuỳ ý → hướng khả thi là
  userspace OS trên kernel gốc. Chroot an toàn nhất: OS gốc còn nguyên, xoá file
  `.ext3` là về như cũ. Nhược điểm: không systemd, kernel cũ, RAM chia sẻ với
  KindleOS.
- **Kobo/PineNote/reMarkable**: có thể thay rootfs thật, thậm chí dual-boot qua
  SD; PineNote có UART nên khôi phục dễ hơn.
- **Build system**: reMarkable dùng Yocto; PineNote dùng Debian/Arch rootfs +
  script; InkBox và nhiều dự án nhỏ dùng Buildroot hoặc rootfs distro có sẵn.
  Với 256–512 MB RAM, Alpine/musl hoặc Debian minimal thực tế hơn desktop đầy
  đủ.
- **UI stack**:
  - X11: Kindle stock, Kobo mainline.
  - Wayland/Sway: PineNote.
  - Qt: InkBox.
  - Vẽ trực tiếp framebuffer: KOReader, Plato, rm2fb, libremarkable.
  - Vẽ trực tiếp cho độ trễ thấp nhất và kiểm soát refresh tốt nhất.

## Bài học / anti-pattern

1. **Đừng bắt đầu bằng compositor/desktop environment.** Các dự án thành công
   thường là app toàn màn hình vẽ trực tiếp fb; X/Wayland chỉ dùng khi phần cứng
   đủ mạnh hoặc đã có sẵn.
2. **Refresh policy quan trọng hơn tối ưu distro.** Cùng máy, trải nghiệm khác
   nhau hoàn toàn nhờ partial update + full refresh định kỳ + tránh repaint thừa.
3. **Giữ đường lui.** Chroot/không flash là lựa chọn của `alpine_kindle` và
   `kindle-linux-chroot` vì Kindle không có recovery tiện như Kobo (thẻ SD) hay
   PineNote (UART).
4. **Đừng giả định có systemd/GPU/kernel mới.** Kernel 4.1.15 thiếu nhiều tính
   năng hiện đại; service phải tự quản.
5. **RAM là ràng buộc thật.** Chromium/Electron/Java UI gần như không phù hợp;
   app C/Qt nhẹ hoặc Python tối giản mới ổn.
6. **Tài liệu cộng đồng quan trọng hơn code.** MobileRead, Kindle Modding Wiki
   và Discord KindleModding là nơi có thông tin jailbreak/ioctl/model map.

## Hàm ý cho GleWorks OS trên PW3

Hai hướng thực tế:

- **Hướng an toàn:** giữ kernel/firmware gốc, xây userspace riêng (init nhẹ +
  app toàn màn hình) chạy trực tiếp trên `/dev/fb0`, dùng FBInk hoặc ioctl
  `FBIO_EINK_*`; đóng gói thành KUAL extension/rootfs image để bật/tắt. Đây là
  mô hình `alpine_kindle`/`kindle-linux-chroot`/KOReader, né được rủi ro
  bootloader.
- **Hướng "OS thật":** chỉ nên làm nếu chấp nhận tự lo boot/recovery; trên
  Kindle gần như bắt buộc dựa vào kernel stock, nên thực chất vẫn là distro
  trong chroot hoặc init riêng chạy trên kernel stock, không phải thay kernel.

Kiến trúc nên đọc theo thứ tự:

| Mục đích | Tham chiếu |
| --- | --- |
| Framebuffer server/client shim khi driver không cho app vẽ trực tiếp | `ddvk/remarkable2-framebuffer` |
| Lớp vẽ/refresh đã kiểm chứng nhiều máy | `NiLuJe/FBInk` |
| Event loop, input, partial refresh, đóng gói app | KOReader, Plato |
| Tổ chức một OS thay thế hoàn chỉnh nhưng nhẹ | InkBox OS |
| Đồ hoạ hiện đại trên E-ink (nếu chuyển thiết bị mở hơn) | PineNote Arch + Sway |
| Giới hạn vật lý của refresh/waveform | Modos Glider/Caster |

## Nguồn

Sơ cấp (tài liệu/README của dự án):

- reMarkable software stack (Yocto):
  <https://developer.remarkable.com/documentation/software-stack>
- rm2fb: <https://github.com/ddvk/remarkable2-framebuffer>
- libremarkable: <https://github.com/canselcik/libremarkable>
- PineNote releases: <https://www.pine64.org/documentation/PineNote/Releases>
- PineNote Arch + Sway: <https://github.com/hrdl-github/pinenote-arch>
- postmarketOS Kobo Clara HD:
  <https://gitlab.com/postmarketOS/pmaports/-/merge_requests/2334>
- InkBox OS: <https://github.com/Szybet/InkBox>
- `alpine_kindle`: <https://github.com/schuhumi/alpine_kindle>
- `kindle-linux-chroot`: <https://github.com/Sqrilizz/kindle-linux-chroot>
- FBInk: <https://github.com/NiLuJe/FBInk>
- Kindle Modding Wiki: <https://kindlemodding.org/>
- KOReader porting/framebuffer:
  <https://koreader.rocks/doc/topics/Porting.md.html>
- Modos Glider: <https://github.com/Modos-Labs/Glider>

Thứ cấp (bài viết, phân tích):

- Phân tích ioctl framebuffer Kindle:
  <https://www.sixfoisneuf.fr/posts/kindle-hacking-deeper-dive-internals>
- Kindle chạy Linux (XDA):
  <https://www.xda-developers.com/my-unsupported-kindle-runs-linux-now-and-its-more-useful-than-when-it-was-new>
- Thảo luận latency/waveform E-ink (Hacker News):
  <https://news.ycombinator.com/item?id=28191067>
- E-ink tablet làm monitor cho Linux:
  <https://tisankan.dev/e-ink-linux-monitor>

## Đối chiếu với cách làm của GleWorks OS

Không có dự án công khai nào làm đúng tổ hợp này, nhưng từng thành phần đều đã
có tiền lệ. Đánh giá trung thực:

| Thành phần | Mức độ mới | Nhận xét |
| --- | --- | --- |
| Alpine trên Kindle | Đã có | `alpine_kindle`, `kindle-linux-chroot` chạy trong chroot |
| Alpine là **global PID1**, không chroot | Hiếm | Khác biệt thật; khó hơn vì phải tự lo init, service, handoff, recovery |
| Giữ U-Boot + kernel stock, thay rootfs | Hiếm | Đa số dự án chọn chroot để né rủi ro; đây là trade-off có chủ đích |
| Bỏ X11/Wayland, LVGL vẽ trực tiếp fb/EPDC | Ít phổ biến | Cùng tinh thần KOReader/Plato/InkBox (Qt), khác Kindle stock (X11) |
| Boot gate + USB rescue độc lập rootfs | Hiếm | Đây là phần đáng giá nhất về mặt kỹ thuật |
| Chỉ hỗ trợ một model/firmware | Khác biệt | Đánh đổi phạm vi lấy độ sâu; hợp lý cho V1 |

Vì sao ít người làm giống vậy: Kindle khoá bootloader và recovery khó; thị
trường nhỏ; công sức lớn; và phần lớn người dùng chỉ cần đọc sách nên
chroot/KOReader là đủ. Đó là lý do **kinh tế**, không phải bằng chứng cách làm
sai.

Các điều kiện để cách làm này không trở thành sai lầm:

1. Không hy sinh đường cứu hộ để đổi lấy tiến độ boot.
2. Không cố làm OS tổng quát cho nhiều model trước khi PW3 ổn định.
3. Không tự viết driver panel/waveform; dùng driver vendor và waveform có sẵn.
4. Không đuổi theo desktop/compositor; giữ refresh policy là ưu tiên hạng nhất.
5. Đo pin/suspend thật, không suy ra từ "màn hình tĩnh".
6. Định nghĩa thành công bằng gate (recovery, driver, suspend, đọc sách), không
   bằng việc "khác người".

## Việc cần kiểm chứng lại trước khi dựa vào

- Chưa đọc source của `InkBox`, `kindle-linux-chroot`, `alpine_kindle`; các
  nhận định về kiến trúc dựa trên README.
- Chưa xác nhận ioctl/offset cụ thể trên firmware PW3 `5.16.2.1.1`; các giá trị
  `0x46db`/`0x46e1` lấy từ bài phân tích firmware Kindle khác.
- Chưa xác nhận thời điểm/phiên bản mới nhất của từng dự án; kiểm tra lại
  commit gần nhất trước khi dùng.
- Các link có thể thay đổi; ghi lại ngày truy cập nếu trích dẫn ra ngoài.
