---
status: current
updated: 2026-11-05
verified_build: none
supersedes: none
audience: maintainer
---

# PW3 native Alpine vs stock + WinterBreak2 — benchmark 2026-11-04

Đây là so sánh thực nghiệm trên cùng một PW3 (G090KB, firmware 5.16.2.1.1,
kernel 3.0.35-lab126), không phải benchmark trong phòng thí nghiệm. Mục tiêu là
đối chiếu tài nguyên và thời gian mở reader giữa hai cấu hình. Số liệu được
tính lại từ log gốc ngày 2026-11-05; không phải một lần đo mới trên thiết bị.

## Cấu hình đo

| Nhánh | Thành phần | Ghi chú |
| --- | --- | --- |
| Native | Alpine 3.24.3, `gw-system` + `gw-shell`, không Amazon framework | Build `990061d1630da5de49bd4c5df3fdd6b00c62731115147e8b321e93b67ce2b069`, boot `6986b82c-62b7-4ef5-ae98-390e55cf339f` |
| Stock + JB | Stock 5.16.2.1.1 + WinterBreak2 + KUAL + KOReader 2026.07.1 | Boot ID không ghi lại; đo sau khi cài KUAL/KOReader |

Native đo bằng `tools/maintenance/pw3-audit.py --sample-seconds 60 --full`.
Stock+JB đo bằng một KUAL extension chạy cùng mẫu `/proc` trong 60 giây, với
KUAL đang mở. Hai phép đo không hoàn toàn giống nhau về ngữ cảnh UI, nên các
số dưới đây là xu hướng có kiểm chứng, không phải số tuyệt đối để so từng KB.

## Kết quả

| Hạng mục | Native Alpine | Stock + WinterBreak2 + KOReader |
| --- | ---: | ---: |
| RAM theo cùng công thức, loại buffer/cache | **43.9 MiB** | **232.4 MiB** |
| `AnonPages` | **3.1 MiB** | **179.2 MiB** |
| CPU idle (~60 s) | **96.2%** | **89.4%** |
| CPU bận, `100 − idle` | **3.8%** | **10.6%** |
| CPU user / system | 1.3% / 2.5% | 9.0% / 1.2% |
| CPU governor | performance, 996 MHz | ondemand, 996 MHz |
| Root filesystem đã dùng | **139.5 MiB** / 170.4 MiB | **383.5 MiB** / 437.4 MiB |
| Process nặng nhất | `gw-shell` 2.3 MiB | `cvm` 118 MiB, `KPPMainApp` 51 MiB, `Xorg` 11 MiB |
| Mở KOReader | **~3 s** (số cũ, build 2026-11-01) | **~4–5 s** (bấm giờ thủ công) |
| Đọc sách | KOReader | KOReader |

### RAM

Native:

```text
MemTotal   514724 kB
MemFree     99692 kB
Buffers    158916 kB
Cached     211192 kB
AnonPages    3224 kB
Slab        14652 kB
```

Stock+JB:

```text
MemTotal   514724 kB
MemFree     37752 kB
Buffers    104840 kB
Cached     134204 kB
AnonPages  183516 kB
Slab        17828 kB
total user 242424 kB
RSS sum    102592 kB
```

Tính cả hai cấu hình bằng cùng công thức:

```text
RAM = MemTotal − MemFree − Buffers − Cached
Native = (514724 − 99692 − 158916 − 211192) / 1024 = 43.87 MiB
Stock  = (514724 − 37752 − 104840 − 134204) / 1024 = 232.35 MiB
```

Native dùng ít RAM hơn khoảng **81.1%** theo công thức này; stock dùng khoảng
**5.3 lần** native. Đây không phải `MemAvailable` hoặc tổng working set của ứng
dụng. `AnonPages` tương ứng ~3.1 và ~179.2 MiB; phần lớn chênh lệch gắn với
Amazon framework (`cvm`, `KPPMainApp`, `Xorg`, `mesquite`, `kfxreader`,
`webreader`), không phải KOReader.

Số cũ **32 MiB / ~7 lần** không khớp log và được thay bằng phép tính trên.
Dòng RSS cũ cũng được bỏ vì đã trộn top process với tổng RSS; không dùng nó
để kết luận dung lượng RAM toàn hệ thống.

### CPU

Native idle 96.2% trong 62.1 giây; stock+JB idle 89.4% trong 60.1 giây. Native
dùng CPU bận 3.8% so với 10.6%, thấp hơn khoảng **64.3%** trong mẫu này
(stock/native ~2.8 lần). Đây là tải nền, không phải tốc độ thực thi ứng dụng. Tuy nhiên native đang dùng governor `performance`
ở 996 MHz, còn stock dùng `ondemand`; vì vậy **không thể kết luận native tiết
kiệm pin hơn**. Chưa đo dòng tiêu thụ hoặc thời lượng pin.

### Lưu trữ

Native dùng root image 176 MiB, root đã dùng **139.5 MiB**; stock dùng root
partition 450 MiB, root đã dùng **383.5 MiB** theo `df -k` (392674 KiB).
Root native dùng ít dung lượng hơn **63.6%**, stock/native ~2.75 lần. Đây chỉ
là dung lượng root: KOReader của stock nằm trên p4, còn native có KOReader
trong root; không coi đây là tổng dung lượng toàn bộ OS, ứng dụng và dữ liệu.

### Đọc sách

Cả hai dùng KOReader, cùng panel và kernel/driver EPDC. Ghi nhận thủ công
cho thấy native mở KOReader ~3 giây, stock ~4–5 giây; không phải phép đo đồng
điều kiện hoặc cùng build. Mẫu ~3 giây được ghi trong
[`latency-audit-2026-11-01.md`](latency-audit-2026-11-01.md).

Chưa đo đối chiếu touch-to-visible, cuộn, lật trang hoặc độ ổn định độ trễ.
Waveform ~0.66 giây cho cập nhật lớn không đủ để kết luận trải nghiệm hai hệ
là tương đương. **Chưa chứng minh định lượng native mượt hơn toàn diện.**

## Công sức cài đặt

Trong phiên thực nghiệm này, đường native yêu cầu build host, backup p1/modules,
nhiều bước cài có kiểm soát, nhiều lần reboot và ghi p1. Đường jailbreak ban đầu
dùng LanguageBreak gặp lỗi Wi-Fi stock (`wpa_supplicant` SIGILL), phải cài lại
firmware official; sau đó WinterBreak2 + KUAL + KOReader cài nhanh hơn nhiều và
không cần build host.

Đây là quan sát định tính về công sức cài đặt, không phải số đo hiệu năng
hoặc cơ sở để kết luận nên ngừng phát triển native.

## Kết luận và hướng phát triển — xác nhận 2026-11-05

- **Tiếp tục phát triển Alpine Reader native** theo quyết định của người dùng.
  Alpine thay userspace Amazon; giữ kernel/bootloader hiện có. KOReader là
  ứng dụng đọc sách trong hệ này.
- Log xác nhận RAM thấp hơn rõ rệt, root gọn hơn và tải CPU nền thấp hơn.
  Ghi nhận mở KOReader có lợi cho native nhưng chưa phải benchmark đồng điều
  kiện. Lợi ích native bao gồm tài nguyên và phản hồi, không chỉ quyền kiểm
  soát hệ thống; độ mượt tổng thể vẫn cần đo.
- Amazon OS + jailbreak là phương án phục hồi/đối chiếu, không thay thế hướng
  sản phẩm Alpine Reader. Kết luận trước đây rằng jailbreak thắng cost/benefit
  cho người đọc sách là đánh giá chủ quan, không được số liệu này chứng minh.
- Thiết bị đang mắc ở bootstrap p1; cần phục hồi boot trước khi tiếp tục đo
  hoặc triển khai trên máy. USB rescue chỉ export p4; phục hồi p1/Amazon chưa
  được chứng nhận. Giữ nguyên các kết quả phần cứng cũ dưới dạng lịch sử,
  không coi chúng là xác nhận thiết bị hiện vẫn boot được.

## Giới hạn

- Stock+JB đo với KUAL đang mở; native đo ở Home. Không phải cùng một scene.
- Số mở KOReader của native lấy từ evidence 2026-11-01, không đo lại trên build
  hiện tại; số JB là bấm giờ thủ công, sai số ~0.5–1 s.
- Chưa đo boot time, page-turn latency riêng, hay battery drain.
- Không có build hash cho nhánh stock+JB; đây là stock firmware + WinterBreak2 +
  KUAL + KOReader, không phải một build GleWorks.

## Nguồn số liệu

Log gốc được giữ nguyên trên host, thuộc vùng private:

- Native: `work/private/bench/2026-11-03-native/audit.json` và `audit.raw.txt`.
- Stock: `work/private/bench/2026-11-03-jb/bench-stock-20261104-014815.txt`.
- Rescue probe: `work/private/recovery-20261104-usb/probe-before.json`,
  `probe-after.json` và `REPORT.vi.md`.

Các file này không được đóng gói thành bằng chứng công khai; bảng trên là
phân tích lại dữ liệu đã lưu, không phải kiểm chứng runtime mới.
