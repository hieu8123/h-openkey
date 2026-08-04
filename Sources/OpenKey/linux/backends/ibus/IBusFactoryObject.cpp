//
//  IBusFactoryObject.cpp
//  OpenKey cho Linux
//

#include "IBusFactoryObject.h"

#include "IBusEngineObject.h"

namespace openkey {

IBusFactoryObject::IBusFactoryObject(QDBusConnection bus, QObject* parent)
    : QObject(parent), _bus(std::move(bus)) {
    _bus.registerObject("/org/freedesktop/IBus/Factory", this,
                        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
}

QDBusObjectPath IBusFactoryObject::CreateEngine(const QString& engineName) {
    const QString path =
        QString("/org/freedesktop/IBus/Engine/%1/%2").arg(engineName).arg(_nextId++);

    // Cha là factory nên engine tự chết theo backend, không phải tự dọn tay.
    auto* engine = new IBusEngineObject(_bus, path, this);
    if (_onCreated) _onCreated(engine);
    return QDBusObjectPath(path);
}

void IBusFactoryObject::Destroy() {}

} // namespace openkey
