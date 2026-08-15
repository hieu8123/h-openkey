//
//  Backend.h
//  OpenKey cho Linux
//
//  Lop truu tuong ngan cach logic go voi driver evdev/uinput. Nho vay toan bo
//  logic go kiem thu duoc bang FakeBackend, khong can phien do hoa.
//

#ifndef OPENKEY_LINUX_BACKEND_H
#define OPENKEY_LINUX_BACKEND_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace openkey {

struct BackendCaps {
    // Driver hien xoa bang phim BackSpace cua thiet bi uinput.
    bool hasSurroundingText = false;
    // Biet duoc app dang focus, dieu kien de Smart Switch Key hoat dong.
    bool hasAppId = false;
    // Chuyen tiep duoc phim goc cho ung dung.
    bool canForwardKey = false;
};

struct KeyEvent {
    uint32_t keycode = 0;  // keycode X11 = evdev + 8, dung nhu platforms/linux.h
    bool pressed = false;
    bool repeat = false;   // evdev value=2
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
    uint32_t keyPresses = 0;  // so lan BackSpace phat qua uinput
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
    // Goi sau khi core da gan xong handler. Driver dung moc nay de bat luong
    // doc evdev rieng, tranh chay handler truoc khi OpenKeyCore san sang.
    virtual void activate() {}
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

    // Goi deu dan de driver lam viec bao tri, vi du quet lai ban phim vua ket noi.
    virtual void tick() {}

    void setKeyHandler(std::function<KeyVerdict(const KeyEvent&)> h) {
        _handler = std::move(h);
    }

    // Goi khi ung dung dang focus doi. Chi backend nao co caps().hasAppId moi
    // goi toi; cac backend khac de nguyen.
    void setFocusHandler(std::function<void(const std::string&)> h) {
        _focusHandler = std::move(h);
    }

    // Driver khong biet app-id tren Wayland nhung van co the quan sat click
    // chuot. Click co the doi o nhap, nen core phai bo bo dem tu dang go.
    void setContextBreakHandler(std::function<void()> h) {
        _contextBreakHandler = std::move(h);
    }

protected:
    std::function<KeyVerdict(const KeyEvent&)> _handler;
    std::function<void(const std::string&)> _focusHandler;
    std::function<void()> _contextBreakHandler;
};

// Ban Linux chi co mot duong: evdev -> OpenKeyCore -> uinput.
enum class BackendKind { Driver };

BackendKind backendKindFromString(const std::string& s);
const char* backendKindToString(BackendKind k);

// Backend rỗng: không bắt phím, không gõ được chữ nào, và quan trọng nhất là
// không đụng tới bàn phím của phiên. Dùng khi không còn đường nào gõ được.
std::unique_ptr<IBackend> makeNullBackend();

// KHÔNG BAO GIỜ trả về nullptr. Không dùng được driver thì trả về backend
// rỗng, để bảng điều khiển — chỗ duy nhất đổi lại được cấu hình — vẫn mở lên được.
//
// `notice` rỗng nghĩa là driver đã sẵn sàng; khác rỗng là lý do không gõ được.
std::unique_ptr<IBackend> createBackend(BackendKind requested, std::string& notice);

} // namespace openkey

#endif // OPENKEY_LINUX_BACKEND_H
