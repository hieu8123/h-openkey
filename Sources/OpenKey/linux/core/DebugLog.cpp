//
//  DebugLog.cpp
//  OpenKey cho Linux
//

#include "DebugLog.h"
#include "AsyncDebugLog.h"

#include <sys/stat.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace openkey {
namespace {

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
    mkdir(dir.c_str(), 0700);
    chmod(dir.c_str(), 0700);
    return dir + "/debug.log";
}

} // namespace

const std::string& debugLogPath() {
    static const std::string path = computeLogPath();
    return path;
}

bool setDebugLogging(bool on) {
    return AsyncDebugLog::instance().setFileEnabled(on, debugLogPath());
}

bool debugLoggingEnabled() {
    return AsyncDebugLog::instance().enabled();
}

bool debugFileLoggingEnabled() {
    return AsyncDebugLog::instance().fileEnabled();
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

    AsyncDebugLog::instance().enqueue(tag, message);
}

} // namespace openkey
