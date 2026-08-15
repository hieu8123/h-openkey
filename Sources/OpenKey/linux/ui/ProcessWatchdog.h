// Watchdog fail-fast cho driver ban phim.

#ifndef OPENKEY_LINUX_PROCESS_WATCHDOG_H
#define OPENKEY_LINUX_PROCESS_WATCHDOG_H

#include <atomic>
#include <thread>

namespace openkey {

class IBackend;

class ProcessWatchdog {
public:
    explicit ProcessWatchdog(const IBackend& backend);
    ~ProcessWatchdog();

    void start();
    void stop();

private:
    void run();

    const IBackend& _backend;
    std::atomic<bool> _running{false};
    std::thread _thread;
};

} // namespace openkey

#endif // OPENKEY_LINUX_PROCESS_WATCHDOG_H
