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
    return makeX11Backend(error);
#else
    error = "bản build này không kèm backend X11";
    return nullptr;
#endif
}

} // namespace

std::unique_ptr<IBackend> createBackend(BackendKind requested, std::string& error,
                                        std::string* fallbackReason) {
    if (fallbackReason) fallbackReason->clear();

    // Nguoi dung chi dinh ro mot backend ma backend do hong: van ro xuong Auto.
    // Truoc day cho o day la ket cung - OpenKey khong chay thi khong mo duoc bang
    // dieu khien, ma bang dieu khien lai la cho duy nhat de chon lai backend.
    // Doi lai, tuyet doi khong ro xuong trong im lang: nguoi goi phai bao cho
    // nguoi dung biet, neu khong ho tuong cau hinh cua minh dang co hieu luc.
    if (requested == BackendKind::Wayland || requested == BackendKind::X11) {
        std::string requestedError;
        auto b = requested == BackendKind::Wayland ? tryWayland(requestedError)
                                                   : tryX11(requestedError);
        if (b) return b;

        std::string autoError;
        auto fallback = createBackend(BackendKind::Auto, autoError);
        if (!fallback) {
            error = std::string("cấu hình yêu cầu backend ") +
                    backendKindToString(requested) + " nhưng không dùng được: " +
                    requestedError + "\n" + autoError;
            return nullptr;
        }
        // Chi tra ve nguyen nhan tran trui: nguoi goi da noi san "khong dung duoc
        // backend X" roi, nhac lai lan nua trong cung mot hop thoai la thua.
        if (fallbackReason) *fallbackReason = requestedError;
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

    error = "không dùng được backend nào.\n  wayland: " + waylandError +
            "\n  x11: " + x11Error;
    return nullptr;
}

} // namespace openkey
