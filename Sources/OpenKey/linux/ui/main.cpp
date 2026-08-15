//
//  main.cpp
//  OpenKey cho Linux
//

#include <QApplication>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QProcess>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QWindow>

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <unistd.h>

#include "AppState.h"
#include "Backend.h"
#include "Config.h"
#include "DriverKeymap.h"
#include "OpenKeyCore.h"
#include "ProcessWatchdog.h"
#include "SessionLockMonitor.h"
#include "MainWindow.h"
#include "SingleInstance.h"
#include "StartupManager.h"
#include "Theme.h"
#include "TrayIcon.h"

namespace {

// SIGTERM/SIGINT (systemctl stop, kill thuong) khong tu goi app.exec() thoat,
// nen backend->stop() phia duoi main() khong duoc chay. Bat tin hieu bang
// self-pipe roi thoat qua duong Qt binh thuong de driver nha EVIOCGRAB va huy
// ban phim uinput mot cach sach se.
int g_signalPipe[2] = {-1, -1};

void handleTermSignal(int) {
    const char byte = 0;
    ssize_t ignored = write(g_signalPipe[1], &byte, 1);
    (void)ignored;
}

void selectAvailableQtPlatform() {
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) return;

    const QString platforms = QLibraryInfo::path(QLibraryInfo::PluginsPath) +
                              "/platforms/";
    const bool hasWayland =
        QFileInfo::exists(platforms + "libqwayland-egl.so") ||
        QFileInfo::exists(platforms + "libqwayland-generic.so");
    if (!hasWayland && QFileInfo::exists(platforms + "libqxcb.so")) {
        // Chi chon cach ve giao dien Qt. Driver ban phim van la evdev/uinput va
        // khong thay doi theo QPA platform.
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
}

bool runBoundedCommand(const QString& name, const QStringList& arguments,
                       QByteArray* output, QString& error) {
    const QString executable = QStandardPaths::findExecutable(name);
    if (executable.isEmpty()) {
        error = QString("không tìm thấy lệnh %1").arg(name);
        return false;
    }
    QProcess process;
    process.start(executable, arguments, QIODevice::ReadOnly);
    if (!process.waitForStarted(1000) || !process.waitForFinished(2000)) {
        process.kill();
        process.waitForFinished(500);
        error = QString("lệnh %1 không phản hồi").arg(name);
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (error.isEmpty()) error = QString("lệnh %1 thất bại").arg(name);
        return false;
    }
    if (output) *output = process.readAllStandardOutput();
    return true;
}

bool usesGnomeInputSources() {
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP") + ":" +
                            qEnvironmentVariable("XDG_SESSION_DESKTOP");
    return desktop.contains("gnome", Qt::CaseInsensitive) ||
           desktop.contains("ubuntu", Qt::CaseInsensitive) ||
           desktop.contains("zorin", Qt::CaseInsensitive) ||
           desktop.contains("pop", Qt::CaseInsensitive);
}

bool activateDriverInputSource(QString& error) {
    static const QString schema = "org.gnome.desktop.input-sources";
    QByteArray sources;
    if (!runBoundedCommand("gsettings", {"get", schema, "sources"},
                           &sources, error)) {
        return false;
    }
    size_t index = 0;
    if (!openkey::findDriverSourceIndex(sources.toStdString(), index)) {
        error = "không có H-OpenKey Layout (xkb:custom) trong nguồn nhập GNOME; "
                "hãy chạy lại trình cài đặt";
        return false;
    }
    if (!runBoundedCommand("gsettings",
                           {"set", schema, "current",
                            QString("uint32 %1").arg(index)},
                           nullptr, error)) {
        return false;
    }

    // GNOME cap nhat Wayland qua GSettings. Dong bo them XWayland, nhung loi
    // setxkbmap khong duoc lam that bai nguon Wayland vua chuyen thanh cong.
    if (qEnvironmentVariableIsSet("DISPLAY")) {
        QString ignored;
        runBoundedCommand("setxkbmap", {"-layout", "custom"}, nullptr,
                          ignored);
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    // Lenh cai dat chay khong giao dien, truoc ca QApplication va khoa mot-ban.
    // Installer goi duong nay de layout va ma trong binary luon cung mot phien ban.
    const bool installDriverLayout =
        argc == 2 && std::strcmp(argv[1], "--install-driver-layout") == 0;
    const bool configureDriver =
        argc == 2 && std::strcmp(argv[1], "--configure-driver") == 0;
    if (installDriverLayout || configureDriver) {
        std::string error;
        if (!openkey::installDriverXkbLayout(error)) {
            std::fprintf(stderr, "H-OpenKey: %s\n", error.c_str());
            return 1;
        }
        if (installDriverLayout) {
            std::printf("Đã sinh symbols XKB H-OpenKey.\n");
            return 0;
        }
        openkey::Config config;
        config.load();
        config.backend = openkey::BackendKind::Driver;
        if (!config.save()) {
            std::fprintf(stderr, "H-OpenKey: không lưu được cấu hình driver\n");
            return 1;
        }
        std::string activationError;
        if (!openkey::driverXkbLayoutIsInstalled(activationError)) {
            std::printf("Đã cài layout XKB và chọn driver trực tiếp. %s.\n",
                        activationError.c_str());
        } else {
            std::printf("Đã cài layout XKB và chọn driver trực tiếp.\n");
        }
        return 0;
    }

    selectAvailableQtPlatform();
    QApplication app(argc, argv);
    QApplication::setApplicationName("H-OpenKey");
    QApplication::setApplicationDisplayName("H-OpenKey");
    QApplication::setOrganizationName("OpenKey");
    QApplication::setDesktopFileName("h-openkey");
    openkey::applyTheme(app);
    // Dong cua so cuoi cung khong duoc thoat ung dung: OpenKey song o khay.
    QApplication::setQuitOnLastWindowClosed(false);

    // Chan ban thu hai TRUOC KHI lam bat cu viec gi khac. Chay hai ban cung luc
    // la loi nang: ca hai deu bat phim va deu gui chu, nen chu bi nhan doi.
    openkey::SingleInstance instance;
    if (instance.notifyRunningInstance()) {
        std::printf("OpenKey đã chạy rồi, đang mở bảng điều khiển của bản đó.\n");
        return 0;
    }
    if (!instance.claim()) {
        std::fprintf(stderr,
                     "OpenKey: một bản khác đang khởi động; từ chối chạy bản "
                     "thứ hai để không grab bàn phím hai lần\n");
        instance.notifyRunningInstance();
        return 0;
    }

    openkey::Config config;
    config.load();
    config.loadMacroTable();
    config.loadSmartSwitchTable();

    QString sourceActivationError;
    const bool manageGnomeSource = usesGnomeInputSources();
    if (manageGnomeSource && vLanguage == 1 &&
        !activateDriverInputSource(sourceActivationError)) {
        // Khong cho core phat carrier Unicode trong khi GNOME van dung keymap
        // Mozc/IBus/us. Che do Anh an toan hon viec nuot ky tu khong thong bao.
        vLanguage = 0;
    }

    std::string notice;
    auto backend = openkey::createBackend(config.backend, notice);

    // Driver khởi động hỏng thì vẫn giữ giao diện và khay để người dùng xem lỗi,
    // nhưng tuyệt đối không rơi sang một cơ chế gõ khác.
    if (!backend->start()) {
        std::string reason = backend->lastError();
        if (!notice.empty()) reason = notice + "\n" + reason;
        notice = reason;
        backend = openkey::makeNullBackend();
        backend->start();
    }

    if (!notice.empty()) {
        const QString message =
            QString("Driver trực tiếp chưa khởi động được:\n\n%1\n\n"
                    "Ứng dụng vẫn chạy để bạn mở bảng điều khiển, nhưng không bắt "
                    "bàn phím và không tự chuyển sang IBus, X11 hay input-method-v2. "
                    "Hãy kiểm tra nguồn nhập xkb:custom, quyền /dev/input và "
                    "/dev/uinput, sau đó đăng xuất rồi đăng nhập lại.")
                .arg(QString::fromStdString(notice));
        std::fprintf(stderr, "OpenKey: %s\n", message.toUtf8().constData());
        QMessageBox::warning(nullptr, "H-OpenKey", message);
    }

    openkey::OpenKeyCore core(*backend);
    core.attach();

    openkey::SessionLockMonitor lockMonitor;
    QObject::connect(&lockMonitor, &openkey::SessionLockMonitor::lockedChanged,
                     &app, [&backend](bool locked) {
                         backend->setSecureInput(locked);
                     });
    lockMonitor.start();

    std::printf("OpenKey: dùng backend %s\n", backend->name());

    QSocketNotifier* signalNotifier = nullptr;
    if (pipe(g_signalPipe) == 0) {
        std::signal(SIGTERM, handleTermSignal);
        std::signal(SIGINT, handleTermSignal);
        signalNotifier = new QSocketNotifier(g_signalPipe[0], QSocketNotifier::Read, &app);
        QObject::connect(signalNotifier, &QSocketNotifier::activated, &app,
                         [] { QApplication::quit(); });
    }

    openkey::TrayIcon tray(config, core);

    // Phim tat va lan kich hoat sau khi doi source deu co the doi ngon ngu tu
    // luong khac; bieu tuong khay phai luon phan anh trang thai moi.
    core.onStateChanged = [&tray] {
        QMetaObject::invokeMethod(&tray, [&tray] { tray.refresh(); },
                                  Qt::QueuedConnection);
    };
    if (manageGnomeSource) {
        core.onVietnameseActivationRequested = [&app, &core, &tray] {
            QMetaObject::invokeMethod(
                &app,
                [&core, &tray] {
                    QString error;
                    const bool activated = activateDriverInputSource(error);
                    core.completeVietnameseActivation(activated);
                    if (!activated) {
                        tray.showWarning(
                            QObject::tr("Không bật tiếng Việt: %1").arg(error));
                    }
                },
                Qt::QueuedConnection);
        };
    }

    auto scheduleRuntimeWarning = [&backend, &tray] {
        const QString warning =
            QString::fromStdString(backend->runtimeWarning());
        if (warning.isEmpty()) {
            tray.setRuntimeWarning({});
            return;
        }
        QTimer::singleShot(3000, &tray, [&backend, &tray] {
            tray.setRuntimeWarning(
                QString::fromStdString(backend->runtimeWarning()));
        });
    };
    backend->setRuntimeStatusHandler([&tray, scheduleRuntimeWarning] {
        QMetaObject::invokeMethod(&tray, scheduleRuntimeWarning,
                                  Qt::QueuedConnection);
    });
    scheduleRuntimeWarning();
    // Core va callback giao dien da san sang; tu day luong epoll rieng moi bat
    // dau doc phím. Qt khong con nam tren duong go.
    backend->activate();
    openkey::ProcessWatchdog watchdog(*backend);
    watchdog.start();

    openkey::MainWindow window(config, core);
    window.resize(680, 520);

    auto showPanel = [&] {
        window.refreshFromState();
        window.show();
        window.raise();
        window.activateWindow();
    };
    QObject::connect(&tray, &openkey::TrayIcon::controlPanelRequested, &app, showPanel);

    // Mo lai ung dung tu menu se hien bang dieu khien. Day la duong chac chan
    // nhat: menu chuot phai o khay he thong khong phai desktop nao cung ho tro.
    QObject::connect(&instance, &openkey::SingleInstance::showControlPanelRequested,
                     &app, showPanel);

    // Khi cua so cua chinh OpenKey dang nhan focus thi ngung xu ly phim, neu
    // khong go vao chinh no se tao vong lap phan hoi.
    QObject::connect(&app, &QApplication::focusWindowChanged, &app,
                     [&](QWindow* focused) {
                         core.setSuspended(focused != nullptr &&
                                           focused == window.windowHandle());
                     });
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        std::fprintf(stderr,
                     "OpenKey: khay hệ thống chưa sẵn sàng, sẽ đăng ký biểu tượng khi khay xuất hiện\n");
    }
    // Qt tự thêm biểu tượng khi khay xuất hiện sau thời điểm ứng dụng khởi động,
    // nhưng chỉ khi QSystemTrayIcon đã được đặt visible từ trước.
    tray.show();
    if (!sourceActivationError.isEmpty()) {
        tray.showWarning(
            QObject::tr("Đã giữ chế độ tiếng Anh để tránh mất chữ: %1")
                .arg(sourceActivationError));
    }

    // Dung khi lam giao dien: tu chup cua so ra tep roi thoat, de doi chieu
    // thiet ke ma khong can cong cu chup anh cua desktop.
    if (const char* shot = std::getenv("OPENKEY_SCREENSHOT")) {
        const QString path = QString::fromUtf8(shot);
        showPanel();
        QTimer::singleShot(900, &app, [&window, path] {
            window.grab().save(path);
            QApplication::quit();
        });
    }

    backend->flush();
    const int result = app.exec();

    // Tat watchdog truoc khi ha backend de trang thai dung binh thuong khong bi
    // hieu nham la luong dispatch da chet.
    watchdog.stop();
    backend->stop();
    config.save();
    config.saveMacroTable();
    config.saveSmartSwitchTable();
    return result;
}
