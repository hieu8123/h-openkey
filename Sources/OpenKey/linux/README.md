# H-OpenKey — OpenKey cho Linux

Bộ gõ tiếng Việt cho Linux.

Đây là bản **fork** của [OpenKey](https://github.com/tuyenvm/OpenKey) của Tuyên Mai,
làm ra **chỉ để hỗ trợ người dùng Linux**. Công lao thiết kế bộ gõ và toàn bộ
engine xử lý tiếng Việt thuộc về dự án gốc; phần Linux dùng **chung một engine
không sửa đổi** với bản macOS và Windows, nên cách gõ giống hệt.

Bản Linux có tên `h-openkey` để không lẫn với bản gốc.

OpenKey **không** chạy trên ibus hay fcitx5. Nó tự bắt bàn phím, và cách bắt khác
nhau theo phiên làm việc:

| Phiên | Bắt phím | Trả chữ về |
| --- | --- | --- |
| X11 / Xorg | `EVIOCGRAB` trên `/dev/input/event*` (tầng kernel) | XTEST + keycode gán sẵn |
| Wayland | `zwp_input_method_v2` | BackSpace ảo + `commit_string` |

Không dùng preedit ở cả hai — đó là nguồn gốc của lỗi gạch chân và nhân đôi chữ
mà OpenKey sinh ra để loại bỏ.

## Phạm vi đã kiểm chứng

**X11 là đường chạy tốt nhất**, đã kiểm chứng trên Zorin OS (GNOME/Xorg): chặn
phím ở tầng kernel nên không dính giới hạn nào của giao thức input method.

Trên **Pop!_OS (COSMIC)** gõ được nhưng vướng hai giới hạn của Wayland mà không
sửa được từ phía bộ gõ — menu chuột phải của dock/panel không mở được
([cosmic-comp#1763](https://github.com/pop-os/cosmic-comp/issues/1763)), và phím
tắt đổi chế độ chỉ chạy khi con trỏ đang ở ô nhập văn bản. Chi tiết ở
[README gốc repo](../../../README.md).

## Backend X11: chặn ở kernel, không phải XRecord

XRecord **chỉ quan sát được phím, không chặn được**. Nghĩa là phím gốc đã tới ứng
dụng trước khi bộ gõ kịp chạy, nên lúc nào cũng phải "gõ đè lên sau" — race
condition là không tránh khỏi, và đó chính là bệnh mất chữ khi gõ nhanh. Cách duy
nhất chặn được thật là xuống tầng kernel: `EVIOCGRAB` trên `/dev/input/event*`.
Sau khi grab, **không ai khác** — kể cả X server — nhận được sự kiện của thiết bị
đó nữa. Đổi lại phải thuộc nhóm `input`.

Bốn thứ phải tự lo sau khi grab, mỗi thứ đều là một lỗi đã đo được:

1. **Không được đổi keymap lúc đang gõ.** Ứng dụng tra chữ bằng bản sao keymap
   của riêng nó, chỉ cập nhật khi xử lý tới `MappingNotify`. Gõ nhanh thì phím
   tới **trước** thông báo đó, ứng dụng tra ra `NoSymbol` và **bỏ luôn ký tự**.
   Vì vậy 237 ký tự được gán sẵn vào keycode trống **một lần lúc khởi động**
   (67 cặp hoa/thường tiếng Việt + 8 dấu tổ hợp, ASCII thì dùng thẳng phím thật).
2. **Không được bấm một phím đang được giữ.** Gõ nhanh là gõ gối đầu — phím trước
   chưa nhả thì phím sau đã bấm. Lúc bơm lại chính phím đó để xuất chữ, X thấy nó
   *đang bấm* nên **lọc bỏ lệnh bấm**, chỉ release lọt qua và ký tự biến mất. Đo
   bằng `xev`: lần hỏng chỉ có `R kc=38`, không hề có `P kc=38`. Vì vậy
   `tapKeycode()` tự nhả phím đó ra trước khi bơm.
3. **Keysym Latin-1 phải đúng chuẩn.** Ký tự trong `U+0020..U+00FF` phải dùng
   thẳng giá trị mã (`â` = `0x00E2`), không phải dạng Unicode `0x010000E2` —
   nhiều ứng dụng không đổi ngược dạng phi chuẩn ra ký tự nên bỏ qua phím đó.
4. **Phải tắt lặp phím của X** (`XAutoRepeatOff`). Bàn phím thật đã bị chặn ở
   kernel nên mọi lần lặp phải đi ra từ evdev; để X tự lặp thêm thì màn hình có
   ký tự mà engine không biết, sổ sách đếm xoá lệch ngay.

Ngoài ra keycode đã mượn phải **trả về `NoSymbol` lúc thoát**, nếu không mỗi lần
chạy lại mất dần cho tới khi cạn sạch; và `/dev/input` phải **quét lại định kỳ**
vì bàn phím không dây hay ngắt rồi hiện lại dưới node khác.

## Đầu ra trên Wayland: vì sao lại rắc rối đến vậy

Trên Windows và macOS, xoá và chèn là **cùng một loại vật thể**: `SendInput` với
`KEYEVENTF_UNICODE`, hoặc `CGEventKeyboardSetUnicodeString` — cùng một hàng đợi
nằm dưới ứng dụng, thứ tự do hàng đợi bảo đảm, ứng dụng không phân biệt được với
gõ tay thật. Không có gì để trộn.

Wayland cố tình không có cả hai thứ đó. Muốn nuốt phím thì **buộc phải làm input
method**; mà làm input method thì ứng dụng chuyển sang chờ chữ ở kênh text-input.
Thành ra chữ đi ra bằng **hai đường** và không có gì bảo đảm thứ tự giữa chúng:

| Đường | Dùng cho |
| --- | --- |
| Bàn phím ảo | BackSpace, chuyển tiếp phím, gõ chữ bằng keymap ghép sẵn |
| text-input | `commit_string` |

Các bộ soạn thảo giàu định dạng trên web (ô chat Facebook dùng Lexical, và
Draft.js/ProseMirror) tự dựng lại nội dung theo mô hình riêng, nên khi hai đường
đến lệch nhau thì một trong hai bị nuốt mất — không đoán trước được bên nào:

```
mất phần xoá:   "ting vie" + (xoá 1, chèn "ê")  →  "ting vieê"
mất phần chèn:  "tie"      + (xoá 1, chèn "ê")  →  "ti"
```

Cách xử lý: **một hàng đợi FIFO duy nhất** cho mọi thao tác. Trong cùng một đường
thì chạy liền không chờ; chỉ khi **đổi đường** mới dừng lại chờ ứng dụng báo đã
xử lý xong (`surrounding_text`), có hạn chót 60 ms để không bao giờ kẹt.

Đường đi chọn theo ứng dụng, mỗi nhánh dựa trên đo đạc thực tế:

| Ứng dụng | Xoá | Chèn |
| --- | --- | --- |
| Không có phiên text-input (XWayland) | BackSpace ảo | keymap ghép sẵn |
| Firefox | BackSpace ảo | keymap ghép sẵn |
| Chrome / Chromium / Edge… | BackSpace ảo | `commit_string`, kể cả ký tự thường |
| Còn lại (VS Code, terminal…) | BackSpace ảo | `commit_string` |

Với họ Chromium, ký tự thường đi đường text-input **không phải vì đường phím
hỏng** — nó chạy tốt — mà để khỏi phải đổi đường: gõ một chữ có dấu vì thế chỉ
tốn một lần chờ thay vì hai.

### Ba hướng đã thử và đã chết

Ghi lại để khỏi ai đi lại:

1. **Chọn cơ chế theo app-id đơn thuần** — không được. Trong *cùng* một app-id
   `google-chrome`, ô nhập thường xử lý `delete_surrounding_text` đúng, còn ô chat
   Facebook bỏ qua nó. Không có tín hiệu nào ở tầng giao thức tách được hai ô nằm
   trong cùng một trình duyệt.
2. **Dùng `delete_surrounding_text` cho mọi ứng dụng có surrounding text** — hỏng
   VS Code và terminal: chúng **báo cáo** surrounding text nhưng **bỏ qua** yêu
   cầu xoá, vì văn bản thuộc về bộ soạn dòng của shell / xterm.js chứ không phải
   ô nhập của toolkit. Chữ dồn lại thành rác (`"sửa"` ra `"suaửa"`).
3. **Đưa tất cả qua bàn phím ảo** — hỏng Chrome và terminal: chúng không nhận các
   keycode tự chế trong keymap ghép. Firefox thì nhận, nên nhánh Firefox giữ lại
   cách này.

Ngoài ra `surrounding_text` **không đáng tin ở mọi nơi**: VS Code báo
`" đây à õ ên"` trong khi màn hình hiện đúng `"đây là gõ trên"`. Vì vậy nó chỉ
được dùng làm tín hiệu báo-xong cho họ Chromium.

Keymap ghép sẵn có trần cứng: khoảng mã phím trống chỉ còn 249 chỗ (trên 767 là
vượt `KEY_MAX` của evdev), danh sách hiện dùng 237. Muốn thêm — ví dụ khoảng
`U+00A0..U+00FF` cho bảng mã TCVN3/VNI — thì phải giải phóng 95 chỗ của ASCII
trước, bằng cách trả ký tự ASCII về phím thật thay vì cấp phím riêng.

## Build

Không cần `sudo`, không cần cài thêm gì trên một máy đã có môi trường phát triển
thông thường.

```sh
cmake -S Sources/OpenKey/linux -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Phụ thuộc: `qt6-base-dev`, `libwayland-dev`, `wayland-protocols`,
`libxkbcommon-dev`, `libx11-dev`, `libxtst-dev`, `cmake`, `g++`.

## Chạy

Trên **Wayland**, chỉ **một** bộ gõ được giữ input method của phiên. Phải tắt hẳn
bộ gõ khác trước (trên X11 thì không cần, vì phím bị chặn từ tầng kernel):

```sh
systemctl --user stop app-org.fcitx.Fcitx5@autostart.service   # hoặc ibus exit
./build/ui/h-openkey
```

Trên **X11**, backend cần đọc được `/dev/input/event*` để `EVIOCGRAB`. Thiếu
quyền thì nó báo `khong co quyen doc /dev/input/event*` rồi dừng:

```sh
sudo usermod -aG input $USER   # rồi ĐĂNG XUẤT và đăng nhập lại
```

Script cài tự làm bước này; build tay thì phải tự chạy.

Lưu ý về cosmic-comp: khi đã có bộ gõ khác giữ chỗ, nó **không** gửi sự kiện
`unavailable` mà chỉ lặng lẽ không bao giờ kích hoạt client thứ hai. Triệu chứng
là OpenKey chạy bình thường nhưng không gõ ra chữ nào. Vì vậy OpenKey tự dò các
tiến trình `fcitx5`, `fcitx`, `ibus-daemon` lúc khởi động và cảnh báo.

Nhật ký chẩn đoán bật được hai cách: `OPENKEY_DEBUG=1` lúc chạy từ terminal (in
ra stderr), hoặc nút **Bắt đầu ghi nhật ký** trong bảng điều khiển → tab *Hệ
thống* (ghi ra `~/.local/share/h-openkey/debug.log`, có mốc thời gian tới mili
giây). Nội dung gồm từng phím nhận được, từng lần xoá/chèn, keycode đã bơm, và
các lần đổi cửa sổ focus.

## Cài đặt bằng một lệnh

```sh
curl -fsSL https://raw.githubusercontent.com/hieu8123/OpenKey/master/Sources/OpenKey/linux/packaging/install.sh | bash
```

Script tự cài phụ thuộc theo distro (apt, dnf, pacman, zypper), tải mã nguồn từ
bản phát hành mới nhất, build, **chạy kiểm thử trước khi cài**, dựng systemd user
service, và hỏi trước khi tắt bộ gõ cũ. Gỡ ra bằng `bash install.sh --uninstall`.

Vì sao build tại máy thay vì tải binary sẵn: bản binary phụ thuộc phiên bản Qt6
của máy build, nên hay vỡ khi đem sang distro khác.

## Cài đặt thủ công

```sh
cmake -S Sources/OpenKey/linux -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build
cmake --install build
```

Chạy cùng phiên đăng nhập. Unit systemd được cài vào `$PREFIX/lib/systemd/user`,
đúng chỗ khi cài toàn hệ thống; với bản cài vào `~/.local` thì phải liên kết nó
sang thư mục systemd đọc được:

```sh
mkdir -p ~/.config/systemd/user
ln -sf ~/.local/lib/systemd/user/h-openkey.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now h-openkey.service
```

Nhớ tắt hẳn bộ gõ cũ, nếu không OpenKey sẽ không bao giờ được kích hoạt:

```sh
systemctl --user stop app-org.fcitx.Fcitx5@autostart.service
mv ~/.config/autostart/org.fcitx.Fcitx5.desktop{,.disabled}
```

## Cấu hình

`~/.config/openkey/config.json` — các tuỳ chọn dạng số của engine, kèm khoá
`backend` nhận `auto`, `wayland` hoặc `x11`. Bảng gõ tắt và bảng nhớ mã theo ứng
dụng nằm ở `macro.dat` và `smartswitch.dat` cùng thư mục, dùng đúng định dạng nhị
phân sẵn có của engine.

## Cấu trúc

```
core/       thuần logic, không phụ thuộc Wayland/X11/Qt — kiểm thử bằng FakeBackend
backends/   evdev/   chặn phím ở kernel (EVIOCGRAB), dùng cho backend X11
            wayland/ input-method-v2 + virtual-keyboard
            x11/     evdev + XTEST
ui/         Qt6: biểu tượng khay, bảng điều khiển
tests/      bơm chuỗi phím, đối chiếu văn bản ra — không cần compositor
```

Thiết kế đầy đủ: `docs/superpowers/specs/2026-07-28-openkey-linux-design.md`.
