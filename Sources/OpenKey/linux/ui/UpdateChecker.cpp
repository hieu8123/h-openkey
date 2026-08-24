//
//  UpdateChecker.cpp
//

#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "ReleaseVersion.h"

namespace openkey {
namespace {

const QUrl kLatestReleaseApi(
    QStringLiteral("https://api.github.com/repos/hieu8123/h-openkey/releases/latest"));

bool isTrustedReleaseUrl(const QUrl& url) {
    return url.scheme() == QStringLiteral("https") &&
           url.host() == QStringLiteral("github.com") &&
           url.path().startsWith(QStringLiteral("/hieu8123/h-openkey/releases/"));
}

} // namespace

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent), _network(this) {}

void UpdateChecker::checkForUpdates(bool userInitiated) {
    if (_checking) return;
    _checking = true;
    emit checkStarted();

    QNetworkRequest request(kLatestReleaseApi);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader(
        "User-Agent",
        QString("H-OpenKey/%1").arg(QCoreApplication::applicationVersion())
            .toUtf8());
    request.setTransferTimeout(6000);

    QNetworkReply* reply = _network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, userInitiated] {
        _checking = false;
        const auto finishWithError = [this, userInitiated](const QString& detail) {
            emit checkFinished(false, {},
                               tr("Không kiểm tra được bản mới: %1").arg(detail),
                               userInitiated);
        };

        if (reply->error() != QNetworkReply::NoError) {
            finishWithError(reply->errorString());
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document =
            QJsonDocument::fromJson(reply->readAll(), &parseError);
        reply->deleteLater();
        if (parseError.error != QJsonParseError::NoError ||
            !document.isObject()) {
            finishWithError(tr("phản hồi GitHub không hợp lệ"));
            return;
        }

        const QJsonObject release = document.object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        const QUrl url(release.value(QStringLiteral("html_url")).toString());
        QString version;
        const bool newer = isNewerLinuxRelease(
            QCoreApplication::applicationVersion(), tag, version);
        if (newer && isTrustedReleaseUrl(url)) {
            _availableVersion = version;
            _availableUrl = url;
            emit checkFinished(
                true, version,
                tr("Có bản H-OpenKey %1. Bấm Tải bản mới để cập nhật.")
                    .arg(version),
                userInitiated);
            return;
        }

        _availableVersion.clear();
        _availableUrl.clear();
        emit checkFinished(
            false, {},
            tr("Bạn đang dùng bản mới nhất (%1).")
                .arg(QCoreApplication::applicationVersion()),
            userInitiated);
    });
}

bool UpdateChecker::hasAvailableRelease() const {
    return !_availableVersion.isEmpty() && _availableUrl.isValid();
}

QString UpdateChecker::availableVersion() const { return _availableVersion; }

void UpdateChecker::openAvailableRelease() const {
    if (hasAvailableRelease()) QDesktopServices::openUrl(_availableUrl);
}

} // namespace openkey
