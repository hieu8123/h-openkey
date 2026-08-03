//
//  EvdevKeyboard.cpp
//  OpenKey cho Linux
//

#include "EvdevKeyboard.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace openkey {
namespace {

// evdev + 8 = keycode X11, quy uoc dung chung toan bo phan con lai (xem
// platforms/linux.h va comment o WaylandBackend.cpp).
constexpr uint32_t kEvdevToX11 = 8;

bool bitSet(const unsigned long* bits, int bit) {
    constexpr int kBitsPerLong = sizeof(unsigned long) * 8;
    return (bits[bit / kBitsPerLong] >> (bit % kBitsPerLong)) & 1;
}

} // namespace

EvdevKeyboard::~EvdevKeyboard() { stop(); }

bool EvdevKeyboard::looksLikeKeyboard(int fd) const {
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

bool EvdevKeyboard::tryAddDevice(const std::string& path, bool* permissionDenied) {
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

    _devices.push_back({fd, path});
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
}

void EvdevKeyboard::rescan() {
    DIR* dir = opendir("/dev/input");
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
        tryAddDevice(std::string("/dev/input/") + entry->d_name, nullptr);
    }
    closedir(dir);
}

bool EvdevKeyboard::start(std::string& error) {
    DIR* dir = opendir("/dev/input");
    if (!dir) {
        error = "khong mo duoc /dev/input";
        return false;
    }

    _epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (_epollFd < 0) {
        closedir(dir);
        error = "khong tao duoc epoll";
        return false;
    }

    bool anyPermissionDenied = false;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
        tryAddDevice(std::string("/dev/input/") + entry->d_name, &anyPermissionDenied);
    }
    closedir(dir);

    if (_devices.empty()) {
        close(_epollFd);
        _epollFd = -1;
        error = anyPermissionDenied
            ? "khong co quyen doc /dev/input/event*: them user vao group "
              "'input' (hoac cai udev rule cua goi cai dat) roi dang nhap lai"
            : "khong tim thay thiet bi ban phim nao trong /dev/input";
        return false;
    }
    return true;
}

void EvdevKeyboard::stop() {
    for (const Device& d : _devices) {
        // Tra lai quyen doc cho compositor/X server truoc khi dong, neu khong
        // ban phim vat ly se "bien mat" cho toi khi rut cam lai.
        ioctl(d.fd, EVIOCGRAB, 0);
        close(d.fd);
    }
    _devices.clear();
    if (_epollFd >= 0) {
        close(_epollFd);
        _epollFd = -1;
    }
}

void EvdevKeyboard::dispatchEvents() {
    if (_epollFd < 0) return;

    struct epoll_event events[16];
    const int n = epoll_wait(_epollFd, events, 16, 0);
    for (int i = 0; i < n; i++) {
        const int fd = events[i].data.fd;

        if (events[i].events & (EPOLLERR | EPOLLHUP)) {
            removeDevice(fd);
            continue;
        }

        bool dead = false;
        for (;;) {
            struct input_event ev;
            const ssize_t got = read(fd, &ev, sizeof(ev));
            if (got == static_cast<ssize_t>(sizeof(ev))) {
                if (ev.type == EV_KEY) {
                    handleEvent(ev);
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
        }
    }
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
