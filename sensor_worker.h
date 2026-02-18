/*
    Read bytes from sensor (kernel / driver)

    Append bytes to its own stream_buffer

    Feed bytes into its own FrameParser

    When a frame is ready:

    attach timestamp

    push to global queue (with drop-oldest policy)
*/
#include <chrono>
#include <atomic>
#include <thread>
#include <algorithm>
#include <iostream>
#include <vector>
#include "stream_buffer.h"
#include "frame_parser.h"
#include "uart_frame_parser.h"
#include "global_queue.h"
#include "sensor_source.h"
#include "uart_sensor_source.h"
#include "measurement.h"


class sensor_worker {

    private:
        static constexpr size_t MAX_SOURCE_READ_BUFFER = 256; //bytes
        static constexpr size_t PARSER_CHUNK_SIZE = 64; //bytes
        stream_buffer st_buffer;
        size_t sensor_id;
        frame_parser &f_parser;
        global_queue<measurement> &global_q;
        sensor_source &s_source;
        std::atomic<bool> stop_req;
        bool started;
        size_t read_errors;
        size_t eos_count;
        size_t stream_overflow_bytes;
        std::thread worker_thread;

        /*
        run() must exit when any of these happen:
        stop_req == true
        sensor_source.read_bytes() returns 0 / error
        global_q.push() returns false
        */
        void run() {
            uint8_t chunk[PARSER_CHUNK_SIZE];
            std::vector<uint8_t> read_buffer(std::min(MAX_SOURCE_READ_BUFFER, st_buffer.get_capacity()));
            ssize_t num_of_bytes_from_sensor = 0;
            size_t s_buf_num_of_bytes = 0;
       
            while (!stop_req.load()) {
            /*
                >0  : bytes read
                =0  : permanent end-of-stream (device closed / shutdown/stop request )
                <0  : <0 : error (caller retries / counts)   
            */                             
                num_of_bytes_from_sensor = s_source.read_bytes(read_buffer.data(), read_buffer.size());

                if (num_of_bytes_from_sensor == 0) {
                    //std::cout << "end-of-stream" << std::endl;
                    eos_count++;
                    break;
                }

                if (num_of_bytes_from_sensor < 0) {
                    //std::cout << "error" << std::endl;
                    read_errors++;
                    continue;
                }                

                s_buf_num_of_bytes = st_buffer.append(read_buffer.data(), static_cast<size_t>(num_of_bytes_from_sensor));
                if (s_buf_num_of_bytes < static_cast<size_t>(num_of_bytes_from_sensor)) {
                    stream_overflow_bytes += static_cast<size_t>(num_of_bytes_from_sensor) - s_buf_num_of_bytes;
                    //std::cout << "ERROR: stream buffer avilable bytes:  " << s_buf_num_of_bytes << "< " << "bytes count from sensor: " << num_of_bytes_from_sensor <<std::endl;
                    //stream buffer capacity is to small for the amount of data from sensor
                    //lost data do to stream buffer , can get statistics from this
                }

                // Extraction from the stream buffer append to parser and to globle queue:
                while (st_buffer.available() > 0) {
                    size_t min_extract = std::min(st_buffer.available() ,PARSER_CHUNK_SIZE);
                    
                    if (!st_buffer.extract(chunk, min_extract)) {
                        //should no happened , maybe throw a exception
                        return;
                    }

                    //std::cout << "extracted " << min_extract << " bytes from stream buffer" << std::endl; 
                    f_parser.feed_bytes(chunk, min_extract);

                    //push to global queue
                    while (f_parser.has_frame()) {
                        measurement meas = f_parser.extract_frame();
                        meas.sensor_id = this->sensor_id;
                        meas.system_timestamp = std::chrono::steady_clock::now();
                        if (global_q.push(std::move(meas)) == false) {
                            return;
                            //shot down proc
                        }
                    }
                }
            } 

            //read_bytes() which is blocking
            //bytes save at tmp buffer
            //all tmp buffer and it size append to stream buffer
            //extract chunks from string buffer and passed to parser
            //pull measurements from parser and passed to globle_queue
            //must exit run when globle_queue return false
        }


    public:
        sensor_worker(size_t stream_buffer_size, size_t sensorid, sensor_source &sen_s, frame_parser &f_prsr, global_queue<measurement> &g_q): st_buffer(std::max(stream_buffer_size, PARSER_CHUNK_SIZE)), sensor_id(sensorid), f_parser(f_prsr), global_q(g_q), s_source(sen_s), stop_req{false}, started{false}, read_errors(0), eos_count(0), stream_overflow_bytes(0) {
        }

        ~sensor_worker() {
            stop();
        }

        size_t get_sensor_id() const {
            return sensor_id;
        }
        // start() may be called only once per object lifetime
        // start() is not thread-safe; must be called from a single control thread        
        bool start() {
            //can be executed only once
            if (started) {
                return false;
            }

            started = true;
            stop_req = false;
            //start a new thread
            //thread execute run()
            worker_thread = std::thread(&sensor_worker::run, this);
            return true;
        }

        void stop() {

            //Set a stop_requested flag
            if (stop_req.exchange(true) || !started) {
                return;
            }
            //Signal sensor_source to unblock
            s_source.stop_request();
            //Join the worker thread
            if (worker_thread.joinable()) {
                worker_thread.join();
            }
            started = false;
        }
        
        size_t get_read_errors_count() {
            return read_errors;
        }

        size_t get_eos_count() {
            return eos_count;
        }  
};
