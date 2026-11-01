# PW3 storage optimization — 2026-11-01

## Kết quả

| Chỉ số | Trước | Sau |
| --- | --- | --- |
| Userstore `/mnt/us` trống | 892,304 KiB (871 MiB) | 3,007,888 KiB (2.87 GiB) |
| Dung lượng thêm cho sách | — | 2,115,584 KiB (2.02 GiB) |
| Root image | 256 MiB | 176 MiB (184,549,376 byte) |
| Repo working tree | 61 GiB | 20 GiB |
| Host `/` trống | 85 GiB | 125 GiB |

Thiết bị: PW3 `G090KB`, boot trước `ae4fa08e-4d3c-468e-b339-387773498842`,
boot sau deploy `88026376-275c-4d1c-918b-c24d1fdf8f0a`, Alpine 3.24.3,
root image `/dev/loop1` ext3 read/write.

## Nguyên nhân trên máy

`/mnt/us/gleworks-alpine` chứa 7 ảnh cứu hộ tích lũy từ các lần deploy/rescue.
Không có ảnh nào còn là trạng thái boot hiện tại:

| Tệp | Byte | SHA256 | Bản host khớp |
| --- | --- | --- | --- |
| `p1-current-20261031.ext3` | 471859200 | `e50079c10169…` | `work/private/evidence/phase4a-rescue-20261031/images/` |
| `system-after-hotplug-20261030.ext3` | 268435456 | `ad86fca45f9f…` | `phase4a-rescue-20261031/images/` |
| `system-before-candidate-94462d39be54.ext3` | 268435456 | `f66914c948f1…` | `phase4a-rescue-20261031/images/` |
| `system-before-os-trial-20261030.ext3` | 268435456 | `0678b9927992…` | `phase4a-rescue-20261031/images/` |
| `system-before-rca-20261030.ext3` | 268435456 | `fa11773957ce…` | `phase4a-rescue-20261031/images/` |
| `system-pre-hardening-20261031.ext3` | 268435456 | `34147cd303b9…` | `phase4c-20261031/images/system-rollback-pre-hardening-20261031.ext3` |
| `system-restoration-candidate-20261031.ext3` | 268435456 | `40ff83f3e9b4…` | `phase4b-20261031/images/system-post-restore-20261031.ext3` |

Các thư mục evidence host nêu trên đã được nén ngày 2026-11-01 trong
`work/private/evidence/archive-20261101/` (giải nén `*.tar.zst` để truy cập
đúng đường dẫn cũ).

Hash lấy trực tiếp trên thiết bị bằng `sha256sum`; hash host lấy từ bản lưu
tương ứng. Sau khi khớp, từng tệp được kiểm tra kích thước rồi xóa bằng
`rm -f` và `sync`. Giữ nguyên `system.ext3`, `boot.enabled`, `boot.pending`,
các log nhỏ và `rca-hotplug-20261030.sdr`.

Manifest trước/sau: `work/private/evidence/device-storage-20261101/prune-before.json`.

## Thay đổi mã nguồn

- `tools/lib/pw3_common.py`: `clean_image()` chấp nhận ảnh 64 MiB–1024 MiB thay
  vì bắt buộc đúng 256 MiB; thêm `image_size_mib()` tính kích thước ảnh từ
  tổng byte root + 15% + 32 MiB headroom, làm tròn 8 MiB, tối thiểu 128 MiB.
- `tools/build/pw3-build.py`, `tools/build/pw3-os-trial.py`: dùng
  `image_size_mib()` và ghi `image_mib` vào manifest.
- `tools/maintenance/kindle-linux.py`: `deploy-root` từ chối ghi khi userstore
  không đủ ảnh mới + 64 MiB, và in các ảnh cứu hộ còn giữ cùng dung lượng trống
  sau cài.

## Thu nhỏ và deploy ảnh đang chạy

`tools/build/pw3-build.py` chỉ tạo Alpine + KOReader, **không** có `gw-system`,
`gw-shell`, `gwctl`, OpenRC hay Wi-Fi/tailscale; deploy thẳng sẽ mất UI. Vì vậy
không deploy candidate 128 MiB vừa build. Thay vào đó giữ nguyên nội dung máy
đang chạy và chỉ thu nhỏ filesystem:

1. Vào rescue USB (`kindle-linux.py rescue`), copy `system.ext3` 256 MiB về host
   (`system-current-256.ext3`, SHA256 `e63900ae…`).
2. `e2fsck -f` trên bản copy: 0 lỗi, 3.079 tệp.
3. Xóa phần không cần cho runtime, sau khi đối chiếu không có service nào dùng:
   Python 3.14 (`/usr/lib/python3.14`, `libpython*`, `python*`, ~20 MiB),
   `gdb`/`gdbserver`/`gdb-add-index` (~6.7 MiB), `/usr/lib/debug` (2.9 MiB),
   `/var/cache/apk` (2.8 MiB), `tailscaled.log1/2.txt` (~6 MiB).
4. `e2fsck -f` lại 0 lỗi; `resize2fs` còn **176 MiB** (180.224 block 1 KiB),
   còn 29.503 KiB trống bên trong.
5. Deploy bằng `kindle-linux.py deploy-root`, hash
   `025808ea94c84f1b1cecefb73ae2631b0f32a8fcb333e2964bc67ba7fb88dcc0`;
   verify readback, giữ bản 256 MiB thành `system-before-shrink-20261101.ext3`.
6. Boot lại: boot ID mới `88026376…`, `gw-system --supervise` và `gw-shell`
   chạy, UI vào render loop, cảm ứng `/dev/input/event1`, Wi-Fi driver nạp;
   `df /` = 174.535 KiB, dùng 145.032, trống 29.503.
7. Đối chiếu SHA256 bản backup trên FAT với bản host (`e63900ae…` khớp), rồi xóa
   backup khỏi FAT. Userstore còn 3.007.888 KiB (2.87 GiB) trống.

Bản 256 MiB gốc giữ trên host tại
`work/private/maintenance/storage-20261101/system-current-256.ext3`. Ảnh 176 MiB
đã deploy có hash lúc cài `025808ea…`; sau khi chạy, nội dung ext3 thay đổi nên
hash đọc lại khác — đây là hành vi bình thường của root read/write.

## Dọn workspace host

`work/` là vùng scratch không được git theo dõi. Đã xóa 119 artifact sinh tự
động (ảnh emulator/trial `*.img`, `*.ext3` lớn, và 4 cây build cũ) sau khi ghi
manifest `work/archive/PRUNED-20261101.json`. Giữ `work/sources`,
`work/toolchains`, `work/downloads`, `work/images/releases`, candidate build
hiện tại và toàn bộ `work/private`. Các bản sao y hệt trong
`work/private/evidence` được hardlink để giữ nguyên đường dẫn nhưng chỉ chiếm
một bản trên đĩa (10 tệp, 2.47 GiB biểu kiến).

## Giới hạn

- Ảnh 176 MiB giữ nguyên toàn bộ binary đang chạy; chỉ bỏ Python, gdb, debug
  symbols, cache apk và log tailscale cũ. Nếu sau này cần Python trên máy phải
  cài lại bằng apk (ảnh còn ~29 MiB trống).
- Chưa đo lại dung lượng sau khi đọc sách hoặc sau update app.
- Candidate `pw3-build.py` 128 MiB còn trong `work/build/pw3-reader-storage-20261101`
  nhưng **không dùng để deploy**: thiếu service/UI/OpenRC. Cần sửa builder hoặc
  dùng đường `pw3-os-trial.py` trước khi coi nó là ảnh OS đầy đủ.
