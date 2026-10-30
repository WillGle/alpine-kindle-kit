# Bắt đầu session — Kindle PW3 Alpine native

Cập nhật: 2026-10-30. Tài liệu tập trung tại `docs/`; backlog duy nhất là [BACKLOG.md](../BACKLOG.md).
Đây là điểm tra cứu trạng thái hiện hành. Những số đo dưới
đây là lần kiểm chứng gần nhất, không đảm bảo thiết bị vẫn kết nối ở session mới.
Lượt cập nhật tài liệu này không reboot hay thay đổi thiết bị.

## Mục tiêu và phạm vi

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
| Đọc sách | Người dùng xác nhận mở sách/chuyển trang trên v2; v4 và sau dọn đã kiểm tra tự mở/thư viện trống |
| Boot ID lần cuối ghi nhận | `f0600849-2199-4d16-8797-7311ac08f040` |
| Cứu hộ | Đã thực hiện Alpine → USB cứu hộ độc lập → Alpine trên máy này |
| RAM sau dọn bản dịch | Khoảng 38 MiB ngoài buffers/cache; số đo lúc thư viện trống, không phải benchmark đọc PDF |
| Dung lượng | Userstore trống khoảng 2.8 GiB; root image 256 MiB, bên trong trống khoảng 168 MiB sau dọn |
| Chưa triển khai | Wi-Fi/Tailscale/web upload/suspend hoàn chỉnh trên native Alpine |

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
/gleworks/guard-control status
free -m
df -h
tail -20 /run/reader.log
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
dung, không còn là hướng dẫn chạy hiện hành. Workspace vẫn không có Git dùng
được. Không ghi thiết bị hoặc đổi paired identity đang chạy trong lượt này.

Build/check nguồn sau chuyển là bằng chứng host, không nghiệm thu hardware
mới. First-install bằng coordinator mới, reader trên candidate mới và dựng
toolchain từ môi trường sạch vẫn cần bằng chứng riêng.
