//
//  Backend.h
//  OpenKey cho Linux
//
//  Lop truu tuong ngan cach logic go voi cach tung phien lam viec cua Linux
//  bat phim va tra chu ve. Wayland va X11 cai cung interface nay, nho vay
//  toan bo logic go kiem thu duoc bang FakeBackend, khong can compositor.
//

#ifndef OPENKEY_LINUX_BACKEND_H
#define OPENKEY_LINUX_BACKEND_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace openkey {

struct BackendCaps {
    // Xoa duoc bang delete_surrounding_text thay vi phai gui phim BackSpace.
    bool hasSurroundingText = false;
    // Biet duoc app dang focus, dieu kien de Smart Switch Key hoat dong.
    bool hasAppId = false;
    // Chuyen tiep duoc phim goc cho ung dung.
    bool canForwardKey = false;
};

struct KeyEvent {
    uint32_t keycode = 0;  // keycode X11 = evdev + 8, dung nhu platforms/linux.h
    bool pressed = false;
    bool shift = false;    // Shift dang giu
    bool capsLock = false; // CapsLock dang bat
    bool ctrl = false;
    bool alt = false;
    bool super = false;

    bool otherControlKey() const { return ctrl || alt || super; }
};

enum class KeyVerdict {
    Forward,  // khong dung toi, de ung dung nhan nguyen phim goc
    Swallow,  // OpenKey da xu ly, nuot phim
};

// Phan can xoa truoc khi chen chu moi. Hai con so cho hai duong xuat khac nhau,
// va chung KHAC nhau: mot chu tieng Viet co dau chiem nhieu byte UTF-8, con o
// bang ma cu (TCVN3, VNI-Windows) mot chu logic lai hien thanh hai ky tu trong
// ung dung. Dem nham la xoa sai.
struct DeleteRequest {
    uint32_t utf8Bytes = 0;   // cho delete_surrounding_text
    uint32_t keyPresses = 0;  // so lan BackSpace neu phai dung duong fallback
};

class IBackend {
public:
    virtual ~IBackend() = default;

    virtual const char* name() const = 0;
    virtual BackendCaps caps() const = 0;

    // False nghĩa là backend này không gõ được chữ nào (xem makeNullBackend).
    // Ứng dụng vẫn chạy bình thường, chỉ là không có tiếng Việt.
    virtual bool canType() const { return true; }

    // Tra ve false kem thong bao qua lastError() neu khong khoi dong duoc.
    virtual bool start() = 0;
    virtual void stop() = 0;

    // Xoa phan `del` ngay truoc con tro roi chen `out`. Tuyet doi khong dung
    // preedit: do la nguyen nhan gay gach chan va nhan doi chu.
    virtual void sendResult(const DeleteRequest& del, const std::u32string& out) = 0;

    virtual void forwardKey(const KeyEvent& ev) = 0;

    virtual std::string focusedAppId() = 0;

    virtual const std::string& lastError() const = 0;

    // File descriptor de vong lap su kien cua ung dung theo doi, -1 neu backend
    // khong can. Khi fd san sang doc, goi dispatchEvents().
    virtual int eventFd() const { return -1; }
    virtual void dispatchEvents() {}

    // Goi truoc khi vong lap su kien di ngu, de day cac yeu cau con ton dong.
    virtual void flush() {}

    // Goi deu dan (vai mili giay mot lan). Backend nao xep hang cac thao tac va
    // cho ung dung bao lai thi dung day lam han chot, de khong ket khi ung dung
    // im lang. Backend khong can thi de nguyen.
    virtual void tick() {}

    void setKeyHandler(std::function<KeyVerdict(const KeyEvent&)> h) {
        _handler = std::move(h);
    }

    // Goi khi ung dung dang focus doi. Chi backend nao co caps().hasAppId moi
    // goi toi; cac backend khac de nguyen.
    void setFocusHandler(std::function<void(const std::string&)> h) {
        _focusHandler = std::move(h);
    }

protected:
    std::function<KeyVerdict(const KeyEvent&)> _handler;
    std::function<void(const std::string&)> _focusHandler;
};

// Backend nao se duoc dung. `Auto` dò theo thu tu wayland -> x11.
enum class BackendKind { Auto, Wayland, X11 };

BackendKind backendKindFromString(const std::string& s);
const char* backendKindToString(BackendKind k);

// Backend rỗng: không bắt phím, không gõ được chữ nào, và quan trọng nhất là
// không đụng tới bàn phím của phiên. Dùng khi không còn đường nào gõ được.
std::unique_ptr<IBackend> makeNullBackend();

// KHÔNG BAO GIỜ trả về nullptr. Không dùng được backend nào thì trả về backend
// rỗng, để bảng điều khiển — chỗ duy nhất đổi lại được cấu hình — vẫn mở lên được.
//
// `notice` rỗng nghĩa là mọi thứ đúng ý muốn. Khác rỗng là có chuyện cần báo cho
// người dùng: hoặc đã rơi xuống backend khác, hoặc không gõ được chữ nào cả.
std::unique_ptr<IBackend> createBackend(BackendKind requested, std::string& notice);

} // namespace openkey

#endif // OPENKEY_LINUX_BACKEND_H
