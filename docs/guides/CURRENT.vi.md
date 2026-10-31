# Bắt đầu session — Kindle PW3 Alpine native

Cập nhật: 2026-10-31. Tài liệu tập trung tại `docs/`; backlog duy nhất là [BACKLOG.md](../BACKLOG.md).
Đây là điểm tra cứu trạng thái hiện hành. Những số đo dưới
đây là lần kiểm chứng gần nhất, không đảm bảo thiết bị vẫn kết nối ở session mới.
Lượt cập nhật tài liệu này không reboot hay thay đổi thiết bị.

## Mục tiêu và phạm vi

**Định nghĩa sản phẩm:** GleWorks OS là một Linux native tối giản cho E-ink —
Alpine làm nền, kernel/bootloader gốc, UI LVGL vẽ trực tiếp framebuffer/EPDC
(không X11/Wayland/compositor), đọc sách là ưu tiên; KOReader là app, không là OS
shell. "Tối giản" là không có lớp desktop trung gian và màn tĩnh không sinh công
render, không phải ít tính năng. V1 chỉ hỗ trợ PW3 `G090KB` / `5.16.2.1.1`.

Máy đọc sách là chính, tiến tới edge device E-ink tiết kiệm điện: Alpine độc
lập, đọc sách, quản trị từ xa, Wi-Fi + Tailscale và web kéo/thả tài liệu.
Giữ bootloader/kernel gốc. Chỉ dùng USB thường; không UART/mở máy.
Người dùng đã cho phép chuyển OS, ghi thiết bị và xóa dữ liệu Amazon cũ;
không cần lặp lại câu hỏi cho công việc đã được cho phép. Yêu cầu tránh brick
vẫn áp dụng. Dữ liệu nghiên cứu/ghi chú phải ở host.

## Thiết bị và trạng thái đã xác nhận

| Mục | Kết quả gần nhất |
| --- | --- |
| Phần cứng | Paperwhite 3, Muscat/Wario, i.MX6SL ARMv7, RAM khoảng 503 MiB khả dụng |
| Firmware nền | 5.16.2.1.1; kernel `3.0.35-lab126`, bootloader giữ nguyên |
| OS đang dùng | Alpine 3.24.3, BusyBox global PID1, root đọc/ghi; không phải chroot |
| Reader | KOReader v2026.07.1 tự mở; cảm ứng và tiếng Việt hoạt động |
| Đọc sách | Người dùng xác nhận mở sách/chuyển trang trên v2; v4 và sau dọn đã kiểm tra tự mở/thư viện trống; trên OS trial `55f05ba1…` người dùng xác nhận khóa màn hình ngoài → Unlock → mở reader → lật trang bình thường |
| Return to Home | Đã xác nhận hoạt động: KOReader thoát mã 0, service báo `foreground=none`, shell trở lại và không respawn reader. Có flash ngắn (~0.5 giây) khi chuyển. Crash → Home có thông báo lỗi vẫn **chưa** kiểm chứng |
| Nguồn / màn hình | Nút nguồn ngắn: service phát `POWER LOCK`, shell hiện **màn hình khóa**, tắt frontlight, rồi vào **mem suspend thật** nếu không cáp và không sạc; nhấn lại khôi phục ánh sáng, vẫn giữ khóa. Khi KOReader đang mở, plugin thấy `lock=locked`, thoát reader và shell mới hiện màn hình khóa. RTC 15 phút là wake fallback; resume không nút thì giữ màn tối và hẹn ngủ lại sau 30 giây. Đã nghiệm thu mem suspend + RTC wake + đường evdev bằng uinput và **bấm nút vật lý thật** (lock → suspend → wake → Unlock). `CAPS light-set=1 suspend=1`, `display=on/off` trong STATUS. Paint policy: mặc định `quality` (một GC16 mỗi scene, không flash thứ hai), `balanced` (DU4 + GC16 mỗi 4 scene), `fast` (DU4 + REAGL, có ghosting/flash); xem [EINK-UI-RENDERING.vi.md](../technical/EINK-UI-RENDERING.vi.md) |
| Paint quality đo trên máy | Build `0163000e…`, policy `quality`: 20 scene = 20 GC16, idle 5 phút = 5 submit, phím A2 68 ms, 20 ký tự không full flash, 50 scene soak fd 7→7/RSS 656→680 kB. Ghost/flash nhìn thấy **chưa** được người dùng chấm; rescue chưa chạy lại. Bằng chứng: `docs/evidence/paint-quality/` |
| Boot ID lần cuối ghi nhận | `03319c70-5a61-4fd8-b8c1-6095030127ee` (Alpine 3.24.3 sau hai lần reset; release v4: `f0600849-2199-4d16-8797-7311ac08f040`) |
| Console USB | `0525:a4a7` / `/dev/ttyACM0`; nonce handshake và inspect đã hoạt động lại, cùng boot ID. UDC hash trước đó khớp candidate đã thử reconnect |
| Tailscale | Alpine báo IP `100.100.178.75`, `net=connected`; host thấy peer `alpine-reader` online. SSH đến được Tailscale SSH nhưng đang chờ xác thực trình duyệt; chưa có shell SSH |
| Daemon Tailscale | PID `4720` đang chạy userspace trên boot này; tự khởi chạy sau reboot chưa xác minh |
| Màn hình | Home/Settings/Applications/App updates đã dùng thật trên máy; framebuffer 1072×1448, 8 bpp, stride 1088. Chất lượng hiển thị/ghosting trên panel vẫn cần đánh giá khi đọc nhiều trang |
| Cứu hộ | Đã thực hiện Alpine → USB cứu hộ độc lập → Alpine trên máy này |
| RAM sau dọn bản dịch | Khoảng 38 MiB ngoài buffers/cache; số đo lúc thư viện trống, không phải benchmark đọc PDF |
| Dung lượng | Userstore trống khoảng 2.8 GiB; root image 256 MiB, bên trong trống khoảng 168 MiB sau dọn |
| Wi-Fi bền | Đã lưu cấu hình 0600 và **tự kết nối lại sau clean reboot** (`9b2a013b…` → `fd727e9a…`). Service giám sát link mỗi 5 giây, retry backoff 5→60 giây; Disconnect tắt auto-connect nhưng giữ credentials |
| App manager + updater | Đã có `gw-appctl`, trust key RSA-2048 tại `/etc/gleworks/app-keys/release.pem`, cài/update/remove bundle `.gwapp` có chữ ký từ USB; giữ version cũ và app data. UI Applications/App updates đã dùng thật trên máy |
| Chưa nghiệm thu | Tailscale/SSH sau xác thực, web upload, OS A/B update, **generic launch cho app ngoài Reader**, UID riêng/limit/fd handoff, crash→Home và data image |

Không dùng kết quả VPN/web của hệ Amazon/JB cũ để chứng nhận native Alpine.
Không coi E-ink giữ ảnh tĩnh là CPU đã ngủ hay OS còn sống.

## Phân biệt thiết bị, nguồn và release

- **Release v4 đã đóng gói:** `work/archive/releases/pw3/kindle-native-pw3-reader-v4-2026-10-30.tar.gz`.
  Ảnh sạch: `work/images/releases/pw3/alpine-reader-v4.ext3`, 268435456 byte,
  SHA256 `927e9ab68fa9677e18d35691e226bd9ad519b44e666aa275d9ea6c3d3eb3d3eb`.
  Archive SHA256 `c49e29572011f60f0602ee13227bba3bd4f556d184ed61d1512ff08dafcef2fe`.
  [Kiểm tra gói](../../work/archive/releases/pw3/verification-v4.json): 173 file đúng manifest/hash,
  giải nén mới, CLI help và e2fsck ảnh đều qua.
- **Thiết bị và nguồn hiện tại mới hơn v4:** đã thêm GC16 flashing khi mở reader,
  full refresh mỗi trang cả ngày/đêm để giảm ghosting; bỏ 59 bản dịch giao diện,
  giữ Việt/Anh và giấy phép. Bản dịch giảm 18335 → 975 KiB (~17 MiB).
- Các sửa sau v4 **chưa có ảnh release/gói mới**. Cài lại v4 sẽ mất các sửa đó.
  Hash ảnh v4 là hash artifact sạch trên host, không phải hash root đang chạy.
- Người dùng xác nhận full refresh thủ công đã hết hoặc giảm rõ ghosting;
  chưa có xác nhận đọc nhiều trang sau đổi cấu hình. Ảnh framebuffer không
  chứng minh hết bóng mờ trên panel.

## Bố trí lưu trữ và an toàn

- p1: 471859200 byte, firmware Amazon và static `/sbin/init` bootstrap.
  Init gốc giữ ở `/sbin/init.gleworks-stock`; không cắt bớt firmware fallback.
- p3: dữ liệu Amazon đã làm sạch, dựng từ factory templates; giữ guid và waveform.
  Fallback còn file nhưng **chưa kiểm chứng boot Amazon sau dọn**; không có menu
  boot Amazon được chứng nhận. Không gọi đây là firmware “Amazon lite” đã test.
- p4: FAT ở offset 8192, mount duy nhất `/boot/gleworks-control`;
  `/mnt/us` là symlink tới đó. Sách nằm trong `documents/`.
- `gleworks-alpine/system.ext3`: root Alpine 256 MiB. `boot.enabled` và
  `boot.pending` mang nội dung chính xác `gleworks-alpine-v1\n`, là trạng thái
  boot bắt buộc. Pending còn lại sau crash/dirty root phải qua cứu hộ, không xóa
  để lách kiểm tra. Pending tồn tại lúc Alpine chạy là bình thường.
- Log boot/reader ở `/run` tmpfs. Shutdown helper giữ một bản ghi chuyển trạng
  thái ngắn trên FAT, boot Alpine kế tiếp xóa. Không tích lũy lịch sử trên Kindle.
- Không mount/export FAT khi root ảnh bên trong còn đang dùng. Không chạy
  `reboot -f`, Amazon Factory Reset, flash mainline/barebox hoặc sửa IDME/p1
  để chữa lỗi UI. Factory Reset Amazon sẽ xóa cả Alpine trên p4.
- Thử nghiệm shutdown v3 từng treo, cần giữ nút nguồn để phục hồi; nguyên nhân
  chưa xác định. v4 đã quay lại binary shutdown đã được kiểm chứng trước đó.

## Bắt đầu làm việc, không thay đổi boot

Chạy từ `/home/will/dev/kindle`. Không chạy hai phiên console cùng lúc.

```sh
python3 tools/maintenance/kindle-linux.py --help
python3 tools/maintenance/kindle-linux.py inspect --output /tmp/pw3-session-inspect.log
python3 tools/lib/usb_console.py <<'SH'
cat /proc/sys/kernel/random/boot_id
readlink /proc/1/exe
gwctl status
free -m
df -h
tail -20 /run/gw-system.log
SH
```

Inspect chỉ hoạt động khi đúng serial gadget và nonce mới trả lời. Khi không
thấy console, xác định chế độ USB trước; đừng suy ra máy hỏng từ màn hình cũ:
`0525:a4a7` là Alpine serial; `0525:a4a5` bcd `0200` là mass-storage rescue
độc lập không deadline ghi. `a4a6` rỗng hoặc Amazon `1949:0004` không phải rescue.
Xem [vận hành](OPERATIONS.vi.md) cho rescue/resume/deploy/chép sách/phục hồi.

## Bản đồ tài liệu và bằng chứng

| Cần tra cứu | Nơi đọc |
| --- | --- |
| Cài, dựng lại và đóng gói | [README.vi.md](BUILD-INSTALL.vi.md) |
| Bảo trì, USB, sửa lỗi reader | [OPERATIONS.vi.md](OPERATIONS.vi.md) |
| Reverse engineering, quyết định và giới hạn | [REVERSE-ENGINEERING.vi.md](../technical/linux/native-kit/REVERSE-ENGINEERING.vi.md) |
| Trạng thái có cấu trúc | [ACCEPTANCE.json](../evidence/ACCEPTANCE.json) |
| Power / suspend / nút nguồn | [pw3-power-suspend-2026-10-31.md](../evidence/pw3-power-suspend-2026-10-31.md), [physical button](../evidence/pw3-power-physical-button-2026-10-31.md) |
| Paint / transition / section isolation | [PW3-PAINT-POLICY.md](../technical/PW3-PAINT-POLICY.md), [cơ chế waveform + policy UI](../technical/EINK-UI-RENDERING.vi.md) |
| Kế hoạch sạch ghost/flicker + phản hồi | [paint quality plan](../technical/superpowers/plans/2026-10-31-eink-paint-quality.md) |
| Wi-Fi lưu cấu hình + auto-reconnect | [pw3-wifi-autoconnect-2026-10-31.md](../evidence/pw3-wifi-autoconnect-2026-10-31.md) |
| App manager, installer, chữ ký, UI update | [pw3-app-installer-2026-10-31.md](../evidence/pw3-app-installer-2026-10-31.md) |
| Hợp đồng runtime app (draft/cập nhật) | [APP-SESSION](../technical/superpowers/specs/2026-10-31-app-session-contract.md) |
| Kế hoạch app runtime còn lại | [app runtime plan](../technical/superpowers/plans/2026-10-31-app-runtime.md) |
| Nguồn upstream, commit/hash | [SOURCES.json](../technical/SOURCES.json) |
| Nhật ký đầy đủ, theo thời gian | [BACKLOG.md](../BACKLOG.md#history-001) — phần cũ là lịch sử |
| Mã hiện hành | `os/`, `apps/koreader/`, `tools/` |
| Capture gốc của phần cứng | `work/private/evidence/captures` |
| Backup p1 bắt buộc giữ | `work/private/evidence/pretrial-backup/p1.ext3` |
| Touch modules đã capture | `work/private/evidence/captures/gleworks-module-capture-20261029-194813-17967/stock-modules.tar` |
| KOReader upstream local | `work/sources/koreader/` |
| Dữ liệu đã đưa khỏi Kindle | `work/private/evidence/cleanup/final-device-history`, `docs/evidence/device-notes` |

Các đường dẫn code trong bảng tính từ thư mục gốc workspace. Có dữ liệu
riêng nằm ngoài source package; không suy từ “có gói” thành đầy đủ backup/token
để dựng một máy khác. Đầu vào build phải là ảnh offline sạch, không dùng root
đang chạy và không mặc định mọi `*.ext3` cũ là base phù hợp. README mô tả quy trình;
review cạnh từng artifact ghi nguồn và hash. Không thay token guardian riêng lẻ.

Hash entrypoint đã được kiểm tra ở mốc dọn p3 (phải đọc lại trước sửa p1):
static `/sbin/init` = `b4aa162ffc2972a59a3c649d15b600c90e495e5025f6c29bada7798acb0b7175`;
stock wrapper = `ad624cbad3090bf15e65c8420a4e451ee10fd067a40b1bfe662965e82d932d0a`.
Binary shutdown v4 đã quay lại bản đã kiểm chứng, SHA256
`30fee4cde6e2643f5d3d5718b3d0e9979a1a9a2fe7b61921d8d18f7042cb369a`.
Không dùng hash hay kết quả emulator để thay cho thử phục hồi máy thật.

Bằng chứng gần nhất (trong `work/private/evidence/cleanup` trên host):

- `v4-deployment.json`, `v4-final-runtime.log`: triển khai và Alpine/reader sau rescue.
- `amazon-state-cleanup.log`, `eink-hardware.tar.gz`: dữ liệu p3/waveform giữ lại.
- `storage-cleanup.json`, `final-storage-cleanup.json`: dọn userstore, hash ảnh còn lại.
- `ghosting-refresh-fix.log`: cấu hình full refresh đã áp dụng.
- `locale-trim.log`, `locale-trim-runtime.log`: 59 bản dịch đã dọn, reader chạy lại.
- `unused-reader-translations.tar.gz` và `.tar.review.json`: bản sao/hash bản dịch.
- `v3-recovery/`: giữ bằng chứng trial lỗi, không tái dùng ảnh đó để boot.

## Tiếp tục và bàn giao

### Bộ cài PW3 cho cộng đồng — source candidate 2026-10-30

Người dùng đã duyệt hoàn thiện bộ cài cho PW3 đã jailbreak/có root trước,
sau đó chuẩn bị công khai để cộng đồng đóng góp phiên bản khác. Hướng dẫn
độc lập tiếng Anh tại [community/README.md](../community/README.md), build tại
[community/BUILD.md](../community/BUILD.md). Đây là phần mới trên host, không
phải thay đổi hệ Alpine đang cài hoặc nghiệm thu một PW3 khác.

- `pw3-control.py`: tạo bộ SSH tạm riêng, key-only/host-bound, không chạy diagnostic
  cũ hoặc lưu log lên FAT trong chế độ control-only.
- `pw3-build.py`: dựng bộ paired guardian/bootstrap/control với identity mới,
  root sạch 256 MiB và reader/refresh/locale hiện hành; input/paths tường minh.
- `pw3-install.py`: preflight, backup, bind, dispatcher, stock return, readonly
  Alpine, ảnh lỗi/rescue repair, promote và final native/rescue roundtrip.
- `public-source.py`: allowlist source/docs, không lấy image/binary/khóa/token/
  capture. Không dùng lệnh package lịch sử để tạo gói public này.

Profile đầu vào hiện giới hạn đúng mã `0KB`/G090KB, firmware 5.16.2.1.1,
kernel/layout/init wrapper đã biết. Không coi mọi PW3 đều đã được hỗ trợ.
Candidate riêng đã build từ source giải nén trong thư mục mới, ARM checks và
filesystem qua; các dependency/toolchain đã chuẩn bị được chỉ định rõ, chưa
phải tái dựng toàn bộ toolchain từ môi trường sạch. Không dùng identity mới
để thay lẻ component trên máy đang chạy. Bằng chứng host nằm tại
[community-installer-host-checks.json](../evidence/community-installer-host-checks.json).

**Còn chờ:** cài lần đầu bằng coordinator mới trên PW3 stock phù hợp, đầy đủ
gate lỗi/phục hồi và xác nhận đọc nhiều trang trên candidate mới. Máy tham
chiếu hiện đã native, nên không dùng update root để giả làm bằng chứng đó.
Không public/upload trong lượt triển khai này. V4 và trạng thái máy cũ giữ nguyên.

Ưu tiên theo yêu cầu tiếp theo của người dùng; các việc còn thiếu là: đóng gói
release mới gồm sửa ghosting/dọn locale, kiểm chứng đọc thực tế sau sửa,
Wi-Fi + Tailscale, web upload có xác thực, và suspend/điện năng. Chưa tự động
bắt đầu các việc đó chỉ vì mở session. Chưa thu nhỏ root image hoặc firmware p1.

Sau mỗi thay đổi, cập nhật tài liệu này và ACCEPTANCE; ghi bằng chứng ở host,
append BACKLOG. Luôn tách: kiểm tra nguồn/host, chạy máy thật, xác nhận từ người
và hash release. Không sửa archive lịch sử để giả làm release hiện hành.

## Bố cục nguồn mới — 2026-10-30

Đã tách nguồn OS tại `os/alpine/` và `os/devices/pw3/`, reader tại
`apps/koreader/`, công cụ host tại `tools/`. Build mặc định là base Alpine
không KOReader; dùng `--variant reader --reader ...` cho lớp ứng dụng.
`--work-dir` chọn dữ liệu local độc lập vị trí nguồn/cwd. Xem
[STRUCTURE.md](STRUCTURE.md) và [bảng chuyển](../evidence/layout-moves.json).

Backup/capture riêng ở `work/private/evidence/`; bản nguồn trước sửa và
inventory đối chiếu ở `work/private/migrations/2026-10-30-layout/`. Artifact
lịch sử giữ nguyên byte; log và script lịch sử giữ đường dẫn gốc trong nội
dung, không còn là hướng dẫn chạy hiện hành. Khi bắt đầu chuyển thư mục, Git chưa dùng được; metadata Git đã xuất hiện
ở lượt kiểm tra cuối. Không tạo commit hoặc dùng Git để rollback. Không ghi thiết bị hoặc đổi paired identity đang chạy trong lượt này.

Build/check nguồn sau chuyển là bằng chứng host, không nghiệm thu hardware
mới. First-install bằng coordinator mới, reader trên candidate mới và dựng
toolchain từ môi trường sạch vẫn cần bằng chứng riêng.

## Hardware RCA — lỗi ban đầu (xem cập nhật sau sửa bên dưới)

Lượt thử30/10 xác nhận rút/cắm USB trong native làm mất enumeration dù UI sống; PMIC nhận host nhưng clockUSB vẫn0/0. Reboot sạch từ menu phục hồi. Force-detect pulse đã thực thi nhưng không sửa được. Reset cứng giữpending/dirty và chuyểnrescue theo policy; đã repair bản sao offline và xác minh reboot/cứu hộ sạch. Chưa cài kit mới, chưa qualify first-install. Xem [RCA](../technical/PW3-USB-AUTOBOOT-RCA-2026-10-30.vi.md). Không coi hotplug/crash auto-recovery đã hoàn thiện.

## Sau sửa USB và bootstrap — 30/10

Module UDC mới đã qua ba lần rút/cắm USB thật trên cùng boot ID; console được xác minh lại. Bản sửa giữ controller/PHY thức sau unplug, chưa đo hao pin. Bootstrap có phục hồi journal offline giới hạn một lần, pin tối thiểu50%, không export USB khi repair. Bootstrap đã cài cùng identity guardian trên máy tham chiếu, có backup p1 init; reboot sạch và vào rescue đạt. Người dùng thấy dòng trạng thái USB rescue trên nền ảnh cũ. Reset cứng40s thật đã tự replay journal và vào Alpine/KOReader; marker phục hồi xóa và guardian0 được xác minh. Driver đã đồng bộ cả native và p1 rescue, giữ backup. Mới kiểm chứng một lần reset cứng; chưa nghiệm thu first-install kit mới. Bootloader và kernel image giữ nguyên. Xem [RCA](../technical/PW3-USB-AUTOBOOT-RCA-2026-10-30.vi.md).

## Cài sạch candidate reader mới — đang nghiệm thu

Sau cho phép xóa/cài lại, đã cài ảnh reader sạch `pw3-reader-94462d39be54` cùng identity bootstrap/guardian mới. Backup p1 pair và ảnh hệ cũ trên host; giữ ảnh cũ trên thiết bị. Boot native mới, hash payload runtime, guardian0 và vòng cứu hộ/resume đã đạt. Boot sau resume `3feb7cf3-c7e2-4be7-967f-6131a0573828`; KOReader chạy ổn định quá90s. Framebuffer thư viện hiển thị EPUB thử tiếng Việt. Người dùng xác nhận đọc/lật trang; hotplug cùng boot và reset cứng tự replay journal trên chính candidate này đều đạt. Đây là reinstall từ native, chưa nghiệm thu first-install coordinator từ Amazon OS. Bằng chứng riêng: `work/private/hardware-trials/candidate-reader-20261030T074953Z/`.

Candidate mới: người dùng xác nhận đọc/lật trang bình thường; USB nối lại qua hai chu kỳ kernel disconnect/reconnect, console xác minh cùng boot `3feb7cf3-c7e2-4be7-967f-6131a0573828`, uptime670s, guardian0. Reset cứng trên candidate này đã đạt, xem kết quả cuối dưới đây.

### Kết quả nghiệm thu reinstall

Candidate reader mới đã qua đọc/lật trang thật, hai chu kỳ USB reconnect cùng boot, reset cứng40s tự replay journal, kiểm tra root sạch offline, ảnh không hợp lệ→rescue→khôi phục hash→native. Máy cuối chạy KOReader, guardian0, boot `75863c3b-1b18-4d40-9704-61003d6f2394`. Candidate/pairing nguồn giữ tại `work/build/pw3-reader-94462d39be54/`; backup và log trong trial riêng. Xem [biên bản](../evidence/pw3-clean-reinstall-2026-10-30.json). Không gộp reinstall từ native với first-install từ Amazon OS; chưa đo hao pin USB.

## Phạm vi sản phẩm mới — Alpine OS hoàn chỉnh, UI tối giản

Người dùng xác nhận cần Home, thư viện, cài đặt, cài/quản lý app, quản lý phần cứng, power, kết nối và cập nhật thống nhất; UI ưu tiên e-ink/sách/app. KOReader là app, không là OS shell. [Thiết kế v1](../technical/superpowers/specs/2026-10-30-minimal-alpine-reader-os-design.md) đã được triển khai một phần và nghiệm thu trên máy: Home, Settings, Applications, App updates, Network, Calendar, Notes, Lock; `gw-system` sở hữu nút nguồn/power, Wi-Fi và app lifecycle; signed app installer + update UI chạy thật (xem evidence 2026-10-31). Chưa có A/B OS update, catalog online, sandbox UID riêng hoặc chuyển quyền phần cứng đầy đủ.

Power native đã đạt **mem suspend + RTC wake**, đường evdev bằng uinput và
**một lần bấm nút vật lý thật** (lock → suspend → wake → Unlock); cổng cáp/sạc
chặn suspend đúng. Adapter power kiểu KOReader không được promote; service là
nơi duy nhất sở hữu nút và suspend. Chi tiết:
[power evidence](../evidence/pw3-power-suspend-2026-10-31.md),
[physical button](../evidence/pw3-power-physical-button-2026-10-31.md).

Service và shell đã được cài và chạy trên máy (boot `fd727e9a-cfe4-4f60-a2d4-98f530232789`): `gw-system --supervise`, `gw-shell`, diag socket, LVGL/EPDC, touch, Home và chuyển app. Framebuffer 1072×1448, 8 bpp, stride 1088; input `max77696-onkey`/`cyttsp4_mt`. Các mục còn thiếu được liệt kê ở bảng trên, không suy từ bằng chứng host.

Đã ghi nhận một lần nhấn/thả KEY_POWER thật và chạy service tạm qua USB để
đọc trạng thái rồi dừng sạch; reader vẫn chạy cùng boot. Ở thời điểm ghi, đây
chưa phải nghiệm thu nút ngủ/thức hay chuyển quyền phần cứng; sau đó đường
nút vật lý đã đạt (xem bảng trên). Một bản sao root riêng đã được
cài OpenRC và dependency để kiểm tra; các runlevel còn rỗng và chưa boot thử.

Ảnh thử riêng `work/build/reader-os-trial-20261030/` đã được tạo với Home,
màn hình khóa, reader quản lý và Wi-Fi thử nghiệm. Kiểm tra mã nguồn gộp và
kiểm tra ext3 offline đạt; ảnh chưa cài và chưa được nghiệm thu trên máy.
Trước khi thử cần bảo toàn dữ liệu reader hiện tại và kiểm tra đường cứu hộ
độc lập. Reader đang chạy không bị thay đổi trong lượt này.

**Cập nhật sau đó:** ảnh thử có chép dữ liệu reader đã boot trên PW3: boot
`55f05ba1-28ff-4d2d-a67c-c010dc8d92da`, BusyBox PID1, guardian0, USB console,
OpenRC, service và shell đang chạy. Ảnh reader cũ được giữ trong USB cứu hộ
và bản sao host có hash `0678b99279922a548cc19d09baa9f080dc9c5e077a7caa23d82b20c4ba030a9e`.
Màn hình, cảm ứng, reader và Wi-Fi của ảnh thử chưa nghiệm thu; candidate cũ
vẫn là đường khôi phục.

Người dùng xác nhận thấy Home và mở được KOReader; service báo
`foreground=koreader` cùng boot. Lúc đó chưa xác nhận quay về Home: người dùng
chưa thấy mục Return to Home và thao tác Thoát đầu tiên chưa đưa về Home (sau
đó đã xác nhận; xem cập nhật bên dưới). Màn hình
cho thấy mục `Thoát ▸` có menu con. Khóa, Wi-Fi và vòng đời đầy đủ vẫn chờ
kiểm chứng.

Sau đó người dùng nhấn nút nguồn và xác nhận màn hình khóa xuất hiện. Log ghi
KOReader thoát mã 0, đóng input; shell chạy lại và mở node cảm ứng trên cùng
boot. Người dùng xác nhận Unlock trả về Home có cảm ứng. Thao tác Thoát từ
menu được xác nhận sau đó (xem dưới); suspend và Wi-Fi vẫn chưa được xác nhận.
Shell thử có nút Wi-Fi đã được
đặt cho lần mở Home kế tiếp, giữ bản shell trước trong `/usr/sbin/gw-shell-before-wifi-trial`.

**Cập nhật Return to Home:** người dùng xác nhận mục “Return to Home” hiển thị
trong menu KOReader và đưa về Home trên cùng boot `55f05ba1…`. Log ghi KOReader
thoát mã 0 lúc 13:09:29, đóng input; service báo `foreground=none`,
`reader-exit=0`, `lock=unlocked`; `gw-shell` restart và không respawn reader.
Crash → Home có thông báo lỗi, cùng việc lưu/khôi phục đúng vị trí đọc sau khi
về Home, vẫn chưa kiểm chứng.

Bản thử mới hơn đã nạp driver ath6kl và dùng `wpa_supplicant` quét mạng;
người dùng xác nhận danh sách mạng xuất hiện. Bản sửa chọn mạng đầu tiên đã
được nạp. Kết nối, lưu cấu hình và tự kết nối lại sau reboot chưa nghiệm thu.

Theo ảnh tham chiếu mới, Home thử đã đổi sang lưới ba cột, đen trắng, chữ
lớn hơn; chỉ Reader được hiển thị là app có sẵn. Shell chờ cập nhật màn hình
khi bàn giao thay vì sau mỗi lần vẽ. Framebuffer đã chụp được giao diện mới;
độ sắc nét, tốc độ cảm nhận và bàn giao màn hình vẫn cần người dùng kiểm tra.
Wi-Fi tạm gác theo yêu cầu.

**UI polish host-only (2026-11-01):** đợt chỉnh UI theo
[PW3-UI-POLISH-PLAN.md](../technical/PW3-UI-POLISH-PLAN.md) đã xong Phase 0–7
trên host. Giao diện chuyển sang **tiếng Anh** (quyết định sản phẩm D1; nội
dung người dùng như tên sách/nhiệm vụ/SSID vẫn đa ngôn ngữ), nav 5 tab
Home/Library/Calendar/To-dos/Settings, bỏ hero “Today.” và Quick Access, mọi
target chạm ≥96 px gồm Wi-Fi/Light/Lock/battery trên status bar; Settings có
Battery/Storage/Version/Uptime/Date và stepper đèn; To-dos có due date và
Undo; Calendar có chuyển tháng/ô 96 px/dot; Network có list, signal, security,
confirm và busy; Applications/App updates/Lock theo bảng primary. Host suite
đạt **2503 check, 0 fail**, heap LVGL **182616/262144 (69.8%)**, ARM
`gw-shell` **3.064.832 byte** (+438.272 so với baseline). 24 ảnh 1072×1448 và
geometry nằm ở
[evidence/pw3-ui-polish-2026-11-01](../evidence/pw3-ui-polish-2026-11-01/README.md).
Đã push lên PW3 ngày 2026-11-01, giữ rollback
`/usr/sbin/gw-shell.before-ui-polish-20261101` và
`/usr/sbin/gw-shell.before-network-scroll`; ảnh framebuffer xác nhận Home
mới và shell do `gw-system --supervise` chạy lại. Chưa nghiệm thu cảm ứng,
ghosting/hình ảnh trên panel; paint quality regression vẫn là cổng riêng trước
release.
