#include "ps_muxer.h"
#include <algorithm>

namespace mpeg2 {

PSMuxer::PSMuxer()
    : first_frame_(true) {
    system_ = std::make_unique<SystemHeader>();
    system_->rate_bound = 26234;
    
    psm_ = std::make_unique<ProgramStreamMap>();
    psm_->current_next_indicator = 1;
    psm_->program_stream_map_version = 1;
}

PSMuxer::~PSMuxer() = default;

uint8_t PSMuxer::AddStream(PSStreamType cid) {
    if (cid == PSStreamType::H265 || cid == PSStreamType::H264) {
        ElementaryStream es(static_cast<uint8_t>(PESStreamID::STREAM_VIDEO) + system_->video_bound);
        es.p_std_buffer_bound_scale = 1;
        es.p_std_buffer_size_bound = 400;
        system_->streams.push_back(es);
        system_->video_bound++;
        
        psm_->stream_map.push_back(ElementaryStreamElem(static_cast<uint8_t>(cid), es.stream_id));
        psm_->program_stream_map_version++;
        return es.stream_id;
    } else {
        ElementaryStream es(static_cast<uint8_t>(PESStreamID::STREAM_AUDIO) + system_->audio_bound);
        es.p_std_buffer_bound_scale = 0;
        es.p_std_buffer_size_bound = 32;
        system_->streams.push_back(es);
        system_->audio_bound++;
        
        psm_->stream_map.push_back(ElementaryStreamElem(static_cast<uint8_t>(cid), es.stream_id));
        psm_->program_stream_map_version++;
        return es.stream_id;
    }
}

int PSMuxer::Write(uint8_t sid, const std::vector<uint8_t>& frame, uint64_t pts, uint64_t dts) {
    // Find stream
    ElementaryStreamElem* stream = nullptr;
    for (auto& es : psm_->stream_map) {
        if (es.elementary_stream_id == sid) {
            stream = &es;
            break;
        }
    }
    
    if (!stream) {
        return static_cast<int>(Mpeg2Error::NOT_FOUND);
    }
    
    if (frame.empty()) {
        return static_cast<int>(Mpeg2Error::SUCCESS);
    }
    
    bool with_aud = false;
    bool idr_flag = false;
    bool vcl = false;
    
    PSStreamType stream_type = static_cast<PSStreamType>(stream->stream_type);
    
    // Check for AUD NALU (simplified - would need full codec implementation)
    if (stream_type == PSStreamType::H264 || stream_type == PSStreamType::H265) {
        // Simplified check - in real implementation would parse NALUs
        if (frame.size() >= 6) {
            if (stream_type == PSStreamType::H264) {
                if (frame[0] == 0 && frame[1] == 0 && frame[2] == 0 && 
                    frame[3] == 1 && (frame[4] & 0x1F) == 9) {
                    with_aud = true;
                }
            } else {
                if (frame[0] == 0 && frame[1] == 0 && frame[2] == 0 && 
                    frame[3] == 1 && ((frame[4] >> 1) & 0x3F) == 35) {
                    with_aud = true;
                }
            }
        }
    }
    
    // Convert timestamps to 90kHz
    dts = dts * 90;
    pts = pts * 90;
    
    BitStreamWriter bsw(1024);
    
    // Write PS Pack Header
    PSPackHeader pack;
    pack.system_clock_reference_base = dts - 3600;
    pack.system_clock_reference_extension = 0;
    pack.program_mux_rate = 6106;
    pack.Encode(bsw);
    
    // Write System Header and PSM on first frame or IDR
    if (first_frame_ || idr_flag) {
        system_->Encode(bsw);
        psm_->Encode(bsw);
        first_frame_ = false;
    }
    
    // Write PES packets
    auto frame_data = frame;
    bool first = true;
    
    while (!frame_data.empty()) {
        PesPacket pes_pkg;
        pes_pkg.stream_id = sid;
        pes_pkg.pts_dts_flags = 0x03;
        pes_pkg.pes_header_data_length = 10;
        pes_pkg.pts = pts;
        pes_pkg.dts = dts;
        
        if (idr_flag) {
            pes_pkg.data_alignment_indicator = 1;
        }
        
        size_t pes_hdr_len = 13;
        
        // Add AUD if needed
        if (first && !with_aud && vcl) {
            if (stream_type == PSStreamType::H264) {
                pes_pkg.pes_payload.insert(pes_pkg.pes_payload.end(), 
                                          H264_AUD_NALU, 
                                          H264_AUD_NALU + H264_AUD_NALU_SIZE);
                pes_hdr_len += H264_AUD_NALU_SIZE;
            } else if (stream_type == PSStreamType::H265) {
                pes_pkg.pes_payload.insert(pes_pkg.pes_payload.end(),
                                          H265_AUD_NALU,
                                          H265_AUD_NALU + H265_AUD_NALU_SIZE);
                pes_hdr_len += H265_AUD_NALU_SIZE;
            }
        }
        
        // Split large frames
        if (pes_hdr_len + frame_data.size() >= 0xFFFF) {
            pes_pkg.pes_packet_length = 0xFFFF;
            size_t payload_size = 0xFFFF - pes_hdr_len;
            pes_pkg.pes_payload.insert(pes_pkg.pes_payload.end(),
                                       frame_data.begin(),
                                       frame_data.begin() + payload_size);
            frame_data.erase(frame_data.begin(), frame_data.begin() + payload_size);
        } else {
            pes_pkg.pes_packet_length = pes_hdr_len + frame_data.size();
            pes_pkg.pes_payload.insert(pes_pkg.pes_payload.end(),
                                       frame_data.begin(),
                                       frame_data.end());
            frame_data.clear();
        }
        
        pes_pkg.Encode(bsw);
        
        if (on_packet_) {
            on_packet_(bsw.Bits());
        }
        
        bsw.Reset();
        first = false;
    }
    
    return static_cast<int>(Mpeg2Error::SUCCESS);
}

} // namespace mpeg2
