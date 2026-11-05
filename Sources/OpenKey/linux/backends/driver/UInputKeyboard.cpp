//
//  UInputKeyboard.cpp
//

#include "UInputKeyboard.h"

#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace openkey {

UInputKeyboard::~UInputKeyboard() { stop(); }

bool UInputKeyboard::start(std::string& error) {
    if (_fd >= 0) return true;

    _fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (_fd < 0) {
        error = "khong mo duoc /dev/uinput: " + std::string(std::strerror(errno)) +
                ". Hay cai udev rule cua H-OpenKey";
        return false;
    }

    if (ioctl(_fd, UI_SET_EVBIT, EV_KEY) < 0 ||
        ioctl(_fd, UI_SET_EVBIT, EV_SYN) < 0) {
        error = "khong khai bao duoc ban phim uinput";
        stop();
        return false;
    }
    for (int code = 0; code <= KEY_MAX; ++code) {
        // BTN_MISC..KEY_OK-1 la nut chuot, joystick va gamepad chen giua hai
        // dai KEY_*. Khai bao ca doan nay lam libinput/Mutter coi ban phim ao
        // la thiet bi lai pointer+keyboard va tao them luong xu ly vo ich.
        if (code >= BTN_MISC && code < KEY_OK) continue;
        if (ioctl(_fd, UI_SET_KEYBIT, code) < 0) {
            error = "khong khai bao duoc keycode uinput " + std::to_string(code);
            stop();
            return false;
        }
    }

    struct uinput_setup setup {};
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x484f;  // "HO"
    setup.id.product = 0x4b59; // "KY"
    setup.id.version = 1;
    std::strncpy(setup.name, kOpenKeyVirtualKeyboardName, UINPUT_MAX_NAME_SIZE - 1);

    if (ioctl(_fd, UI_DEV_SETUP, &setup) < 0 || ioctl(_fd, UI_DEV_CREATE) < 0) {
        error = "khong tao duoc ban phim ao uinput: " +
                std::string(std::strerror(errno));
        stop();
        return false;
    }
    return true;
}

void UInputKeyboard::stop() {
    if (_fd < 0) return;
    ioctl(_fd, UI_DEV_DESTROY);
    close(_fd);
    _fd = -1;
    _batching = false;
    _pending.clear();
}

bool UInputKeyboard::emit(uint16_t type, uint16_t code, int32_t value) {
    if (_fd < 0) return false;
    struct input_event event {};
    event.type = type;
    event.code = code;
    event.value = value;
    if (_batching) {
        _pending.push_back(event);
        return true;
    }
    return write(_fd, &event, sizeof(event)) == static_cast<ssize_t>(sizeof(event));
}

bool UInputKeyboard::sendKey(uint16_t code, int32_t value) {
    if (!emit(EV_KEY, code, value)) return false;
    return emit(EV_SYN, SYN_REPORT, 0);
}

bool UInputKeyboard::tap(uint16_t code) {
    return sendKey(code, 1) && sendKey(code, 0);
}

void UInputKeyboard::beginBatch() {
    _pending.clear();
    _batching = true;
}

bool UInputKeyboard::writePending() {
    // Kernel chi giu 63 su kien cho moi reader cua thiet bi (EVDEV_MIN_BUFFER_SIZE
    // = 64). Ghi ca lo mot lan thi Xorg/libinput bao SYN_DROPPED va mat dau giua
    // tu. Chia thanh dot nho, nghi ngan de reader kip rut hang doi.
    // ponytail: nghi co dinh, reader tre > ~3 dot (6 ms) van co the mat su kien.
    constexpr size_t kChunk = kMaxEventsPerWrite;
    for (size_t begin = 0; begin < _pending.size(); begin += kChunk) {
        if (begin > 0) usleep(kChunkPauseUs);
        const size_t count = std::min(kChunk, _pending.size() - begin);
        const char* data = reinterpret_cast<const char*>(&_pending[begin]);
        size_t remaining = count * sizeof(struct input_event);
        while (remaining > 0) {
            const ssize_t written = write(_fd, data, remaining);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) return false;
            data += written;
            remaining -= static_cast<size_t>(written);
        }
    }
    return true;
}

bool UInputKeyboard::endBatch() {
    if (!_batching) return true;
    _batching = false;
    const bool ok = _pending.empty() || writePending();
    _pending.clear();
    return ok;
}

} // namespace openkey
