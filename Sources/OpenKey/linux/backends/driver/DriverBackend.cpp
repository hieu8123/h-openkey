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
constexpr auto kDriverHangTimeout = std::chrono::seconds(2);

int64_t steadyNowNanoseconds() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

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
    void setSecureInput(bool secure) override;
    bool watchdogHealthy() const override;
    std::string runtimeWarning() const override;

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
    // 0 khi epoll dang ngu. Khac 0 khi da co event va duong dispatch chua tra
    // ve; moi event cap nhat lai moc nay. Watchdog chi doc atomic nay.
    std::atomic<int64_t> _dispatchHeartbeatNs{0};
    std::atomic<bool> _outputFailed{false};
    std::atomic<bool> _secureInput{false};
    std::atomic<bool> _resetRequested{false};
    std::atomic<size_t> _pendingKeyboardCount{0};
    std::thread _worker;
    BackendCaps _caps;
    std::string _error;
};

bool DriverBackend::start() {
    if (!driverXkbLayoutIsInstalled(_error)) {
        _error = "layout H-OpenKey chưa được cài đúng: " + _error;
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

    _evdev.onPendingKeyboardCountChanged = [this](size_t count) {
        _pendingKeyboardCount.store(count, std::memory_order_release);
        if (_runtimeStatusHandler) _runtimeStatusHandler();
    };
    if (!_evdev.start(_error, driverReservedKeycodes(),
                      kOpenKeyVirtualKeyboardName)) {
        _uinput.stop();
        return false;
    }
    _evdev.onKey = [this](const EvdevKeyEvent& ev) { handleKey(ev); };
    _evdev.onContextBreak = [this] {
        if (_contextBreakHandler) _contextBreakHandler();
    };
    _evdev.onDispatchState = [this](bool active) {
        _dispatchHeartbeatNs.store(active ? steadyNowNanoseconds() : 0,
                                   std::memory_order_release);
    };
    _outputFailed.store(false, std::memory_order_release);

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

bool DriverBackend::watchdogHealthy() const {
    // Watchdog chi duoc khoi dong sau activate(). Neu khong tao duoc worker thi
    // grab da mo nhung khong co ai bom event; phai fail-fast ngay.
    if (!_running.load(std::memory_order_acquire)) return false;
    if (_outputFailed.load(std::memory_order_acquire)) return false;
    const int64_t heartbeat =
        _dispatchHeartbeatNs.load(std::memory_order_acquire);
    if (heartbeat == 0) return true; // epoll_wait dang ngu, khong phai hang
    return steadyNowNanoseconds() - heartbeat <=
           std::chrono::duration_cast<std::chrono::nanoseconds>(
               kDriverHangTimeout)
               .count();
}

std::string DriverBackend::runtimeWarning() const {
    const size_t count =
        _pendingKeyboardCount.load(std::memory_order_acquire);
    if (count == 0) return {};
    return std::to_string(count) +
           " bàn phím chưa được H-OpenKey nhận vì vẫn báo có phím đang giữ. "
           "Hãy nhả hết phím; nếu cảnh báo không mất, hãy rút thiết bị HID "
           "bị lỗi hoặc khởi động lại service.";
}

void DriverBackend::setSecureInput(bool secure) {
    _secureInput.store(secure, std::memory_order_release);
    // Core sống ở worker; chỉ yêu cầu reset tại event kế tiếp để không gọi nó
    // đồng thời từ luồng DBus/Qt.
    _resetRequested.store(true, std::memory_order_release);
}

void DriverBackend::sendVirtualKey(uint16_t code, int32_t value) {
    if (value == 1) {
        _virtualKeysDown.insert(code);
    } else if (value == 0) {
        _virtualKeysDown.erase(code);
    }
    if (!_uinput.sendKey(code, value)) {
        _error = "không ghi được sự kiện vào /dev/uinput";
        _outputFailed.store(true, std::memory_order_release);
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

    if (_resetRequested.exchange(false, std::memory_order_acq_rel) &&
        _contextBreakHandler) {
        _contextBreakHandler();
    }

    const uint16_t physical = static_cast<uint16_t>(ev.keycode - kEvdevToX11);
    const KeyVerdict verdict = _secureInput.load(std::memory_order_acquire)
                                   ? KeyVerdict::Forward
                                   : (_handler ? _handler(ev) : KeyVerdict::Forward);

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

    if (del.clearAutocomplete) typeCodePoint(0x202F);
    if (del.clearAutocomplete) tapVirtualKey(KEY_BACKSPACE);
    for (uint32_t i = 0; i < del.keyPresses; ++i) {
        tapVirtualKey(KEY_BACKSPACE);
    }
    for (char32_t cp : out) typeCodePoint(cp);

    if (_capsLock) tapVirtualKey(KEY_CAPSLOCK);
    restoreHeldModifiers(released);
    if (!_uinput.endBatch()) {
        _error = "không ghi được gói sự kiện vào /dev/uinput";
        _outputFailed.store(true, std::memory_order_release);
    }
}

} // namespace

std::unique_ptr<IBackend> makeDriverBackend(std::string& error) {
    if (!driverXkbLayoutIsInstalled(error)) return nullptr;
    if (access("/dev/uinput", W_OK) != 0) {
        error = "không có quyền ghi /dev/uinput; hãy cài udev rule của H-OpenKey";
        return nullptr;
    }
    return std::make_unique<DriverBackend>();
}

} // namespace openkey
