# Nhật ký kỹ thuật: PW3 từ Amazon userspace sang Alpine native

## Phạm vi và kết luận

Mục tiêu được chốt: giữ bootloader, boot Alpine làm OS mặc định và dùng e-ink
để đọc sách. Chỉ cần cáp USB thường; không dựa vào mở máy/UART để cứu thiết bị.
Không có yêu cầu giữ Amazon OS hoạt động. Tuy nhiên không xóa p1 khi đường
phục hồi cho hỏng p1/kernel chưa được chứng minh. Tài liệu này mô tả **PW3 đã
kiểm chứng**, không suy rộng sang model/firmware khác.

Mốc hệ điều hành native đã đạt: static bootstrap làm entrypoint `/sbin/init`,
Alpine BusyBox là global PID1 sau pivot_root, root riêng ghi được, không chạy
Amazon init/framework. Mốc reader: FBInk và KOReader dựng trực tiếp framebuffer,
cyttsp4 đọc cảm ứng; người dùng xác nhận mở tài liệu và chuyển trang được.
Bootloader và kernel gốc vẫn là nền phần cứng, không tự động đồng nghĩa còn có
Amazon quản lý userspace. File stock được giữ để rollback, chưa bị xóa sạch.

## Thiết bị và dữ kiện quyết định

| Thành phần | Dữ kiện trên máy | Ý nghĩa |
|---|---|---|
| Model | Kindle Paperwhite 3, Wario/Muscat | Dùng đúng board và driver PW3 |
| CPU/RAM | i.MX6SL ARMv7, 512 MiB | Dùng ARMv7 Alpine; không chọn x86/aarch64 |
| Firmware gốc | 5.16.2.1.1 | Dùng nguồn/module/kernel ABI của đúng bản |
| Kernel giữ lại | 3.0.35-lab126 | Driver màn hình/touch/USB đã có, không cần port mainline để thay userspace |
| Stock config | KEXEC, USE_OF, DEVTMPFS, TUN tắt | Không giả định có kexec, device tree, tự tạo `/dev`, hoặc Tailscale kernel TUN |
| p1 | start 65536 sector, 921600 sector, 471859200 byte | Sao lưu đúng toàn bộ root gốc trước thay init |
| User storage | p4, FAT bắt đầu cách đầu p4 8192 byte | Loop FAT đúng offset; USB cứu hộ xuất p4, không xuất toàn eMMC |
| E-ink | `mxc_epdc_fb`, 1072×1448, 8 bpp, stride 1088 | Chụp/render đúng kích thước và bỏ padding mỗi hàng |
| Cảm ứng | cyttsp4_i2c + core + mt_b → `cyttsp4_mt`, event1 | Nạp đúng ba module; không nạp firmware loader để thử bừa |
| Nguồn/pin | max77696 battery/backlight trong sysfs | Reader đọc pin và điều khiển frontlight trực tiếp |

Bằng chứng gốc ở `ours/investigation/`, `private-captures/`, các bản review và
`EXECUTION.md`. Capture có thể chứa định danh thiết bị nên giữ riêng. Không đưa
số serial, khóa Wi-Fi, SSH key hoặc p1 backup vào gói source phát hành.

## Các hướng nghiên cứu và lý do chọn đường hiện tại

1. **Upstream Linux 6.18 / barebox:** đã lấy nguồn công khai và ghim commit,
   nhưng không có board target PW3 hoàn chỉnh hoặc bằng chứng driver panel,
   touch, power, Wi-Fi tương đương stock. Clone/build host không chứng minh
   boot được. Các tag annotated trả về tag object là bình thường; lưu commit
   resolved, không suy ra hỗ trợ phần cứng từ việc download thành công.
2. **U-Boot/Fastboot giữ lại:** mã Lab126 `fastboot.c` cho thấy lệnh `boot`
   chạy `run_command(getenv("bootcmd"), 0)`, không phải Android-style boot một
   ảnh RAM vừa download. Không ghi `setvar`/IDME hoặc dùng nó làm thử boot RAM.
   Thay bootloader không cần thiết cho thay userspace.
3. **Kexec/native handoff:** stock không bật CONFIG_KEXEC. Có các module/probe
   RAM, load-only, chuỗi trang và mã chuyển ARM đã nghiên cứu riêng. Dừng DMA,
   phần cứng, initramfs và USB hồi phục chưa thể suy từ compile hoặc mô phỏng.
   Đường này được lưu làm nghiên cứu; không phải cơ chế boot của bản hiện tại.
4. **Ponder/OpenID:** truy cập nguồn trực tiếp bị chuyển đăng nhập; không dựa
   vào nguồn chưa đọc hoặc yêu cầu tài khoản để triển khai. Không suy diễn
   đăng nhập là bằng chứng có phí. Bản hiện tại không phụ thuộc Ponder.
5. **schuhumi/alpine_kindle:** nguồn tham khảo thành công trên PW3 nhưng là
   chroot với Amazon kernel/userspace vẫn hoạt động. Dùng làm dữ kiện ABI/UI,
   không gọi nó là boot native. Cảnh báo cùng ghi FAT qua USB là đúng và được
   giải quyết bằng tách độc quyền Alpine root và USB storage.
6. **PaperOS:** tham khảo mẫu bootstrap/pivot/one-shot; thích nghi riêng với
   kernel3.0.35, không devtmpfs, offset FAT8192 và cơ chế guardian của PW3.
7. **Đường chọn:** giữ kernel/bootloader đã hỗ trợ thiết bị, thay entrypoint
   userspace bằng bootstrap static + Alpine root trong một file ext3. Cách này
   đạt OS native độc lập với Amazon services và có phục hồi p4 qua USB thường.

Nguồn/commit nằm trong `ALPINE-KINDLE-REFERENCE.md`, `PAPEROS-REFERENCE.md`,
`UBOOT-REVIEW.md`, `KEXEC-REVIEW.md`, `KERNEL-REVIEW.md`, `BAREBOX-REVIEW.md` và
các file `.resolved-commit`. Hash local ghi provenance, không thay thế chữ ký
upstream hay bằng chứng tương thích phần cứng.

## Chuỗi boot cuối cùng

```text
Bootloader gốc → kernel + initramfs gốc → p1 readonly: static /sbin/init
  → mount p4/FAT đúng offset, đọc boot.enabled
  → boot.pending chưa có + ext3 sạch: ghi pending, bật guardian90s
  → mount Alpine readonly, move FAT mount, pivot_root, detach p1
  → Alpine BusyBox globalPID1 → proc/sys/dev/tmpfs + USB serial
  → hủy guardian, remount Alpine RW; pending vẫn giữ khi chạy
  → KOReader trực tiếp /dev/fb0 + /dev/input/event1

Ảnh thiếu/hỏng/dirty hoặc pending còn từ boot trước
  → đóng root/loop/FAT trước khi xuất storage
  → USB cứu hộ riêng 0525:a4a5 bcd0200
  → host sửa/thay ảnh, xử lý pending, sync + unmount + eject
  → bootstrap reboot → Alpine
```

`boot.once` là token one-shot cho trial; `--trial` không rơi sang writable
persistent mode khi thiếu request. `boot.enabled` là token persistent. Nội
dung đúng `gleworks-alpine-v1\n`; token sai không được dùng để chọn root.
`boot.pending` là bằng chứng cần phục hồi sau reset, không phải bằng chứng OS
sống. Nonce serial mới, `/proc/1/exe`, mounts và boot ID mới mới chứng minh boot.

## Các phát hiện khó và sửa đổi quan trọng

### USB trên PW3

Lần bind gadget đầu tiên có thể dừng ở low-power dù cáp đã cắm. Điều tra
B-session, `/proc/asession`, dynamic debug và chuỗi resume cho thấy cần helper
mô phỏng trạng thái device-cable trước khi bind cứu hộ. Helper kiểm tra connected
và USB-host mode, dùng write(2) thay vì dựa vào buffered stdio trên procfs.
Alpine serial dùng chuỗi bind gadget trống rồi đổi sang g_serial đã thử thật.

ID USB phải tách bạch: `1949:0004` là stock; `0525:a4a7` là native serial;
`0525:a4a6` là handshake trống; `0525:a4a5` là cứu hộ độc lập. Không chấp nhận
việc thoáng thấy USB stock sau reset làm proof cứu hộ Alpine.

### Deadline và host ghi filesystem

Bản cứu hộ đầu có reset90s không đủ an toàn cho host ghi lại ảnh. Tính thời gian
từ enumeration không chặn được I/O kéo dài và tái sử dụng tên sdX. Bản cũ được
đánh dấu `rescue-host-repair-NOT-APPROVED.py`, không dùng làm lệnh hiện tại.

Cơ chế sửa: khi guardian còn chạy, chỉ bind gadget **không có backing file**.
Chỉ sau host handshake, kiểm tra còn ít nhất15s, hủy guardian và kiểm tra state0,
tháo gadget trống, mới xuất p4 bằng ID/bcd0200. Không có reset tự động trong
cửa sổ host có thể ghi. SCSI eject đóng backing; đợi2s rồi reboot. Host kiểm
tra lại USB identity, devnum và block instance trước mutation/eject.

### Writable root và boot lỗi

Chỉ kiểm tra file init có X_OK chưa đủ: thiếu interpreter vẫn exec lỗi. Nếu
persistent token còn nguyên, máy có thể lặp reboot. Tương tự, chương trình
console lỗi sau early setup có thể làm máy chạy nhưng không có đường quản trị.

Sửa bằng pending độc quyền trước exec; pending giữ suốt uptime và chỉ xóa sau
unmount root thành công. Reset giữa chừng chọn cứu hộ. Normal startup có
reset90s cho giai đoạn đầu; chỉ remount RW sau hủy deadline. Mount ext3 readonly
vẫn có thể replay journal nếu loop writable, nên kiểm tra superblock clean và
không needs-recovery **trước mount**. Ảnh dirty chuyển sang USB sửa offline.

### Đóng nested ext3/FAT đúng thứ tự

`sync; reboot -f` không đủ để đóng root RW trong file trên FAT. BusyBox restart
exec helper static từ `/run` tmpfs làm PID1, dừng writer, move FAT/dev/proc,
unmount ext3, ghi journal xác nhận, xóa pending khi phù hợp, unmount FAT rồi
reboot. Nếu unmount lỗi, dừng để cứu, không ép reset filesystem RW.

Mô phỏng tìm thấy EBUSY do descriptor console được kế thừa trỏ vào inode device
trên ảnh Alpine, nằm dưới `/dev` tmpfs mới. Đóng mọi descriptor cũ và mở lại
console sau pivot sang RAM giải phóng tham chiếu. Sau đó cả root và FAT tháo
sạch được; ca normal/early-failure/exec-failure được kiểm tra riêng.

### Màn hình cây không phải kernel treo

Sau native boot, host gửi nonce mới vẫn nhận Alpine PID1 và uptime. E-ink giữ
ảnh khi không có chương trình refresh. `/proc/fb` có driver nhưng Alpine dùng
/dev tmpfs không tạo `/dev/fb0`. Không có touch modules ngoài on-key event0.

Bổ sung nodefb0, nạp exact cyttsp modules, tạo input nodes theo sysfs. FBInk xác
nhận PW3, 1072×1448, 8bpp, stride1088; chụp framebuffer kiểm tra bằng PNG từ
stdlib Python, không cần Pillow. KOReader KindlePW2 cần glibc soft-float; giữ
Alpine/musl làm OS và thêm thư viện GNUglibc2.20 tương thích riêng trong
`/opt/reader-compat`, không sao chép/chạy các service Amazon.

`native.lua` bỏ fake LIPC events/launcher/ads assumptions, mở touch event1,
đọc pin/sysfs và chỉnh frontlight trực tiếp. Không bật Wi-Fi/suspend manager
Amazon. Hộp thoại “startup script đã cập nhật” ban đầu do so script stock trong
`/var/tmp`; native mode dùng startup riêng nên bỏ kiểm tra cũ đúng phạm vi.
Thoát thường trả về thư viện; crash giữ USB sống và hiện chẩn đoán, tránh lặp
crash. Root mở rộng64→256MiB để đủ reader/fonts và dung lượng ghi.

## Bằng chứng theo giai đoạn

| Gate | Bằng chứng | Kết luận đúng phạm vi |
|---|---|---|
| Input/kernel ABI | captures, `module-abi-summary.json`, stock source/config | Đúng máy/ABI, chưa phải native boot |
| Chroot Alpine | control-session + ABI checks | BusyBox/musl chạy trên stock kernel; stock vẫn PID1 |
| Native readonly | `bootstrap/native-v5-usb-proof.json` | NonceUSB + Alpine globalPID1, có deadline |
| USB cứu hộ thật | `bootstrap/physical-usb-rescue-enumeration.json` | ID riêng, đã unmount host, chưa proof repair |
| Reinstall thật qua USB | `bootstrap/physical-usb-rescue-repair-proof.json` | Ảnh lỗi→ghi ảnh64MiB→hash/nonce→stock return |
| Persistent OS | `native-rootfs-persistent/review.json` | RootRW, guardian canceled, uptime117.85s, clean reboot, dữ liệu bền |
| Abnormal reset | `physical-persistent-reset-rescue-proof.json` | Reset có kiểm soát với root đã RO→USB→offline check→Alpine |
| Reader | `native-display/` runtime logs/framebuffer + user confirmation | File browser tiếng Việt, mở sách và chuyển trang trên phần cứng |

Boot IDs lịch sử trong `EXECUTION.md`: readonly9959ef9f→stock05fec872;
persistent0b196b96→rebootf62e0680→rescue→finale1771f04. Display image đầu boot
2a4670b6; artifact và kết quả cuối nằm ở acceptance JSON cạnh release hiện tại.
Các ID là correlation cho lần chạy cũ, không được dùng thay nonce cho lần chạy mới.

Các simulation không chứng minh thời gian DMA/USB vật lý. Reset bất thường thử
trên root đã đồng bộ và remount readonly; không cố phá journal đang ghi để
chứng minh power-loss immunity. Recovery p4 đã đạt, recovery khi p1/bootstrap,
kernel hoặc eMMC hỏng chưa được chứng minh. Vì thế giữ p1 backup và không xóa p1.

## Mã và cách tái thực hiện

- Probe stock: `investigation/gleworks-linux-probe.sh`, `gleworks-boot-capture.sh`,
  `gleworks-module-capture.sh`; `native-kit/stock-capture.py` thực thi qua SSH ghim.
- Build/bootstrap: `bootstrap/bootstrap.c`, `shutdown.c`, `pid1-handoff/`,
  `native-rootfs*/`, `native-kit/prepare-trial-bundle.py`.
- Stock stage/promotion: `bootstrap/install-default-trial.sh`,
  `native-kit/arm-stock-trial.sh`, `observe-trial.py`, `install-native-entry.sh`.
- Reader source: `native-display/native.lua`, display/service shell scripts,
  `prepare-overlay.py`, `prepare-image.py`, `capture-screen.py`, `usb_console.py`.
- Cài lại root: `native-kit/kindle-linux.py rescue/deploy-root/inspect/package`.

Các câu lệnh đầy đủ ở README và OPERATIONS. Gói source có manifest và snapshot
ảnh sạch tùy chọn. Source acquisition, captured inputs, build output và kết quả
hardware luôn là bốn loại bằng chứng khác nhau. Không đóng gói các thư mục riêng
hoặc hứa một lần chạy unattended trên thiết bị chưa kiểm chứng.

## Phần chưa thuộc bản minimal

Wi-Fi trên Alpine, Tailscale, HTTP drag/drop, remote server client và power
suspend/resume chưa được triển khai/kiểm chứng. Đọc local và quản trị USB là
phạm vi hiện tại. Stock CONFIG_TUN tắt nên VPN sau này cần chọn userspace path
hoặc thay kernel có kiểm chứng. Không nhét API shell hoặc package install
không xác thực vào web UI để giải quyết tạm. Kế hoạch thay bootloader tương lai
được giữ ở các bootloader review; muốn làm phải mở gate riêng cho USB recovery
khi p1/kernel hỏng, không suy từ thành công userspace hiện tại.

## Dọn dữ liệu Amazon và lịch sử khỏi Kindle, 2026-10-30

Yêu cầu tiếp theo: giữ Amazon làm fallback mặc định sạch dữ liệu; mọi ghi chú
và lịch sử nghiên cứu nằm trong repo host. Không chạy `factory_reset` gốc vì
`shutdown_special` xóa cả p3 và userstore/p4, nơi chứa root Alpine đang dùng.
Không cắt thư viện, framework hay dịch vụ của p1: chưa có bằng chứng một bản
firmware rút gọn như vậy vẫn khởi động an toàn. “Lite” ở đây là dữ liệu người
dùng đã dọn, không phải tuyên bố có một firmware Amazon rút gọn được kiểm chứng.

Đã mount p1 readonly, giữ nguyên hash static init và stock init. p3 được dựng
lại từ `/opt/var/local` và `/opt/amazon/factory_fresh` như luồng khởi tạo gốc;
xóa reg, Wi-Fi, database/lịch sử, log và payload jailbreak. Giữ `/eink` vì đó
là waveform màn hình, không phải dữ liệu cá nhân; SHA256 hai file trước/sau
không đổi, có bản sao hardware-only ở repo host. KMC cũ dùng cờ immutable;
script gỡ cờ chỉ trên các mục đã chọn xóa, không đụng waveform hay p1.

Lượt dọn userstore đầu giải phóng 1517830144 byte, giữ nguyên/hash ảnh Alpine.
Sau yêu cầu đưa mọi ghi chú về repo, hướng dẫn EPUB/TXT, log và snapshot còn
lại được chép về host rồi xóa khỏi Kindle. Reader v4 giữ boot log và reader log trong tmpfs. Bản ghi shutdown ngắn
được xóa ở lần boot kế tiếp, không giữ lịch sử trên FAT. Các request
boot và crash-pending vẫn ở FAT vì cần cho phục hồi sau mất nguồn.

Scripts tái thực hiện: `native-kit/clean-amazon-state.sh` và
`clean-legacy-storage.py`. Bằng chứng riêng ở `native-kit/.private-cleanup/`,
hướng dẫn thử trước đây ở `native-kit/archived-device-notes/`. Không đưa các
thư mục riêng này vào gói chia sẻ. Không thử Factory Reset thật hoặc boot
lại Amazon sau dọn; fallback vẫn là root Amazon giữ nguyên để rollback
entrypoint có giám sát, không phải một chế độ tự chọn trong bootstrap hiện tại.

Bản thử v3 đổi cả đường ghi journal của static shutdown sang tmpfs: emulator
reboot thành công, nhưng lượt chuyển cứu hộ thật mất phản hồi và phải dùng nút
nguồn. USB riêng 0525:a4a5 bcd0200 phục hồi được; ảnh root còn journal recovery
bit nên không boot lại ảnh đó. Toàn bộ control files/ảnh lỗi được lưu về host
ở `ours/docs/evidence/private/cleanup/v3-recovery`. Chưa xác định nguyên nhân dừng trên phần
cứng; không coi emulator là bằng chứng giải quyết lỗi này.

Bản v4 quay lại shutdown binary giống từng byte với bản v2 đã kiểm chứng
(SHA25630fee4cde6e2643f5d3d5718b3d0e9979a1a9a2fe7b61921d8d18f7042cb369a),
chỉ chuyển boot/reader log sang RAM và xóa handoff log cũ sau khi startup đã
cancel guardian/remount root RW. Giữ nguyên cơ chế pending/unmount/FAT/reset.
