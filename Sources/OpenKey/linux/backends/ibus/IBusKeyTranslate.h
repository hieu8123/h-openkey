//
//  IBusKeyTranslate.h
//  OpenKey cho Linux
//
//  Phan thuan cua backend IBus: doi tham so cua ProcessKeyEvent sang KeyEvent.
//  Tach rieng de test duoc ma khong can ibus-daemon.
//

#ifndef OPENKEY_LINUX_IBUS_KEY_TRANSLATE_H
#define OPENKEY_LINUX_IBUS_KEY_TRANSLATE_H

#include <cstdint>

#include "Backend.h"

namespace openkey {

KeyEvent keyEventFromIBus(uint32_t keyval, uint32_t keycode, uint32_t state);

} // namespace openkey

#endif // OPENKEY_LINUX_IBUS_KEY_TRANSLATE_H
