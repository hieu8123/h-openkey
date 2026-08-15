// Kiem thu bang phim ao + layout XKB cua driver evdev/uinput.

#include <linux/input.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "DriverKeymap.h"
#include "UInputKeyboard.h"

namespace {

int failures = 0;

void check(bool ok, const char* message) {
    if (ok) return;
    std::printf("FAIL: %s\n", message);
    failures++;
}

std::vector<char32_t> vietnameseCharacters() {
    std::vector<char32_t> chars;
    for (char32_t cp = 0x1EA0; cp <= 0x1EF9; ++cp) chars.push_back(cp);
    static const char32_t extra[] = {
        0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C8, 0x00C9, 0x00CA, 0x00CC,
        0x00CD, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D9, 0x00DA, 0x00DD,
        0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E8, 0x00E9, 0x00EA, 0x00EC,
        0x00ED, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F9, 0x00FA, 0x00FD,
        0x0102, 0x0103, 0x0110, 0x0111, 0x0128, 0x0129, 0x0168, 0x0169,
        0x01A0, 0x01A1, 0x01AF, 0x01B0,
        0x0300, 0x0301, 0x0302, 0x0303, 0x0306, 0x0309, 0x031B, 0x0323,
    };
    chars.insert(chars.end(), std::begin(extra), std::end(extra));
    return chars;
}

char32_t typeStroke(xkb_state* state, const openkey::DriverKeyStroke& stroke) {
    const xkb_keycode_t shift = KEY_LEFTSHIFT + 8;
    const xkb_keycode_t level3 = openkey::driverLevel3Keycode() + 8;
    const xkb_keycode_t level5 = openkey::driverLevel5Keycode() + 8;
    const xkb_keycode_t key = stroke.evdevCode + 8;
    if (stroke.shift) xkb_state_update_key(state, shift, XKB_KEY_DOWN);
    if (stroke.level3) xkb_state_update_key(state, level3, XKB_KEY_DOWN);
    if (stroke.level5) xkb_state_update_key(state, level5, XKB_KEY_DOWN);
    // Compositor tra keysym theo trang thai TRUOC khi cap nhat chinh phim dang
    // xu ly; day cung la thu tu libxkbcommon yeu cau cho server.
    const char32_t result = xkb_state_key_get_utf32(state, key);
    xkb_state_update_key(state, key, XKB_KEY_DOWN);
    xkb_state_update_key(state, key, XKB_KEY_UP);
    if (stroke.level5) xkb_state_update_key(state, level5, XKB_KEY_UP);
    if (stroke.level3) xkb_state_update_key(state, level3, XKB_KEY_UP);
    if (stroke.shift) xkb_state_update_key(state, shift, XKB_KEY_UP);
    return result;
}

void testMappings() {
    size_t sourceIndex = 99;
    check(openkey::findDriverSourceIndex(
              "[('xkb', 'us'), ('ibus', 'mozc-jp'), ('xkb', 'custom')]",
              sourceIndex) && sourceIndex == 2,
          "phai tim dung vi tri xkb:custom trong sources GNOME");
    check(!openkey::findDriverSourceIndex(
              "@a(ss) [('xkb', 'us'), ('ibus', 'mozc-jp')]", sourceIndex),
          "khong duoc bao co xkb:custom khi source chua duoc cai");
    const auto& reserved = openkey::driverReservedKeycodes();
    check(reserved.size() == 2, "chi duoc dung hai keycode modifier rieng");
    check(std::set<uint16_t>(reserved.begin(), reserved.end()).size() == reserved.size(),
          "keycode rieng khong duoc trung nhau");

    std::set<std::tuple<uint16_t, bool, bool, bool>> strokes;
    const auto chars = vietnameseCharacters();
    check(chars.size() == 142, "phai bao phu 142 code point tieng Viet");
    for (char32_t cp : chars) {
        const auto stroke = openkey::driverKeyStrokeFor(cp);
        check(stroke.evdevCode != 0, "thieu anh xa mot code point tieng Viet");
        strokes.emplace(stroke.evdevCode, stroke.shift, stroke.level3,
                        stroke.level5);
    }
    check(strokes.size() == chars.size(), "hai ky tu dang dung chung mot level");
    check(openkey::driverKeyStrokeFor(U'A').evdevCode == KEY_A,
          "ASCII hoa phai dung phim vat ly US");
    check(openkey::driverKeyStrokeFor(U'?').evdevCode == KEY_SLASH,
          "dau ASCII Shift phai duoc anh xa");
    check(openkey::driverKeyStrokeFor(0x202F).evdevCode != 0,
          "thieu ky tu dem sua autocomplete U+202F");
}

void testGeneratedXkb() {
    char pattern[] = "/tmp/h-openkey-xkb-XXXXXX";
    const char* made = mkdtemp(pattern);
    check(made != nullptr, "khong tao duoc thu muc XKB tam");
    if (!made) return;

    const std::filesystem::path root = std::filesystem::path(made) / "xkb";
    std::filesystem::create_directories(root / "symbols");
    std::ofstream(root / "symbols" / "hopenkey") << openkey::driverXkbSymbols();

    // Dat thu muc tam len dau de test dung symbols vua sinh, ke ca khi may da
    // cai mot ban hopenkey cu trong /usr/share/X11/xkb.
    xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES);
    check(context != nullptr, "khong tao duoc xkb_context");
    if (!context) return;
    xkb_context_include_path_append(context, root.c_str());
    xkb_context_include_path_append_default(context);

    const xkb_rule_names names = {"evdev", "pc105", "hopenkey", nullptr, nullptr};
    xkb_keymap* keymap = xkb_keymap_new_from_names(
        context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    check(keymap != nullptr, "libxkbcommon khong bien dich duoc layout hopenkey");
    if (keymap) {
        xkb_state* state = xkb_state_new(keymap);
        check(state != nullptr, "khong tao duoc trang thai XKB");
        if (state) {
            for (char32_t cp : vietnameseCharacters()) {
                const char32_t got = typeStroke(state, openkey::driverKeyStrokeFor(cp));
                if (got != cp) {
                    std::printf("FAIL: U+%04X qua XKB thanh U+%04X\n",
                                static_cast<unsigned>(cp),
                                static_cast<unsigned>(got));
                    failures++;
                }
            }
            check(typeStroke(state, openkey::driverKeyStrokeFor(U'Z')) == U'Z',
                  "XKB phai giu dung ASCII hoa");
            check(typeStroke(state, openkey::driverKeyStrokeFor(U'@')) == U'@',
                  "XKB phai giu dung dau ASCII Shift");
            check(typeStroke(state, openkey::driverKeyStrokeFor(0x202F)) == 0x202F,
                  "XKB phai phat dung ky tu dem autocomplete U+202F");
            xkb_state_unref(state);
        }
        xkb_keymap_unref(keymap);
    }
    xkb_context_unref(context);
    std::filesystem::remove_all(made);
}

void testLayoutInstaller() {
    char pattern[] = "/tmp/h-openkey-install-XXXXXX";
    const char* made = mkdtemp(pattern);
    check(made != nullptr, "khong tao duoc thu muc cai dat XKB tam");
    if (!made) return;

    const char* oldConfig = std::getenv("XDG_CONFIG_HOME");
    const bool hadConfig = oldConfig != nullptr;
    const std::string savedConfig = oldConfig ? oldConfig : "";
    setenv("XDG_CONFIG_HOME", made, 1);
    std::string error;
    check(openkey::installDriverXkbLayout(error),
          error.empty() ? "khong cai duoc layout XKB" : error.c_str());
    check(std::filesystem::exists(
              std::filesystem::path(made) / "xkb/symbols/hopenkey"),
          "trinh cai dat phai tao tep symbols");

    if (hadConfig) {
        setenv("XDG_CONFIG_HOME", savedConfig.c_str(), 1);
    } else {
        unsetenv("XDG_CONFIG_HOME");
    }
    std::filesystem::remove_all(made);
}

void testUInputCanCreateDevice() {
    if (access("/dev/uinput", W_OK) != 0) {
        std::printf("SKIP: chua co quyen /dev/uinput\n");
        return;
    }
    openkey::UInputKeyboard keyboard;
    std::string error;
    check(keyboard.start(error), error.empty() ? "khong tao duoc uinput" : error.c_str());
    keyboard.stop();
}

} // namespace

int main() {
    testMappings();
    testGeneratedXkb();
    testLayoutInstaller();
    testUInputCanCreateDevice();
    if (failures == 0) {
        std::printf("Tat ca kiem thu driver deu dat.\n");
        return 0;
    }
    std::printf("%d kiem thu driver khong dat.\n", failures);
    return 1;
}
