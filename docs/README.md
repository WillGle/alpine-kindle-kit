# Tài liệu Kindle Alpine

Toàn bộ tài liệu dự án được lưu tại đây, không có bản sao hay trang chuyển
hướng ở thư mục mã nguồn. Tài liệu upstream/giấy phép đi kèm mã bên thứ ba và
nội dung bên trong các release archive cũ được giữ nguyên.

- [BACKLOG.md](BACKLOG.md): backlog duy nhất, chứa việc còn mở và toàn bộ lịch sử
  nhập từ execution/verification/plan cũ. Mỗi thay đổi mới append vào đây.
- [guides/CURRENT.vi.md](guides/CURRENT.vi.md): bắt đầu session, trạng thái gần
  nhất, ranh giới kiểm chứng và bản đồ bằng chứng.
- [guides/OPERATIONS.vi.md](guides/OPERATIONS.vi.md): sử dụng, USB, đọc sách, cứu hộ.
- [guides/BUILD-INSTALL.vi.md](guides/BUILD-INSTALL.vi.md): dựng, cài và đóng gói.
- [guides/WORKSPACE.md](guides/WORKSPACE.md): quy ước làm việc trong workspace.
- [community/README.md](community/README.md): bộ cài PW3 cho cộng đồng,
  hướng dẫn tiếng Anh, build và đóng góp; first-install mới chưa nghiệm thu máy thật.
- `technical/`: phân tích và đặc tả theo chủ đề; giữ cấu trúc tên gốc để tra cứu.
- `evidence/`: acceptance, dữ liệu kiểm chứng và dữ liệu đã đưa khỏi Kindle.
  Backup/capture riêng được tách vào work/private/.

Nguồn hiện hành tách tại `os/`, `apps/` và `tools/`; dữ liệu local và artifact
lịch sử nằm trong `work/`, gói source mới tại `dist/`. Xem
[STRUCTURE.md](guides/STRUCTURE.md) và [bảng chuyển đường dẫn](evidence/layout-moves.json).
Backup/capture riêng ở `work/private/evidence/`, không nằm trong docs hoặc gói public.
Các bản ghi lịch sử giữ nguyên nội dung/đường dẫn tại thời điểm tạo; dùng bảng
chuyển để tra cứu. Không sửa lịch sử để giả làm kiểm chứng mới.
