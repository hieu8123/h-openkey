//
//  X11Backend.cpp
//  OpenKey cho Linux
//

#include "X11Backend.h"

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "CharCodec.h"
#include "DebugLog.h"
#include "EvdevKeyboard.h"

namespace openkey {
namespace {

#define X11_LOG(...) debugLog("x11", __VA_ARGS__)

// Keycode X11 cua cac phim bo tro.
bool isModifierKeycode(uint32_t keycode) {
    switch (keycode) {
        case 50: case 62:   // Shift
        case 37: case 105:  // Control
        case 64: case 108:  // Alt
        case 133: case 134: // Super
        case 66: case 77: case 78: // Caps/Num/Scroll Lock
            return true;
        default:
            return false;
    }
}

class X11Backend final : public IBackend {
public:
    ~X11Backend() override { stop(); }

    bool open(std::string& error);

    const char* name() const override { return "x11-xtest"; }
    BackendCaps caps() const override { return _caps; }
    bool start() override;
    void stop() override;
    const std::string& lastError() const override { return _error; }
    std::string focusedAppId() override;

    void sendResult(const DeleteRequest& del, const std::u32string& out) override;
    // EvdevKeyboard chan phim o kernel nen phim goc KHONG con tu den ung dung
    // nua: neu handler tra ve Forward, chinh ta phai go lai bang XTEST.
    void forwardKey(const KeyEvent& ev) override;

    int eventFd() const override;
    void dispatchEvents() override;
    void flush() override;
    void tick() override;

private:
    void handleKey(const EvdevKeyEvent& ev);

    void sendKeycode(unsigned int keycode, bool pressed);
    // Bam roi nha mot phim de sinh ra dung mot ky tu. Xem phan cai dat: phai tu
    // nha truoc neu phim do dang duoc giu, neu khong X se lang le bo lenh bam.
    void tapKeycode(unsigned int keycode);
    void sendBackspaces(uint32_t count);
    void typeCodePoint(char32_t codePoint);
    void collectSpareKeycodes();
    void buildCharMap();

    Display* _control = nullptr; // gui XTEST, doc/ghi ban do ban phim
    EvdevKeyboard _evdev;         // chan phim vat ly o kernel

    // Cac keycode X11 chua duoc dung, de gan tam keysym cho ky tu can go.
    std::vector<unsigned int> _spareKeycodes;
    unsigned int _backspaceKeycode = 0;
    unsigned int _shiftKeycode = 0;
    size_t _nextSpare = 0;

    // Ky tu tieng Viet -> (keycode da gan san, co can Shift khong). Gan MOT LAN
    // luc khoi dong nen luc go khong phai doi ban do phim nua.
    std::map<char32_t, std::pair<unsigned int, bool>> _charMap;

    // Shift vat ly co dang duoc giu khong. Can biet de bom dung tang keysym.
    bool _shiftDown = false;

    // Nhung keycode ma X dang coi la "dang bam". Moi phim deu di qua ta nen ta
    // biet chinh xac; can biet de khong bam mot phim dang bam (X se bo lenh do).
    std::set<unsigned int> _keysDownInX;

    // Dem nhip de do lai /dev/input mot giay mot lan, khong phai moi 5ms.
    unsigned int _ticksSinceScan = 0;

    // Dem nhip de hoi cua so dang focus, va ten cua so lan hoi truoc.
    unsigned int _ticksSinceFocus = 0;
    std::string _appId;

    // Chu cho gui o nhip sau. Xem sendResult() de biet vi sao phai tach ra.
    std::u32string _pendingText;
    bool _hasPendingText = false;
    void flushPendingText();

    BackendCaps _caps;
    std::string _error;
};

} // namespace

// --- vong doi ------------------------------------------------------------

bool X11Backend::open(std::string& error) {
    _control = XOpenDisplay(nullptr);
    if (!_control) {
        error = "không mở được màn hình X11";
        return false;
    }

    int major = 0;
    int minor = 0;
    int eventBase = 0;
    int errorBase = 0;
    if (!XTestQueryExtension(_control, &eventBase, &errorBase, &major, &minor)) {
        error = "X server không có phần mở rộng XTEST";
        return false;
    }
    return true;
}

bool X11Backend::start() {
    _backspaceKeycode = XKeysymToKeycode(_control, XK_BackSpace);
    if (_backspaceKeycode == 0) {
        _error = "không tìm được keycode của phím BackSpace";
        return false;
    }
    collectSpareKeycodes();
    if (_spareKeycodes.empty()) {
        _error = "không còn keycode trống nào để gõ chữ";
        return false;
    }
    X11_LOG("co %zu keycode trong de go chu", _spareKeycodes.size());
    buildCharMap();

    if (!_evdev.start(_error)) {
        return false;
    }
    _evdev.onKey = [this](const EvdevKeyEvent& ev) { handleKey(ev); };

    // Tat lap phim cua X server. Ban phim that da bi chan o kernel, nen moi lan
    // lap deu phai di ra tu evdev — neu de X tu lap them cho phim no tuong dang
    // giu thi man hinh co ky tu ma engine khong he biet, bo dem lech ngay va cac
    // lan xoa sau do se xoa sai so ky tu.
    XAutoRepeatOff(_control);
    XSync(_control, False);

    _caps.canForwardKey = true; // phim goc bi chan o kernel, ta phai tu go lai
    _caps.hasSurroundingText = false;
    _caps.hasAppId = true;
    return true;
}

void X11Backend::stop() {
    _evdev.stop();
    if (_control) {
        XAutoRepeatOn(_control); // tra lai lap phim cho phien lam viec
    }
    // Tra lai NoSymbol cho cac keycode da muon truoc khi dong ket noi, neu
    // khong lan chay sau se thay chung "co ky hieu" va cang ngay cang can
    // kiet keycode trong, den luc khong con cai nao thi khong khoi dong duoc
    // nua (loi "khong con keycode trong nao de go chu").
    if (_control && !_spareKeycodes.empty()) {
        KeySym symbols[2] = {NoSymbol, NoSymbol};
        for (unsigned int code : _spareKeycodes) {
            XChangeKeyboardMapping(_control, static_cast<int>(code), 2, symbols, 1);
        }
        XSync(_control, False);
    }
    if (_control) {
        XCloseDisplay(_control);
        _control = nullptr;
    }
}

int X11Backend::eventFd() const {
    return _evdev.epollFd();
}

void X11Backend::dispatchEvents() {
    _evdev.dispatchEvents();
}

void X11Backend::tick() {
    // Chu cua lan sua truoc da doi mot nhip, gio moi cho ra — tach han khoi
    // cum phim BackSpace vua gui.
    flushPendingText();

    // X11 khong co su kien bao "cua so focus vua doi" cho ung dung ngoai, nen
    // phai tu hoi. Khong hoi thi Smart Switch Key khong chay va bo dem go
    // khong duoc xoa khi doi ung dung — go tiep o ung dung moi se xoa sai so
    // ky tu. Hoi moi ~150ms la du nhanh voi mat nguoi ma khong ton gi.
    if (++_ticksSinceFocus >= 30) {
        _ticksSinceFocus = 0;
        const std::string appId = focusedAppId();
        if (appId != _appId) {
            _appId = appId;
            X11_LOG("focus doi sang: %s", appId.c_str());
            if (_focusHandler) {
                _focusHandler(appId);
            }
        }
    }

    // Do lai /dev/input mot giay mot lan de bat ban phim vua duoc cam vao.
    // tick() duoc goi rat day (5ms), nen phai tu han che lai.
    if (++_ticksSinceScan < 200) {
        return;
    }
    _ticksSinceScan = 0;
    _evdev.rescan();
}

void X11Backend::flush() {
    if (_control) {
        XFlush(_control);
    }
}

// --- ban do ban phim -----------------------------------------------------

void X11Backend::collectSpareKeycodes() {
    _spareKeycodes.clear();

    int minCode = 0;
    int maxCode = 0;
    XDisplayKeycodes(_control, &minCode, &maxCode);

    int perCode = 0;
    KeySym* map = XGetKeyboardMapping(_control, static_cast<KeyCode>(minCode),
                                      maxCode - minCode + 1, &perCode);
    if (!map) {
        return;
    }

    for (int code = minCode; code <= maxCode; code++) {
        bool used = false;
        for (int level = 0; level < perCode; level++) {
            const KeySym sym = map[(code - minCode) * perCode + level];
            // Keysym >= 0x01000000 la quy uoc Unicode cua X11 (xem
            // typeCodePoint): chinh la dau vet OpenKey tung muon keycode nay
            // roi thoat khong sach (crash, kill -9...). Coi no la trong lai
            // duoc, neu khong keycode se can kiet dan qua moi lan chay hong.
            if (sym != NoSymbol && sym < 0x01000000) {
                used = true;
                break;
            }
        }
        if (!used) {
            _spareKeycodes.push_back(static_cast<unsigned int>(code));
        }
    }
    XFree(map);
}

// Keysym X11 cua mot ky tu Unicode. Vung Latin-1 (U+0020..U+00FF) BAT BUOC
// dung thang gia tri ma ky tu — do la keysym chuan (vd 'â' la 0x00E2). Dang
// Unicode 0x01000000|ma chi danh cho tu U+0100 tro len; dung no cho Latin-1 se
// tao ra keysym phi chuan ma nhieu ung dung khong doi nguoc ra ky tu duoc, va
// chung lang le bo qua phim do — dung trieu chung "go chu co dau bi mat".
KeySym keysymFor(char32_t codePoint) {
    return codePoint < 0x100 ? static_cast<KeySym>(codePoint)
                             : static_cast<KeySym>(codePoint) | 0x01000000;
}

// Cap chu hoa/chu thuong cua tieng Viet. Gan chung vao cung mot keycode (tang
// 0 la chu thuong, tang 1 la chu hoa) de mot keycode go duoc hai ky tu — nho
// vay 116 ma trong du cho ca bang chu, khong phai doi ban do phim luc dang go.
std::vector<std::pair<char32_t, char32_t>> vietnamesePairs() {
    std::vector<std::pair<char32_t, char32_t>> pairs;

    // U+1EA0..U+1EF9: ma chan la chu hoa, ma le ngay sau la chu thuong.
    for (char32_t upper = 0x1EA0; upper < 0x1EF9; upper += 2) {
        pairs.emplace_back(upper + 1, upper); // {thuong, hoa}
    }

    static const char32_t latin[][2] = {
        {0x00E0, 0x00C0}, {0x00E1, 0x00C1}, {0x00E2, 0x00C2}, {0x00E3, 0x00C3},
        {0x00E8, 0x00C8}, {0x00E9, 0x00C9}, {0x00EA, 0x00CA}, {0x00EC, 0x00CC},
        {0x00ED, 0x00CD}, {0x00F2, 0x00D2}, {0x00F3, 0x00D3}, {0x00F4, 0x00D4},
        {0x00F5, 0x00D5}, {0x00F9, 0x00D9}, {0x00FA, 0x00DA}, {0x00FD, 0x00DD},
        {0x0103, 0x0102}, {0x0111, 0x0110}, {0x0129, 0x0128}, {0x0169, 0x0168},
        {0x01A1, 0x01A0}, {0x01B0, 0x01AF},
    };
    for (const auto& p : latin) {
        pairs.emplace_back(p[0], p[1]);
    }

    // Dau to hop, dung cho bang ma "Unicode to hop". Khong co dang hoa/thuong.
    static const char32_t marks[] = {0x0300, 0x0301, 0x0302, 0x0303,
                                     0x0306, 0x0309, 0x031B, 0x0323};
    for (char32_t m : marks) {
        pairs.emplace_back(m, m);
    }
    return pairs;
}

void X11Backend::buildCharMap() {
    _charMap.clear();
    _shiftKeycode = XKeysymToKeycode(_control, XK_Shift_L);

    const auto pairs = vietnamesePairs();

    // Chua vai ma trong lai lam duong lui cho ky tu khong nam trong bang (vi du
    // bang ma cu TCVN3/VNI): nhung ky tu do van phai doi ban do phim luc go.
    constexpr size_t kFallbackReserve = 4;
    const size_t usable = _spareKeycodes.size() > kFallbackReserve
                              ? _spareKeycodes.size() - kFallbackReserve
                              : 0;

    size_t assigned = 0;
    for (const auto& p : pairs) {
        if (assigned >= usable) break;
        const unsigned int code = _spareKeycodes[assigned];
        KeySym symbols[2] = {keysymFor(p.first), keysymFor(p.second)};
        XChangeKeyboardMapping(_control, static_cast<int>(code), 2, symbols, 1);
        _charMap[p.first] = {code, false};
        if (p.second != p.first) {
            _charMap[p.second] = {code, true};
        }
        assigned++;
    }

    // Ky tu ASCII da co san phim that tren ban phim — dung thang phim do. Engine
    // rat hay tra ve chuoi lan chu thuong (vd "ưa", "ại"), neu de chung roi vao
    // duong lui thi van phai doi ban do phim ngay truoc khi bom, va van mat chu
    // khi go nhanh y het truoc day.
    for (char32_t c = 0x20; c <= 0x7E; c++) {
        const KeySym sym = keysymFor(c);
        const KeyCode code = XKeysymToKeycode(_control, sym);
        if (code == 0) continue;
        if (XkbKeycodeToKeysym(_control, code, 0, 0) == sym) {
            _charMap[c] = {code, false};
        } else if (XkbKeycodeToKeysym(_control, code, 0, 1) == sym) {
            _charMap[c] = {code, true};
        }
    }

    // Chi mot lan dong bo cho ca bang: ung dung nhan mot MappingNotify duy nhat
    // luc khoi dong, thay vi mot cai truoc TUNG ky tu nhu truoc day.
    XSync(_control, False);

    // Duong lui bat dau tu sau vung da gan.
    _nextSpare = assigned;
    X11_LOG("gan san %zu ky tu vao %zu keycode, con %zu ma lam duong lui",
            _charMap.size(), assigned, _spareKeycodes.size() - assigned);
}

// --- nhan phim -----------------------------------------------------------

void X11Backend::handleKey(const EvdevKeyEvent& raw) {
    _shiftDown = raw.shift;

    // Chu dang cho phai ra truoc phim moi, neu khong thu tu se dao lon.
    flushPendingText();

    if (!_handler) {
        return;
    }

    KeyEvent ev;
    ev.keycode = raw.x11Keycode; // evdev + 8, dung nhu platforms/linux.h
    ev.pressed = raw.pressed;
    ev.shift = raw.shift;
    ev.capsLock = raw.capsLock;
    ev.ctrl = raw.ctrl;
    ev.alt = raw.alt;
    ev.super = raw.super;

    const bool forward = _handler(ev) == KeyVerdict::Forward;

    // Phim bo tro thi LUON phai chuyen tiep, du engine bao nuot. Phim tat doi
    // che do mac dinh (Ctrl+Shift) chi gom phim bo tro nen duoc kich hoat luc
    // NHA ra, va engine nuot dung lan nha do. Truoc day XRecord khong chan duoc
    // phim nen nuot cung vo hai, con bay gio nuot la that: lan bam da gui roi ma
    // lan nha khong toi X, nen X tuong Shift bi giu mai — go tiep ra chu hoa va
    // loan phim tat.
    if (forward || isModifierKeycode(ev.keycode)) {
        forwardKey(ev);
    }
    flush();
}

void X11Backend::forwardKey(const KeyEvent& ev) {
    sendKeycode(ev.keycode, ev.pressed);
}

// --- xuat chu ------------------------------------------------------------

void X11Backend::sendKeycode(unsigned int keycode, bool pressed) {
    if (pressed) {
        _keysDownInX.insert(keycode);
    } else {
        _keysDownInX.erase(keycode);
    }
    XTestFakeKeyEvent(_control, keycode, pressed ? True : False, 0);
}

void X11Backend::tapKeycode(unsigned int keycode) {
    // X BO QUA lenh bam mot phim ma no dang coi la bam san. Khi go nhanh, nguoi
    // dung thuong chua kip nha phim truoc thi da bam phim sau (go goi dau), nen
    // dung luc ta bom lai chinh phim do de xuat chu thi lenh bam bi nuot — chi
    // con release lot qua va ky tu bien mat. Da do duoc bang xev: lan hong chi
    // co "R kc=38", khong he co "P kc=38".
    //
    // Moi phim deu di qua evdev cua ta nen ta biet chac phim nao dang bam: cu
    // nha no ra truoc. Lan nguoi dung nha that sau do se thanh mot release thua,
    // va X bo qua release cua phim dang nha — vo hai.
    if (_keysDownInX.count(keycode) > 0) {
        sendKeycode(keycode, false);
    }
    sendKeycode(keycode, true);
    sendKeycode(keycode, false);
}

void X11Backend::sendBackspaces(uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        tapKeycode(_backspaceKeycode);
    }
}

void X11Backend::typeCodePoint(char32_t codePoint) {
    // Duong chinh: ky tu da co san mot keycode tu luc khoi dong, cu bam thang.
    // KHONG doi ban do phim o day — do chinh la nguyen nhan mat chu khi go
    // nhanh: ung dung giai ma phim bang ban sao keymap cua rieng no, chi cap
    // nhat khi xu ly toi MappingNotify. Go nhanh thi phim toi TRUOC thong bao
    // do, ung dung tra keycode trong ban cu thay NoSymbol va bo luon ky tu.
    const auto it = _charMap.find(codePoint);
    if (it != _charMap.end()) {
        const unsigned int code = it->second.first;
        const bool needShift = it->second.second;
        const bool toggleShift = needShift != _shiftDown;

        X11_LOG("  bom U+%04X qua keycode %u%s", static_cast<unsigned>(codePoint),
                code, needShift ? " (shift)" : "");

        if (toggleShift && _shiftKeycode) {
            sendKeycode(_shiftKeycode, needShift);
        }
        tapKeycode(code);
        if (toggleShift && _shiftKeycode) {
            sendKeycode(_shiftKeycode, !needShift);
        }
        return;
    }

    // Duong lui cho ky tu ngoai bang (bang ma cu TCVN3, VNI-Windows...). Van
    // phai doi ban do phim nen van con race condition nhu tren, nhung cac ky tu nay
    // hiem, va luan phien qua vai ma trong de giam va cham.
    if (_nextSpare >= _spareKeycodes.size()) {
        return;
    }
    const KeySym sym = keysymFor(codePoint);
    const unsigned int code = _spareKeycodes[_nextSpare];
    _nextSpare++;
    if (_nextSpare >= _spareKeycodes.size()) {
        _nextSpare = _charMap.empty() ? 0 : _spareKeycodes.size() - 4;
    }
    KeySym symbols[2] = {sym, sym};
    XChangeKeyboardMapping(_control, static_cast<int>(code), 2, symbols, 1);
    XSync(_control, False);

    X11_LOG("  bom U+%04X qua keycode %u (duong lui)",
            static_cast<unsigned>(codePoint), code);
    tapKeycode(code);
    XSync(_control, False);
}

void X11Backend::flushPendingText() {
    if (!_hasPendingText) {
        return;
    }
    _hasPendingText = false;
    for (char32_t cp : _pendingText) {
        typeCodePoint(cp);
    }
    _pendingText.clear();
    flush();
}

void X11Backend::sendResult(const DeleteRequest& del, const std::u32string& out) {
    X11_LOG("sendResult: xoa %u phim roi chen \"%s\"", del.keyPresses,
            utf8Encode(out).c_str());

    // Con chu cua lan truoc chua ra thi phai ra truoc, giu dung thu tu.
    flushPendingText();

    if (del.keyPresses == 0 || out.empty()) {
        sendBackspaces(del.keyPresses);
        for (char32_t cp : out) {
            typeCodePoint(cp);
        }
        flush();
        return;
    }

    // Xoa truoc, con chu moi de sang nhip sau (~5ms). Ban than X giao du ca hai
    // (da do bang xev), nhung ung dung doc ban phim theo tung cum: don ca "xoa
    // xoa + chu moi" vao mot cum thi bo phan tich phim cua mot so o nhap — nhat
    // la cac TUI — chi lay duoc phan dau va rung ky tu cuoi. Tach lam hai cum
    // thi moi cum deu gon va khong con rung.
    sendBackspaces(del.keyPresses);
    flush();
    _pendingText = out;
    _hasPendingText = true;
}

std::string X11Backend::focusedAppId() {
    if (!_control) {
        return {};
    }

    Window focus = None;
    int revert = 0;
    XGetInputFocus(_control, &focus, &revert);
    if (focus == None || focus == PointerRoot) {
        return {};
    }

    // WM_CLASS cua X11 la tuong duong gan nhat cua app-id tren Wayland.
    XClassHint hint;
    std::memset(&hint, 0, sizeof(hint));
    std::string appId;
    if (XGetClassHint(_control, focus, &hint)) {
        if (hint.res_class) {
            appId = hint.res_class;
            XFree(hint.res_class);
        }
        if (hint.res_name) {
            XFree(hint.res_name);
        }
    }
    return appId;
}

std::unique_ptr<IBackend> makeX11Backend(std::string& error) {
    auto backend = std::make_unique<X11Backend>();
    if (!backend->open(error)) {
        return nullptr;
    }
    return backend;
}

} // namespace openkey
