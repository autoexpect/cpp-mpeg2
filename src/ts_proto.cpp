#include "ts_proto.h"
#include "ps_proto.h"
#include <cstring>

namespace mpeg2 {

int AdaptationField::Decode(BitStream& bs) {
    if (bs.RemainBytes() < 1) {
        return static_cast<int>(Mpeg2Error::NEED_MORE);
    }
    
    try {
        adaptation_field_length = static_cast<uint8_t>(bs.Uint8(8));
        size_t start_offset = bs.RemainBits();
        
        if (bs.RemainBytes() < adaptation_field_length) {
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        if (adaptation_field_length == 0) {
            return static_cast<int>(Mpeg2Error::SUCCESS);
        }
        
        discontinuity_indicator = static_cast<uint8_t>(bs.Uint8(1));
        random_access_indicator = static_cast<uint8_t>(bs.Uint8(1));
        elementary_stream_priority_indicator = static_cast<uint8_t>(bs.Uint8(1));
        pcr_flag = static_cast<uint8_t>(bs.Uint8(1));
        opcr_flag = static_cast<uint8_t>(bs.Uint8(1));
        splicing_point_flag = static_cast<uint8_t>(bs.Uint8(1));
        transport_private_data_flag = static_cast<uint8_t>(bs.Uint8(1));
        adaptation_field_extension_flag = static_cast<uint8_t>(bs.Uint8(1));
        
        if (pcr_flag == 1) {
            program_clock_reference_base = bs.Uint64(33);
            bs.SkipBits(6);
            program_clock_reference_extension = static_cast<uint16_t>(bs.Uint16(9));
        }
        
        if (opcr_flag == 1) {
            original_program_clock_reference_base = bs.Uint64(33);
            bs.SkipBits(6);
            original_program_clock_reference_extension = static_cast<uint16_t>(bs.Uint16(9));
        }
        
        if (splicing_point_flag == 1) {
            bs.SkipBits(8); // splice_countdown
        }
        
        if (transport_private_data_flag == 1) {
            uint8_t private_data_len = static_cast<uint8_t>(bs.Uint8(8));
            bs.SkipBits(private_data_len * 8);
        }
        
        if (adaptation_field_extension_flag == 1) {
            uint8_t ext_len = static_cast<uint8_t>(bs.Uint8(8));
            bs.SkipBits(ext_len * 8);
        }
        
        // Skip remaining stuffing bytes
        size_t bits_consumed = start_offset - bs.RemainBits();
        size_t remaining_bits = adaptation_field_length * 8 - bits_consumed;
        if (remaining_bits > 0) {
            bs.SkipBits(remaining_bits);
        }
        
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

void AdaptationField::Encode(BitStreamWriter& bsw) const {
    size_t loc = bsw.Bits().size();
    bsw.PutByte(adaptation_field_length);
    
    if (adaptation_field_length == 0) {
        return;
    }
    
    bsw.PutUint8(discontinuity_indicator, 1);
    bsw.PutUint8(random_access_indicator, 1);
    bsw.PutUint8(elementary_stream_priority_indicator, 1);
    bsw.PutUint8(pcr_flag, 1);
    bsw.PutUint8(opcr_flag, 1);
    bsw.PutUint8(splicing_point_flag, 1);
    bsw.PutUint8(transport_private_data_flag, 1);
    bsw.PutUint8(adaptation_field_extension_flag, 1);
    
    if (pcr_flag == 1) {
        bsw.PutUint64(program_clock_reference_base, 33);
        bsw.PutUint8(0, 6);
        bsw.PutUint16(program_clock_reference_extension, 9);
    }
    
    if (opcr_flag == 1) {
        bsw.PutUint64(original_program_clock_reference_base, 33);
        bsw.PutUint8(0, 6);
        bsw.PutUint16(original_program_clock_reference_extension, 9);
    }
    
    if (splicing_point_flag == 1) {
        bsw.PutByte(0); // splice_countdown
    }
}

int TSPacket::DecodeHeader(BitStream& bs) {
    try {
        if (bs.Uint8(8) != 0x47) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        transport_error_indicator = static_cast<uint8_t>(bs.Uint8(1));
        payload_unit_start_indicator = static_cast<uint8_t>(bs.Uint8(1));
        transport_priority = static_cast<uint8_t>(bs.Uint8(1));
        pid = static_cast<uint16_t>(bs.Uint16(13));
        transport_scrambling_control = static_cast<uint8_t>(bs.Uint8(2));
        adaptation_field_control = static_cast<uint8_t>(bs.Uint8(2));
        continuity_counter = static_cast<uint8_t>(bs.Uint8(4));
        
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

void TSPacket::EncodeHeader(BitStreamWriter& bsw) const {
    bsw.PutByte(0x47);
    bsw.PutUint8(transport_error_indicator, 1);
    bsw.PutUint8(payload_unit_start_indicator, 1);
    bsw.PutUint8(transport_priority, 1);
    bsw.PutUint16(pid, 13);
    bsw.PutUint8(transport_scrambling_control, 2);
    bsw.PutUint8(adaptation_field_control, 2);
    bsw.PutUint8(continuity_counter, 4);
}

int PAT::Decode(BitStream& bs) {
    try {
        table_id = static_cast<uint8_t>(bs.Uint8(8));
        if (table_id != static_cast<uint8_t>(TableID::PAS)) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        section_syntax_indicator = static_cast<uint8_t>(bs.Uint8(1));
        bs.SkipBits(3);
        section_length = static_cast<uint16_t>(bs.Uint16(12));
        transport_stream_id = static_cast<uint16_t>(bs.Uint16(16));
        bs.SkipBits(2);
        version_number = static_cast<uint8_t>(bs.Uint8(5));
        current_next_indicator = static_cast<uint8_t>(bs.Uint8(1));
        section_number = static_cast<uint8_t>(bs.Uint8(8));
        last_section_number = static_cast<uint8_t>(bs.Uint8(8));
        
        pmts.clear();
        for (int i = 0; i + 4 <= section_length - 5 - 4; i += 4) {
            PmtPair pmt;
            pmt.program_number = static_cast<uint16_t>(bs.Uint16(16));
            bs.SkipBits(3);
            pmt.pid = static_cast<uint16_t>(bs.Uint16(13));
            pmts.push_back(pmt);
        }
        
        bs.SkipBits(32); // CRC32
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

void PAT::Encode(BitStreamWriter& bsw) const {
    bsw.PutByte(0x00);
    size_t loc = bsw.Bits().size();
    bsw.PutUint8(section_syntax_indicator, 1);
    bsw.PutUint8(0x00, 1);
    bsw.PutUint8(0x03, 2);
    bsw.PutUint16(0, 12); // placeholder
    
    size_t mark_pos = bsw.Bits().size() * 8;
    
    bsw.PutUint16(transport_stream_id, 16);
    bsw.PutUint8(0x03, 2);
    bsw.PutUint8(version_number, 5);
    bsw.PutUint8(current_next_indicator, 1);
    bsw.PutUint8(section_number, 8);
    bsw.PutUint8(last_section_number, 8);
    
    for (const auto& pmt : pmts) {
        bsw.PutUint16(pmt.program_number, 16);
        bsw.PutUint8(0x07, 3);
        bsw.PutUint16(pmt.pid, 13);
    }
    
    uint16_t length = (bsw.Bits().size() * 8 - mark_pos) / 8 + 4;
    auto& bits = const_cast<std::vector<uint8_t>&>(bsw.Bits());
    uint16_t section_val = (length & 0x0FFF) | (section_syntax_indicator << 15) | 0x3000;
    bits[loc] = (section_val >> 8) & 0xFF;
    bits[loc + 1] = section_val & 0xFF;
    
    // CRC32
    size_t crc_start = loc - 1;
    uint32_t crc = CalcCrc32(0xFFFFFFFF, bits.data() + crc_start, bits.size() - crc_start);
    bsw.PutByte(crc & 0xFF);
    bsw.PutByte((crc >> 8) & 0xFF);
    bsw.PutByte((crc >> 16) & 0xFF);
    bsw.PutByte((crc >> 24) & 0xFF);
}

int PMT::Decode(BitStream& bs) {
    try {
        table_id = static_cast<uint8_t>(bs.Uint8(8));
        if (table_id != static_cast<uint8_t>(TableID::PMS)) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        section_syntax_indicator = static_cast<uint8_t>(bs.Uint8(1));
        bs.SkipBits(3);
        section_length = static_cast<uint16_t>(bs.Uint16(12));
        program_number = static_cast<uint16_t>(bs.Uint16(16));
        bs.SkipBits(2);
        version_number = static_cast<uint8_t>(bs.Uint8(5));
        current_next_indicator = static_cast<uint8_t>(bs.Uint8(1));
        section_number = static_cast<uint8_t>(bs.Uint8(8));
        last_section_number = static_cast<uint8_t>(bs.Uint8(8));
        bs.SkipBits(3);
        pcr_pid = static_cast<uint16_t>(bs.Uint16(13));
        bs.SkipBits(4);
        program_info_length = static_cast<uint16_t>(bs.Uint16(12));
        
        bs.SkipBits(program_info_length * 8);
        
        streams.clear();
        for (int i = 0; i < section_length - 9 - program_info_length - 4; ) {
            PmtStream stream;
            stream.stream_type = static_cast<uint8_t>(bs.Uint8(8));
            bs.SkipBits(3);
            stream.elementary_pid = static_cast<uint16_t>(bs.Uint16(13));
            bs.SkipBits(4);
            stream.es_info_length = static_cast<uint16_t>(bs.Uint16(12));
            
            bs.SkipBits(stream.es_info_length * 8);
            streams.push_back(stream);
            i += 5 + stream.es_info_length;
        }
        
        bs.SkipBits(32); // CRC32
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

void PMT::Encode(BitStreamWriter& bsw) const {
    bsw.PutByte(table_id);
    size_t loc = bsw.Bits().size();
    bsw.PutUint8(section_syntax_indicator, 1);
    bsw.PutUint8(0x00, 1);
    bsw.PutUint8(0x03, 2);
    bsw.PutUint16(section_length, 12);
    
    size_t mark_pos = bsw.Bits().size() * 8;
    
    bsw.PutUint16(program_number, 16);
    bsw.PutUint8(0x03, 2);
    bsw.PutUint8(version_number, 5);
    bsw.PutUint8(current_next_indicator, 1);
    bsw.PutUint8(section_number, 8);
    bsw.PutUint8(last_section_number, 8);
    bsw.PutUint8(0x07, 3);
    bsw.PutUint16(pcr_pid, 13);
    bsw.PutUint8(0x0F, 4);
    bsw.PutUint16(0x0000, 12); // program_info_length
    
    for (const auto& stream : streams) {
        bsw.PutUint8(stream.stream_type, 8);
        bsw.PutUint8(0x00, 3);
        bsw.PutUint16(stream.elementary_pid, 13);
        bsw.PutUint8(0x00, 4);
        bsw.PutUint16(0, 12); // es_info_length
    }
    
    uint16_t length = (bsw.Bits().size() * 8 - mark_pos) / 8 + 4;
    auto& bits = const_cast<std::vector<uint8_t>&>(bsw.Bits());
    uint16_t section_val = (length & 0x0FFF) | (section_syntax_indicator << 15) | 0x3000;
    bits[loc] = (section_val >> 8) & 0xFF;
    bits[loc + 1] = section_val & 0xFF;
    
    // CRC32
    size_t crc_start = loc - 1;
    uint32_t crc = CalcCrc32(0xFFFFFFFF, bits.data() + crc_start, bits.size() - crc_start);
    bsw.PutByte(crc & 0xFF);
    bsw.PutByte((crc >> 8) & 0xFF);
    bsw.PutByte((crc >> 16) & 0xFF);
    bsw.PutByte((crc >> 24) & 0xFF);
}

} // namespace mpeg2
