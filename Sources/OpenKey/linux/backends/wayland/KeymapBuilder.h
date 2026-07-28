//
//  KeymapBuilder.h
//  OpenKey cho Linux
//
//  Ghep them mot phim cho tung ky tu tieng Viet vao keymap that, roi nap ban
//  ghep do MOT LAN duy nhat.
//
//  Vi sao phai lam vay: de go chu bang ban phim ao, ta can mot ma phim ung voi
//  ky tu muon go. Cach cu la dung keymap tam roi tra keymap that ve ngay (ky
//  thuat cua wtype). Cach do khong dung duoc: ung dung bien dich lai keymap
//  bat dong bo, nen cac phim ta bom vao bi giai ma bang keymap sai — go "a"
//  roi dau se ra chu khac han.
//
//  Ghep san thi khong bao gio phai doi keymap, nen khong con dua tranh nao.
//  Doi lai, cac ma them vao deu tren 255 — X11 chi ho tro toi 255 nen cach nay
//  khong voi tay duoc toi ung dung XWayland; nhung khoang 8..255 chi con dung
//  MOT ma trong nen cung khong co lua chon khac.
//

#ifndef OPENKEY_LINUX_KEYMAPBUILDER_H
#define OPENKEY_LINUX_KEYMAPBUILDER_H

#include <cstdint>
#include <map>
#include <string>

namespace openkey {

class KeymapBuilder {
public:
    // Tra ve false neu khong phan tich duoc keymap that; khi do dung nguyen ban.
    bool build(const std::string& realKeymap);

    const std::string& mergedKeymap() const { return _merged; }

    // Keycode evdev de go ky tu nay, hoac -1 neu khong ghep duoc.
    int evdevKeycodeFor(char32_t codePoint) const;

    size_t mappedCount() const { return _codes.size(); }

private:
    std::string _merged;
    std::map<char32_t, int> _codes; // code point -> keycode evdev
};

} // namespace openkey

#endif // OPENKEY_LINUX_KEYMAPBUILDER_H
