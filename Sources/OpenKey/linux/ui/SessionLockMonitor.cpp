#include "SessionLockMonitor.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>

#include <unistd.h>

namespace openkey {
namespace {

constexpr char kLoginService[] = "org.freedesktop.login1";
constexpr char kLoginManager[] = "/org/freedesktop/login1";
constexpr char kSessionInterface[] = "org.freedesktop.login1.Session";

} // namespace

SessionLockMonitor::SessionLockMonitor(QObject* parent) : QObject(parent) {}

void SessionLockMonitor::start() {
    QDBusInterface manager(kLoginService, kLoginManager,
                           "org.freedesktop.login1.Manager",
                           QDBusConnection::systemBus());
    const QDBusReply<QDBusObjectPath> session =
        manager.call("GetSessionByPID", static_cast<quint32>(getpid()));
    if (session.isValid()) {
        _sessionPath = session.value().path();
        bool locked = false;
        if (readLogindLocked(locked)) {
            const bool connected = QDBusConnection::systemBus().connect(
                kLoginService, _sessionPath, "org.freedesktop.DBus.Properties",
                "PropertiesChanged", this,
                SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
            if (connected) {
                publish(locked);
                return;
            }
        }
    }

    // GNOME fallback cho may khong cho user service doc login1.
    QDBusConnection::sessionBus().connect(
        "org.gnome.ScreenSaver", "/org/gnome/ScreenSaver",
        "org.gnome.ScreenSaver", "ActiveChanged", this,
        SLOT(onGnomeActiveChanged(bool)));
    QDBusInterface screenSaver("org.gnome.ScreenSaver",
                               "/org/gnome/ScreenSaver",
                               "org.gnome.ScreenSaver",
                               QDBusConnection::sessionBus());
    const QDBusReply<bool> active = screenSaver.call("GetActive");
    if (active.isValid()) publish(active.value());
}

bool SessionLockMonitor::readLogindLocked(bool& locked) const {
    if (_sessionPath.isEmpty()) return false;
    QDBusInterface properties(kLoginService, _sessionPath,
                              "org.freedesktop.DBus.Properties",
                              QDBusConnection::systemBus());
    const QDBusReply<QDBusVariant> value =
        properties.call("Get", kSessionInterface, "LockedHint");
    if (!value.isValid()) return false;
    locked = value.value().variant().toBool();
    return true;
}

void SessionLockMonitor::onPropertiesChanged(const QString& interface,
                                              const QVariantMap& changed,
                                              const QStringList& invalidated) {
    if (interface != kSessionInterface) return;
    const auto found = changed.constFind("LockedHint");
    if (found != changed.constEnd()) {
        publish(found->toBool());
    } else if (invalidated.contains("LockedHint")) {
        bool locked = false;
        if (readLogindLocked(locked)) publish(locked);
    }
}

void SessionLockMonitor::onGnomeActiveChanged(bool active) { publish(active); }

void SessionLockMonitor::publish(bool locked) {
    if (_known && _locked == locked) return;
    _known = true;
    _locked = locked;
    emit lockedChanged(locked);
}

} // namespace openkey
