# H-OpenKey

[![Giấy phép](https://img.shields.io/badge/gi%E1%BA%A5y%20ph%C3%A9p-GPL--3.0-blue.svg)](LICENSE)
[![Phiên bản](https://img.shields.io/badge/phi%C3%AAn%20b%E1%BA%A3n-1.2-brightgreen.svg)](CHANGELOG.md)
[![Nền tảng](https://img.shields.io/badge/n%E1%BB%81n%20t%E1%BA%A3ng-Linux-lightgrey.svg)](#-tương-thích)
[![Backend](https://img.shields.io/badge/backend-evdev%20%7C%20input--method--v2-orange.svg)](Sources/OpenKey/linux/README.md)
[![Standard Readme](https://img.shields.io/badge/readme%20style-standard-brightgreen.svg)](https://github.com/RichardLitt/standard-readme)

> Bộ gõ tiếng Việt cho Linux, sử dụng engine OpenKey của Tuyên Mai

H-OpenKey đem bộ gõ [OpenKey](https://github.com/tuyenvm/OpenKey) sang Linux. Phần
engine xử lý tiếng Việt được dùng lại **nguyên vẹn, không sửa một dòng nào**, nên
cách gõ, kiểu gõ và bảng mã giống hệt bản macOS và Windows — ai quen OpenKey rồi
thì sang Linux không phải học lại gì.

Bộ gõ chạy thẳng, **không cần ibus hay fcitx5**, và không dùng vùng tiền soạn nên
**không có gạch chân dưới chữ đang gõ**. Trên X11 nó chặn phím ở tầng kernel
(`EVIOCGRAB`); trên Wayland thì qua `zwp_input_method_v2`.

Bản Linux mang tên `h-openkey` để không lẫn với bản gốc nếu bạn dùng cả hai.

## Mục lục

- [Bối cảnh](#-bối-cảnh)
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

Bản fork này thêm phần Linux. Trên Linux, cái khó không nằm ở engine mà ở đường
đưa chữ ra: Wayland cố tình không cho ứng dụng ngoài móc bàn phím toàn cục hay bơm
Unicode tuỳ ý như `SendInput` của Windows hay `CGEventPost` của macOS. Muốn nuốt
phím thì buộc phải tự làm input method, và từ đó phát sinh một loạt chuyện chỉ
Linux mới có. Chi tiết nằm ở [tài liệu kỹ thuật](Sources/OpenKey/linux/README.md).

> [!IMPORTANT]
> Kể từ phiên bản 1.1, bản fork này **đi đường riêng và không còn theo dõi hay
> đồng bộ với repo gốc** nữa. Repo gốc là điểm khởi đầu, không phải nguồn cập nhật.

## ✅ Tương thích

Điều quyết định là **compositor**, không phải distro.

| Môi trường | Trạng thái |
| --- | --- |
| 🟢 Bất kỳ phiên X11/Xorg nào | Chạy tốt nhất. Backend evdev + XTEST, đã kiểm chứng trên Zorin OS |
| 🟡 Pop!_OS (COSMIC) | Gõ được, nhưng **hỏng menu chuột phải** — bug của cosmic-comp, xem bên dưới |
| 🟡 KDE Plasma, sway, wlroots | Nhiều khả năng chạy, chưa ai thử |
| 🔴 GNOME — phiên Wayland | **Không** chạy trên Wayland, xem bên dưới |

> [!WARNING]
> Trên **phiên Wayland của GNOME** thì không gõ được, vì Mutter không cấp
> `input-method-v2` cho bộ gõ ngoài. Cách xử lý: đăng nhập bằng phiên **Xorg**
> (chọn ở màn hình đăng nhập), khi đó backend X11 chạy bình thường.

### Hai giới hạn của Wayland (không sửa được từ phía bộ gõ)

**1. Trên COSMIC: menu chuột phải của dock/panel không mở được.**
Để nuốt phím, bộ gõ buộc phải gọi `zwp_input_method_v2_grab_keyboard` và giữ grab
đó cả phiên. cosmic-comp lại từ chối **mọi** popup grab (menu chuột phải của
dock, panel, và menu ứng dụng đều là XDG popup) khi trên seat đang có bất kỳ
keyboard grab nào — [cosmic-comp#1763](https://github.com/pop-os/cosmic-comp/issues/1763),
[#1211](https://github.com/pop-os/cosmic-comp/issues/1211). Hai PR sửa
([#1923](https://github.com/pop-os/cosmic-comp/pull/1923),
[#2243](https://github.com/pop-os/cosmic-comp/pull/2243)) **đều đã đóng mà không
merge**, nên bản cosmic-comp mới nhất vẫn dính. fcitx5 và ibus bị y hệt. Không có
cách nào sửa từ phía OpenKey: muốn chặn phím thì phải giữ grab, mà giữ grab thì
compositor chặn menu.

**2. Trên mọi compositor Wayland: phím tắt đổi chế độ chỉ chạy khi con trỏ đang ở
ô nhập văn bản.** Giao thức `input-method-v2` chỉ chuyển phím tới bộ gõ khi đang
có text input được kích hoạt. Lúc focus nằm ở dock/panel hay cửa sổ không có ô
nhập, bộ gõ **không nhận được phím nào**, nên `Ctrl+Shift` không có tác dụng ở đó.

Cả hai đều **không xảy ra trên X11**, vì backend X11 chặn phím ở tầng kernel
(`EVIOCGRAB`) chứ không qua giao thức input method của compositor.

### Backend X11 chặn phím ở tầng kernel

Trên X11, bộ gõ **không** dùng XRecord (chỉ quan sát được phím, không chặn được,
nên phím gốc lọt tới ứng dụng trước khi kịp sửa). Thay vào đó nó chặn thẳng ở
kernel bằng `EVIOCGRAB` trên `/dev/input/event*`, nhờ vậy phím gốc không bao giờ
lọt ra ngoài và hết sạch race condition khi gõ nhanh.

Đổi lại, tài khoản của bạn phải thuộc nhóm `input`. Script cài tự lo việc này;
làm tay thì:

```sh
sudo usermod -aG input $USER   # rồi ĐĂNG XUẤT và đăng nhập lại
```

Backend Wayland cần hai giao thức, thiếu một cái là không khởi động được:

- `zwp_input_method_manager_v2`
- `zwp_virtual_keyboard_manager_v1`

GNOME/Mutter — mặc định của Ubuntu — không cung cấp `input-method-v2` cho ứng
dụng ngoài. Đã kiểm chứng trên **Zorin OS phiên Wayland**: backend Wayland dừng
ngay với lỗi `compositor khong ho tro zwp_input_method_manager_v2`. Khi đó bộ gõ
rơi xuống backend X11, mà X11 dưới XWayland chỉ với tới được ứng dụng XWayland —
nên cách xử lý vẫn là đăng nhập bằng phiên Xorg.

Nếu trong bảng điều khiển bạn chọn cứng một backend mà backend đó không dùng
được, OpenKey **không tắt**: nó rơi về “Tự động”, báo cho bạn biết đã rơi vì lý
do gì và đang chạy bằng backend nào. Không có việc chọn nhầm rồi kẹt luôn, vì
bảng điều khiển — chỗ duy nhất đổi lại được — lại nằm trong chính ứng dụng đó.

Kiểm tra máy của bạn:

```sh
OPENKEY_DEBUG=1 h-openkey 2>&1 | head -5
```

Thấy dòng `khong theo doi duoc cua so dang focus` nghĩa là compositor không cho
biết cửa sổ nào đang focus. Khi đó Smart Switch Key tắt, và các cách xử lý riêng
cho Firefox với Chrome cũng không kích hoạt — chữ có thể ra sai trong ô chat
Facebook và vài ô nhập trên web.

## 🐞 Báo lỗi gõ

Lỗi gõ hầu hết là race condition hoặc lỗi thứ tự sự kiện, chỉ tái hiện trên máy
người dùng — đoán mò không ra. Bảng điều khiển có sẵn công cụ ghi lại:

1. Mở bảng điều khiển → tab **Hệ thống** → mục **Chẩn đoán lỗi gõ**
2. Bấm **Bắt đầu ghi nhật ký**
3. Gõ lại cho đúng lỗi tái hiện
4. Bấm **Dừng ghi nhật ký**, rồi **Mở thư mục chứa log**
5. Gửi kèm file `~/.local/share/h-openkey/debug.log` vào issue

File log chỉ chứa mã phím và chữ mà bộ gõ sinh ra, không gửi đi đâu cả — bạn tự
xem trước rồi mới gửi.

## 📦 Cài đặt

Một lệnh, không cần clone mã:

```sh
curl -fsSL https://raw.githubusercontent.com/hieu8123/OpenKey/master/Sources/OpenKey/linux/packaging/install.sh | bash
```

Script tự cài phụ thuộc (`apt`, `dnf`, `pacman`, `zypper`), build, đặt vào
`~/.local/bin`, tạo mục trong trình đơn ứng dụng và bật chạy cùng phiên đăng nhập.

Gỡ ra:

```sh
bash install.sh --uninstall
```

<details>
<summary>Tự build từ mã nguồn</summary>

```sh
git clone https://github.com/hieu8123/OpenKey.git
cd OpenKey
cmake -S Sources/OpenKey/linux -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Phụ thuộc: `cmake`, `g++`, `qt6-base-dev`, `libwayland-dev`, `wayland-protocols`,
`libxkbcommon-dev`, `libx11-dev`, `libxtst-dev`.

</details>

## 🚀 Sử dụng

> [!CAUTION]
> Mỗi phiên Wayland chỉ **một** bộ gõ được giữ input method. Phải tắt hẳn bộ gõ
> khác trước, nếu không H-OpenKey chạy nhưng không gõ ra chữ nào.

```sh
systemctl --user stop app-org.fcitx.Fcitx5@autostart.service   # hoặc: ibus exit
h-openkey
```

Bộ gõ nằm ở khay hệ thống. Biểu tượng cho biết đang ở chế độ tiếng Việt hay tiếng
Anh; bấm vào để mở bảng điều khiển. Mặc định chuyển chế độ bằng `Ctrl + Shift`,
đổi được trong bảng điều khiển.

## ✨ Tính năng

| | |
| --- | --- |
| ⌨️ **Kiểu gõ** | Telex, VNI, Simple Telex 1, Simple Telex 2 |
| 🔤 **Bảng mã** | Unicode dựng sẵn, TCVN3 (ABC), VNI Windows, Unicode tổ hợp |
| 🧠 **Smart Switch Key** | Nhớ chế độ gõ theo từng ứng dụng, chuyển cửa sổ là tự đổi |
| ✍️ **Gõ tắt** | Bảng gõ tắt riêng, dùng được cả khi đang ở chế độ tiếng Anh |
| 🔄 **Chuyển mã** | Công cụ chuyển văn bản giữa các bảng mã |
| 🎯 **Kiểm tra chính tả** | Gõ sai thì trả lại nguyên các phím đã bấm |
| 🔠 **Tự viết hoa** | Viết hoa đầu câu tự động |
| ⚡ **Gõ tắt phụ âm** | `f→ph`, `j→gi`, `w→qu`, `g→ng`, `h→nh`, `k→ch` |
| 🎛️ **Đặt dấu** | Chọn kiểu `oà, uý` hoặc `òa, úy`; cho phép đặt dấu tự do |

## 🔬 Tìm hiểu sâu

Kiến trúc, cách chọn đường đưa chữ ra theo từng ứng dụng, và **ba hướng đã thử rồi
thất bại** (kèm lý do, để không ai đi lại vòng đó) nằm ở:

**→ [`Sources/OpenKey/linux/README.md`](Sources/OpenKey/linux/README.md)**

## 🤝 Đóng góp

Rất hoan nghênh, nhất là báo cáo từ các distro và compositor khác — đó đúng là chỗ
đang thiếu dữ liệu nhất.

Khi báo lỗi, kèm giúp:

1. Distro và compositor (`echo $XDG_CURRENT_DESKTOP $XDG_SESSION_TYPE`)
2. Ứng dụng gặp lỗi, và chuỗi phím đã gõ
3. File nhật ký — xem [Báo lỗi gõ](#-báo-lỗi-gõ) ở trên

## 🙏 Credits

Bộ gõ này sử dụng engine của [**OpenKey**](https://github.com/tuyenvm/OpenKey), tác
giả [**Tuyên Mai**](https://github.com/tuyenvm). Toàn bộ công lao thiết kế bộ gõ,
kỹ thuật `Backspace` và engine xử lý tiếng Việt thuộc về dự án gốc — bản fork này
chỉ thêm phần chạy trên Linux.

Trang chủ dự án gốc: [open-key.org](http://open-key.org)

## 📄 Giấy phép

[GPL-3.0](LICENSE) — kế thừa từ dự án gốc.

Bạn hoàn toàn có thể tải mã nguồn về tự build và cải tiến theo mục đích của mình.
Nếu tái phân phối bản cải tiến, nó cũng phải là mã nguồn mở và ghi rõ bản gốc là
OpenKey.
