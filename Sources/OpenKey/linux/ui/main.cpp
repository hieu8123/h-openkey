//
//  main.cpp
//  OpenKey cho Linux
//

#include <QApplication>
#include <QMessageBox>
#include <QSocketNotifier>
#include <QSystemTrayIcon>
#include <QWindow>

#include <QDir>
#include <QFile>

#include <cstdio>

#include "Backend.h"
#include "Config.h"
#include "OpenKeyCore.h"
#include "MainWindow.h"
#include "TrayIcon.h"

namespace {

// cosmic-comp khong gui su kien `unavailable` khi mot bo go khac da giu cho:
// no chi lang le khong bao gio kich hoat chung ta. Trieu chung la OpenKey chay
// binh thuong nhung khong go duoc chu nao, rat kho doan. Nen phai tu kiem tra.
QString conflictingInputMethod() {
    static const QStringList known = {"fcitx5", "fcitx", "ibus-daemon"};
    const QStringList entries =
        QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString& entry : entries) {
        bool isPid = false;
        entry.toInt(&isPid);
        if (!isPid) continue;

        QFile comm("/proc/" + entry + "/comm");
        if (!comm.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        const QString name = QString::fromUtf8(comm.readLine()).trimmed();
        if (known.contains(name)) {
            return name;
        }
    }
    return {};
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("OpenKey");
    QApplication::setOrganizationName("OpenKey");
    // Dong cua so cuoi cung khong duoc thoat ung dung: OpenKey song o khay.
    QApplication::setQuitOnLastWindowClosed(false);

    openkey::Config config;
    config.load();
    config.loadMacroTable();
    config.loadSmartSwitchTable();

    std::string error;
    auto backend = openkey::createBackend(config.backend, error);
    if (!backend) {
        std::fprintf(stderr, "OpenKey: %s\n", error.c_str());
        QMessageBox::critical(nullptr, "OpenKey",
                              QString::fromStdString("Không khởi động được:\n\n" + error));
        return 1;
    }

    if (!backend->start()) {
        const std::string reason = backend->lastError();
        std::fprintf(stderr, "OpenKey: %s\n", reason.c_str());
        QMessageBox::critical(nullptr, "OpenKey",
                              QString::fromStdString("Không khởi động được:\n\n" + reason));
        return 1;
    }

    openkey::OpenKeyCore core(*backend);
    core.attach();

    std::printf("OpenKey: dùng backend %s\n", backend->name());

    if (const QString other = conflictingInputMethod(); !other.isEmpty()) {
        const QString message =
            QString("Đang có bộ gõ khác chạy: %1.\n\n"
                    "Chỉ một bộ gõ được giữ input method của phiên Wayland. "
                    "OpenKey sẽ khởi động nhưng không gõ được chữ nào cho tới khi "
                    "bạn tắt hẳn %1 rồi mở lại OpenKey.")
                .arg(other);
        std::fprintf(stderr, "OpenKey: %s\n", message.toUtf8().constData());
        QMessageBox::warning(nullptr, "OpenKey", message);
    }

    // Gan file descriptor cua backend vao vong lap su kien cua Qt. Nho vay chi
    // can mot tien trinh, khong phai tach daemon rieng.
    QSocketNotifier notifier(backend->eventFd(), QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, &app,
                     [&backend] { backend->dispatchEvents(); });

    openkey::TrayIcon tray(config, core);

    // Smart Switch Key co the tu doi ngon ngu khi chuyen ung dung; bieu tuong
    // khay phai phan anh dieu do, neu khong nguoi dung khong biet dang o che do nao.
    core.onStateChanged = [&tray] { tray.refresh(); };

    openkey::MainWindow window(config, core);
    window.resize(680, 520);

    QObject::connect(&tray, &openkey::TrayIcon::controlPanelRequested, &app, [&] {
        window.refreshFromState();
        window.show();
        window.raise();
        window.activateWindow();
    });

    // Khi cua so cua chinh OpenKey dang nhan focus thi ngung xu ly phim, neu
    // khong go vao chinh no se tao vong lap phan hoi.
    QObject::connect(&app, &QApplication::focusWindowChanged, &app,
                     [&](QWindow* focused) {
                         core.setSuspended(focused != nullptr &&
                                           focused == window.windowHandle());
                     });
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray.show();
    } else {
        std::fprintf(stderr,
                     "OpenKey: khong co khay he thong, van chay nhung khong co bieu tuong\n");
    }

    backend->flush();
    const int result = app.exec();

    config.save();
    config.saveMacroTable();
    config.saveSmartSwitchTable();
    backend->stop();
    return result;
}
