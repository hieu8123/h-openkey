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

Driver không grab một thiết bị giữa lúc có phím đang được giữ. Việc này bảo đảm
lần nhấn và lần nhả luôn đi qua cùng một thiết bị đối với Mutter, tránh trạng
thái phím kẹt và tự lặp sau khi service khởi động lại. Thiết bị bị bỏ qua tạm
thời sẽ được nhận khi `inotify` báo thay đổi thiết bị hoặc khi service khởi động
lại sau khi người dùng nhả hết phím.

Layout dùng tên `custom` đã được xkeyboard-config đăng ký sẵn. Trình cài đặt sinh
một bản symbols trong thư mục người dùng rồi cài bản đó thành
`/usr/share/X11/xkb/symbols/custom` bằng `sudo`, vì Mutter 46 không đọc symbols
người dùng khi tạo keymap cho seat. Trình cài đặt không sửa `evdev.xml` của
distro và không ghi đè một tệp `custom` có sẵn của người dùng. Phần symbols kế
thừa `us(basic)`. Hai keycode dưới giới hạn
X11 làm `ISO_Level3_Shift` và `ISO_Level5_Shift`; 24 phím chữ giữ nguyên ASCII ở
level 1/2 và dùng sáu level còn lại cho 142 code point tiếng Việt dựng sẵn và tổ
hợp. Hai mã modifier được chọn ngoài các ánh xạ `inet(evdev)` và được kiểm tra
va chạm trên từng máy trước khi grab. Cách này tránh lỗi bản cũ dùng mã evdev từ
352 trở lên: Mutter Wayland nhận được nhưng XWayland chỉ hỗ trợ keycode tới 255,
khiến Chrome nhận Backspace mà không nhận ký tự thay thế.

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

Chỉ chạy một bộ gõ độc lập. Dừng fcitx/fcitx5 trước khi bật H-OpenKey:

```sh
systemctl --user stop app-org.fcitx.Fcitx5@autostart.service
./build/ui/h-openkey
```

Nếu phiên hiện tại vẫn có `GTK_IM_MODULE=fcitx`, `QT_IM_MODULE=fcitx` hoặc
`XMODIFIERS=@im=fcitx`, hãy đăng xuất rồi đăng nhập lại.

Backend trực tiếp cần đọc `/dev/input/event*` và ghi `/dev/uinput`. Trình cài đặt
tự cài quy tắc udev; nếu tự biên dịch, tài khoản cần thuộc nhóm `input`:

```sh
sudo usermod -aG input $USER   # rồi ĐĂNG XUẤT và đăng nhập lại
```

Quyền `input` có thể đọc toàn bộ phím bấm. Không cấp quyền cho tài khoản hoặc
chương trình không đáng tin cậy.

H-OpenKey phát hiện tiến trình và cấu hình tự khởi động của fcitx5, fcitx cùng
một số bộ gõ phổ biến khác. Nếu người dùng đồng ý, ứng dụng sẽ tắt bộ gõ gây
xung đột; nếu từ chối, H-OpenKey sẽ tắt để tránh hai bộ cùng sửa luồng phím.

Có thể bật nhật ký chẩn đoán theo hai cách: đặt `OPENKEY_DEBUG=1` khi chạy từ
terminal (nội dung được ghi ra stderr), hoặc chọn **Bắt đầu ghi nhật ký** trong
bảng điều khiển → tab *Hệ thống* (ghi ra
`~/.local/share/h-openkey/debug.log`, có mốc thời gian tới mili
giây). Nội dung gồm từng phím nhận được, từng lần xoá/chèn, keycode đã phát, và
các lần đổi cửa sổ focus.

## Cài đặt bằng một lệnh

```sh
curl -fsSL https://raw.githubusercontent.com/hieu8123/OpenKey/master/Sources/OpenKey/linux/packaging/install.sh | bash
```

Trình cài đặt tự động cài các gói phụ thuộc theo bản phân phối (`apt`, `dnf`,
`pacman`, `zypper`), tải mã nguồn từ bản phát hành mới nhất, biên dịch, **chạy
kiểm thử trước khi cài đặt**, tạo dịch vụ systemd cho người dùng và yêu cầu xác
nhận trước khi tắt bộ gõ cũ. Trên GNOME Wayland, trình cài đặt cài quy tắc udev,
layout `custom`, lưu nguồn nhập cũ để khôi phục và chọn backend `driver`.
Layout dùng `symbols/custom` trong XKB root hệ thống; tên `custom` vốn đã có
trong registry chuẩn nên không cần sửa rules của distro. Gỡ cài đặt bằng
`bash install.sh --uninstall`.

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
sudo usermod -aG input "$USER"
sudo modprobe uinput
sudo udevadm control --reload-rules
sudo install -m0644 ~/.config/xkb/symbols/hopenkey \
  /usr/share/X11/xkb/symbols/custom
gsettings set org.gnome.desktop.input-sources sources "[('xkb', 'custom')]"
gsettings set org.gnome.desktop.input-sources current 'uint32 0'
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

Cần dừng và vô hiệu hoá autostart của bộ gõ cũ trước khi kích hoạt OpenKey:

```sh
systemctl --user stop app-org.fcitx.Fcitx5@autostart.service
mv ~/.config/autostart/org.fcitx.Fcitx5.desktop{,.disabled}
```

## Cấu hình

`~/.config/openkey/config.json` chứa các tuỳ chọn dạng số của engine. Khoá
`backend` luôn được ghi là `driver`; giá trị từ bản cũ cũng được tự nâng cấp sang
driver. Bảng gõ tắt và bảng nhớ mã theo ứng dụng nằm ở `macro.dat` và
`smartswitch.dat` cùng thư mục, dùng đúng định dạng nhị phân sẵn có của engine.

## Cấu trúc

```
core/       thuần logic, không phụ thuộc desktop hay Qt — kiểm thử bằng FakeBackend
backends/   driver/  evdev + uinput + layout XKB H-OpenKey
            evdev/   chặn phím vật lý ở kernel (EVIOCGRAB)
ui/         Qt6: biểu tượng khay, bảng điều khiển
tests/      kiểm tra engine và đủ 142 ánh xạ Unicode qua libxkbcommon
```

Thiết kế đầy đủ: `docs/superpowers/specs/2026-07-28-openkey-linux-design.md`.
