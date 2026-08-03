//
//  DebugLog.cpp
//  OpenKey cho Linux
//

#include "DebugLog.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace openkey {
namespace {

FILE* g_file = nullptr;
bool g_enabled = false;
bool g_toStderr = false;

std::string homeDir() {
    const char* home = std::getenv("HOME");
    return home && *home ? home : "/tmp";
}

std::string computeLogPath() {
    // Dat trong thu muc du lieu chuan de khong rai rac o thu muc nha, nhung van
    // la duong dan ngan de nguoi dung tim ra ma gui di.
    const std::string dir = homeDir() + "/.local/share/h-openkey";
    mkdir((homeDir() + "/.local").c_str(), 0755);
    mkdir((homeDir() + "/.local/share").c_str(), 0755);
    mkdir(dir.c_str(), 0755);
    return dir + "/debug.log";
}

void writeHeader() {
    if (!g_file) return;
    const std::time_t now = std::time(nullptr);
    char stamp[64] = {};
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::fprintf(g_file, "=== H-OpenKey: bat dau ghi nhat ky luc %s ===\n", stamp);
    std::fflush(g_file);
}

} // namespace

const std::string& debugLogPath() {
    static const std::string path = computeLogPath();
    return path;
}

bool setDebugLogging(bool on) {
    if (on == g_enabled) {
        return true;
    }
    if (!on) {
        if (g_file) {
            std::fprintf(g_file, "=== dung ghi nhat ky ===\n");
            std::fclose(g_file);
            g_file = nullptr;
        }
        g_enabled = false;
        return true;
    }

    // "w": moi lan bat la mot phien chan doan moi, khong de log cu lan vao.
    g_file = std::fopen(debugLogPath().c_str(), "w");
    if (!g_file) {
        return false;
    }
    g_enabled = true;
    writeHeader();
    return true;
}

bool debugLoggingEnabled() {
    static const bool fromEnv = [] {
        const char* v = std::getenv("OPENKEY_DEBUG");
        const bool on = v && *v && std::strcmp(v, "0") != 0;
        if (on) {
            // Chay tu terminal de chan doan: van in ra stderr nhu truoc.
            g_toStderr = true;
            g_enabled = true;
        }
        return on;
    }();
    (void)fromEnv;
    return g_enabled;
}

void debugLog(const char* tag, const char* format, ...) {
    if (!debugLoggingEnabled()) {
        return;
    }

    char message[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (g_toStderr) {
        std::fprintf(stderr, "[%s] %s\n", tag, message);
    }
    if (g_file) {
        // Dau thoi gian den mili giay: phan lon loi go la race condition / loi thu tu,
        // khong co moc thoi gian thi khong lan ra duoc.
        struct timespec ts {};
        clock_gettime(CLOCK_REALTIME, &ts);
        char stamp[32] = {};
        std::strftime(stamp, sizeof(stamp), "%H:%M:%S", std::localtime(&ts.tv_sec));
        std::fprintf(g_file, "%s.%03ld [%s] %s\n", stamp, ts.tv_nsec / 1000000, tag,
                     message);
        std::fflush(g_file); // treo may thi van con du log den truoc luc treo
    }
}

} // namespace openkey
