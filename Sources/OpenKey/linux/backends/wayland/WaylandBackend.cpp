//
//  WaylandBackend.cpp
//  OpenKey cho Linux
//
//  Ghi chu ve kien truc: ca hai manh (input method va virtual keyboard) nam
//  chung mot file vi chung dung chung mot ket noi, mot con dau thoi gian va mot
//  keymap; tach ra se phai chuyen qua lai gan nhu toan bo trang thai.
//
//  Nguyen tac quan trong nhat: KHONG BAO GIO dung set_preedit_string. Preedit
//  chinh la thu gay gach chan va nhan doi chu ma OpenKey sinh ra de loai bo.
//  Chung ta chi dung delete_surrounding_text + commit_string.
//

#include "WaylandBackend.h"

#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/memfd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "CharCodec.h"
#include "KeymapBuilder.h"
#include "ToplevelWatcher.h"
#include "cosmic-toplevel-info-unstable-v1-client-protocol.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "input-method-unstable-v2-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

namespace openkey {
namespace {

// Bat bang OPENKEY_DEBUG=1. Chan doan tren Wayland rat kho neu khong nhin duoc
// ung dung nao cho surrounding text va chung ta da gui gi xuong.
bool debugEnabled() {
    static const bool on = [] {
        const char* v = std::getenv("OPENKEY_DEBUG");
        return v && *v && std::strcmp(v, "0") != 0;
    }();
    return on;
}

#define OK_LOG(...)                              \
    do {                                         \
        if (debugEnabled()) {                    \
            std::fprintf(stderr, "[openkey] ");  \
            std::fprintf(stderr, __VA_ARGS__);   \
            std::fputc('\n', stderr);            \
        }                                        \
    } while (0)

// Keycode evdev cua phim BackSpace. Duong fallback dung no khi ung dung khong
// ho tro surrounding text.
constexpr uint32_t kEvdevBackspace = 14;

// Wayland gui keycode evdev; engine (platforms/linux.h) dung keycode X11.
constexpr uint32_t kEvdevToX11 = 8;

// Keycode X11 cua cac phim bo tro.
bool isModifierKeycode(uint32_t keycode) {
    switch (keycode) {
        case 50: case 62:   // Shift
        case 37: case 105:  // Control
        case 64: case 108:  // Alt
        case 133: case 134: // Super
        case 66: case 77: case 78: // Caps/Num/Scroll Lock
            return true;
        default:
            return false;
    }
}

class WaylandBackend final : public IBackend {
public:
    ~WaylandBackend() override { stop(); }

    bool connect(std::string& error);

    const char* name() const override { return "wayland-im-v2"; }
    BackendCaps caps() const override { return _caps; }
    bool start() override;
    void stop() override;
    const std::string& lastError() const override { return _error; }
    std::string focusedAppId() override { return _appId; }

    void sendResult(const DeleteRequest& del, const std::u32string& out) override;
    void forwardKey(const KeyEvent& ev) override;

    int eventFd() const override {
        return _display ? wl_display_get_fd(_display) : -1;
    }
    void dispatchEvents() override {
        if (_display) wl_display_dispatch(_display);
    }
    void flush() override {
        if (_display) wl_display_flush(_display);
    }

private:
    // --- registry ---------------------------------------------------------
    static void onGlobal(void* data, wl_registry* r, uint32_t id, const char* iface,
                         uint32_t version);
    static void onGlobalRemove(void*, wl_registry*, uint32_t) {}

    // --- zwp_input_method_v2 ---------------------------------------------
    static void onActivate(void* data, zwp_input_method_v2*);
    static void onDeactivate(void* data, zwp_input_method_v2*);
    static void onSurroundingText(void* data, zwp_input_method_v2*, const char* text,
                                  uint32_t cursor, uint32_t anchor);
    static void onTextChangeCause(void*, zwp_input_method_v2*, uint32_t cause) {
        OK_LOG("text_change_cause: %u", cause);
    }
    static void onContentType(void*, zwp_input_method_v2*, uint32_t hint, uint32_t purpose) {
        OK_LOG("content_type: hint=%u purpose=%u", hint, purpose);
    }
    static void onDone(void* data, zwp_input_method_v2*);
    static void onUnavailable(void* data, zwp_input_method_v2*);

    // --- zwp_input_method_keyboard_grab_v2 -------------------------------
    static void onKeymap(void* data, zwp_input_method_keyboard_grab_v2*, uint32_t format,
                         int32_t fd, uint32_t size);
    static void onKey(void* data, zwp_input_method_keyboard_grab_v2*, uint32_t serial,
                      uint32_t time, uint32_t key, uint32_t state);
    static void onModifiers(void* data, zwp_input_method_keyboard_grab_v2*,
                            uint32_t serial, uint32_t depressed, uint32_t latched,
                            uint32_t locked, uint32_t group);
    static void onRepeatInfo(void*, zwp_input_method_keyboard_grab_v2*, int32_t, int32_t) {}

    void regrabKeyboard();
    void sendBackspaces(uint32_t count);

    // Duong xuat cho ung dung khong noi text-input-v3 (XWayland, Chrome,
    // VS Code, phan lon terminal): nap mot keymap chi chua dung nhung ky tu
    // can go, bam chung, roi tra keymap that ve ngay.
    void typeViaVirtualKeyboard(const std::u32string& out);
    bool uploadKeymap(zwp_virtual_keyboard_v1* vk, const std::string& keymap);
    bool modActive(const char* name) const;

    wl_display* _display = nullptr;
    wl_registry* _registry = nullptr;
    wl_seat* _seat = nullptr;
    zwp_input_method_manager_v2* _imManager = nullptr;
    zwp_virtual_keyboard_manager_v1* _vkManager = nullptr;
    zwp_input_method_v2* _im = nullptr;
    zwp_input_method_keyboard_grab_v2* _grab = nullptr;
    ext_foreign_toplevel_list_v1* _toplevelList = nullptr;
    zcosmic_toplevel_info_v1* _toplevelInfo = nullptr;
    zwlr_foreign_toplevel_manager_v1* _wlrToplevels = nullptr;
    ToplevelWatcher _toplevels;

    // Mot ban phim ao duy nhat, keymap ghep san va nap mot lan: vua chuyen tiep
    // phim goc vua go chu, nen khong bao gio phai doi keymap.
    zwp_virtual_keyboard_v1* _vk = nullptr;
    std::string _realKeymap;
    KeymapBuilder _keymap;
    bool _keymapUploaded = false;

    xkb_context* _xkbContext = nullptr;
    xkb_keymap* _xkbKeymap = nullptr;
    xkb_state* _xkbState = nullptr;

    // Trang thai cua zwp_input_method_v2 la double-buffered: gom vao `_pending`
    // roi chi ap dung khi nhan su kien `done`.
    struct {
        bool active = false;
        bool hasSurroundingText = false;
    } _pending, _current;

    // cosmic-comp chi cai keyboard grab tai thoi diem grab_keyboard duoc goi.
    // Grab tao luc khoi dong (khi chua co o nhap nao) bi bo di sau lan doi
    // focus dau tien, nen phai grab lai moi lan input method duoc kich hoat.
    bool _grabbedWhileActive = false;

    uint32_t _serial = 0; // so su kien `done` da nhan, dung cho commit()
    uint32_t _lastTime = 0;

    BackendCaps _caps;
    std::string _error;
    std::string _appId;
};

// --- registry ------------------------------------------------------------

void WaylandBackend::onGlobal(void* data, wl_registry* r, uint32_t id, const char* iface,
                              uint32_t version) {
    auto* self = static_cast<WaylandBackend*>(data);
    if (std::strcmp(iface, wl_seat_interface.name) == 0) {
        const uint32_t v = version < 7 ? version : 7;
        self->_seat = static_cast<wl_seat*>(
            wl_registry_bind(r, id, &wl_seat_interface, v));
    } else if (std::strcmp(iface, zwp_input_method_manager_v2_interface.name) == 0) {
        self->_imManager = static_cast<zwp_input_method_manager_v2*>(
            wl_registry_bind(r, id, &zwp_input_method_manager_v2_interface, 1));
    } else if (std::strcmp(iface, ext_foreign_toplevel_list_v1_interface.name) == 0) {
        self->_toplevelList = static_cast<ext_foreign_toplevel_list_v1*>(
            wl_registry_bind(r, id, &ext_foreign_toplevel_list_v1_interface, 1));
    } else if (std::strcmp(iface, zcosmic_toplevel_info_v1_interface.name) == 0) {
        // Can it nhat ban 2: tu ban do tro di moi co get_cosmic_toplevel de
        // gan trang thai vao handle cua giao thuc chuan.
        if (version >= 2) {
            const uint32_t v = version < 3 ? version : 3;
            self->_toplevelInfo = static_cast<zcosmic_toplevel_info_v1*>(
                wl_registry_bind(r, id, &zcosmic_toplevel_info_v1_interface, v));
        }
    } else if (std::strcmp(iface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        const uint32_t v = version < 3 ? version : 3;
        self->_wlrToplevels = static_cast<zwlr_foreign_toplevel_manager_v1*>(
            wl_registry_bind(r, id, &zwlr_foreign_toplevel_manager_v1_interface, v));
    } else if (std::strcmp(iface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
        self->_vkManager = static_cast<zwp_virtual_keyboard_manager_v1*>(
            wl_registry_bind(r, id, &zwp_virtual_keyboard_manager_v1_interface, 1));
    }
}

// --- input method --------------------------------------------------------

void WaylandBackend::onActivate(void* data, zwp_input_method_v2*) {
    auto* self = static_cast<WaylandBackend*>(data);
    OK_LOG("activate");
    self->_pending.active = true;
    // Moi lan focus vao o nhap moi, phai gia dinh la khong co surrounding text
    // cho toi khi su kien tuong ung toi truoc `done`.
    self->_pending.hasSurroundingText = false;
}

void WaylandBackend::onDeactivate(void* data, zwp_input_method_v2*) {
    OK_LOG("deactivate");
    static_cast<WaylandBackend*>(data)->_pending.active = false;
}

void WaylandBackend::onSurroundingText(void* data, zwp_input_method_v2*,
                                       const char* text, uint32_t cursor,
                                       uint32_t anchor) {
    auto* self = static_cast<WaylandBackend*>(data);
    self->_pending.hasSurroundingText = true;

    // Ghi lai 20 byte ngay truoc con tro. Day la bang chung truc tiep ve viec
    // lan xoa vua roi co an hay khong — thay cho viec phong doan.
    const std::string all = text ? text : "";
    const size_t at = cursor <= all.size() ? cursor : all.size();
    const size_t from = at > 20 ? at - 20 : 0;
    OK_LOG("surrounding_text: truoc con tro=\"%s\" (%zu byte)",
           all.substr(from, at - from).c_str(), at);


}

void WaylandBackend::regrabKeyboard() {
    static const zwp_input_method_keyboard_grab_v2_listener grabListener = {
        onKeymap, onKey, onModifiers, onRepeatInfo};

    if (_grab) {
        zwp_input_method_keyboard_grab_v2_release(_grab);
        _grab = nullptr;
    }
    _grab = zwp_input_method_v2_grab_keyboard(_im);
    if (_grab) {
        zwp_input_method_keyboard_grab_v2_add_listener(_grab, &grabListener, this);
        OK_LOG("grab lai ban phim");
    }
    flush();
}

void WaylandBackend::onDone(void* data, zwp_input_method_v2*) {
    auto* self = static_cast<WaylandBackend*>(data);
    self->_serial++;
    self->_current = self->_pending;
    self->_caps.hasSurroundingText = self->_current.hasSurroundingText;
    OK_LOG("done: active=%d surroundingText=%d serial=%u", self->_current.active,
           self->_current.hasSurroundingText, self->_serial);

    if (self->_current.active && !self->_grabbedWhileActive) {
        self->regrabKeyboard();
        self->_grabbedWhileActive = true;
    } else if (!self->_current.active) {
        self->_grabbedWhileActive = false;
    }
}

void WaylandBackend::onUnavailable(void* data, zwp_input_method_v2*) {
    auto* self = static_cast<WaylandBackend*>(data);
    self->_error =
        "mot bo go khac dang giu input method cua phien nay. "
        "Hay tat fcitx5 va ibus roi chay lai OpenKey.";
    self->_current.active = false;
    self->_pending.active = false;
}

// --- keyboard grab -------------------------------------------------------


void WaylandBackend::onKeymap(void* data, zwp_input_method_keyboard_grab_v2*,
                              uint32_t format, int32_t fd, uint32_t size) {
    auto* self = static_cast<WaylandBackend*>(data);

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    OK_LOG("keymap: format=%u size=%u", format, size);
    char* map = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (map != MAP_FAILED) {
        if (self->_xkbState) xkb_state_unref(self->_xkbState);
        if (self->_xkbKeymap) xkb_keymap_unref(self->_xkbKeymap);
        self->_xkbKeymap = xkb_keymap_new_from_string(
            self->_xkbContext, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        self->_xkbState = self->_xkbKeymap ? xkb_state_new(self->_xkbKeymap) : nullptr;
        // Giu ban sao de tra ve sau moi lan go chu bang keymap sinh dong.
        self->_realKeymap.assign(map, strnlen(map, size));
        if (const char* dump = std::getenv("OPENKEY_DUMP_KEYMAP")) {
            if (FILE* f = std::fopen(dump, "wb")) {
                std::fwrite(self->_realKeymap.data(), 1, self->_realKeymap.size(), f);
                std::fclose(f);
            }
        }
        munmap(map, size);
    }

    // Nap keymap MOT LAN: ban ghep gom keymap that cong them mot phim cho tung
    // ky tu tieng Viet. Nap lai nhieu lan se lam ung dung phai bien dich lai,
    // va do chinh la nguon goc cua loi go ra sai chu.
    if (self->_vk && !self->_keymapUploaded) {
        if (self->_keymap.build(self->_realKeymap)) {
            if (self->uploadKeymap(self->_vk, self->_keymap.mergedKeymap())) {
                self->_keymapUploaded = true;
                OK_LOG("nap keymap ghep san, %zu ky tu go duoc bang ban phim ao",
                       self->_keymap.mappedCount());
            }
        }
        if (!self->_keymapUploaded) {
            // Khong ghep duoc thi it nhat phim chuyen tiep phai dung.
            zwp_virtual_keyboard_v1_keymap(self->_vk, format, fd, size);
            OK_LOG("khong ghep duoc keymap, khong go duoc chu bang ban phim ao");
        }
    }
    close(fd);
}

void WaylandBackend::onModifiers(void* data, zwp_input_method_keyboard_grab_v2*, uint32_t,
                                 uint32_t depressed, uint32_t latched, uint32_t locked,
                                 uint32_t group) {
    auto* self = static_cast<WaylandBackend*>(data);
    OK_LOG("modifiers: depressed=0x%X latched=0x%X locked=0x%X group=%u", depressed,
           latched, locked, group);
    if (self->_xkbState) {
        xkb_state_update_mask(self->_xkbState, depressed, latched, locked, 0, 0, group);
    }
    // Ung dung can biet trang thai phim bo tro, neu khong Shift+a se ra 'a'.
    if (self->_vk) {
        zwp_virtual_keyboard_v1_modifiers(self->_vk, depressed, latched, locked, group);
    }
}

bool WaylandBackend::modActive(const char* name) const {
    if (!_xkbState) return false;
    return xkb_state_mod_name_is_active(_xkbState, name, XKB_STATE_MODS_EFFECTIVE) > 0;
}

void WaylandBackend::onKey(void* data, zwp_input_method_keyboard_grab_v2*, uint32_t,
                           uint32_t time, uint32_t key, uint32_t state) {
    auto* self = static_cast<WaylandBackend*>(data);
    self->_lastTime = time;
    OK_LOG("key: evdev=%u state=%u active=%d", key, state, self->_current.active);

    KeyEvent ev;
    ev.keycode = key + kEvdevToX11;
    ev.pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    ev.shift = self->modActive(XKB_MOD_NAME_SHIFT);
    ev.capsLock = self->modActive(XKB_MOD_NAME_CAPS);
    ev.ctrl = self->modActive(XKB_MOD_NAME_CTRL);
    ev.alt = self->modActive(XKB_MOD_NAME_ALT);
    ev.super = self->modActive(XKB_MOD_NAME_LOGO);

    // Engine chay ke ca khi khong co o nhap text-input-v3 (XWayland, Chrome,
    // VS Code): luc do ket qua se duoc go ra bang ban phim ao.
    if (!self->_handler) {
        self->forwardKey(ev);
        self->flush();
        return;
    }

    if (self->_handler(ev) == KeyVerdict::Forward) {
        self->forwardKey(ev);
    }
    self->flush();
}

// --- IBackend ------------------------------------------------------------

bool WaylandBackend::connect(std::string& error) {
    _display = wl_display_connect(nullptr);
    if (!_display) {
        error = "khong ket noi duoc toi compositor Wayland";
        return false;
    }

    _registry = wl_display_get_registry(_display);
    static const wl_registry_listener registryListener = {onGlobal, onGlobalRemove};
    wl_registry_add_listener(_registry, &registryListener, this);
    wl_display_roundtrip(_display);

    if (!_seat) {
        error = "compositor khong cung cap wl_seat";
        return false;
    }
    if (!_imManager) {
        error =
            "compositor khong ho tro zwp_input_method_manager_v2, "
            "nen khong the tu lam bo go";
        return false;
    }
    if (!_vkManager) {
        error = "compositor khong ho tro zwp_virtual_keyboard_manager_v1";
        return false;
    }
    return true;
}

bool WaylandBackend::start() {
    _xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!_xkbContext) {
        _error = "khong tao duoc xkb context";
        return false;
    }

    // Tao ban phim ao truoc, de khi su kien keymap toi la co san cho nap vao.
    _vk = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(_vkManager, _seat);
    if (!_vk) {
        _error = "khong tao duoc ban phim ao";
        return false;
    }

    _im = zwp_input_method_manager_v2_get_input_method(_imManager, _seat);
    if (!_im) {
        _error = "khong tao duoc input method";
        return false;
    }
    static const zwp_input_method_v2_listener imListener = {
        onActivate, onDeactivate, onSurroundingText, onTextChangeCause,
        onContentType, onDone, onUnavailable};
    zwp_input_method_v2_add_listener(_im, &imListener, this);

    _grab = zwp_input_method_v2_grab_keyboard(_im);
    if (!_grab) {
        _error = "khong grab duoc ban phim";
        return false;
    }
    static const zwp_input_method_keyboard_grab_v2_listener grabListener = {
        onKeymap, onKey, onModifiers, onRepeatInfo};
    zwp_input_method_keyboard_grab_v2_add_listener(_grab, &grabListener, this);

    _caps.canForwardKey = true;

    // Smart Switch Key chi chay duoc khi compositor cho biet cua so nao dang
    // duoc kich hoat. Thieu thi bo qua, phan con lai van chay binh thuong.
    _toplevels.onFocusChanged = [this](const std::string& appId) {
        _appId = appId;
        OK_LOG("focus doi sang: %s", appId.c_str());
        if (_focusHandler) {
            _focusHandler(appId);
        }
    };
    _caps.hasAppId = _toplevels.start(_toplevelList, _toplevelInfo, _wlrToplevels);
    if (_caps.hasAppId) {
        OK_LOG("theo doi cua so dang focus qua %s", _toplevels.mode());
    } else {
        OK_LOG("khong theo doi duoc cua so dang focus, tat Smart Switch Key");
    }

    wl_display_roundtrip(_display);

    // `unavailable` toi trong vong roundtrip tren neu co bo go khac dang giu cho.
    return _error.empty();
}

void WaylandBackend::stop() {
    _toplevels.stop();
    if (_grab) { zwp_input_method_keyboard_grab_v2_release(_grab); _grab = nullptr; }
    if (_im) { zwp_input_method_v2_destroy(_im); _im = nullptr; }
    if (_vk) { zwp_virtual_keyboard_v1_destroy(_vk); _vk = nullptr; }
    if (_xkbState) { xkb_state_unref(_xkbState); _xkbState = nullptr; }
    if (_xkbKeymap) { xkb_keymap_unref(_xkbKeymap); _xkbKeymap = nullptr; }
    if (_xkbContext) { xkb_context_unref(_xkbContext); _xkbContext = nullptr; }
    if (_display) { wl_display_flush(_display); wl_display_disconnect(_display); _display = nullptr; }
}

void WaylandBackend::forwardKey(const KeyEvent& ev) {
    if (!_vk) return;

    // Khong gui keycode cua phim bo tro. Giao thuc virtual-keyboard co request
    // `modifiers` rieng chinh vi compositor KHONG tu suy ra trang thai bo tro
    // tu cac phim ta bom vao. Gui ca hai duong la dem hai lan, va trang thai
    // Shift bi ket o trang thai dang giu — dung trieu chung da thay khi chay
    // that: caps=1 dinh lien tuc qua hang loat phim khong lien quan.
    if (isModifierKeycode(ev.keycode)) {
        return;
    }

    zwp_virtual_keyboard_v1_key(
        _vk, _lastTime, ev.keycode - kEvdevToX11,
        ev.pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
}

void WaylandBackend::sendBackspaces(uint32_t count) {
    if (!_vk) return;
    for (uint32_t i = 0; i < count; i++) {
        zwp_virtual_keyboard_v1_key(_vk, _lastTime, kEvdevBackspace,
                                    WL_KEYBOARD_KEY_STATE_PRESSED);
        zwp_virtual_keyboard_v1_key(_vk, _lastTime, kEvdevBackspace,
                                    WL_KEYBOARD_KEY_STATE_RELEASED);
    }
}


bool WaylandBackend::uploadKeymap(zwp_virtual_keyboard_v1* vk, const std::string& keymap) {
    if (!vk || keymap.empty()) return false;

    const int fd = static_cast<int>(syscall(SYS_memfd_create, "openkey-keymap", MFD_CLOEXEC));
    if (fd < 0) return false;

    const size_t size = keymap.size() + 1;
    if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
        close(fd);
        return false;
    }
    void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        close(fd);
        return false;
    }
    std::memcpy(p, keymap.c_str(), size);
    munmap(p, size);

    zwp_virtual_keyboard_v1_keymap(vk, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd,
                                   static_cast<uint32_t>(size));
    close(fd);
    return true;
}

void WaylandBackend::typeViaVirtualKeyboard(const std::u32string& out) {
    if (!_vk || out.empty()) return;

    if (!_keymapUploaded) {
        OK_LOG("chua co keymap ghep san, bo qua \"%s\"", utf8Encode(out).c_str());
        return;
    }

    // Khong duoc de sot phim bo tro nao dang giu, neu khong chu se ra sai.
    zwp_virtual_keyboard_v1_modifiers(_vk, 0, 0, 0, 0);

    for (char32_t cp : out) {
        const int code = _keymap.evdevKeycodeFor(cp);
        if (code < 0) {
            // Thuong la ky tu cua bang ma cu (TCVN3, VNI-Windows) khong nam
            // trong bang ghep. Bo qua con hon go ra ky tu sai.
            OK_LOG("khong co ma phim cho U+%04X, bo qua", static_cast<unsigned>(cp));
            continue;
        }
        zwp_virtual_keyboard_v1_key(_vk, _lastTime, static_cast<uint32_t>(code),
                                    WL_KEYBOARD_KEY_STATE_PRESSED);
        zwp_virtual_keyboard_v1_key(_vk, _lastTime, static_cast<uint32_t>(code),
                                    WL_KEYBOARD_KEY_STATE_RELEASED);
    }
}

void WaylandBackend::sendResult(const DeleteRequest& del, const std::u32string& out) {
    if (!_im) return;

    OK_LOG("sendResult: xoa %u byte (%u phim) roi chen \"%s\" [%s]", del.utf8Bytes,
           del.keyPresses, utf8Encode(out).c_str(),
           !_current.active ? "ban-phim-ao" : "backspace+commit");

    // NGUYEN TAC: khong bao gio tron hai co che trong cung mot lan xuat.
    //
    // Phim BackSpace ao di qua dinh tuyen ban phim, con commit_string di qua
    // text-input. Khong co gi bao dam ung dung xu ly chung dung thu tu, va khi
    // sai thu tu thi ra dung nhung loi da thay khi chay that: go "con"+f ra
    // "conon" (xoa khong an) hoac go "nefu" ra "ieu" (xoa lo mat chu dau).
    //
    // Vi vay: co surrounding text thi ca xoa lan chen deu qua text-input;
    // khong co thi ca hai deu qua ban phim ao.
    // Khong co o nhap text-input-v3 nao (XWayland: Chrome, VS Code). Chi con
    // duong ban phim ao.
    if (!_current.active) {
        if (del.keyPresses > 0) sendBackspaces(del.keyPresses);
        typeViaVirtualKeyboard(out);
        flush();
        return;
    }

    // Xoa bang phim BackSpace that, cho MOI ung dung.
    //
    // Truoc day o day dung delete_surrounding_text khi ung dung co ve ho tro,
    // roi them ca mot co che tu do de biet ung dung nao that su xu ly. Ca hai
    // deu sai huong: cosmic-term BAO CAO surrounding text nhung BO QUA yeu cau
    // xoa (hop ly — bo soan dong cua shell moi so huu van ban), va phep do thi
    // gan vao app-id, ma app-id bao ve khong luon khop voi ung dung dang thuc
    // su nhan chu.
    //
    // Phim BackSpace that thi chay o moi noi: terminal buoc phai dung no, con
    // Firefox va editor cung nhan no binh thuong. delete_surrounding_text chi
    // gon hon ve hinh thuc chu khong doi lay duoc gi.
    if (del.keyPresses > 0) {
        sendBackspaces(del.keyPresses);
    }
    if (!out.empty()) {
        zwp_input_method_v2_commit_string(_im, utf8Encode(out).c_str());
    }
    zwp_input_method_v2_commit(_im, _serial);
    flush();
}

} // namespace

std::unique_ptr<IBackend> makeWaylandBackend(std::string& error) {
    auto backend = std::make_unique<WaylandBackend>();
    if (!backend->connect(error)) {
        return nullptr;
    }
    return backend;
}

} // namespace openkey
