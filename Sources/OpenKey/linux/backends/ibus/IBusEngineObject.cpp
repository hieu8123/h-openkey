//
//  IBusEngineObject.cpp
//  OpenKey cho Linux
//

#include "IBusEngineObject.h"

#include <cstdlib>

#include "CharCodec.h"
#include "DebugLog.h"
#include "IBusDeletePlan.h"
#include "IBusKeyTranslate.h"
#include "IBusTypes.h"

namespace openkey {
namespace {

#define IBUS_LOG(...) debugLog("ibus", __VA_ARGS__)

// Đường thoát hiểm: tắt hẳn DeleteSurroundingText, chỉ xoá bằng BackSpace. Để
// đó phòng khi cách bảo vệ trong canDeleteSurrounding vẫn còn sót một lối nào
// làm Mutter abort — hậu quả của nó là người dùng mất cả phiên đăng nhập.
bool surroundingTextDisabled() {
    const char* v = std::getenv("OPENKEY_IBUS_NO_SURROUNDING");
    return v != nullptr && *v != '\0';
}

// IBUS_CAP_SURROUNDING_TEXT
constexpr uint kCapSurroundingText = 1u << 5;

// Keysym và keycode evdev của BackSpace.
constexpr uint kKeyvalBackSpace = 0xff08;
constexpr uint kKeycodeBackSpace = 14;

constexpr uint kReleaseMask = 1u << 30;

} // namespace

IBusEngineObject::IBusEngineObject(QDBusConnection bus, QString path, QObject* parent)
    : QObject(parent), _bus(std::move(bus)), _path(std::move(path)) {
    _bus.registerObject(_path, this,
                        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
}

bool IBusEngineObject::ProcessKeyEvent(uint keyval, uint keycode, uint state) {
    if (!_handler) return false;
    const KeyEvent ev = keyEventFromIBus(keyval, keycode, state);
    return _handler(ev) == KeyVerdict::Swallow;
}

void IBusEngineObject::FocusIn() {
    _surroundingFresh = false;
}

void IBusEngineObject::FocusOut() {
    _surroundingFresh = false;
    if (_reset) _reset();
}

void IBusEngineObject::Reset() {
    _surroundingFresh = false;
    if (_reset) _reset();
}

void IBusEngineObject::SetSurroundingText(const QDBusVariant& text, uint cursor, uint) {
    (void)text;  // Chỉ cần biết có bao nhiêu ký tự trước con trỏ, không cần nội dung.
    _charsBeforeCursor = cursor;
    _surroundingFresh = true;
    IBUS_LOG("surrounding: %u ky tu truoc con tro", cursor);
}

void IBusEngineObject::Enable() {}
void IBusEngineObject::Disable() {}
void IBusEngineObject::SetCursorLocation(int, int, int, int) {}
void IBusEngineObject::PropertyActivate(const QString&, uint) {}
void IBusEngineObject::Destroy() {}

void IBusEngineObject::SetCapabilities(uint caps) {
    _clientHasSurroundingText = (caps & kCapSurroundingText) != 0;
}

void IBusEngineObject::sendBackspaces(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        emit ForwardKeyEvent(kKeyvalBackSpace, kKeycodeBackSpace, 0);
        emit ForwardKeyEvent(kKeyvalBackSpace, kKeycodeBackSpace, kReleaseMask);
    }
}

void IBusEngineObject::commit(const DeleteRequest& del, const std::u32string& out) {
    // Chỉ đi đường surrounding text khi biết chắc có đủ ký tự để xoá. Xin nhiều
    // hơn số đang có làm Mutter abort và người dùng mất cả phiên đăng nhập —
    // xem chú thích ở _surroundingFresh. Không chắc thì gửi BackSpace, chậm hơn
    // một chút nhưng không bao giờ hạ được cả phiên.
    static const bool noSurrounding = surroundingTextDisabled();
    const bool safeToUseSurrounding =
        !noSurrounding && canDeleteSurrounding(del, _clientHasSurroundingText,
                                               _surroundingFresh, _charsBeforeCursor);
    const IBusDeletePlan plan = planDelete(del, safeToUseSurrounding);
    IBUS_LOG("xoa %u ky tu bang %s (co %u truoc con tro, so lieu %s)", del.keyPresses,
             plan.useSurrounding ? "surrounding" : "BackSpace", _charsBeforeCursor,
             _surroundingFresh ? "moi" : "cu");
    if (plan.useSurrounding) {
        emit DeleteSurroundingText(-static_cast<int>(plan.chars), plan.chars);
    } else {
        sendBackspaces(plan.backspaces);
    }

    // Ta vừa sửa ô nhập nên con số ký tự trước con trỏ không còn đúng nữa, cho
    // tới khi ứng dụng gửi lại SetSurroundingText.
    _surroundingFresh = false;

    if (out.empty()) return;
    const QString text = QString::fromStdString(utf8Encode(out));
    emit CommitText(QDBusVariant(QVariant::fromValue(makeIBusText(text))));
}

} // namespace openkey
