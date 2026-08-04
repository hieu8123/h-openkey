# OpenKey Change Log

> Từ đây trở đi, phần Linux (`h-openkey`) đi đường riêng và **không còn theo dõi
> repo gốc** nữa. Các mục dưới mốc "H-OpenKey (Linux)" là của bản fork; phần
> lịch sử của OpenKey gốc giữ nguyên bên dưới để ghi nhận nguồn.

## H-OpenKey (Linux)

##### Version 1.2.1: (04/08/2026)

**Chọn nhầm backend không còn làm kẹt cứng ứng dụng.**

Cấu hình chỉ định rõ một backend mà backend đó không dùng được thì OpenKey thoát
luôn. Nghe thì hợp lý, nhưng bảng điều khiển lại là chỗ *duy nhất* đổi được lựa
chọn đó — ứng dụng không chạy thì không mở được bảng điều khiển, nên người dùng
kẹt cứng, không có đường nào tự sửa. Gặp thật trên Zorin OS phiên Wayland: chọn
backend `wayland`, Mutter không cấp `zwp_input_method_manager_v2`, thế là hỏng.

Nay khi backend được chỉ định không dùng được, bộ gõ rơi xuống “Tự động” và vẫn
khởi động. Việc rơi xuống **không im lặng**: hiện hộp thoại nói rõ backend nào
hỏng, vì sao, đang chạy bằng backend gì; đồng thời đặt lại cấu hình về “Tự động”
để bảng điều khiển hiển thị đúng thứ đang chạy. Nếu không backend nào dùng được
thì vẫn báo lỗi và dừng như cũ — lúc đó có mở bảng điều khiển cũng không cứu được.

**Thông báo lỗi nay là tiếng Việt có dấu.** Toàn bộ chuỗi lỗi của các backend
trước đây không dấu — vốn là quy ước cho *comment* trong mã nguồn, không phải cho
chữ người dùng đọc. Nay các câu hiện trong hộp thoại và ghi ra terminal đều có
dấu đầy đủ (`compositor không hỗ trợ …`, `không có quyền đọc /dev/input/event*: …`).
Log gỡ rối của `OPENKEY_DEBUG` giữ nguyên không dấu, vì đó là chữ cho người sửa mã.

Kèm theo: ghi nhận tình trạng GNOME/Mutter trong README đã đổi từ “suy ra từ yêu
cầu trong code” thành đã kiểm chứng trên máy thật.

##### Version 1.2: (04/08/2026)

**Backend X11 viết lại: chặn phím ở tầng kernel, hết mất chữ khi gõ nhanh.**

XRecord chỉ *quan sát* được phím chứ không chặn được, nên phím gốc luôn tới ứng
dụng trước rồi bộ gõ mới chạy theo sửa — race condition là không tránh khỏi. Nay chặn
thẳng ở kernel bằng `EVIOCGRAB`, phím gốc không còn lọt ra ngoài. Cần tài khoản
thuộc nhóm `input`; script cài tự lo.

Sáu lỗi tìm được và sửa, tất cả đều đo bằng `xev` chứ không đoán:

- **Mất chữ khi gõ nhanh.** Gõ nhanh là gõ gối đầu — phím trước chưa nhả thì phím
  sau đã bấm. Lúc bộ gõ bơm lại chính phím đó để xuất chữ, X thấy phím *đang bấm*
  nên lọc bỏ lệnh bấm, chỉ release lọt qua và ký tự biến mất. Đo được: lần hỏng
  chỉ có `R kc=38`, không hề có `P kc=38`. Sửa: nhả phím đó ra trước khi bơm.
  Kết quả đo lại — trước: `của của củ củ của củ`, sau: mất 0 ký tự.
- **Keysym Latin-1 sai chuẩn.** `â` gán bằng `0x010000E2` thay vì `0x00E2`; nhiều
  ứng dụng không đổi ngược ra ký tự nên bỏ qua luôn — kéo theo cả họ `ấ ầ ẩ ẫ ậ`.
- **Đổi bảng phím ngay trước mỗi ký tự.** Ứng dụng tra chữ bằng bản sao keymap
  riêng, chỉ cập nhật khi xử lý tới `MappingNotify`; gõ nhanh thì phím tới trước
  thông báo đó. Nay gán sẵn 237 ký tự một lần lúc khởi động.
- **Rò rỉ keycode khi thoát.** Không trả lại keycode đã mượn, nên mỗi lần chạy
  lại mất dần cho tới khi cạn sạch và không khởi động được nữa.
- **Bàn phím rời/không dây rớt kết nối.** Node thiết bị cũ bị xoá nhưng fd chết
  vẫn nằm trong epoll → quay vòng 100% CPU; bàn phím hiện lại dưới node mới thì
  không được chặn nên mất hẳn tiếng Việt. Nay dọn fd chết và quét lại mỗi giây.
- **X tự lặp phím** sinh ký tự mà engine không biết, làm lệch bộ đệm đếm xoá.

Ba lỗi nữa quanh việc chuyển tiếng Việt / tiếng Anh:

- **`Ctrl+Shift` làm kẹt Shift.** Phím tắt chỉ gồm phím bổ trợ được kích hoạt lúc
  *nhả* phím, và engine nuốt đúng lần nhả đó. Trước đây XRecord không chặn được
  phím nên nuốt cũng vô hại; nay chặn thật nên lệnh bấm đã gửi mà lệnh nhả không
  tới X — X tưởng Shift bị giữ mãi, gõ tiếp ra chữ hoa và loạn phím tắt. Nay phím
  bổ trợ luôn được chuyển tiếp bất kể engine phán gì.
- **Phím tắt đổi chế độ nhưng giao diện không biết.** Chế độ đã đổi thật nhưng
  biểu tượng khay đứng im nên tưởng phím tắt hỏng. Đường bấm bằng chuột tự vẽ lại
  nên không dính; nay `toggleLanguage()` tự báo cho giao diện.
- **Smart Switch Key không chạy trên X11.** Backend khai `hasAppId = true` nhưng
  chưa bao giờ báo cửa sổ focus đổi, nên tính năng nhớ chế độ theo ứng dụng nằm
  im và bộ đệm gõ không được xoá khi đổi ứng dụng (gõ tiếp ở ứng dụng mới sẽ xoá
  sai số ký tự). Nay tự hỏi cửa sổ đang focus mỗi ~150ms.

- **Chế độ tiếng Anh chưa bao giờ có tác dụng.** `Engine.h` chỉ *khai báo*
  `vLanguage` chứ engine không đọc nó — việc chặn lại là phần của tầng nền tảng.
  Bản macOS (`OpenKey.mm:672`) và Windows (`OpenKey.cpp:565`) đều có bước
  `if (vLanguage == 0)`, riêng bản Linux thiếu hẳn, nên chuyển sang tiếng Anh chỉ
  đổi biểu tượng còn chữ vẫn bị bỏ dấu. Nay ở chế độ tiếng Anh chỉ xử lý gõ tắt
  (nếu bật), còn lại chuyển tiếp nguyên phím.

Đổi: phím tắt mặc định thành **Ctrl + Shift** (chỉ phím bổ trợ) cho khớp README;
trước đó code để `Alt + Z` trong khi tài liệu ghi `Ctrl + Shift`.

Thêm: nút **bắt đầu/dừng ghi nhật ký** trong tab Hệ thống, ghi ra
`~/.local/share/h-openkey/debug.log` để gửi kèm khi báo lỗi.

##### Version 1.1: (28/07/2026)

**Sửa lỗi không gõ được trong ô chat Facebook và một số ô nhập trên web.**

Nguyên nhân: chữ đi ra bằng **hai đường** khác nhau — phím ảo (BackSpace, chuyển
tiếp phím) và text-input (`commit_string`). Trong cùng một đường thì thứ tự được
bảo đảm, nhưng giữa hai đường thì không. Các bộ soạn thảo giàu định dạng trên web
(ô chat Facebook dùng Lexical, và Draft.js/ProseMirror) tự dựng lại nội dung theo
mô hình riêng, nên khi hai đường đến lệch nhau thì **một trong hai bị nuốt mất**,
và không đoán trước được bên nào. Đo được cả hai chiều trong cùng ô chat đó:

```
mất phần xoá:   "ting vie" + (xoá 1, chèn "ê")  →  "ting vieê"
mất phần chèn:  "tie"      + (xoá 1, chèn "ê")  →  "ti"
```

Sửa:

- Mọi thao tác xuất chữ đi qua **một hàng đợi FIFO duy nhất**, không còn chỗ nào
  gửi thẳng ra ngoài hàng đợi. Trong cùng một đường thì chạy liền, chỉ khi **đổi
  đường** mới chờ ứng dụng báo đã xử lý xong (`surrounding_text`). Có hạn chót
  60 ms để không bao giờ kẹt nếu ứng dụng im lặng.
- Với họ Chromium, **ký tự thường cũng đi đường text-input** — không phải vì
  đường phím hỏng, mà để khỏi phải đổi đường; gõ một chữ có dấu vì thế chỉ còn
  một lần chờ thay vì hai, đỡ giật khi gõ nhanh.
- **Firefox** đi đường bàn phím ảo, là cấu hình đo được là đúng với nó.
- VS Code, terminal và các ứng dụng khác **giữ nguyên** cơ chế cũ, không chờ.

##### Version 1.0: (07/2026)
- Bộ gõ tiếng Việt native trên Wayland qua `zwp_input_method_v2`.
- Bảng điều khiển Qt6, bảng gõ tắt, công cụ chuyển mã, tuỳ chọn phím chuyển chế độ.
- Smart Switch Key theo ứng dụng đang focus.
- Backend X11 (XRecord + XTEST).
- Script cài một lệnh, đóng gói bản phát hành, chạy cùng phiên đăng nhập.

---

## OpenKey gốc (macOS / Windows)

##### Version 1.2 RC5: (26/08/2019)
- Sửa lỗi không gõ được chữ "quởn".
- Không kiểm tra chính tả khi sử dụng dấu "[ ] { }".

##### Version 1.2 RC4: (24/08/2019)
- Bảng gõ tắt tiện lợi hơn, thêm tính năng Sửa từ.
- Cải thiện khả năng bỏ dấu, tốc độ nhanh hơn.
- Tự phục hồi dấu câu khi xóa ký tự (chữ “tuỳa” xóa “a” sẽ thành “tùy”,… )
- Sửa lỗi không gõ được từ “quét” khi bật chức năng tự phục hồi phím.
- Sửa lỗi ư và ơ khi gõ font Palatino trong MS Word.
- Sửa lỗi bảng mã VNI khi xóa ký tự, không thể gõ tiếng việt tiếp.

##### Version 1.2 RC3: (16/08/2019)
- Không gõ được "dui9, duoi96".
- Không gõ được "tuyps".

##### Version 1.2 RC2: (15/08/2019)
- Sửa lỗi không gõ được d i e u 9 6.
- Sửa lỗi không gõ tắt được khi dùng chế độ tiếng Anh với từ bắt đầu bằng.
- Sửa lỗi tự nhảy dấu khi gõ sai.
- Thêm thông tin phiên bản trong bảng giới thiệu.

##### Version 1.2 RC1: (13/08/2019)
- Chuyển chế độ thông minh: Bạn đang dùng chế độ gõ Tiếng Việt trên ứng dụng A, bạn chuyển qua ứng dụng B trước đó bạn dùng chế độ gõ Tiếng Anh, OpenKey sẽ tự động chuyển qua chế độ gõ Tiếng Anh cho bạn, khi bạn quay lại ứng dụng A, OpenKey tất nhiên sẽ chuyển lại chế độ gõ tiếng Việt, rất cơ động.
- Viết Hoa chữ cái đầu câu: Khi gõ văn bản dài, đôi khi bạn quên ghi hoa chữ cái đầu câu khi kết thúc một câu hoặc khi xuống hàng, tính năng này sẽ tự ghi hoa chữ cái đầu câu cho bạn, thật tuyệt vời.
Khôi phục phím với từ sai: hỗ trợ thêm các dấu ngắt câu như dấu chấm, phẩy,...
Sửa vài lỗi nho nhỏ khác.

##### Version 1.1 RC: (12/08/2019)
- Chế độ “Gửi từng phím”: OpenKey bản mới (1.1) mặc định dùng kỹ thuật mới gửi dữ liệu 1 lần thay vì gửi nhiều lần cho chuỗi ký tự, nên nếu có ứng dụng nào không tương thích, hãy bật tính năng này lên, mặc định thì nên tắt vì kỹ thuật mới sẽ chạy nhanh hơn.
- Phục hồi phím với từ sai.
- Nâng cao khả năng check chính tả.
- Sửa lỗi thanh địa chỉ trình duyệt (on/off).
- Bỏ tính năng "cho phép bỏ dấu tự do".
- Gõ tắt: bao gồm bật/tắt, bảng soạn các từ gõ tắt, hỗ trợ ký tự bất kỳ, bảng mã bất kỳ. Khi soạn thảo các từ gõ tắt, bạn phải nhập ở bảng mã Unicode dựng sẵn.
- Gõ tắt ngay khi trong chế độ gõ tiếng Anh (on/off).
- Sửa lỗi trên một số phần mềm.

##### Version 1.0.20: (06/08/2019)
- Sửa lỗi phím tắt chuyển chế độ, phím tắt của ứng dụng khác vẫn hoạt động nếu bị trùng.
- Cho phép gõ “Đ” ngay sau phụ âm.
- Không hiện Icon trên thanh Dock mục recent app.
- Sửa lỗi “oăc” ra “ooạc” trong kiểu gõ VNI.
- Sửa vài lỗi nhỏ xíu khác.

##### Version 1.0.19: (04/08/2019)
- Sửa lỗi không gõ được chữ “gì” khi dùng bỏ dấu kiểu cũ.
- Sửa lỗi không gõ được Unicode tổ hợp trên ứng dụng Stickies.
- Sửa lỗi gõ các âm "oong, ooc".

##### Version 1.0.18: (01/08/2019)
- Không sử dụng w -> ư trong Simple Telex.
- Bật tắt kêu beep khi chuyển chế độ.
- Thêm phím chuyển Shift, giờ có thể sử dụng Ctrl + Shift hoặc Command + Shift.
- Sửa vài lỗi khác.
- Hỗ trợ cho macOS bản cũ.

##### Version 1.0.17: (31/07/2019)
- Add Simple Telex mode.
- Black/White icon on menu bar.
- Space and back key improved.
- Modern orthography.
- Custom switch key.
- Quick telex (cc=ch, gg=gi, kk=kh, nn=ng, qq=qu, pp=ph, tt=th).
- Support TextWrangler.

##### Version 1.0.14: (09/04/2019)
- Add case "uýt".
- Improve typing English in Vietnamese mode.

##### Version 1.0.11: (27/02/2019)
- Add case "chú thòong", "gòong".

##### Version 1.0.10: (26/02/2019)
- Fix case "duocd".

##### Version 1.0.9: (22/02/2019)
- Fix incorrect word when switch language without pressing Space key.

##### Version 1.0.8: (19/02/2019)
- Switch key: Control + Command + Space  --> Control + Z

##### Version 1.0.7: (15/02/2019)
- Fix case "duongd".
- Fix end consonant "t".

##### Version 1.0.6: (15/02/2019)
- Fix case "quatw".

##### Version 1.0.5: (13/02/2019)
- Spelling enhanced.
- Correct 1x menu icon.

##### Version 1.0.3: (11/02/2019)
- Fix auto correct on Chrome.

##### Version 1.0: (11/02/2019)
- First release.



# OpenKey lịch sử

##### Version 1.0.17: (31/07/2019)
- Thêm chế độ Simple Telex.
- Icon trắng đen trên menu bar.
- Lỡ bấm phím Space, xoá space vẫn có thể bỏ dấu.
- Bỏ dấu kiểu cũ/mới: òa, úy | oà, uý.
- Tuỳ chọn phím chuyển.
- Gõ nhanh (cc=ch, gg=gi, kk=kh, nn=ng, qq=qu, pp=ph, tt=th).
- Hỗ trợ TextWrangler.

##### Version 1.0.11: (27/02/2019)
- Thêm trường hợp "chú thòong", "gòong".

##### Version 1.0.10: (26/02/2019)
- Sửa lỗi "duocd".

##### Version 1.0.9: (22/02/2019)
- Sửa lỗi từ sai khi đổi chế độ mà không bấm phím Space.

##### Version 1.0.8: (19/02/2019)
- Phím chuyển: Control + Command + Space  --> Control + Z

##### Version 1.0.7: (15/02/2019)
- Sửa lỗi "duongd".
- Sửa lỗi phụ âm cuối "t".

##### Version 1.0.6: (15/02/2019)
- Sửa lỗi "quatw".

##### Version 1.0.5: (13/02/2019)
- Nâng cao chính tả.
- Sửa icon cho màn hình non-retina.

##### Version 1.0.3: (11/02/2019)
- Sửa lỗi thanh địa chỉ trên Chrome.

##### Version 1.0: (11/02/2019)
- Phát hành lần đầu.