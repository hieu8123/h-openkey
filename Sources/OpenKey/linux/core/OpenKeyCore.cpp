//
//  OpenKeyCore.cpp
//  OpenKey cho Linux
//
//  Trinh tu xu ly o day sao lai ban macOS (OpenKey.mm, ham eventTapFunction) va
//  ban Windows (OpenKey.cpp). Diem khac duy nhat: thay vi ban tung phim ao,
//  chung ta gom ket qua thanh mot lan xoa + mot lan chen.
//

#include "OpenKeyCore.h"

#include "AppState.h"
#include "CharCodec.h"
#include "Engine.h"
#include "SmartSwitchKey.h"

namespace openkey {

OpenKeyCore::OpenKeyCore(IBackend& backend) : _backend(backend) {
    _hook = static_cast<vKeyHookState*>(vKeyInit());
}

void OpenKeyCore::attach() {
    _backend.setKeyHandler([this](const KeyEvent& ev) { return onKey(ev); });
}

void OpenKeyCore::resetTypingState() {
    _sent.clear();
    startNewSession();
}

void OpenKeyCore::onMouseDown() {
    vKeyHandleEvent(vKeyEvent::Mouse, vKeyEventState::MouseDown, 0);
    _sent.clear();
}

void OpenKeyCore::toggleLanguage() {
    vLanguage = vLanguage == 1 ? 0 : 1;
    resetTypingState();
}

void OpenKeyCore::onFocusChanged(const std::string& appId) {
    _focusedAppId = appId;
    resetTypingState();

    if (!vUseSmartSwitchKey || appId.empty()) {
        return;
    }

    // Engine goi cung mot o nho cho ca ngon ngu lan bang ma: bit 0 la ngon ngu,
    // cac bit tren la bang ma.
    const int current = vLanguage | (vCodeTable << 1);
    const int remembered = getAppInputMethodStatus(appId, current);
    if (remembered >= 0) {
        vLanguage = remembered & 0x1;
        if (vRememberCode) {
            vCodeTable = remembered >> 1;
        }
    }
}

bool OpenKeyCore::matchSwitchKey(const KeyEvent& ev) const {
    const int wanted = GET_SWITCH_KEY(vSwitchKeyStatus);
    if (wanted == 0xFE) {
        // Phim tat chi gom phim bo tro. Chua ho tro o giai doan nay.
        return false;
    }
    if (static_cast<int>(ev.keycode) != wanted) {
        return false;
    }
    return HAS_CONTROL(vSwitchKeyStatus) == (ev.ctrl ? 1 : 0) &&
           HAS_OPTION(vSwitchKeyStatus) == (ev.alt ? 1 : 0) &&
           HAS_COMMAND(vSwitchKeyStatus) == (ev.super ? 1 : 0) &&
           HAS_SHIFT(vSwitchKeyStatus) == (ev.shift ? 1 : 0);
}

void OpenKeyCore::appendEngineChar(uint32_t data, std::u32string& text,
                                   std::vector<SentChar>& costs) const {
    const DecodedChar dec = decodeEngineChar(data);
    if (dec.count == 0) {
        return;
    }
    SentChar cost;
    cost.units = dec.count;
    cost.utf8Bytes = 0;
    for (uint8_t i = 0; i < dec.count; i++) {
        text.push_back(dec.cp[i]);
        cost.utf8Bytes = static_cast<uint8_t>(cost.utf8Bytes + utf8Length(dec.cp[i]));
    }
    costs.push_back(cost);
}

void OpenKeyCore::emitResult(int backspaceCount, const std::u32string& text,
                       const std::vector<SentChar>& costs) {
    DeleteRequest del;

    for (int i = 0; i < backspaceCount; i++) {
        if (!_sent.empty()) {
            const SentChar& c = _sent.back();
            del.utf8Bytes += c.utf8Bytes;
            del.keyPresses += c.units;
            _sent.pop_back();
        } else {
            // Ky uc khong con khop voi o nhap (vi du nguoi dung vua dan van ban).
            // Doan mot ky tu ASCII, giong gia dinh cua ban macOS va Windows.
            del.utf8Bytes += 1;
            del.keyPresses += 1;
        }
    }

    _backend.sendResult(del, text);
    _sent.insert(_sent.end(), costs.begin(), costs.end());
}

KeyVerdict OpenKeyCore::onKey(const KeyEvent& ev) {
    if (!ev.pressed) {
        return KeyVerdict::Forward;
    }

    if (matchSwitchKey(ev)) {
        toggleLanguage();
        return KeyVerdict::Swallow;
    }

    const Uint8 capsStatus = ev.shift ? 1 : (ev.capsLock ? 2 : 0);
    vKeyHandleEvent(vKeyEvent::Keyboard, vKeyEventState::KeyDown,
                    static_cast<Uint16>(ev.keycode), capsStatus,
                    ev.otherControlKey());

    if (_hook->code == vDoNothing) {
        // Engine khong dung toi phim nay, nhung van phai ghi lai anh huong cua
        // no len o nhap de lan sau tinh dung so byte can xoa.
        switch (_hook->extCode) {
            case 1: // phim ngat tu
                _sent.clear();
                break;
            case 2: // phim xoa
                if (!_sent.empty()) {
                    _sent.pop_back();
                }
                break;
            case 3: // phim thuong
                _sent.push_back(SentChar{1, 1});
                break;
            default:
                break;
        }
        return KeyVerdict::Forward;
    }

    if (_hook->code == vWillProcess || _hook->code == vRestore ||
        _hook->code == vRestoreAndStartNewSession) {
        std::u32string text;
        std::vector<SentChar> costs;

        // charData duoc engine xep nguoc, ky tu dau tien nam o cuoi mang.
        const int n = _hook->newCharCount;
        if (n > 0 && n <= MAX_BUFF) {
            for (int i = n - 1; i >= 0; i--) {
                appendEngineChar(_hook->charData[i], text, costs);
            }
        }

        if (_hook->code == vRestore || _hook->code == vRestoreAndStartNewSession) {
            // Tu khong hop le: tra lai nguyen phim vua bam.
            appendEngineChar(ev.keycode | (capsStatus ? CAPS_MASK : 0), text, costs);
        }

        emitResult(_hook->backspaceCount, text, costs);

        if (_hook->code == vRestoreAndStartNewSession) {
            startNewSession();
            _sent.clear();
        }
        return KeyVerdict::Swallow;
    }

    if (_hook->code == vReplaceMaro) {
        std::u32string text;
        std::vector<SentChar> costs;

        // macroData xep xuoi, nguoc voi charData.
        for (uint32_t data : _hook->macroData) {
            appendEngineChar(data, text, costs);
        }
        // Phim kich hoat macro cung phai duoc go ra.
        appendEngineChar(ev.keycode | (capsStatus ? CAPS_MASK : 0), text, costs);

        emitResult(_hook->backspaceCount, text, costs);
        return KeyVerdict::Swallow;
    }

    return KeyVerdict::Swallow;
}

} // namespace openkey
