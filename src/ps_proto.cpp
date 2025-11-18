#include "ps_proto.h"
#include <cstring>
#include <stdexcept>

namespace mpeg2 {

// CRC32 table for polynomial 0x04C11DB7
static const uint32_t crc32_table[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
    0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
    0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
    0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
    0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
    0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
    0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
    0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
    0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
    0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
    0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
    0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
    0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
    0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
    0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
    0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
    0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
    0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
    0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
    0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
    0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
    0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

uint32_t CalcCrc32(uint32_t crc, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ data[i]) & 0xFF];
    }
    return crc;
}

int PSPackHeader::Decode(BitStream& bs) {
    if (bs.RemainBytes() < 5) {
        return static_cast<int>(Mpeg2Error::NEED_MORE);
    }
    
    try {
        if (bs.Uint32(32) != 0x000001BA) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        if (bs.NextBits(2) == 0x01) { // MPEG-2
            if (bs.RemainBytes() < 10) {
                return static_cast<int>(Mpeg2Error::NEED_MORE);
            }
            return DecodeMpeg2(bs);
        } else if (bs.NextBits(4) == 0x02) { // MPEG-1
            if (bs.RemainBytes() < 8) {
                return static_cast<int>(Mpeg2Error::NEED_MORE);
            }
            is_mpeg1 = true;
            return DecodeMpeg1(bs);
        } else {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

int PSPackHeader::DecodeMpeg2(BitStream& bs) {
    try {
        bs.SkipBits(2);
        system_clock_reference_base = bs.Uint32(3);
        bs.SkipBits(1);
        system_clock_reference_base = (system_clock_reference_base << 15) | bs.Uint32(15);
        bs.SkipBits(1);
        system_clock_reference_base = (system_clock_reference_base << 15) | bs.Uint32(15);
        bs.SkipBits(1);
        system_clock_reference_extension = static_cast<uint16_t>(bs.Uint16(9));
        bs.SkipBits(1);
        program_mux_rate = bs.Uint32(22);
        bs.SkipBits(1);
        bs.SkipBits(1);
        bs.SkipBits(5);
        pack_stuffing_length = static_cast<uint8_t>(bs.Uint8(3));
        
        if (bs.RemainBytes() < pack_stuffing_length) {
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        bs.SkipBits(pack_stuffing_length * 8);
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

int PSPackHeader::DecodeMpeg1(BitStream& bs) {
    try {
        bs.SkipBits(4);
        system_clock_reference_base = bs.Uint32(3);
        bs.SkipBits(1);
        system_clock_reference_base = (system_clock_reference_base << 15) | bs.Uint32(15);
        bs.SkipBits(1);
        system_clock_reference_base = (system_clock_reference_base << 15) | bs.Uint32(15);
        bs.SkipBits(1);
        system_clock_reference_extension = 1;
        program_mux_rate = bs.Uint32(7);
        bs.SkipBits(1);
        program_mux_rate = (program_mux_rate << 15) | bs.Uint32(15);
        bs.SkipBits(1);
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

void PSPackHeader::Encode(BitStreamWriter& bsw) const {
    uint8_t start_code[] = {0x00, 0x00, 0x01, 0xBA};
    bsw.PutBytes(start_code, 4);
    bsw.PutUint8(1, 2);
    bsw.PutUint64(system_clock_reference_base >> 30, 3);
    bsw.PutUint8(1, 1);
    bsw.PutUint64(system_clock_reference_base >> 15, 15);
    bsw.PutUint8(1, 1);
    bsw.PutUint64(system_clock_reference_base, 15);
    bsw.PutUint8(1, 1);
    bsw.PutUint16(system_clock_reference_extension, 9);
    bsw.PutUint8(1, 1);
    bsw.PutUint32(program_mux_rate, 22);
    bsw.PutUint8(1, 1);
    bsw.PutUint8(1, 1);
    bsw.PutUint8(0x1F, 5);
    bsw.PutUint8(pack_stuffing_length, 3);
    for (int i = 0; i < pack_stuffing_length; i++) {
        bsw.PutByte(0xFF);
    }
}

int SystemHeader::Decode(BitStream& bs) {
    if (bs.RemainBytes() < 12) {
        return static_cast<int>(Mpeg2Error::NEED_MORE);
    }
    
    try {
        if (bs.Uint32(32) != 0x000001BB) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        header_length = static_cast<uint16_t>(bs.Uint16(16));
        
        if (bs.RemainBytes() < header_length) {
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        if (header_length < 6 || (header_length - 6) % 3 != 0) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        bs.SkipBits(1);
        rate_bound = bs.Uint32(22);
        bs.SkipBits(1);
        audio_bound = static_cast<uint8_t>(bs.Uint8(6));
        fixed_flag = static_cast<uint8_t>(bs.Uint8(1));
        csps_flag = static_cast<uint8_t>(bs.Uint8(1));
        system_audio_lock_flag = static_cast<uint8_t>(bs.Uint8(1));
        system_video_lock_flag = static_cast<uint8_t>(bs.Uint8(1));
        bs.SkipBits(1);
        video_bound = static_cast<uint8_t>(bs.Uint8(5));
        packet_rate_restriction_flag = static_cast<uint8_t>(bs.Uint8(1));
        bs.SkipBits(7);
        
        streams.clear();
        uint16_t least = header_length - 6;
        
        while (least > 0 && bs.NextBits(1) == 0x01) {
            ElementaryStream es;
            es.stream_id = static_cast<uint8_t>(bs.Uint8(8));
            bs.SkipBits(2);
            es.p_std_buffer_bound_scale = static_cast<uint8_t>(bs.Uint8(1));
            es.p_std_buffer_size_bound = static_cast<uint16_t>(bs.Uint16(13));
            streams.push_back(es);
            least -= 3;
        }
        
        if (least > 0) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

void SystemHeader::Encode(BitStreamWriter& bsw) const {
    uint8_t start_code[] = {0x00, 0x00, 0x01, 0xBB};
    bsw.PutBytes(start_code, 4);
    
    size_t loc = bsw.Bits().size();
    bsw.PutUint16(0, 16); // Placeholder for header_length
    
    size_t mark_pos = bsw.Bits().size() * 8;
    
    bsw.PutUint8(1, 1);
    bsw.PutUint32(rate_bound, 22);
    bsw.PutUint8(1, 1);
    bsw.PutUint8(audio_bound, 6);
    bsw.PutUint8(fixed_flag, 1);
    bsw.PutUint8(csps_flag, 1);
    bsw.PutUint8(system_audio_lock_flag, 1);
    bsw.PutUint8(system_video_lock_flag, 1);
    bsw.PutUint8(1, 1);
    bsw.PutUint8(video_bound, 5);
    bsw.PutUint8(packet_rate_restriction_flag, 1);
    bsw.PutUint8(0x7F, 7);
    
    for (const auto& stream : streams) {
        bsw.PutUint8(stream.stream_id, 8);
        bsw.PutUint8(3, 2);
        bsw.PutUint8(stream.p_std_buffer_bound_scale, 1);
        bsw.PutUint16(stream.p_std_buffer_size_bound, 13);
    }
    
    // Calculate and set header_length
    uint16_t length = (bsw.Bits().size() * 8 - mark_pos) / 8;
    auto& bits = const_cast<std::vector<uint8_t>&>(bsw.Bits());
    bits[loc] = (length >> 8) & 0xFF;
    bits[loc + 1] = length & 0xFF;
}

int ProgramStreamMap::Decode(BitStream& bs) {
    if (bs.RemainBytes() < 16) {
        return static_cast<int>(Mpeg2Error::NEED_MORE);
    }
    
    try {
        if (bs.Uint32(24) != 0x000001) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        map_stream_id = static_cast<uint8_t>(bs.Uint8(8));
        if (map_stream_id != 0xBC) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        program_stream_map_length = static_cast<uint16_t>(bs.Uint16(16));
        
        if (bs.RemainBytes() < program_stream_map_length) {
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        current_next_indicator = static_cast<uint8_t>(bs.Uint8(1));
        bs.SkipBits(2);
        program_stream_map_version = static_cast<uint8_t>(bs.Uint8(5));
        bs.SkipBits(8);
        program_stream_info_length = static_cast<uint16_t>(bs.Uint16(16));
        
        if (bs.RemainBytes() < program_stream_info_length + 2) {
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        bs.SkipBits(program_stream_info_length * 8);
        elementary_stream_map_length = static_cast<uint16_t>(bs.Uint16(16));
        
        if (program_stream_map_length != 6 + program_stream_info_length + elementary_stream_map_length + 4) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        if (bs.RemainBytes() < elementary_stream_map_length + 4) {
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        stream_map.clear();
        int i = 0;
        
        while (i < elementary_stream_map_length) {
            ElementaryStreamElem elem;
            elem.stream_type = static_cast<uint8_t>(bs.Uint8(8));
            elem.elementary_stream_id = static_cast<uint8_t>(bs.Uint8(8));
            elem.elementary_stream_info_length = static_cast<uint16_t>(bs.Uint16(16));
            
            if (bs.RemainBytes() < elem.elementary_stream_info_length) {
                return static_cast<int>(Mpeg2Error::PARSE_ERROR);
            }
            
            bs.SkipBits(elem.elementary_stream_info_length * 8);
            i += 4 + elem.elementary_stream_info_length;
            stream_map.push_back(elem);
        }
        
        if (i != elementary_stream_map_length) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        bs.SkipBits(32); // CRC32
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

void ProgramStreamMap::Encode(BitStreamWriter& bsw) const {
    uint8_t start_code[] = {0x00, 0x00, 0x01, 0xBC};
    bsw.PutBytes(start_code, 4);
    
    size_t loc = bsw.Bits().size();
    bsw.PutUint16(program_stream_map_length, 16);
    
    size_t mark_pos = bsw.Bits().size() * 8;
    
    bsw.PutUint8(current_next_indicator, 1);
    bsw.PutUint8(3, 2);
    bsw.PutUint8(program_stream_map_version, 5);
    bsw.PutUint8(0x7F, 7);
    bsw.PutUint8(1, 1);
    bsw.PutUint16(0, 16); // program_stream_info_length
    
    uint16_t elem_map_len = stream_map.size() * 4;
    bsw.PutUint16(elem_map_len, 16);
    
    for (const auto& elem : stream_map) {
        bsw.PutUint8(elem.stream_type, 8);
        bsw.PutUint8(elem.elementary_stream_id, 8);
        bsw.PutUint16(0, 16);
    }
    
    uint16_t length = (bsw.Bits().size() * 8 - mark_pos) / 8 + 4;
    auto& bits = const_cast<std::vector<uint8_t>&>(bsw.Bits());
    bits[loc] = (length >> 8) & 0xFF;
    bits[loc + 1] = length & 0xFF;
    
    // Calculate CRC32
    size_t crc_start = loc - 4;
    uint32_t crc = CalcCrc32(0xFFFFFFFF, bits.data() + crc_start, bits.size() - crc_start);
    
    // Write CRC in little-endian
    bsw.PutByte(crc & 0xFF);
    bsw.PutByte((crc >> 8) & 0xFF);
    bsw.PutByte((crc >> 16) & 0xFF);
    bsw.PutByte((crc >> 24) & 0xFF);
}

int ProgramStreamDirectory::Decode(BitStream& bs) {
    if (bs.RemainBytes() < 6) {
        return static_cast<int>(Mpeg2Error::NEED_MORE);
    }
    
    try {
        if (bs.Uint32(32) != 0x000001FF) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        pes_packet_length = static_cast<uint16_t>(bs.Uint16(16));
        
        if (bs.RemainBytes() < pes_packet_length) {
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        bs.SkipBits(pes_packet_length * 8);
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

int CommonPesPacket::Decode(BitStream& bs) {
    if (bs.RemainBytes() < 6) {
        return static_cast<int>(Mpeg2Error::NEED_MORE);
    }
    
    try {
        bs.SkipBits(24);
        stream_id = static_cast<uint8_t>(bs.Uint8(8));
        pes_packet_length = static_cast<uint16_t>(bs.Uint16(16));
        
        if (bs.RemainBytes() < pes_packet_length) {
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        bs.SkipBits(pes_packet_length * 8);
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

} // namespace mpeg2
