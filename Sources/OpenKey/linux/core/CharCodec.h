//
//  CharCodec.h
//  OpenKey cho Linux
//
//  Engine tra ve charData[] la cac Uint32 da ma hoa. Cach giai ma phu thuoc
//  bang ma dang chon va la phan de sai nhat cua toan bo cong viec port, nen no
//  duoc tach rieng ra day de kiem thu doc lap.
//

#ifndef OPENKEY_LINUX_CHARCODEC_H
#define OPENKEY_LINUX_CHARCODEC_H

#include <cstdint>
#include <string>
#include <vector>

namespace openkey {

// Mot ky tu logic cua engine, sau khi giai ma.
struct DecodedChar {
    char32_t cp[2] = {0, 0};  // toi da 2 code point (bang ma cu, Unicode to hop)
    uint8_t count = 0;
};

// Giai ma mot phan tu charData[] hoac macroData[]. Doc vCodeTable hien hanh.
// Tra ve count == 0 neu phan tu khong sinh ra ky tu nao.
DecodedChar decodeEngineChar(uint32_t data);

// So byte UTF-8 cua mot code point.
uint8_t utf8Length(char32_t cp);

// Noi them mot code point vao chuoi UTF-8.
void utf8Append(std::string& out, char32_t cp);

// Ma hoa ca chuoi sang UTF-8.
std::string utf8Encode(const std::u32string& s);

} // namespace openkey

#endif // OPENKEY_LINUX_CHARCODEC_H
