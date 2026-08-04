//
//  IBusDeletePlan.h
//  OpenKey cho Linux
//
//  Chọn đường xoá cho backend IBus. Tách riêng khỏi DBus để test được.
//

#ifndef OPENKEY_LINUX_IBUS_DELETE_PLAN_H
#define OPENKEY_LINUX_IBUS_DELETE_PLAN_H

#include <cstdint>

#include "Backend.h"

namespace openkey {

struct IBusDeletePlan {
    bool useSurrounding = false;
    uint32_t chars = 0;       // số KÝ TỰ cho DeleteSurroundingText
    uint32_t backspaces = 0;  // số lần ForwardKeyEvent(BackSpace)
};

IBusDeletePlan planDelete(const DeleteRequest& del, bool clientHasSurroundingText);

} // namespace openkey

#endif // OPENKEY_LINUX_IBUS_DELETE_PLAN_H
