#include <vector>
#include <cstdint>
#include <cstddef>

// NOTE: This class is NOT thread-safe.
// Assumes single producer and single consumer :
// Assumes exactly one thread calls append()
// and exactly one thread calls extract().


class stream_buffer {

    public:
    stream_buffer(size_t buf_size) : capacity(buf_size), read(0), write(0), current_size(0) {
        vec.resize(capacity);
    }
    ~stream_buffer() = default;

    bool extract(uint8_t *out_buffer, size_t len);
    size_t append(const uint8_t *in_buffer, size_t len); //return actually written bytes
    size_t get_capacity() const { return capacity;}
    size_t available() const {return current_size;}


    private:
    std::vector<uint8_t> vec;
    size_t capacity;
    size_t read;
    size_t write;
    size_t current_size;
};