//
//  IBusConnection.h
//  OpenKey cho Linux
//
//  Đường dây tới ibus-daemon: tìm địa chỉ bus, kết nối, đăng ký component.
//  Tách riêng khỏi đối tượng Engine để mỗi thứ chỉ lo một việc.
//

#ifndef OPENKEY_LINUX_IBUS_CONNECTION_H
#define OPENKEY_LINUX_IBUS_CONNECTION_H

#include <QDBusConnection>
#include <QObject>
#include <QString>

#include <string>

namespace openkey {

class IBusConnection : public QObject {
    Q_OBJECT

public:
    explicit IBusConnection(QObject* parent = nullptr);

    // Tìm địa chỉ bus rồi kết nối. False kèm `error` đọc được nếu không xong.
    bool open(std::string& error);

    QDBusConnection& bus() { return _bus; }

    // Khai component tên "openkey" với daemon.
    bool registerComponent(std::string& error);

    // Bảo GNOME chuyển sang engine của ta ngay.
    void setGlobalEngine();

signals:
    // ibus-daemon vừa sống lại: phải đăng ký lại từ đầu.
    void daemonRestarted();

private:
    // Đọc $IBUS_ADDRESS, không có thì đọc ~/.config/ibus/bus/<id>-unix-<display>.
    static QString findAddress(std::string& error);

    // Một lần thử: tìm địa chỉ rồi kết nối. Không tự bật daemon.
    bool connectOnce(std::string& error);

    QDBusConnection _bus;
};

} // namespace openkey

#endif // OPENKEY_LINUX_IBUS_CONNECTION_H
