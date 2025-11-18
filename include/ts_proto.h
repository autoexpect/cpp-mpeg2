#ifndef MPEG2_TS_PROTO_H
#define MPEG2_TS_PROTO_H

#include <cstdint>
#include <vector>
#include <memory>
#include "bitstream.h"

namespace mpeg2 {

const size_t TS_PACKET_SIZE = 188;

enum class TSStreamType : uint8_t {
    AUDIO_MPEG1 = 0x03,
    AUDIO_MPEG2 = 0x04,
    AAC = 0x0F,
    H264 = 0x1B,
    H265 = 0x24
};

enum class TSPID : uint16_t {
    PAT = 0x0000,
    CAT = 0x0001,
    TSDT = 0x0002,
    IPMP = 0x0003,
    NIL = 0x1FFF
};

enum class TableID : uint8_t {
    PAS = 0x00,
    CAS = 0x01,
    PMS = 0x02,
    SDS = 0x03,
    FORBIDDEN = 0xFF
};

struct AdaptationField {
    uint8_t adaptation_field_length = 0;
    uint8_t discontinuity_indicator = 0;
    uint8_t random_access_indicator = 0;
    uint8_t elementary_stream_priority_indicator = 0;
    uint8_t pcr_flag = 0;
    uint8_t opcr_flag = 0;
    uint8_t splicing_point_flag = 0;
    uint8_t transport_private_data_flag = 0;
    uint8_t adaptation_field_extension_flag = 0;
    uint64_t program_clock_reference_base = 0;
    uint16_t program_clock_reference_extension = 0;
    uint64_t original_program_clock_reference_base = 0;
    uint16_t original_program_clock_reference_extension = 0;

    int Decode(BitStream& bs);
    void Encode(BitStreamWriter& bsw) const;
};

struct TSPacket {
    uint8_t transport_error_indicator = 0;
    uint8_t payload_unit_start_indicator = 0;
    uint8_t transport_priority = 0;
    uint16_t pid = 0;
    uint8_t transport_scrambling_control = 0;
    uint8_t adaptation_field_control = 0;
    uint8_t continuity_counter = 0;
    std::unique_ptr<AdaptationField> field;
    std::vector<uint8_t> payload;

    int DecodeHeader(BitStream& bs);
    void EncodeHeader(BitStreamWriter& bsw) const;
};

struct PmtPair {
    uint16_t program_number = 0;
    uint16_t pid = 0;
};

struct PAT {
    uint8_t table_id = 0;
    uint8_t section_syntax_indicator = 0;
    uint16_t section_length = 0;
    uint16_t transport_stream_id = 0;
    uint8_t version_number = 0;
    uint8_t current_next_indicator = 0;
    uint8_t section_number = 0;
    uint8_t last_section_number = 0;
    std::vector<PmtPair> pmts;

    int Decode(BitStream& bs);
    void Encode(BitStreamWriter& bsw) const;
};

struct PmtStream {
    uint8_t stream_type = 0;
    uint16_t elementary_pid = 0;
    uint16_t es_info_length = 0;
};

struct PMT {
    uint8_t table_id = 0;
    uint8_t section_syntax_indicator = 0;
    uint16_t section_length = 0;
    uint16_t program_number = 0;
    uint8_t version_number = 0;
    uint8_t current_next_indicator = 0;
    uint8_t section_number = 0;
    uint8_t last_section_number = 0;
    uint16_t pcr_pid = 0;
    uint16_t program_info_length = 0;
    std::vector<PmtStream> streams;

    int Decode(BitStream& bs);
    void Encode(BitStreamWriter& bsw) const;
};

} // namespace mpeg2

#endif // MPEG2_TS_PROTO_H
