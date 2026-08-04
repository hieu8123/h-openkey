//
//  test_ibus.cpp
//  OpenKey cho Linux — kiểm thử phần thuần của backend IBus
//
//  Không cần ibus-daemon, không cần màn hình.
//

#include <cstdio>

#include "IBusDeletePlan.h"
#include "IBusKeyTranslate.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  [hỏng] %s\n", what);
        ++failures;
    }
}

// Keycode evdev của phím 'a' là 30; quy ước nội bộ là X11 = evdev + 8 = 38.
constexpr uint32_t kEvdevA = 30;
constexpr uint32_t kX11A = 38;

void testPhimBamThuong() {
    const openkey::KeyEvent ev = openkey::keyEventFromIBus(0x61, kEvdevA, 0);
    check(ev.pressed, "phím không có bit nhả thì phải là đang bấm");
    check(ev.keycode == kX11A, "keycode phải đổi sang quy ước X11");
    check(!ev.shift && !ev.ctrl && !ev.alt && !ev.super && !ev.capsLock,
          "không có modifier nào được bật");
}

void testPhimNha() {
    const openkey::KeyEvent ev = openkey::keyEventFromIBus(0x61, kEvdevA, 1u << 30);
    check(!ev.pressed, "bit 1<<30 nghĩa là phím nhả");
}

void testTungModifier() {
    check(openkey::keyEventFromIBus(0x41, kEvdevA, 1u << 0).shift, "bit 0 là Shift");
    check(openkey::keyEventFromIBus(0x61, kEvdevA, 1u << 1).capsLock, "bit 1 là CapsLock");
    check(openkey::keyEventFromIBus(0x61, kEvdevA, 1u << 2).ctrl, "bit 2 là Ctrl");
    check(openkey::keyEventFromIBus(0x61, kEvdevA, 1u << 3).alt, "bit 3 là Alt");
    check(openkey::keyEventFromIBus(0x61, kEvdevA, 1u << 6).super, "bit 6 là Super");
}

void testNhieuModifierCungLuc() {
    const openkey::KeyEvent ev =
        openkey::keyEventFromIBus(0x61, kEvdevA, (1u << 0) | (1u << 2) | (1u << 30));
    check(ev.shift && ev.ctrl, "Ctrl+Shift cùng lúc phải bật cả hai");
    check(!ev.pressed, "và vẫn là phím nhả");
    check(ev.otherControlKey(), "Ctrl tính là phím điều khiển khác");
}

void testXoaKhiUngDungHoTroSurrounding() {
    openkey::DeleteRequest del;
    del.utf8Bytes = 9;   // 3 ký tự tiếng Việt có dấu
    del.keyPresses = 3;
    const openkey::IBusDeletePlan plan = openkey::planDelete(del, true);
    check(plan.useSurrounding, "ứng dụng hỗ trợ thì phải dùng surrounding text");
    check(plan.chars == 3, "đếm theo KÝ TỰ (3), không phải byte (9)");
    check(plan.backspaces == 0, "không gửi BackSpace nào");
}

void testXoaKhiUngDungKhongHoTro() {
    openkey::DeleteRequest del;
    del.utf8Bytes = 9;
    del.keyPresses = 3;
    const openkey::IBusDeletePlan plan = openkey::planDelete(del, false);
    check(!plan.useSurrounding, "ứng dụng không hỗ trợ thì phải rơi xuống BackSpace");
    check(plan.backspaces == 3, "gửi đúng 3 lần BackSpace");
    check(plan.chars == 0, "không dùng surrounding text");
}

void testKhongCoGiDeXoa() {
    openkey::DeleteRequest del;  // cả hai đều 0
    const openkey::IBusDeletePlan surrounding = openkey::planDelete(del, true);
    const openkey::IBusDeletePlan backspace = openkey::planDelete(del, false);
    check(!surrounding.useSurrounding && surrounding.chars == 0,
          "không có gì để xoá thì không gọi surrounding text");
    check(backspace.backspaces == 0, "và không gửi BackSpace nào");
}

void testSoByteKhacSoKyTu() {
    // Bẫy chính: một chữ 'e' thường chiếm 1 byte, 'ế' có dấu chiếm 3 byte.
    // Nếu cài đặt lỡ dùng utf8Bytes thì cả hai ca dưới đây đều ra 1.
    openkey::DeleteRequest motChuCoDau;
    motChuCoDau.utf8Bytes = 3;
    motChuCoDau.keyPresses = 1;
    check(openkey::planDelete(motChuCoDau, true).chars == 1,
          "một chữ có dấu vẫn chỉ là một ký tự");
}

// Nhóm test này ra đời từ một sự cố thật: DeleteSurroundingText xin xoá 1 ký tự
// trong lúc ô nhập chưa có ký tự nào làm Mutter vấp assertion rồi tự sát bằng
// SIGABRT, và người dùng bị đăng xuất cả phiên đăng nhập.
void testKhongXoaSurroundingKhiOTrong() {
    openkey::DeleteRequest del;
    del.keyPresses = 1;
    check(!openkey::canDeleteSurrounding(del, true, true, 0),
          "ô nhập trống thì tuyệt đối không được xoá lùi bằng surrounding text");
}

void testKhongXoaSurroundingKhiXinQuaSoDangCo() {
    openkey::DeleteRequest del;
    del.keyPresses = 3;
    check(!openkey::canDeleteSurrounding(del, true, true, 2),
          "xin xoá 3 mà chỉ có 2 ký tự thì phải từ chối");
    check(openkey::canDeleteSurrounding(del, true, true, 3),
          "có đúng 3 ký tự thì cho phép");
    check(openkey::canDeleteSurrounding(del, true, true, 10),
          "có dư ký tự thì cho phép");
}

void testKhongXoaSurroundingKhiSoLieuDaCu() {
    openkey::DeleteRequest del;
    del.keyPresses = 1;
    check(!openkey::canDeleteSurrounding(del, true, false, 99),
          "số liệu cũ thì không dựa vào được, dù con số nhìn có vẻ dư");
    check(!openkey::canDeleteSurrounding(del, false, true, 99),
          "ứng dụng không hỗ trợ thì cũng không");
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
    testKhongXoaSurroundingKhiOTrong();
    testKhongXoaSurroundingKhiXinQuaSoDangCo();
    testKhongXoaSurroundingKhiSoLieuDaCu();

    if (failures == 0) {
        std::printf("  tất cả đều đạt\n");
        return 0;
    }
    std::printf("  %d chỗ hỏng\n", failures);
    return 1;
}
