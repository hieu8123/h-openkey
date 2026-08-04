//
//  IBusKeyTranslate.cpp
//  OpenKey cho Linux
//

#include "IBusKeyTranslate.h"

namespace openkey {
namespace {

// Mat na modifier cua IBus (IBusModifierType). Bit 1<<30 la IBUS_RELEASE_MASK.
constexpr uint32_t kShift = 1u << 0;
constexpr uint32_t kLock = 1u << 1;
constexpr uint32_t kControl = 1u << 2;
constexpr uint32_t kMod1 = 1u << 3;  // Alt
constexpr uint32_t kMod4 = 1u << 6;  // Super
constexpr uint32_t kRelease = 1u << 30;

// IBus gui keycode evdev, con toan bo engine dung quy uoc X11 = evdev + 8.
// Xem Backend.h:30. Do bang tools/ibus_spike.py.
constexpr uint32_t kEvdevToX11 = 8;

} // namespace

KeyEvent keyEventFromIBus(uint32_t keyval, uint32_t keycode, uint32_t state) {
    (void)keyval;  // Engine chi lam viec theo keycode, khong theo keysym.

    KeyEvent ev;
    ev.keycode = keycode + kEvdevToX11;
    ev.pressed = (state & kRelease) == 0;
    ev.shift = (state & kShift) != 0;
    ev.capsLock = (state & kLock) != 0;
    ev.ctrl = (state & kControl) != 0;
    ev.alt = (state & kMod1) != 0;
    ev.super = (state & kMod4) != 0;
    return ev;
}

} // namespace openkey
