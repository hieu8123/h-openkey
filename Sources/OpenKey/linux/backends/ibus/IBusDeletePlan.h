//
//  IBusDeletePlan.h
//  OpenKey cho Linux
//
//  Chon duong xoa cho backend IBus. Tach rieng khoi DBus de test duoc.
//

#ifndef OPENKEY_LINUX_IBUS_DELETE_PLAN_H
#define OPENKEY_LINUX_IBUS_DELETE_PLAN_H

#include <cstdint>

#include "Backend.h"

namespace openkey {

struct IBusDeletePlan {
    bool useSurrounding = false;
    uint32_t chars = 0;       // so KY TU cho DeleteSurroundingText
    uint32_t backspaces = 0;  // so lan ForwardKeyEvent(BackSpace)
};

IBusDeletePlan planDelete(const DeleteRequest& del, bool clientHasSurroundingText);

} // namespace openkey

#endif // OPENKEY_LINUX_IBUS_DELETE_PLAN_H
