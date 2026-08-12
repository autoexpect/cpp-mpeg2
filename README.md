# cpp-mpeg2

A lightweight C++ library for MPEG-2 Transport Stream (TS) muxing. It provides a
simple API to multiplex H.264/H.265 video and AAC audio into standard MPEG-2 TS.

## Features

- **Video Support**: H.264 (AVC) and H.265 (HEVC), Annex-B byte stream
- **Audio Support**: AAC (ADTS)
- **Standard Compliance**: Generates standard MPEG-2 Transport Streams (ISO/IEC 13818-1)
- **Easy to Use**: Simple C++11 API with callback-based output
- **No Dependencies**: Pure C++ implementation without external dependencies

## Build

```bash
make
```

This builds the static library `libmpeg2.a` and the example program `ts_test`.
`CXXFLAGS` can be overridden, e.g. `make CXXFLAGS="-O1 -g -fsanitize=address"`.

## Usage

### Basic Example

```cpp
#include "mpeg2/ts_muxer.h"
#include <fstream>
#include <vector>

using namespace mpeg2;

int main() {
    TSMuxer muxer;
    std::ofstream out("output.ts", std::ios::binary);

    // Add streams. Each call returns the PID assigned to that stream,
    // or TS_INVALID_PID if none could be allocated.
    uint16_t video_pid = muxer.AddStream(TS_STREAM_H264);
    uint16_t audio_pid = muxer.AddStream(TS_STREAM_AAC);

    // Each finished 188-byte packet is handed to this callback.
    muxer.SetOnPacket([&](const std::vector<uint8_t>& packet) {
        out.write((const char*)packet.data(), packet.size());
    });

    // pts/dts are in 90 kHz units (TS_CLOCK_HZ): 40 ms == 3600 ticks.
    uint64_t pts = 0;
    for (/* each frame */;;) {
        muxer.Write(video_pid, frame_data, frame_size, pts, pts);
        pts += 3600;
    }
    return 0;
}
```

### Timestamps

`TSMuxer::Write` takes `pts` and `dts` in **90 kHz units**, the MPEG-2 system
clock. To convert from milliseconds, multiply by 90. Pass `pts == dts` for
streams without B-frames; when they differ, both are written to the PES header.

### Input format

`Write` expects one complete access unit per call:

- **H.264 / H.265**: Annex-B byte stream (`00 00 00 01` start codes). An access
  unit delimiter is inserted automatically when the frame does not carry one, and
  frames containing an IDR are flagged as random access points. To keep the stream
  decodable from any random access point, repeat SPS/PPS (and VPS for H.265) ahead
  of each IDR — see `examples/ts_test.cpp`.
- **AAC**: one ADTS frame, header included. `SplitAACFrame` splits an ADTS
  bitstream into frames, and `ADTSSampleRate` gives the sample rate needed to
  compute timestamps.

PAT and PMT are emitted automatically every 400 ms. The PCR is carried on a video
stream when the program has one, otherwise on the first stream written.

### Limits

A program is limited to 33 elementary streams, since PSI sections are never split
across transport packets. Oversized sections are dropped with a message on
`stderr` rather than emitted as corrupt packets.

### Running the Example

```bash
# Usage: ./ts_test <input_video> <output_ts> [interval_ms] [input_aac]

# Mux H.264 video only
./ts_test input.h264 output.ts

# Mux H.265 video (file extension must be .h265 or .hevc)
./ts_test input.h265 output.ts

# Mux with AAC audio at 25 fps; audio loops if shorter than the video
./ts_test input.h264 output.ts 40 input.aac
```

## Project Structure

- `include/mpeg2/`: Header files
- `src/`: Source code
- `examples/`: Example usage code

## License

MIT License
