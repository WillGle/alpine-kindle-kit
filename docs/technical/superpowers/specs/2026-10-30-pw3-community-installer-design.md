# Bộ cài Alpine native PW3 cho cộng đồng

Trạng thái: người dùng đã duyệt phạm vi và yêu cầu hoàn thiện bộ cài PW3;
triển khai source candidate trên host, chưa công khai repo hoặc nghiệm thu
first-install mới trên máy stock. Xem CURRENT và bằng chứng host hiện hành.

## Mục tiêu đã thống nhất

Biến bộ công cụ hiện có thành bộ cài Alpine native thay môi trường Amazon.
Bản đầu chỉ hỗ trợ và kiểm chứng Paperwhite 3 đã jailbreak, có quyền root.
Chuẩn bị mã nguồn để công khai và tiếp nhận đóng góp cho model/firmware khác;
không quảng cáo hỗ trợ phần cứng chưa kiểm chứng.

Alpine không cần tài khoản, dịch vụ cập nhật hoặc cơ chế quản lý thiết bị
Amazon để hoạt động. Không khẳng định Amazon có khả năng brick từ xa khi
chưa có bằng chứng, và không hứa thiết bị miễn nhiễm mọi lỗi phần cứng/phần mềm.

## Hướng được chọn

Tiếp tục đường native đã chứng minh: giữ kernel/bootloader, dùng bootstrap
chuyển sang root Alpine với global PID1 riêng và KOReader. Amazon userspace
không tham gia phiên Alpine. Giữ các thành phần fallback/phần cứng cần thiết.

Chroot không đáp ứng mục tiêu thay môi trường Amazon. Thay cả kernel hoặc
bootloader chưa có mức kiểm chứng và cứu hộ tương đương nên ngoài phạm vi.

## Phạm vi bản đầu

- Host Linux trước, tận dụng Python, shell, UDisks và builder hiện có.
  Đây là đề xuất triển khai, chưa phải yêu cầu riêng của người dùng.
- Cấu hình tham chiếu: PW3 Muscat/Wario, firmware 5.16.2.1.1,
  kernel 3.0.35-lab126. Chỉ mở rộng điều kiện khi có bằng chứng tương ứng.
- Một CLI dẫn qua các bước kiểm tra, sao lưu, chuẩn bị, thử, cài và phục hồi;
  không tạo GUI, dịch vụ cloud hay cơ chế tải/chạy mã từ xa.
- KOReader và các sửa refresh/locale hiện hành nằm trong đầu vào release mới.
  Wi-Fi, Tailscale, web upload và suspend hoàn chỉnh là việc riêng, không được
  công bố là tính năng đã có của bộ cài đầu tiên.

## Luồng cài

1. Kiểm tra host, kết nối root được xác thực, model, firmware, kernel,
   partition layout, pin và dung lượng. Sai cấu hình hoặc thiếu đầu vào thì
   dừng trước ghi. Không dò đoán block device và không chạy trên máy đã native
   như thể máy stock mới.
2. Capture module/thành phần cần thiết và sao lưu phục hồi về host; kiểm tra
   hash, khả năng đọc và đủ bộ dữ liệu. Phân biệt dữ liệu riêng với source.
3. Dựng stage và root image sạch trong thư mục riêng từng thiết bị/lượt cài.
   Sinh identity mới cho bộ guardian/control/bootstrap đồng bộ; không dùng
   token, khóa hay cấu hình riêng của thiết bị tham chiếu. Không sửa identity
   của bộ đang chạy. Build phải chỉ rõ nguồn, phiên bản và hash đầu vào.
4. Cài dispatcher thử có giữ stock init. Thử đường quay lại stock, Alpine
   one-shot, rồi lỗi khởi động có chủ ý và sửa root qua USB rescue độc lập.
   Các bước phá ảnh chỉ được tác động bản thử, không backup hoặc ảnh duy nhất.
5. Thu bằng chứng gắn với đúng thiết bị, artifact và lượt thử: nhận diện USB,
   nonce mới, PID1/root, hash và return/rescue. Thiếu gate không được promote;
   sửa gate bằng tay không phải đường được hỗ trợ. Mất điện/mất kết nối phải
   có hướng dẫn nhận diện trạng thái, không tự lặp thao tác ghi.
6. Chuyển mặc định sang bootstrap bằng đường đã kiểm chứng, giữ bản phục hồi;
   boot lại và xác minh Alpine, reader, USB rescue và quay lại Alpine.
   Không tự xóa dữ liệu Amazon/người dùng như điều kiện cài thành công.

Mỗi lần chỉ một phiên console/maintenance. Không export FAT khi root image
trên FAT đang dùng. Không flash bootloader/kernel, sửa IDME, Factory Reset
Amazon hoặc dùng reboot cưỡng bức để xử lý lỗi UI.

## Tổ chức và phục hồi

Tận dụng native-kit, bootstrap, native-rootfs-persistent và native-display;
chỉ bổ sung phần điều phối và bỏ phụ thuộc đường dẫn/hash riêng khi cần.
Trạng thái cài và bằng chứng lưu ở host, tách theo thiết bị; log không chứa
khóa/token. Không coi marker tồn tại hay ảnh E-ink là chứng minh OS còn sống.

Phân biệt ba đường: stock chưa cài, đã native cần cập nhật, và USB rescue.
CLI phải nhận diện và từ chối lệnh không phù hợp. Tiếp tục sau gián đoạn phải
đọc lại trạng thái thiết bị và hash, không chỉ tin trạng thái host.

Phục hồi root qua USB là khả năng đã có trên máy tham chiếu. Khôi phục Amazon
toàn phần sau dọn dữ liệu chưa được kiểm chứng; không gọi là uninstall đã
chứng nhận. Bộ cài phải ghi rõ ranh giới này và lưu đủ đầu vào trước chuyển đổi.

## Chuẩn bị công khai và đóng góp

Tạo bộ source sạch có danh sách file cho phép; loại backups, private evidence,
khóa, token, serial và dữ liệu người dùng. Không public nguyên workspace.
Kiểm tra giấy phép/provenance từng thành phần trước khi đưa vào release;
thành phần chưa rõ quyền phân phối được capture từ máy người dùng hoặc lấy
từ nguồn upstream hợp lệ, không mặc định gói binary hiện tại được public.

Tài liệu tiếng Anh cho người đóng góp, giữ hướng dẫn tiếng Việt hiện có.
Ghi ma trận model/firmware/kernel và mức bằng chứng; ngoài cấu hình đủ gate
thì từ chối cài. Đóng góp model mới cần nhận diện riêng, nguồn/giấy phép,
backup, boot và USB recovery trên đúng model; không thêm framework plugin
trước khi có nhu cầu cụ thể. Việc tạo repo công khai/upload release chỉ làm
sau khi có bản sạch để xem xét và đích công khai được xác định.

## Tiêu chí nghiệm thu

- Gói giải nén sạch dùng được với hướng dẫn và dependency đã liệt kê,
  không phụ thuộc đường dẫn home/Nix store hoặc bí mật từ workspace tác giả.
- Kiểm tra rẻ hiện có, syntax, CLI, manifest/hash và filesystem qua; không
  thêm maintained tests khi chưa được yêu cầu. Dùng probe disposable nếu cần.
- Chứng minh từ stock đã root tới Alpine và recovery trên PW3. Máy tham chiếu
  hiện đã native: không coi triển khai root lại là bằng chứng cài lần đầu;
  nếu chưa có máy stock phù hợp, đánh dấu nghiệm thu này còn chờ, không hạ gate.
- Có bằng chứng reader thực tế sau sửa refresh, cùng release mới đúng nguồn.
- Gói public không chứa dữ liệu riêng; giấy phép và thành phần không phân phối
  được được ghi rõ. Không đổi hay viết đè release v4 lịch sử.

## Phần để implementer quyết định

Tên lệnh và cấu trúc metadata tối thiểu, cách tái dùng SSH capture đã pin,
và cách đóng gói toolchain trên Linux; ghi lại lựa chọn trong kế hoạch.
Không mở rộng sang model khác, khai thác jailbreak, GUI hoặc kernel mới.

## Trạng thái hiện hành

Đọc guides/CURRENT.vi.md và evidence/ACCEPTANCE.json trước khi thao tác thiết bị.
Thiết kế này không thay thế bằng chứng hiện tại và không chứng nhận phần cứng
mới. Cập nhật CURRENT/ACCEPTANCE khi trạng thái thực tế đổi; append BACKLOG.
