//
//  IBusBackend.h
//  OpenKey cho Linux
//

#ifndef OPENKEY_LINUX_IBUS_BACKEND_H
#define OPENKEY_LINUX_IBUS_BACKEND_H

#include <memory>
#include <string>

#include "Backend.h"

namespace openkey {

std::unique_ptr<IBackend> makeIBusBackend(std::string& error);

} // namespace openkey

#endif // OPENKEY_LINUX_IBUS_BACKEND_H
