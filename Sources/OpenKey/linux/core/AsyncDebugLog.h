#ifndef OPENKEY_LINUX_ASYNC_DEBUG_LOG_H
#define OPENKEY_LINUX_ASYNC_DEBUG_LOG_H

#include <cstdio>
#include <string>

namespace openkey {

struct DebugLogRecord;

class AsyncDebugLog {
public:
    static AsyncDebugLog& instance();

    bool setFileEnabled(bool enabled, const std::string& path);
    bool fileEnabled() const;
    bool enabled();
    void enqueue(const char* tag, const char* message);

    ~AsyncDebugLog();

private:
    AsyncDebugLog();
    AsyncDebugLog(const AsyncDebugLog&) = delete;
    AsyncDebugLog& operator=(const AsyncDebugLog&) = delete;

    class State;
    State* _state;
};

} // namespace openkey

#endif // OPENKEY_LINUX_ASYNC_DEBUG_LOG_H
