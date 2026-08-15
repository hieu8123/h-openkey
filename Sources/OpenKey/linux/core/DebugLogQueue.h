// Ring buffer MPSC co dinh: producer khong mutex, consumer duy nhat.

#ifndef OPENKEY_LINUX_DEBUG_LOG_QUEUE_H
#define OPENKEY_LINUX_DEBUG_LOG_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <time.h>

namespace openkey {

struct DebugLogRecord {
    struct timespec timestamp {};
    bool toFile = false;
    bool toStderr = false;
    char tag[24] {};
    char message[1024] {};
};

class DebugLogQueue {
public:
    DebugLogQueue() {
        for (size_t i = 0; i < kCapacity; ++i) _slots[i].sequence = i;
    }

    bool push(const DebugLogRecord& record) {
        size_t pos = _enqueue.load(std::memory_order_relaxed);
        Slot* slot;
        for (;;) {
            slot = &_slots[pos & (kCapacity - 1)];
            const size_t sequence = slot->sequence.load(std::memory_order_acquire);
            const intptr_t difference = static_cast<intptr_t>(sequence) -
                                        static_cast<intptr_t>(pos);
            if (difference == 0) {
                if (_enqueue.compare_exchange_weak(pos, pos + 1,
                                                   std::memory_order_relaxed)) {
                    break;
                }
            } else if (difference < 0) {
                return false;
            } else {
                pos = _enqueue.load(std::memory_order_relaxed);
            }
        }
        slot->record = record;
        slot->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool pop(DebugLogRecord& record) {
        const size_t pos = _dequeue.load(std::memory_order_relaxed);
        Slot& slot = _slots[pos & (kCapacity - 1)];
        const size_t sequence = slot.sequence.load(std::memory_order_acquire);
        if (static_cast<intptr_t>(sequence) -
                static_cast<intptr_t>(pos + 1) != 0) {
            return false;
        }
        record = slot.record;
        _dequeue.store(pos + 1, std::memory_order_relaxed);
        slot.sequence.store(pos + kCapacity, std::memory_order_release);
        return true;
    }

private:
    static constexpr size_t kCapacity = 2048; // luy thua hai
    struct Slot {
        std::atomic<size_t> sequence{0};
        DebugLogRecord record;
    };

    Slot _slots[kCapacity];
    std::atomic<size_t> _enqueue{0};
    std::atomic<size_t> _dequeue{0};
};

} // namespace openkey

#endif // OPENKEY_LINUX_DEBUG_LOG_QUEUE_H
