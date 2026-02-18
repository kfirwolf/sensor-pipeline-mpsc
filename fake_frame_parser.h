#include "frame_parser.h"
#include <cassert>


class fake_frame_parser : public frame_parser {
    private:
    static constexpr size_t frame_size = 8;
    std::vector<uint8_t> buf;


    public:

    fake_frame_parser() {

    }


    void feed_bytes(const uint8_t *chunk, size_t len) override {
        buf.insert(buf.end(),  chunk, chunk + len);
    }

    measurement extract_frame() override {
        assert(has_frame());
        measurement m;

        m.payload.insert(m.payload.end(), buf.begin(), buf.begin() +frame_size);
        buf.erase(buf.begin(), buf.begin() +frame_size);
        return m;
    }

    bool has_frame() const override {
        return (buf.size() >= frame_size);
    }

    size_t error_count() const override {
        return 0;
    }

    size_t dropped_frames() const override {
        return 0;
    }    
};