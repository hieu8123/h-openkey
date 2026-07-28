//
//  FakeBackend.h
//  OpenKey cho Linux — kiem thu
//
//  Gia lap o nhap van ban cua mot ung dung: giu mot bo dem UTF-8, ap dung moi
//  lenh xoa/chen ma core gui xuong. Nho vay kiem tra duoc ket qua go cuoi cung
//  chu khong chi kiem tra tung loi goi rieng le.
//

#ifndef OPENKEY_LINUX_FAKEBACKEND_H
#define OPENKEY_LINUX_FAKEBACKEND_H

#include <string>

#include "Backend.h"
#include "CharCodec.h"
#include "Vietnamese.h"

namespace openkey {

class FakeBackend : public IBackend {
public:
    explicit FakeBackend(bool surroundingText = true) {
        _caps.hasSurroundingText = surroundingText;
        _caps.hasAppId = true;
        _caps.canForwardKey = true;
    }

    const char* name() const override { return "fake"; }
    BackendCaps caps() const override { return _caps; }
    bool start() override { return true; }
    void stop() override {}
    const std::string& lastError() const override { return _error; }
    std::string focusedAppId() override { return _appId; }

    void sendResult(const DeleteRequest& del, const std::u32string& out) override {
        deleteCalls++;
        // Bo dem cua ung dung tinh theo byte, dung nhu delete_surrounding_text.
        const size_t n = del.utf8Bytes > buffer.size() ? buffer.size() : del.utf8Bytes;
        buffer.erase(buffer.size() - n);
        buffer += utf8Encode(out);
    }

    void forwardKey(const KeyEvent& ev) override {
        forwardCalls++;
        const uint16_t ch = keyCodeToCharacter(
            ev.keycode | ((ev.shift || ev.capsLock) ? CAPS_MASK : 0));
        if (ch != 0) {
            utf8Append(buffer, static_cast<char32_t>(ch));
        }
    }

    // Chuyen tiep phim theo dung phan xu cua core, giong vong lap that.
    void feed(const KeyEvent& ev) {
        if (_handler && _handler(ev) == KeyVerdict::Forward) {
            forwardKey(ev);
        }
    }

    std::string buffer;
    int deleteCalls = 0;
    int forwardCalls = 0;

private:
    BackendCaps _caps;
    std::string _error;
    std::string _appId = "test.app";
};

} // namespace openkey

#endif // OPENKEY_LINUX_FAKEBACKEND_H
