# sensor-pipeline-mpsc

Embedded-style multi-threaded sensor ingestion pipeline in C++:

**sensor_source → stream_buffer → frame_parser → global_queue (MPSC) → consumer**

This project demonstrates a practical producer-consumer architecture that is common in embedded Linux systems:
multiple sensor threads ingest raw bytes, parse frames, and publish structured measurements into a bounded global queue.

---

## Key Features

- **Multi-threaded design**: one worker thread per sensor
- **MPSC global queue**: multiple producers, single consumer
- **Bounded capacity**: fixed-size buffers and queues (embedded-friendly)
- **Stream buffering**: decouple read chunk size from parser requirements
- **Frame parsing**: UART-style framing logic (SYNC | LEN | PAYLOAD | CRC)
- **Graceful stop**: stop request wakes up blocking I/O using `poll()` + `eventfd`
- **Fake sensor source + fake parser** for easy local testing without hardware

---

## Architecture (High Level)

Each sensor is managed by a `sensor_worker` thread:

┌──────────────┐ bytes ┌──────────────┐ chunks ┌──────────────┐ frames ┌──────────────┐
│ sensor_source │ ───────▶ │ stream_buffer │ ────────▶ │ frame_parser │ ───────▶ │ global_queue │
└──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘
│
▼
┌──────────────┐
│ consumer │
└──────────────┘


---

## Core Components

- `sensor_manager`  
  Owns all sensors and their workers. Responsible for creation and lifecycle.

- `sensor_worker`  
  Runs in its own thread. Reads bytes from a sensor source, feeds the parser, and pushes parsed measurements into the global queue.

- `sensor_source`  
  Abstract interface for reading raw bytes.
  - `uart_sensor_source` (real Linux UART)
  - `fake_sensor_source` (testing)

- `frame_parser`  
  Abstract interface for parsing frames from a byte stream.
  - `uart_frame_parser`
  - `fake_frame_parser`

- `stream_buffer`  
  Bounded FIFO buffer used between the source and parser.

- `global_queue`  
  Bounded MPSC queue of `measurement` objects.

---

## Stop / Shutdown Sequence

![Stop Sequence](docs/diagrams/mpsc_stop_seq.svg)

---

## Measurement Format

Parsed frames are published as:

```cpp
struct measurement {
    std::vector<uint8_t> payload;
    std::chrono::steady_clock::time_point system_timestamp{};
    size_t sensor_id = 0;
    size_t sequence_number = 0;
};
