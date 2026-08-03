//
//  EvdevKeyboard.h
//  OpenKey cho Linux
//
//  Chan phim vat ly thang o tang kernel bang EVIOCGRAB, dung chung cho ca
//  backend Wayland lan X11. Khac voi XRecord (chi QUAN SAT, phim goc da toi
//  ung dung roi) hay zwp_input_method_v2_grab_keyboard (COSMIC tu choi moi
//  popup grab khi dang co keyboard grab nao do tren seat, xem cosmic-comp
//  #1763), EVIOCGRAB chan that o kernel: sau khi grab, KHONG con ai khac -
//  khong X server, khong Wayland compositor - nhan duoc su kien cua thiet bi
//  do nua. Doi lai, ta phai tu lam moi thu ma compositor/X server truoc day
//  lam ho: theo doi trang thai Shift/Ctrl/Alt/Super/CapsLock, va tu goi
//  forwardKey() de go lai nhung phim khong can sua.
//
//  Can quyen doc/ghi /dev/input/eventX va /dev/uinput. Xem packaging/udev
//  de biet cach cap quyen mot lan luc cai dat.

#ifndef OPENKEY_LINUX_EVDEVKEYBOARD_H
#define OPENKEY_LINUX_EVDEVKEYBOARD_H

#include <linux/input.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace openkey {

struct EvdevKeyEvent {
    uint32_t x11Keycode = 0; // evdev + 8, dung quy uoc chung voi phan con lai
    bool pressed = false;
    // Su kien tu lap do kernel sinh ra khi giu phim (value=2), khong phai mot
    // lan bam moi. X server tu sinh lap cho phim no thay dang giu, nen neu ta
    // chuyen tiep ca cai nay nua thi ung dung nhan hai lan.
    bool repeat = false;
    bool shift = false;
    bool capsLock = false;
    bool ctrl = false;
    bool alt = false;
    bool super = false;
};

class EvdevKeyboard {
public:
    ~EvdevKeyboard();

    // Do va grab moi thiet bi trong /dev/input/event* co dang la ban phim
    // that (co day du phim chu cai). Tra ve false kem ly do neu khong tim
    // duoc thiet bi nao hoac thieu quyen truy cap.
    bool start(std::string& error);
    void stop();

    // Mot epoll fd duy nhat gom tat ca ban phim vat ly, de ghep vao vong lap
    // su kien cua ung dung ma khong can doi interface IBackend.
    int epollFd() const { return _epollFd; }
    // Goi khi epollFd() san sang doc. Doc het moi thiet bi dang co du lieu.
    void dispatchEvents();

    // Do lai /dev/input de bat nhung ban phim vua duoc cam vao. Ban phim khong
    // day va dongle USB rat hay ngat roi hien lai duoi mot node khac; khong do
    // lai thi tu luc do tro di phim di thang ra ngoai, khong qua bo go nua.
    void rescan();

    std::function<void(const EvdevKeyEvent&)> onKey;

private:
    struct Device {
        int fd = -1;
        std::string path;
    };

    bool looksLikeKeyboard(int fd) const;
    void handleEvent(const struct input_event& ev);
    bool tryAddDevice(const std::string& path, bool* permissionDenied);
    void removeDevice(int fd);

    std::vector<Device> _devices;
    int _epollFd = -1;

    // Compositor/X server khong con thay phim bo tro nua sau khi grab, nen
    // phai tu dem lay: khong ai khac lam ho duoc.
    bool _shift = false;
    bool _ctrl = false;
    bool _alt = false;
    bool _super = false;
    bool _capsLock = false;
};

} // namespace openkey

#endif // OPENKEY_LINUX_EVDEVKEYBOARD_H
