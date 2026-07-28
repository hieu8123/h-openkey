//
//  X11Backend.h
//  OpenKey cho Linux
//
//  Backend cho phien dang nhap X11, va cho cac ung dung XWayland khi bi ep dung.
//
//  Khac biet co ban so voi backend Wayland: XRecord chi QUAN SAT duoc phim, chu
//  khong chan duoc. Nghia la phim goc da di den ung dung truoc khi ta kip xu ly.
//  Vi vay khi engine muon thay the mot chum ky tu, ta phai xoa THEM mot ky tu —
//  chinh la ky tu ma phim vua bam da tao ra.
//
//  Doi lai, cach nay khong doi quyen dac biet nao. Cach duy nhat chan duoc phim
//  tren X11 la evdev grab + uinput, ma cach do can quyen root hoac udev rule.
//

#ifndef OPENKEY_LINUX_X11BACKEND_H
#define OPENKEY_LINUX_X11BACKEND_H

#include <memory>
#include <string>

#include "Backend.h"

namespace openkey {

// Tra ve nullptr kem ly do trong `error` neu khong mo duoc man hinh X11 hoac
// thieu phan mo rong XRecord/XTEST.
std::unique_ptr<IBackend> makeX11Backend(std::string& error);

} // namespace openkey

#endif // OPENKEY_LINUX_X11BACKEND_H
