# OpenKey cho Linux

Bộ gõ tiếng Việt nguồn mở cho Linux, dùng chung engine với bản macOS và Windows.

OpenKey **không** chạy trên ibus hay fcitx5. Nó tự làm input method: bắt bàn phím
bằng `zwp_input_method_v2` rồi trả chữ về bằng `delete_surrounding_text` +
`commit_string`. Không dùng preedit — đó là nguồn gốc của lỗi gạch chân và nhân
đôi chữ mà OpenKey sinh ra để loại bỏ.

## Trạng thái

| Giai đoạn | Nội dung | Trạng thái |
| --- | --- | --- |
| 1 | Gõ được trên Wayland (`zwp_input_method_v2`) | đang làm |
| 2 | Bảng điều khiển Qt6 đầy đủ | chưa |
| 3 | X11 và XWayland (Chrome, VS Code, Electron) | chưa |

Hết giai đoạn 1, chỉ những ứng dụng nói `text-input-v3` mới gõ được: GTK4, GTK3
với `GTK_IM_MODULE=wayland`, Qt6, Firefox. Chrome và các ứng dụng Electron phải
đợi giai đoạn 3.

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
./build/ui/openkey
```

Lưu ý về cosmic-comp: khi đã có bộ gõ khác giữ chỗ, nó **không** gửi sự kiện
`unavailable` mà chỉ lặng lẽ không bao giờ kích hoạt client thứ hai. Triệu chứng
là OpenKey chạy bình thường nhưng không gõ ra chữ nào. Vì vậy OpenKey tự dò các
tiến trình `fcitx5`, `fcitx`, `ibus-daemon` lúc khởi động và cảnh báo.

Đặt `OPENKEY_DEBUG=1` để xem nhật ký sự kiện: `activate`, `done`, từng phím nhận
được và từng lần xoá/chèn.

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
