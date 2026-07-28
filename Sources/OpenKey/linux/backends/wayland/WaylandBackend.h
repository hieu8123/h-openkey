//
//  WaylandBackend.h
//  OpenKey cho Linux
//
//  OpenKey tu lam input method bang zwp_input_method_v2, khong di qua ibus hay
//  fcitx5. grab_keyboard cho phep bat phim truoc khi ung dung thay, tuong duong
//  CGEventTap tren macOS va WH_KEYBOARD_LL tren Windows.
//

#ifndef OPENKEY_LINUX_WAYLANDBACKEND_H
#define OPENKEY_LINUX_WAYLANDBACKEND_H

#include <memory>
#include <string>

#include "Backend.h"

namespace openkey {

// Tra ve nullptr kem ly do trong `error` neu khong ket noi hoac khong bind duoc.
std::unique_ptr<IBackend> makeWaylandBackend(std::string& error);

} // namespace openkey

#endif // OPENKEY_LINUX_WAYLANDBACKEND_H
