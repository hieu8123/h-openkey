//
//  Config.h
//  OpenKey cho Linux
//
//  Cau hinh nam o ~/.config/openkey/. File config.json chi chua cac tuy chon
//  dang so cua engine; bang go tat va bang nho ma theo ung dung dung thang
//  dinh dang nhi phan san co cua engine (Macro.cpp, SmartSwitchKey.cpp) nen
//  khong phai viet lai bo tuan tu hoa.
//

#ifndef OPENKEY_LINUX_CONFIG_H
#define OPENKEY_LINUX_CONFIG_H

#include <string>

#include "Backend.h"

namespace openkey {

class Config {
public:
    // Thu muc ~/.config/openkey (ton trong XDG_CONFIG_HOME). Tu tao neu chua co.
    static std::string configDir();

    // Nap config.json vao cac bien engine. Thieu file thi giu nguyen mac dinh
    // va ghi ra mot file moi. Tra ve false neu doc duoc file nhung noi dung hong.
    bool load();

    // Ghi toan bo trang thai hien tai xuong dia.
    bool save() const;

    // Nap va ghi bang go tat, bang nho ma theo ung dung.
    void loadMacroTable() const;
    void saveMacroTable() const;
    void loadSmartSwitchTable() const;
    void saveSmartSwitchTable() const;

    BackendKind backend = BackendKind::Driver;

private:
    std::string configPath() const;
    std::string macroPath() const;
    std::string smartSwitchPath() const;
};

} // namespace openkey

#endif // OPENKEY_LINUX_CONFIG_H
