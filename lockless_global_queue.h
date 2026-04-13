#ifndef _LOCKLESS_GLOBAL_QUEUE_H_
#define _LOCKLESS_GLOBAL_QUEUE_H_

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <stdexcept>
#include <memory>
#include "measurement.h"

/*
T must be move constructible and move assignable, but not necessarily copyable.
Also T must have a default constructor, because the slot array is initialized with default constructed T objects.
*/

/*
alignas(64):
    Forces each atomic to begin on its own 64-byte boundary.
    This ensures:
    read lives in its own cache line
    write lives in its own cache line
    No false sharing
*/

/*
memory order:
    The producer must use release semantics when publishing the slot (updating seq), and the consumer must use acquire semantics when loading seq.
    This ensures that the write to the data happens-before the consumer reads it.
    Without acquire/release, the CPU could reorder operations and the consumer might observe a published slot before the data is fully written.

    “ordering is via seq acquire/release”.
*/

enum class queue_status{ OK, FULL, EMPTY, SHUTDOWN };

template <typename T>
class global_queue
{
private:

    typedef struct {
        std::atomic<uint64_t> seq;
        T data;
    } slot;

    std::unique_ptr<slot[]> vec;
    alignas(64) std::atomic<uint64_t> read;
    alignas(64) std::atomic<uint64_t> write;
    size_t total_capacity; // must be a power of 2
    size_t mask;
    std::atomic<bool> shut_down;
    
public:
    // capacity must be larger then 0
    global_queue(size_t capacity): read(0), write(0), total_capacity(capacity), mask(capacity -1), shut_down(false) {

        if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("illegal capacity value");
        }

        vec = (std::make_unique<slot[]>(total_capacity));

        for (size_t i = 0; i < total_capacity; i++) {
            vec[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    ~global_queue() = default;

    size_t capacity() const { return total_capacity;}

    /*
        “After calling push, the passed measurement object must not be used.”
        Call site                 What happens:
        push(measurement{...})    => move
        push(std::move(m))        => move
        push(m)	                  => copy
        push(const m)             => copy
    */
    queue_status push(T new_meas) {

        uint64_t write_tmp{};
        int64_t  diff{};
        slot *s = nullptr;

        while (true) {
            if (shut_down.load(std::memory_order_relaxed)) {
                return queue_status::SHUTDOWN;
            }

            write_tmp = write.load(std::memory_order_relaxed);
            s = &vec[write_tmp & mask];

            diff = (int64_t)s->seq.load(std::memory_order_acquire) - (int64_t)write_tmp;
            if (diff == 0) {
                // slot is free — try to reserve it
                if (write.compare_exchange_weak(write_tmp, write_tmp + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                    //successfully claimed the slot
                    break;
                }
                //slot accupied, try again
            }
            else if(diff < 0) {
                return queue_status::FULL;
            }
            else {
                //slot is being used by producer, try again
            }
        }

        s->data = std::move(new_meas);
        s->seq.store(write_tmp + 1, std::memory_order_release);        
        return queue_status::OK;
    }

    queue_status pop(T &meas) {   

        uint64_t read_tmp{};
        int64_t  diff{};
        slot *s = nullptr;

        while (true) {
            if (shut_down.load(std::memory_order_relaxed)) {
                return queue_status::SHUTDOWN;
            }        

            read_tmp = read.load(std::memory_order_relaxed);
            s = &vec[(read_tmp & mask)];
            diff = (int64_t)s->seq.load(std::memory_order_acquire) - (int64_t)(read_tmp + 1);

            if (diff == 0) {
                //measurment is avilable to pop

                if (read.compare_exchange_weak(read_tmp, read_tmp + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                    //successfully claimed the slot
                    break;
                } 
                else {
                    //slot is being used by consumer, try again to claim a slot
                }
            }
            else if(diff < 0) {
                //queue is empty
                return queue_status::EMPTY;
            }
            else {
                //slot is being used by producer, try again
            }      
        }
        meas = std::move(s->data);
        s->seq.store(read_tmp + total_capacity, std::memory_order_release);
        return queue_status::OK;
    }


    void shutdown() {

        if (shut_down.load(std::memory_order_acquire)) return;
        shut_down.store(true, std::memory_order_release);
    }
};

#endif