# cpp-mpeg2

A lightweight, header-only capable C++ library for MPEG-2 Transport Stream (TS) muxing. This library provides a simple API to multiplex H.264/H.265 video and AAC audio into standard MPEG-2 TS format.

## Features

- **Video Support**: H.264 (AVC) and H.265 (HEVC)
- **Audio Support**: AAC (ADTS)
- **Standard Compliance**: Generates standard MPEG-2 Transport Streams (ISO/IEC 13818-1)
- **Easy to Use**: Simple C++11 API with callback-based output
- **No Dependencies**: Pure C++ implementation without external dependencies

## Build

The project comes with a simple Makefile.

```bash
make
```

This will build the static library `libmpeg2.a` and the example program `ts_test`.

## Usage

### Basic Example

Here is a minimal example of how to use the library to mux video and audio:

```cpp
#include "mpeg2/ts_muxer.h"
#include <fstream>
#include <vector>

using namespace mpeg2;

int main() {
    TSMuxer muxer;
    std::ofstream out("output.ts", std::ios::binary);

    // Add streams
    // Returns PID for the stream
    uint16_t video_pid = muxer.AddStream(TS_STREAM_H264); 
    uint16_t audio_pid = muxer.AddStream(TS_STREAM_AAC);

    // Set output callback
    muxer.SetOnPacket([&](const std::vector<uint8_t>& packet) {
        out.write((const char*)packet.data(), packet.size());
    });

    // Write data (example loop)
    // pts and dts are in 90kHz clock units
    uint64_t pts = 0;
    uint64_t dts = 0;
    
    // ... loop over your frames ...
    // muxer.Write(video_pid, frame_data, frame_size, pts, dts);
    
    return 0;
}
```

### Running the Example

The provided example `ts_test` can be used to mux raw H.264/H.265 files.

```bash
# Usage: ./ts_test <input_video> <output_ts> [interval_ms] [input_audio]

# Mux H.264 video
./ts_test input.h264 output.ts

# Mux H.265 video (file extension must be .h265)
./ts_test input.h265 output.ts

# Mux with AAC audio
./ts_test input.h264 output.ts 40 input.aac
```

## Project Structure

- `include/mpeg2/`: Header files
- `src/`: Source code
- `examples/`: Example usage code

## License

MIT License
