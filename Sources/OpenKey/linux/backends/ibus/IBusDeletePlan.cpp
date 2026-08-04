//
//  IBusDeletePlan.cpp
//  OpenKey cho Linux
//

#include "IBusDeletePlan.h"

namespace openkey {

IBusDeletePlan planDelete(const DeleteRequest& del, bool clientHasSurroundingText) {
    IBusDeletePlan plan;
    if (del.keyPresses == 0) return plan;

    // DeleteSurroundingText đếm theo KÝ TỰ, nên dùng keyPresses. Dùng utf8Bytes
    // sẽ xoá thừa mỗi khi chữ có dấu: một chữ như 'ế' chiếm 3 byte UTF-8.
    if (clientHasSurroundingText) {
        plan.useSurrounding = true;
        plan.chars = del.keyPresses;
    } else {
        plan.backspaces = del.keyPresses;
    }
    return plan;
}

} // namespace openkey
