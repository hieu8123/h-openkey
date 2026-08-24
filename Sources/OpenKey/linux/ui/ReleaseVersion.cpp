//
//  ReleaseVersion.cpp
//

#include "ReleaseVersion.h"

#include <QVersionNumber>

namespace openkey {
namespace {

bool parseStableVersion(const QString& text, QVersionNumber& version) {
    qsizetype suffixIndex = 0;
    version = QVersionNumber::fromString(text, &suffixIndex);
    return suffixIndex == text.size() && version.segmentCount() == 3;
}

} // namespace

bool isNewerLinuxRelease(const QString& currentVersion,
                         const QString& releaseTag, QString& versionText) {
    static const QString prefix = QStringLiteral("linux-v");
    if (!releaseTag.startsWith(prefix)) return false;

    versionText = releaseTag.mid(prefix.size());
    QVersionNumber current;
    QVersionNumber release;
    if (!parseStableVersion(currentVersion, current) ||
        !parseStableVersion(versionText, release)) {
        versionText.clear();
        return false;
    }
    return QVersionNumber::compare(release, current) > 0;
}

} // namespace openkey
