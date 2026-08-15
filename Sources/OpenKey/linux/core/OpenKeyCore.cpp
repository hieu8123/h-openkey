//
//  OpenKeyCore.cpp
//  OpenKey cho Linux
//
//  Trinh tu xu ly o day sao lai ban macOS (OpenKey.mm, ham eventTapFunction) va
//  ban Windows (OpenKey.cpp). Diem khac duy nhat: thay vi ban tung phim ao,
//  chung ta gom ket qua thanh mot lan xoa + mot lan chen.
//

#include "OpenKeyCore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "AppState.h"
#include "CharCodec.h"
#include "DebugLog.h"
#include "Engine.h"
#include "SmartSwitchKey.h"

namespace openkey {
namespace {

// Phim bo tro. Tuyet doi khong duoc dua vao engine: platforms/linux.h khong co
// ma cho chung, nen engine se nhet rac vao bo dem tu dang go, kiem tra chinh ta
// that bai, roi tat luon viec xu ly cho toi khi ngat tu. Ban macOS khong dinh
// loi nay vi Cmd/Alt/Shift den qua flagsChanged chu khong phai keyDown.
bool isModifierKey(uint32_t keycode) {
    switch (keycode) {
        case 50:  // Shift_L
        case 62:  // Shift_R
        case 37:  // Control_L
        case 105: // Control_R
        case 64:  // Alt_L
        case 108: // Alt_R
        case 133: // Super_L
        case 134: // Super_R
        case 66:  // Caps_Lock
        case 77:  // Num_Lock
        case 78:  // Scroll_Lock
            return true;
        default:
            return false;
    }
}

// Phim khong sinh ky tu va co the lam con tro nhay cho khac. Engine khong biet
// chung, nen ta tu ngat tu thay vi de chung roi vao bo dem.
bool isNonTextKey(uint32_t keycode) {
    if (keycode >= 67 && keycode <= 76) return true;   // F1..F10
    if (keycode == 95 || keycode == 96) return true;   // F11, F12
    switch (keycode) {
        case 9:   // Escape
        case 107: // Print
        case 110: // Home
        case 111: // Up
        case 112: // Prior (Page Up)
        case 113: // Left
        case 114: // Right
        case 115: // End
        case 116: // Down
        case 117: // Next (Page Down)
        case 118: // Insert
        case 119: // Delete (xoa toi truoc)
        case 127: // Pause
        case 135: // Menu
            return true;
        default:
            return false;
    }
}

bool coreDebug() { return debugLoggingEnabled(); }

} // namespace

OpenKeyCore::OpenKeyCore(IBackend& backend) : _backend(backend) {
    _hook = static_cast<vKeyHookState*>(vKeyInit());
}

void OpenKeyCore::attach() {
    _backend.setKeyHandler([this](const KeyEvent& ev) { return onKey(ev); });
    _backend.setFocusHandler([this](const std::string& appId) { onFocusChanged(appId); });
    _backend.setContextBreakHandler([this] { onMouseDown(); });
}

void OpenKeyCore::resetTypingState() {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _sent.clear();
    startNewSession();
}

void OpenKeyCore::onMouseDown() {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    vKeyHandleEvent(vKeyEvent::Mouse, vKeyEventState::MouseDown, 0);
    _sent.clear();
}

void OpenKeyCore::setSuspended(bool suspended) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (_suspended == suspended) return;
    _suspended = suspended;
    resetTypingState();
}

void OpenKeyCore::toggleLanguage() {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (vLanguage != 1 && onVietnameseActivationRequested) {
        if (_vietnameseActivationPending) return;
        _vietnameseActivationPending = true;
        resetTypingState();
        onVietnameseActivationRequested();
        return;
    }

    _vietnameseActivationPending = false;
    vLanguage = vLanguage == 1 ? 0 : 1;
    resetTypingState();
    rememberCurrentApp();

    // Phai bao cho giao dien ve lai, neu khong bieu tuong khay dung im va nguoi
    // dung tuong phim tat khong an — trong khi che do da doi that. Duong bam
    // bang chuot o khay tu goi refresh nen khong lo loi nay, con duong phim tat
    // thi phai bao o day.
    if (onStateChanged) {
        onStateChanged();
    }
}

void OpenKeyCore::completeVietnameseActivation(bool activated) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (!_vietnameseActivationPending) return;
    _vietnameseActivationPending = false;
    if (!activated) {
        resetTypingState();
        return;
    }
    vLanguage = 1;
    resetTypingState();
    rememberCurrentApp();
    if (onStateChanged) onStateChanged();
}

void OpenKeyCore::rememberCurrentApp() {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (!vUseSmartSwitchKey || _focusedAppId.empty()) {
        return;
    }
    // Engine goi chung mot o nho cho ca hai: bit 0 la ngon ngu, cac bit tren
    // la bang ma.
    setAppInputMethodStatus(_focusedAppId, vLanguage | (vCodeTable << 1));
}

void OpenKeyCore::onFocusChanged(const std::string& appId) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _focusedAppId = appId;
    resetTypingState();

    if (!vUseSmartSwitchKey || appId.empty()) {
        return;
    }

    // Engine goi cung mot o nho cho ca ngon ngu lan bang ma: bit 0 la ngon ngu,
    // cac bit tren la bang ma.
    const int current = vLanguage | (vCodeTable << 1);
    const int remembered = getAppInputMethodStatus(appId, current);
    if (remembered < 0 || remembered == current) {
        return;
    }

    vLanguage = remembered & 0x1;
    if (vRememberCode) {
        vCodeTable = remembered >> 1;
        onTableCodeChange();
    }
    if (onStateChanged) {
        onStateChanged();
    }
}

bool OpenKeyCore::modifiersMatchSwitchKey(const KeyEvent& ev) const {
    return HAS_CONTROL(vSwitchKeyStatus) == (ev.ctrl ? 1 : 0) &&
           HAS_OPTION(vSwitchKeyStatus) == (ev.alt ? 1 : 0) &&
           HAS_COMMAND(vSwitchKeyStatus) == (ev.super ? 1 : 0) &&
           HAS_SHIFT(vSwitchKeyStatus) == (ev.shift ? 1 : 0);
}

bool OpenKeyCore::matchSwitchKey(const KeyEvent& ev) const {
    const int wanted = GET_SWITCH_KEY(vSwitchKeyStatus);
    if (wanted == kSwitchKeyModifiersOnly) {
        return false; // xu ly rieng o handleModifierOnlySwitchKey
    }
    return static_cast<int>(ev.keycode) == wanted && modifiersMatchSwitchKey(ev);
}

bool OpenKeyCore::handleModifierOnlySwitchKey(const KeyEvent& ev) {
    if (coreDebug() && isModifierKey(ev.keycode)) {
        debugLog("switch",
                 "keycode=%u pressed=%d ctrl=%d alt=%d shift=%d super=%d | "
                 "status=%d want(ctrl=%d alt=%d shift=%d super=%d key=%d) armed=%d",
                 ev.keycode, (int)ev.pressed, (int)ev.ctrl, (int)ev.alt,
                 (int)ev.shift, (int)ev.super, vSwitchKeyStatus,
                 HAS_CONTROL(vSwitchKeyStatus), HAS_OPTION(vSwitchKeyStatus),
                 HAS_SHIFT(vSwitchKeyStatus), HAS_COMMAND(vSwitchKeyStatus),
                 GET_SWITCH_KEY(vSwitchKeyStatus), (int)_switchKeyArmed);
    }

    if (GET_SWITCH_KEY(vSwitchKeyStatus) != kSwitchKeyModifiersOnly) {
        return false;
    }

    if (!isModifierKey(ev.keycode)) {
        // Co phim khac xen vao: day la mot to hop that su, khong phai y dinh
        // doi che do. Huy trang thai san sang.
        if (ev.pressed) {
            _switchKeyArmed = false;
        }
        return false;
    }

    if (modifiersMatchSwitchKey(ev)) {
        _switchKeyArmed = true;
        return false;
    }

    // To hop vua bi pha vo. Neu truoc do da du thi day chinh la luc nha ra.
    if (_switchKeyArmed) {
        _switchKeyArmed = false;
        toggleLanguage();
        return true;
    }
    return false;
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
    // Cung workaround cua OpenKey tren Windows/macOS: mot ky tu dem vo hinh
    // lam Chromium/Excel bo phan autocomplete dang chon truoc khi xoa. Engine
    // danh dau extCode=4 cho cac ca tuyet doi khong duoc chen ky tu dem.
    del.clearAutocomplete = vFixRecommendBrowser && _hook->extCode != 4;

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
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (_suspended) {
        return KeyVerdict::Forward;
    }

    // Phai xet ca luc nha phim, vi phim tat chi gom phim bo tro duoc kich hoat
    // khi nha ra chu khong phai khi bam xuong.
    if (handleModifierOnlySwitchKey(ev)) {
        return KeyVerdict::Swallow;
    }

    if (!ev.pressed) {
        return KeyVerdict::Forward;
    }

    if (matchSwitchKey(ev)) {
        toggleLanguage();
        return KeyVerdict::Swallow;
    }

    if (isModifierKey(ev.keycode)) {
        return KeyVerdict::Forward;
    }

    if (isNonTextKey(ev.keycode)) {
        resetTypingState();
        return KeyVerdict::Forward;
    }

    const Uint8 capsStatus = ev.shift ? 1 : (ev.capsLock ? 2 : 0);

    // Che do tieng Anh. Engine KHONG tu doc vLanguage — Engine.h chi khai bao
    // no, con viec chan lai la phan cua tang nen tang (xem OpenKey.mm dong 672
    // va win32/OpenKey.cpp dong 565). Thieu buoc nay thi doi sang tieng Anh chi
    // doi bieu tuong con chu van bi bo dau nhu thuong.
    if (vLanguage == 0) {
        if (vUseMacro && vUseMacroInEnglishMode) {
            vEnglishMode(vKeyEventState::KeyDown, static_cast<Uint16>(ev.keycode),
                         ev.shift || ev.capsLock, ev.otherControlKey());
            if (_hook->code == vReplaceMaro) {
                std::u32string text;
                std::vector<SentChar> costs;
                for (uint32_t data : _hook->macroData) {
                    appendEngineChar(data, text, costs);
                }
                appendEngineChar(ev.keycode | (capsStatus ? CAPS_MASK : 0), text, costs);
                emitResult(_hook->backspaceCount, text, costs);
                return KeyVerdict::Swallow;
            }
        }
        return KeyVerdict::Forward;
    }

    vKeyHandleEvent(vKeyEvent::Keyboard, vKeyEventState::KeyDown,
                    static_cast<Uint16>(ev.keycode), capsStatus,
                    ev.otherControlKey());

    if (coreDebug()) {
        debugLog("core",
                 "keycode=%u caps=%u ctrl=%d code=%u ext=%u bs=%u nc=%u "
                 "lang=%d input=%d",
                 ev.keycode, capsStatus, (int)ev.otherControlKey(), _hook->code,
                 _hook->extCode, _hook->backspaceCount, _hook->newCharCount,
                 vLanguage, vInputType);
    }

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
