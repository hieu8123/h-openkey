# Chữ ký GVariant của các đối tượng IBus

Backend IBus tự nói DBus nên phải tự dựng đúng cấu trúc GVariant mà ibus-daemon
mong đợi. Đoán chữ ký này là nguồn lỗi rất khó chẩn đoán: daemon chỉ im lặng từ
chối, không báo gì cả.

Các chữ ký dưới đây lấy từ chính thư viện IBus trên máy thật bằng
`tools/ibus_dump_variants.py`. Khi lên IBus phiên bản mới mà đăng ký component
thất bại, việc đầu tiên là chạy lại script đó và đối chiếu.

IBus phiên bản: **1.5.29-rc2**

## IBusEngineDesc

```
signature: (sa{sv}ssssssssussssssss)
value    : ('IBusEngineDesc', @a{sv} {}, 'openkey', 'OpenKey', 'Bộ gõ tiếng Việt',
            'vi', 'GPL', 'hieulc', 'h-openkey', 'us', uint32 0,
            '', '', '', '', '', '', '', '')
```

Thứ tự trường:

| # | Kiểu | Trường | Giá trị của OpenKey |
| --- | --- | --- | --- |
| 1 | s | tên lớp | `IBusEngineDesc` |
| 2 | a{sv} | attachments | rỗng |
| 3 | s | name | `openkey` |
| 4 | s | longname | `OpenKey` |
| 5 | s | description | `Bộ gõ tiếng Việt` |
| 6 | s | language | `vi` |
| 7 | s | license | `GPL` |
| 8 | s | author | `hieulc` |
| 9 | s | icon | `h-openkey` |
| 10 | s | layout | `us` |
| 11 | u | rank | `0` |
| 12-19 | s | hotkeys, symbol, setup, layout_variant, layout_option, version, textdomain, icon_prop_key | rỗng hết |

Tám chuỗi rỗng cuối cùng **không được bỏ**: thiếu một cái là sai chữ ký.

## IBusComponent

```
signature: (sa{sv}ssssssssavav)
value    : ('IBusComponent', @a{sv} {}, 'org.freedesktop.IBus.OpenKey', 'H-OpenKey',
            '1.2.1', 'GPL', 'hieulc', 'https://github.com/hieu8123/OpenKey', '', '',
            @av [], [<IBusEngineDesc...>])
```

Thứ tự trường:

| # | Kiểu | Trường | Giá trị của OpenKey |
| --- | --- | --- | --- |
| 1 | s | tên lớp | `IBusComponent` |
| 2 | a{sv} | attachments | rỗng |
| 3 | s | name | `org.freedesktop.IBus.OpenKey` |
| 4 | s | description | `H-OpenKey` |
| 5 | s | version | phiên bản hiện tại |
| 6 | s | license | `GPL` |
| 7 | s | author | `hieulc` |
| 8 | s | homepage | URL repo |
| 9 | s | exec | **rỗng** — ta đăng ký động, không để daemon spawn tiến trình |
| 10 | s | textdomain | rỗng |
| 11 | av | observed_paths | rỗng |
| 12 | av | engines | một phần tử, là IBusEngineDesc ở trên |

## IBusText

```
signature: (sa{sv}sv)
value    : ('IBusText', @a{sv} {}, 'tiếng Việt', <('IBusAttrList', @a{sv} {}, @av [])>)
```

| # | Kiểu | Trường |
| --- | --- | --- |
| 1 | s | tên lớp, `IBusText` |
| 2 | a{sv} | attachments, rỗng |
| 3 | s | nội dung chữ, UTF-8 |
| 4 | v | IBusAttrList: `('IBusAttrList', a{sv} rỗng, av rỗng)` |

OpenKey không bao giờ dùng thuộc tính hiển thị (gạch chân, màu) vì không dùng
preedit, nên IBusAttrList luôn rỗng.

## Kết quả đo khác từ spike

- `capabilities` của ứng dụng GTK đo được: `0x29`, tức **có** bit surrounding
  text (`1<<5`). Nghĩa là đường `DeleteSurroundingText` dùng được thật, không
  phải luôn rơi xuống BackSpace.
- GNOME **chấp nhận engine đăng ký động**: không cần file component XML nào,
  `ibus engine` báo đúng tên engine vừa đăng ký.
- `ibus list-engine` **không** liệt kê engine đăng ký động. Đó là bình thường,
  nó chỉ đọc các component cài sẵn bằng XML. Đừng lấy điều này làm phép thử.
