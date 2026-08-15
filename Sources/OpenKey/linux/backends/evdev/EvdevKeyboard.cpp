//
//  EvdevKeyboard.cpp
//  OpenKey cho Linux
//

#include "EvdevKeyboard.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace openkey {
namespace {

// evdev + 8 = quy uoc keycode ma engine Linux da dung trong platforms/linux.h.
constexpr uint32_t kEvdevToX11 = 8;

bool bitSet(const unsigned long* bits, int bit) {
    constexpr int kBitsPerLong = sizeof(unsigned long) * 8;
    return (bits[bit / kBitsPerLong] >> (bit % kBitsPerLong)) & 1;
}

void setBit(unsigned long* bits, int bit) {
    constexpr int kBitsPerLong = sizeof(unsigned long) * 8;
    bits[bit / kBitsPerLong] |= 1UL << (bit % kBitsPerLong);
}

bool hasHeldKeys(int fd) {
    unsigned long state[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    if (ioctl(fd, EVIOCGKEY(sizeof(state)), state) < 0) return true;
    for (int code = 0; code <= KEY_MAX; ++code) {
        if (bitSet(state, code)) return true;
    }
    return false;
}

bool looksLikePointer(int fd) {
    unsigned long bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0) return false;
    return bitSet(bits, BTN_LEFT) || bitSet(bits, BTN_RIGHT) ||
           bitSet(bits, BTN_MIDDLE);
}

bool isPointerButton(uint16_t code) {
    return code >= BTN_LEFT && code <= BTN_TASK;
}

bool keepOnlyPointerButtons(int fd) {
    unsigned long keyMask[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    for (int code = BTN_LEFT; code <= BTN_TASK; ++code) {
        setBit(keyMask, code);
    }

    struct input_mask mask {};
    mask.type = EV_KEY;
    mask.codes_size = sizeof(keyMask);
    mask.codes_ptr = reinterpret_cast<uintptr_t>(keyMask);
    if (ioctl(fd, EVIOCSMASK, &mask) < 0) return false;

    // Mask la theo tung file descriptor. Xoa tat ca event type con lai de
    // REL/ABS/SYN tu chuot va touchpad khong danh thuc event loop ban phim.
    unsigned long empty = 0;
    mask.codes_size = sizeof(empty);
    mask.codes_ptr = reinterpret_cast<uintptr_t>(&empty);
    for (int type = 0; type <= EV_MAX; ++type) {
        if (type == EV_KEY) continue;
        mask.type = static_cast<uint32_t>(type);
        if (ioctl(fd, EVIOCSMASK, &mask) < 0) return false;
    }
    return true;
}

} // namespace

EvdevKeyboard::~EvdevKeyboard() { stop(); }

bool EvdevKeyboard::looksLikeKeyboard(int fd) const {
    char name[256] = {};
    if (!_ignoredDeviceName.empty() &&
        ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 &&
        _ignoredDeviceName == name) {
        return false;
    }

    // Chi coi la ban phim "that" neu co day du phim chu cai — loai duoc chuot,
    // volume key rieng le, va chinh cac thiet bi ao (virtual keyboard/uinput)
    // ma backend khac cua ta tao ra, vi chung thuong chi co mot vai keycode.
    unsigned long bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0) {
        return false;
    }
    for (int k = KEY_Q; k <= KEY_P; k++) {
        if (!bitSet(bits, k)) return false;
    }
    return bitSet(bits, KEY_SPACE) && bitSet(bits, KEY_ENTER);
}

bool EvdevKeyboard::tryAddDevice(const std::string& path, bool* permissionDenied,
                                 bool* forbiddenCollision, bool* heldKeys) {
    // Da giu thiet bi nay roi thi thoi. So sanh theo duong dan vi do la thu duy
    // nhat on dinh giua hai lan do.
    for (const Device& d : _devices) {
        if (d.path == path) return false;
    }

    const int fd = open(path.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        if (errno == EACCES && permissionDenied) *permissionDenied = true;
        return false;
    }
    if (!looksLikeKeyboard(fd)) {
        close(fd);
        return false;
    }

    if (!_forbiddenCodes.empty()) {
        unsigned long bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0) {
            close(fd);
            return false;
        }
        for (uint16_t code : _forbiddenCodes) {
            if (code <= KEY_MAX && bitSet(bits, code)) {
                if (forbiddenCollision) *forbiddenCollision = true;
                close(fd);
                return false;
            }
        }
    }

    // Khong grab giua mot lan nhan/nha. Van theo doi fd de chinh su kien release
    // cuoi cung danh thuc epoll; nhu vay khong can polling hay doi hotplug.
    if (hasHeldKeys(fd)) {
        if (heldKeys) *heldKeys = true;
        struct epoll_event pending {};
        pending.events = EPOLLIN;
        pending.data.fd = fd;
        if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &pending) != 0) {
            close(fd);
            return false;
        }
        _devices.push_back({fd, path, false, true});
        publishPendingKeyboardCount();
        return true;
    }
    if (ioctl(fd, EVIOCGRAB, 1) != 0) {
        close(fd);
        return false;
    }

    struct epoll_event ev {};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        ioctl(fd, EVIOCGRAB, 0);
        close(fd);
        return false;
    }

    _devices.push_back({fd, path, true});
    return true;
}

bool EvdevKeyboard::tryGrabPendingDevice(int fd) {
    if (hasHeldKeys(fd) || ioctl(fd, EVIOCGRAB, 1) != 0) return false;
    // Dong cua so dua giua EVIOCGKEY va EVIOCGRAB: neu mot phim vua duoc nhan,
    // bo grab ngay de compositor van nhan release cua lan nhan do.
    if (hasHeldKeys(fd)) {
        ioctl(fd, EVIOCGRAB, 0);
        return false;
    }
    for (Device& device : _devices) {
        if (device.fd != fd) continue;
        device.grabbedKeyboard = true;
        device.pendingKeyboard = false;
        publishPendingKeyboardCount();
        return true;
    }
    ioctl(fd, EVIOCGRAB, 0);
    return false;
}

bool EvdevKeyboard::tryAddPointerDevice(const std::string& path) {
    for (const Device& d : _devices) {
        if (d.path == path) return false;
    }

    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) return false;
    char name[256] = {};
    if (!_ignoredDeviceName.empty() &&
        ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 &&
        _ignoredDeviceName == name) {
        close(fd);
        return false;
    }
    if (!looksLikePointer(fd)) {
        close(fd);
        return false;
    }

    // Neu kernel ho tro event mask, chi dua click vao epoll. Neu khong ho tro,
    // van giu fallback cu de context reset hoat dong tren kernel cu.
    keepOnlyPointerButtons(fd);

    struct epoll_event ev {};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        close(fd);
        return false;
    }
    // Chi doc song song de biet luc nao con tro co the doi o nhap. Tuyet doi
    // khong EVIOCGRAB, nen chuot van di thang toi compositor nhu binh thuong.
    _devices.push_back({fd, path, false});
    return true;
}

void EvdevKeyboard::removeDevice(int fd) {
    // Thiet bi da bi rut: fd chet van bao "san sang" mai mai, khong go ra khoi
    // epoll thi vong lap su kien quay 100% CPU va bo go ngung phan hoi.
    epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    for (size_t i = 0; i < _devices.size(); i++) {
        if (_devices[i].fd == fd) {
            _devices.erase(_devices.begin() + static_cast<long>(i));
            break;
        }
    }
    publishPendingKeyboardCount();
}

void EvdevKeyboard::publishPendingKeyboardCount() {
    size_t count = 0;
    for (const Device& device : _devices) {
        if (device.pendingKeyboard) ++count;
    }
    if (count == _lastPendingKeyboardCount) return;
    _lastPendingKeyboardCount = count;
    if (onPendingKeyboardCountChanged) {
        onPendingKeyboardCountChanged(count);
    }
}

void EvdevKeyboard::rescan() {
    DIR* dir = opendir("/dev/input");
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
        const std::string path = std::string("/dev/input/") + entry->d_name;
        tryAddDevice(path, nullptr, nullptr, nullptr);
        tryAddPointerDevice(path);
    }
    closedir(dir);
}

bool EvdevKeyboard::start(std::string& error,
                          const std::vector<uint16_t>& forbiddenCodes,
                          const std::string& ignoredDeviceName) {
    _forbiddenCodes = forbiddenCodes;
    _ignoredDeviceName = ignoredDeviceName;
    DIR* dir = opendir("/dev/input");
    if (!dir) {
        error = "không mở được /dev/input";
        return false;
    }

    _epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (_epollFd < 0) {
        closedir(dir);
        error = "không tạo được epoll";
        return false;
    }

    _wakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    struct epoll_event wakeEvent {};
    wakeEvent.events = EPOLLIN;
    wakeEvent.data.fd = _wakeFd;
    if (_wakeFd < 0 ||
        epoll_ctl(_epollFd, EPOLL_CTL_ADD, _wakeFd, &wakeEvent) != 0) {
        closedir(dir);
        error = "không tạo được eventfd đánh thức luồng bàn phím";
        stop();
        return false;
    }

    // Khong quet /dev/input theo timer: mot so node HID tren may that lam lan
    // quet mat hang tram ms. Kernel se danh thuc fd nay chi khi thu muc doi.
    _inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (_inotifyFd >= 0) {
        _inputWatch = inotify_add_watch(
            _inotifyFd, "/dev/input", IN_CREATE | IN_MOVED_TO | IN_DELETE);
        struct epoll_event notifyEvent {};
        notifyEvent.events = EPOLLIN;
        notifyEvent.data.fd = _inotifyFd;
        if (_inputWatch < 0 ||
            epoll_ctl(_epollFd, EPOLL_CTL_ADD, _inotifyFd, &notifyEvent) != 0) {
            if (_inputWatch >= 0) inotify_rm_watch(_inotifyFd, _inputWatch);
            close(_inotifyFd);
            _inotifyFd = -1;
            _inputWatch = -1;
        }
    }

    bool anyPermissionDenied = false;
    bool forbiddenCollision = false;
    bool heldKeys = false;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
        const std::string path = std::string("/dev/input/") + entry->d_name;
        tryAddDevice(path, &anyPermissionDenied, &forbiddenCollision, &heldKeys);
        tryAddPointerDevice(path);
    }
    closedir(dir);

    if (forbiddenCollision) {
        stop();
        error = "bàn phím thật dùng trùng keycode dành riêng của H-OpenKey; "
                "đã từ chối grab để tránh làm mất phím";
        return false;
    }

    bool hasKeyboard = false;
    for (const Device& device : _devices) {
        hasKeyboard = hasKeyboard || device.grabbedKeyboard ||
                      device.pendingKeyboard;
    }
    if (!hasKeyboard) {
        stop();
        error = heldKeys
            ? "đang có phím được giữ; hãy nhả hết phím rồi bật lại H-OpenKey"
            : anyPermissionDenied
            ? "không có quyền đọc /dev/input/event*: thêm user vào nhóm "
              "'input' (hoặc cài udev rule của gói cài đặt) rồi đăng nhập lại"
            : "không tìm thấy thiết bị bàn phím nào trong /dev/input";
        return false;
    }
    return true;
}

void EvdevKeyboard::stop() {
    for (const Device& d : _devices) {
        // Tra lai quyen doc cho compositor/X server truoc khi dong, neu khong
        // ban phim vat ly se "bien mat" cho toi khi rut cam lai.
        if (d.grabbedKeyboard) ioctl(d.fd, EVIOCGRAB, 0);
        close(d.fd);
    }
    _devices.clear();
    publishPendingKeyboardCount();
    if (_inotifyFd >= 0) {
        if (_inputWatch >= 0) inotify_rm_watch(_inotifyFd, _inputWatch);
        close(_inotifyFd);
        _inotifyFd = -1;
        _inputWatch = -1;
    }
    if (_wakeFd >= 0) {
        close(_wakeFd);
        _wakeFd = -1;
    }
    if (_epollFd >= 0) {
        close(_epollFd);
        _epollFd = -1;
    }
}

void EvdevKeyboard::dispatchEvents() {
    waitAndDispatch(0);
}

void EvdevKeyboard::wake() {
    if (_wakeFd < 0) return;
    const uint64_t value = 1;
    const ssize_t ignored = write(_wakeFd, &value, sizeof(value));
    (void)ignored;
}

void EvdevKeyboard::waitAndDispatch(int timeoutMs) {
    if (_epollFd < 0) return;

    struct epoll_event events[16];
    const int n = epoll_wait(_epollFd, events, 16, timeoutMs);
    if (n <= 0) return;
    if (onDispatchState) onDispatchState(true);
    bool needsRescan = false;
    for (int i = 0; i < n; i++) {
        const int fd = events[i].data.fd;

        if (fd == _wakeFd) {
            uint64_t value;
            while (read(fd, &value, sizeof(value)) > 0) {}
            continue;
        }

        if (fd == _inotifyFd) {
            alignas(struct inotify_event) char buffer[4096];
            for (;;) {
                const ssize_t got = read(fd, buffer, sizeof(buffer));
                if (got <= 0) break;
                size_t offset = 0;
                while (offset + sizeof(struct inotify_event) <=
                       static_cast<size_t>(got)) {
                    const auto* changed = reinterpret_cast<const struct inotify_event*>(
                        buffer + offset);
                    if (changed->len > 0 &&
                        std::strncmp(changed->name, "event", 5) == 0 &&
                        (changed->mask & (IN_CREATE | IN_MOVED_TO))) {
                        needsRescan = true;
                    }
                    offset += sizeof(struct inotify_event) + changed->len;
                }
            }
            continue;
        }

        bool grabbedKeyboard = false;
        bool pendingKeyboard = false;
        for (const Device& device : _devices) {
            if (device.fd == fd) {
                grabbedKeyboard = device.grabbedKeyboard;
                pendingKeyboard = device.pendingKeyboard;
                break;
            }
        }

        if (events[i].events & (EPOLLERR | EPOLLHUP)) {
            removeDevice(fd);
            continue;
        }

        bool dead = false;
        for (;;) {
            struct input_event ev;
            const ssize_t got = read(fd, &ev, sizeof(ev));
            if (got == static_cast<ssize_t>(sizeof(ev))) {
                if (onDispatchState) onDispatchState(true);
                if (ev.type == EV_KEY && grabbedKeyboard) {
                    handleEvent(ev);
                } else if (ev.type == EV_KEY && ev.value == 1 &&
                           isPointerButton(ev.code) && onContextBreak) {
                    onContextBreak();
                }
                continue;
            }
            // EAGAIN la het du lieu — binh thuong. Con lai (nhat la ENODEV khi
            // ban phim vua bi rut) nghia la fd nay da chet, phai go bo ngay chu
            // khong se bao "san sang" mai mai va quay vong vo tan.
            if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }
            dead = true;
            break;
        }
        if (dead) {
            removeDevice(fd);
        } else if (pendingKeyboard) {
            tryGrabPendingDevice(fd);
        }
    }
    if (needsRescan) rescan();
    if (onDispatchState) onDispatchState(false);
}

void EvdevKeyboard::handleEvent(const struct input_event& ev) {
    // value: 0 = nha, 1 = bam, 2 = tu lap (giu phim). Coi tu lap nhu bam lai,
    // giong hanh vi go-lap ma X server/compositor van lam khi con quan ly
    // ban phim nay.
    const bool pressed = ev.value != 0;
    const bool isEdge = ev.value != 2;

    switch (ev.code) {
        case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT:
            if (isEdge) _shift = pressed;
            break;
        case KEY_LEFTCTRL: case KEY_RIGHTCTRL:
            if (isEdge) _ctrl = pressed;
            break;
        case KEY_LEFTALT: case KEY_RIGHTALT:
            if (isEdge) _alt = pressed;
            break;
        case KEY_LEFTMETA: case KEY_RIGHTMETA:
            if (isEdge) _super = pressed;
            break;
        case KEY_CAPSLOCK:
            if (ev.value == 1) _capsLock = !_capsLock;
            break;
        default:
            break;
    }

    if (!onKey) return;

    EvdevKeyEvent out;
    out.x11Keycode = static_cast<uint32_t>(ev.code) + kEvdevToX11;
    out.pressed = pressed;
    out.repeat = ev.value == 2;
    out.shift = _shift;
    out.capsLock = _capsLock;
    out.ctrl = _ctrl;
    out.alt = _alt;
    out.super = _super;
    onKey(out);
}

} // namespace openkey
