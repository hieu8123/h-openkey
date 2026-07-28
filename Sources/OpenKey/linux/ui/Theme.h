//
//  Theme.h
//  OpenKey cho Linux
//
//  Dat palette tuong minh thay vi pho mac cho he thong.
//
//  Ly do: Qt tren cac desktop khong dua tren Qt (COSMIC, GNOME) thuong nhan mot
//  palette nhat nheo — hop chon ra xam tren xam, doc nhu dang bi vo hieu hoa.
//  Dat ro thi bang dieu khien trong nhu nhau o moi noi.
//
//  Mau nhan la do son mai, cung ho voi bieu tuong o khay.
//

#ifndef OPENKEY_LINUX_THEME_H
#define OPENKEY_LINUX_THEME_H

class QApplication;

namespace openkey {

// Tu chon ban sang hay ban toi theo palette dang co cua he thong.
void applyTheme(QApplication& app);

} // namespace openkey

#endif // OPENKEY_LINUX_THEME_H
