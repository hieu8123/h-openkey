//
//  test_ibus.cpp
//  OpenKey cho Linux — kiem thu phan thuan cua backend IBus
//
//  Khong can ibus-daemon, khong can man hinh.
//

#include <cstdio>

#include "IBusDeletePlan.h"
#include "IBusKeyTranslate.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  [hong] %s\n", what);
        ++failures;
    }
}

// Keycode evdev cua phim 'a' la 30; quy uoc noi bo la X11 = evdev + 8 = 38.
constexpr uint32_t kEvdevA = 30;
constexpr uint32_t kX11A = 38;

void testPhimBamThuong() {
    const openkey::KeyEvent ev = openkey::keyEventFromIBus(0x61, kEvdevA, 0);
    check(ev.pressed, "phim khong co bit nha thi phai la dang bam");
    check(ev.keycode == kX11A, "keycode phai doi sang quy uoc X11");
    check(!ev.shift && !ev.ctrl && !ev.alt && !ev.super && !ev.capsLock,
          "khong co modifier nao duoc bat");
}

void testPhimNha() {
    const openkey::KeyEvent ev = openkey::keyEventFromIBus(0x61, kEvdevA, 1u << 30);
    check(!ev.pressed, "bit 1<<30 nghia la phim nha");
}

void testTungModifier() {
    check(openkey::keyEventFromIBus(0x41, kEvdevA, 1u << 0).shift, "bit 0 la Shift");
    check(openkey::keyEventFromIBus(0x61, kEvdevA, 1u << 1).capsLock, "bit 1 la CapsLock");
    check(openkey::keyEventFromIBus(0x61, kEvdevA, 1u << 2).ctrl, "bit 2 la Ctrl");
    check(openkey::keyEventFromIBus(0x61, kEvdevA, 1u << 3).alt, "bit 3 la Alt");
    check(openkey::keyEventFromIBus(0x61, kEvdevA, 1u << 6).super, "bit 6 la Super");
}

void testNhieuModifierCungLuc() {
    const openkey::KeyEvent ev =
        openkey::keyEventFromIBus(0x61, kEvdevA, (1u << 0) | (1u << 2) | (1u << 30));
    check(ev.shift && ev.ctrl, "Ctrl+Shift cung luc phai bat ca hai");
    check(!ev.pressed, "va van la phim nha");
    check(ev.otherControlKey(), "Ctrl tinh la phim dieu khien khac");
}

void testXoaKhiUngDungHoTroSurrounding() {
    openkey::DeleteRequest del;
    del.utf8Bytes = 9;   // 3 ky tu tieng Viet co dau
    del.keyPresses = 3;
    const openkey::IBusDeletePlan plan = openkey::planDelete(del, true);
    check(plan.useSurrounding, "ung dung ho tro thi phai dung surrounding text");
    check(plan.chars == 3, "dem theo KY TU (3), khong phai byte (9)");
    check(plan.backspaces == 0, "khong gui BackSpace nao");
}

void testXoaKhiUngDungKhongHoTro() {
    openkey::DeleteRequest del;
    del.utf8Bytes = 9;
    del.keyPresses = 3;
    const openkey::IBusDeletePlan plan = openkey::planDelete(del, false);
    check(!plan.useSurrounding, "ung dung khong ho tro thi phai roi xuong BackSpace");
    check(plan.backspaces == 3, "gui dung 3 lan BackSpace");
    check(plan.chars == 0, "khong dung surrounding text");
}

void testKhongCoGiDeXoa() {
    openkey::DeleteRequest del;  // ca hai deu 0
    const openkey::IBusDeletePlan surrounding = openkey::planDelete(del, true);
    const openkey::IBusDeletePlan backspace = openkey::planDelete(del, false);
    check(!surrounding.useSurrounding && surrounding.chars == 0,
          "khong co gi de xoa thi khong goi surrounding text");
    check(backspace.backspaces == 0, "va khong gui BackSpace nao");
}

void testSoByteKhacSoKyTu() {
    // Bay chinh: mot chu 'e' thuong chiem 1 byte, 'e' co dau chiem 3 byte.
    // Neu cai dat lo dung utf8Bytes thi ca hai ca duoi day deu ra 1.
    openkey::DeleteRequest motChuCoDau;
    motChuCoDau.utf8Bytes = 3;
    motChuCoDau.keyPresses = 1;
    check(openkey::planDelete(motChuCoDau, true).chars == 1,
          "mot chu co dau van chi la mot ky tu");
}

} // namespace

int main() {
    std::printf("test_ibus\n");
    testPhimBamThuong();
    testPhimNha();
    testTungModifier();
    testNhieuModifierCungLuc();
    testXoaKhiUngDungHoTroSurrounding();
    testXoaKhiUngDungKhongHoTro();
    testKhongCoGiDeXoa();
    testSoByteKhacSoKyTu();

    if (failures == 0) {
        std::printf("  tat ca deu dat\n");
        return 0;
    }
    std::printf("  %d cho hong\n", failures);
    return 1;
}
