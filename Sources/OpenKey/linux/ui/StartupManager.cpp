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

bool desktopAutoStartEnabled(const QString& fileName) {
    const QString userPath = userConfigDir() + "/autostart/" + fileName;
    if (QFileInfo::exists(userPath)) return !desktopEntryHidden(userPath);

    const QStringList configDirs =
        QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    for (const QString& dir : configDirs) {
        if (dir == userConfigDir()) continue;
        const QString path = dir + "/autostart/" + fileName;
        if (QFileInfo::exists(path)) return !desktopEntryHidden(path);
    }
    return false;
}

bool processRunning(const QString& wanted) {
    const QStringList entries =
        QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : entries) {
        bool isPid = false;
        entry.toInt(&isPid);
        if (!isPid) continue;
        QFile comm("/proc/" + entry + "/comm");
        if (!comm.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        if (QString::fromUtf8(comm.readLine()).trimmed() == wanted) return true;
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

bool maskDesktopAutoStart(const QString& fileName, QString& error) {
    const QString dir = userConfigDir() + "/autostart";
    if (!QDir().mkpath(dir)) {
        error = "không tạo được thư mục " + dir;
        return false;
    }
    const QString path = dir + "/" + fileName;
    if (QFileInfo::exists(path) && desktopEntryHidden(path)) return true;
    if (QFileInfo::exists(path)) {
        const QString backup = path + ".disabled-by-h-openkey";
        if (QFileInfo::exists(backup) || !QFile::rename(path, backup)) {
            error = "không sao lưu được " + path;
            return false;
        }
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error = "không vô hiệu hoá được " + path;
        return false;
    }
    QTextStream out(&file);
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=Disabled by H-OpenKey\n"
        << "Hidden=true\n"
        << "X-H-OpenKey-Disabled=true\n";
    return file.commit();
}

QStringList inputMethodDesktopFiles(const QString& name) {
    if (name == "fcitx5") return {"org.fcitx.Fcitx5.desktop", "fcitx5.desktop"};
    if (name == "fcitx") return {"fcitx-autostart.desktop", "fcitx.desktop"};
    if (name == "nimf") return {"nimf.desktop", "org.nimf.Nimf.desktop"};
    if (name == "uim-xim") return {"uim.desktop", "uim-xim.desktop"};
    if (name == "kime") return {"kime.desktop"};
    if (name == "gcin") return {"gcin.desktop"};
    if (name == "hime") return {"hime.desktop"};
    return {};
}

bool inputMethodAutoStartEnabled(const QString& name) {
    for (const QString& fileName : inputMethodDesktopFiles(name)) {
        if (desktopAutoStartEnabled(fileName)) return true;
    }
    return false;
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

QStringList conflictingInputMethods() {
    QStringList found;
    for (const QString& name : {"fcitx5", "fcitx", "nimf", "uim-xim", "kime",
                                "gcin", "hime"}) {
        if (processRunning(name) || inputMethodAutoStartEnabled(name)) found << name;
    }
    found.removeDuplicates();
    return found;
}

bool disableInputMethods(const QStringList& names, QString& error) {
    error.clear();
    for (const QString& name : names) {
        runSystemctl({"disable", "--now", name + ".service"});
        if (name == "fcitx5") {
            runSystemctl({"stop", "app-org.fcitx.Fcitx5@autostart.service"});
        }
        for (const QString& fileName : inputMethodDesktopFiles(name)) {
            if (desktopAutoStartEnabled(fileName) &&
                !maskDesktopAutoStart(fileName, error)) {
                return false;
            }
        }
        // pkill trả 1 nếu tiến trình đã tắt; đó vẫn là kết quả mong muốn.
        QProcess::execute("pkill", {"-x", name});
    }

    QStringList stillRunning;
    for (const QString& name : names) {
        if (processRunning(name)) stillRunning << name;
    }
    if (!stillRunning.isEmpty()) {
        error = "vẫn chưa dừng được: " + stillRunning.join(", ");
        return false;
    }

    // Driver truc tiep khong can framework input method. Chi dung fcitx la chua
    // du: lan dang nhap sau GTK/QT_IM_MODULE=fcitx se kich hoat lai no.
    const QString imConfig = QStandardPaths::findExecutable("im-config");
    if (!imConfig.isEmpty() && QProcess::execute(imConfig, {"-n", "none"}) != 0) {
        error = "không tắt được cấu hình input method cũ";
        return false;
    }

    // Cập nhật môi trường cho các dịch vụ người dùng được khởi động sau thời
    // điểm này. Các ứng dụng đang mở vẫn cần được khởi động lại.
    runSystemctl({"unset-environment", "GTK_IM_MODULE", "QT_IM_MODULE",
                  "XMODIFIERS"});
    return true;
}

} // namespace openkey
