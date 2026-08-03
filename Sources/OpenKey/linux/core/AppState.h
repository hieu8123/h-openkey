//
//  AppState.h
//  OpenKey cho Linux
//
//  Engine.h khai bao mot loat bien `extern int v*` va yeu cau ung dung tu dinh
//  nghia chung. AppState.cpp la noi dinh nghia that, kem gia tri mac dinh.
//

#ifndef OPENKEY_LINUX_APPSTATE_H
#define OPENKEY_LINUX_APPSTATE_H

#include "Engine.h"

namespace openkey {

// Gia tri quy uoc cua engine cho phim tat chi gom phim bo tro, khong co phim chinh.
constexpr int kSwitchKeyModifiersOnly = 0xFE;

// Ctrl + Shift, kieu quen thuoc cua UniKey/EVKey va dung nhu README noi. Ban
// Windows mac dinh Alt+Z, nhung nguoi dung Linux quen Ctrl+Shift hon. Bit 8 la
// Control, bit 11 la Shift theo quy uoc trong Engine.h.
constexpr int kDefaultSwitchKeyStatus = kSwitchKeyModifiersOnly | 0x100 | 0x800;

// Dat toan bo bien engine ve gia tri mac dinh: Telex, Unicode, tieng Viet bat.
void resetAppStateToDefault();

} // namespace openkey

#endif // OPENKEY_LINUX_APPSTATE_H
