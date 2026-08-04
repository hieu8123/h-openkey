//
//  main.cpp
//  OpenKey cho Linux
//

#include <QApplication>
#include <QMessageBox>
#include <QSocketNotifier>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QWindow>

#include <QDir>
#include <QFile>

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>

#include "Backend.h"
#include "Config.h"
#include "OpenKeyCore.h"
#include "MainWindow.h"
#include "SingleInstance.h"
#include "Theme.h"
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

// SIGTERM/SIGINT (systemctl stop, kill thuong) khong tu goi app.exec() thoat,
// nen backend->stop() phia duoi main() khong duoc chay: Wayland thi khong sao,
// nhung X11Backend can stop() de tra lai cac keycode da muon, neu khong lan
// chay sau se can kiet dan. Bat tin hieu bang self-pipe roi thoat qua duong
// Qt binh thuong, thay vi de mac dinh giet tien trinh ngay lap tuc.
int g_signalPipe[2] = {-1, -1};

void handleTermSignal(int) {
    const char byte = 0;
    ssize_t ignored = write(g_signalPipe[1], &byte, 1);
    (void)ignored;
}

} // namespace

int main(int argc, char** argv) {
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

    // Khởi động hỏng thì cũng không tắt ứng dụng: đổi sang backend rỗng rồi chạy
    // tiếp. Tắt hẳn là kẹt cứng — không chạy thì không mở được bảng điều khiển,
    // mà bảng điều khiển lại là chỗ duy nhất để đổi lại cấu hình.
    if (!backend->start()) {
        std::string reason = backend->lastError();
        if (!notice.empty()) reason = notice + "\n" + reason;
        notice = reason;
        backend = openkey::makeNullBackend();
        backend->start();
    }

    if (!notice.empty()) {
        const QString requested =
            QString::fromUtf8(openkey::backendKindToString(config.backend));
        QString message;
        if (backend->canType()) {
            // Đã rơi xuống được một backend thật. Đổi luôn cấu hình sang Auto: bảng
            // điều khiển phải hiện đúng thứ đang chạy, và lần mở sau không lặp lại
            // màn hình cảnh báo này.
            config.backend = openkey::BackendKind::Auto;
            message = QString("Không dùng được backend “%1” như cấu hình đang chọn:\n\n%2\n\n"
                              "OpenKey đã chuyển về “Tự động” và đang chạy bằng backend %3. "
                              "Bạn có thể chọn lại trong bảng điều khiển.")
                          .arg(requested, QString::fromStdString(notice),
                               QString::fromUtf8(backend->name()));
        } else {
            // Không còn đường nào gõ được. Vẫn chạy, vẫn có khay và bảng điều
            // khiển — chỉ là không gõ được tiếng Việt, và tuyệt đối không đụng
            // tới bàn phím của phiên.
            message = QString("OpenKey chưa gõ được tiếng Việt trong phiên này:\n\n%1\n\n"
                              "Ứng dụng vẫn chạy để bạn mở bảng điều khiển, và bàn phím "
                              "của bạn không bị đụng tới. Trên phiên Wayland của GNOME, "
                              "cách xử lý là đăng nhập lại bằng phiên Xorg.")
                          .arg(QString::fromStdString(notice));
        }
        std::fprintf(stderr, "OpenKey: %s\n", message.toUtf8().constData());
        QMessageBox::warning(nullptr, "H-OpenKey", message);
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
        QMessageBox::warning(nullptr, "H-OpenKey", message);
    }

    // Gan file descriptor cua backend vao vong lap su kien cua Qt. Nho vay chi
    // can mot tien trinh, khong phai tach daemon rieng.
    // Backend rỗng không bắt phím nên không có fd nào để theo dõi.
    if (backend->eventFd() >= 0) {
        auto* notifier =
            new QSocketNotifier(backend->eventFd(), QSocketNotifier::Read, &app);
        QObject::connect(notifier, &QSocketNotifier::activated, &app,
                         [&backend] { backend->dispatchEvents(); });
    }

    QSocketNotifier* signalNotifier = nullptr;
    if (pipe(g_signalPipe) == 0) {
        std::signal(SIGTERM, handleTermSignal);
        std::signal(SIGINT, handleTermSignal);
        signalNotifier = new QSocketNotifier(g_signalPipe[0], QSocketNotifier::Read, &app);
        QObject::connect(signalNotifier, &QSocketNotifier::activated, &app,
                         [] { QApplication::quit(); });
    }

    // Nhip cho hang doi xuat chu cua backend. Backend Wayland xep hang cac thao
    // tac va cho ung dung bao da xu ly xong; neu ung dung im lang thi nhip nay la
    // han chot de day tiep, khong de chu ket lai.
    QTimer outputTick;
    outputTick.setInterval(5);
    QObject::connect(&outputTick, &QTimer::timeout, &app, [&backend] { backend->tick(); });
    outputTick.start();

    openkey::TrayIcon tray(config, core);

    // Smart Switch Key co the tu doi ngon ngu khi chuyen ung dung; bieu tuong
    // khay phai phan anh dieu do, neu khong nguoi dung khong biet dang o che do nao.
    core.onStateChanged = [&tray] { tray.refresh(); };

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
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray.show();
    } else {
        std::fprintf(stderr,
                     "OpenKey: không có khay hệ thống, vẫn chạy nhưng không có biểu tượng\n");
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

    config.save();
    config.saveMacroTable();
    config.saveSmartSwitchTable();
    backend->stop();
    return result;
}
