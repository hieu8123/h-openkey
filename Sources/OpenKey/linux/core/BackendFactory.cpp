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
    // Dang o phien Wayland thi DISPLAY nay la cua XWayland, va dung backend X11
    // o day la tu ban vao chan: no chan phim ngay o kernel bang EVIOCGRAB nen
    // compositor khong con nhan duoc phim vat ly, trong khi phim bom lai bang
    // XTEST chi toi duoc ung dung XWayland. Ung dung Wayland thuan mat sach phim,
    // ca phien coi nhu liet ban phim. Tha khong go duoc tieng Viet con hon.
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

// Backend khong lam gi ca. Diem mau chot la no KHONG dung toi ban phim: khong
// EVIOCGRAB, khong grab cua compositor. Nho vay khi khong go duoc tieng Viet thi
// nguoi dung van go duoc binh thuong moi thu khac.
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

    // Nguoi dung chi dinh ro mot backend ma backend do hong: van ro xuong Auto.
    // Bat han ung dung o day la ket cung - khong chay thi khong mo duoc bang dieu
    // khien, ma bang dieu khien lai la cho duy nhat de chon lai backend.
    // Doi lai, tuyet doi khong ro xuong trong im lang: nguoi goi phai bao cho
    // nguoi dung biet, neu khong ho tuong cau hinh cua minh dang co hieu luc.
    if (requested == BackendKind::Wayland || requested == BackendKind::X11) {
        std::string requestedError;
        auto b = requested == BackendKind::Wayland ? tryWayland(requestedError)
                                                   : tryX11(requestedError);
        if (b) return b;

        std::string autoNotice;
        auto fallback = createBackend(BackendKind::Auto, autoNotice);
        // Ro xuong duoc mot backend that thi chi can noi nguyen nhan tran trui:
        // nguoi goi da noi san "khong dung duoc backend X" roi. Con neu khong ro
        // xuong duoc gi thi dung autoNotice, vi no da liet ke ca hai duong.
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
