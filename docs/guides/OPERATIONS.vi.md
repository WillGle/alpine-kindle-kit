# Dùng Kindle Alpine + KOReader

Đọc [CURRENT.vi.md](CURRENT.vi.md) để biết trạng thái, phiên bản và nơi lưu bằng chứng.

## Đọc sách

Máy tự boot Alpine rồi vào thư viện KOReader. Chạm tài liệu để mở. Chạm mép
phải/trái để sang/quay trang; chạm vùng trên cùng để mở trình đơn. Có thể chỉnh
font/cỡ chữ EPUB hoặc zoom PDF trong KOReader. Mục đèn dùng sysfs của Alpine,
không gọi Amazon powerd. Thoát thường mở lại thư viện sau khoảng hai giây.

Tài liệu nằm trong `documents` của user storage; liên kết `/mnt/us` trỏ tới mount
FAT duy nhất đã được bootstrap chuyển vào Alpine. Các hướng dẫn thử nghiệm và log đã chuyển về repo trên host; chúng không
được giữ trong thư viện của thiết bị. Nội dung cũ đã được dọn theo yêu cầu.
KOReader lưu sidecar/cài đặt theo cơ chế của ứng dụng. Đóng tài liệu hoặc thoát
reader trước khi reboot/chép file để lưu trạng thái đang đọc. Đóng filesystem
sạch không thay thế việc ứng dụng lưu những thay đổi còn trong RAM.

Nếu cần chỉnh đèn qua console, đọc giá trị hiện tại ở
`/sys/class/backlight/max77696-bl/brightness`; kernel chấp nhận0..4095. UI ánh
sáng ánh xạ0..24 theo đường cong giới hạn trong `native.lua`. Bản minimal chưa
có suspend/resume tiết kiệm điện hoàn chỉnh; không coi màn hình e-ink tĩnh là
bằng chứng CPU đã ngủ. Giữ nguồn USB trong quá trình tinkering dài.

## USB quản trị

USB đang là serial gadget `0525:a4a7`, thường là `/dev/ttyACM0`, không phải ổ
đĩa. Gửi `STATUS` để đọc trạng thái, `SHELL` để vào shell quản trị root, `REBOOT`
để reboot sạch. Console vật lý này không được đưa lên mạng. Đã vào SHELL thì
`REBOOT` không còn là lệnh shell; dùng `/bin/busybox kill -QUIT 1`.

```sh
python3 tools/maintenance/kindle-linux.py inspect --output /tmp/pw3.log
python3 tools/maintenance/kindle-linux.py reboot
python3 tools/maintenance/capture-screen.py
```

Ảnh screenshot lấy từ framebuffer hiện tại; mặc định lưu ảnh có tên riêng tại `WORK/private/screens/`.
Đây không phải camera chụp panel: chữ đã ghi vào framebuffer và update driver
thành công chưa thay thế hoàn toàn việc nhìn màn hình/tap thực tế. Acceptance
hiện có cả screenshot và xác nhận người dùng mở sách/chuyển trang.

`usb_console.py` dùng được để chạy lệnh bảo trì đã chọn qua USB:

```sh
python3 tools/lib/usb_console.py <<'SH'
cat /proc/uptime
tail -30 /run/reader.log
SH
```

Lệnh chạy root trên đúng serial gadget và cần trả lời nonce mới. Không mở shell
mạng, không có HTTP command endpoint. Khóa phiên SSH cũ không cần cho vận hành
native và không nằm trong archive.

## Chép tài liệu bằng cáp

Không bật mass storage khi root Alpine còn dùng ảnh trên FAT. Để chép file bằng
file manager của host, chuyển vào cứu hộ độc lập:

```sh
python3 tools/maintenance/kindle-linux.py rescue
```

Lệnh tắt lựa chọn boot thường, đóng sạch Alpine, reboot rồi mount đúng rescue
USB. Có thể chép bằng file manager rồi gọi lệnh tự kiểm tra root sạch và eject:

```sh
python3 tools/maintenance/kindle-linux.py resume
```

Hoặc dùng một lệnh cho toàn bộ lượt chép, từ Alpine đang chạy → rescue → copy →
đọc lại hash → resume/eject → thư viện reader:

```sh
python3 tools/maintenance/kindle-linux.py put-documents /path/to/book.epub
```

Lệnh không ghi đè tài liệu cùng tên. Nếu ghi thất bại, thiết bị ở rescue, không
bị reset tự động giữa lúc host ghi. Không rút dây giữa lúc host đang ghi. Đường
chuyển chế độ tương tự được `deploy-root` dùng khi cài ảnh mới.

## Khi giao diện lỗi

- Nếu còn console: xem `/run/reader.log`, `/run/reader-stopped`, `/proc/bus/input/devices`
  và `/dev/fb0`. Reader lỗi không làm PID1/USB console mất theo.
- Sau khi sửa nguyên nhân, `rm -f /run/reader-stopped` cho service thử lại ở lượt
  respawn tiếp. `touch /etc/gleworks-reader.enabled` bật autostart;
  xóa file này để tắt autostart mà giữ console.
- Màn hình cây hoặc ảnh cũ chưa kết luận kernel treo. `inspect` xác nhận bằng
  nonce, boot ID, PID1 và uptime, không dựa vào USB enumeration đơn thuần.
- Không dùng `reboot -f` cho vận hành thường. Reboot qua menu reader/console đi
  vào helper RAM, dừng writer, unmount ext3 rồi FAT trước reset.

## Cứu sau reset/crash

`boot.pending` còn từ lần chạy trước hoặc ảnh ext3 dirty → bootstrap vào USB
cứu hộ. USB này có ID`0525:a4a5`, bcd`0200` và không còn deadline tự reset khi
host được phép ghi. Host có thể mount, sao lưu `system.ext3`, chạy e2fsck offline
hoặc thay bằng release sạch. Chỉ sau kiểm tra/thay ảnh mới, xử lý pending có
chủ ý. CLI hỗ trợ `deploy-root ... --clear-pending` với ảnh nguồn đã sạch.

Không xóa pending để né một lỗi chưa hiểu. Không mount ext3 của thiết bị trên
host đồng thời với thiết bị. Không sửa p1/IDME/bootloader như một cách thử UI.
Không suy từ cứu p4 thành khả năng cứu máy khi kernel/p1 hoặc eMMC hỏng.

## Khôi phục root và quay về stock

Cài lại ảnh Alpine gốc/sạch bằng `deploy-root`, luôn chọn tên backup mới.
Rollback **Amazon entrypoint** là giai đoạn khác: phải boot được Alpine và
restore nguyên tử init đã giữ trên p1; hoặc dùng quy trình signed recovery
được kiểm chứng riêng. Các bản gốc còn ở `init.gleworks-stock`,
`init.gleworks-dispatch`, thư mục bootstrap và host p1 backup riêng. Bộ công cụ
bảo trì hiện tại không tự flash p1 hay toàn eMMC.

## Ngủ/thức bằng nút nguồn

Nhấn ngắn nút nguồn: màn hình khóa, đèn tắt; nếu **không cắm cáp và không sạc**,
service vào kernel `mem` suspend thật. Nhấn ngắn lần nữa để thức (đèn trở lại,
màn hình vẫn khóa, bấm Unlock để dùng). RTC 15 phút là wake fallback; nếu thức
do RTC mà không có người bấm, máy giữ màn tối và tự ngủ lại sau 30 giây.

- Cắm cáp quản trị hoặc đang sạc: chỉ khóa màn hình, **không** suspend, để console
  và chép file không bị gián đoạn.
- Giữ nút 40 giây vẫn là reset phần cứng, không đổi.
- Kiểm tra trạng thái: `gwctl status` (`display=`, `lock=`, `usb=`).
- Test không cần bấm nút thật: `gwctl suspend-test` (root, tự thức sau 10 giây).

## Cài/cập nhật app có chữ ký

Bundle `.gwapp` phải đi kèm `.gwapp.sig`; cả hai đặt trong
`/mnt/us/gleworks/apps/` (thư mục `gleworks/apps` trên USB). Sau đó:

1. Settings → Applications → **Install from USB**, hoặc
2. Applications → **App updates** → Check USB → Install update.

Chỉ bundle ký bằng release key tại `/etc/gleworks/app-keys/release.pem` được
chấp nhận; file bị sửa, thiếu chữ ký, sai `arch`/`profile`, chứa symlink hoặc
đường dẫn `..` bị từ chối trước khi giải nén. Bản cũ được giữ để rollback, dữ
liệu app được giữ khi Remove. Nếu USB có nhiều bản của cùng app, bản cao nhất
được chọn; thư mục không quyết định thứ tự cài.

Lệnh bảo trì (root): `gw-appctl list`, `gw-appctl verify FILE`,
`gw-appctl install FILE`, `gw-appctl remove ID`, `gw-appctl scan`.

Đóng gói trên host:

```sh
python3 tools/build/gwapp.py build --source /path/app-dir --out /path/name.gwapp
```

Private key chỉ nằm trên host tại `work/private/app-keys/release.key`; không
chép khóa riêng lên Kindle hoặc vào gói public.

## Giữ dữ liệu nghiên cứu ở host

Từ bản reader v4, boot log nằm ở `/run/persistent-boot.log`, reader log ở
`/run/reader.log`. Helper shutdown đã kiểm chứng để lại một bản ghi chuyển trạng thái ngắn;
lần boot Alpine kế tiếp xóa bản ghi đó. Không tích lũy lịch sử shutdown
trên FAT. Log RAM mất sau reboot; thu qua USB vào repo trước khi reboot nếu cần
điều tra. `boot.enabled` và `boot.pending` vẫn phải ở FAT vì là trạng thái an
toàn boot, không phải lịch sử. Giữ hai file này theo đúng cơ chế bootstrap.

Thư viện mới có thể trống sau dọn. Chép sách thật bằng `put-documents`; không
đưa ghi chú reverse engineering, ảnh root dự phòng hoặc log thử nghiệm vào
Kindle. Các dữ liệu đó nằm tại repo host.


### Bóng mờ E-ink (bản sửa sau v4)

Người dùng xác nhận bóng mờ hết hoặc giảm rõ sau lệnh `fbink -W GC16 -f -w -s`
qua console native. Đã áp dụng trực tiếp: GC16 flashing khi khởi chạy reader,
`full_refresh_count = 1` và `night_full_refresh_count = 1` để refresh mỗi trang
(thay mặc định 6). Màn hình sẽ chớp thường xuyên hơn. Nguồn build đã đồng bộ;
archive v4 đã phát hành trước thay đổi này và không chứa bản sửa.
Log kiểm tra trên host: `work/private/evidence/cleanup/ghosting-refresh-fix.log`.
Ảnh framebuffer không chứng minh hết bóng mờ trên tấm nền; kết luận về tấm nền
ở đây dựa trên xác nhận của người dùng sau full refresh, chưa phải kiểm tra
đọc nhiều trang sau khi đổi cấu hình.


### Tối ưu dung lượng lần 1 — bản dịch giao diện

Đo trên Kindle trước dọn: RAM ngoài buffers/cache khoảng 42 MiB trên 503 MiB,
KOReader RSS khoảng 20 MiB. Cache là bộ nhớ có thể thu hồi, không phải bloat;
không ép drop_caches hoặc thêm swap. Root đã tối giản, không chạy dịch vụ Amazon.

Đã giữ `vi`, `en_GB`, chuỗi tiếng Anh mặc định và giấy phép; bỏ 59 file dịch
còn lại sau khi so SHA256 với nguồn trên host và kiểm tra lại archive.
Thư mục l10n giảm từ 18,335 KiB xuống 975 KiB, tiết kiệm 17,360 KiB bên trong
root. Ảnh system.ext3 vẫn 256 MiB: phần vừa giải phóng dùng được trong Alpine,
chưa trở thành dung lượng trống cho sách trên FAT. Chưa thu nhỏ ảnh đang chạy.
Không bỏ font vì có thể làm mất ký tự trong sách; giữ trình quản lý gói,
thư viện TLS, thiết bị cảm ứng và USB cứu hộ.

Lặp lại trên một máy chưa dọn bằng nguồn overlay phù hợp:

```sh
python3 tools/maintenance/trim-reader-locales.py --archive /path/on/host/unused-translations.tar.gz
```

Archive và evidence của lượt này ở `work/private/evidence/cleanup/unused-reader-translations.tar.gz`,
`locale-trim.log` và `locale-trim-runtime.log` trên host. Nguồn prepare-image.py
đã lọc các bản dịch dư cho lần build sau; archive v4 cũ chưa chứa thay đổi này.

### Tối ưu dung lượng lần 2 — snapshot cứu hộ và kích thước ảnh

Đo trên máy ngày 2026-11-01: `/mnt/us` chỉ còn 871 MiB trống. Nguyên nhân không
phải root phình to mà là 7 bản sao cứu hộ nằm trong `/mnt/us/gleworks-alpine`:
6 × 256 MiB `system-*.ext3` và 1 × 450 MiB `p1-current-20261031.ext3`. Mọi bản
đã có bản sao host khớp SHA256 (xem `work/private/evidence/`) nên đã xóa khỏi
FAT sau khi kiểm tra kích thước. Sau đó root image được thu nhỏ 256 → 176 MiB
(bỏ Python, gdb, debug symbols, cache apk và log tailscale cũ; e2fsck 0 lỗi) và
deploy lại; boot mới `88026376…`, `gw-system`/`gw-shell`/UI chạy bình thường.
Bản 256 MiB gốc giữ trên host tại
`work/private/maintenance/storage-20261101/system-current-256.ext3`.

Userstore còn **3.007.888 KiB (2.87 GiB) trống**, tức thêm 2.02 GiB cho sách so
với ban đầu. Không xóa `system.ext3`, `boot.enabled`, `boot.pending`, log hay
tài liệu.

Quy tắc từ nay:

```sh
# Trên host, trước khi xóa bất kỳ ảnh cứu hộ nào:
sha256sum /mnt/us/gleworks-alpine/system-*.ext3 /mnt/us/gleworks-alpine/p1-current-*.ext3
# So hash với bản đã lưu trong work/private/; chỉ xóa khi khớp tuyệt đối.
```

`deploy-root` sẽ từ chối ghi nếu userstore không đủ chỗ cho ảnh mới cộng 64 MiB
headroom, và in danh sách ảnh còn giữ cùng dung lượng trống sau cài. Khi danh
sách dài, chép ảnh cần giữ về host rồi xóa bớt trên FAT.

Build mới không còn cố định 256 MiB: `image_size_mib()` tính từ tổng byte của
root cộng 15% và 32 MiB headroom, làm tròn 8 MiB, tối thiểu 128 MiB. Kích thước
được ghi vào `image_mib` trong `candidate.json`; `clean_image` chấp nhận
64 MiB–1024 MiB. Ảnh đang chạy đã là 176 MiB sau lượt deploy 2026-11-01.
Lưu ý: `pw3-build.py` chỉ dựng Alpine + KOReader, **không** có service/UI/OpenRC;
không deploy thẳng output đó. Dùng đường `pw3-os-trial.py` hoặc thu nhỏ ảnh đang
chạy như lượt này. Bằng chứng:
[PW3-STORAGE-OPTIMIZATION-2026-11-01.md](../evidence/PW3-STORAGE-OPTIMIZATION-2026-11-01.md).
