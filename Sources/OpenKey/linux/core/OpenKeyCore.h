//
//  OpenKeyCore.h
//  OpenKey cho Linux
//
//  Cau noi giua engine dung chung va backend. Khong biet gi ve Wayland, X11 hay
//  Qt, nen kiem thu duoc tron ven bang FakeBackend.
//

#ifndef OPENKEY_LINUX_OPENKEYCORE_H
#define OPENKEY_LINUX_OPENKEYCORE_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "Backend.h"

struct vKeyHookState;

namespace openkey {

class OpenKeyCore {
public:
    explicit OpenKeyCore(IBackend& backend);

    // Cai dat key handler cho backend. Goi mot lan sau khi backend.start().
    void attach();

    KeyVerdict onKey(const KeyEvent& ev);

    // Ung dung dang focus doi. Dung cho Smart Switch Key va de bat dau tu moi.
    void onFocusChanged(const std::string& appId);

    // Nguoi dung bam chuot: con tro nhay cho khac, tu dang go khong con lien tuc.
    void onMouseDown();

    void toggleLanguage();

    // Tam ngung khi chinh cua so OpenKey dang nhan focus, tranh vong lap phan hoi.
    void setSuspended(bool suspended);

    // Ghi nho ngon ngu va bang ma hien tai cho ung dung dang focus. Goi moi khi
    // nguoi dung tu doi, de lan sau quay lai ung dung do thi khoi phai doi nua.
    void rememberCurrentApp();

    // Bao cho giao dien biet trang thai vua doi (vi du Smart Switch Key vua tu
    // chuyen ngon ngu), de bieu tuong khay cap nhat theo.
    std::function<void()> onStateChanged;

    // Xoa ky uc ve nhung gi da nam trong tu hien tai. Goi khi khong con chac
    // trang thai cua o nhap con khop voi ky uc cua minh.
    void resetTypingState();

private:
    // Mot ky tu logic ma engine coi la mot don vi, kem chi phi thuc te cua no
    // trong o nhap cua ung dung.
    struct SentChar {
        uint8_t utf8Bytes = 1;
        uint8_t units = 1; // so lan BackSpace can de xoa no
    };

    bool matchSwitchKey(const KeyEvent& ev) const;
    bool modifiersMatchSwitchKey(const KeyEvent& ev) const;

    // Xu ly loai phim tat chi gom phim bo tro (vi du Ctrl+Shift): khong co
    // phim chinh nao nen phai bat luc NHA ra, va chi khi khong co phim nao
    // khac duoc bam xen vao giua.
    bool handleModifierOnlySwitchKey(const KeyEvent& ev);

    // Duyet charData/macroData, dung chuoi ra va ghi chi phi tung ky tu.
    void appendEngineChar(uint32_t data, std::u32string& text,
                          std::vector<SentChar>& costs) const;

    void emitResult(int backspaceCount, const std::u32string& text,
              const std::vector<SentChar>& costs);

    IBackend& _backend;
    vKeyHookState* _hook = nullptr;

    // Chi phi cua tung ky tu logic hien nam trong tu dang go. Nho no ma tinh
    // duoc so byte can xoa, thay vi doan.
    std::vector<SentChar> _sent;

    std::string _focusedAppId;
    bool _suspended = false;
    bool _switchKeyArmed = false;
    std::recursive_mutex _mutex;

    // Neu khoang lang giua hai phim qua lau, rat co the nguoi dung da chuyen
    // sang mot o nhap khac roi quay lai ma khong co su kien doi focus nao bao
    // (vi du doi tab trong cung mot cua so terminal/TUI) — bo dem tu dang go
    // luc do khong con dung voi thuc te tren man hinh nua.
    std::chrono::steady_clock::time_point _lastKeyTime;
    bool _hasLastKeyTime = false;
};

} // namespace openkey

#endif // OPENKEY_LINUX_OPENKEYCORE_H
