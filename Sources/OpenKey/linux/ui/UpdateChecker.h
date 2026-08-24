//
//  UpdateChecker.h
//  Kiem tra GitHub Release moi cho H-OpenKey Linux.
//

#ifndef OPENKEY_LINUX_UPDATECHECKER_H
#define OPENKEY_LINUX_UPDATECHECKER_H

#include <QObject>
#include <QString>
#include <QUrl>

#include <QNetworkAccessManager>

namespace openkey {

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);

    void checkForUpdates(bool userInitiated = false);
    bool hasAvailableRelease() const;
    QString availableVersion() const;
    void openAvailableRelease() const;

signals:
    void checkStarted();
    void checkFinished(bool available, const QString& version,
                       const QString& message, bool userInitiated);

private:
    QNetworkAccessManager _network;
    bool _checking = false;
    QString _availableVersion;
    QUrl _availableUrl;
};

} // namespace openkey

#endif // OPENKEY_LINUX_UPDATECHECKER_H
