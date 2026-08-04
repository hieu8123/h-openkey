//
//  IBusFactoryObject.h
//  OpenKey cho Linux
//
//  ibus-daemon gọi CreateEngine mỗi khi người dùng chuyển sang bộ gõ của ta.
//  Việc duy nhất của lớp này là đẻ ra một IBusEngineObject và trả về đường dẫn
//  của nó.
//

#ifndef OPENKEY_LINUX_IBUS_FACTORY_OBJECT_H
#define OPENKEY_LINUX_IBUS_FACTORY_OBJECT_H

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QObject>
#include <QString>

#include <functional>

namespace openkey {

class IBusEngineObject;

class IBusFactoryObject : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.IBus.Factory")

public:
    IBusFactoryObject(QDBusConnection bus, QObject* parent = nullptr);

    // Backend đặt hàm này để được báo mỗi khi có engine mới ra đời, còn nối
    // handler gõ phím vào.
    void setEngineCreatedHandler(std::function<void(IBusEngineObject*)> h) {
        _onCreated = std::move(h);
    }

public slots:  // ibus-daemon gọi tới qua DBus
    QDBusObjectPath CreateEngine(const QString& engineName);
    void Destroy();

private:
    QDBusConnection _bus;
    std::function<void(IBusEngineObject*)> _onCreated;

    // Mỗi engine cần một đường dẫn riêng, kể cả khi engine cũ đã bị huỷ.
    unsigned _nextId = 0;
};

} // namespace openkey

#endif // OPENKEY_LINUX_IBUS_FACTORY_OBJECT_H
