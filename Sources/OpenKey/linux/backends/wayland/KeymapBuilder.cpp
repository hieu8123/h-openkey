//
//  KeymapBuilder.cpp
//  OpenKey cho Linux
//

#include "KeymapBuilder.h"

#include <cctype>
#include <cstdio>
#include <set>
#include <vector>

namespace openkey {
namespace {

// Keycode xkb = keycode evdev + 8. Chi cap phat tu 256 tro len: khoang duoi do
// evdev da dung gan het, va de nguyen cho phim that.
constexpr int kFirstXkbCode = 256;

// evdev khong dinh nghia ma nao qua 767, nen day la gioi han an toan.
constexpr int kLastXkbCode = 767;

// Nhung ky tu can go bang ban phim ao. Chi gom thu that su can:
//  - ASCII in duoc: cho truong hop engine tra lai nguyen phim da bam
//  - U+1EA0..U+1EF9: toan bo chu tieng Viet mo rong
//  - mot so chu Latin-1 va Latin mo rong ma tieng Viet dung
//  - dau to hop, cho bang ma "Unicode to hop"
std::vector<char32_t> charactersToMap() {
    std::vector<char32_t> chars;

    for (char32_t c = 0x20; c <= 0x7E; c++) {
        chars.push_back(c);
    }
    for (char32_t c = 0x1EA0; c <= 0x1EF9; c++) {
        chars.push_back(c);
    }

    static const char32_t extra[] = {
        0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C8, 0x00C9, 0x00CA, 0x00CC, 0x00CD,
        0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D9, 0x00DA, 0x00DD,
        0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E8, 0x00E9, 0x00EA, 0x00EC, 0x00ED,
        0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F9, 0x00FA, 0x00FD,
        0x0102, 0x0103, 0x0110, 0x0111, 0x0128, 0x0129, 0x0168, 0x0169,
        0x01A0, 0x01A1, 0x01AF, 0x01B0,
        0x0300, 0x0301, 0x0302, 0x0303, 0x0306, 0x0309, 0x031B, 0x0323,
    };
    for (char32_t c : extra) {
        chars.push_back(c);
    }
    return chars;
}

// Vi tri dong `};` dung o dau dong, ket thuc mot khoi cua keymap.
size_t findSectionEnd(const std::string& text, const std::string& sectionName) {
    const size_t start = text.find(sectionName);
    if (start == std::string::npos) {
        return std::string::npos;
    }
    // Cac dong `};` long trong deu co thut le, nen `\n};` chi khop dong ket thuc khoi.
    return text.find("\n};", start);
}

void collectUsedKeycodes(const std::string& text, std::set<int>& used) {
    const size_t start = text.find("xkb_keycodes");
    const size_t end = findSectionEnd(text, "xkb_keycodes");
    if (start == std::string::npos || end == std::string::npos) {
        return;
    }

    size_t i = start;
    while (i < end) {
        const size_t eq = text.find('=', i);
        if (eq == std::string::npos || eq >= end) {
            break;
        }
        size_t j = eq + 1;
        while (j < end && std::isspace(static_cast<unsigned char>(text[j]))) j++;
        int value = 0;
        bool digits = false;
        while (j < end && std::isdigit(static_cast<unsigned char>(text[j]))) {
            value = value * 10 + (text[j] - '0');
            digits = true;
            j++;
        }
        if (digits) {
            used.insert(value);
        }
        i = j + 1;
    }
}

int groupCount(const std::string& text) {
    int count = 0;
    size_t i = 0;
    while ((i = text.find("name[Group", i)) != std::string::npos) {
        count++;
        i += 10;
    }
    return count < 1 ? 1 : count;
}

} // namespace

bool KeymapBuilder::build(const std::string& realKeymap) {
    _merged.clear();
    _codes.clear();

    const size_t keycodesEnd = findSectionEnd(realKeymap, "xkb_keycodes");
    const size_t symbolsEnd = findSectionEnd(realKeymap, "xkb_symbols");
    if (keycodesEnd == std::string::npos || symbolsEnd == std::string::npos ||
        keycodesEnd >= symbolsEnd) {
        return false;
    }

    std::set<int> used;
    collectUsedKeycodes(realKeymap, used);

    const std::vector<char32_t> chars = charactersToMap();
    std::string keycodeLines;
    std::string symbolLines;
    const int groups = groupCount(realKeymap);

    int next = kFirstXkbCode;
    for (char32_t cp : chars) {
        while (next <= kLastXkbCode && used.count(next) > 0) {
            next++;
        }
        if (next > kLastXkbCode) {
            std::fprintf(stderr,
                         "[openkey] het ma phim trong, chi ghep duoc %zu ky tu\n",
                         _codes.size());
            break;
        }

        char name[32];
        std::snprintf(name, sizeof(name), "<OK%04X>", static_cast<unsigned>(cp));

        char line[128];
        std::snprintf(line, sizeof(line), "\t%s = %d;\n", name, next);
        keycodeLines += line;

        char sym[32];
        std::snprintf(sym, sizeof(sym), "U%04X", static_cast<unsigned>(cp));

        // Phai dinh nghia cho MOI nhom: keymap nay co hai nhom (English US va
        // Vietnamese US), va nguoi dung co the dang o nhom nao cung duoc.
        std::string groupsPart;
        for (int g = 0; g < groups; g++) {
            if (g > 0) groupsPart += ", ";
            groupsPart += std::string("[ ") + sym + " ]";
        }
        symbolLines += "\tkey " + std::string(name) + " { " + groupsPart + " };\n";

        _codes[cp] = next - 8; // keycode evdev
        next++;
    }

    if (_codes.empty()) {
        return false;
    }

    // Chen vao truoc dong ket thuc cua tung khoi. Chen phan symbols truoc de vi
    // tri cua khoi keycodes khong bi xe dich.
    _merged = realKeymap;
    _merged.insert(symbolsEnd + 1, symbolLines);
    _merged.insert(keycodesEnd + 1, keycodeLines);

    // Noi tran keycode neu can, neu khong xkb se tu choi cac ma vua them.
    const size_t maxPos = _merged.find("maximum");
    if (maxPos != std::string::npos) {
        const size_t eq = _merged.find('=', maxPos);
        const size_t semi = _merged.find(';', maxPos);
        if (eq != std::string::npos && semi != std::string::npos && eq < semi) {
            const int declared = std::atoi(_merged.substr(eq + 1, semi - eq - 1).c_str());
            if (declared < kLastXkbCode) {
                _merged.replace(eq + 1, semi - eq - 1,
                                " " + std::to_string(kLastXkbCode));
            }
        }
    }

    return true;
}

int KeymapBuilder::evdevKeycodeFor(char32_t codePoint) const {
    auto it = _codes.find(codePoint);
    return it == _codes.end() ? -1 : it->second;
}

} // namespace openkey
