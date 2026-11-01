# App runtime cho GleWorks OS — kế hoạch cập nhật 2026-10-31

- Trạng thái: **đề xuất**, chưa triển khai. Tài liệu này không sửa mã, thiết
  bị hay boot.
- Thay thế phần runtime còn mở của
  [kế hoạch OS 2026-10-30](2026-10-30-minimal-alpine-reader-os.md); không thay
  thế lịch sử đã ghi ở đó.
- Contract chi tiết:
  [APP-SESSION](../specs/2026-10-31-app-session-contract.md).
- Phạm vi: PW3 `G090KB` / `5.16.2.1.1` / kernel `3.0.35-lab126`.

## 1. Vì sao phải cập nhật kế hoạch

Đợt cập nhật 2026-10-31 đã hoàn thành nhiều mốc từng bị coi là "tương lai":

| Năng lực | Trước | Hiện tại | Bằng chứng |
| --- | --- | --- | --- |
| Cài app | đề xuất `.apk` + manifest | `.gwapp` ký RSA/SHA-256, cài/update/remove thật | `docs/evidence/pw3-app-installer-2026-10-31.md` |
| UI quản lý app | chưa có | Applications + App updates dùng thật | `docs/evidence/pw3-app-ui-2026-10-31/` |
| Power | nghiên cứu | mem suspend + RTC wake + evdev path | `docs/evidence/pw3-power-suspend-2026-10-31.md` |
| Nút nguồn | chưa chắc | **đã xác nhận nút vật lý thật**: lock → suspend → wake → Unlock | `docs/evidence/pw3-power-physical-button-2026-10-31.md` |
| Paint/transition | tap ~690 ms, full flash mỗi lần đổi màn | DU4 transition ~76 ms, section isolation, cleanup REAGL | `docs/technical/PW3-PAINT-POLICY.md` |
| Test lặp UI/power | chưa có | `uinput` touch + power injection | `tools/maintenance/pw3-uinput-touch.py`, `pw3-uinput-power.py` |
| Wi-Fi | scan thử | lưu cấu hình, auto-reconnect sau reboot, supervision | `docs/evidence/pw3-wifi-autoconnect-2026-10-31.md` |
| Idle | render/log liên tục | log 0 B/s, 1 EPDC/phút, shell CPU 0.10% | `docs/evidence/PW3-PHASE1-OPTIMIZATION.md` |
| Đo reader | chưa có | peak 52/70/33 MiB; boot→Home ~23 s | `docs/evidence/PW3-PHASE2-ACTIVE-PERF.md` |

Nhưng app do người dùng cài **vẫn chưa chạy được**:

- `APP LAUNCH` chỉ chấp nhận `koreader`; `APP EXIT`/`APP CRASH` trả
  `not-accepted`.
- Shell Applications chỉ có nút Open cho built-in Reader.
- App/Reader chạy **root**, chưa có UID riêng, chưa `RLIMIT_*`, chưa watchdog.
- Reader tự mở `/dev/fb0`; chưa có fd handoff/`libgwdisplay`.
- Crash → Home, OOM, treo, fd leak chưa kiểm chứng.
- Data app nằm trên root ext3; data image mới là tool, chưa nghiệm thu thiết bị.

Vì vậy kế hoạch mới tập trung vào **runtime**, không làm lại installer.

## 2. Mục tiêu và không phải mục tiêu

**Mục tiêu:** app đã cài, không phải Reader, có thể được launch từ OS; chạy
foreground với UID riêng và trần tài nguyên; vẽ/ nhận touch qua hợp đồng; thoát
hoặc crash đều về Home; không ảnh hưởng boot, rescue hay đường đọc sách.

**Không làm trong đợt này:**

- Broker đa cửa sổ, Wayland/X11, compositor.
- Sandbox namespace/seccomp (kernel không có).
- Catalog online, cài từ mạng.
- A/B OS update.
- Hỗ trợ model khác PW3.
- Port KOReader sang `libgwdisplay` (giữ ngoại lệ có ghi rõ).

## 3. Quyết định triển khai

| ID | Quyết định | Lý do |
| --- | --- | --- |
| R1 | Session nằm trong `gw-system`, không thêm daemon | Một chủ sở hữu phần cứng; tránh tranh chấp |
| R2 | Launch theo `id`, entrypoint lấy từ manifest đã cài; không nhận đường dẫn/shell text từ client | Giảm bề mặt lệnh |
| R3 | App mới chạy UID riêng; Reader có thể là ngoại lệ root có thời hạn | DAC là ranh giới duy nhất kernel hỗ trợ |
| R4 | App mới nhận fb/touch qua fd + `libgwdisplay`; không tự mở `/dev/fb0` | Thu hồi quyền bằng đóng fd; không cần broker |
| R5 | Refresh policy nằm trong `libgwdisplay`/service, không trong app | Chống ghosting và tranh chấp |
| R6 | Mọi app foreground đều có timeout launch/stop và watchdog RSS | Không có cgroup; phải tự giám sát |
| R7 | Data app tách khỏi payload; ưu tiên data image khi nghiệm thu | Root ext3 nhỏ, dễ đầy, ext3 writeback |
| R8 | UI chỉ hiện Open khi runtime backend đã đạt cho app đó | Không hứa tính năng chưa có |

`D3` (data image) và `D2` (UID cho Reader) vẫn là quyết định mở trong contract;
kế hoạch này coi chúng là **cổng chặn** trước khi mở launch cho app người dùng.

## 4. Kiến trúc runtime đích

```text
gw-shell (Home) --IPC--> gw-system
                           ├─ app registry (manifest, active version)
                           ├─ session state machine
                           ├─ spawn: setuid/setgid + rlimits + env/fd
                           ├─ monitor: waitpid, timeout, RSS, fd
                           ├─ display/input owner (fd handoff)
                           └─ power/lock owner (SIGUSR1/2, bounded wait)

app process: entrypoint cố định
             ├─ GW_FB_FD / GW_TOUCH_FD
             ├─ GW_APP_ID / GW_APP_DATA / GW_APP_VERSION
             ├─ không mở /dev/fb0, không đọc nút nguồn
             └─ exit 0/1/… hoặc crash → service dọn và vẽ Home
```

Luồng launch:

1. Shell gửi `APP LAUNCH <id>`; service xác thực peer là shell PID, session
   `HOME`, không lock.
2. Service đọc `manifest` từ `active`, kiểm `arch/profile/min_os/entrypoint`.
3. Service dừng nhận input của shell, hoàn tất refresh, chuẩn bị fd.
4. Fork; con `setgid/setuid`, `chdir` data, đặt rlimit/nice/oom_score_adj, đóng
   fd thừa, export env, `execve` entrypoint.
5. Cha ghi nhận PID/UID/thời gian; nếu quá 5 giây chưa có tín hiệu "ready"
   (hoặc process chết) → kill, về Home.
6. Khi app thoát: thu hồi fd, log exit/signal, full refresh, vẽ Home.
7. Khi lock/suspend: gửi `SIGUSR1`, chờ tối đa 2 giây, rồi tiếp tục; resume gửi
   `SIGUSR2` nếu app còn sống.

## 5. Các phase

Mỗi phase có cổng thoát riêng. Không sang phase sau khi cổng trước chưa đạt
hoặc chưa ghi lý do chưa đạt.

### Phase 0 — Chốt lưu trữ và đo lại baseline

**Mục tiêu:** biết chính xác còn bao nhiêu chỗ/RAM trên build hiện tại trước
khi thêm runtime.

- [ ] Đo `df`, `du` của root/appdata, kích thước app đã cài; xác nhận số liệu
      mâu thuẫn giữa baseline Phase 0 (root 76–83%) và ghi chú "còn ~168 MiB".
- [ ] Quyết định `D3`: tiếp tục appdata trên root ext3 với quota, hay chặn
      launch cho đến khi data image được nghiệm thu.
- [ ] Nếu chọn data image: mount thử, shutdown/unmount, rescue, power-loss trên
      ảnh disposable trước; không đụng ảnh đang chạy.
- [ ] Chạy lại AS-01…AS-05 trên build hiện tại; lưu
      `docs/evidence/app-session/as-01..05.*`.
- [ ] Dùng `pw3-uinput-touch.py` và `pw3-uinput-power.py` cho các phép lặp
      UI/power; vẫn cần người xác nhận trải nghiệm vật lý ở mốc cuối.
- [ ] Chốt trần RAM cuối cùng vào contract §10.2.

**Cổng thoát:** có quyết định D3 + số đo baseline + trần RAM; không còn giả
thuyết về dung lượng.

### Phase 1 — Generic launch tối thiểu

**Mục tiêu:** app không phải Reader chạy được, thoát được, về Home, không cần
UID/limit vội.

- [ ] Thêm `APP LAUNCH <id>` cho app đã cài; từ chối ID lạ, app đang foreground,
      máy đang lock, entrypoint sai.
- [ ] Thêm `APP EXIT`/trạng thái `foreground=<id>`; service `waitpid` và tự về
      Home, không respawn.
- [ ] Timeout launch 5 giây; timeout stop 3 giây (`SIGTERM` → `SIGKILL`).
- [ ] Tạo bundle `gwtest` ký thật; entrypoint C nhỏ, chưa cần vẽ đẹp.
- [ ] Shell: nút Open cho app đã cài; chỉ bật khi service trả capability.
- [ ] Chạy 20 chu kỳ launch/exit; boot ID không đổi; không rò process/fd.

**AS:** AS-08, AS-09, AS-10, AS-12.
**Cổng thoát:** `gwtest` chạy/thoát 20/20, Home trở lại, không respawn.

### Phase 2 — UID, quyền và giới hạn tài nguyên

**Mục tiêu:** app không còn chạy root; tài nguyên bị chặn trước khi phá OS.

- [ ] Tạo user/group `gwapp-<id>` khi cài; appdata `chown` cho UID đó.
- [ ] Payload root `0555`; app không ghi được (kiểm bằng `gwtest --write-payload`).
- [ ] Child `setgid/setuid` trước `execve`; không giữ group root; không setuid
      binary.
- [ ] Đặt `RLIMIT_AS`, `RLIMIT_DATA`, `RLIMIT_NOFILE`, `RLIMIT_NPROC`,
      `nice`, `oom_score_adj`; cấu hình theo profile trong service, không theo
      manifest.
- [ ] Watchdog RSS: vượt trần → `SIGTERM`, sau 3 giây `SIGKILL`, log lý do.
- [ ] `gwtest --alloc`, `--fds`, `--nproc` kiểm giới hạn.

**AS:** AS-20, AS-21, AS-23.
**Cổng thoát:** không còn process app nào UID 0; vượt trần bị kill; payload
không ghi được.

### Phase 3 — Bàn giao hiển thị và input

**Mục tiêu:** app mới vẽ và nhận touch mà không tự sở hữu thiết bị.

- [ ] Tạo `libgwdisplay` tối giản: nhận fd, `FBIOGET_VSCREENINFO`, commit
      damage rect + hint, release.
- [ ] Dùng lại policy đã đo trong `os/ui/lv_port_epdc.h` (DU4 transition,
      section isolation, deferred REAGL cleanup) thay vì tạo policy thứ hai.
      **Cập nhật 2026-10-31:** policy `fast` gây ghosting + flash thứ hai;
      mặc định chuyển sang `quality` (một GC16/scene), `balanced` là lựa chọn
      trung gian. Xem `docs/technical/EINK-UI-RENDERING.vi.md` và
      [paint quality plan](2026-10-31-eink-paint-quality.md).
- [ ] Service mở fb/touch, truyền `GW_FB_FD`/`GW_TOUCH_FD`; app không mở
      `/dev/fb0`.
- [ ] Chính sách refresh: partial cho vùng nhỏ, full sau handoff và định kỳ;
      đếm ghosting.
- [ ] Service giữ `KEY_POWER`; touch chỉ chuyển cho foreground; drain event sau
      resume.
- [ ] `gwtest --draw` vẽ pattern, thoát; Home vẽ lại sạch, không còn pixel cũ.
- [ ] 100 chu kỳ; fd count ổn định; không double event.

**AS:** AS-25, AS-26, AS-27.
**Cổng thoát:** app mới vẽ/touch qua fd; Home không nhận input đồng thời;
không rò fd.

### Phase 4 — Lỗi, treo, OOM, boot loop

**Mục tiêu:** app hỏng không kéo OS theo.

- [ ] `gwtest --crash` (SIGSEGV), `--hang`, `--exit N`.
- [ ] Crash → Home ≤ 3 giây, thông báo đọc được, log signal; không respawn.
- [ ] Treo launch → kill sau 5 giây; treo exit → kill sau 3 giây.
- [ ] OOM do app → app bị kill, Reader/PID1 sống; OOM toàn hệ thống có
      `oom_score_adj` bảo vệ.
- [ ] 20 lần crash liên tiếp; boot ID không đổi; vẫn mở được Reader.
- [ ] 100 chu kỳ launch/exit; fd/service socket không tăng đơn điệu.

**AS:** AS-13…AS-19.
**Cổng thoát:** crash lặp không boot loop; Reader vẫn mở; fd ổn định.

### Phase 5 — Power trong app

**Mục tiêu:** khóa/suspend hoạt động khi app đang foreground.

- [ ] Gửi `SIGUSR1` trước lock/suspend; chờ tối đa 2 giây.
- [ ] App có thể lưu trạng thái; nếu không phản hồi, OS vẫn tiếp tục.
- [ ] Resume: `SIGUSR2`, vẽ lại, drain input; không wake loop.
- [ ] Thử 5 chu kỳ suspend/resume với `gwtest`, có/không cáp.
- [ ] Đường nút vật lý đã đạt ở Home; dùng lại capture/uinput cho app, nhưng
      phải có một lần bấm tay khi app đang foreground trước khi nhận đầy đủ.

**AS:** AS-28, AS-29, AS-30.
**Cổng thoát:** 5 chu kỳ không kẹt; nút thật hoặc ghi rõ giới hạn; không
suspend khi có cáp.

### Phase 6 — Đóng gói nâng cao và dữ liệu

**Mục tiêu:** cài/update không phá máy trong điều kiện xấu.

- [ ] Test sai arch/profile trên thiết bị (không chỉ host).
- [ ] Hết chỗ: từ chối trước khi ghi; root cũ còn nguyên.
- [ ] Mất điện mô phỏng ở staging/extract/active; boot lại vào bản cũ hoặc Home.
- [ ] Rollback về version trước; gỡ app giữ data; xóa data riêng.
- [ ] Nếu D3 chọn data image: migrate appdata, kiểm shutdown/rescue/power-loss.

**AS:** AS-33, AS-35, AS-36, AS-38.
**Cổng thoát:** không mất bản đang chạy; boot lại được; dữ liệu người dùng còn.

### Phase 7 — UI, hồi quy, vận hành

**Mục tiêu:** mở tính năng cho người dùng chỉ khi backend đã đạt.

- [ ] Nút Open chỉ hiện cho app có runtime hỗ trợ; app lỗi hiện thông báo.
- [ ] Dùng `gwctl ui-nav/ui-tap` + `pw3-uinput-touch.py` cho lặp UI; vẫn cần
      người xác nhận trải nghiệm vật lý ở mốc cuối.
- [ ] Reader regression: mở sách, lật trang, Return to Home.
- [ ] Reboot/rescue/restore; hash khớp.
- [ ] IPC auth: client lạ, dòng quá dài, command injection.
- [ ] Log không lộ Wi-Fi/token/tên sách.
- [ ] Cập nhật `CURRENT.vi.md`, `ACCEPTANCE.json`, `BACKLOG.md`.

**AS:** AS-39…AS-48.
**Cổng thoát:** checklist đạt trên build hiện tại; có bằng chứng và giới hạn.

## 6. App kiểm thử `gwtest`

Một bundle ký thật, không phải Reader, có các mode:

| Mode | Dùng cho |
| --- | --- |
| `--draw` | Vẽ pattern qua fd; kiểm handoff/refresh |
| `--exit N` | Exit code 0/1/64/70/75 |
| `--crash` | SIGSEGV |
| `--hang` | Treo launch/exit |
| `--alloc MB` | Vượt trần RAM |
| `--fds N` | Mở nhiều fd |
| `--nproc N` | Fork nhiều process |
| `--write-payload` | Kiểm payload read-only |
| `--sigwait` | Nhận `SIGUSR1/2`, lưu trạng thái |
| `--sleep S` | Đo idle/wakeup |

Bundle này là công cụ kiểm chứng, không phát hành cho người dùng.

## 7. Rủi ro và cách chặn

| Rủi ro | Chặn |
| --- | --- |
| Root ext3 đầy vì appdata | D3 + quota + kiểm dung lượng trước khi cài/chạy |
| App chạy root phá OS | Phase 2 bắt buộc trước khi mở Open cho app người dùng |
| `RLIMIT_AS` phá KOReader | Đo AS-02; nếu phá, dùng watchdog RSS + `oom_score_adj` |
| App tự mở fb gây tranh chấp | Chỉ cấp fd; kiểm `gwtest` không mở `/dev/fb0` |
| Crash lặp → boot loop | Không respawn; shell failure budget; rescue |
| Data image chưa nghiệm thu | Không bật cho đến khi shutdown/rescue/power-loss đạt |
| Paint policy UI đã đạt nhưng app mới có thể vẽ sai policy | App dùng `libgwdisplay`, không tự chọn waveform; đo ghosting sau 30 lần lật trang |

## 8. Định nghĩa hoàn thành

1. App `gwtest` và một app thật (không phải Reader) chạy foreground, thoát về
   Home, không respawn.
2. App chạy UID riêng, payload không ghi, vượt RAM/fd bị chặn.
3. App mới vẽ/touch qua fd; không tự mở fb/power.
4. Crash/treo/OOM lặp không làm mất Home, Reader, USB console hay rescue.
5. Lock/suspend/resume hoạt động khi app foreground; có giới hạn chờ.
6. Cài/update/remove vẫn giữ data; mất điện giữa cài không làm mất bản cũ.
7. Toàn bộ `AS-*` liên quan có bằng chứng trên cùng model; các mục chưa đạt
   được ghi rõ, không đánh dấu khống.

## 9. Execution record

- 2026-10-31: kế hoạch được tạo sau khi review codebase. Chưa chạy phase nào;
  chưa sửa mã/thiết bị. Các mốc installer/power/Wi-Fi/perf lấy từ evidence
  ngày 2026-10-31, không phải kết quả của kế hoạch này.
- 2026-10-31 (review 2): bổ sung bằng chứng paint policy (DU4 ~76 ms) và nút
  vật lý thật; thêm `uinput` touch/power vào test tooling. Kết luận không đổi:
  generic app launch/UID/limit/fd handoff vẫn là khoảng trống chính.
