//
//  StartupManager.cpp
//  H-OpenKey cho Linux
//

#include "StartupManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

namespace openkey {
namespace {

constexpr const char* kOpenKeyUnit = "h-openkey.service";
constexpr const char* kOpenKeyDesktop = "h-openkey.desktop";

QString userConfigDir() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
}

bool desktopEntryHidden(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.compare("Hidden=true", Qt::CaseInsensitive) == 0 ||
            line.compare("X-GNOME-Autostart-enabled=false", Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool runSystemctl(const QStringList& args) {
    return QProcess::execute("systemctl", QStringList{"--user"} + args) == 0;
}

QString installedServiceFile() {
    const QDir binDir(QCoreApplication::applicationDirPath());
    const QString prefix = QFileInfo(binDir.filePath("..")).canonicalFilePath();
    const QStringList candidates = {
        prefix + "/lib/systemd/user/" + kOpenKeyUnit,
        prefix + "/share/systemd/user/" + kOpenKeyUnit,
        "/usr/local/lib/systemd/user/" + QString(kOpenKeyUnit),
        "/usr/lib/systemd/user/" + QString(kOpenKeyUnit),
    };
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) return QFileInfo(path).canonicalFilePath();
    }
    return {};
}

bool enableSystemdAutoStart() {
    // Nếu installer đã đặt unit vào đường tìm kiếm chuẩn thì dùng luôn.
    if (!runSystemctl({"cat", kOpenKeyUnit})) {
        const QString source = installedServiceFile();
        if (source.isEmpty()) return false;
        const QString unitDir = userConfigDir() + "/systemd/user";
        if (!QDir().mkpath(unitDir)) return false;
        const QString link = unitDir + "/" + kOpenKeyUnit;
        if (!QFileInfo::exists(link) && !QFile::link(source, link)) return false;
        if (!runSystemctl({"daemon-reload"})) return false;
    }
    // Không dùng --now: tiến trình hiện tại đã chạy rồi; start thêm một bản chỉ
    // tạo cuộc đua với khoá single-instance.
    return runSystemctl({"enable", kOpenKeyUnit});
}

bool writeDesktopAutoStart(QString& error) {
    const QString dir = userConfigDir() + "/autostart";
    if (!QDir().mkpath(dir)) {
        error = "không tạo được thư mục " + dir;
        return false;
    }
    QSaveFile file(dir + "/" + kOpenKeyDesktop);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error = "không ghi được tệp tự khởi động " + file.fileName();
        return false;
    }
    QTextStream out(&file);
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=H-OpenKey\n"
        << "Exec=h-openkey\n"
        << "Icon=h-openkey\n"
        << "Terminal=false\n"
        << "X-GNOME-Autostart-enabled=true\n";
    if (!file.commit()) {
        error = "không hoàn tất được tệp tự khởi động " + file.fileName();
        return false;
    }
    return true;
}

} // namespace

bool isOpenKeyAutoStartEnabled() {
    if (runSystemctl({"is-enabled", "--quiet", kOpenKeyUnit})) return true;
    const QString path = userConfigDir() + "/autostart/" + kOpenKeyDesktop;
    return QFileInfo::exists(path) && !desktopEntryHidden(path);
}

bool setOpenKeyAutoStartEnabled(bool enabled, QString& error) {
    error.clear();
    const QString desktop = userConfigDir() + "/autostart/" + kOpenKeyDesktop;
    if (!enabled) {
        // Không --now để công tắc chỉ điều khiển lần đăng nhập sau.
        runSystemctl({"disable", kOpenKeyUnit});
        if (QFileInfo::exists(desktop) && !QFile::remove(desktop)) {
            error = "không xoá được " + desktop;
            return false;
        }
        return true;
    }

    if (enableSystemdAutoStart()) return true;
    return writeDesktopAutoStart(error);
}

} // namespace openkey
