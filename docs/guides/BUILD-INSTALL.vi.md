# Dựng và cài Alpine native trên PW3

Đọc [CURRENT.vi.md](CURRENT.vi.md) trước khi thao tác máy và
[STRUCTURE.md](STRUCTURE.md) để biết bố cục nguồn/workspace.
Bộ cài mới vẫn là source candidate: kiểm tra host không thay cho nghiệm thu
first-install trên PW3 stock. Máy tham chiếu hiện đang Alpine + KOReader;
đổi bố cục repo không thay đổi hệ đang chạy.

## Nguồn, dữ liệu local và phiên bản

- `os/alpine/`: phiên bản/checksum Alpine và overlay tối thiểu.
- `os/devices/pw3/`: profile, kernel config, bootstrap, guardian và runtime PW3.
- `apps/koreader/`: lớp reader riêng, không thuộc base OS.
- `tools/`: build, cài, bảo trì và đóng gói trên host.
- `work/`: download, source upstream, toolchain, build, image và dữ liệu riêng.
- `dist/`: gói source mới và checksum. Gói cũ giữ nguyên tại
  `work/archive/releases/`; ảnh v4 giữ tại `work/images/releases/pw3/`.

Lệnh chạy được từ cwd khác bằng đường dẫn tuyệt đối tới công cụ. Dùng
`--work-dir /path/to/local-work` để chọn ổ/thư mục làm việc; mặc định `work/`
cạnh thư mục nguồn. Các `--output`, `--session`, đường dẫn evidence/backup
được chỉ định rõ sẽ ghi đúng vị trí người dùng chọn.

## Dựng base OS hoặc bản có reader

Hướng dẫn đầu vào, dependency và lệnh đầy đủ tại
[community/BUILD.md](../community/BUILD.md). `pw3-build.py` mặc định dựng
**base Alpine không KOReader**, có runtime PW3 và console USB vật lý.
Thêm `--variant reader --reader /path/to/clean-koreader` khi cần bản reader.
`--reader` riêng lẻ không tự chọn variant để tránh dựng nhầm.

```sh
python3 tools/build/pw3-build.py --help
```

Builder dùng archive Alpine đã pin, module capture và backup p1 của đúng máy,
launcher riêng cùng toolchain/kernel-build/musl/headers được chỉ định tường minh.
Backup p1 vẫn cần cho provenance của bộ cài; base variant không trích thư viện
reader từ đó. Mỗi build tạo identity mới đồng bộ bootstrap/guardian/control.
Không thay lẻ một component vào máy đang chạy; không dùng root image đang mount.

Nếu không chỉ định `--output`, kết quả nằm trong thư mục mới
`WORK/build/pw3-VARIANT-ID`. Dữ liệu build và candidate là riêng tư; không
đưa cả thư mục này lên repo public. Build ARM/QEMU/e2fsck không phải nghiệm
thu boot/cứu hộ/reader trên máy thật. Chưa chứng minh whole-toolchain clean-room.

## Cài lần đầu từ Amazon OS

Chỉ profile `G090KB`/0KB, firmware 5.16.2.1.1, kernel 3.0.35-lab126,
layout và init wrapper đã biết; cần jailbreak/root sẵn. Hướng dẫn đầy đủ tại
[community/README.md](../community/README.md). Không dùng đường first-install
cho máy đã chuyển native.

```sh
python3 tools/install/pw3-control.py --help
python3 tools/install/pw3-install.py --help
```

Luồng bắt buộc: kiểm tra → backup host → bind candidate → dispatcher → stock
return → readonly native trial → ảnh lỗi và USB repair → stock return với nonce
→ promote → native → clean rescue → native. Thiếu gate thì dừng; không sửa
session.json hoặc xóa boot.pending để khai đạt. Khi ngắt giữa thao tác ghi,
đọc status/reconcile và hướng dẫn phục hồi, không tự chạy lại mù.

## Bảo trì máy đã native

```sh
python3 tools/maintenance/kindle-linux.py inspect --output /tmp/pw3-inspection.log
python3 tools/maintenance/kindle-linux.py rescue
python3 tools/maintenance/kindle-linux.py deploy-root \
  /path/to/clean-system.ext3 --sha256 HASH_TRONG_MANIFEST \
  --backup-name system-before-upgrade.ext3 --evidence /tmp/pw3-deployment.json
```

Đường bảo trì được giữ nguyên hành vi kiểm tra USB/hash/ảnh sạch và không flash
bootloader/kernel/p1. Chỉ một console/maintenance tại một thời điểm. Không
export FAT khi Alpine còn dùng root image bên trong. Xem
[OPERATIONS.vi.md](OPERATIONS.vi.md) cho đọc sách, chép sách và cứu hộ.

## Gói source và dữ liệu lịch sử

```sh
python3 tools/package/public-source.py /path/to/new-source.tar.gz
python3 tools/package/verify-package.py /path/to/new-source.tar.gz \
  --extract-to /path/to/new-check-directory
```

Gói dùng allowlist `os/`, `apps/`, `tools/` và tài liệu cộng đồng; không lấy
work/archive, image, token, khóa, capture, backup hoặc thư viện sách.
`kindle-linux.py package` ủy quyền cùng bộ đóng gói này và từ chối kèm private
image. Phân phối binary/vendor component cần kiểm tra quyền riêng, không suy
rằng gói private hiện tại có thể public.

Mã thử cũ ở `archive/experiments/`, app Amazon/JB cũ ở
`archive/amazon-appliance/`. Chúng là snapshot lịch sử, không phải lệnh cài
hiện hành. Nội dung archive/log/release cũ không được viết lại; tra
[bảng chuyển](../evidence/layout-moves.json) để tìm đường dẫn cũ. Bản nguồn
trước khi sửa, inventory và hash đối chiếu giữ trong `work/private/migrations/`.
