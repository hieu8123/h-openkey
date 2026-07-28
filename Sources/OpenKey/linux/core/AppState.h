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

// Alt + Z, khop voi mac dinh cua ban Windows (chi khac keycode vi Linux dung
// keycode X11). Bit 9 la Option/Alt theo quy uoc trong Engine.h.
constexpr int kDefaultSwitchKeyStatus = 0x200 | KEY_Z;

// Gia tri quy uoc cua engine cho phim tat chi gom phim bo tro, khong co phim chinh.
constexpr int kSwitchKeyModifiersOnly = 0xFE;

// Dat toan bo bien engine ve gia tri mac dinh: Telex, Unicode, tieng Viet bat.
void resetAppStateToDefault();

} // namespace openkey

#endif // OPENKEY_LINUX_APPSTATE_H
