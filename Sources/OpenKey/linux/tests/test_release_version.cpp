// Kiem thu parser tag Linux dung cho thong bao cap nhat.

#include <cstdio>

#include "ReleaseVersion.h"

namespace {

int failures = 0;

void check(bool ok, const char* message) {
    if (ok) return;
    std::printf("FAIL: %s\n", message);
    failures++;
}

} // namespace

int main() {
    QString version;
    check(openkey::isNewerLinuxRelease("1.3.1", "linux-v1.3.2", version) &&
              version == "1.3.2",
          "phai nhan ra patch Linux moi");
    check(!openkey::isNewerLinuxRelease("1.3.2", "linux-v1.3.2", version),
          "khong duoc bao cap nhat cho cung phien ban");
    check(!openkey::isNewerLinuxRelease("1.4.0", "linux-v1.3.9", version),
          "khong duoc ha phien ban");
    check(!openkey::isNewerLinuxRelease("1.3.1", "v2.0.0", version),
          "phai bo qua tag khong thuoc Linux");
    check(!openkey::isNewerLinuxRelease("1.3.1", "linux-v1.4.0-beta", version),
          "phai bo qua prerelease khong on dinh");

    if (failures == 0) {
        std::printf("Tat ca kiem thu phien ban deu dat.\n");
        return 0;
    }
    return 1;
}
