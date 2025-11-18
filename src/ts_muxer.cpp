#include "ts_muxer.h"
#include "ps_proto.h"

namespace mpeg2 {

TSMuxer::TSMuxer()
    : stream_pid_(0x100), pmt_pid_(0x200), pat_period_(0) {
    pat_ = std::make_unique<TablePAT>();
}

TSMuxer::~TSMuxer() = default;

uint16_t TSMuxer::AddStream(TSStreamType cid) {
    if (!pat_) {
        pat_ = std::make_unique<TablePAT>();
    }
    
    if (pat_->pmts.empty()) {
        auto pmt = std::make_unique<TablePMT>();
        pmt->pid = pmt_pid_;
        pmt->pm = 1;
        pmt_pid_++;
        pat_->pmts.push_back(std::move(pmt));
    }
    
    uint16_t sid = stream_pid_;
    auto stream = std::make_unique<PESStream>();
    stream->pid = sid;
    stream->stream_type = cid;
    stream->cc = 0;
    stream_pid_++;
    
    pat_->pmts[0]->streams.push_back(std::move(stream));
    return sid;
}

int TSMuxer::Write(uint16_t pid, const std::vector<uint8_t>& data, uint64_t pts, uint64_t dts) {
    // Find stream and PMT
    TablePMT* which_pmt = nullptr;
    PESStream* which_stream = nullptr;
    
    for (auto& pmt : pat_->pmts) {
        for (auto& stream : pmt->streams) {
            if (stream->pid == pid) {
                which_pmt = pmt.get();
                which_stream = stream.get();
                break;
            }
        }
        if (which_stream) break;
    }
    
    if (!which_pmt || !which_stream) {
        return static_cast<int>(Mpeg2Error::NOT_FOUND);
    }
    
    // Set PCR PID
    if (which_pmt->pcr_pid == 0) {
        which_pmt->pcr_pid = pid;
    }
    
    // Check for AUD NALU
    bool with_aud = false;
    if (which_stream->stream_type == TSStreamType::H264 || 
        which_stream->stream_type == TSStreamType::H265) {
        if (data.size() >= 6) {
            if (which_stream->stream_type == TSStreamType::H264) {
                if (data[0] == 0 && data[1] == 0 && data[2] == 0 && 
                    data[3] == 1 && (data[4] & 0x1F) == 9) {
                    with_aud = true;
                }
            } else {
                if (data[0] == 0 && data[1] == 0 && data[2] == 0 && 
                    data[3] == 1 && ((data[4] >> 1) & 0x3F) == 35) {
                    with_aud = true;
                }
            }
        }
    }
    
    // Write PAT/PMT periodically
    if (pat_period_ == 0 || pat_period_ + 400 < dts) {
        pat_period_ = dts;
        if (pat_period_ == 0) {
            pat_period_ = 1;
        }
        
        // Write PAT
        WritePAT();
        
        // Write PMT
        WritePMT(which_pmt);
    }
    
    // Check if IDR frame (simplified)
    bool idr_flag = false;
    
    // Write PES
    WritePES(which_stream, which_pmt, data, pts * 90, dts * 90, idr_flag, with_aud);
    
    return static_cast<int>(Mpeg2Error::SUCCESS);
}

void TSMuxer::WritePAT() {
    PAT pat;
    pat.table_id = static_cast<uint8_t>(TableID::PAS);
    pat.section_syntax_indicator = 1;
    pat.current_next_indicator = 1;
    pat.version_number = pat_->version_number;
    pat.transport_stream_id = 1;
    
    for (const auto& pmt : pat_->pmts) {
        PmtPair pair;
        pair.program_number = pmt->pm;
        pair.pid = pmt->pid;
        pat.pmts.push_back(pair);
    }
    
    TSPacket ts_hdr;
    ts_hdr.payload_unit_start_indicator = 1;
    ts_hdr.pid = 0;
    ts_hdr.adaptation_field_control = 0x01;
    ts_hdr.continuity_counter = pat_->cc;
    pat_->cc = (pat_->cc + 1) % 16;
    
    BitStreamWriter bsw(TS_PACKET_SIZE);
    ts_hdr.EncodeHeader(bsw);
    bsw.PutByte(0x00); // pointer
    pat.Encode(bsw);
    
    // Fill remaining with 0xFF
    while (bsw.Bits().size() < TS_PACKET_SIZE) {
        bsw.PutByte(0xFF);
    }
    
    if (on_packet_) {
        on_packet_(bsw.Bits());
    }
}

void TSMuxer::WritePMT(TablePMT* pmt) {
    PMT pmt_table;
    pmt_table.table_id = static_cast<uint8_t>(TableID::PMS);
    pmt_table.section_syntax_indicator = 1;
    pmt_table.current_next_indicator = 1;
    pmt_table.program_number = pmt->pm;
    pmt_table.version_number = pmt->version_number;
    pmt_table.pcr_pid = pmt->pcr_pid;
    
    for (const auto& stream : pmt->streams) {
        PmtStream ps;
        ps.stream_type = static_cast<uint8_t>(stream->stream_type);
        ps.elementary_pid = stream->pid;
        ps.es_info_length = 0;
        pmt_table.streams.push_back(ps);
    }
    
    TSPacket ts_hdr;
    ts_hdr.payload_unit_start_indicator = 1;
    ts_hdr.pid = pmt->pid;
    ts_hdr.adaptation_field_control = 0x01;
    ts_hdr.continuity_counter = pmt->cc;
    pmt->cc = (pmt->cc + 1) % 16;
    
    BitStreamWriter bsw(TS_PACKET_SIZE);
    ts_hdr.EncodeHeader(bsw);
    bsw.PutByte(0x00); // pointer
    pmt_table.Encode(bsw);
    
    // Fill remaining with 0xFF
    while (bsw.Bits().size() < TS_PACKET_SIZE) {
        bsw.PutByte(0xFF);
    }
    
    if (on_packet_) {
        on_packet_(bsw.Bits());
    }
}

void TSMuxer::WritePES(PESStream* pes_stream, TablePMT* pmt, 
                       const std::vector<uint8_t>& data,
                       uint64_t pts, uint64_t dts, bool idr_flag, bool with_aud) {
    auto frame_data = data;
    bool first_pes_packet = true;
    
    while (!frame_data.empty()) {
        BitStreamWriter bsw(TS_PACKET_SIZE);
        
        TSPacket ts_hdr;
        ts_hdr.payload_unit_start_indicator = first_pes_packet ? 1 : 0;
        ts_hdr.pid = pes_stream->pid;
        ts_hdr.adaptation_field_control = 0x01;
        ts_hdr.continuity_counter = pes_stream->cc;
        pes_stream->cc = (pes_stream->cc + 1) % 16;
        
        size_t head_len = 4;
        std::unique_ptr<AdaptationField> adaptation;
        
        // Add adaptation field if needed
        if (first_pes_packet && idr_flag) {
            adaptation = std::make_unique<AdaptationField>();
            ts_hdr.adaptation_field_control |= 0x20;
            adaptation->random_access_indicator = 1;
            head_len += 2;
        }
        
        // Add PCR if needed
        if (first_pes_packet && pes_stream->pid == pmt->pcr_pid) {
            if (!adaptation) {
                adaptation = std::make_unique<AdaptationField>();
                head_len += 2;
            }
            ts_hdr.adaptation_field_control |= 0x20;
            adaptation->pcr_flag = 1;
            
            uint64_t pcr_base = (dts == 0 ? pts : dts) / 300;
            uint16_t pcr_ext = (dts == 0 ? pts : dts) % 300;
            adaptation->program_clock_reference_base = pcr_base;
            adaptation->program_clock_reference_extension = pcr_ext;
            head_len += 6;
        }
        
        std::vector<uint8_t> payload;
        PesPacket* pes_pkg = nullptr;
        
        if (first_pes_packet) {
            size_t old_head_len = head_len;
            head_len += 19;
            
            // Add AUD if needed
            if (!with_aud) {
                if (pes_stream->stream_type == TSStreamType::H264) {
                    payload.insert(payload.end(), H264_AUD_NALU, 
                                  H264_AUD_NALU + H264_AUD_NALU_SIZE);
                    head_len += H264_AUD_NALU_SIZE;
                } else if (pes_stream->stream_type == TSStreamType::H265) {
                    payload.insert(payload.end(), H265_AUD_NALU,
                                  H265_AUD_NALU + H265_AUD_NALU_SIZE);
                    head_len += H265_AUD_NALU_SIZE;
                }
            }
            
            PesPacket pes;
            pes.pts_dts_flags = 0x03;
            pes.pes_header_data_length = 10;
            pes.pts = pts;
            pes.dts = dts;
            pes.stream_id = static_cast<uint8_t>(PESStreamID::STREAM_VIDEO);
            
            if (idr_flag) {
                pes.data_alignment_indicator = 1;
            }
            
            if (head_len - old_head_len - 6 + frame_data.size() > 0xFFFF) {
                pes.pes_packet_length = 0;
            } else {
                pes.pes_packet_length = frame_data.size() + head_len - old_head_len - 6;
            }
            
            // Encode PES to get payload
            BitStreamWriter temp_bsw;
            pes.Encode(temp_bsw);
            // Extract PES header and add to payload (simplified)
        }
        
        // Fill payload
        size_t available = TS_PACKET_SIZE - head_len;
        if (frame_data.size() < available) {
            // Need stuffing
            if (!adaptation) {
                adaptation = std::make_unique<AdaptationField>();
                head_len += 1;
                available = TS_PACKET_SIZE - head_len;
            }
            
            size_t stuffing = TS_PACKET_SIZE - frame_data.size() - head_len;
            payload.insert(payload.end(), frame_data.begin(), frame_data.end());
            frame_data.clear();
            
            // Add stuffing bytes
            for (size_t i = 0; i < stuffing; i++) {
                payload.push_back(0xFF);
            }
        } else {
            payload.insert(payload.end(), frame_data.begin(), 
                          frame_data.begin() + available);
            frame_data.erase(frame_data.begin(), frame_data.begin() + available);
        }
        
        // Encode TS packet
        if (adaptation) {
            ts_hdr.field = std::move(adaptation);
        }
        
        ts_hdr.EncodeHeader(bsw);
        
        if (first_pes_packet) {
            // Write PES packet with payload
            PesPacket pes;
            pes.stream_id = static_cast<uint8_t>(PESStreamID::STREAM_VIDEO);
            pes.pts_dts_flags = 0x03;
            pes.pes_header_data_length = 10;
            pes.pts = pts;
            pes.dts = dts;
            pes.pes_payload = payload;
            if (idr_flag) {
                pes.data_alignment_indicator = 1;
            }
            pes.Encode(bsw);
        } else {
            bsw.PutBytes(payload.data(), payload.size());
        }
        
        // Ensure packet is exactly TS_PACKET_SIZE
        while (bsw.Bits().size() < TS_PACKET_SIZE) {
            bsw.PutByte(0xFF);
        }
        
        if (on_packet_ && bsw.Bits().size() == TS_PACKET_SIZE) {
            on_packet_(bsw.Bits());
        }
        
        first_pes_packet = false;
    }
}

} // namespace mpeg2
