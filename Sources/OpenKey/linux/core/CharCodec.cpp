//
//  CharCodec.cpp
//  OpenKey cho Linux
//
//  Quy tac giai ma o day sao lai dung logic SendNewCharString cua ban macOS
//  (Sources/OpenKey/macOS/ModernKey/OpenKey.mm) va SendKeyCode cua ban Windows.
//  Bat ky thay doi nao o hai noi kia deu phai doi chieu lai voi day.
//

#include "CharCodec.h"

#include "Engine.h"
#include "Vietnamese.h"

namespace openkey {

DecodedChar decodeEngineChar(uint32_t data) {
    DecodedChar out;

    if (data & PURE_CHARACTER_MASK) {
        // Ky tu tho, gui thang khong dien giai.
        out.cp[0] = static_cast<char32_t>(static_cast<uint16_t>(data));
        out.count = 1;
        return out;
    }

    if (!(data & CHAR_CODE_MASK)) {
        // Day la keycode chu khong phai ma ky tu: tra ve chu cai tuong ung,
        // co xet CAPS_MASK. Tra ve 0 nghia la phim khong sinh ky tu in duoc.
        const uint16_t ch = keyCodeToCharacter(data);
        if (ch == 0) {
            return out; // count == 0
        }
        out.cp[0] = static_cast<char32_t>(ch);
        out.count = 1;
        return out;
    }

    const uint16_t raw = static_cast<uint16_t>(data);

    if (vCodeTable == 0) {
        // Unicode dung san: 16 bit thap chinh la code point.
        out.cp[0] = static_cast<char32_t>(raw);
        out.count = 1;
    } else if (vCodeTable == 1 || vCodeTable == 2 || vCodeTable == 4) {
        // TCVN3, VNI-Windows, CP1258: bang ma mot byte. Mot chu co the gom hai
        // byte, byte thap truoc. Nguong 32 la quy uoc san co cua engine.
        const uint8_t lo = static_cast<uint8_t>(raw & 0xFF);
        const uint8_t hi = static_cast<uint8_t>((raw >> 8) & 0xFF);
        out.cp[0] = static_cast<char32_t>(lo);
        out.count = 1;
        if (hi > 32) {
            out.cp[1] = static_cast<char32_t>(hi);
            out.count = 2;
        }
    } else if (vCodeTable == 3) {
        // Unicode to hop: 13 bit thap la chu goc, 3 bit cao chon dau to hop.
        const uint16_t markIndex = static_cast<uint16_t>(raw >> 13);
        out.cp[0] = static_cast<char32_t>(raw & 0x1FFF);
        out.count = 1;
        if (markIndex > 0) {
            out.cp[1] = static_cast<char32_t>(_unicodeCompoundMark[markIndex - 1]);
            out.count = 2;
        }
    } else {
        out.cp[0] = static_cast<char32_t>(raw);
        out.count = 1;
    }

    return out;
}

uint8_t utf8Length(char32_t cp) {
    if (cp < 0x80) return 1;
    if (cp < 0x800) return 2;
    if (cp < 0x10000) return 3;
    return 4;
}

void utf8Append(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string utf8Encode(const std::u32string& s) {
    std::string out;
    out.reserve(s.size() * 3);
    for (char32_t cp : s) {
        utf8Append(out, cp);
    }
    return out;
}

} // namespace openkey
