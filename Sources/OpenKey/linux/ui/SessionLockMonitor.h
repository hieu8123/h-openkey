#ifndef OPENKEY_LINUX_SESSION_LOCK_MONITOR_H
#define OPENKEY_LINUX_SESSION_LOCK_MONITOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace openkey {

class SessionLockMonitor : public QObject {
    Q_OBJECT

public:
    explicit SessionLockMonitor(QObject* parent = nullptr);
    void start();

signals:
    void lockedChanged(bool locked);

private slots:
    void onPropertiesChanged(const QString& interface,
                             const QVariantMap& changed,
                             const QStringList& invalidated);
    void onGnomeActiveChanged(bool active);

private:
    bool readLogindLocked(bool& locked) const;
    void publish(bool locked);

    QString _sessionPath;
    bool _known = false;
    bool _locked = false;
};

} // namespace openkey

#endif // OPENKEY_LINUX_SESSION_LOCK_MONITOR_H
