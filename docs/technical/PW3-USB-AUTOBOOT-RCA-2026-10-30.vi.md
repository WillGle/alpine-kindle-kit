# RCA USB và autoboot PW3 — 2026-10-30

## Phạm vi và bằng chứng

Người dùng cho phép thử nghiệm, clean install và xóa dữ liệu. Ưu tiên chuyển sang RCA trước cài mới. Giai đoạn điều tra ban đầu không ghi p1; phần cập nhật cuối ghi rõ lần thay bootstrap p1 có backup. Bootloader và kernel image giữ nguyên. Đã phục hồi **rootfs cũ cùng cặp identity đang cài**, chưa triển khai candidate base/reader mới hoặc kiểm chứng first-install coordinator.

Bằng chứng riêng: `work/private/hardware-trials/continuous-usb-20261030T064223Z/`. `events.jsonl` theo dõi USB 100 ms; `RCA-journal.jsonl` ghi sự kiện/phán đoán; ảnh trước sửa, marker, checksum deploy, log host/device/fsck được giữ riêng. Không sửa ký ức Codex.

## Quan sát ban đầu — xem cập nhật cuối cho trạng thái sau sửa

1. Không thấy host khóa USB: authorized/default=1, không USBGuard, driver hoạt động. Thiết bị xuất hiện authorized=1 và đọc/ghi được qua UDisks; không phải cản permission trong lượt kết nối thành công.
2. Host ghi disconnect lúc 12:41:23. Rescue xuất hiện rồi disconnect trong cùng giây 13:37:31. Chưa đủ log thiết bị tại thời điểm đó để xác định reset, mất nguồn, cáp hay vendor USB session; **chưa xác định nguyên nhân mất enumeration ban đầu**. Đèn sạc không đủ để chẩn đoán pin/cáp. Đề nghị sạc ở lượt trước là suy đoán; đo sau phục hồi cho thấy pin100%.
3. Sau reset tiếp theo, rescue0525:a4a5 bcd0200 ổn định từ13:42:37, partition sẵn sàng13:42:39. `boot.enabled` và `boot.pending` đều đúng token. Rootfs có incompat=6, bao gồm needs_journal_recovery. Bản sao gốc SHA256 `fa11773957ce483b9d7bf70c91cfe34bfd42c040504cdd055b5e7c9189c728db`.
4. `os/devices/pw3/overlay/etc/gleworks-early.sh` cố ý giữ pending suốt runtime. `bootstrap/bootstrap.c` từ chối pending cũ hoặc ext3 cần repair và chọn rescue. `bootstrap/shutdown.c` chỉ xóa pending sau unmount root thành công. Đây là nguyên nhân có chứng cứ cho việc **không tự vào Alpine sau reset cứng**, không phải Amazon remote lock. Không có bằng chứng brick; rescue vẫn đọc/ghi được và đã phục hồi Alpine.
5. Readonly e2fsck -fn trả0 nhưng cảnh báo bỏ journal recovery và sai free counts; không dùng exit0 một mình để kết luận sạch. Sửa bản sao bằng -fy: replay journal, sửa counts, rc1. Kiểm tra lại -fn rc0, không needs_recovery. Deploy qua maintenance, hash readback khớp, giữ bản cũ trên host và thiết bị, eject an toàn. Không đổi paired guardian/bootstrap.
6. Native sau sửa: Alpine3.24.3, PID1/bin/busybox, USB0525:a4a7; guardianstate0; cable connected1/usbhost1; battery100%. Reader và touch khởi động; chưa có xác nhận thao tác/panel từ người dùng.
7. Reboot sạch thành công: boot65af13a6-5971-4085-8dbf-7295c75e6a01 → eb2a8c9d-ac62-472b-b08f-86dbc565474f, USB console trở lại. Alpine→rescue sạch thành công; pending vắng mặt, rootfs không needs_recovery, e2fsck -fn rc0, shutdown journal `GW_ALPINE_ROOT_CLEANLY_UNMOUNTED`. Resume cuối đã đạt: native bootID00918ac9-b3b8-4b8c-83e7-177df386fd74, USB console và PID1 xác minh mới.

## Khoảng trống thiết kế cần xử lý trước coi là appliance ổn định

- Reset cứng/mất nguồn không tự sửa journal: lần boot sau ở rescue chờ host. Cơ chế bảo vệ đúng với mã hiện tại nhưng khả năng tự phục hồi chưa đáp ứng máy đọc độc lập. Không xóa pending mù hoặc vô hiệu gate; cần thiết kế recovery offline có giới hạn và nghiệm thu riêng.
- Khi vào rescue, panel có thể vẫn giữ ảnh boot cũ và không giải thích trạng thái; người dùng có thể tưởng brick rồi reset tiếp. Cần thông báo cứu hộ trên panel hoặc đường trạng thái có chứng cứ; chưa triển khai thay đổi này.
- Chưa kiểm chứng native USB reconnect sau rút/cắm khi máy vẫn chạy. Code init có prime gadget đầu boot; điều đó không tự chứng minh hotplug an toàn. Cần thử tách riêng, lưu uptime/bootID/asession/dmesg trước-sau.
- Nhánh shutdown stop() dừng thay vì force reboot khi unmount thất bại; đây là lựa chọn tránh hỏng filesystem, chưa có bằng chứng nhánh này gây sự cố hiện tại. Cần thu log trước khi quy kết.

## Vận hành để tránh lặp lại ngay

Dùng `kindle-linux.py reboot`, `rescue`, `resume`; tránh giữ40s trong runtime trừ khi thực sự mất điều khiển. Nếu pending/dirty: giữ ảnh gốc, repair offline rồi readback/eject có kiểm tra. Không bật USB mass storage khi Alpine còn giữ ảnh trên FAT. Giữ monitor và log trước mỗi thay đổi boot.

## Cập nhật — tái hiện hotplug lỗi

13:48:28 host ghi disconnect sau thao tác rút/cắm không reset được người dùng xác nhận. Không re-enumerate hoặc ttyACM sau đó. Trước thử: native bootID00918ac9-b3b8-4b8c-83e7-177df386fd74, guardian0, reader/touch khởi động, pin100%. Đây là bằng chứng lỗi reconnect độc lập; pending/journal không đủ giải thích mất USB. Chưa biết thiết bị còn sống hay treo vì chưa có telemetry lưu qua disconnect. Driver vendor có nhánh low-power và B-session, mới là hướng điều tra; chưa kết luận nguyên nhân chính xác hoặc sửa bằng register/proc tùy đoán. Cần reset phục hồi rồi cài telemetry giới hạn, lưu bằng chứng qua một lần hotplug trước bản sửa.

## Cập nhật — phục hồi và instrument hotplug

13:51:48 cứu hộ trở lại, partition13:51:50. Người dùng báo ảnh cậu bé, trong khi host vẫn đọc được cứu hộ; ảnh panel không chứng minh treo. Lưu `after-hotplug-reset.ext3`: pending còn và needs_recovery có sau hard reset; không dùng trạng thái hậu reset này để suy ra kernel đã treo trước reset. Khôi phục bản sửa đã biết qua readback/eject; native bootID3d816792-2a1d-4622-9a3a-5f35d0409356 và guardian0 được xác minh.

Bật script chẩn đoán tạm thời `/run/rca-usb-capture.sh`, không sửa source/inittab hoặc controller, tối đa180 mẫu mỗi giây và sync; log `gleworks-alpine/rca-hotplug-20261030.log` trên FAT. Đã đọc lại thấy SAMPLE4, uptime35.75, charger/usbhost/connected đều1. Mục đích phân biệt tiến trình còn chạy sau disconnect với treo/reset. Dữ liệu trên FAT tạo thêm writer có giới hạn trong phép đo; cần cân nhắc nhiễu quan sát khi diễn giải. Đang chờ rút/cắm có kiểm soát tiếp theo.

## Cập nhật — hotplug instrumented lần hai

13:55:04 monitor ghi disconnect sau rút/cắm không reset; USB chưa trở lại. Người dùng xác nhận panel vẫn thư viện KOReader. Chưa dùng ảnh tĩnh để kết luận tiến trình sống; đang xác minh menu phản hồi và ưu tiên reboot sạch qua Device:reboot (native.lua gửi QUIT đến PID1) để lấy log, tránh hard reset nếu có thể. Log thiết bị đã bật180 mẫu trước khi rút.

## Cập nhật — UI sống khi USB mất

Người dùng xác nhận menu KOReader phản hồi sau lần hotplug thứ hai. Do đó không phải treo toàn bộ kernel/UI tại thời điểm đó; lỗi tập trung ở USB transport/session. Đã tra source filemanager_menu_order, common_exit_menu_table, native.lua và catalogvi để hướng dẫn reboot sạch: tab☰ → Thoát → Khởi động lại thiết bị → xác nhận. Chờ máy trở lại để đọc log180 mẫu đã lưu.

## Cập nhật — bằng chứng clock/session sau hotplug

Reboot từ menu thành công, USB trở lại13:57:57, bootID7718c037-cb94-475d-a187-c4dd777813ce. Thu được371321 ký tự console bao gồm log180 mẫu (thực tế171 mẫu hoàn chỉnh trước reboot). Uptime tiếp tục tăng sau hotplug; PMIC ghi charger disconnect ởuptime77.52 rồi connect92.82. Sau reconnect, charger1/usbhost1 nhưng clockusb_oh3=0, usb_phy1=0, gadgetconnected0 kéo dài tới ít nhấtuptime232.72. Không treo toàn máy, không phải host permission. Chính xác callback/board detection nào không tái bật PHY còn cần thí nghiệm.

Đã compile probe tạm dùng API vendor đã đọc source: sau60s, chỉ khi charger1/usbhost1/cảhai clock0, write(2) `/proc/asession` lệnh5 0, chờ1s, rồi5 1; không đổi boot/p1/MMIO trực tiếp. Chưa tuyên bố fix đến khi re-enumerate cùng bootID. Upload Console.put bản nhị phân lớn thất bại base64 truncated input do đường interactive; đã truyền chunk384bytes, readbackhash đúng trước chạy. Ghi nhận finding riêng cho upload tool, chưa sửa ngoài phạm vi RCA.

## Cập nhật — force-detect một lần chưa phục hồi

Probe arm13:59:56, hostdisconnect14:00:28; sau mốc dự kiến14:00:56 vẫn chưa re-enumerate. Không tuyên bố toggling5 0/5 1 là bản sửa. Stderr/returncode probe hiện ởRAM, không truy cập được khi USB mất; thiếu durable execution marker nên chưa khẳng định guard đã qua hay driver call thành công. Cần logging đầy đủ cho phép thử tiếp theo; yêu cầu reboot sạch từ menu thay vì hard reset.

## Cập nhật — thử lại với durable probe

Probe v2 bắt đầu14:05:23, giới hạn300s, xác minh log khởi động và clock1 trước thử. Người dùng xác nhận replug; hostdisconnect14:09:04, vẫn trong cửa sổ theo dõi. Đến14:09:37 USB chưa trở lại. Đang yêu cầu reboot sạch để đọc durable guard/write-return/state; không quy kết probe chưa chạy hoặc driver call đã thành công khi chưa có log.

## Kết quả durable probe — xác nhận thất bại và giới hạn RCA

Thu `wake-v2-result.log` sau reboot menu: GUARD_MATCHED, WRITE5 0 và5 1 đều result4/expected4/errno0, TRACE_DONE. Cả10 mẫu sau pulse vẫn clock0/0. Đây là bằng chứng force-detect đơn thuần không đủ, không phải lỗi timing/guard không chạy. Native phục hồi bằng reboot sạch, bootID946cf828-b129-4f62-b153-fbf8b6da7a50, guardian0. Inventory driver xác nhận usb-wakeup đã đăng ký và IRQ75 chia sẻ với OTG/UDC, không có bằng chứng thiếu module wakeup.

RCA có chứng cứ: sau rút cáp, controller/PHY về low-power; cắm lại PMIC nhận host nhưng clock không wake, khiến host không enumerate trong khi UI/process vẫn sống. Chưa xác định chính xác callback/IRQ hoặc điều kiện board nào mất wake nên chưa gọi là root cause ở mức dòng driver. Không triển khai workaround force-detect, không sửa kernel/p1, không bật/tắt clock tùy đoán. Hai issue riêng: USB hotplug wake bị hỏng; dirty/pending chuyển rescue sau hard reset đúng policy nhưng thiếu tự phục hồi/thông báo panel.

Trạng thái cuối lượt đo: giữ hệ native đã phục hồi, các probe ở/run mất sau reboot và không có dịch vụ probe thường trú; log chẩn đoán còn ởFAT để bảo toàn bằng chứng. Monitor host tiếp tục chạy. Chưa clean-install kit mới hoặc qualify first-install. Trước khi public, giữ hotplug và crash recovery là blocker; vận hành tạm dùng reboot từ menu khi USB mất, tránh reset40s khi UI còn phản hồi.

## Cập nhật — bản sửa UDC đã qua ba lần hotplug cùng boot

Thay đổi tối thiểu trong disconnect worker của `arcotg_udc.c`: giữ PHY/controller hoạt động thay vì vào low-power không tự wake; không dùng force-detect hoặc reboot để che lỗi. Module mới SHA256 `92e272e07122866c289199caf816ff2371defadc6a83531d0bfb2b72fa853976` đã được nạp sau reboot sạch. Ba lần rút/cắm đều có `g_serial ... high speed config #2`; console nonce xác minh lại boot ID vẫn `b4948a0b-3483-4ca4-a485-1556203a9f67`, uptime643s, pin99%. Người dùng xác nhận hai lần rút/cắm bổ sung. Bằng chứng: `patched-hotplug-repeat-confirmed.log` và monitor `events.jsonl` trong thư mục trial riêng.

Giới hạn: giữ controller thức có thể tăng hao pin; chưa đo. Đây là kết quả runtime đang cài, chưa phải nghiệm thu first-install kit mới. Module rescue trên p1 vẫn nguyên bản tại thời điểm phép thử này. Builder đã tích hợp rebuild module và public-source allowlist đã chứa patch; build base/reader thành công với dependency chuẩn bị sẵn.

Console upload đã chuyển sang chunk384byte và xác minh SHA256 trước rename; challenge mới thoát shell cũ còn lại sau cable drop. Upload module thực tế thành công. Bootstrap autorepair đang được kiểm chứng riêng, chưa gọi là sửa xong crash recovery. Stock p1 dùng ext2: mount ext3 noload bị EINVAL, mount ext2 readonly thành công. Stock e2fsck1.46.4 chạy được qua chroot thật; bootstrap p1 đã sao lưu SHA256 `b4aa162ffc2972a59a3c649d15b600c90e495e5025f6c29bada7798acb0b7175`. Probe ARM/QEMU cho nhận biết superblock sạch/dirty và child exit/timeout đạt.

## Cập nhật — bootstrap phục hồi đã cài cùng identity và reboot sạch đạt

Đã cập nhật early script rồi thay atomically `/sbin/init` p1, giữ bootstrap gốc trên host và p1; readback SHA256 mới `f59ed10a6ce31fdad82f90c1888dfe2a1e4b0b6d5e4b55b55a5c5529677c21e0`. Guardian/control pairing đã xác minh trước thay. Không thay bootloader/kernel image hoặc module rescue p1. Reboot sạch: `b4948a0b-3483-4ca4-a485-1556203a9f67` → `d85844e2-51a1-4a58-a77b-0b9c8b289125`, PID1busybox, guardian0.

Alpine→rescue với bootstrap mới đạt: rootfs sạch theo superblock và e2fsck-fn, pending/repairing vắng mặt, shutdown journal cleanly-unmounted. Người dùng xác nhận dòng USB rescue hiển thị trên nền ảnh cậu bé cũ; panel không được xóa toàn bộ. Đây là thông báo trạng thái hoạt động, không phải bằng chứng màn hình đã refresh toàn trang. Chưa kết luận autojournal repair tới khi có reset cứng thật.

## Cập nhật — reset cứng thật tự phục hồi PASS

Người dùng giữ40s rồi thả, giữ nguyên cáp. Monitor ghi native disconnect07:42:51UTC và native enumerate07:43:08UTC, không thấy mass-storage rescue xen giữa. Boot ID `2ce67f42-7116-4ff0-9aeb-0a93baf4d82a` → `e640d68f-9bd1-430d-ae0a-96d92a10dbff`. Log stock fsck: `AlpineTrial: recovering journal`, sau đó `clean, 1687/65536 files, 90454/262144 blocks`. Console xác minh PID1busybox, guardian0, battery99%, boot.repairing vắng mặt và pending mới đúng token. Sau32s KOReader luajit đang chạy và touch cyttsp4 đã khởi tạo. Bằng chứng `hard-reset-autorepair-result.log`; đây là một lần reset cứng, không phải chứng nhận mọi dạng mất nguồn/hỏng storage.

Đã đồng bộ module UDC đã kiểm chứng sang p1 rescue bằng backup+hash+atomic rename; bản gốc còn ở p1 và host. Không thay bootloader/kernel image. Tiếp tục thử gate interrupted-repair bằng marker trên ảnh sạch, không cố tình làm hỏng filesystem thật.

## Kết quả cuối — gate gián đoạn và build nguồn

Sau crash recovery, reboot sạch vào rescue bằng module UDC p1 mới đạt; host kiểm tra superblock và e2fsck-fn sạch. Đặt marker `boot.repairing` thử nghiệm trên ảnh sạch và bật persistent, eject an toàn: máy trở lại rescue, giữ nguyên marker, không tạo pending hay tự bỏ qua gate. Công cụ resume mới từ chối interrupted marker trước boot. Chỉ sau kiểm tra ảnh sạch, marker thử nghiệm được bỏ và resume an toàn. Không cố tình hỏng filesystem hoặc ngắt điện giữa fsck trên thiết bị.

Gói nguồn55file đã trích riêng, hash manifest và không chứa reset-token.h được kiểm tra; base `pw3-base-6071d82ab4d6` và reader `pw3-reader-94462d39be54` dựng đạt với dependencies chuẩn bị sẵn. Các thay đổi host sau build bổ sung gate marker ở resume/deploy/install acceptance/cleanup; đã kiểm tra syntax và nhánh resume từ chối trên rescue thật. Không phát hành/upload.

Kết luận giới hạn: ba hotplug cùng boot đạt; một reset cứng thật tự replay journal và trở lại Alpine đạt; reboot/cứu hộ/resume và thông báo panel đạt; marker interrupted-repair dừng ở rescue đạt. Chưa đo pin do giữ USB controller thức, chưa thử mọi lỗi filesystem/mất nguồn, chưa nghiệm thu first-install coordinator mới hoặc các model khác. Bootloader/kernel image không đổi.

Trạng thái cuối: native boot `226a3077-f6a3-49ed-82b0-ac0f14bf6820`, PID1busybox, guardian0, repairing vắng, module hash đúng, reader process sống. Người dùng xác nhận thư viện KOReader hiển thị và menu cảm ứng phản hồi. Monitor host tiếp tục chạy.

## Cài sạch candidate reader mới — không gộp với kết quả runtime cũ

Người dùng cho phép xóa/cài lại. Candidate reader mới root SHA256 `530042c8d7e9d0320c33f697d653d3431604e003ceee2d295e54e2e44341b4d0`, UDC do builder dựng SHA256 `63a5333d9717a37f58b153382c5c7bbfb7eaf78d8ce415a9b8c81dee0ceb4edc`. Đã xác minh manifest, nguồn OS/app và fsck; sao lưu pair p1 cũ trên host, root sạch cũ trên host+thiết bị. Tắt boot.enabled trước thay pair p1 rồi boot rescue thành công trước thay root. Deploy-root kiểm tra hash đọc lại và eject an toàn. Không thay bootloader/kernel image.

Native boot đầu `ce5ff026-3467-4f98-86b5-f80b59054015`: hash guard-control/shutdown/UDC khớp candidate, guardian0, marker repairing vắng. Sau clean rescue/chuyển EPUB/resume: boot `3feb7cf3-c7e2-4be7-967f-6131a0573828`, reader sống quá90s và framebuffer thư viện có đúng dấu tiếng Việt. Đang chờ physical book/page acceptance; hotplug và reset cứng của runtime cũ không tự chuyển thành acceptance cho candidate mới. Trial riêng `work/private/hardware-trials/candidate-reader-20261030T074953Z/`. Không chạy luồng stock-first-install trên máy native, không sửa session để khai đạt.

Candidate reader mới đạt đọc/lật trang theo người dùng. Dmesg có hai chu kỳ disconnect/connect ởuptime610–613s và620–635s; nonce console xác minh cùng boot `3feb7cf3-c7e2-4be7-967f-6131a0573828`, uptime670s, guardian0. Log `candidate-user-replug-and-reading.log` trong trial riêng. Hotplug của module do builder dựng đã có chứng cứ hardware riêng; reset cứng candidate vẫn chờ.

## Candidate reader — kết quả nghiệm thu sau cài sạch

User xác nhận reset40s. Boot mới `5664c181-2a17-4ffb-8f8a-b98687c4b447`, PID1busybox, guardian0, repairing vắng; repair.log bổ sung `AlpinePW3: recovering journal` và `AlpinePW3: clean, 1680/65536 files, 93897/262144 blocks` (log còn chứa dòng AlpineTrial cũ, không dùng dòng cũ để kết luận). Sau reboot sạch vào rescue, superblock và e2fsck-fn sạch.

Thử fallback ảnh không hợp lệ: giữ nguyên ảnh sạch bằng rename có hash, thay bằng placeholder ngắn và bật persistent, eject; bootstrap chọn USB rescue trước mount root. Hash placeholder và ảnh được giữ đều khớp. Khôi phục ảnh sạch bằng rename, kiểm tra hash+fsck rồi resume an toàn. Không cố ý phá filesystem thật. Máy cuối native boot `75863c3b-1b18-4d40-9704-61003d6f2394`, reader luajit sống, guardian0, repairing vắng, runtime payload hash đúng. Bằng chứng `candidate-hard-reset-autorepair.log`, `candidate-final-runtime.log`, trial.json và monitor liên tục.

Candidate riêng cùng source/pairing được giữ bền tại `work/build/pw3-reader-94462d39be54/`, xác minh toàn bộ payload hash; preserve rootfs symlinks khi copy. Giới hạn còn: không phải stock-first-install coordinator, một PW3, một hard reset trên candidate mới, hao pin do USB awake chưa đo. Biên bản công khai không chứa token/keys: `docs/evidence/pw3-clean-reinstall-2026-10-30.json`.

## Cập nhật 2026-10-31 — xác minh lại đường vào Alpine sau reset

Sau khi người dùng báo không thấy cổng USB và màn hình còn “Checking storage”,
host thấy `0525:a4a7` cùng `/dev/ttyACM0`; nonce console được xác minh. Máy đang
chạy Alpine3.24.3, PID1 `/bin/busybox`, boot ID
`03319c70-5a61-4fd8-b8c1-6095030127ee`, `gw-system --supervise` và `gw-shell`.
`gwctl status`: `foreground=none`, `lock=unlocked`, `display=on`. Framebuffer
đọc qua USB là Home grid, dù ảnh e-ink người dùng thấy vẫn là splash cũ.

Module UDC runtime SHA256
`63a5333d9717a37f58b153382c5c7bbfb7eaf78d8ce415a9b8c81dee0ceb4edc` khớp
candidate `pw3-reader-94462d39be54`. `/proc/asession` báo charger/USB host và
clock `usb_oh3`/`usb_phy1` đều đang hoạt động; dmesg ghi `g_serial ... high speed
config #2`. Đây là bản UDC giữ PHY/controller thức để tránh lỗi reconnect đã
được tái hiện và vá ở trên; candidate cũng đã qua hai chu kỳ disconnect/connect
cùng boot. Trong lượt này cổng thực tế đã kết nối khi kiểm tra, nên chưa thể
khẳng định nó mất liên tục trong khoảng trước đó.

Đã gọi FBInk `-W GC16 -f -w -s`; lệnh báo gửi cập nhật toàn màn hình thành công.
Chưa có xác nhận trực tiếp rằng panel đã đổi sang Home. Không reset hay rút cáp
để thử lại trong lượt xử lý này.
