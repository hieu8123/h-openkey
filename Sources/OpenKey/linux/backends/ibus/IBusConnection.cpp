//
//  IBusConnection.cpp
//  OpenKey cho Linux
//

#include "IBusConnection.h"

#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QThread>

#include "DebugLog.h"
#include "IBusTypes.h"

namespace openkey {
namespace {

#define IBUS_LOG(...) debugLog("ibus", __VA_ARGS__)

// Chờ tối đa 3 giây cho daemon vừa bật ghi xong tệp địa chỉ và mở socket.
constexpr int kDaemonWaitTries = 30;
constexpr int kDaemonWaitStepMs = 100;

constexpr const char* kService = "org.freedesktop.IBus";
constexpr const char* kBusPath = "/org/freedesktop/IBus";
constexpr const char* kBusInterface = "org.freedesktop.IBus";

// Tên kết nối trong Qt, không phải tên trên bus. Chỉ để phân biệt với kết nối
// session bus thông thường mà Qt tự mở.
constexpr const char* kConnectionName = "openkey-ibus";

IBusComponentStruct buildComponent(const QString& version) {
    IBusEngineDescStruct engine;
    engine.name = "openkey";
    engine.longname = "OpenKey";
    engine.description = "Bo go tieng Viet";
    engine.language = "vi";
    engine.license = "GPL";
    engine.author = "hieulc";
    engine.icon = "h-openkey";
    engine.layout = "us";

    IBusComponentStruct component;
    component.name = "org.freedesktop.IBus.OpenKey";
    component.description = "H-OpenKey";
    component.version = version;
    component.license = "GPL";
    component.author = "hieulc";
    component.homepage = "https://github.com/hieu8123/OpenKey";
    // exec để rỗng: ta đăng ký động lúc chạy, không để daemon spawn tiến trình.
    component.engines.append(QDBusVariant(QVariant::fromValue(engine)));
    return component;
}

} // namespace

IBusConnection::IBusConnection(QObject* parent)
    : QObject(parent), _bus(QDBusConnection(QString())) {
    registerIBusTypes();
}

QString IBusConnection::findAddress(std::string& error) {
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString direct = env.value("IBUS_ADDRESS");
    if (!direct.isEmpty()) return direct;

    // ibus-daemon ghi địa chỉ ra một tệp tên theo machine-id và màn hình.
    QFile machineIdFile("/etc/machine-id");
    if (!machineIdFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = "không đọc được /etc/machine-id để tìm ibus-daemon";
        return {};
    }
    const QString machineId = QString::fromUtf8(machineIdFile.readLine()).trimmed();

    // Phiên Wayland dùng thẳng WAYLAND_DISPLAY (ví dụ "wayland-0"), phiên X11
    // dùng số màn hình trong DISPLAY (":1" -> "1").
    const QString display = !env.value("WAYLAND_DISPLAY").isEmpty()
                                ? env.value("WAYLAND_DISPLAY")
                                : env.value("DISPLAY").section(':', 1).section('.', 0, 0);
    const QString path =
        QDir::homePath() + "/.config/ibus/bus/" + machineId + "-unix-" + display;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = "ibus-daemon chưa chạy (không thấy " + path.toStdString() + ")";
        return {};
    }
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.startsWith("IBUS_ADDRESS=")) {
            const QString address = line.mid(QString("IBUS_ADDRESS=").size()).trimmed();
            if (address.isEmpty()) break;
            return address;
        }
    }
    error = "ibus-daemon chưa chạy (tệp địa chỉ không có IBUS_ADDRESS)";
    return {};
}

bool IBusConnection::open(std::string& error) {
    if (connectOnce(error)) return true;

    // Nhiều máy tắt hẳn ibus, và tệp địa chỉ còn sót lại trỏ vào một socket đã
    // chết. Cả hai trường hợp đều cứu được bằng cách tự bật daemon lên rồi thử
    // lại, thay vì bắt người dùng mở terminal gõ lệnh.
    if (!QProcess::startDetached("ibus-daemon", {"-drx"})) {
        return false;  // giữ nguyên `error` của lần thử đầu
    }
    IBUS_LOG("ibus-daemon chua chay, da tu bat");

    // Daemon mất một lúc mới ghi xong tệp địa chỉ và mở socket.
    for (int i = 0; i < kDaemonWaitTries; ++i) {
        QThread::msleep(kDaemonWaitStepMs);
        std::string retryError;
        if (connectOnce(retryError)) return true;
    }
    error = "bật được ibus-daemon nhưng vẫn không kết nối được";
    return false;
}

bool IBusConnection::connectOnce(std::string& error) {
    const QString address = findAddress(error);
    if (address.isEmpty()) return false;

    // Lần thử trước có thể đã để lại một kết nối hỏng mang đúng tên này.
    QDBusConnection::disconnectFromBus(kConnectionName);

    _bus = QDBusConnection::connectToBus(address, kConnectionName);
    if (!_bus.isConnected()) {
        error = "không kết nối được tới ibus-daemon: " +
                _bus.lastError().message().toStdString();
        return false;
    }

    // Daemon chết rồi sống lại thì phải đăng ký lại từ đầu, nếu không OpenKey
    // câm lặng mà không báo gì cả — kiểu lỗi khó đoán nhất.
    auto* watcher = new QDBusServiceWatcher(
        kService, _bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString&, const QString& oldOwner, const QString& newOwner) {
                if (!newOwner.isEmpty() && !oldOwner.isEmpty()) emit daemonRestarted();
            });
    return true;
}

bool IBusConnection::registerComponent(std::string& error) {
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kBusPath, kBusInterface,
                                                       "RegisterComponent");
    call << QVariant::fromValue(
        QDBusVariant(QVariant::fromValue(buildComponent(OPENKEY_VERSION))));

    const QDBusMessage reply = _bus.call(call);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        error = "ibus-daemon từ chối đăng ký component: " +
                reply.errorMessage().toStdString();
        return false;
    }
    return true;
}

void IBusConnection::setGlobalEngine() {
    QDBusMessage call =
        QDBusMessage::createMethodCall(kService, kBusPath, kBusInterface, "SetGlobalEngine");
    call << QString("openkey");
    _bus.asyncCall(call);
}

} // namespace openkey
