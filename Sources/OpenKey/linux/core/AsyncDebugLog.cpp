#include "AsyncDebugLog.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <thread>

#include "DebugLogQueue.h"

namespace openkey {

namespace {

void writeSessionMarker(FILE* file, const char* message) {
    const std::time_t now = std::time(nullptr);
    char stamp[64] = {};
    struct tm local {};
    localtime_r(&now, &local);
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
    std::fprintf(file, "=== H-OpenKey: %s luc %s ===\n", message, stamp);
    std::fflush(file);
}

} // namespace

class AsyncDebugLog::State {
public:
    DebugLogQueue queue;
    std::atomic<bool> fileEnabled{false}, toStderr{false}, running{false};
    std::atomic<uint32_t> producers{0};
    std::atomic<uint64_t> accepted{0}, consumed{0}, flushed{0};
    std::atomic<uint64_t> droppedFile{0}, droppedStderr{0};
    std::atomic<uint64_t> reportedFile{0}, reportedStderr{0};
    std::once_flag envOnce;
    std::mutex control, waitMutex, drainMutex;
    std::condition_variable wake, drained;
    std::thread flusher;
    std::atomic<FILE*> file{nullptr};

    bool startLocked() {
        if (running.load()) return true;
        running = true;
        try { flusher = std::thread([this] { loop(); }); }
        catch (...) { running = false; return false; }
        return true;
    }

    void initEnvironment() {
        std::call_once(envOnce, [this] {
            const char* value = std::getenv("OPENKEY_DEBUG");
            if (!value || !*value || std::strcmp(value, "0") == 0) return;
            std::lock_guard<std::mutex> lock(control);
            if (startLocked()) toStderr = true;
        });
    }

    void writeRecord(const DebugLogRecord& record) {
        struct tm local {};
        localtime_r(&record.timestamp.tv_sec, &local);
        char stamp[32] = {};
        std::strftime(stamp, sizeof(stamp), "%H:%M:%S", &local);
        if (record.toStderr) {
            std::fprintf(stderr, "[%s] %s\n", record.tag, record.message);
        }
        FILE* output = file.load(std::memory_order_acquire);
        if (record.toFile && output) {
            std::fprintf(output, "%s.%03ld [%s] %s\n", stamp,
                         record.timestamp.tv_nsec / 1000000, record.tag,
                         record.message);
        }
    }

    void flushCycle() {
        const uint64_t fileDropped =
            droppedFile.load(std::memory_order_acquire);
        const uint64_t stderrDropped =
            droppedStderr.load(std::memory_order_acquire);
        const uint64_t oldFile = reportedFile.load(std::memory_order_relaxed);
        const uint64_t oldStderr =
            reportedStderr.load(std::memory_order_relaxed);
        FILE* output = file.load(std::memory_order_acquire);
        if (output && fileDropped > oldFile) {
            std::fprintf(output,
                         "[debug] [dropped %llu] ring buffer day; nhat ky co "
                         "the bi thieu ban ghi\n",
                         static_cast<unsigned long long>(fileDropped - oldFile));
        }
        if (stderrDropped > oldStderr) {
            std::fprintf(stderr,
                         "[debug] [dropped %llu] ring buffer day; nhat ky co "
                         "the bi thieu ban ghi\n",
                         static_cast<unsigned long long>(stderrDropped - oldStderr));
        }
        if (output) std::fflush(output);
        if (toStderr.load() || stderrDropped > oldStderr) std::fflush(stderr);

        // Cong bo marker chi sau khi I/O da flush. Nguoi tat log co the doi
        // hai counter nay ma khong dong file trong luc flusher dang ghi.
        if (output || fileDropped == oldFile) {
            reportedFile.store(fileDropped, std::memory_order_release);
        }
        reportedStderr.store(stderrDropped, std::memory_order_release);
        flushed.store(consumed.load(std::memory_order_acquire),
                      std::memory_order_release);
        drained.notify_all();
    }

    void loop() {
        while (running.load(std::memory_order_acquire)) {
            DebugLogRecord record;
            size_t count = 0;
            while (count < 1024 && queue.pop(record)) {
                writeRecord(record);
                consumed.fetch_add(1, std::memory_order_release);
                ++count;
            }
            flushCycle();
            if (count == 1024) continue;
            std::unique_lock<std::mutex> lock(waitMutex);
            wake.wait_for(lock, std::chrono::milliseconds(200));
        }
        DebugLogRecord record;
        while (queue.pop(record)) {
            writeRecord(record);
            consumed.fetch_add(1, std::memory_order_release);
        }
        flushCycle();
    }

    void waitForProducers() {
        while (producers.load(std::memory_order_acquire) != 0) {
            std::this_thread::yield();
        }
    }

    void waitUntilFlushed(uint64_t target, uint64_t fileDropTarget,
                          uint64_t stderrDropTarget) {
        wake.notify_one();
        std::unique_lock<std::mutex> lock(drainMutex);
        drained.wait(lock, [this, target, fileDropTarget, stderrDropTarget] {
            return flushed.load(std::memory_order_acquire) >= target &&
                   reportedFile.load(std::memory_order_acquire) >=
                       fileDropTarget &&
                   reportedStderr.load(std::memory_order_acquire) >=
                       stderrDropTarget;
        });
    }
};

AsyncDebugLog& AsyncDebugLog::instance() {
    static AsyncDebugLog logger;
    return logger;
}

AsyncDebugLog::AsyncDebugLog() : _state(new State) {}

AsyncDebugLog::~AsyncDebugLog() {
    setFileEnabled(false, {});
    _state->toStderr = false;
    _state->waitForProducers();
    _state->waitUntilFlushed(_state->accepted.load(),
                             _state->droppedFile.load(),
                             _state->droppedStderr.load());
    _state->running = false;
    _state->wake.notify_one();
    if (_state->flusher.joinable()) _state->flusher.join();
    delete _state;
}

bool AsyncDebugLog::setFileEnabled(bool enabled, const std::string& path) {
    _state->initEnvironment();
    std::unique_lock<std::mutex> lock(_state->control);
    if (enabled == _state->fileEnabled.load()) return true;
    if (enabled) {
        const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC |
                           O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd < 0) return false;
        fchmod(fd, 0600);
        FILE* opened = fdopen(fd, "w");
        if (!opened) { close(fd); return false; }
        setvbuf(opened, nullptr, _IOFBF, 64 * 1024);
        if (!_state->startLocked()) { std::fclose(opened); return false; }
        writeSessionMarker(opened, "bat dau ghi nhat ky");
        _state->file.store(opened, std::memory_order_release);
        _state->fileEnabled = true;
        return true;
    }
    _state->fileEnabled = false;
    lock.unlock();
    _state->waitForProducers();
    _state->waitUntilFlushed(_state->accepted.load(),
                             _state->droppedFile.load(),
                             _state->droppedStderr.load());
    lock.lock();
    if (FILE* output = _state->file.exchange(nullptr, std::memory_order_acq_rel)) {
        writeSessionMarker(output, "dung ghi nhat ky");
        std::fclose(output);
    }
    return true;
}

bool AsyncDebugLog::fileEnabled() const { return _state->fileEnabled.load(); }

bool AsyncDebugLog::enabled() {
    _state->initEnvironment();
    return _state->fileEnabled.load() || _state->toStderr.load();
}

void AsyncDebugLog::enqueue(const char* tag, const char* message) {
    _state->producers.fetch_add(1, std::memory_order_acq_rel);
    DebugLogRecord record;
    record.toFile = _state->fileEnabled.load(std::memory_order_acquire);
    record.toStderr = _state->toStderr.load(std::memory_order_acquire);
    if (record.toFile || record.toStderr) {
        clock_gettime(CLOCK_REALTIME, &record.timestamp);
        std::snprintf(record.tag, sizeof(record.tag), "%s", tag);
        std::snprintf(record.message, sizeof(record.message), "%s", message);
        if (_state->queue.push(record)) {
            _state->accepted.fetch_add(1);
        } else {
            if (record.toFile) _state->droppedFile.fetch_add(1);
            if (record.toStderr) _state->droppedStderr.fetch_add(1);
            _state->wake.notify_one();
        }
    }
    _state->producers.fetch_sub(1, std::memory_order_release);
}

} // namespace openkey
