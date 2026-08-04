//
//  IBusEngineObject.h
//  OpenKey cho Linux
//
//  Đối tượng mà ibus-daemon gọi tới khi người dùng đang gõ. Xuất interface
//  org.freedesktop.IBus.Engine qua QtDBus.
//

#ifndef OPENKEY_LINUX_IBUS_ENGINE_OBJECT_H
#define OPENKEY_LINUX_IBUS_ENGINE_OBJECT_H

#include <QDBusConnection>
#include <QDBusVariant>
#include <QObject>
#include <QString>

#include <functional>
#include <string>

#include "Backend.h"

namespace openkey {

class IBusEngineObject : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.IBus.Engine")

public:
    IBusEngineObject(QDBusConnection bus, QString path, QObject* parent = nullptr);

    QString objectPath() const { return _path; }

    void setKeyHandler(std::function<KeyVerdict(const KeyEvent&)> h) {
        _handler = std::move(h);
    }

    // Gọi khi IBus báo ô nhập đã đổi (Reset, FocusOut). Backend nối cái này vào
    // _focusHandler(""): OpenKeyCore::onFocusChanged với appId rỗng gọi thẳng
    // resetTypingState() rồi thoát sớm, đúng thứ ta cần.
    void setResetHandler(std::function<void()> h) { _reset = std::move(h); }

    // Xoá phần cũ rồi chèn chữ mới. Tuyệt đối không dùng preedit.
    void commit(const DeleteRequest& del, const std::u32string& out);

public slots:  // ibus-daemon gọi tới qua DBus
    bool ProcessKeyEvent(uint keyval, uint keycode, uint state);
    void FocusIn();
    void FocusOut();
    void Reset();
    void Enable();
    void Disable();
    void SetCapabilities(uint caps);
    void SetCursorLocation(int x, int y, int w, int h);
    void PropertyActivate(const QString& name, uint state);
    void Destroy();

signals:  // ta phát ngược về cho ứng dụng
    void CommitText(const QDBusVariant& text);
    void ForwardKeyEvent(uint keyval, uint keycode, uint state);
    void DeleteSurroundingText(int offset, uint nchars);

private:
    void sendBackspaces(uint32_t count);

    QDBusConnection _bus;
    QString _path;
    std::function<KeyVerdict(const KeyEvent&)> _handler;
    std::function<void()> _reset;

    // Ứng dụng đang gõ có cho xoá bằng surrounding text không. IBus báo qua
    // SetCapabilities; mặc định coi là không cho, an toàn hơn.
    bool _clientHasSurroundingText = false;
};

} // namespace openkey

#endif // OPENKEY_LINUX_IBUS_ENGINE_OBJECT_H
