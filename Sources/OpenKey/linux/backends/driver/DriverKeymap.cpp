//
//  DriverKeymap.cpp
//

#include "DriverKeymap.h"

#include <linux/input.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace openkey {
namespace {

// Hai ma nam duoi gioi han X11 (evdev <= 247), khong bi inet(evdev) anh xa va
// khong duoc ban phim tren may thu nghiem khai bao. Backend van kiem tra lai
// tren tung may; neu co va cham thi tu choi grab thay vi pha phim that.
const std::vector<uint16_t> kReserved = {
    KEY_DELETEFILE, KEY_CLOSECD,
};

struct CarrierKey {
    uint16_t evdevCode;
    const char* xkbName;
    char lower;
    char upper;
};

// 24 phim x 6 level bo sung = 144 vi tri, du cho 142 code point. Level 1/2
// van la ky tu US goc, nen go ASCII va shortcut khong thay doi.
const CarrierKey kCarriers[] = {
    {KEY_Q, "AD01", 'q', 'Q'}, {KEY_W, "AD02", 'w', 'W'},
    {KEY_E, "AD03", 'e', 'E'}, {KEY_R, "AD04", 'r', 'R'},
    {KEY_T, "AD05", 't', 'T'}, {KEY_Y, "AD06", 'y', 'Y'},
    {KEY_U, "AD07", 'u', 'U'}, {KEY_I, "AD08", 'i', 'I'},
    {KEY_O, "AD09", 'o', 'O'}, {KEY_P, "AD10", 'p', 'P'},
    {KEY_A, "AC01", 'a', 'A'}, {KEY_S, "AC02", 's', 'S'},
    {KEY_D, "AC03", 'd', 'D'}, {KEY_F, "AC04", 'f', 'F'},
    {KEY_G, "AC05", 'g', 'G'}, {KEY_H, "AC06", 'h', 'H'},
    {KEY_J, "AC07", 'j', 'J'}, {KEY_K, "AC08", 'k', 'K'},
    {KEY_L, "AC09", 'l', 'L'}, {KEY_Z, "AB01", 'z', 'Z'},
    {KEY_X, "AB02", 'x', 'X'}, {KEY_C, "AB03", 'c', 'C'},
    {KEY_V, "AB04", 'v', 'V'}, {KEY_B, "AB05", 'b', 'B'},
};

std::vector<char32_t> mappedCharacters() {
    std::vector<char32_t> chars;
    chars.reserve(std::size(kCarriers) * 6);

    // U+1EA0..U+1EF9: ma chan la hoa, ma le ke tiep la thuong. Xep
    // thuong/hoa lien nhau de level Shift cua keymap cho dung truc giac.
    for (char32_t upper = 0x1EA0; upper < 0x1EF9; upper += 2) {
        chars.push_back(upper + 1);
        chars.push_back(upper);
    }

    static const char32_t latin[][2] = {
        {0x00E0, 0x00C0}, {0x00E1, 0x00C1}, {0x00E2, 0x00C2}, {0x00E3, 0x00C3},
        {0x00E8, 0x00C8}, {0x00E9, 0x00C9}, {0x00EA, 0x00CA}, {0x00EC, 0x00CC},
        {0x00ED, 0x00CD}, {0x00F2, 0x00D2}, {0x00F3, 0x00D3}, {0x00F4, 0x00D4},
        {0x00F5, 0x00D5}, {0x00F9, 0x00D9}, {0x00FA, 0x00DA}, {0x00FD, 0x00DD},
        {0x0103, 0x0102}, {0x0111, 0x0110}, {0x0129, 0x0128}, {0x0169, 0x0168},
        {0x01A1, 0x01A0}, {0x01B0, 0x01AF},
    };
    for (const auto& pair : latin) {
        chars.push_back(pair[0]);
        chars.push_back(pair[1]);
    }

    static const char32_t marks[] = {
        0x0300, 0x0301, 0x0302, 0x0303, 0x0306, 0x0309, 0x031B, 0x0323,
    };
    chars.insert(chars.end(), std::begin(marks), std::end(marks));
    return chars;
}

const std::map<char32_t, DriverKeyStroke>& strokeMap() {
    static const std::map<char32_t, DriverKeyStroke> map = [] {
        std::map<char32_t, DriverKeyStroke> out;
        const auto chars = mappedCharacters();
        for (size_t i = 0; i < chars.size(); ++i) {
            const size_t keyIndex = i / 6;
            const size_t level = i % 6;
            if (keyIndex >= std::size(kCarriers)) break;
            out[chars[i]] = DriverKeyStroke{
                kCarriers[keyIndex].evdevCode,
                level == 1 || level == 3 || level == 5,
                level == 0 || level == 1 || level == 4 || level == 5,
                level >= 2};
        }
        return out;
    }();
    return map;
}

DriverKeyStroke asciiStroke(char32_t cp) {
    bool shift = false;
    if (cp >= U'A' && cp <= U'Z') {
        cp = cp - U'A' + U'a';
        shift = true;
    }

    uint16_t code = 0;
    switch (cp) {
        case U'a': code = KEY_A; break; case U'b': code = KEY_B; break;
        case U'c': code = KEY_C; break; case U'd': code = KEY_D; break;
        case U'e': code = KEY_E; break; case U'f': code = KEY_F; break;
        case U'g': code = KEY_G; break; case U'h': code = KEY_H; break;
        case U'i': code = KEY_I; break; case U'j': code = KEY_J; break;
        case U'k': code = KEY_K; break; case U'l': code = KEY_L; break;
        case U'm': code = KEY_M; break; case U'n': code = KEY_N; break;
        case U'o': code = KEY_O; break; case U'p': code = KEY_P; break;
        case U'q': code = KEY_Q; break; case U'r': code = KEY_R; break;
        case U's': code = KEY_S; break; case U't': code = KEY_T; break;
        case U'u': code = KEY_U; break; case U'v': code = KEY_V; break;
        case U'w': code = KEY_W; break; case U'x': code = KEY_X; break;
        case U'y': code = KEY_Y; break; case U'z': code = KEY_Z; break;
        case U'1': code = KEY_1; break; case U'2': code = KEY_2; break;
        case U'3': code = KEY_3; break; case U'4': code = KEY_4; break;
        case U'5': code = KEY_5; break; case U'6': code = KEY_6; break;
        case U'7': code = KEY_7; break; case U'8': code = KEY_8; break;
        case U'9': code = KEY_9; break; case U'0': code = KEY_0; break;
        case U' ': code = KEY_SPACE; break;
        case U'\n': code = KEY_ENTER; break;
        case U'\t': code = KEY_TAB; break;
        case U'-': code = KEY_MINUS; break; case U'=': code = KEY_EQUAL; break;
        case U'[': code = KEY_LEFTBRACE; break; case U']': code = KEY_RIGHTBRACE; break;
        case U'\\': code = KEY_BACKSLASH; break; case U';': code = KEY_SEMICOLON; break;
        case U'\'': code = KEY_APOSTROPHE; break; case U'`': code = KEY_GRAVE; break;
        case U',': code = KEY_COMMA; break; case U'.': code = KEY_DOT; break;
        case U'/': code = KEY_SLASH; break;
        case U'!': code = KEY_1; shift = true; break;
        case U'@': code = KEY_2; shift = true; break;
        case U'#': code = KEY_3; shift = true; break;
        case U'$': code = KEY_4; shift = true; break;
        case U'%': code = KEY_5; shift = true; break;
        case U'^': code = KEY_6; shift = true; break;
        case U'&': code = KEY_7; shift = true; break;
        case U'*': code = KEY_8; shift = true; break;
        case U'(': code = KEY_9; shift = true; break;
        case U')': code = KEY_0; shift = true; break;
        case U'_': code = KEY_MINUS; shift = true; break;
        case U'+': code = KEY_EQUAL; shift = true; break;
        case U'{': code = KEY_LEFTBRACE; shift = true; break;
        case U'}': code = KEY_RIGHTBRACE; shift = true; break;
        case U'|': code = KEY_BACKSLASH; shift = true; break;
        case U':': code = KEY_SEMICOLON; shift = true; break;
        case U'"': code = KEY_APOSTROPHE; shift = true; break;
        case U'~': code = KEY_GRAVE; shift = true; break;
        case U'<': code = KEY_COMMA; shift = true; break;
        case U'>': code = KEY_DOT; shift = true; break;
        case U'?': code = KEY_SLASH; shift = true; break;
        default: break;
    }
    return DriverKeyStroke{code, shift, false, false};
}

std::string xkbSymbol(char32_t cp) {
    if (cp == 0) return "NoSymbol";
    std::ostringstream out;
    out << 'U' << std::uppercase << std::hex << static_cast<uint32_t>(cp);
    return out.str();
}

std::string readCommand(const char* command) {
    std::string out;
    FILE* pipe = popen(command, "r");
    if (!pipe) return out;
    char chunk[256];
    while (std::fgets(chunk, sizeof(chunk), pipe)) out += chunk;
    if (pclose(pipe) != 0) out.clear();
    return out;
}

std::filesystem::path userConfigRoot(std::string& error) {
    if (const char* config = std::getenv("XDG_CONFIG_HOME"); config && *config) {
        return config;
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / ".config";
    }
    error = "không tìm thấy XDG_CONFIG_HOME hoặc HOME";
    return {};
}

bool writeIfChanged(const std::filesystem::path& path, const std::string& text,
                    bool& changed, std::string& error) {
    std::ifstream old(path, std::ios::binary);
    if (old) {
        const std::string current((std::istreambuf_iterator<char>(old)),
                                  std::istreambuf_iterator<char>());
        if (current == text) return true;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "không mở được " + path.string() + " để ghi";
        return false;
    }
    file << text;
    if (!file) {
        error = "ghi tệp XKB thất bại: " + path.string();
        return false;
    }
    changed = true;
    return true;
}

} // namespace

uint16_t driverLevel3Keycode() { return kReserved.front(); }

uint16_t driverLevel5Keycode() { return kReserved.back(); }

const std::vector<uint16_t>& driverReservedKeycodes() { return kReserved; }

DriverKeyStroke driverKeyStrokeFor(char32_t codePoint) {
    const auto it = strokeMap().find(codePoint);
    return it == strokeMap().end() ? asciiStroke(codePoint) : it->second;
}

std::string driverXkbSymbols() {
    const auto chars = mappedCharacters();
    std::ostringstream out;
    out << "// Tu sinh boi H-OpenKey. Khong sua bang tay.\n"
           "default partial alphanumeric_keys\n"
           "xkb_symbols \"basic\" {\n"
           "    include \"us(basic)\"\n"
           "    name[Group1] = \"H-OpenKey\";\n\n";

    out << "    key <I" << (driverLevel3Keycode() + 8)
        << "> { type[Group1]=\"ONE_LEVEL\", [ ISO_Level3_Shift ] };\n";
    out << "    key <I" << (driverLevel5Keycode() + 8)
        << "> { type[Group1]=\"ONE_LEVEL\", [ ISO_Level5_Shift ] };\n\n";

    for (size_t keyIndex = 0; keyIndex < std::size(kCarriers); ++keyIndex) {
        const size_t first = keyIndex * 6;
        out << "    key <" << kCarriers[keyIndex].xkbName
            << "> { type[Group1]=\"EIGHT_LEVEL\", [ "
            << kCarriers[keyIndex].lower << ", " << kCarriers[keyIndex].upper;
        for (size_t level = 0; level < 6; ++level) {
            const size_t at = first + level;
            out << ", " << xkbSymbol(at < chars.size() ? chars[at] : 0);
        }
        out << " ] };\n";
    }
    out << "};\n";
    return out.str();
}

bool installDriverXkbLayout(std::string& error) {
    const std::filesystem::path base = userConfigRoot(error);
    if (base.empty()) return false;

    const std::filesystem::path symbolsDir = base / "xkb" / "symbols";
    std::error_code ec;
    std::filesystem::create_directories(symbolsDir, ec);
    if (ec) {
        error = "không tạo được thư mục XKB: " + ec.message();
        return false;
    }

    bool changed = false;
    return writeIfChanged(symbolsDir / "hopenkey", driverXkbSymbols(), changed,
                          error);
}

bool parseDriverSourceIndex(const std::string& text, size_t& index) {
    std::smatch match;
    const std::regex trailingNumber("([0-9]+)\\s*$");
    if (!std::regex_search(text, match, trailingNumber)) return false;
    try {
        index = std::stoul(match[1].str());
        return true;
    } catch (...) {
        return false;
    }
}

bool driverXkbLayoutIsActive(std::string& error) {
    const std::filesystem::path base = userConfigRoot(error);
    if (base.empty()) return false;
    if (!std::filesystem::exists(base / "xkb" / "symbols" / "hopenkey")) {
        error = "thiếu symbols của layout xkb:custom";
        return false;
    }
    // "custom" da co san trong registry xkeyboard-config. Chi can cai symbols
    // vao XKB root; nhu vay khong sua evdev.xml cua distro va khong can GNOME
    // Shell nap them mot ten layout moi.
    const std::filesystem::path systemCandidates[] = {
        "/usr/share/X11/xkb/symbols/custom",
        "/usr/share/xkeyboard-config-2/symbols/custom",
    };
    bool hasSystemSymbols = false;
    for (const auto& path : systemCandidates) {
        std::ifstream file(path, std::ios::binary);
        const std::string contents((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
        if (contents.find("Tu sinh boi H-OpenKey") != std::string::npos) {
            hasSystemSymbols = true;
            break;
        }
    }
    if (!hasSystemSymbols) {
        error = "Mutter chưa thấy symbols H-OpenKey trong XKB root hệ thống; "
                "hãy chạy lại trình cài đặt";
        return false;
    }
    const std::string sources = readCommand(
        "gsettings get org.gnome.desktop.input-sources sources 2>/dev/null");
    const std::string current = readCommand(
        "gsettings get org.gnome.desktop.input-sources current 2>/dev/null");
    if (sources.empty() || current.empty()) {
        error = "không đọc được input source hiện tại của GNOME";
        return false;
    }

    size_t active = 0;
    // gsettings tra "uint32 0". Lay chu so dau tien se thanh 32 (nam trong ten
    // kieu uint32), la loi khien driver khong khoi dong sau khi dang nhap.
    if (!parseDriverSourceIndex(current, active)) {
        error = "GNOME trả về chỉ số input source không hợp lệ: " + current;
        return false;
    }

    const std::regex tuple("\\('([^']+)',\\s*'([^']+)'\\)");
    size_t index = 0;
    for (auto it = std::sregex_iterator(sources.begin(), sources.end(), tuple);
         it != std::sregex_iterator(); ++it, ++index) {
        if (index != active) continue;
        if ((*it)[1] == "xkb" && (*it)[2] == "custom") return true;
        error = "input source hiện tại là " + (*it)[1].str() + ":" +
                (*it)[2].str() + ", cần xkb:custom (H-OpenKey)";
        return false;
    }
    error = "không tìm thấy input source hiện tại trong cấu hình GNOME";
    return false;
}

} // namespace openkey
