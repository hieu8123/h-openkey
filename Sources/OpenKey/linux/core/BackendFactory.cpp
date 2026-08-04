//
//  BackendFactory.cpp
//  OpenKey cho Linux
//
//  Linux khong co mot cach duy nhat de bat phim toan cuc: tuy phien dang nhap
//  ma phai di duong khac. Day la noi dò va chon.
//

#include "Backend.h"

#include <cstdlib>

#ifdef OPENKEY_HAVE_WAYLAND
#include "../backends/wayland/WaylandBackend.h"
#endif

#ifdef OPENKEY_HAVE_X11
#include "../backends/x11/X11Backend.h"
#endif

namespace openkey {

BackendKind backendKindFromString(const std::string& s) {
    if (s == "wayland") return BackendKind::Wayland;
    if (s == "x11") return BackendKind::X11;
    return BackendKind::Auto;
}

const char* backendKindToString(BackendKind k) {
    switch (k) {
        case BackendKind::Wayland: return "wayland";
        case BackendKind::X11: return "x11";
        case BackendKind::Auto: break;
    }
    return "auto";
}

namespace {

bool envHasValue(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && *v != '\0';
}

std::unique_ptr<IBackend> tryWayland(std::string& error) {
#ifdef OPENKEY_HAVE_WAYLAND
    if (!envHasValue("WAYLAND_DISPLAY")) {
        error = "không thấy WAYLAND_DISPLAY";
        return nullptr;
    }
    return makeWaylandBackend(error);
#else
    error = "bản build này không kèm backend Wayland";
    return nullptr;
#endif
}

std::unique_ptr<IBackend> tryX11(std::string& error) {
#ifdef OPENKEY_HAVE_X11
    if (!envHasValue("DISPLAY")) {
        error = "không thấy DISPLAY";
        return nullptr;
    }
    // Đang ở phiên Wayland thì DISPLAY này là của XWayland, và dùng backend X11
    // ở đây là tự bắn vào chân: nó chặn phím ngay ở kernel bằng EVIOCGRAB nên
    // compositor không còn nhận được phím vật lý, trong khi phím bơm lại bằng
    // XTEST chỉ tới được ứng dụng XWayland. Ứng dụng Wayland thuần mất sạch phím,
    // cả phiên coi như liệt bàn phím. Thà không gõ được tiếng Việt còn hơn.
    if (envHasValue("WAYLAND_DISPLAY") && !envHasValue("OPENKEY_ALLOW_X11_ON_WAYLAND")) {
        error = "đang ở phiên Wayland, dùng backend X11 sẽ chặn mất phím của "
                "ứng dụng Wayland (đặt OPENKEY_ALLOW_X11_ON_WAYLAND=1 nếu bạn "
                "chỉ dùng ứng dụng XWayland)";
        return nullptr;
    }
    return makeX11Backend(error);
#else
    error = "bản build này không kèm backend X11";
    return nullptr;
#endif
}

// Backend không làm gì cả. Điểm mấu chốt là nó KHÔNG đụng tới bàn phím: không
// EVIOCGRAB, không grab của compositor. Nhờ vậy khi không gõ được tiếng Việt thì
// người dùng vẫn gõ được bình thường mọi thứ khác.
class NullBackend final : public IBackend {
public:
    const char* name() const override { return "khong-go-duoc"; }
    BackendCaps caps() const override { return {}; }
    bool canType() const override { return false; }
    bool start() override { return true; }
    void stop() override {}
    void sendResult(const DeleteRequest&, const std::u32string&) override {}
    void forwardKey(const KeyEvent&) override {}
    std::string focusedAppId() override { return {}; }
    const std::string& lastError() const override { return _error; }

private:
    std::string _error;
};

} // namespace

std::unique_ptr<IBackend> makeNullBackend() { return std::make_unique<NullBackend>(); }

std::unique_ptr<IBackend> createBackend(BackendKind requested, std::string& notice) {
    notice.clear();

    // Người dùng chỉ định rõ một backend mà backend đó hỏng: vẫn rơi xuống Auto.
    // Tắt hẳn ứng dụng ở đây là kẹt cứng — không chạy thì không mở được bảng điều
    // khiển, mà bảng điều khiển lại là chỗ duy nhất để chọn lại backend.
    // Đổi lại, tuyệt đối không rơi xuống trong im lặng: người gọi phải báo cho
    // người dùng biết, nếu không họ tưởng cấu hình của mình đang có hiệu lực.
    if (requested == BackendKind::Wayland || requested == BackendKind::X11) {
        std::string requestedError;
        auto b = requested == BackendKind::Wayland ? tryWayland(requestedError)
                                                   : tryX11(requestedError);
        if (b) return b;

        std::string autoNotice;
        auto fallback = createBackend(BackendKind::Auto, autoNotice);
        // Rơi xuống được một backend thật thì chỉ cần nói nguyên nhân trần trụi:
        // người gọi đã nói sẵn "không dùng được backend X" rồi. Còn nếu không rơi
        // xuống được gì thì dùng autoNotice, vì nó đã liệt kê cả hai đường.
        notice = fallback->canType() ? requestedError : autoNotice;
        return fallback;
    }

    std::string waylandError;
    if (auto b = tryWayland(waylandError)) {
        return b;
    }

    std::string x11Error;
    if (auto b = tryX11(x11Error)) {
        return b;
    }

    notice = "không dùng được backend nào.\n  wayland: " + waylandError +
             "\n  x11: " + x11Error;
    return makeNullBackend();
}

} // namespace openkey
