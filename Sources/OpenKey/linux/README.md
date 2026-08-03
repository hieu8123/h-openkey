# H-OpenKey — OpenKey cho Linux

Bộ gõ tiếng Việt cho Linux.

Đây là bản **fork** của [OpenKey](https://github.com/tuyenvm/OpenKey) của Tuyên Mai,
làm ra **chỉ để hỗ trợ người dùng Linux**. Công lao thiết kế bộ gõ và toàn bộ
engine xử lý tiếng Việt thuộc về dự án gốc; phần Linux dùng **chung một engine
không sửa đổi** với bản macOS và Windows, nên cách gõ giống hệt.

Bản Linux có tên `h-openkey` để không lẫn với bản gốc.

OpenKey **không** chạy trên ibus hay fcitx5. Nó tự làm input method: bắt bàn phím
bằng `zwp_input_method_v2` rồi trả chữ về bằng phím BackSpace + `commit_string`.
Không dùng preedit — đó là nguồn gốc của lỗi gạch chân và nhân đôi chữ mà OpenKey
sinh ra để loại bỏ.

## Phạm vi đã kiểm chứng

**Chỉ Pop!_OS (COSMIC) là đã chạy ổn định 100%.** Ubuntu và các distro khác chưa
được kiểm chứng — xem [README ở gốc repo](../../../README.md) để biết chi tiết
điều kiện compositor.

## Đầu ra: vì sao lại rắc rối đến vậy

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
`libxkbcommon-dev`, `cmake`, `g++`.

## Chạy

Chỉ **một** bộ gõ được giữ input method của phiên Wayland. Phải tắt hẳn bộ gõ
khác trước:

```sh
systemctl --user stop app-org.fcitx.Fcitx5@autostart.service   # hoặc ibus exit
./build/ui/h-openkey
```

Lưu ý về cosmic-comp: khi đã có bộ gõ khác giữ chỗ, nó **không** gửi sự kiện
`unavailable` mà chỉ lặng lẽ không bao giờ kích hoạt client thứ hai. Triệu chứng
là OpenKey chạy bình thường nhưng không gõ ra chữ nào. Vì vậy OpenKey tự dò các
tiến trình `fcitx5`, `fcitx`, `ibus-daemon` lúc khởi động và cảnh báo.

Đặt `OPENKEY_DEBUG=1` để xem nhật ký sự kiện: `activate`, `done`, từng phím nhận
được và từng lần xoá/chèn.

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
backends/   wayland/ (giai đoạn 1), x11/ (giai đoạn 3)
ui/         Qt6: biểu tượng khay, bảng điều khiển
tests/      bơm chuỗi phím, đối chiếu văn bản ra — không cần compositor
```

Thiết kế đầy đủ: `docs/superpowers/specs/2026-07-28-openkey-linux-design.md`.
