#ifndef MPEG2_PES_PROTO_H
#define MPEG2_PES_PROTO_H

#include <cstdint>
#include <vector>
#include <memory>
#include "bitstream.h"

namespace mpeg2 {

// H.264 and H.265 AUD NALU
extern const uint8_t H264_AUD_NALU[];
extern const size_t H264_AUD_NALU_SIZE;
extern const uint8_t H265_AUD_NALU[];
extern const size_t H265_AUD_NALU_SIZE;

enum class PESStreamID : uint8_t {
    STREAM_END = 0xB9,
    STREAM_START = 0xBA,
    STREAM_SYSTEM_HEAD = 0xBB,
    STREAM_MAP = 0xBC,
    STREAM_PRIVATE = 0xBD,
    STREAM_AUDIO = 0xC0,
    STREAM_VIDEO = 0xE0
};

struct PesPacket {
    uint8_t stream_id = 0;
    uint16_t pes_packet_length = 0;
    uint8_t pes_scrambling_control = 0;
    uint8_t pes_priority = 0;
    uint8_t data_alignment_indicator = 0;
    uint8_t copyright = 0;
    uint8_t original_or_copy = 0;
    uint8_t pts_dts_flags = 0;
    uint8_t escr_flag = 0;
    uint8_t es_rate_flag = 0;
    uint8_t dsm_trick_mode_flag = 0;
    uint8_t additional_copy_info_flag = 0;
    uint8_t pes_crc_flag = 0;
    uint8_t pes_extension_flag = 0;
    uint8_t pes_header_data_length = 0;
    uint64_t pts = 0;
    uint64_t dts = 0;
    uint64_t escr_base = 0;
    uint16_t escr_extension = 0;
    uint32_t es_rate = 0;
    uint8_t trick_mode_control = 0;
    uint8_t trick_value = 0;
    uint8_t additional_copy_info = 0;
    uint16_t previous_pes_packet_crc = 0;
    std::vector<uint8_t> pes_payload;

    int Decode(BitStream& bs);
    int DecodeMpeg1(BitStream& bs);
    void Encode(BitStreamWriter& bsw) const;
};

enum class Mpeg2Error {
    SUCCESS = 0,
    NEED_MORE = 1,
    PARSE_ERROR = 2,
    NOT_FOUND = 3
};

} // namespace mpeg2

#endif // MPEG2_PES_PROTO_H
