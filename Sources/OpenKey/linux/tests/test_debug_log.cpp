#include <sys/stat.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "DebugLog.h"

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

} // namespace

int main() {
    char tempTemplate[] = "/tmp/h-openkey-debug-test-XXXXXX";
    char* temp = mkdtemp(tempTemplate);
    check(temp != nullptr, "tao thu muc tam");
    if (!temp) return 1;

    setenv("HOME", temp, 1);
    setenv("OPENKEY_DEBUG", "1", 1);

    check(openkey::debugLoggingEnabled(), "OPENKEY_DEBUG bat stderr");
    check(!openkey::debugFileLoggingEnabled(),
          "stderr khong duoc bao nham la dang ghi file");
    check(openkey::setDebugLogging(true), "UI mo duoc file log");
    check(openkey::debugFileLoggingEnabled(), "trang thai file da bat");

    openkey::debugLog("test", "noi dung can drain truoc khi dong");
    std::vector<std::thread> producers;
    for (int worker = 0; worker < 4; ++worker) {
        producers.emplace_back([worker] {
            for (int item = 0; item < 32; ++item) {
                openkey::debugLog("stress", "worker=%d item=%d", worker, item);
            }
        });
    }
    for (auto& producer : producers) producer.join();
    check(openkey::setDebugLogging(false), "UI dong duoc file log");
    check(!openkey::debugFileLoggingEnabled(), "trang thai file da tat");

    const std::string path = openkey::debugLogPath();
    struct stat info {};
    check(stat(path.c_str(), &info) == 0, "file log ton tai");
    check((info.st_mode & 0777) == 0600, "file log chi user duoc doc ghi");

    std::ifstream file(path);
    const std::string contents((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    check(contents.find("noi dung can drain") != std::string::npos,
          "tat log phai drain ring buffer");
    check(contents.find("worker=3 item=31") != std::string::npos,
          "ring buffer MPSC phai drain du producer");

    std::filesystem::remove_all(temp);
    if (failures == 0) std::cout << "Tat ca test debug log da dat.\n";
    return failures == 0 ? 0 : 1;
}
