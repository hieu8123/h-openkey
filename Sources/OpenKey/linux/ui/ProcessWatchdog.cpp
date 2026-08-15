// Watchdog nay khong dung mutex cua core/driver va khong poll thiet bi input.

#include "ProcessWatchdog.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "Backend.h"

namespace openkey {
namespace {

int connectNotifySocket() {
    const char* path = std::getenv("NOTIFY_SOCKET");
    if (!path || !*path) return -1;

    const int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;

    struct sockaddr_un address {};
    address.sun_family = AF_UNIX;
    const size_t length = std::strlen(path);
    if (length == 0 || length >= sizeof(address.sun_path)) {
        close(fd);
        return -1;
    }
    socklen_t addressLength;
    if (path[0] == '@') {
        address.sun_path[0] = '\0';
        std::memcpy(address.sun_path + 1, path + 1, length - 1);
        // Abstract socket khong co NUL ket thuc; moi byte deu la mot phan ten.
        addressLength = static_cast<socklen_t>(
            offsetof(struct sockaddr_un, sun_path) + length);
    } else {
        std::memcpy(address.sun_path, path, length + 1);
        addressLength = static_cast<socklen_t>(
            offsetof(struct sockaddr_un, sun_path) + length + 1);
    }
    if (connect(fd, reinterpret_cast<struct sockaddr*>(&address),
                addressLength) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

} // namespace

ProcessWatchdog::ProcessWatchdog(const IBackend& backend) : _backend(backend) {}

ProcessWatchdog::~ProcessWatchdog() { stop(); }

void ProcessWatchdog::start() {
    if (_running.exchange(true)) return;
    try {
        _thread = std::thread([this] { run(); });
    } catch (...) {
        _running = false;
        // Khong tiep tuc giu EVIOCGRAB neu co che cuu ho cung khong khoi dong.
        std::abort();
    }
}

void ProcessWatchdog::stop() {
    _running = false;
    if (_thread.joinable()) _thread.join();
}

void ProcessWatchdog::run() {
    const int notifyFd = connectNotifySocket();
    constexpr char kHeartbeat[] = "WATCHDOG=1";

    while (_running.load(std::memory_order_acquire)) {
        if (!_backend.watchdogHealthy()) {
            // Chap nhan process chet de kernel dong fd evdev/uinput va nha grab.
            // Khong ghi log o day: write/stdio cung co the chinh la thu dang ket.
            std::abort();
        }
        if (notifyFd >= 0) {
            send(notifyFd, kHeartbeat, sizeof(kHeartbeat) - 1, MSG_NOSIGNAL);
        }
        // Day la nhip song cua watchdog, khong phai polling ban phim. Chia nho
        // de stop() khong phai doi ca nua giay.
        for (int i = 0; i < 5 && _running.load(std::memory_order_acquire); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    if (notifyFd >= 0) close(notifyFd);
}

} // namespace openkey
