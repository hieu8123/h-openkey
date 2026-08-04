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

// Có được phép gọi DeleteSurroundingText không.
//
// Xin xoá nhiều ký tự hơn số đang thực có trước con trỏ làm Mutter vấp assertion
// trong meta_wayland_text_input_focus_delete_surrounding rồi tự sát bằng SIGABRT.
// gnome-shell chết trên Wayland nghĩa là người dùng bị đăng xuất cả phiên, mất
// hết việc đang làm. Nên câu trả lời phải là "không" mỗi khi còn nghi ngờ.
bool canDeleteSurrounding(const DeleteRequest& del, bool clientSupports,
                          bool surroundingFresh, uint32_t charsBeforeCursor);

} // namespace openkey

#endif // OPENKEY_LINUX_IBUS_DELETE_PLAN_H
