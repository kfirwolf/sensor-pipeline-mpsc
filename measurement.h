#ifndef _MEASUREMENT_H_
#define _MEASUREMENT_H_

#include <vector>
#include <cstdint>
#include <chrono>

struct measurement {

    std::vector<uint8_t> payload;
    std::chrono::steady_clock::time_point system_timestamp{};
    size_t sensor_id = 0;
    size_t sequence_number = 0;
};

#endif