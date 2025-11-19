#ifndef MPEG2_PS_MUXER_H
#define MPEG2_PS_MUXER_H

#include "mpeg2/mpeg2_def.h"
#include "mpeg2/bitstream.h"
#include <vector>
#include <functional>
#include <map>

namespace mpeg2 {

struct ElementaryStream {
    uint8_t stream_id;
    uint8_t p_std_buffer_bound_scale;
    uint16_t p_std_buffer_size_bound;
};

struct SystemHeader {
    uint32_t rate_bound;
    uint8_t audio_bound;
    uint8_t video_bound;
    std::vector<ElementaryStream> streams;
};

struct ElementaryStreamElem {
    uint8_t stream_type;
    uint8_t elementary_stream_id;
};

struct ProgramStreamMap {
    uint8_t current_next_indicator;
    uint8_t program_stream_map_version;
    std::vector<ElementaryStreamElem> stream_map;
};

class PSMuxer {
public:
    PSMuxer();
    ~PSMuxer();

    uint8_t AddStream(PS_STREAM_TYPE type);
    void Write(uint8_t stream_id, const uint8_t* data, size_t size, uint64_t pts, uint64_t dts);
    
    void SetOnPacket(std::function<void(const std::vector<uint8_t>&)> callback);

private:
    void WritePackHeader(uint64_t scr, uint32_t mux_rate);
    void WriteSystemHeader();
    void WriteProgramStreamMap();
    void WritePesPacket(uint8_t stream_id, const uint8_t* data, size_t size, uint64_t pts, uint64_t dts, bool idr_flag, bool with_aud, uint8_t stream_type);

    SystemHeader system_header_;
    ProgramStreamMap psm_;
    bool first_frame_;
    
    std::function<void(const std::vector<uint8_t>&)> on_packet_;
};

} // namespace mpeg2

#endif // MPEG2_PS_MUXER_H
