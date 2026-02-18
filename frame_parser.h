
/*
FrameParser (sensor-specific)
“The sensor will send bytes and some of them will be CRC/magic/payload size”

UART sensor A: magic + len + CRC
UART sensor B: fixed 8-byte frame + CRC
GPIO sensor: edge-triggered event (no framing)
SPI sensor: already packetized

If parser never finds sync:
If protocol defines sync → keep scanning
If protocol has no sync → continue best-effort parsing
Increment “desync” or “sync-not-found” counters
Infra remains alive

If CRC keeps failing:
Discard bad frames
Attempt resync if possible
Count failures
After threshold → signal degraded health / request reset
Never halt infra

If frames arrive faster than consumers:
Apply global queue policy (drop oldest)
Count drops
Raise warning (optional)
Keep system responsive

Inside the parser:
get a chunk of bytes
It keeps its own internal scratch buffer
It scans for magic
Once magic found → waits for enough bytes
Validates CRC
Emits a frame


The parser:
maintains its own internal buffer
consumes bytes incrementally
may:
discard bytes
wait for more
produce 0, 1, or many frames

It’s OK for the parser to receive chunks that don’t contain a complete frame because the parser is stateful:
it incrementally scans for sync, accumulates bytes, and only constructs a frame once enough aligned data exists.

Think of the parser like this:
It has a sliding window
Bytes drip in arbitrarily
Most of the time it says: “not enough info yet”
Sometimes it says: “sync found”
Later it says: “frame complete”
Everything else is ignored or buffered
That’s why incomplete chunks are not just OK — they are expected.

parser should never block (can return false from extract method if there are  no measurment ready)
parser does not add timestamp

*/

#ifndef _FRAME_PARSER_H
#define _FRAME_PARSER_H

#include <stdint.h>
#include <cstddef>
#include "measurement.h"

class frame_parser
{

public:

    virtual ~frame_parser() = default;
    virtual measurement extract_frame() = 0;
    virtual bool has_frame() const = 0;
    virtual void feed_bytes(const uint8_t *chunk, size_t len) = 0;
    virtual size_t error_count() const = 0;
    virtual size_t dropped_frames() const = 0;
};

#endif
