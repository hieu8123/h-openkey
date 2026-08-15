//
//  UInputKeyboard.h
//  Ban phim ao cap kernel cua H-OpenKey.
//

#ifndef OPENKEY_LINUX_UINPUT_KEYBOARD_H
#define OPENKEY_LINUX_UINPUT_KEYBOARD_H

#include <linux/input.h>

#include <cstdint>
#include <string>
#include <vector>

namespace openkey {

inline constexpr const char* kOpenKeyVirtualKeyboardName =
    "H-OpenKey Virtual Keyboard";

class UInputKeyboard {
public:
    ~UInputKeyboard();

    bool start(std::string& error);
    void stop();
    bool active() const { return _fd >= 0; }

    // value dung quy uoc evdev: 0=nha, 1=bam, 2=lap.
    bool sendKey(uint16_t code, int32_t value);
    bool tap(uint16_t code);

    // Gom mot phep sua thanh mot write() nhung van giu SYN_REPORT sau moi
    // canh phim. Khong co timer: endBatch() ghi ngay trong cung lan xu ly phim.
    void beginBatch();
    bool endBatch();

private:
    bool emit(uint16_t type, uint16_t code, int32_t value);
    bool writePending();

    int _fd = -1;
    bool _batching = false;
    std::vector<struct input_event> _pending;
};

} // namespace openkey

#endif // OPENKEY_LINUX_UINPUT_KEYBOARD_H
