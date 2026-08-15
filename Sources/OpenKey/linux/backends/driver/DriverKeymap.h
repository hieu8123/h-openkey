//
//  DriverKeymap.h
//  Bang ma phim rieng cho ban phim ao H-OpenKey.
//

#ifndef OPENKEY_LINUX_DRIVER_KEYMAP_H
#define OPENKEY_LINUX_DRIVER_KEYMAP_H

#include <cstdint>
#include <string>
#include <vector>

namespace openkey {

struct DriverKeyStroke {
    uint16_t evdevCode = 0;
    bool shift = false;
    bool level3 = false;
    bool level5 = false;
};

// Phim ao dung lam ISO_Level3_Shift. Day la mot ma ma moi ban phim vat ly tren
// may deu phai KHONG ho tro; DriverBackend kiem tra dieu do truoc khi grab.
uint16_t driverLevel3Keycode();

// Phim ao thu hai dung lam ISO_Level5_Shift. Hai modifier ket hop voi Shift
// tao sau level Unicode tren moi phim chu carrier.
uint16_t driverLevel5Keycode();

// Hai ma evdev ma layout H-OpenKey chiem dung cho Level3 va Level5. Cac phim
// chu carrier van giu nguyen level 1/2 nen khong can chiem keycode rieng.
const std::vector<uint16_t>& driverReservedKeycodes();

// Tra ve cach go mot ky tu qua layout H-OpenKey. ASCII dung vi tri phim US;
// chu tieng Viet dung level 3..8 cua cac phim chu. evdevCode=0 nghia la chua
// ho tro.
DriverKeyStroke driverKeyStrokeFor(char32_t codePoint);

// Noi dung ~/.config/xkb/symbols/hopenkey. Sinh tu cung bang voi ham tren de
// code va layout khong bao gio lech nhau.
std::string driverXkbSymbols();

// Ghi ban symbols dung de staging vao XDG_CONFIG_HOME. Trinh cai dat se chep
// no thanh symbols/custom trong XKB root he thong.
bool installDriverXkbLayout(std::string& error);

// GNOME phai dang dung source xkb "custom"; neu con de source khac thi cac ma
// rieng se bi hieu thanh phim media. Kiem tra truoc khi grab.
bool driverXkbLayoutIsActive(std::string& error);

// Tach rieng de kiem thu hoi quy chuoi "uint32 0" cua gsettings.
bool parseDriverSourceIndex(const std::string& text, size_t& index);

} // namespace openkey

#endif // OPENKEY_LINUX_DRIVER_KEYMAP_H
