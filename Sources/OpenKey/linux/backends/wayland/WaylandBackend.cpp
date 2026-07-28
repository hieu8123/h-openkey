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
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "CharCodec.h"
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
    bool modActive(const char* name) const;

    wl_display* _display = nullptr;
    wl_registry* _registry = nullptr;
    wl_seat* _seat = nullptr;
    zwp_input_method_manager_v2* _imManager = nullptr;
    zwp_virtual_keyboard_manager_v1* _vkManager = nullptr;
    zwp_input_method_v2* _im = nullptr;
    zwp_input_method_keyboard_grab_v2* _grab = nullptr;
    zwp_virtual_keyboard_v1* _vk = nullptr;

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

void WaylandBackend::onSurroundingText(void* data, zwp_input_method_v2*, const char*,
                                       uint32_t, uint32_t) {
    // Chi can biet ung dung co ho tro hay khong; noi dung khong dung toi vi
    // chung ta tu theo doi do dai da go trong OpenKeyCore.
    OK_LOG("surrounding_text");
    static_cast<WaylandBackend*>(data)->_pending.hasSurroundingText = true;
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
        munmap(map, size);
    }

    // Ban phim ao phai dung dung keymap cua ban phim that, neu khong phim
    // chuyen tiep se ra sai chu.
    if (self->_vk) {
        zwp_virtual_keyboard_v1_keymap(self->_vk, format, fd, size);
    }
    close(fd);
}

void WaylandBackend::onModifiers(void* data, zwp_input_method_keyboard_grab_v2*, uint32_t,
                                 uint32_t depressed, uint32_t latched, uint32_t locked,
                                 uint32_t group) {
    auto* self = static_cast<WaylandBackend*>(data);
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

    // Khong co o nhap nao dang nhan chu thi khong co gi de sua: tra phim ve
    // nguyen ven, dung dung toi engine.
    if (!self->_current.active || !self->_handler) {
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
    _caps.hasAppId = false; // Smart Switch Key thuoc giai doan 2.

    wl_display_roundtrip(_display);

    // `unavailable` toi trong vong roundtrip tren neu co bo go khac dang giu cho.
    return _error.empty();
}

void WaylandBackend::stop() {
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

void WaylandBackend::sendResult(const DeleteRequest& del, const std::u32string& out) {
    if (!_im || !_current.active) return;

    OK_LOG("sendResult: xoa %u byte (%u phim) roi chen \"%s\" [%s]", del.utf8Bytes,
           del.keyPresses, utf8Encode(out).c_str(),
           _current.hasSurroundingText ? "surrounding-text" : "backspace-fallback");

    if (_current.hasSurroundingText) {
        // Duong chinh: mot lan xoa gon gang, dem theo byte UTF-8.
        if (del.utf8Bytes > 0) {
            zwp_input_method_v2_delete_surrounding_text(_im, del.utf8Bytes, 0);
        }
    } else if (del.keyPresses > 0) {
        // Duong fallback: dung ky thuat backspace nhu ban macOS va Windows.
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
