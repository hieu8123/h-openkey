//
//  main.cpp
//  OpenKey cho Linux
//

#include <QApplication>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QSocketNotifier>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QWindow>

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <unistd.h>

#include "Backend.h"
#include "Config.h"
#include "DriverKeymap.h"
#include "OpenKeyCore.h"
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
        if (!openkey::driverXkbLayoutIsActive(activationError)) {
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
                     "OpenKey: không giành được ổ cắm một-bản-duy-nhất, "
                     "vẫn chạy nhưng không chặn được bản thứ hai\n");
    }

    openkey::Config config;
    config.load();
    config.loadMacroTable();
    config.loadSmartSwitchTable();

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

    std::printf("OpenKey: dùng backend %s\n", backend->name());

    const QStringList conflicts = openkey::conflictingInputMethods();
    if (!conflicts.isEmpty()) {
        const QString names = conflicts.join(", ");
        const auto answer = QMessageBox::question(
            nullptr, "Xung đột bộ gõ",
            QString("Đang có bộ gõ khác chạy hoặc được thiết lập khởi động cùng "
                    "phiên đăng nhập: %1.\n\n"
                    "Dừng bộ gõ đó và vô hiệu hoá cơ chế tự khởi động của nó để "
                    "tiếp tục sử dụng H-OpenKey?")
                .arg(names),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        QString error;
        if (answer != QMessageBox::Yes ||
            !openkey::disableInputMethods(conflicts, error)) {
            QString ignored;
            openkey::setOpenKeyAutoStartEnabled(false, ignored);
            const QString reason = answer == QMessageBox::Yes
                                       ? "Không tắt được bộ gõ khác: " + error
                                       : "Bạn đã chọn giữ bộ gõ hiện tại.";
            QMessageBox::information(
                nullptr, "H-OpenKey",
                reason + "\n\nH-OpenKey sẽ tắt và không khởi động cùng phiên đăng "
                         "nhập để hai bộ gõ không chạy đồng thời.");
            backend->stop();
            return 0;
        }
        QMessageBox::information(
            nullptr, "Đã tắt bộ gõ cũ",
            "Đã tắt bộ gõ gây xung đột. H-OpenKey dùng driver bàn phím trực "
            "tiếp, không dùng IBus/preedit. Hãy đăng xuất rồi đăng nhập lại "
            "để môi trường của phiên được cập nhật đầy đủ.");
    }

    QSocketNotifier* signalNotifier = nullptr;
    if (pipe(g_signalPipe) == 0) {
        std::signal(SIGTERM, handleTermSignal);
        std::signal(SIGINT, handleTermSignal);
        signalNotifier = new QSocketNotifier(g_signalPipe[0], QSocketNotifier::Read, &app);
        QObject::connect(signalNotifier, &QSocketNotifier::activated, &app,
                         [] { QApplication::quit(); });
    }

    openkey::TrayIcon tray(config, core);

    // Smart Switch Key co the tu doi ngon ngu khi chuyen ung dung; bieu tuong
    // khay phai phan anh dieu do, neu khong nguoi dung khong biet dang o che do nao.
    core.onStateChanged = [&tray] {
        QMetaObject::invokeMethod(&tray, [&tray] { tray.refresh(); },
                                  Qt::QueuedConnection);
    };
    // Core va callback giao dien da san sang; tu day luong epoll rieng moi bat
    // dau doc phím. Qt khong con nam tren duong go.
    backend->activate();

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

    // Dung luong ban phim truoc khi doc va luu cac bien engine tu UI thread.
    backend->stop();
    config.save();
    config.saveMacroTable();
    config.saveSmartSwitchTable();
    return result;
}
