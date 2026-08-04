//
//  IBusEngineObject.cpp
//  OpenKey cho Linux
//

#include "IBusEngineObject.h"

#include "CharCodec.h"
#include "IBusDeletePlan.h"
#include "IBusKeyTranslate.h"
#include "IBusTypes.h"

namespace openkey {
namespace {

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

void IBusEngineObject::FocusIn() {}

void IBusEngineObject::FocusOut() {
    if (_reset) _reset();
}

void IBusEngineObject::Reset() {
    if (_reset) _reset();
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
    const IBusDeletePlan plan = planDelete(del, _clientHasSurroundingText);
    if (plan.useSurrounding) {
        emit DeleteSurroundingText(-static_cast<int>(plan.chars), plan.chars);
    } else {
        sendBackspaces(plan.backspaces);
    }

    if (out.empty()) return;
    const QString text = QString::fromStdString(utf8Encode(out));
    emit CommitText(QDBusVariant(QVariant::fromValue(makeIBusText(text))));
}

} // namespace openkey
