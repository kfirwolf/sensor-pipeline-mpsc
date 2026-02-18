#ifndef _GLOBAL_QUEUE_H_
#define _GLOBAL_QUEUE_H_

#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdexcept> 
#include "measurement.h"

/*
    This class is suited for MPSC
*/

template <typename T>
class global_queue
{
private:
    std::vector<T> vec;
    size_t read;
    size_t write;
    size_t total_capacity;
    size_t current_size;
    std::mutex m;
    std::condition_variable cv;
    bool shut_down;
    void advanced_read() {
        read = (read == (total_capacity -1)) ? 0 : read +1;
    }

    void advanced_write() {
        write = (write == (total_capacity -1)) ? 0 : write +1;
    }

public:
    // capacity must be larger then 0
    global_queue(size_t capacity): read(0), write(0), total_capacity(capacity), current_size(0), shut_down(false) {

        if (capacity == 0) {
            throw std::invalid_argument("illegal capacity value");
        }

        vec.resize(capacity);
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
        std::unique_lock<std::mutex> lock(m);
        if (shut_down) {
            return false;
        }

        //push to buffer
        vec[write] = std::move(new_meas);
        if (current_size == total_capacity) {
            advanced_read();
        }
        advanced_write();
        if (current_size < total_capacity) {
            current_size++;
        }
        
        lock.unlock();
        cv.notify_one();
        return true;
    }

    //blocking consumer function
    bool pop(T &meas) {   
        std::unique_lock<std::mutex> lock(m);

        // this line cv.wait(lock, predicate) is the “safe” overload, its equivlent to:
        //while (!predicate()) {
            //cv.wait(lock);

        cv.wait(lock, [this]{return (shut_down || current_size != 0);}); 

        if (shut_down && current_size == 0) {
            return false;
        }

        //pop from buffer
        meas = vec[read];
        advanced_read();
        current_size--;

        return true;
    }

    //non blocking consumer function
    bool try_pop(T &meas) {
        std::unique_lock<std::mutex> lock(m);

        if (current_size == 0) {
            return false;
        } 

        meas = vec[read];
        advanced_read();
        current_size--;

        return true;
    }

    void shutdown() {

        if (shut_down) return;
        std::unique_lock<std::mutex> lock(m);
        shut_down = true;
        lock.unlock();
        cv.notify_all();
    }
};

#endif