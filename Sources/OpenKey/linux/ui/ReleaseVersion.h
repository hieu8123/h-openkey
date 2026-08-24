//
//  ReleaseVersion.h
//  So sanh phien ban H-OpenKey Linux tren GitHub Releases.
//

#ifndef OPENKEY_LINUX_RELEASEVERSION_H
#define OPENKEY_LINUX_RELEASEVERSION_H

#include <QString>

namespace openkey {

// Chi chap nhan tag on dinh linux-vX.Y.Z. Tra ve true neu tag moi hon phien
// ban dang chay; version nhan X.Y.Z de hien thi tren giao dien.
bool isNewerLinuxRelease(const QString& currentVersion,
                         const QString& releaseTag, QString& version);

} // namespace openkey

#endif // OPENKEY_LINUX_RELEASEVERSION_H
