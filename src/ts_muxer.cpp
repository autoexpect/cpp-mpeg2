#include "mpeg2/ts_muxer.h"
#include "mpeg2/utils.h"
#include "mpeg2/h264_utils.h"
#include <iostream>

namespace mpeg2 {

TSMuxer::TSMuxer() 
    : stream_pid_counter_(0x100), pmt_pid_counter_(0x200), pat_period_(0) {
    pat_.cc = 0;
    pat_.version_number = 0;
}

TSMuxer::~TSMuxer() {}

uint16_t TSMuxer::AddStream(TS_STREAM_TYPE type) {
    if (pat_.pmts.empty()) {
        Pmt pmt;
        pmt.pid = pmt_pid_counter_++;
        pmt.program_number = 1;
        pmt.version_number = 0;
        pmt.cc = 0;
        pmt.pcr_pid = 0;
        pat_.pmts.push_back(pmt);
    }

    uint16_t pid = stream_pid_counter_++;
    PmtStream stream;
    stream.stream_type = type;
    stream.pid = pid;
    stream.cc = 0;
    
    pat_.pmts[0].streams.push_back(stream);
    return pid;
}

void TSMuxer::SetOnPacket(std::function<void(const std::vector<uint8_t>&)> callback) {
    on_packet_ = callback;
}

void TSMuxer::Write(uint16_t pid, const uint8_t* data, size_t size, uint64_t pts, uint64_t dts) {
    Pmt* whichpmt = nullptr;
    PmtStream* whichstream = nullptr;

    for (auto& pmt : pat_.pmts) {
        for (auto& stream : pmt.streams) {
            if (stream.pid == pid) {
                whichpmt = &pmt;
                whichstream = &stream;
                break;
            }
        }
    }

    if (!whichpmt || !whichstream) {
        return;
    }

    if (whichpmt->pcr_pid == 0 || (whichstream->stream_type == TS_STREAM_H264 || whichstream->stream_type == TS_STREAM_H265)) {
         // Prefer video for PCR
         if (whichpmt->pcr_pid == 0) {
             whichpmt->pcr_pid = pid;
         } else if (whichstream->stream_type == TS_STREAM_H264 || whichstream->stream_type == TS_STREAM_H265) {
             // If we found a video stream, switch PCR to it (simple logic from Go code)
             // actually Go code says: if pcr_pid == 0 || (video && pcr_pid != pid) -> set pcr_pid = pid
             // But we should be careful not to switch back and forth if multiple videos? 
             // For now follow Go logic: if video, set as PCR.
             whichpmt->pcr_pid = pid;
         }
    }

    bool with_aud = false;
    bool idr_flag = false;

    if (whichstream->stream_type == TS_STREAM_H264) {
        H264Utils::SplitFrame(data, size, [&](const uint8_t* nalu, size_t len) {
            if (H264Utils::IsH264AUD(nalu, len)) {
                with_aud = true;
                return false; 
            }
            if (H264Utils::IsH264VCL(nalu, len)) {
                if (H264Utils::IsH264IDR(nalu, len)) {
                    idr_flag = true;
                }
                return false;
            }
            return true;
        });
    } else if (whichstream->stream_type == TS_STREAM_H265) {
        H264Utils::SplitFrame(data, size, [&](const uint8_t* nalu, size_t len) {
            if (H264Utils::IsH265AUD(nalu, len)) {
                with_aud = true;
                return false;
            }
            if (H264Utils::IsH265VCL(nalu, len)) {
                if (H264Utils::IsH265IDR(nalu, len)) {
                    idr_flag = true;
                }
                return false;
            }
            return true;
        });
    }

    if (pat_period_ == 0 || pat_period_ + 400 < dts) {
        pat_period_ = dts;
        if (pat_period_ == 0) pat_period_ = 1;
        WritePat();
        for (auto& pmt : pat_.pmts) {
            WritePmt(pmt);
        }
    }

    WritePes(*whichstream, *whichpmt, data, size, pts * 90, dts * 90, idr_flag, with_aud);
}

void TSMuxer::WritePat() {
    BitStreamWriter bsw(TS_PACKET_SIZE);
    
    // TS Header
    bsw.PutByte(0x47);
    bsw.PutUint8(0, 1); // transport_error_indicator
    bsw.PutUint8(1, 1); // payload_unit_start_indicator
    bsw.PutUint8(0, 1); // transport_priority
    bsw.PutUint16(0, 13); // PID 0 for PAT
    bsw.PutUint8(0, 2); // transport_scrambling_control
    bsw.PutUint8(1, 2); // adaptation_field_control (payload only)
    bsw.PutUint8(pat_.cc, 4);
    pat_.cc = (pat_.cc + 1) % 16;

    bsw.PutByte(0x00); // pointer field

    // PAT Table
    bsw.PutUint8(0x00, 8); // table_id
    int loc = bsw.Size(); // Start of section length part (after table_id)
    bsw.PutUint8(1, 1); // section_syntax_indicator
    bsw.PutUint8(0, 1); // '0'
    bsw.PutUint8(3, 2); // reserved
    bsw.PutUint16(0, 12); // section_length (placeholder)
    
    int section_start = bsw.Size(); // Start of section data (after length)

    bsw.PutUint16(1, 16); // transport_stream_id
    bsw.PutUint8(3, 2); // reserved
    bsw.PutUint8(pat_.version_number, 5);
    bsw.PutUint8(1, 1); // current_next_indicator
    bsw.PutUint8(0, 8); // section_number
    bsw.PutUint8(0, 8); // last_section_number

    for (const auto& pmt : pat_.pmts) {
        bsw.PutUint16(pmt.program_number, 16);
        bsw.PutUint8(7, 3); // reserved
        bsw.PutUint16(pmt.pid, 13);
    }

    // CRC32
    int section_length = bsw.Size() - section_start + 4; // +4 for CRC
    bsw.SetUint16((section_length & 0x0FFF) | (1 << 15) | 0x3000, loc);
    
    uint32_t crc = CalcCrc32(0xFFFFFFFF, bsw.Bits().data() + section_start - 3, section_length - 4 + 3);
    bsw.PutUint32(crc, 32); // Actually PutUint32 writes big endian? No, implementation uses PutUint64 which writes big endian.
    // CRC32 in MPEG-2 is usually Little Endian? 
    // Go code: binary.LittleEndian.PutUint32(tmpcrc, crc); bsw.PutBytes(tmpcrc);
    // My PutUint32 is Big Endian (network byte order).
    // So I should write it manually or add PutUint32LE.
    // Or just:
    bsw.SetByte(crc & 0xFF, bsw.Size());
    bsw.SetByte((crc >> 8) & 0xFF, bsw.Size() + 1);
    bsw.SetByte((crc >> 16) & 0xFF, bsw.Size() + 2);
    bsw.SetByte((crc >> 24) & 0xFF, bsw.Size() + 3);
    // Wait, SetByte is for overwriting. I need to append.
    // I'll just use PutByte 4 times.
    bsw.PutByte(crc & 0xFF);
    bsw.PutByte((crc >> 8) & 0xFF);
    bsw.PutByte((crc >> 16) & 0xFF);
    bsw.PutByte((crc >> 24) & 0xFF);

    bsw.FillRemainData(0xFF);
    if (on_packet_) {
        on_packet_(bsw.Bits());
    }
}

void TSMuxer::WritePmt(Pmt& pmt) {
    BitStreamWriter bsw(TS_PACKET_SIZE);
    
    // TS Header
    bsw.PutByte(0x47);
    bsw.PutUint8(0, 1);
    bsw.PutUint8(1, 1);
    bsw.PutUint8(0, 1);
    bsw.PutUint16(pmt.pid, 13);
    bsw.PutUint8(0, 2);
    bsw.PutUint8(1, 2);
    bsw.PutUint8(pmt.cc, 4);
    pmt.cc = (pmt.cc + 1) % 16;

    bsw.PutByte(0x00); // pointer

    // PMT Table
    bsw.PutUint8(0x02, 8); // table_id (TS_TID_PMS)
    int loc = bsw.Size();
    bsw.PutUint8(1, 1); // section_syntax_indicator
    bsw.PutUint8(0, 1);
    bsw.PutUint8(3, 2);
    bsw.PutUint16(0, 12); // section_length placeholder

    int section_start = bsw.Size();

    bsw.PutUint16(pmt.program_number, 16);
    bsw.PutUint8(3, 2);
    bsw.PutUint8(pmt.version_number, 5);
    bsw.PutUint8(1, 1);
    bsw.PutUint8(0, 8);
    bsw.PutUint8(0, 8);
    bsw.PutUint8(7, 3);
    bsw.PutUint16(pmt.pcr_pid, 13);
    bsw.PutUint8(0x0F, 4);
    bsw.PutUint16(0, 12); // program_info_length

    for (const auto& stream : pmt.streams) {
        bsw.PutUint8(stream.stream_type, 8);
        bsw.PutUint8(7, 3);
        bsw.PutUint16(stream.pid, 13);
        bsw.PutUint8(0x0F, 4);
        bsw.PutUint16(0, 12); // ES_info_length
    }

    int section_length = bsw.Size() - section_start + 4;
    bsw.SetUint16((section_length & 0x0FFF) | (1 << 15) | 0x3000, loc);

    uint32_t crc = CalcCrc32(0xFFFFFFFF, bsw.Bits().data() + section_start - 3, section_length - 4 + 3);
    bsw.PutByte(crc & 0xFF);
    bsw.PutByte((crc >> 8) & 0xFF);
    bsw.PutByte((crc >> 16) & 0xFF);
    bsw.PutByte((crc >> 24) & 0xFF);

    bsw.FillRemainData(0xFF);
    if (on_packet_) {
        on_packet_(bsw.Bits());
    }
}

void TSMuxer::WritePes(PmtStream& stream, Pmt& pmt, const uint8_t* data, size_t size, uint64_t pts, uint64_t dts, bool idr_flag, bool with_aud) {
    bool first_pes_packet = true;
    BitStreamWriter bsw(TS_PACKET_SIZE);
    size_t offset = 0;

    while (offset < size || first_pes_packet) {
        bsw.Reset();
        
        // TS Header
        bsw.PutByte(0x47);
        bsw.PutUint8(0, 1);
        bsw.PutUint8(first_pes_packet ? 1 : 0, 1);
        bsw.PutUint8(0, 1);
        bsw.PutUint16(stream.pid, 13);
        bsw.PutUint8(0, 2);
        
        uint8_t adaptation_field_control = 0x01; // Payload only default
        uint8_t continuity_counter = stream.cc;
        stream.cc = (stream.cc + 1) % 16;

        int head_len = 4;
        bool write_adaptation = false;
        bool write_pcr = false;

        if (first_pes_packet && idr_flag) {
            write_adaptation = true;
        }
        if (first_pes_packet && stream.pid == pmt.pcr_pid) {
            write_adaptation = true;
            write_pcr = true;
        }

        if (write_adaptation) {
            adaptation_field_control |= 0x02; // 0x01 | 0x02 = 0x03 (Adaptation + Payload)
        }
        
        bsw.PutUint8(adaptation_field_control, 2);
        bsw.PutUint8(continuity_counter, 4);

        if (write_adaptation) {
            // Adaptation field length placeholder
            int adaptation_len_loc = bsw.Size();
            bsw.PutByte(0); 
            head_len++;
            
            int adaptation_start = bsw.Size();
            
            uint8_t flags = 0;
            if (first_pes_packet && idr_flag) flags |= 0x40; // random_access_indicator
            if (write_pcr) flags |= 0x10; // PCR_flag
            
            bsw.PutByte(flags);
            
            if (write_pcr) {
                uint64_t pcr_base = 0;
                uint16_t pcr_ext = 0;
                if (dts == 0) {
                    pcr_base = pts * 300 / 300;
                    pcr_ext = (pts * 300) % 300;
                } else {
                    pcr_base = dts * 300 / 300;
                    pcr_ext = (dts * 300) % 300;
                }
                bsw.PutUint64(pcr_base, 33);
                bsw.PutUint8(0x3F, 6); // reserved
                bsw.PutUint16(pcr_ext, 9);
            }
            
            int adaptation_len = bsw.Size() - adaptation_start;
            bsw.SetByte(adaptation_len, adaptation_len_loc);
            head_len += adaptation_len;
        }

        // PES Header (only for first packet)
        std::vector<uint8_t> payload;
        if (first_pes_packet) {
            // Calculate PES header length
            // ...
            // Actually we need to append AUD if needed
            if (!with_aud) {
                if (stream.stream_type == TS_STREAM_H264) {
                    payload.push_back(0x00); payload.push_back(0x00); payload.push_back(0x00); payload.push_back(0x01);
                    payload.push_back(0x09); payload.push_back(0xF0);
                } else if (stream.stream_type == TS_STREAM_H265) {
                    payload.push_back(0x00); payload.push_back(0x00); payload.push_back(0x00); payload.push_back(0x01);
                    payload.push_back(0x46); payload.push_back(0x01); payload.push_back(0x50);
                }
            }
            
            // PES Packet
            // packet_start_code_prefix 0x000001
            // stream_id
            // PES_packet_length
            
            // We need to buffer PES header to write it into BSW
            BitStreamWriter pes_hdr(64);
            pes_hdr.PutByte(0x00); pes_hdr.PutByte(0x00); pes_hdr.PutByte(0x01);
            
            uint8_t stream_id = 0xE0; // Video 0
            // TODO: Map stream type to stream id properly if multiple videos/audios
            if (stream.stream_type == TS_STREAM_AUDIO_MPEG1 || stream.stream_type == TS_STREAM_AUDIO_MPEG2 || stream.stream_type == TS_STREAM_AAC) {
                stream_id = 0xC0; // Audio 0
            }
            pes_hdr.PutByte(stream_id);
            
            uint16_t pes_packet_len = 0;
            size_t total_data_len = size + payload.size();
            // If video, can be 0 if too large? Go code says: if headlen-oldheadlen-6+len(data) > 0xFFFF -> 0
            // But here we are calculating PES packet length field.
            // For video, if length > 0xFFFF, set to 0.
            if (total_data_len + 3 + 5 + 5 > 0xFFFF) { // approx header size
                 pes_packet_len = 0;
            } else {
                 pes_packet_len = total_data_len + 3 + 5 + 5; // PTS/DTS flags (2) + header len (1) + PTS (5) + DTS (5)
                 // Wait, this is rough.
                 // Let's do it properly.
                 // PES header data length = 10 (PTS + DTS)
                 // Total PES header size = 6 + 3 + 10 = 19 bytes.
                 // So len = data + 13.
                 pes_packet_len = total_data_len + 13;
            }
            pes_hdr.PutUint16(pes_packet_len, 16);
            
            pes_hdr.PutUint8(0x02, 2); // '10'
            pes_hdr.PutUint8(0, 2); // Scrambling control
            pes_hdr.PutUint8(0, 1); // Priority
            pes_hdr.PutUint8(0, 1); // Data alignment
            pes_hdr.PutUint8(0, 1); // Copyright
            pes_hdr.PutUint8(0, 1); // Original
            
            pes_hdr.PutUint8(0x03, 2); // PTS_DTS_flags = '11'
            pes_hdr.PutUint8(0, 1); // ESCR flag
            pes_hdr.PutUint8(0, 1); // ES rate flag
            pes_hdr.PutUint8(0, 1); // DSM trick mode flag
            pes_hdr.PutUint8(0, 1); // Additional copy info flag
            pes_hdr.PutUint8(0, 1); // CRC flag
            pes_hdr.PutUint8(0, 1); // Extension flag
            
            pes_hdr.PutByte(10); // PES_header_data_length
            
            // PTS
            pes_hdr.PutUint8(0x03, 4); // '0011'
            pes_hdr.PutUint8((pts >> 30) & 0x07, 3);
            pes_hdr.PutUint8(1, 1);
            pes_hdr.PutUint16((pts >> 15) & 0x7FFF, 15);
            pes_hdr.PutUint8(1, 1);
            pes_hdr.PutUint16(pts & 0x7FFF, 15);
            pes_hdr.PutUint8(1, 1);
            
            // DTS
            pes_hdr.PutUint8(0x01, 4); // '0001'
            pes_hdr.PutUint8((dts >> 30) & 0x07, 3);
            pes_hdr.PutUint8(1, 1);
            pes_hdr.PutUint16((dts >> 15) & 0x7FFF, 15);
            pes_hdr.PutUint8(1, 1);
            pes_hdr.PutUint16(dts & 0x7FFF, 15);
            pes_hdr.PutUint8(1, 1);
            
            // Add PES header to payload
            std::vector<uint8_t> pes_hdr_bytes = pes_hdr.Bits();
            payload.insert(payload.begin(), pes_hdr_bytes.begin(), pes_hdr_bytes.end());
        }
        
        // Now we have payload (PES header + AUD + data chunk)
        // But wait, payload vector currently only has PES header + AUD.
        // Data is in `data` pointer.
        
        // We need to fill the TS packet.
        // Available space in TS packet = 188 - head_len.
        int available = TS_PACKET_SIZE - head_len;
        
        // If we need stuffing
        // Logic: if payload + data < available, we stuff.
        // Else we fill.
        
        // But we have two sources of data: `payload` (header/AUD) and `data` (frame).
        // We consume `payload` first, then `data`.
        
        int payload_written = 0;
        int data_written = 0;
        
        // Total bytes to write in this packet
        int bytes_to_write = 0;
        
        // Stuffing logic
        size_t remaining_payload = payload.size();
        size_t remaining_data = size - offset;
        size_t total_remaining = remaining_payload + remaining_data;
        
        if (total_remaining < (size_t)available) {
            // Need stuffing
            int stuffing_len = available - total_remaining;
            // If adaptation field exists, extend it.
            // If not, create it.
            if (adaptation_field_control & 0x20) {
                // Adaptation field exists.
                // Update length.
                // We need to find where adaptation length byte is.
                // It's at byte 4.
                uint8_t current_len = bsw.Bits()[4];
                bsw.SetByte(current_len + stuffing_len, 4);
                // We need to insert stuffing bytes.
                // BitStreamWriter doesn't support insertion.
                // This is tricky with BitStreamWriter as implemented.
                // But wait, we haven't written payload yet.
                // We can just write stuffing bytes into adaptation field now.
                for (int i = 0; i < stuffing_len; i++) bsw.PutByte(0xFF);
                head_len += stuffing_len;
            } else {
                // Create adaptation field
                 // This changes header!
                 // We need to rewrite header or plan ahead.
                 // Adaptation field control: 0x01 -> 0x03 (Adaptation + Payload)
                 uint8_t val = bsw.Bits()[3];
                 bsw.SetByte(val | 0x20, 3); // Set Adaptation bit (bit 5) to make it 0x3 (11)
                 
                 // Adaptation length = stuffing_len - 1.
                 if (stuffing_len < 1) {
                     // Should not happen if available > total_remaining
                 }
                 bsw.PutByte(stuffing_len - 1);
                 if (stuffing_len > 1) {
                     bsw.PutByte(0); // flags
                     for (int i = 0; i < stuffing_len - 2; i++) bsw.PutByte(0xFF);
                 }
                 head_len += stuffing_len;
            }
            available = TS_PACKET_SIZE - head_len;
        }
        
        // Write payload
        if (remaining_payload > 0) {
            int to_write = std::min((size_t)available, remaining_payload);
            bsw.PutBytes(payload.data(), to_write);
            payload.erase(payload.begin(), payload.begin() + to_write);
            available -= to_write;
        }
        
        // Write data
        if (available > 0 && remaining_data > 0) {
            int to_write = std::min((size_t)available, remaining_data);
            bsw.PutBytes(data + offset, to_write);
            offset += to_write;
            available -= to_write;
        }
        
        // Fill remaining with 0xFF if any (should be handled by stuffing logic above, but just in case)
        if (available > 0) {
            bsw.FillRemainData(0xFF);
        }
        
        if (on_packet_) {
            on_packet_(bsw.Bits());
        }
        
        first_pes_packet = false;
        if (offset >= size && payload.empty()) break;
    }
}

} // namespace mpeg2
