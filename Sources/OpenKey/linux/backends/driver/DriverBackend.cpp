//
//  DriverBackend.cpp
//

#include "DriverBackend.h"

#include <linux/input.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <set>
#include <thread>

#include "CharCodec.h"
#include "DebugLog.h"
#include "DriverKeymap.h"
#include "EvdevKeyboard.h"
#include "UInputKeyboard.h"

namespace openkey {
namespace {

#define DRIVER_LOG(...) debugLog("driver", __VA_ARGS__)

constexpr uint32_t kEvdevToX11 = 8;

bool isModifier(uint16_t code) {
    switch (code) {
        case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT:
        case KEY_LEFTCTRL: case KEY_RIGHTCTRL:
        case KEY_LEFTALT: case KEY_RIGHTALT:
        case KEY_LEFTMETA: case KEY_RIGHTMETA:
        case KEY_CAPSLOCK: case KEY_NUMLOCK: case KEY_SCROLLLOCK:
            return true;
        default:
            return false;
    }
}

bool isHeldModifier(uint16_t code) {
    switch (code) {
        case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT:
        case KEY_LEFTCTRL: case KEY_RIGHTCTRL:
        case KEY_LEFTALT: case KEY_RIGHTALT:
        case KEY_LEFTMETA: case KEY_RIGHTMETA:
            return true;
        default:
            return false;
    }
}

class DriverBackend final : public IBackend {
public:
    ~DriverBackend() override { stop(); }

    const char* name() const override { return "evdev-uinput"; }
    BackendCaps caps() const override { return _caps; }
    bool start() override;
    void activate() override;
    void stop() override;
    const std::string& lastError() const override { return _error; }
    std::string focusedAppId() override { return {}; }

    void sendResult(const DeleteRequest& del, const std::u32string& out) override;
    void forwardKey(const KeyEvent& ev) override;

private:
    void inputLoop();
    void handleKey(const EvdevKeyEvent& raw);
    void sendVirtualKey(uint16_t code, int32_t value);
    void tapVirtualKey(uint16_t code);
    void typeCodePoint(char32_t codePoint);
    std::vector<uint16_t> releaseHeldModifiers();
    void restoreHeldModifiers(const std::vector<uint16_t>& released);

    EvdevKeyboard _evdev;
    UInputKeyboard _uinput;
    std::set<uint16_t> _virtualKeysDown;
    std::set<uint16_t> _swallowedPhysicalKeys;
    std::set<uint16_t> _tappedPhysicalKeys;
    bool _capsLock = false;
    std::atomic<bool> _running{false};
    std::thread _worker;
    BackendCaps _caps;
    std::string _error;
};

bool DriverBackend::start() {
    if (!driverXkbLayoutIsActive(_error)) {
        _error = "layout H-OpenKey chưa hoạt động: " + _error;
        return false;
    }
    // GNOME/Mutter khong phai luc nao cung dong bo keymap moi sang XWayland
    // trong cung phien. Lenh nay chi cap nhat keymap, khong nam tren duong go.
    if (std::getenv("DISPLAY") != nullptr) {
        const int status = std::system(
            "setxkbmap -layout custom >/dev/null 2>&1");
        DRIVER_LOG("dong bo XWayland layout custom: status=%d", status);
    }
    if (!_uinput.start(_error)) return false;

    // UI_DEV_CREATE tra ve truoc khi udev va compositor mo xong thiet bi. Day
    // chi la thoi gian khoi dong, khong nam tren duong go phim.
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    if (!_evdev.start(_error, driverReservedKeycodes(),
                      kOpenKeyVirtualKeyboardName)) {
        _uinput.stop();
        return false;
    }
    _evdev.onKey = [this](const EvdevKeyEvent& ev) { handleKey(ev); };
    _evdev.onContextBreak = [this] {
        if (_contextBreakHandler) _contextBreakHandler();
    };

    _caps.canForwardKey = true;
    _caps.hasSurroundingText = false;
    _caps.hasAppId = false;
    DRIVER_LOG("bat dau: grab evdev, xuat qua uinput");
    return true;
}

void DriverBackend::activate() {
    if (_running.exchange(true)) return;
    try {
        _worker = std::thread([this] { inputLoop(); });
    } catch (...) {
        _running = false;
        _error = "không tạo được luồng bàn phím riêng";
    }
}

void DriverBackend::stop() {
    _running = false;
    _evdev.wake();
    if (_worker.joinable()) _worker.join();
    _evdev.stop();
    _uinput.stop();
    _virtualKeysDown.clear();
    _swallowedPhysicalKeys.clear();
    _tappedPhysicalKeys.clear();
}

void DriverBackend::inputLoop() {
    while (_running.load()) {
        _evdev.waitAndDispatch(-1);
    }
}

void DriverBackend::sendVirtualKey(uint16_t code, int32_t value) {
    if (value == 1) {
        _virtualKeysDown.insert(code);
    } else if (value == 0) {
        _virtualKeysDown.erase(code);
    }
    if (!_uinput.sendKey(code, value)) {
        _error = "không ghi được sự kiện vào /dev/uinput";
    }
}

void DriverBackend::tapVirtualKey(uint16_t code) {
    // Neu phim that chua kip nha ma ket qua can go lai dung phim do, mot lan
    // press thu hai se bi bo. Nha ban ao truoc; release vat ly toi sau la thua
    // nhung vo hai.
    if (_virtualKeysDown.count(code) > 0) sendVirtualKey(code, 0);
    sendVirtualKey(code, 1);
    sendVirtualKey(code, 0);
}

void DriverBackend::forwardKey(const KeyEvent& ev) {
    if (ev.keycode < kEvdevToX11 || ev.keycode - kEvdevToX11 > KEY_MAX) return;
    const uint16_t code = static_cast<uint16_t>(ev.keycode - kEvdevToX11);
    sendVirtualKey(code, ev.repeat ? 2 : (ev.pressed ? 1 : 0));
}

void DriverBackend::handleKey(const EvdevKeyEvent& raw) {
    if (raw.x11Keycode < kEvdevToX11 ||
        raw.x11Keycode - kEvdevToX11 > KEY_MAX) return;

    KeyEvent ev;
    ev.keycode = raw.x11Keycode;
    ev.pressed = raw.pressed;
    ev.repeat = raw.repeat;
    ev.shift = raw.shift;
    ev.capsLock = raw.capsLock;
    ev.ctrl = raw.ctrl;
    ev.alt = raw.alt;
    ev.super = raw.super;
    _capsLock = raw.capsLock;

    const uint16_t physical = static_cast<uint16_t>(ev.keycode - kEvdevToX11);
    const KeyVerdict verdict = _handler ? _handler(ev) : KeyVerdict::Forward;

    if (!ev.pressed) {
        const bool wasSwallowed = _swallowedPhysicalKeys.erase(physical) > 0;
        const bool wasTapped = _tappedPhysicalKeys.erase(physical) > 0;
        if (isModifier(physical) || (!wasSwallowed && !wasTapped)) {
            forwardKey(ev);
        }
    } else if (verdict == KeyVerdict::Forward || isModifier(physical)) {
        if (isModifier(physical)) {
            forwardKey(ev);
        } else {
            // Phim thuong duoc tap tron ven ngay khi kernel bao press. Neu giu
            // phim, tung EV_KEY value=2 cua ban phim that lai thanh mot tap.
            // Nhu vay compositor khong con mot key-down mo de tu sinh them mot
            // luong repeat song song voi repeat ma driver dang chuyen tiep.
            tapVirtualKey(physical);
            if (!ev.repeat) _tappedPhysicalKeys.insert(physical);
        }
    } else if (!ev.repeat) {
        _swallowedPhysicalKeys.insert(physical);
    }

}

std::vector<uint16_t> DriverBackend::releaseHeldModifiers() {
    std::vector<uint16_t> released;
    for (uint16_t code : _virtualKeysDown) {
        if (isHeldModifier(code)) released.push_back(code);
    }
    for (uint16_t code : released) sendVirtualKey(code, 0);
    return released;
}

void DriverBackend::restoreHeldModifiers(const std::vector<uint16_t>& released) {
    for (uint16_t code : released) sendVirtualKey(code, 1);
}

void DriverBackend::typeCodePoint(char32_t codePoint) {
    const DriverKeyStroke stroke = driverKeyStrokeFor(codePoint);
    if (stroke.evdevCode == 0) {
        DRIVER_LOG("khong co keycode cho U+%04X",
                   static_cast<unsigned>(codePoint));
        return;
    }

    if (stroke.shift) sendVirtualKey(KEY_LEFTSHIFT, 1);
    if (stroke.level3) sendVirtualKey(driverLevel3Keycode(), 1);
    if (stroke.level5) sendVirtualKey(driverLevel5Keycode(), 1);
    tapVirtualKey(stroke.evdevCode);
    if (stroke.level5) sendVirtualKey(driverLevel5Keycode(), 0);
    if (stroke.level3) sendVirtualKey(driverLevel3Keycode(), 0);
    if (stroke.shift) sendVirtualKey(KEY_LEFTSHIFT, 0);
}

void DriverBackend::sendResult(const DeleteRequest& del,
                               const std::u32string& out) {
    DRIVER_LOG("sendResult: xoa %u phim, chen \"%s\"", del.keyPresses,
               utf8Encode(out).c_str());

    // Ket qua core da mang dung hoa/thuong. Trung hoa toan bo modifier va Caps
    // trong vai su kien nay de XKB khong bien doi ket qua them lan nua, sau do
    // tra lai dung trang thai ma nguoi dung dang giu.
    _uinput.beginBatch();
    const std::vector<uint16_t> released = releaseHeldModifiers();
    if (_capsLock) tapVirtualKey(KEY_CAPSLOCK);

    for (uint32_t i = 0; i < del.keyPresses; ++i) {
        tapVirtualKey(KEY_BACKSPACE);
    }
    for (char32_t cp : out) typeCodePoint(cp);

    if (_capsLock) tapVirtualKey(KEY_CAPSLOCK);
    restoreHeldModifiers(released);
    if (!_uinput.endBatch()) {
        _error = "không ghi được gói sự kiện vào /dev/uinput";
    }
}

} // namespace

std::unique_ptr<IBackend> makeDriverBackend(std::string& error) {
    if (!driverXkbLayoutIsActive(error)) return nullptr;
    if (access("/dev/uinput", W_OK) != 0) {
        error = "không có quyền ghi /dev/uinput; hãy cài udev rule của H-OpenKey";
        return nullptr;
    }
    return std::make_unique<DriverBackend>();
}

} // namespace openkey
