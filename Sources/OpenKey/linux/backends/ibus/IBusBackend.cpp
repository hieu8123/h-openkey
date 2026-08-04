//
//  IBusBackend.cpp
//  OpenKey cho Linux
//
//  Đường gõ duy nhất chạy được trên phiên Wayland của GNOME. Khác hẳn hai
//  backend kia ở chỗ nó không hề đụng tới bàn phím: ibus-daemon hỏi ta từng
//  phím một và chờ câu trả lời nuốt hay không, nên không phải grab gì cả.
//

#include "IBusBackend.h"

#include <QObject>

#include <memory>

#include "DebugLog.h"
#include "IBusConnection.h"
#include "IBusEngineObject.h"
#include "IBusFactoryObject.h"

namespace openkey {
namespace {

#define IBUS_LOG(...) debugLog("ibus", __VA_ARGS__)

class IBusBackend final : public IBackend {
public:
    ~IBusBackend() override { stop(); }

    bool open(std::string& error);

    const char* name() const override { return "ibus"; }
    BackendCaps caps() const override { return _caps; }
    bool start() override;
    void stop() override;
    const std::string& lastError() const override { return _error; }

    // Smart Switch Key cần biết cửa sổ nào đang focus, mà GNOME Wayland không
    // cho ứng dụng ngoài hỏi điều đó. Nên caps().hasAppId = false và ta trả
    // chuỗi rỗng, tính năng đó tự tắt.
    std::string focusedAppId() override { return {}; }

    void sendResult(const DeleteRequest& del, const std::u32string& out) override;

    // Để trống có chủ ý. IBus tự lo phím gốc khi ProcessKeyEvent trả về false;
    // tự bơm lại ở đây sẽ làm nhân đôi ký tự.
    void forwardKey(const KeyEvent&) override {}

    // QtDBus đã gắn sẵn vào vòng lặp sự kiện của Qt nên không có fd nào để
    // ứng dụng theo dõi, cũng không cần nhịp đẩy hàng đợi.
    int eventFd() const override { return -1; }

private:
    void wireEngine(IBusEngineObject* engine);

    std::unique_ptr<IBusConnection> _connection;
    IBusFactoryObject* _factory = nullptr;

    // Engine đang phục vụ ô nhập hiện tại. ibus-daemon đẻ engine mới mỗi lần
    // đổi ứng dụng, nên con trỏ này đổi luôn theo.
    IBusEngineObject* _engine = nullptr;

    BackendCaps _caps;
    std::string _error;
};

bool IBusBackend::open(std::string& error) {
    _connection = std::make_unique<IBusConnection>();
    if (!_connection->open(error)) return false;
    if (!_connection->registerComponent(error)) return false;

    _factory = new IBusFactoryObject(_connection->bus());
    _factory->setEngineCreatedHandler([this](IBusEngineObject* engine) { wireEngine(engine); });

    // Daemon sống lại thì mọi đăng ký cũ mất sạch, phải khai lại từ đầu.
    QObject::connect(_connection.get(), &IBusConnection::daemonRestarted, _factory, [this] {
        std::string reason;
        if (!_connection->registerComponent(reason)) {
            IBUS_LOG("dang ky lai that bai: %s", reason.c_str());
            return;
        }
        IBUS_LOG("ibus-daemon song lai, da dang ky lai");
        _connection->setGlobalEngine();
    });

    _caps.hasSurroundingText = true;
    _caps.hasAppId = false;
    _caps.canForwardKey = true;
    return true;
}

void IBusBackend::wireEngine(IBusEngineObject* engine) {
    _engine = engine;
    engine->setKeyHandler([this](const KeyEvent& ev) {
        return _handler ? _handler(ev) : KeyVerdict::Forward;
    });
    // OpenKeyCore::onFocusChanged với appId rỗng gọi thẳng resetTypingState().
    engine->setResetHandler([this] {
        if (_focusHandler) _focusHandler("");
    });
    IBUS_LOG("engine moi: %s", engine->objectPath().toUtf8().constData());
}

bool IBusBackend::start() {
    _connection->setGlobalEngine();
    return true;
}

void IBusBackend::stop() {
    _engine = nullptr;
    delete _factory;
    _factory = nullptr;
    _connection.reset();
}

void IBusBackend::sendResult(const DeleteRequest& del, const std::u32string& out) {
    // Chưa có ô nhập nào đang hoạt động thì không có chỗ nào để gửi chữ tới.
    if (!_engine) return;
    _engine->commit(del, out);
}

} // namespace

std::unique_ptr<IBackend> makeIBusBackend(std::string& error) {
    auto backend = std::make_unique<IBusBackend>();
    if (!backend->open(error)) return nullptr;
    return backend;
}

} // namespace openkey
