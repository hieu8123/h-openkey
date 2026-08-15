# H-OpenKey

[![Giấy phép](https://img.shields.io/badge/gi%E1%BA%A5y%20ph%C3%A9p-GPL--3.0-blue.svg)](LICENSE)
[![Phiên bản](https://img.shields.io/badge/phi%C3%AAn%20b%E1%BA%A3n-1.3.0-brightgreen.svg)](CHANGELOG.md)
[![Nền tảng](https://img.shields.io/badge/n%E1%BB%81n%20t%E1%BA%A3ng-Linux-lightgrey.svg)](#-tương-thích)
[![Backend](https://img.shields.io/badge/backend-evdev%20%2B%20uinput-orange.svg)](Sources/OpenKey/linux/README.md)
[![Standard Readme](https://img.shields.io/badge/readme%20style-standard-brightgreen.svg)](https://github.com/RichardLitt/standard-readme)

> Bộ gõ tiếng Việt trực tiếp cho Linux — không IBus, không preedit, không độ trễ nhân tạo

H-OpenKey đem engine của [OpenKey](https://github.com/tuyenvm/OpenKey) lên Linux
bằng một driver đầu vào trực tiếp. Engine xử lý tiếng Việt được giữ nguyên, nên
Telex, VNI, bảng mã và quy tắc đặt dấu nhất quán với OpenKey trên macOS/Windows.

H-OpenKey chỉ có một đường nhập liệu: đọc mã phím vật lý từ
`/dev/input/event*`, xử lý bằng OpenKeyCore rồi phát kết quả qua bàn phím ảo
`/dev/uinput`. Môi trường đồ họa và ứng dụng nhận các sự kiện đó như từ một bàn
phím phần cứng. H-OpenKey không dùng IBus, preedit, `CommitText`,
`DeleteSurroundingText`, XTEST hay khoảng chờ khi sửa dấu.

Bản Linux mang tên `h-openkey` để phân biệt với dự án gốc.

### Điểm nổi bật

- Gõ trực tiếp trong Chrome, Facebook/Lexical, Electron, VS Code, terminal và
  ứng dụng GTK/Qt bằng cùng một đường sự kiện bàn phím.
- Không có gạch chân tiền soạn, không `CommitText`, không timer và không chờ
  render trước khi xử lý phím kế tiếp.
- Luồng bàn phím riêng ngủ trong `epoll_wait`; Qt, biểu tượng khay và DBus không
  nằm trên đường gõ. Hotplug dùng `inotify`, không quét thiết bị định kỳ.
- Chỉ có một backend `evdev → OpenKeyCore → uinput`, không tự rơi sang một cơ
  chế khác có hành vi hoặc độ trễ khác.

## Mục lục

- [Bối cảnh](#-bối-cảnh)
- [Kiến trúc](#-kiến-trúc)
- [Tương thích](#-tương-thích)
- [Báo lỗi gõ](#-báo-lỗi-gõ)
- [Cài đặt](#-cài-đặt)
- [Sử dụng](#-sử-dụng)
- [Tính năng](#-tính-năng)
- [Tìm hiểu sâu](#-tìm-hiểu-sâu)
- [Đóng góp](#-đóng-góp)
- [Credits](#-credits)
- [Giấy phép](#-giấy-phép)

## 📖 Bối cảnh

OpenKey của Tuyên Mai là bộ gõ tiếng Việt nguồn mở cho macOS và Windows, nổi bật ở
kỹ thuật `Backspace` — thay vì giữ chữ trong vùng tiền soạn rồi mới chốt, nó sửa
trực tiếp vào ô nhập. Nhờ vậy không có gạch chân, không nhân đôi chữ.

Bản fork này bổ sung khả năng hỗ trợ Linux. Wayland không cấp cho ứng dụng thông
thường API toàn cục tương đương `SendInput` của Windows. H-OpenKey vì vậy làm
việc ở tầng thiết bị đầu vào của kernel: `EVIOCGRAB` chặn bàn phím vật lý và
`uinput` tạo bàn phím ảo. Một layout XKB riêng dùng hai keycode modifier và sáu
level bổ sung trên 24 phím chữ để phát đủ 142 ký tự tiếng Việt. Mọi keycode đều
nằm trong giới hạn của X11/XWayland, nên cùng dùng được trên Chrome chạy native
Wayland lẫn Chrome/Electron chạy qua XWayland. Chi tiết nằm trong
[tài liệu kỹ thuật](Sources/OpenKey/linux/README.md).

> [!IMPORTANT]
> Kể từ phiên bản 1.1, bản fork này **được phát triển độc lập và không còn theo
> dõi hoặc đồng bộ với kho mã nguồn gốc**. Dự án gốc là nền tảng ban đầu, không
> phải nguồn cập nhật cho bản fork này.

## ⚙️ Kiến trúc

```text
Bàn phím thật
    │  EVIOCGRAB
    ▼
Luồng epoll riêng ──► OpenKeyCore ──► /dev/uinput ──► XKB custom ──► Ứng dụng
       ▲
       └── inotify chỉ báo khi thiết bị được thêm hoặc gỡ
```

Phím vật lý được chặn một lần ở tầng kernel, xử lý đồng bộ rồi phát lại qua một
bàn phím ảo thuần `EV_KEY`. Phím thường được hoàn tất thành một tap ngay khi
nhấn; sự kiện giữ phím chỉ có một nguồn repeat, tránh kẹt phím hoặc sinh chuỗi
lặp. Backspace và ký tự tiếng Việt đi qua cùng thiết bị nên không có cuộc đua
giữa hai giao thức nhập liệu khác nhau.

Layout `xkb:custom` dùng các keycode nằm trong giới hạn của cả Wayland và
X11/XWayland. Driver vì vậy không cần biết ứng dụng đang dùng GTK, Qt, Chromium,
Electron, terminal hay trình soạn thảo web.

## ✅ Tương thích

Đường gõ chỉ cần Linux evdev/uinput và XKB, không phụ thuộc trình quản lý gói hay
API nhập văn bản của desktop. Khác biệt giữa các bản phân phối hiện nằm ở bước
cài phụ thuộc và kích hoạt layout, không nằm trong engine hoặc driver runtime.

| Phạm vi | Trạng thái | Ghi chú |
| --- | --- | --- |
| Chrome native Wayland và XWayland | 🟢 Đã kiểm chứng | Thanh tìm kiếm, trang web và Facebook/Lexical |
| Electron, VS Code và terminal | 🟢 Đã kiểm chứng | Không preedit, không giao thức chèn văn bản riêng |
| Ứng dụng GTK/Qt | 🟢 Hỗ trợ qua bàn phím hệ thống | Ứng dụng nhận thiết bị như bàn phím phần cứng |
| GNOME Wayland | 🟢 Cài đặt tự động | Tự cài và chọn `xkb:custom`; không cần `input-method-v2` |
| GNOME X11/Xorg | 🟢 Cùng driver | Đồng bộ XWayland/X11 bằng `setxkbmap` khi có `DISPLAY` |
| KDE Plasma, COSMIC, wlroots | 🟡 Driver tương thích | Cần bổ sung bước tự chọn layout theo từng desktop |

Trình cài đặt nhận diện các họ phân phối dùng `apt`, `dnf`, `pacman` và
`zypper`, bao gồm Debian/Ubuntu/Zorin/Pop!_OS, Fedora, Arch/Manjaro và openSUSE.
Quy trình kích hoạt layout tự động hiện yêu cầu GNOME `gsettings`; trên desktop
khác, driver dùng chung nhưng bước tích hợp layout vẫn cần cấu hình thủ công.

> [!IMPORTANT]
> Driver cần quyền đọc sự kiện bàn phím và ghi `/dev/uinput`. Quyền này có thể
> quan sát toàn bộ phím bấm, vì vậy chỉ nên cài từ mã nguồn đáng tin cậy. Trình
> cài đặt thêm quy tắc udev giới hạn quyền cho người dùng đang hoạt động tại
> seat, đồng thời dùng nhóm `input` làm phương án dự phòng.

Trên GNOME, trình cài đặt lưu danh sách nguồn nhập hiện tại, sau đó chỉ bật layout
`xkb:custom` có sẵn trong registry hệ thống. H-OpenKey cài symbols của mình cho
layout này; phím thường vẫn là bàn phím US, còn bàn phím ảo có thêm các level
Unicode. Khi gỡ cài đặt, danh sách nguồn nhập cũ được khôi phục. Nếu máy đã có
`symbols/custom` không phải do H-OpenKey tạo, trình cài đặt dừng và không ghi đè.

Không có cơ chế tự chuyển backend. Nếu driver chưa sẵn sàng, H-OpenKey giữ bảng
điều khiển và khay hệ thống để báo đúng nguyên nhân nhưng không bắt bàn phím.

Driver hiện không đọc app-id của cửa sổ đang nhận tiêu điểm, nên Smart Switch Key
theo từng ứng dụng chưa hoạt động trên đường này. Việc gõ và sửa dấu không phụ
thuộc app-id.

## 🐞 Báo lỗi gõ

Lỗi gõ hầu hết liên quan đến race condition hoặc thứ tự sự kiện và chỉ tái hiện
trong một số môi trường cụ thể. Bảng điều khiển cung cấp công cụ ghi nhật ký để
thu thập dữ liệu chẩn đoán:

1. Mở bảng điều khiển → tab **Hệ thống** → mục **Chẩn đoán lỗi gõ**
2. Bấm **Bắt đầu ghi nhật ký**
3. Gõ lại cho đúng lỗi tái hiện
4. Bấm **Dừng ghi nhật ký**, rồi **Mở thư mục chứa nhật ký**
5. Đính kèm tệp `~/.local/share/h-openkey/debug.log` trong báo cáo lỗi

Tệp nhật ký chỉ chứa mã phím và văn bản do bộ gõ tạo ra. Dữ liệu không được tự
động truyền ra ngoài; người dùng có thể kiểm tra nội dung trước khi gửi.

## 📦 Cài đặt

Bản ổn định hiện tại: [H-OpenKey Linux 1.3.0](https://github.com/hieu8123/h-openkey/releases/tag/linux-v1.3.0).

Cài đặt bằng một lệnh, không cần sao chép kho mã nguồn:

```sh
curl -fsSL https://raw.githubusercontent.com/hieu8123/h-openkey/master/Sources/OpenKey/linux/packaging/install.sh | bash
```

Trình cài đặt tự động cài các gói phụ thuộc thông qua `apt`, `dnf`, `pacman` hoặc
`zypper`, biên dịch ứng dụng, cài vào `~/.local/bin`, tạo mục trong trình đơn và
cấu hình khởi động cùng phiên đăng nhập. Trên GNOME Wayland, trình cài đặt còn
cài quy tắc udev, layout `xkb:custom` và chọn backend driver. Bước cấp quyền
cần `sudo`; symbols được cài vào XKB root hệ thống vì Mutter 46 không dùng bản
trong thư mục người dùng khi tạo keymap cho seat. Sau lần cài đặt hoặc cập nhật
layout, phải đăng xuất rồi đăng nhập lại để GNOME nạp cả symbols và đăng ký rules
mới; trình cài đặt không chạy driver sớm trong phiên cũ vì việc đó có thể làm
mất chữ có dấu.

Gỡ cài đặt:

```sh
curl -fsSL https://raw.githubusercontent.com/hieu8123/h-openkey/master/Sources/OpenKey/linux/packaging/install.sh | bash -s -- --uninstall
```

<details>
<summary>Tự biên dịch từ mã nguồn</summary>

```sh
git clone https://github.com/hieu8123/h-openkey.git
cd h-openkey
cmake -S Sources/OpenKey/linux -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Phụ thuộc: `cmake`, `g++`, `qt6-base-dev`, `pkg-config`, `libxkbcommon-dev`.

</details>

## 🚀 Sử dụng

> [!CAUTION]
> Không chạy đồng thời H-OpenKey với một bộ gõ độc lập khác đang sửa cùng luồng
> phím. Ứng dụng phát hiện fcitx/fcitx5 và yêu cầu xác nhận trước khi vô hiệu hoá.

```sh
systemctl --user stop app-org.fcitx.Fcitx5@autostart.service
h-openkey
```

Bộ gõ nằm ở khay hệ thống. Biểu tượng cho biết đang ở chế độ tiếng Việt hay tiếng
Anh; bấm vào để mở bảng điều khiển. Mặc định chuyển chế độ bằng `Ctrl + Shift`,
đổi được trong bảng điều khiển.

Trên GNOME, nguồn nhập phải là layout **Custom** do H-OpenKey cài
(`xkb:custom`). Bảng điều khiển chỉ hiển thị cơ chế **Driver trực tiếp** vì
không còn backend khác.

> [!NOTE]
> Driver không đặt khoảng chờ khi sửa dấu. Luồng `epoll_wait` được kernel đánh
> thức trực tiếp; Backspace và ký tự thay thế được gửi thành một gói uinput tức
> thời, không đi qua event loop Qt.

## ✨ Tính năng

| | |
| --- | --- |
| 🚀 **Driver trực tiếp** | evdev → OpenKeyCore → uinput; không IBus, preedit hoặc backend dự phòng |
| ⚡ **Đường gõ sự kiện** | Luồng epoll riêng, hotplug bằng inotify, không polling và không timer |
| ⌨️ **Kiểu gõ** | Telex, VNI, Simple Telex 1, Simple Telex 2 |
| 🔤 **Bảng mã** | Unicode dựng sẵn, TCVN3 (ABC), VNI Windows, Unicode tổ hợp |
| 🧠 **Smart Switch Key** | Engine vẫn hỗ trợ; nhận diện app-id đang chờ tích hợp cho driver trực tiếp |
| ✍️ **Gõ tắt** | Bảng gõ tắt riêng, dùng được cả khi đang ở chế độ tiếng Anh |
| 🔄 **Chuyển mã** | Công cụ chuyển văn bản giữa các bảng mã |
| 🎯 **Kiểm tra chính tả** | Gõ sai thì trả lại nguyên các phím đã bấm |
| 🔠 **Tự viết hoa** | Viết hoa đầu câu tự động |
| 🚀 **Tự khởi động** | Bật hoặc tắt trong tab Hệ thống; xử lý xung đột fcitx/fcitx5 sau khi người dùng xác nhận |
| ⚡ **Gõ tắt phụ âm** | `f→ph`, `j→gi`, `w→qu`, `g→ng`, `h→nh`, `k→ch` |
| 🎛️ **Đặt dấu** | Chọn kiểu `oà, uý` hoặc `òa, úy`; cho phép đặt dấu tự do |

## 🔬 Tìm hiểu sâu

Kiến trúc driver và các giới hạn tích hợp trên từng môi trường được trình bày tại:

**→ [`Sources/OpenKey/linux/README.md`](Sources/OpenKey/linux/README.md)**

## 🤝 Đóng góp

Mọi đóng góp đều được hoan nghênh. Ưu tiên hiện tại là tự động kích hoạt layout
trên KDE Plasma, COSMIC và các compositor wlroots để trải nghiệm cài đặt đạt mức
một lệnh giống GNOME.

Khi báo lỗi, vui lòng cung cấp:

1. Bản phân phối và compositor (`echo $XDG_CURRENT_DESKTOP $XDG_SESSION_TYPE`)
2. Ứng dụng gặp lỗi, và chuỗi phím đã gõ
3. Tệp nhật ký — xem [Báo lỗi gõ](#-báo-lỗi-gõ) ở trên

## 🙏 Credits

Bộ gõ này sử dụng engine của [**OpenKey**](https://github.com/tuyenvm/OpenKey), tác
giả [**Tuyên Mai**](https://github.com/tuyenvm). Toàn bộ công lao thiết kế bộ gõ,
kỹ thuật `Backspace` và engine xử lý tiếng Việt thuộc về dự án gốc; bản fork này
bổ sung khả năng hoạt động trên Linux.

Trang chủ dự án gốc: [open-key.org](http://open-key.org)

## 📄 Giấy phép

[GPL-3.0](LICENSE) — kế thừa từ dự án gốc.

Bạn có thể tải mã nguồn, tự biên dịch và cải tiến theo nhu cầu. Bản phân phối lại
phải tuân thủ GPL-3.0 và ghi nhận dự án OpenKey gốc.
