//
//  IBusDeletePlan.cpp
//  OpenKey cho Linux
//

#include "IBusDeletePlan.h"

namespace openkey {

IBusDeletePlan planDelete(const DeleteRequest& del, bool clientHasSurroundingText) {
    IBusDeletePlan plan;
    if (del.keyPresses == 0) return plan;

    // DeleteSurroundingText dem theo KY TU, nen dung keyPresses. Dung utf8Bytes
    // se xoa thua moi khi chu co dau: mot chu nhu 'e' chiem 3 byte UTF-8.
    if (clientHasSurroundingText) {
        plan.useSurrounding = true;
        plan.chars = del.keyPresses;
    } else {
        plan.backspaces = del.keyPresses;
    }
    return plan;
}

} // namespace openkey
