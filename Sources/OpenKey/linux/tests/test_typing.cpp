//
//  test_typing.cpp
//  OpenKey cho Linux — kiem thu
//
//  Bom mot chuoi phim vao core roi doi chieu voi van ban ky vong. Khong can
//  compositor, khong can man hinh.
//

#include <cstdio>
#include <map>
#include <string>

#include "AppState.h"
#include "FakeBackend.h"
#include "OpenKeyCore.h"

namespace {

int failures = 0;

uint32_t keycodeForChar(char c) {
    static const std::map<char, uint32_t> table = {
        {'a', KEY_A}, {'b', KEY_B}, {'c', KEY_C}, {'d', KEY_D}, {'e', KEY_E},
        {'f', KEY_F}, {'g', KEY_G}, {'h', KEY_H}, {'i', KEY_I}, {'j', KEY_J},
        {'k', KEY_K}, {'l', KEY_L}, {'m', KEY_M}, {'n', KEY_N}, {'o', KEY_O},
        {'p', KEY_P}, {'q', KEY_Q}, {'r', KEY_R}, {'s', KEY_S}, {'t', KEY_T},
        {'u', KEY_U}, {'v', KEY_V}, {'w', KEY_W}, {'x', KEY_X}, {'y', KEY_Y},
        {'z', KEY_Z},
        {'1', KEY_1}, {'2', KEY_2}, {'3', KEY_3}, {'4', KEY_4}, {'5', KEY_5},
        {'6', KEY_6}, {'7', KEY_7}, {'8', KEY_8}, {'9', KEY_9}, {'0', KEY_0},
        {' ', KEY_SPACE}, {'.', KEY_DOT}, {',', KEY_COMMA},
        {'[', KEY_LEFT_BRACKET}, {']', KEY_RIGHT_BRACKET},
    };
    auto it = table.find(c);
    return it == table.end() ? 0 : it->second;
}

// Go mot chuoi ky tu. Chu hoa trong `keys` nghia la giu Shift.
std::string typeKeys(const std::string& keys, int inputType, int codeTable) {
    openkey::resetAppStateToDefault();
    vInputType = inputType;
    vCodeTable = codeTable;

    openkey::FakeBackend backend;
    openkey::OpenKeyCore core(backend);
    core.attach();
    core.resetTypingState();

    for (char c : keys) {
        openkey::KeyEvent ev;
        ev.pressed = true;
        ev.shift = (c >= 'A' && c <= 'Z');
        const char lower = ev.shift ? static_cast<char>(c - 'A' + 'a') : c;
        ev.keycode = keycodeForChar(lower);
        if (ev.keycode == 0) {
            std::printf("  [canh bao] khong biet phim '%c'\n", c);
            continue;
        }
        backend.feed(ev);
    }
    return backend.buffer;
}

void check(const char* label, const std::string& got, const std::string& want) {
    if (got == want) {
        std::printf("  ok   %-28s -> %s\n", label, got.c_str());
    } else {
        std::printf("  FAIL %-28s -> got \"%s\", want \"%s\"\n", label, got.c_str(),
                    want.c_str());
        failures++;
    }
}

void telexTests() {
    std::printf("Telex, bang ma Unicode:\n");
    check("as", typeKeys("as", vTelex, 0), "á");
    check("dd", typeKeys("dd", vTelex, 0), "đ");
    check("vieejt", typeKeys("vieejt", vTelex, 0), "việt");
    check("tieengs", typeKeys("tieengs", vTelex, 0), "tiếng");
    check("ddaay", typeKeys("ddaay", vTelex, 0), "đây");
    // Mac dinh la kieu dat dau cu, nen "hoaf" phai ra "hòa" chu khong phai "hoà".
    check("hoaf (kieu cu)", typeKeys("hoaf", vTelex, 0), "hòa");
    check("nhuwng", typeKeys("nhuwng", vTelex, 0), "nhưng");
    check("Vieejt Nam", typeKeys("Vieejt Nam", vTelex, 0), "Việt Nam");
}

void vniTests() {
    std::printf("VNI, bang ma Unicode:\n");
    check("a1", typeKeys("a1", vVNI, 0), "á");
    check("d9", typeKeys("d9", vVNI, 0), "đ");
    check("vie6t5", typeKeys("vie6t5", vVNI, 0), "việt");
    check("tie6ng1", typeKeys("tie6ng1", vVNI, 0), "tiếng");
}

void modernOrthographyTest() {
    std::printf("Dat dau kieu moi:\n");
    openkey::resetAppStateToDefault();
    vUseModernOrthography = 1;

    openkey::FakeBackend backend;
    openkey::OpenKeyCore core(backend);
    core.attach();
    core.resetTypingState();
    for (char c : std::string("hoaf")) {
        openkey::KeyEvent ev;
        ev.pressed = true;
        ev.keycode = keycodeForChar(c);
        backend.feed(ev);
    }
    check("hoaf (kieu moi)", backend.buffer, "hoà");
}

void multiWordTest() {
    std::printf("Nhieu tu lien tiep:\n");
    check("hoc4 sinh (space)", typeKeys("hocj sinh", vTelex, 0), "học sinh");
    check("xoa giua chung", typeKeys("tieengs Vieejt", vTelex, 0),
          "tiếng Việt");
}

// Hoi quy: phim bo tro tung duoc nap vao engine nhu ky tu, lam hong bo dem tu
// dang go va tat luon viec xu ly cho toi khi ngat tu. Phat hien khi chay that
// tren COSMIC: bam Alt roi go thi khong ra chu nao nua.
void modifierKeyTest() {
    std::printf("Phim bo tro khong duoc lam hong bo dem:\n");
    openkey::resetAppStateToDefault();

    openkey::FakeBackend backend;
    openkey::OpenKeyCore core(backend);
    core.attach();
    core.resetTypingState();

    // Alt trai (keycode X11 64), giong dung chuoi da thay trong log that.
    openkey::KeyEvent alt;
    alt.pressed = true;
    alt.keycode = 64;
    backend.feed(alt);

    for (char c : std::string("vieejt")) {
        openkey::KeyEvent ev;
        ev.pressed = true;
        ev.keycode = keycodeForChar(c);
        backend.feed(ev);
    }
    check("Alt roi 'vieejt'", backend.buffer, "việt");
}

} // namespace

int main() {
    telexTests();
    vniTests();
    modernOrthographyTest();
    multiWordTest();
    modifierKeyTest();

    if (failures == 0) {
        std::printf("\nTat ca deu dat.\n");
        return 0;
    }
    std::printf("\n%d truong hop khong dat.\n", failures);
    return 1;
}
