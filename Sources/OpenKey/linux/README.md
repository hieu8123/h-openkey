# H-OpenKey — OpenKey cho Linux

Bộ gõ tiếng Việt cho Linux.

Đây là bản **fork** của [OpenKey](https://github.com/tuyenvm/OpenKey) của Tuyên Mai,
được phát triển **dành riêng cho người dùng Linux**. Công lao thiết kế bộ gõ và toàn bộ
engine xử lý tiếng Việt thuộc về dự án gốc; phần Linux sử dụng **chung một engine
không sửa đổi** với các bản macOS và Windows, nên hành vi nhập liệu nhất quán.

Bản Linux có tên `h-openkey` để phân biệt với dự án gốc.

H-OpenKey chỉ dùng driver trực tiếp. Driver đọc sự kiện từ
`/dev/input/event*`, gọi OpenKeyCore và phát kết quả qua `/dev/uinput`. GNOME,
Chrome, Electron và terminal nhìn thấy đầu ra như từ một bàn phím phần cứng;
không có preedit hay giao thức chèn văn bản của ứng dụng.

| Cơ chế duy nhất | Tiếp nhận phím | Xuất văn bản |
| --- | --- | --- |
| Driver trực tiếp | `EVIOCGRAB` trên `/dev/input/event*` | Bàn phím kernel `/dev/uinput` + layout XKB cố định |

## Driver Wayland: evdev → OpenKeyCore → uinput

Đây là đường gần nhất với `SendInput` của Windows mà Linux cung cấp ở user space:

1. Tạo `H-OpenKey Virtual Keyboard` qua uinput và chờ compositor nhận thiết bị.
2. Kiểm tra bàn phím thật không dùng hai keycode modifier dành riêng.
3. Dùng `EVIOCGRAB` để sự kiện vật lý không đi thẳng tới compositor.
4. Phím không cần sửa được phát lại nguyên mã; kết quả Telex/VNI được phát thành
   Backspace và các keycode Unicode qua cùng bàn phím ảo.
5. Quan sát nút chuột mà không grab thiết bị trỏ; mỗi lần bấm sẽ ngắt bộ đệm của
   ô nhập cũ trước khi người dùng gõ vào ô mới. Event mask của evdev lọc bỏ
   chuyển động chuột/touchpad trước khi chúng vào hàng đợi của driver.
6. Khi tiến trình thoát hoặc gặp lỗi, đóng file descriptor sẽ huỷ bàn phím ảo
   và tự động trả grab cho thiết bị thật.
7. Khi logind hoặc GNOME báo màn hình đã khoá, backend bỏ qua engine và chuyển
   tiếp nguyên phím; mật khẩu khoá màn hình không bị Telex/VNI biến đổi hoặc ghi
   vào debug log của core.

Driver không grab một thiết bị giữa lúc có phím đang được giữ. Việc này bảo đảm
lần nhấn và lần nhả luôn đi qua cùng một thiết bị đối với Mutter, tránh trạng
thái phím kẹt và tự lặp sau khi service khởi động lại. Thiết bị bị bỏ qua tạm
thời sẽ được nhận khi `inotify` báo thay đổi thiết bị hoặc khi service khởi động
lại sau khi người dùng nhả hết phím.

Layout dùng tên `custom` đã được xkeyboard-config đăng ký sẵn. Trình cài đặt sinh
một bản symbols trong thư mục người dùng rồi cài bản đó thành
`/usr/share/X11/xkb/symbols/custom` bằng `sudo`, vì Mutter 46 không đọc symbols
người dùng khi tạo keymap cho seat. Trình cài đặt đổi riêng metadata hiển thị của
entry `custom` trong rules hệ thống thành **H-OpenKey Layout** và hoàn nguyên
khi gỡ cài đặt; ID vẫn là `xkb:custom`, mã ngắn là **HOK**. Trình cài đặt không
ghi đè một tệp `custom` có sẵn của người dùng. Phần symbols kế thừa `us(basic)`.
Hai keycode dưới giới hạn
X11 làm `ISO_Level3_Shift` và `ISO_Level5_Shift`; 24 phím chữ giữ nguyên ASCII ở
level 1/2 và dùng sáu level còn lại cho 142 code point tiếng Việt dựng sẵn và tổ
hợp. Hai mã modifier được chọn ngoài các ánh xạ `inet(evdev)` và được kiểm tra
va chạm trên từng máy trước khi grab. Cách này tránh lỗi bản cũ dùng mã evdev từ
352 trở lên: Mutter Wayland nhận được nhưng XWayland chỉ hỗ trợ keycode tới 255,
khiến Chrome nhận Backspace mà không nhận ký tự thay thế.

Do symbols và metadata hiển thị phải nằm trong XKB root mà Mutter thực sự đọc,
bản cập nhật gói `xkeyboard-config` có thể thay thế chúng. Driver sẽ phát hiện
mất chữ ký symbols và không grab bàn phím; nếu tên lại thành “A user-defined
custom Layout” hoặc driver báo sai layout, chạy lại installer rồi đăng xuất,
đăng nhập lại trên Wayland.

Ví dụ, chuỗi Telex `dd` đi qua duy nhất kênh EV_KEY:

```text
d lần 1  → EV_KEY(KEY_D)
d lần 2  → EV_KEY(KEY_BACKSPACE) → modifier + EV_KEY(phím carrier của “đ”)
```

Không có timer trên đường gõ, không đổi keymap khi đang nhập và không trộn sự
kiện bàn phím với `CommitText`. Bộ test dùng libxkbcommon biên dịch layout thật,
phát từng tổ hợp level và đối chiếu đủ 142 code point.

Luồng bàn phím ngủ trực tiếp trong `epoll_wait` và được kernel đánh thức khi có
sự kiện; Qt, biểu tượng khay và DBus không nằm trên đường gõ. Việc thêm hoặc gỡ
thiết bị được theo dõi bằng `inotify`, không quét `/dev/input` định kỳ. Phím
thường được phát thành tap hoàn chỉnh để compositor không tự tạo một luồng
repeat song song với `EV_KEY value=2` của bàn phím thật.

Thiết bị uinput chỉ khai báo mã bàn phím, bỏ toàn bộ dải nút chuột, joystick và
gamepad. Driver đồng thời loại thiết bị có tên `H-OpenKey Virtual Keyboard` khỏi
nhánh quan sát con trỏ, nên không tự đọc lại chính đầu ra của mình và libinput
không phân loại nó thành thiết bị lai bàn phím–chuột.

## Phạm vi đã kiểm chứng

Mã driver và layout đã được kiểm thử tự động. Quy trình cài đặt nguồn XKB hiện
nhắm tới GNOME Wayland; KDE, COSMIC và các desktop khác cần thêm bộ tích hợp cấu
hình layout trước khi được xem là hỗ trợ hoàn chỉnh. Không có backend thay thế:
nếu layout hoặc quyền thiết bị chưa đúng, ứng dụng báo lỗi và không bắt bàn phím.

## Biên dịch

Không cần quyền `sudo` nếu hệ thống đã có đầy đủ môi trường phát triển và các gói
phụ thuộc.

```sh
cmake -S Sources/OpenKey/linux -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Phụ thuộc: `qt6-base-dev`, `libxkbcommon-dev`, `pkg-config`, `cmake`, `g++`.

## Chạy ứng dụng

H-OpenKey có thể chạy song song với IBus, Fcitx và engine tiếng Nhật/Hàn:

```sh
./build/ui/h-openkey
```

Khi dùng engine khác, chuyển H-OpenKey sang tiếng Anh để driver chỉ chuyển tiếp
phím nguyên bản. Khi bật lại tiếng Việt, H-OpenKey yêu cầu GNOME chọn
**H-OpenKey Layout** (`xkb:custom`) trước rồi mới bật engine; thất bại thì giữ
chế độ tiếng Anh và báo ở khay, không phát carrier bằng keymap Mozc/IBus rồi
nuốt chữ. `gsettings` chỉ chạy tại lần chuyển chế độ, không nằm trên đường từng
phím. Ứng dụng không `pkill`, không tắt autostart và không xoá `GTK_IM_MODULE`,
`QT_IM_MODULE` hoặc `XMODIFIERS`.

Backend trực tiếp cần đọc `/dev/input/event*` và ghi `/dev/uinput`. Trình cài đặt
dùng ACL `uaccess` của udev để chỉ cấp quyền cho người dùng đang hoạt động tại
seat; nó không thêm tài khoản vào nhóm `input`. Quyền đọc evdev vẫn có thể quan
sát toàn bộ phím bấm, nên không cài hoặc chạy mã không đáng tin cậy.

Không nên bật đồng thời hai engine biến đổi cùng một chuỗi phím. Việc chuyển
H-OpenKey sang tiếng Anh là đủ để IBus/Fcitx xử lý tiếng Nhật hoặc tiếng Hàn;
không cần dừng bất kỳ tiến trình nào.

Ba tuỳ chọn nhớ ngôn ngữ, nhớ bảng mã theo ứng dụng và tự nhận bố cục khác được
ẩn trên driver trực tiếp vì backend không có app-id/focus đáng tin cậy. Nếu bàn
phím còn báo phím giữ lúc khởi động, driver theo dõi chính fd đó và grab ngay khi
nhận lần nhả cuối; sau ba giây vẫn pending, khay sẽ hiện cảnh báo mà không polling
thiết bị.

> [!CAUTION]
> **Tuyệt đối không tùy tiện bật nhật ký chẩn đoán.** Log ghi mã phím và văn bản
> do bộ gõ tạo ra; mật khẩu, mã OTP, khóa API, thông tin thanh toán và dữ liệu
> riêng tư được nhập trong lúc ghi có thể nằm dưới dạng văn bản thuần trong tệp.
> Chỉ bật ngay trước khi tái hiện lỗi, tắt ngay sau đó và tự kiểm tra toàn bộ tệp
> trước khi gửi cho bất kỳ ai.

Có thể bật nhật ký chẩn đoán theo hai cách: đặt `OPENKEY_DEBUG=1` khi chạy từ
terminal (nội dung được ghi ra stderr), hoặc chọn **Bắt đầu ghi nhật ký** trong
bảng điều khiển → tab *Hệ thống* (ghi ra
`~/.local/share/h-openkey/debug.log`, có mốc thời gian tới mili giây). Nội dung
không được tự động truyền ra ngoài; việc bảo quản và chia sẻ hoàn toàn do người
dùng chủ động thực hiện. Nếu đặt `OPENKEY_DEBUG=1` cho service, stderr sẽ đi vào
`journald`, có thể tồn tại lâu hơn file `0600` và đọc được theo chính sách journal
của hệ thống; tuyệt đối không bật biến này thường trực. Khi ring buffer đầy,
đường gõ bỏ bản ghi và flusher chèn `[dropped N]` vào đầu ra để báo chính xác số
bản ghi bị thiếu.

## Cài đặt bằng một lệnh

[Tải H-OpenKey Linux 1.3.0](https://github.com/hieu8123/h-openkey/releases/tag/linux-v1.3.0),
hoặc cài đặt trực tiếp:

> [!WARNING]
> Tích hợp tự động hiện chỉ hỗ trợ GNOME có `gsettings`, với **H-OpenKey Layout**
> dựa trên US QWERTY. Trình cài đặt hỏi xác nhận trước lần đầu thêm layout và
> dừng sớm trên desktop khác. AZERTY, QWERTZ, Dvorak và Colemak chưa được hỗ trợ
> đúng.

```sh
curl -fsSL https://raw.githubusercontent.com/hieu8123/h-openkey/master/Sources/OpenKey/linux/packaging/install.sh | bash
```

Trình cài đặt tự động cài các gói phụ thuộc theo bản phân phối (`apt`, `dnf`,
`pacman`, `zypper`), chỉ tải gói nguồn đính kèm bản phát hành mới nhất, biên dịch, **chạy
kiểm thử trước khi cài đặt** và tạo dịch vụ systemd cho người dùng. Trên GNOME
Wayland, trình cài đặt cài quy tắc udev, thêm layout `custom` mà không xoá các
nguồn nhập hiện có, rồi chọn backend `driver`.
Layout dùng `symbols/custom` trong XKB root hệ thống; installer giữ ID chuẩn này
nhưng đổi metadata hiển thị trong rules thành **H-OpenKey Layout**. Gỡ cài đặt
bằng `bash install.sh --uninstall` sẽ hoàn nguyên metadata nếu nó chưa bị thành
phần khác thay đổi.

Việc biên dịch tại máy giúp tệp thực thi liên kết với đúng phiên bản Qt6 của hệ
thống, tránh vấn đề tương thích giữa các bản phân phối.

## Cài đặt thủ công

```sh
cmake -S Sources/OpenKey/linux -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build
cmake --install build
./build/ui/h-openkey --configure-driver
```

Lệnh cuối cài layout vào `~/.config/xkb` và chọn backend driver, nhưng không cấp
quyền thiết bị. Có thể chép quy tắc đi kèm rồi đăng xuất, đăng nhập lại:

```sh
sudo install -Dm0644 Sources/OpenKey/linux/packaging/70-h-openkey.rules \
  /etc/udev/rules.d/70-h-openkey.rules
sudo modprobe uinput
sudo udevadm control --reload-rules
sudo install -m0644 ~/.config/xkb/symbols/hopenkey \
  /usr/share/X11/xkb/symbols/custom
# Thêm và chọn layout `custom` trong Cài đặt GNOME → Bàn phím → Nguồn nhập;
# tên “H-OpenKey Layout” được trình cài đặt tự động đặt trong metadata hệ thống.
# giữ nguyên các nguồn IBus/Fcitx, tiếng Nhật và tiếng Hàn đang có.
setxkbmap -layout custom  # đồng bộ các ứng dụng XWayland trong phiên hiện tại
```

Chạy cùng phiên đăng nhập. Unit systemd được cài vào `$PREFIX/lib/systemd/user`,
đúng chỗ khi cài toàn hệ thống; với bản cài vào `~/.local` thì phải liên kết nó
sang thư mục systemd đọc được:

```sh
mkdir -p ~/.config/systemd/user
ln -sf ~/.local/lib/systemd/user/h-openkey.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable h-openkey.service
```

Sau đó bật service; nếu ứng dụng Wayland đang mở vẫn giữ keymap cũ, đăng xuất rồi
đăng nhập lại một lần.

## Cấu hình

`~/.config/openkey/config.json` chứa các tuỳ chọn dạng số của engine. Khoá
`backend` luôn được ghi là `driver`; giá trị từ bản cũ cũng được tự nâng cấp sang
driver. Bảng gõ tắt và bảng nhớ mã theo ứng dụng nằm ở `macro.dat` và
`smartswitch.dat` cùng thư mục, dùng đúng định dạng nhị phân sẵn có của engine;
UI driver hiện ẩn phần nhớ theo ứng dụng vì chưa có app-id đáng tin cậy.

## Cấu trúc

```
core/       thuần logic, không phụ thuộc desktop hay Qt — kiểm thử bằng FakeBackend
backends/   driver/  evdev + uinput + layout XKB H-OpenKey
            evdev/   chặn phím vật lý ở kernel (EVIOCGRAB)
ui/         Qt6: biểu tượng khay, bảng điều khiển
tests/      kiểm tra engine và đủ 142 ánh xạ Unicode qua libxkbcommon
```

Các quyết định đang được thực thi trực tiếp trong `backends/driver`,
`backends/evdev` và bộ kiểm thử đi kèm; kho mã nguồn không giữ tài liệu thiết kế
nội bộ đã lỗi thời.
