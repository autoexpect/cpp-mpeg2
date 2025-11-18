#include "pes_proto.h"
#include <cstring>
#include <stdexcept>

namespace mpeg2 {

const uint8_t H264_AUD_NALU[] = {0x00, 0x00, 0x00, 0x01, 0x09, 0xF0};
const size_t H264_AUD_NALU_SIZE = 6;
const uint8_t H265_AUD_NALU[] = {0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x50};
const size_t H265_AUD_NALU_SIZE = 7;

int PesPacket::Decode(BitStream& bs) {
    if (bs.RemainBytes() < 9) {
        return static_cast<int>(Mpeg2Error::NEED_MORE);
    }
    
    try {
        bs.SkipBits(24); // packet_start_code_prefix
        stream_id = static_cast<uint8_t>(bs.Uint8(8));
        pes_packet_length = static_cast<uint16_t>(bs.Uint16(16));
        bs.SkipBits(2); // '10'
        pes_scrambling_control = static_cast<uint8_t>(bs.Uint8(2));
        pes_priority = static_cast<uint8_t>(bs.Uint8(1));
        data_alignment_indicator = static_cast<uint8_t>(bs.Uint8(1));
        copyright = static_cast<uint8_t>(bs.Uint8(1));
        original_or_copy = static_cast<uint8_t>(bs.Uint8(1));
        pts_dts_flags = static_cast<uint8_t>(bs.Uint8(2));
        escr_flag = static_cast<uint8_t>(bs.Uint8(1));
        es_rate_flag = static_cast<uint8_t>(bs.Uint8(1));
        dsm_trick_mode_flag = static_cast<uint8_t>(bs.Uint8(1));
        additional_copy_info_flag = static_cast<uint8_t>(bs.Uint8(1));
        pes_crc_flag = static_cast<uint8_t>(bs.Uint8(1));
        pes_extension_flag = static_cast<uint8_t>(bs.Uint8(1));
        pes_header_data_length = static_cast<uint8_t>(bs.Uint8(8));
        
        if (bs.RemainBytes() < pes_header_data_length) {
            // UnRead not implemented in simple version, would need to track position
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        size_t start_pos = bs.RemainBits();
        
        if ((pts_dts_flags & 0x02) == 0x02) {
            bs.SkipBits(4);
            pts = bs.Uint32(3);
            bs.SkipBits(1);
            pts = (pts << 15) | bs.Uint32(15);
            bs.SkipBits(1);
            pts = (pts << 15) | bs.Uint32(15);
            bs.SkipBits(1);
        }
        
        if ((pts_dts_flags & 0x03) == 0x03) {
            bs.SkipBits(4);
            dts = bs.Uint32(3);
            bs.SkipBits(1);
            dts = (dts << 15) | bs.Uint32(15);
            bs.SkipBits(1);
            dts = (dts << 15) | bs.Uint32(15);
            bs.SkipBits(1);
        } else {
            dts = pts;
        }
        
        if (escr_flag == 1) {
            bs.SkipBits(2);
            escr_base = bs.Uint32(3);
            bs.SkipBits(1);
            escr_base = (escr_base << 15) | bs.Uint32(15);
            bs.SkipBits(1);
            escr_base = (escr_base << 15) | bs.Uint32(15);
            bs.SkipBits(1);
            escr_extension = static_cast<uint16_t>(bs.Uint16(9));
            bs.SkipBits(1);
        }
        
        if (es_rate_flag == 1) {
            bs.SkipBits(1);
            es_rate = bs.Uint32(22);
            bs.SkipBits(1);
        }
        
        if (dsm_trick_mode_flag == 1) {
            trick_mode_control = static_cast<uint8_t>(bs.Uint8(3));
            trick_value = static_cast<uint8_t>(bs.Uint8(5));
        }
        
        if (additional_copy_info_flag == 1) {
            additional_copy_info = static_cast<uint8_t>(bs.Uint8(7));
        }
        
        if (pes_crc_flag == 1) {
            previous_pes_packet_crc = static_cast<uint16_t>(bs.Uint16(16));
        }
        
        size_t bits_read = start_pos - bs.RemainBits();
        size_t remaining_header_bits = pes_header_data_length * 8 - bits_read;
        if (remaining_header_bits > 0) {
            bs.SkipBits(remaining_header_bits);
        }
        
        int data_len = pes_packet_length - 3 - pes_header_data_length;
        
        if (bs.RemainBytes() < data_len) {
            pes_payload.clear();
            const uint8_t* remain = bs.RemainData();
            if (remain) {
                pes_payload.assign(remain, remain + bs.RemainBytes());
            }
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        const uint8_t* payload_data = bs.RemainData();
        if (pes_packet_length == 0 || bs.RemainBytes() <= data_len) {
            pes_payload.assign(payload_data, payload_data + bs.RemainBytes());
            bs.SkipBits(bs.RemainBits());
        } else {
            pes_payload.assign(payload_data, payload_data + data_len);
            bs.SkipBits(data_len * 8);
        }
        
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

int PesPacket::DecodeMpeg1(BitStream& bs) {
    if (bs.RemainBytes() < 6) {
        return static_cast<int>(Mpeg2Error::NEED_MORE);
    }
    
    try {
        bs.SkipBits(24); // packet_start_code_prefix
        stream_id = static_cast<uint8_t>(bs.Uint8(8));
        pes_packet_length = static_cast<uint16_t>(bs.Uint16(16));
        
        if (pes_packet_length != 0 && bs.RemainBytes() < pes_packet_length) {
            return static_cast<int>(Mpeg2Error::NEED_MORE);
        }
        
        size_t start_bits = bs.RemainBits();
        
        while (bs.NextBits(8) == 0xFF) {
            bs.SkipBits(8);
        }
        
        if (bs.NextBits(2) == 0x01) {
            bs.SkipBits(16);
        }
        
        if (bs.NextBits(4) == 0x02) {
            bs.SkipBits(4);
            pts = bs.Uint32(3);
            bs.SkipBits(1);
            pts = (pts << 15) | bs.Uint32(15);
            bs.SkipBits(1);
            pts = (pts << 15) | bs.Uint32(15);
            bs.SkipBits(1);
            dts = pts;
        } else if (bs.NextBits(4) == 0x03) {
            bs.SkipBits(4);
            pts = bs.Uint32(3);
            bs.SkipBits(1);
            pts = (pts << 15) | bs.Uint32(15);
            bs.SkipBits(1);
            pts = (pts << 15) | bs.Uint32(15);
            bs.SkipBits(1);
            bs.SkipBits(4);
            dts = bs.Uint32(3);
            bs.SkipBits(1);
            dts = (dts << 15) | bs.Uint32(15);
            bs.SkipBits(1);
            dts = (dts << 15) | bs.Uint32(15);
            bs.SkipBits(1);
        } else if (bs.NextBits(8) == 0x0F) {
            bs.SkipBits(8);
        } else {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        size_t bits_consumed = start_bits - bs.RemainBits();
        size_t loc = bits_consumed / 8;
        
        if (pes_packet_length < loc) {
            return static_cast<int>(Mpeg2Error::PARSE_ERROR);
        }
        
        const uint8_t* payload_data = bs.RemainData();
        size_t payload_len = pes_packet_length - loc;
        
        if (pes_packet_length == 0 || bs.RemainBits() <= payload_len * 8) {
            pes_payload.assign(payload_data, payload_data + bs.RemainBytes());
            bs.SkipBits(bs.RemainBits());
        } else {
            pes_payload.assign(payload_data, payload_data + payload_len);
            bs.SkipBits(payload_len * 8);
        }
        
        return static_cast<int>(Mpeg2Error::SUCCESS);
    } catch (const std::exception&) {
        return static_cast<int>(Mpeg2Error::PARSE_ERROR);
    }
}

void PesPacket::Encode(BitStreamWriter& bsw) const {
    uint8_t start_code[] = {0x00, 0x00, 0x01};
    bsw.PutBytes(start_code, 3);
    bsw.PutByte(stream_id);
    bsw.PutUint16(pes_packet_length, 16);
    bsw.PutUint8(0x02, 2);
    bsw.PutUint8(pes_scrambling_control, 2);
    bsw.PutUint8(pes_priority, 1);
    bsw.PutUint8(data_alignment_indicator, 1);
    bsw.PutUint8(copyright, 1);
    bsw.PutUint8(original_or_copy, 1);
    bsw.PutUint8(pts_dts_flags, 2);
    bsw.PutUint8(escr_flag, 1);
    bsw.PutUint8(es_rate_flag, 1);
    bsw.PutUint8(dsm_trick_mode_flag, 1);
    bsw.PutUint8(additional_copy_info_flag, 1);
    bsw.PutUint8(pes_crc_flag, 1);
    bsw.PutUint8(pes_extension_flag, 1);
    bsw.PutByte(pes_header_data_length);
    
    if (pts_dts_flags == 0x02) {
        bsw.PutUint8(0x02, 4);
        bsw.PutUint64(pts >> 30, 3);
        bsw.PutUint8(0x01, 1);
        bsw.PutUint64(pts >> 15, 15);
        bsw.PutUint8(0x01, 1);
        bsw.PutUint64(pts, 15);
        bsw.PutUint8(0x01, 1);
    }
    
    if (pts_dts_flags == 0x03) {
        bsw.PutUint8(0x03, 4);
        bsw.PutUint64(pts >> 30, 3);
        bsw.PutUint8(0x01, 1);
        bsw.PutUint64(pts >> 15, 15);
        bsw.PutUint8(0x01, 1);
        bsw.PutUint64(pts, 15);
        bsw.PutUint8(0x01, 1);
        bsw.PutUint8(0x01, 4);
        bsw.PutUint64(dts >> 30, 3);
        bsw.PutUint8(0x01, 1);
        bsw.PutUint64(dts >> 15, 15);
        bsw.PutUint8(0x01, 1);
        bsw.PutUint64(dts, 15);
        bsw.PutUint8(0x01, 1);
    }
    
    if (escr_flag == 1) {
        bsw.PutUint8(0x03, 2);
        bsw.PutUint64(escr_base >> 30, 3);
        bsw.PutUint8(0x01, 1);
        bsw.PutUint64(escr_base >> 15, 15);
        bsw.PutUint8(0x01, 1);
        bsw.PutUint64(escr_base, 15);
        bsw.PutUint8(0x01, 1);
    }
    
    bsw.PutBytes(pes_payload.data(), pes_payload.size());
}

} // namespace mpeg2
