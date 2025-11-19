#include "mpeg2/ps_muxer.h"
#include "mpeg2/utils.h"
#include "mpeg2/codec_utils.h"
#include <iostream>

namespace mpeg2 {

PSMuxer::PSMuxer() : first_frame_(true) {
    system_header_.rate_bound = 26234;
    system_header_.audio_bound = 0;
    system_header_.video_bound = 0;
    
    psm_.current_next_indicator = 1;
    psm_.program_stream_map_version = 1;
}

PSMuxer::~PSMuxer() {}

uint8_t PSMuxer::AddStream(PS_STREAM_TYPE type) {
    uint8_t stream_id = 0;
    if (type == PS_STREAM_H264 || type == PS_STREAM_H265) {
        stream_id = 0xE0 + system_header_.video_bound;
        ElementaryStream es;
        es.stream_id = stream_id;
        es.p_std_buffer_bound_scale = 1;
        es.p_std_buffer_size_bound = 400;
        system_header_.streams.push_back(es);
        system_header_.video_bound++;
        
        ElementaryStreamElem elem;
        elem.stream_type = (uint8_t)type;
        elem.elementary_stream_id = stream_id;
        psm_.stream_map.push_back(elem);
    } else {
        stream_id = 0xC0 + system_header_.audio_bound;
        ElementaryStream es;
        es.stream_id = stream_id;
        es.p_std_buffer_bound_scale = 0;
        es.p_std_buffer_size_bound = 32;
        system_header_.streams.push_back(es);
        system_header_.audio_bound++;
        
        ElementaryStreamElem elem;
        elem.stream_type = (uint8_t)type;
        elem.elementary_stream_id = stream_id;
        psm_.stream_map.push_back(elem);
    }
    psm_.program_stream_map_version++;
    return stream_id;
}

void PSMuxer::SetOnPacket(std::function<void(const std::vector<uint8_t>&)> callback) {
    on_packet_ = callback;
}

void PSMuxer::Write(uint8_t stream_id, const uint8_t* data, size_t size, uint64_t pts, uint64_t dts) {
    ElementaryStreamElem* stream = nullptr;
    for (auto& s : psm_.stream_map) {
        if (s.elementary_stream_id == stream_id) {
            stream = &s;
            break;
        }
    }
    if (!stream) return;
    if (size == 0) return;

    bool with_aud = false;
    bool idr_flag = false;
    bool vcl = false;

    if (stream->stream_type == PS_STREAM_H264) {
        H264Utils::SplitFrame(data, size, [&](const uint8_t* nalu, size_t len) {
            if (H264Utils::IsH264AUD(nalu, len)) {
                with_aud = true;
                return false;
            }
            if (H264Utils::IsH264VCL(nalu, len)) {
                if (H264Utils::IsH264IDR(nalu, len)) {
                    idr_flag = true;
                }
                vcl = true;
                return false;
            }
            return true;
        });
    } else if (stream->stream_type == PS_STREAM_H265) {
        H264Utils::SplitFrame(data, size, [&](const uint8_t* nalu, size_t len) {
            if (H264Utils::IsH265AUD(nalu, len)) {
                with_aud = true;
                return false;
            }
            if (H264Utils::IsH265VCL(nalu, len)) {
                if (H264Utils::IsH265IDR(nalu, len)) {
                    idr_flag = true;
                }
                vcl = true;
                return false;
            }
            return true;
        });
    }

    uint64_t scr = dts * 90; // System Clock Reference
    WritePackHeader(scr, 6106); // 6106 from Go code

    if (first_frame_ || idr_flag) {
        WriteSystemHeader();
        WriteProgramStreamMap();
        first_frame_ = false;
    }

    WritePesPacket(stream_id, data, size, pts * 90, dts * 90, idr_flag, with_aud, stream->stream_type);
}

void PSMuxer::WritePackHeader(uint64_t scr, uint32_t mux_rate) {
    BitStreamWriter bsw(14);
    bsw.PutByte(0x00); bsw.PutByte(0x00); bsw.PutByte(0x01); bsw.PutByte(0xBA);
    bsw.PutUint8(1, 2); // '01'
    bsw.PutUint64((scr >> 30) & 0x07, 3);
    bsw.PutUint8(1, 1);
    bsw.PutUint64((scr >> 15) & 0x7FFF, 15);
    bsw.PutUint8(1, 1);
    bsw.PutUint64(scr & 0x7FFF, 15);
    bsw.PutUint8(1, 1);
    bsw.PutUint16(0, 9); // extension
    bsw.PutUint8(1, 1);
    bsw.PutUint32(mux_rate, 22);
    bsw.PutUint8(1, 1);
    bsw.PutUint8(1, 1);
    bsw.PutUint8(0x1F, 5); // reserved
    bsw.PutUint8(0, 3); // stuffing length
    
    if (on_packet_) on_packet_(bsw.Bits());
}

void PSMuxer::WriteSystemHeader() {
    BitStreamWriter bsw(64);
    bsw.PutByte(0x00); bsw.PutByte(0x00); bsw.PutByte(0x01); bsw.PutByte(0xBB);
    bsw.PutUint16(0, 16); // length placeholder
    int loc = bsw.Size();
    
    bsw.PutUint8(1, 1);
    bsw.PutUint32(system_header_.rate_bound, 22);
    bsw.PutUint8(1, 1);
    bsw.PutUint8(system_header_.audio_bound, 6);
    bsw.PutUint8(0, 1); // fixed_flag
    bsw.PutUint8(0, 1); // CSPS_flag
    bsw.PutUint8(0, 1); // audio_lock
    bsw.PutUint8(0, 1); // video_lock
    bsw.PutUint8(1, 1);
    bsw.PutUint8(system_header_.video_bound, 5);
    bsw.PutUint8(0, 1); // restriction
    bsw.PutUint8(0x7F, 7); // reserved
    
    for (const auto& stream : system_header_.streams) {
        bsw.PutUint8(stream.stream_id, 8);
        bsw.PutUint8(3, 2);
        bsw.PutUint8(stream.p_std_buffer_bound_scale, 1);
        bsw.PutUint16(stream.p_std_buffer_size_bound, 13);
    }
    
    int length = bsw.Size() - loc;
    bsw.SetUint16(length, loc - 2);
    
    if (on_packet_) on_packet_(bsw.Bits());
}

void PSMuxer::WriteProgramStreamMap() {
    BitStreamWriter bsw(64);
    bsw.PutByte(0x00); bsw.PutByte(0x00); bsw.PutByte(0x01); bsw.PutByte(0xBC);
    bsw.PutUint16(0, 16); // length placeholder
    int loc = bsw.Size();
    
    bsw.PutUint8(psm_.current_next_indicator, 1);
    bsw.PutUint8(3, 2);
    bsw.PutUint8(psm_.program_stream_map_version, 5);
    bsw.PutUint8(0x7F, 7);
    bsw.PutUint8(1, 1);
    bsw.PutUint16(0, 16); // program_stream_info_length
    
    uint16_t es_map_len = psm_.stream_map.size() * 4;
    bsw.PutUint16(es_map_len, 16);
    
    for (const auto& stream : psm_.stream_map) {
        bsw.PutUint8(stream.stream_type, 8);
        bsw.PutUint8(stream.elementary_stream_id, 8);
        bsw.PutUint16(0, 16); // ES_info_length
    }
    
    int length = bsw.Size() - loc + 4; // +4 for CRC
    bsw.SetUint16(length, loc - 2);
    
    uint32_t crc = CalcCrc32(0xFFFFFFFF, bsw.Bits().data() + loc - 2 - 4, length - 4 + 4 + 2); 
    // Wait, CRC calculation range is from packet_start_code_prefix? No.
    // Go code: bsw.Bits()[bsw.ByteOffset()-int(length-4)-4:bsw.ByteOffset()]
    // length includes CRC (4 bytes).
    // Start is: ByteOffset - (length - 4) - 4.
    // ByteOffset is current end.
    // So it includes 00 00 01 BC? No.
    // Go code: bsw.PutBytes([]byte{0x00, 0x00, 0x01, 0xBC}) ...
    // The slice is from ... wait.
    // Go code: bsw.Bits()[bsw.ByteOffset()-int(length-4)-4:bsw.ByteOffset()]
    // length is value written to length field.
    // length field is after 00 00 01 BC.
    // So it includes 00 00 01 BC?
    // 00 00 01 BC (4) + Length (2) + Data...
    // Go code seems to include everything from 00 00 01 BC?
    // Let's check Go code again.
    // bsw.PutBytes([]byte{0x00, 0x00, 0x01, 0xBC})
    // loc := bsw.ByteOffset()
    // ...
    // length := bsw.DistanceFromMarkDot()/8 + 4
    // bsw.SetUint16(uint16(length), loc)
    // crc := codec.CalcCrc32(0xffffffff, bsw.Bits()[bsw.ByteOffset()-int(length-4)-4:bsw.ByteOffset()])
    // bsw.ByteOffset() is at the end (before CRC).
    // length is (Data + CRC).
    // length-4 is Data.
    // So it starts at ByteOffset - Data - 4.
    // 4 is 00 00 01 BC? No, 4 is Length field (2) + 00 00 01 BC (4)? No.
    // The offset calculation is confusing.
    // Let's assume it calculates CRC over the whole packet including start code?
    // Standard says: CRC is calculated over the entire Program Stream Map.
    // Program Stream Map starts with packet_start_code_prefix.
    // So yes, from 00 00 01 BC.
    
    // My bsw.Bits() contains everything written so far.
    // Start index is 0 (since I created new BSW).
    // Length to calculate CRC on is bsw.Size().
    crc = CalcCrc32(0xFFFFFFFF, bsw.Bits().data(), bsw.Size());
    
    bsw.PutByte(crc & 0xFF);
    bsw.PutByte((crc >> 8) & 0xFF);
    bsw.PutByte((crc >> 16) & 0xFF);
    bsw.PutByte((crc >> 24) & 0xFF);
    
    if (on_packet_) on_packet_(bsw.Bits());
}

void PSMuxer::WritePesPacket(uint8_t stream_id, const uint8_t* data, size_t size, uint64_t pts, uint64_t dts, bool idr_flag, bool with_aud, uint8_t stream_type) {
    BitStreamWriter bsw(1024);
    size_t offset = 0;
    bool first = true;

    while (offset < size) {
        bsw.Reset();
        
        int pes_hdr_len = 13; // Start code (3) + StreamID (1) + Len (2) + Flags (2) + HdrLen (1) + PTS (5) + DTS (5)
        // Actually:
        // 00 00 01 (3)
        // StreamID (1)
        // Len (2)
        // '10' (2 bits) + flags (6 bits) -> 1 byte
        // PTS_DTS_flags (2 bits) + ... -> 1 byte
        // PES_header_data_length (1 byte)
        // PTS (5)
        // DTS (5)
        // Total: 3 + 1 + 2 + 1 + 1 + 1 + 5 + 5 = 19 bytes.
        // Go code says peshdrlen := 13.
        // Wait, Go code:
        // pespkg.PTS_DTS_flags = 0x03
        // pespkg.PES_header_data_length = 10
        // pespkg.Encode(bsw)
        // Encode does:
        // 00 00 01 StreamID
        // Len
        // '10' ...
        // PTS/DTS
        // So header size is indeed 19 bytes if PTS/DTS present.
        // Go code: peshdrlen := 13.
        // Maybe it means 13 bytes AFTER the first 6 bytes?
        // 13 = 3 (Flags+Len) + 10 (PTS/DTS).
        // Total 6 + 13 = 19.
        
        std::vector<uint8_t> payload_prefix;
        if (first && !with_aud) {
            if (stream_type == PS_STREAM_H264) {
                payload_prefix = {0x00, 0x00, 0x00, 0x01, 0x09, 0xF0};
            } else if (stream_type == PS_STREAM_H265) {
                payload_prefix = {0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x50};
            }
        }
        
        size_t max_payload = 0xFFFF - 19 - payload_prefix.size(); // Max PES packet size 65535
        size_t to_write = std::min(size - offset, max_payload);
        
        // Write PES Header
        bsw.PutByte(0x00); bsw.PutByte(0x00); bsw.PutByte(0x01);
        bsw.PutByte(stream_id);
        
        size_t packet_len = 13 + payload_prefix.size() + to_write; // 13 is header size excluding first 6 bytes
        if (packet_len > 0xFFFF) packet_len = 0xFFFF; // Should not happen with calculation above
        
        bsw.PutUint16(packet_len, 16);
        
        bsw.PutUint8(0x02, 2); // '10'
        bsw.PutUint8(0, 2);
        bsw.PutUint8(0, 1);
        bsw.PutUint8(idr_flag ? 1 : 0, 1); // data_alignment
        bsw.PutUint8(0, 1);
        bsw.PutUint8(0, 1);
        
        bsw.PutUint8(0x03, 2); // PTS_DTS
        bsw.PutUint8(0, 1);
        bsw.PutUint8(0, 1);
        bsw.PutUint8(0, 1);
        bsw.PutUint8(0, 1);
        bsw.PutUint8(0, 1);
        bsw.PutUint8(0, 1);
        
        bsw.PutByte(10); // header data length
        
        // PTS
        bsw.PutUint8(0x03, 4);
        bsw.PutUint8((pts >> 30) & 0x07, 3);
        bsw.PutUint8(1, 1);
        bsw.PutUint16((pts >> 15) & 0x7FFF, 15);
        bsw.PutUint8(1, 1);
        bsw.PutUint16(pts & 0x7FFF, 15);
        bsw.PutUint8(1, 1);
        
        // DTS
        bsw.PutUint8(0x01, 4);
        bsw.PutUint8((dts >> 30) & 0x07, 3);
        bsw.PutUint8(1, 1);
        bsw.PutUint16((dts >> 15) & 0x7FFF, 15);
        bsw.PutUint8(1, 1);
        bsw.PutUint16(dts & 0x7FFF, 15);
        bsw.PutUint8(1, 1);
        
        if (!payload_prefix.empty()) {
            bsw.PutBytes(payload_prefix.data(), payload_prefix.size());
        }
        
        bsw.PutBytes(data + offset, to_write);
        offset += to_write;
        
        if (on_packet_) on_packet_(bsw.Bits());
        
        first = false;
    }
}

} // namespace mpeg2
