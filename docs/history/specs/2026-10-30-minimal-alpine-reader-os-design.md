---
status: superseded
date: 2026-10-30
superseded_by: ../../status/CURRENT.md
---

# Alpine reader OS tối giản — thiết kế v1 cho PW3

Trạng thái: bản thiết kế để duyệt, chưa phải sản phẩm đã triển khai. Builder review độc lập: không còn câu hỏi quan trọng phải trả lời trước khi lập kế hoạch. Phạm vi này mở rộng bản bộ cài đã duyệt trước đây. Các lựa chọn kỹ thuật dưới đây là đề xuất triển khai; không ghi chúng thành yêu cầu do người dùng đã chọn.

> **Cập nhật 2026-10-31:** nhiều mục "hiện trạng" bên dưới đã thay đổi —
> signed `.gwapp` installer, Applications/App updates UI, mem suspend + RTC
> wake, Wi-Fi auto-reconnect và tối ưu idle đã có bằng chứng thiết bị. Phần
> còn thiếu là runtime app tổng quát (launch app ngoài Reader, UID/limit/fd
> handoff, crash/OOM, data image). Đọc
> [APP-SESSION](2026-10-31-app-session-contract.md) và
> [app runtime plan](../plans/2026-10-31-app-runtime.md) trước khi dựa vào các
> câu "chưa có" trong tài liệu này.

Điều chỉnh mới nhất của người dùng: thiết bị có một màn hình chính quản lý
ứng dụng và một màn hình khóa. Wi-Fi là chức năng bắt buộc của máy; thư viện
là một ứng dụng và sẽ được lập kế hoạch chi tiết sau. Các đoạn cũ nói về thư
viện chung hoặc sách gần đọc trên Home được thay bằng yêu cầu này.
Khi kết nối Wi-Fi, máy phải quét và hiển thị danh sách mạng để người dùng
chọn, thay vì yêu cầu nhập tên mạng thủ công; mật khẩu được nhập trên máy.

## Mục tiêu người dùng đã xác nhận

Một OS native dựa trên Alpine, tối giản, có cài đặt ứng dụng, quản lý phần mềm và phần cứng, màn hình chính, thư viện, cài đặt, ngủ/đánh thức, kết nối và cập nhật thống nhất. Giao diện ưu tiên e-ink, sách và ứng dụng. KOReader là ứng dụng đọc sách, không phải màn hình chính hoặc trình quản lý hệ thống. Hoạt động không cần tài khoản Amazon/cloud; không có cơ chế khóa/xóa thiết bị từ xa.

V1 chỉ hỗ trợ PW3 G090KB/firmware-kernel tham chiếu đã kiểm chứng. Không nhận là hỗ trợ model khác hoặc mọi gói Linux. Không sao chép giao diện/tài sản Amazon. Không bổ sung trình duyệt, desktop, cửa hàng thương mại hoặc tài khoản bắt buộc.

## Hiện trạng và ranh giới bằng chứng

Alpine3.24.3 armv7 đã chạy với PID1 riêng trên kernel3.0.35-lab126; KOReader, touch, USB reconnect, reboot, cứu hộ và một lần reset cứng trên candidate mới đã đạt. Đây là native userspace, không phải chroot bên trong Amazon OS. Bootstrap/guardian cùng identity và p1 rescue độc lập phải được giữ.

Bản hiện tại vẫn khởi động thẳng KOReader; thiếu giao diện OS và các dịch vụ thống nhất. Wi-Fi, quản lý app, cập nhật OS và quản lý điện hoàn chỉnh chưa được nghiệm thu. Kernel/bootloader và một số driver vendor vẫn được giữ; thay chúng là dự án port phần cứng riêng. Không quảng cáo đã loại bỏ mọi mã Amazon hoặc khắc phục mọi lỗi bảo mật của kernel cũ.

Thử nghiệm điện gần nhất: PM devices dry-run đạt; kernel mem đã tự thức qua RTC20s, cùng boot ID. Adapter power trong KOReader chỉ là thử nghiệm, chưa nghiệm thu nút thật; mã và log được giữ riêng, bản live đã trả về adapter được kiểm chứng. Kết quả này là đầu vào cho dịch vụ điện của OS, không phải feature đã hoàn tất.

## Hướng kỹ thuật đề xuất

Chọn giao diện C dùng LVGL, vẽ trực tiếp framebuffer/EPDC và đọc evdev; không cần desktop/compositor hoặc browser runtime. Phải có thử nghiệm trên màn hình8bpp/pitch1088 thực tế trước khi chốt driver LVGL. Nếu thử nghiệm không đạt, quay lại lựa chọn renderer, không che lỗi bằng giao diện giả. Không sửa kernel cho đẹp giao diện.

Giữ BusyBox PID1 và shutdown/USB recovery đã kiểm chứng; dùng OpenRC của Alpine làm một nơi quản lý dịch vụ sau khi kiểm tra chuyển đổi thứ tự mount/shutdown. Không chạy song song hai bộ điều phối cùng sở hữu phần cứng. Một dịch vụ hệ thống quản lý trạng thái, phần cứng và thao tác đặc quyền; UI và app không tự điều khiển nguồn/mạng.

Các phương án không chọn: lấy KOReader làm shell tiếp tục buộc OS vào một app; Qt/browser shell tăng dependency và footprint trước khi có nhu cầu. Không xây framework plugin đa thiết bị trước khi PW3 đạt.

## Các phần và trách nhiệm

| Phần | Trách nhiệm |
| --- | --- |
| Alpine nền | Init/dịch vụ, filesystem, thư viện nền và công cụ hệ thống được pin; không chứa dữ liệu cá nhân/app reader |
| Lớp PW3 | EPDC, touch, nút nguồn, đèn, pin/sạc, RTC, Wi-Fi, suspend và USB; xuất trạng thái/thao tác chung |
| Dịch vụ hệ thống | Điều phối power, app foreground, mạng, cài app, update/recovery và thông báo lỗi |
| Giao diện OS | Một Home quản lý ứng dụng và một màn hình khóa; điều khiển Wi-Fi qua API hệ thống thay vì gọi phần cứng trực tiếp |
| Ứng dụng | KOReader trước; có manifest, vòng đời, dữ liệu riêng và đường trở về Home |
| Công cụ host | Build, cài, backup và phục hồi; vẫn độc lập với source OS và workspace |

Dự kiến thêm source hệ thống dưới `os/system/` và `os/ui/`, giữ `os/alpine/`, `os/devices/pw3/`, `apps/`, `tools/` như hiện tại. Đây là sơ đồ để triển khai, chưa tạo thư mục/mã khung trong bước thiết kế.

## Trải nghiệm sử dụng

Boot bình thường vào Home quản lý ứng dụng, với trạng thái pin/sạc/mạng và thao tác Wi-Fi. Mở ứng dụng đọc sách từ Home; thoát/đóng reader trở về Home, không tự respawn vào thư viện KOReader. Thiết bị có một màn hình khóa.

Thư viện nằm trong ứng dụng; kế hoạch chi tiết về import, metadata và tìm kiếm của ứng dụng sẽ thực hiện sau. KOReader hiện có vẫn cung cấp đường mở EPUB/PDF đã kiểm chứng. Tiến độ/bookmark thuộc dữ liệu app; lỗi app không xóa sách.

Home hiển thị ứng dụng đã cài/phiên bản và các thao tác quản lý khi backend đã được nghiệm thu; điều khiển mạng thuộc OS. Gỡ app mặc định giữ dữ liệu; xóa dữ liệu là lựa chọn riêng có xác nhận cụ thể. Không tạo thêm màn hình OS chỉ để chứa các app hoặc thư viện.

Không animation liên tục, không hiệu ứng đổ bóng cần nhiều mức xám, không refresh theo FPS cố định. Dùng vùng cập nhật và full refresh có kiểm soát; cỡ chữ/vùng chạm phù hợp màn hình1072x1448. Một vùng thông báo chung và một nhật ký lỗi hữu hạn; không rải thông báo từ các script.

## Vòng đời ứng dụng và phần mềm

App có ID/phiên bản, entrypoint, định dạng sách hỗ trợ, quyền phần cứng cần thiết và manifest tương thích. OS chỉ cấp foreground cho một app; shell nhường framebuffer/touch trong lúc app chạy và nhận lại khi app đóng. Nút nguồn luôn do OS sở hữu. Không dùng giữ lâu nút nguồn làm phím Home vì phần cứng dùng nó cho reset.

Ứng dụng toàn màn hình phải có thao tác trở về Home. Adapter KOReader cung cấp thao tác đó và lưu trạng thái trước khi thoát; không nhận nhiệm vụ quản lý mạng/power/OS update. Một app crash trở về shell kèm lỗi đọc được, không kéo theo boot loop.

Dùng apk cho dependency/gói Alpine ở tầng build. App manager v1 chỉ cài gói từ nguồn ký được chấp nhận và đã kiểm chứng trên PW3; không hứa chạy mọi APK của Alpine. Gói ứng dụng dùng `.apk` và manifest ứng dụng; dependency được giải vào root ứng dụng riêng trong dữ liệu bền, không ghi đè OS nền. Không chạy maintainer script từ app package. Người dùng có thể cài offline từ file của nguồn tin cậy hoặc từ catalog cấu hình được. Gói không đúng kiến trúc/profile/chữ ký bị từ chối trước khi đổi bản đang chạy.

Đây là app được tin cậy, không gọi là sandbox mạnh trên kernel3.0. Chạy bằng UID riêng, payload không ghi được, dữ liệu tách riêng; chỉ foreground được cấp display/touch. Ranh giới cụ thể phải kiểm chứng bằng DAC và root ứng dụng, không hứa có namespaces/seccomp hiện đại. Shell/app không có API exec tùy ý hoặc endpoint cài package từ xa.

Cài app: tải/đọc vào staging, kiểm chữ ký/hash/manifest/dung lượng, giải vào version mới, kiểm entrypoint rồi mới đổi active version. Lỗi giữ bản cũ; không xóa dữ liệu người dùng. Chính sách khóa ký/cập nhật nguồn công khai trong cấu hình, không khóa vào server của tác giả.

## Phần cứng và power thống nhất

Dịch vụ OS là nơi duy nhất điều khiển nút nguồn, đèn và suspend. App nhận sự kiện chuẩn trước ngủ/sau thức để lưu vị trí và nhường thiết bị; OS không chờ app vô hạn. Bấm ngắn: lưu trạng thái, khóa cảm ứng, tắt đèn, hoàn tất refresh rồi suspend thật. Thức dậy: phục hồi đèn/input/foreground và redraw. Giữ40s vẫn là reset phần cứng, không thay nghĩa.

Khi cắm USB quản trị, v1 cho khóa màn hình/tắt đèn nhưng giữ kernel thức để console và chép file không bị gián đoạn. Rút cáp khi đang ngủ sẽ chuyển sang suspend sau khoảng đệm. USB/RTC wake không gây vòng ngủ/thức dày; phải có log và retry hữu hạn khi lỗi. Chế độ đã ngủ và chỉ khóa màn hình được báo đúng trong diagnostic.

Chưa bật auto-suspend mặc định đến khi nút thật, RTC, sạc, touch, reader và USB reconnect qua nhiều vòng. Thời gian tự ngủ cấu hình được sau nghiệm thu. Không gọi ảnh e-ink tĩnh là bằng chứng đã suspend. Đo dòng/điện lượng khi rút sạc trong các trạng thái, gồm ảnh hưởng của bản sửa UDC. Power-off hoàn toàn chỉ hiện khi đã kiểm chứng bật lại bằng nút nguồn.

## Kết nối và nhập sách

V1: Wi-Fi và nhập sách qua LAN có xác thực; USB console/cứu hộ vẫn độc lập. Backend Wi-Fi dùng driver/firmware và wpa_supplicant/udhcpc sau khi nghiệm thu trên kernel đang giữ. Giao diện cài đặt sở hữu kết nối; KOReader không tự chạy công cụ Amazon hoặc điều khiển Wi-Fi.

Không có Wi-Fi thì Home, thư viện, reader, app đã cài và cài gói offline vẫn dùng được. Credentials lưu0600, không vào log/gói public. Nhập file có giới hạn dung lượng, tên file an toàn và kiểm tra hoàn tất trước đưa vào thư viện; không có API shell/xóa hoặc package install từ LAN. Tailscale là tùy chọn sau Wi-Fi, không chặn đường đọc sách; kernel không có TUN nên phải nghiệm thu userspace networking trước khi nhận hỗ trợ.

USB mass-storage chỉ dùng khi tất cả image/data liên quan đã được unmount sạch. Không export block đang chứa root/data của máy đang chạy. USB chuyển sách khi đang đọc cần giải pháp file-level được kiểm chứng riêng, không quảng cáo raw mass-storage đồng thời là hỗ trợ.

## Lưu trữ, cập nhật và cứu hộ

Giữ partition layout/bootloader/kernel đang được kiểm chứng. Dự kiến có hai image OS A/B và một image dữ liệu ext riêng trong userstore hiện có; không repartition. Dữ liệu chứa sách, thư viện, cấu hình và app roots; OS update không chứa dữ liệu đó. Chỉ chốt kích thước sau đo footprint PW3; luôn giữ đủ vùng dự phòng để staging app/OS và từ chối thao tác thiếu chỗ trước khi ghi. Chuyển dữ liệu cũ phải có backup/hash và bước phục hồi, không tự xóa bản duy nhất.

Update OS là một bản phát hành thống nhất có chữ ký và version tương thích phần cứng/dữ liệu. Ghi slot không active, kiểm hash/filesystem rồi mới chọn trial boot. Chỉ đánh dấu tốt sau khi dịch vụ, shell và đường USB hoạt động; startup lỗi/deadline quay lại slot tốt. Không suy ra FAT rename an toàn mọi mất điện: metadata chọn slot, thứ tự sync và power-cut phải có thiết kế/probe riêng. Khi cả hai slot không dùng được, vào p1 rescue độc lập có thông báo.

Bootstrap/guardian identity hiện tại được giữ khi cập nhật OS thường; không sinh token mới rồi thay lẻ root. Thay bootstrap/kernel/module nền là migration riêng có kiểm chứng USB cứu hộ trước ghi. Chưa có A/B hoặc update manager trong mã hiện hành; không dùng từ rollback an toàn như tính năng đã có.

Migration cấu hình phải giữ khả năng quay lại OS cũ hoặc có snapshot nhỏ của cấu hình; không rollback/xóa sách để rollback OS. Chữ ký release xác minh trước staging; khóa phát hành riêng không nằm trên thiết bị/repo. Không tự bắt buộc cập nhật, khóa tài khoản hoặc thực thi lệnh điều khiển từ xa.

UI giao tiếp với dịch vụ đặc quyền qua Unix socket cục bộ, kiểm danh tính process và tập lệnh cố định có giới hạn kích thước; không truyền shell text tùy ý. Dịch vụ ghi lỗi một nơi, không log khóa Wi-Fi/token/tên sách nhạy cảm. Mọi lệnh từ mạng đều đi qua bề mặt giới hạn riêng cho nhập sách.

## Thứ tự xây dựng và nghiệm thu

1. **Nền OS và contract phần cứng:** chuyển quyền sở hữu khỏi app, init/shutdown thống nhất; giữ USB cứu hộ, thử nguồn/RTC/button và đo baseline. Không đổi thư mục toàn repo lần nữa.
2. **Shell và reader:** spike LVGL/EPDC/evdev trên máy thật, sau đó một Home quản lý app và một màn hình khóa; mở reader, thoát về Home, app crash về Home. Thư viện là app và được thiết kế chi tiết sau. Không xây màn hình mô phỏng không nối backend rồi gọi hoàn tất.
3. **App manager:** package/manifest/chữ ký/data policy, cài/update/gỡ một app thật; từ chối package sai, kiểm mất điện và hết chỗ. Giao diện chỉ hiện tính năng backend đã đạt.
4. **Power và kết nối:** ngủ/thức nhiều vòng ở trạng thái reader/Home, có/không cáp; Wi-Fi/import có xác thực; Tailscale tùy chọn sau khi nền mạng đạt.
5. **OS update và phục hồi:** slot trial/ack/fallback, migration config/data, update lỗi/mất điện, USB rescue và cài lần đầu từ Amazon OS bằng bộ cài mới.
6. **Public v1:** thử sử dụng dài hạn và pin, tài liệu, license/provenance, gói source sạch. Chỉ công bố các chức năng có chứng cứ PW3; không public binary vendor/khóa/backup mặc định.

Mỗi mốc phải hoạt động thật, có bằng chứng và cách quay lại bản ổn định. Không coi boot KOReader hoặc host syntax/build là nghiệm thu toàn OS. Trong khi thiết kế mới chưa được duyệt/triển khai, máy tham chiếu giữ candidate đã kiểm chứng; các thử nghiệm không được ghi thành tính năng production.
