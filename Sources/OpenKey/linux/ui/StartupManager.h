//
//  StartupManager.h
//  H-OpenKey cho Linux
//

#ifndef OPENKEY_LINUX_STARTUP_MANAGER_H
#define OPENKEY_LINUX_STARTUP_MANAGER_H

#include <QString>
#include <QStringList>

namespace openkey {

// Trạng thái autostart thật của phiên, không lưu lặp lại trong config.json.
bool isOpenKeyAutoStartEnabled();
bool setOpenKeyAutoStartEnabled(bool enabled, QString& error);

// Tìm cả tiến trình đang chạy lẫn cấu hình autostart của các bộ gõ cạnh tranh.
// ibus-daemon của desktop không cần bị giết: H-OpenKey không đăng ký engine vào
// đó và nguồn XKB custom chỉ là layout cho bàn phím uinput.
QStringList conflictingInputMethods();

// Chỉ gọi sau khi người dùng đã đồng ý trong hộp thoại. Dừng tiến trình và vô
// hiệu hoá autostart của các bộ gõ đã nêu; trả false nếu vẫn còn xung đột.
bool disableInputMethods(const QStringList& names, QString& error);

} // namespace openkey

#endif // OPENKEY_LINUX_STARTUP_MANAGER_H
