//
//  StartupManager.h
//  H-OpenKey cho Linux
//

#ifndef OPENKEY_LINUX_STARTUP_MANAGER_H
#define OPENKEY_LINUX_STARTUP_MANAGER_H

#include <QString>

namespace openkey {

// Trạng thái autostart thật của phiên, không lưu lặp lại trong config.json.
bool isOpenKeyAutoStartEnabled();
bool setOpenKeyAutoStartEnabled(bool enabled, QString& error);

} // namespace openkey

#endif // OPENKEY_LINUX_STARTUP_MANAGER_H
