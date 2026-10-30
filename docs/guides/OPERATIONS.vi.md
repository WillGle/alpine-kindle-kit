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
