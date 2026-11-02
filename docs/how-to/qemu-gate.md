---
diataxis: how-to
status: current
updated: 2026-11-02
verified_build: none
supersedes: none
audience: maintainer
---

# QEMU local gate — Linux x86-64 (AMD64)

Mục tiêu: mọi thay đổi phải qua build + test trên máy local **Linux x86-64**
trước khi build ARM và deploy lên PW3. Tài liệu này là bước chuẩn bắt buộc,
không thay thế nghiệm thu phần cứng.

## Điều kiện host

- Linux, kiến trúc **x86-64 / AMD64 / 64-bit**. Script từ chối host khác.
- `python3`, `make`, `cc`.
- `qemu-arm` (QEMU user-mode) để chạy binary ARM trên host x86-64.
- Toolchain ARM private và prepared root trong `work/` (không commit).
- Không cần KVM, không cần kết nối thiết bị.

## Không gian độc lập

`vm/` là workspace riêng:

```text
vm/
  README.md          hướng dẫn ngắn
  check-host.sh      gate Linux/x86-64 + make check
  qemu-arm/
    run.sh           build & chạy test ARM bằng qemu-arm
    test-signature.c test chữ ký/fingerprint trên ABI ARM
  images/            image VM tự cung cấp (ignored)
  results/           log giữ lại tuỳ chọn (ignored)
```

Không đưa image, rootfs hay key private vào git.

## Quy trình chuẩn

```sh
# 1. Xác nhận host và chạy toàn bộ host suite
./vm/check-host.sh

# 2. QEMU target gate: policy ARM, signature/fingerprint ARM, CLI smoke
./vm/qemu-arm/run.sh

# 3. Build binary ARM
env -u CC -u STRIP -u CFLAGS \
  bash tools/build/build-arm-binaries.sh

# 4. Chỉ sau khi 1–3 pass mới deploy qua USB console
```

Có thể chạy gộp:

```sh
make vm-gate
```

`make check` một mình vẫn là gate host; `vm-gate` thêm bước QEMU ARM.

## QEMU gate kiểm tra gì

- `test-power-policy` và `test-stage3` build ARM static, chạy bằng `qemu-arm`.
- `test-signature` build ARM dynamic với `libcrypto.so.3` của image; xác nhận:
  - package đúng chữ ký → `rc=0`
  - package sai key → `signature-invalid`
  - fingerprint trust key được kiểm tra.
- `gw-appctl` ARM khởi động và in usage.

Gate này không kiểm tra EPDC, cảm ứng, nút nguồn, USB gadget hay âm thanh; các
phần đó vẫn cần nghiệm thu trên PW3.

## Full-system VM (tuỳ chọn)

Repo không có kernel PW3 boot được bằng QEMU. Nếu cần VM toàn hệ thống, đặt
kernel/rootfs ARM tự chuẩn bị vào `vm/images/` (ignored) và tự chạy
`qemu-system-arm`. Không dùng ảnh private trong CI hay commit.

## Khi gate fail

- Host sai kiến trúc: chuyển sang máy Linux x86-64.
- Thiếu `qemu-arm`: cài QEMU user-mode.
- Thiếu prepared root: build candidate private hoặc đặt `PW3_ROOT`.
- Signature fail: kiểm tra test key/fingerprint và `libcrypto.so.3` ARM trong root.

Không deploy khi bất kỳ bước nào fail.
