#include "frame_parser.h"
#include <cassert>
#include <deque>

class uart_frame_parser : public frame_parser
{
private:

enum parsing_states {
    WAIT_SYNC,
    READ_LEN,
    READ_PAYLOAD,
    READ_CRC
};

size_t frames_dropped;
size_t error_counter;
parsing_states parse_state;
size_t payload_len;
size_t payload_index;
uint8_t crc_acc;
static constexpr size_t measurements_buffer_size = 4; 
static constexpr size_t max_payload_len = 64; //bytes
static constexpr uint8_t sync = 0xAA;
static constexpr uint8_t polynomial = 0x07;
std::vector<uint8_t> measurement_bytes;
std::deque<measurement> measurements_buffer;

static void update_crc(uint8_t &crc_acc, uint8_t payload_byte) {
    crc_acc ^= payload_byte;

    for (size_t i = 0; i < 8; i++) {

        if (crc_acc & 0x80) {
            crc_acc = (crc_acc << 1) ^ polynomial;
        }
        else {
            crc_acc = (crc_acc << 1);
        }
    }
}

public:
    uart_frame_parser(/* args */) : frames_dropped(0), error_counter(0),  parse_state(WAIT_SYNC), payload_len(0), payload_index(0), crc_acc(0) {
    }

    ~uart_frame_parser() {

    }

    void feed_bytes(const uint8_t *chunk, size_t len) override {
        size_t index = 0;
        //starting state machine
        while (index < len) {
            switch (parse_state) {
                case(WAIT_SYNC):

                    if (chunk[index] != sync) {
                        index++;
                        break;
                    }

                    parse_state = READ_LEN;
                    index++;
                    crc_acc = 0;
                    break;

                case(READ_LEN):
                    
                    payload_len = chunk[index];
                    if (payload_len > max_payload_len) {
                        //failed, resync
                        parse_state = WAIT_SYNC;
                        break;
                    }

                    if (measurement_bytes.size() < payload_len) {
                        measurement_bytes.resize(payload_len);
                    }

                    payload_index = 0;
                    parse_state = READ_PAYLOAD;
                    index++;                
                    break;

                case(READ_PAYLOAD):

                    if(payload_index < payload_len) {
                        update_crc(crc_acc, chunk[index]);
                        measurement_bytes[payload_index++] = chunk[index++];
                    }
                    if (payload_index == payload_len) {
                        parse_state = READ_CRC;
                    }
                    break;

                case(READ_CRC):
                    if (chunk[index] == crc_acc) {
                        crc_acc = 0;
                        if (measurements_buffer.size() < measurements_buffer_size) {

                            measurement m;
                            m.payload = std::move(measurement_bytes);
                            measurement_bytes.clear();
                            measurements_buffer.push_back(std::move(m));
                        }
                        else {
                            frames_dropped++;
                        }
                    }
                    else {
                        error_counter++;
                        crc_acc = 0;

                    }

                    parse_state = WAIT_SYNC;
                    index++;
                    break;
            };
        }
        return;
    }

    // Caller must call has_frame() before extract_frame().
    measurement extract_frame() override {
        assert(has_frame());
        measurement m = std::move(measurements_buffer.front());
        measurements_buffer.pop_front();
        return m;
    }

    bool has_frame() const override {
        return (measurements_buffer.size() > 0);
    }

    size_t error_count() const override {
        return error_counter;
    }

    size_t dropped_frames() const override {
        return frames_dropped;
    }

};

