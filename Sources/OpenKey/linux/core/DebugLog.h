//
//  DebugLog.h
//  OpenKey cho Linux
//
//  Ghi nhat ky chan doan. Bat/tat duoc ngay luc dang chay tu bang dieu khien,
//  khong phai dat bien moi truong roi khoi dong lai — nguoi dung gap loi thi
//  bat len, go lai cho loi tai hien, roi gui file log di.
//
//  Van doc OPENKEY_DEBUG=1 luc khoi dong de giu nguyen cach chan doan cu.
//

#ifndef OPENKEY_LINUX_DEBUGLOG_H
#define OPENKEY_LINUX_DEBUGLOG_H

#include <string>

namespace openkey {

// Bat thi mo (va cat rong) file log; tat thi dong lai. Tra ve false neu khong
// mo duoc file.
bool setDebugLogging(bool on);

bool debugLoggingEnabled();

// Duong dan file log, ke ca khi dang tat — de bang dieu khien hien cho nguoi
// dung biet ma gui di.
const std::string& debugLogPath();

// In mot dong kem nhan `tag`. Khong lam gi neu dang tat.
void debugLog(const char* tag, const char* format, ...)
    __attribute__((format(printf, 2, 3)));

} // namespace openkey

#endif // OPENKEY_LINUX_DEBUGLOG_H
