#ifndef MPEG2_PS_PROTO_H
#define MPEG2_PS_PROTO_H

#include <cstdint>
#include <vector>
#include <memory>
#include "bitstream.h"
#include "pes_proto.h"

namespace mpeg2 {

enum class PSStreamType : uint8_t {
    UNKNOWN = 0xFF,
    AAC = 0x0F,
    H264 = 0x1B,
    H265 = 0x24,
    G711A = 0x90,
    G711U = 0x91
};

struct PSPackHeader {
    bool is_mpeg1 = false;
    uint64_t system_clock_reference_base = 0;
    uint16_t system_clock_reference_extension = 0;
    uint32_t program_mux_rate = 0;
    uint8_t pack_stuffing_length = 0;

    int Decode(BitStream& bs);
    void Encode(BitStreamWriter& bsw) const;

private:
    int DecodeMpeg2(BitStream& bs);
    int DecodeMpeg1(BitStream& bs);
};

struct ElementaryStream {
    uint8_t stream_id = 0;
    uint8_t p_std_buffer_bound_scale = 0;
    uint16_t p_std_buffer_size_bound = 0;

    ElementaryStream() = default;
    ElementaryStream(uint8_t sid) : stream_id(sid) {}
};

struct SystemHeader {
    uint16_t header_length = 0;
    uint32_t rate_bound = 0;
    uint8_t audio_bound = 0;
    uint8_t fixed_flag = 0;
    uint8_t csps_flag = 0;
    uint8_t system_audio_lock_flag = 0;
    uint8_t system_video_lock_flag = 0;
    uint8_t video_bound = 0;
    uint8_t packet_rate_restriction_flag = 0;
    std::vector<ElementaryStream> streams;

    int Decode(BitStream& bs);
    void Encode(BitStreamWriter& bsw) const;
};

struct ElementaryStreamElem {
    uint8_t stream_type = 0;
    uint8_t elementary_stream_id = 0;
    uint16_t elementary_stream_info_length = 0;

    ElementaryStreamElem() = default;
    ElementaryStreamElem(uint8_t stype, uint8_t esid)
        : stream_type(stype), elementary_stream_id(esid) {}
};

struct ProgramStreamMap {
    uint8_t map_stream_id = 0;
    uint16_t program_stream_map_length = 0;
    uint8_t current_next_indicator = 0;
    uint8_t program_stream_map_version = 0;
    uint16_t program_stream_info_length = 0;
    uint16_t elementary_stream_map_length = 0;
    std::vector<ElementaryStreamElem> stream_map;

    int Decode(BitStream& bs);
    void Encode(BitStreamWriter& bsw) const;
};

struct ProgramStreamDirectory {
    uint16_t pes_packet_length = 0;

    int Decode(BitStream& bs);
};

struct CommonPesPacket {
    uint8_t stream_id = 0;
    uint16_t pes_packet_length = 0;

    int Decode(BitStream& bs);
};

struct PSPacket {
    std::unique_ptr<PSPackHeader> header;
    std::unique_ptr<SystemHeader> system;
    std::unique_ptr<ProgramStreamMap> psm;
    std::unique_ptr<ProgramStreamDirectory> psd;
    std::unique_ptr<CommonPesPacket> comm_pes;
    std::unique_ptr<PesPacket> pes;
};

// CRC32 utility function
uint32_t CalcCrc32(uint32_t crc, const uint8_t* data, size_t len);

} // namespace mpeg2

#endif // MPEG2_PS_PROTO_H
