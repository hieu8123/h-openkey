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
        error = "khong thay WAYLAND_DISPLAY";
        return nullptr;
    }
    return makeWaylandBackend(error);
#else
    error = "ban build nay khong kem backend Wayland";
    return nullptr;
#endif
}

std::unique_ptr<IBackend> tryX11(std::string& error) {
#ifdef OPENKEY_HAVE_X11
    if (!envHasValue("DISPLAY")) {
        error = "khong thay DISPLAY";
        return nullptr;
    }
    return makeX11Backend(error);
#else
    error = "backend X11 se co o giai doan 3";
    return nullptr;
#endif
}

} // namespace

std::unique_ptr<IBackend> createBackend(BackendKind requested, std::string& error) {
    // Nguoi dung chi dinh ro thi khong tu y roi xuong backend khac: im lang
    // chuyen duong se khien ho tuong cau hinh cua minh dang co hieu luc.
    if (requested == BackendKind::Wayland) {
        auto b = tryWayland(error);
        if (!b) error = "cau hinh yeu cau backend wayland nhung khong dung duoc: " + error;
        return b;
    }
    if (requested == BackendKind::X11) {
        auto b = tryX11(error);
        if (!b) error = "cau hinh yeu cau backend x11 nhung khong dung duoc: " + error;
        return b;
    }

    std::string waylandError;
    if (auto b = tryWayland(waylandError)) {
        return b;
    }

    std::string x11Error;
    if (auto b = tryX11(x11Error)) {
        return b;
    }

    error = "khong dung duoc backend nao.\n  wayland: " + waylandError +
            "\n  x11: " + x11Error;
    return nullptr;
}

} // namespace openkey
