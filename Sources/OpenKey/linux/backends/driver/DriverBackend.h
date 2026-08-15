//
//  DriverBackend.h
//  Backend bat phim vat ly bang evdev va tra lai qua uinput.
//

#ifndef OPENKEY_LINUX_DRIVER_BACKEND_H
#define OPENKEY_LINUX_DRIVER_BACKEND_H

#include <memory>
#include <string>

#include "Backend.h"

namespace openkey {

// Driver nay khong dung input-method API. Compositor nhin thay dau ra nhu mot
// ban phim kernel that; layout XKB H-OpenKey doi cac keycode rieng thanh Unicode.
std::unique_ptr<IBackend> makeDriverBackend(std::string& error);

} // namespace openkey

#endif // OPENKEY_LINUX_DRIVER_BACKEND_H
