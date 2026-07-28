//
//  X11Backend.cpp
//  OpenKey cho Linux
//

#include "X11Backend.h"

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/record.h>
#include <X11/keysym.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <utility>
#include <vector>

#include "CharCodec.h"

namespace openkey {
namespace {

bool debugEnabled() {
    static const bool on = [] {
        const char* v = std::getenv("OPENKEY_DEBUG");
        return v && *v && std::strcmp(v, "0") != 0;
    }();
    return on;
}

#define X11_LOG(...)                                \
    do {                                            \
        if (debugEnabled()) {                       \
            std::fprintf(stderr, "[openkey/x11] "); \
            std::fprintf(stderr, __VA_ARGS__);      \
            std::fputc('\n', stderr);               \
        }                                           \
    } while (0)

class X11Backend final : public IBackend {
public:
    ~X11Backend() override { stop(); }

    bool open(std::string& error);

    const char* name() const override { return "x11-xtest"; }
    BackendCaps caps() const override { return _caps; }
    bool start() override;
    void stop() override;
    const std::string& lastError() const override { return _error; }
    std::string focusedAppId() override;

    void sendResult(const DeleteRequest& del, const std::u32string& out) override;
    void forwardKey(const KeyEvent&) override {
        // Khong can lam gi: XRecord khong chan phim, nen phim goc da tu den ung
        // dung roi. Day chinh la ly do sendResult phai xoa them mot ky tu.
    }

    int eventFd() const override;
    void dispatchEvents() override;
    void flush() override;

private:
    static void onRecord(XPointer closure, XRecordInterceptData* data);
    void handleKey(uint8_t keycode, bool pressed, uint32_t stateMask);

    void sendKeycode(unsigned int keycode, bool pressed);
    void sendBackspaces(uint32_t count);
    void typeCodePoint(char32_t codePoint);
    void collectSpareKeycodes();

    Display* _control = nullptr; // gui XTEST, doc ban do ban phim
    Display* _data = nullptr;    // nhan luong su kien tu XRecord
    XRecordContext _context = 0;
    XRecordRange* _range = nullptr;

    // Cac keycode X11 chua duoc dung, de gan tam keysym cho ky tu can go.
    std::vector<unsigned int> _spareKeycodes;
    unsigned int _backspaceKeycode = 0;
    size_t _nextSpare = 0;

    // Ky tu ma phim vua bam da tao ra, tinh theo byte UTF-8. XRecord khong chan
    // duoc phim nen ky tu nay da nam trong o nhap, phai tru vao khi xoa.
    uint32_t _pendingEchoBytes = 0;

    // XRecord ghi lai CA su kien do chinh XTEST cua ta bom ra. Khong loc thi
    // backspace va chu ta gui se quay lai vao engine thanh vong lap vo tan.
    // Vi vay moi phim ta bom deu duoc ghi vao day truoc, va khi no quay lai thi
    // bo qua.
    std::deque<std::pair<unsigned int, bool>> _injected;

    BackendCaps _caps;
    std::string _error;
};

X11Backend* g_backend = nullptr;

} // namespace

// --- vong doi ------------------------------------------------------------

bool X11Backend::open(std::string& error) {
    _control = XOpenDisplay(nullptr);
    if (!_control) {
        error = "khong mo duoc man hinh X11";
        return false;
    }

    int major = 0;
    int minor = 0;
    if (!XRecordQueryVersion(_control, &major, &minor)) {
        error = "X server khong co phan mo rong XRecord";
        return false;
    }
    int eventBase = 0;
    int errorBase = 0;
    if (!XTestQueryExtension(_control, &eventBase, &errorBase, &major, &minor)) {
        error = "X server khong co phan mo rong XTEST";
        return false;
    }

    // XRecord doi hoi mot ket noi rieng cho luong du lieu.
    _data = XOpenDisplay(nullptr);
    if (!_data) {
        error = "khong mo duoc ket noi thu hai cho XRecord";
        return false;
    }
    return true;
}

bool X11Backend::start() {
    g_backend = this;

    _backspaceKeycode = XKeysymToKeycode(_control, XK_BackSpace);
    if (_backspaceKeycode == 0) {
        _error = "khong tim duoc keycode cua phim BackSpace";
        return false;
    }
    collectSpareKeycodes();
    if (_spareKeycodes.empty()) {
        _error = "khong con keycode trong nao de go chu";
        return false;
    }
    X11_LOG("co %zu keycode trong de go chu", _spareKeycodes.size());

    _range = XRecordAllocRange();
    if (!_range) {
        _error = "khong cap phat duoc XRecordRange";
        return false;
    }
    _range->device_events.first = KeyPress;
    _range->device_events.last = KeyRelease;

    XRecordClientSpec clients = XRecordAllClients;
    _context = XRecordCreateContext(_control, 0, &clients, 1, &_range, 1);
    if (!_context) {
        _error = "khong tao duoc ngu canh XRecord";
        return false;
    }

    // Ngu canh duoc tao tren ket noi dieu khien nhung bat tren ket noi du lieu,
    // nen phai dong bo truoc: neu khong X server chua biet ngu canh nay ton tai
    // va tra ve XRecordBadContext.
    XSync(_control, False);

    if (!XRecordEnableContextAsync(_data, _context, onRecord, nullptr)) {
        _error = "khong bat duoc ngu canh XRecord";
        return false;
    }
    XFlush(_data);

    _caps.canForwardKey = false; // phim goc tu di, khong can ta chuyen tiep
    _caps.hasSurroundingText = false;
    _caps.hasAppId = true;
    return true;
}

void X11Backend::stop() {
    if (_context && _control) {
        XRecordDisableContext(_control, _context);
        XRecordFreeContext(_control, _context);
        _context = 0;
    }
    if (_range) {
        XFree(_range);
        _range = nullptr;
    }
    if (_data) {
        XCloseDisplay(_data);
        _data = nullptr;
    }
    if (_control) {
        XCloseDisplay(_control);
        _control = nullptr;
    }
    g_backend = nullptr;
}

int X11Backend::eventFd() const {
    return _data ? ConnectionNumber(_data) : -1;
}

void X11Backend::dispatchEvents() {
    if (_data) {
        XRecordProcessReplies(_data);
    }
}

void X11Backend::flush() {
    if (_control) {
        XFlush(_control);
    }
}

// --- ban do ban phim -----------------------------------------------------

void X11Backend::collectSpareKeycodes() {
    _spareKeycodes.clear();

    int minCode = 0;
    int maxCode = 0;
    XDisplayKeycodes(_control, &minCode, &maxCode);

    int perCode = 0;
    KeySym* map = XGetKeyboardMapping(_control, static_cast<KeyCode>(minCode),
                                      maxCode - minCode + 1, &perCode);
    if (!map) {
        return;
    }

    for (int code = minCode; code <= maxCode; code++) {
        bool used = false;
        for (int level = 0; level < perCode; level++) {
            if (map[(code - minCode) * perCode + level] != NoSymbol) {
                used = true;
                break;
            }
        }
        if (!used) {
            _spareKeycodes.push_back(static_cast<unsigned int>(code));
        }
    }
    XFree(map);
}

// --- nhan phim -----------------------------------------------------------

void X11Backend::onRecord(XPointer, XRecordInterceptData* data) {
    if (!g_backend || data->category != XRecordFromServer) {
        XRecordFreeData(data);
        return;
    }

    // Goi tin thiet bi cua X11: byte 0 la loai su kien, byte 1 la keycode,
    // va mat na phim bo tro nam o offset 28.
    const unsigned char* payload = data->data;
    const int type = payload[0] & 0x7F;
    const uint8_t keycode = payload[1];
    uint32_t stateMask = 0;
    std::memcpy(&stateMask, payload + 28, sizeof(uint32_t));

    if (type == KeyPress || type == KeyRelease) {
        g_backend->handleKey(keycode, type == KeyPress, stateMask);
    }
    XRecordFreeData(data);
}

void X11Backend::handleKey(uint8_t keycode, bool pressed, uint32_t stateMask) {
    if (!_handler) {
        return;
    }

    // Phim nay la do chinh ta bom ra: bo qua, neu khong se thanh vong lap.
    if (!_injected.empty() && _injected.front().first == keycode &&
        _injected.front().second == pressed) {
        _injected.pop_front();
        return;
    }

    KeyEvent ev;
    ev.keycode = keycode; // keycode X11, dung nhu platforms/linux.h
    ev.pressed = pressed;
    ev.shift = (stateMask & ShiftMask) != 0;
    ev.capsLock = (stateMask & LockMask) != 0;
    ev.ctrl = (stateMask & ControlMask) != 0;
    ev.alt = (stateMask & Mod1Mask) != 0;
    ev.super = (stateMask & Mod4Mask) != 0;

    // Ky tu ma phim nay vua tao ra trong o nhap. Can biet do dai cua no de
    // sendResult xoa cho dung.
    _pendingEchoBytes = 0;
    if (pressed) {
        const KeySym sym = XkbKeycodeToKeysym(_control, keycode, 0, ev.shift ? 1 : 0);
        if (sym >= 0x20 && sym <= 0x7E) {
            _pendingEchoBytes = 1;
        }
    }

    _handler(ev);
}

// --- xuat chu ------------------------------------------------------------

void X11Backend::sendKeycode(unsigned int keycode, bool pressed) {
    _injected.emplace_back(keycode, pressed);
    XTestFakeKeyEvent(_control, keycode, pressed ? True : False, 0);
}

void X11Backend::sendBackspaces(uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        sendKeycode(_backspaceKeycode, true);
        sendKeycode(_backspaceKeycode, false);
    }
}

void X11Backend::typeCodePoint(char32_t codePoint) {
    // Quy uoc cua X11: keysym cua mot code point Unicode la code point + 0x1000000.
    const KeySym sym = codePoint < 0x80 ? static_cast<KeySym>(codePoint)
                                       : static_cast<KeySym>(codePoint) | 0x01000000;

    // Luan phien qua cac keycode trong: dung mai mot ma roi doi ban do lien tuc
    // se khien ung dung giai ma bang ban do cu.
    const unsigned int code = _spareKeycodes[_nextSpare];
    _nextSpare = (_nextSpare + 1) % _spareKeycodes.size();
    KeySym symbols[2] = {sym, sym};
    XChangeKeyboardMapping(_control, static_cast<int>(code), 2, symbols, 1);
    XSync(_control, False);

    sendKeycode(code, true);
    sendKeycode(code, false);
    XSync(_control, False);
}

void X11Backend::sendResult(const DeleteRequest& del, const std::u32string& out) {
    // Phim goc khong bi chan nen ky tu cua no da nam trong o nhap: phai xoa
    // them chinh no, khong thi se con lai mot ky tu la.
    uint32_t backspaces = del.keyPresses + (_pendingEchoBytes > 0 ? 1 : 0);
    _pendingEchoBytes = 0;

    X11_LOG("sendResult: xoa %u phim roi chen \"%s\"", backspaces,
            utf8Encode(out).c_str());

    sendBackspaces(backspaces);
    for (char32_t cp : out) {
        typeCodePoint(cp);
    }
    flush();
}

std::string X11Backend::focusedAppId() {
    if (!_control) {
        return {};
    }

    Window focus = None;
    int revert = 0;
    XGetInputFocus(_control, &focus, &revert);
    if (focus == None || focus == PointerRoot) {
        return {};
    }

    // WM_CLASS cua X11 la tuong duong gan nhat cua app-id tren Wayland.
    XClassHint hint;
    std::memset(&hint, 0, sizeof(hint));
    std::string appId;
    if (XGetClassHint(_control, focus, &hint)) {
        if (hint.res_class) {
            appId = hint.res_class;
            XFree(hint.res_class);
        }
        if (hint.res_name) {
            XFree(hint.res_name);
        }
    }
    return appId;
}

std::unique_ptr<IBackend> makeX11Backend(std::string& error) {
    auto backend = std::make_unique<X11Backend>();
    if (!backend->open(error)) {
        return nullptr;
    }
    return backend;
}

} // namespace openkey
