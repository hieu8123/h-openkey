//
//  IBusKeyTranslate.h
//  OpenKey cho Linux
//
//  Phần thuần của backend IBus: đổi tham số của ProcessKeyEvent sang KeyEvent.
//  Tách riêng để test được mà không cần ibus-daemon.
//

#ifndef OPENKEY_LINUX_IBUS_KEY_TRANSLATE_H
#define OPENKEY_LINUX_IBUS_KEY_TRANSLATE_H

#include <cstdint>

#include "Backend.h"

namespace openkey {

KeyEvent keyEventFromIBus(uint32_t keyval, uint32_t keycode, uint32_t state);

} // namespace openkey

#endif // OPENKEY_LINUX_IBUS_KEY_TRANSLATE_H
