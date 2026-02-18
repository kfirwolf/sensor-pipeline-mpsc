#include <iostream>
#include <random>
#include "sensor_source.h"
#include <vector>
#include <ctime>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <sys/types.h>

class fake_sensor_source : public sensor_source
{
private:
    std::vector<uint8_t> internal_vec;
    size_t read_offset;
    std::atomic<bool> unblck_req;
    std::mutex m;
    std::condition_variable cv;
    
public:

    int stop_request() override {
        unblck_req = true;
        cv.notify_one();
        return 0;
    }

    fake_sensor_source(/* args */) : read_offset(0), unblck_req(false) {

        //setting size of vector
        internal_vec.resize(256);
        size_t int_vec_size = internal_vec.size();
        //fill vector with numbers
        for (size_t i = 0; i < int_vec_size; i++) {
            internal_vec[i] = i;
        }
    }

    ssize_t read_bytes(uint8_t* buf, size_t buf_len) override {
        //TODO: in real sensor source add while here 
        size_t i = 0;
        size_t int_vec_size = internal_vec.size();
        if (read_offset < int_vec_size) {
            size_t current_len = std::min(buf_len, int_vec_size - read_offset);
            while (i < current_len) {
                buf[i++] = internal_vec[read_offset++];    
            }
            return i;
        }

        //block
        std::unique_lock<std::mutex> lock(m);
        //unblock_req means “simulate end-of-stream”
        cv.wait(lock,[this] {return (unblck_req.load());});

        unblck_req = false;
        
        return 0; //TODO: in real sensor source :  (wait should wait for data to arrive and return should not be 0)
    }
};

