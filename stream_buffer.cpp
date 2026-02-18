#include "stream_buffer.h"

/*
append():
- Copies bytes into the stream buffer.
- If input length exceeds buffer capacity, only the newest bytes are kept.
- If the buffer is full, oldest buffered bytes are dropped (drop-oldest policy).
- Returns the number of bytes copied from the input.
*/
size_t stream_buffer::append(const uint8_t *in_buffer, size_t len) {

    if (len == 0) {
        return 0;
    }

    const uint8_t *buffer = in_buffer;
    size_t current_len = len;

    if (len > capacity) {
        buffer = in_buffer + (len - capacity);
        current_len = capacity;
    }

    for (size_t i = 0; i < current_len; i++) {
        vec[write] = buffer[i];
        if (current_size == capacity) {
            read = (read + 1) == capacity ? 0 : read + 1;
        }
        write = (write + 1) == capacity ? 0 : write + 1;
        if (current_size < capacity) {
            current_size++;
        }
    }
    
    return current_len;
}


// extract():
// Atomically extracts exactly `len` bytes into caller-provided buffer.
// If fewer than `len` bytes are available, no data is extracted and false is returned.
// On success, FIFO order is preserved and buffer size is reduced by `len`.
bool stream_buffer::extract(uint8_t *out_buffer, size_t len) {

    if (len == 0) {
        return true;
    }

    if (out_buffer == nullptr || current_size < len) {
        return false;
    }

    size_t local_read = read;
    for (size_t i = 0; i < len; i++) {
        out_buffer[i] = vec[local_read];
        local_read = (local_read + 1) == capacity ? 0 : local_read + 1;
    }

    current_size-= len;
    read = local_read;
    
    return true;
}