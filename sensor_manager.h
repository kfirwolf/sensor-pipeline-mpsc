#include <iostream>
#include <vector>
#include <memory>
#include "sensor_source.h"
#include "uart_sensor_source.h"
#include "fake_sensor_source.h"
#include "sensor_worker.h"
#include "global_queue.h"
#include "frame_parser.h"
#include "uart_frame_parser.h"
#include "fake_frame_parser.h"
#include "measurement.h"


enum class sensor_type {
    UART,
    GPIO,
    I2C,
    SPI,
    FAKE
};

struct sensor_config {
    sensor_type type;
    size_t stream_buffer_size;
    uart_config uart_conf; //only use for uart sensors
};

class sensor_manager {
private:
    size_t sensor_id;
    global_queue<measurement> g_queue;
    std::vector<std::unique_ptr<sensor_source>> sensor_sources;
    std::vector<std::unique_ptr<frame_parser>> frame_parsers;
    std::vector<std::unique_ptr<sensor_worker>> sensor_workers;

public:
    sensor_manager(size_t globle_q_capacity) : sensor_id(0), g_queue(globle_q_capacity) {
    }

    ~sensor_manager() = default;

    void add_sensor(const sensor_config& s_config) {
        switch (s_config.type)
        {
        case sensor_type::UART:
            sensor_sources.push_back(std::make_unique<uart_sensor_source>(s_config.uart_conf));
            frame_parsers.push_back(std::make_unique<uart_frame_parser>());
            sensor_workers.push_back(std::make_unique<sensor_worker>(s_config.stream_buffer_size, sensor_id++, *sensor_sources.back(), *frame_parsers.back(), g_queue));
            break;
        case sensor_type::FAKE:
            sensor_sources.push_back(std::make_unique<fake_sensor_source>());
            frame_parsers.push_back(std::make_unique<fake_frame_parser>());
            sensor_workers.push_back(std::make_unique<sensor_worker>(s_config.stream_buffer_size, sensor_id++, *sensor_sources.back(), *frame_parsers.back(), g_queue));
            break;
        default:
            throw std::runtime_error("Unsupported sensor type");
        }
    }

    void start_all() {

        for ( auto &worker : sensor_workers) {
            worker->start();
        }
    }

    void stop_all() {
        for (auto &worker : sensor_workers) {
            worker->stop();
        }
        g_queue.shutdown();
    }
};


