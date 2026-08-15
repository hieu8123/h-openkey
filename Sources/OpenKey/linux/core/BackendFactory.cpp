//
//  BackendFactory.cpp
//  OpenKey cho Linux
//
//  Ban Linux chi con mot backend driver evdev/uinput.
//

#include "Backend.h"

#ifdef OPENKEY_HAVE_DRIVER
#include "../backends/driver/DriverBackend.h"
#endif

namespace openkey {

BackendKind backendKindFromString(const std::string& s) {
    (void)s; // cau hinh cu cung duoc nang cap thang sang driver
    return BackendKind::Driver;
}

const char* backendKindToString(BackendKind k) {
    (void)k;
    return "driver";
}

namespace {

// Backend không làm gì cả. Điểm mấu chốt là nó KHÔNG đụng tới bàn phím: không
// EVIOCGRAB, không grab của compositor. Nhờ vậy khi không gõ được tiếng Việt thì
// người dùng vẫn gõ được bình thường mọi thứ khác.
class NullBackend final : public IBackend {
public:
    const char* name() const override { return "khong-go-duoc"; }
    BackendCaps caps() const override { return {}; }
    bool canType() const override { return false; }
    bool start() override { return true; }
    void stop() override {}
    void sendResult(const DeleteRequest&, const std::u32string&) override {}
    void forwardKey(const KeyEvent&) override {}
    std::string focusedAppId() override { return {}; }
    const std::string& lastError() const override { return _error; }

private:
    std::string _error;
};

} // namespace

std::unique_ptr<IBackend> makeNullBackend() { return std::make_unique<NullBackend>(); }

std::unique_ptr<IBackend> createBackend(BackendKind requested, std::string& notice) {
    (void)requested;
    notice.clear();
#ifdef OPENKEY_HAVE_DRIVER
    std::string error;
    if (auto backend = makeDriverBackend(error)) return backend;
    notice = error;
#else
    notice = "bản build này không kèm driver evdev/uinput";
#endif
    return makeNullBackend();
}

} // namespace openkey
