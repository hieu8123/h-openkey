# OpenKey cho Linux

Bộ gõ tiếng Việt nguồn mở cho Linux, dùng chung engine với bản macOS và Windows.

OpenKey **không** chạy trên ibus hay fcitx5. Nó tự làm input method: bắt bàn phím
bằng `zwp_input_method_v2` rồi trả chữ về bằng `delete_surrounding_text` +
`commit_string`. Không dùng preedit — đó là nguồn gốc của lỗi gạch chân và nhân
đôi chữ mà OpenKey sinh ra để loại bỏ.

## Trạng thái

| Giai đoạn | Nội dung | Trạng thái |
| --- | --- | --- |
| 1 | Gõ được trên Wayland (`zwp_input_method_v2`) | xong |
| 2 | Bảng điều khiển Qt6 đầy đủ | xong |
| 3 | X11 và XWayland | không cần nữa, xem bên dưới |

Giai đoạn 3 ban đầu định viết một backend X11 riêng (XRecord + XTEST) để phủ
Chrome, VS Code và các ứng dụng XWayland. Khi chạy thử mới thấy không cần: grab
của OpenKey nhận được phím **kể cả** khi không có ô nhập `text-input-v3` nào,
nên vấn đề chỉ nằm ở đầu ra. Vì vậy OpenKey xuất chữ theo hai đường:

- Ứng dụng có gửi surrounding text → `delete_surrounding_text` + `commit_string`
- Còn lại → bàn phím ảo với keymap sinh động (kỹ thuật của `wtype`)

Có một cái bẫy: một số ứng dụng (cosmic-term) **báo cáo** surrounding text nhưng
lại **bỏ qua** `delete_surrounding_text`. Điều này hợp lý về bản chất — terminal
không sở hữu văn bản, bộ soạn dòng của shell mới sở hữu. Không có cách nào biết
trước qua giao thức, nên OpenKey **tự đo**: sau lần xoá đầu tiên trong một ứng
dụng, nó đối chiếu độ dài văn bản mà ứng dụng báo về với độ dài đáng ra phải có.
Nếu lệnh xoá bị bỏ qua thì chuyển sang phím BackSpace thật, và **ghi nhớ kết luận
đó theo từng ứng dụng** để chỉ phải đo một lần.

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
