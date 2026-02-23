#ifndef _LOC_GLOBAL_QUEUE_H_
#define _LOC_GLOBAL_QUEUE_H_

#include <cstdint>
#include <cstddef>
#include <vector>
#include <atomic>
#include <stdexcept> 
#include "measurement.h"

/*


*/

template <typename T>
class global_queue
{
private:

    typedef struct {
        std::atomic<uint64_t> seq;
        T data;
    } slot;

    std::vector<slot> vec;
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

        vec.resize(capacity);
        for (size_t i = 0; i < capacity; i++) {
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
    bool push(T new_meas) {

        uint64_t p{};
        intptr_t diff = 0;
        slot *s = nullptr;

        while (true) {
            if (shut_down.load(std::memory_order_relaxed)) {
                return false;
            }

            p = write.load(std::memory_order_relaxed);
            s = &vec[p & mask];

            diff = (int64_t)s->seq.load(std::memory_order_acquire) - (int64_t)p;
            if (diff == 0) {
                // slot is free — try to reserve it
                if (write.compare_exchange_weak(p, p + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                    //successfully claimed the slot
                    break;
                }
                //slot accupied, try again
            }
            else if(diff < 0) {
                //queue is full
                return false;
            }
            else {
                //slot is being used by producer, try again
            }
        }

        s->data = std::move(new_meas);
        s->seq.store(p + 1, std::memory_order_release);        
        return true;
    }

    //blocking consumer function
    bool pop(T &meas) {   

        uint64_t p{};
        intptr_t diff = 0;
        slot *s = nullptr;

        while (true) {
            if (shut_down.load(std::memory_order_relaxed)) {
                return false;
            }        

            p = read.load(std::memory_order_relaxed);
            s = &vec[p & mask];
            diff = (intptr_t)s->seq.load(std::memory_order_acquire) - (intptr_t)(p + 1);

            if (diff == 0) {
                //measurment is avilable to pop

                if (read.compare_exchange_weak(p, p + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                    //successfully claimed the slot
                    break;
                } 
                else {
                    //slot is being used by consumer, try again to claim a slot
                }
            }
            else if(diff < 0) {
                //queue is empty
                return false;
            }
            else {
                //slot is being used by producer, try again
            }      
        }
        meas = std::move(s->data);
        s->seq.store(p + total_capacity, std::memory_order_release);
        return true;
    }


    void shutdown() {

        if (shut_down.load(std::memory_order_relaxed)) return;
        std::unique_lock<std::mutex> lock(m);
        shut_down.store(true, std::memory_order_relaxed);
    }
};

#endif