#include "ps_demuxer.h"
#include <cstring>

namespace mpeg2 {

PSDemuxer::PSDemuxer()
    : mpeg1_(false) {
}

PSDemuxer::~PSDemuxer() = default;

int PSDemuxer::Input(const uint8_t* data, size_t size) {
    // Merge with cache if needed
    std::vector<uint8_t> buffer;
    const uint8_t* parse_data = data;
    size_t parse_size = size;
    
    if (!cache_.empty()) {
        cache_.insert(cache_.end(), data, data + size);
        buffer = cache_;
        parse_data = buffer.data();
        parse_size = buffer.size();
    }
    
    BitStream bs(parse_data, parse_size);
    int ret = 0;
    
    while (!bs.EOS()) {
        if (ret == static_cast<int>(Mpeg2Error::NEED_MORE)) {
            // Save remaining data to cache
            const uint8_t* remain = bs.RemainData();
            if (remain && bs.RemainBytes() > 0) {
                cache_.assign(remain, remain + bs.RemainBytes());
            }
            break;
        }
        
        if (bs.RemainBits() < 32) {
            ret = static_cast<int>(Mpeg2Error::NEED_MORE);
            continue;
        }
        
        uint32_t prefix_code = bs.NextBits(32);
        
        switch (prefix_code) {
            case 0x000001BA: { // pack header
                if (!pkg_.header) {
                    pkg_.header = std::make_unique<PSPackHeader>();
                }
                ret = pkg_.header->Decode(bs);
                mpeg1_ = pkg_.header->is_mpeg1;
                if (on_packet_) {
                    on_packet_(ret);
                }
                break;
            }
            
            case 0x000001BB: { // system header
                if (!pkg_.system) {
                    pkg_.system = std::make_unique<SystemHeader>();
                }
                ret = pkg_.system->Decode(bs);
                if (on_packet_) {
                    on_packet_(ret);
                }
                break;
            }
            
            case 0x000001BC: { // program stream map
                if (!pkg_.psm) {
                    pkg_.psm = std::make_unique<ProgramStreamMap>();
                }
                ret = pkg_.psm->Decode(bs);
                if (ret == 0) {
                    for (const auto& stream_info : pkg_.psm->stream_map) {
                        if (stream_map_.find(stream_info.elementary_stream_id) == stream_map_.end()) {
                            auto stream = std::make_unique<PSStream>(
                                stream_info.elementary_stream_id,
                                static_cast<PSStreamType>(stream_info.stream_type)
                            );
                            stream_map_[stream->sid] = std::move(stream);
                        }
                    }
                }
                if (on_packet_) {
                    on_packet_(ret);
                }
                break;
            }
            
            case 0x000001B9: // MPEG_program_end_code
                bs.SkipBits(32);
                continue;
                
            default: {
                // Check for PES packet
                if ((prefix_code & 0xFFFFFFE0) == 0x000001C0 || 
                    (prefix_code & 0xFFFFFFE0) == 0x000001E0) {
                    
                    if (!pkg_.pes) {
                        pkg_.pes = std::make_unique<PesPacket>();
                    }
                    
                    if (mpeg1_) {
                        ret = pkg_.pes->DecodeMpeg1(bs);
                    } else {
                        ret = pkg_.pes->Decode(bs);
                    }
                    
                    if (on_packet_) {
                        on_packet_(ret);
                    }
                    
                    if (ret == 0) {
                        auto it = stream_map_.find(pkg_.pes->stream_id);
                        if (it != stream_map_.end()) {
                            if (mpeg1_ && it->second->cid == PSStreamType::UNKNOWN) {
                                GuessCodecId(it->second.get());
                            }
                            DemuxPesPacket(it->second.get(), pkg_.pes.get());
                        } else if (mpeg1_) {
                            // Create unknown stream for MPEG-1
                            auto stream = std::make_unique<PSStream>(
                                pkg_.pes->stream_id,
                                PSStreamType::UNKNOWN
                            );
                            stream->stream_buf.insert(stream->stream_buf.end(),
                                                     pkg_.pes->pes_payload.begin(),
                                                     pkg_.pes->pes_payload.end());
                            stream->pts = pkg_.pes->pts;
                            stream->dts = pkg_.pes->dts;
                            stream_map_[stream->sid] = std::move(stream);
                        }
                    }
                } else {
                    bs.SkipBits(8);
                }
                break;
            }
        }
    }
    
    if (ret == 0 && !cache_.empty()) {
        cache_.clear();
    }
    
    return ret;
}

void PSDemuxer::Flush() {
    for (auto& pair : stream_map_) {
        auto& stream = pair.second;
        if (!stream->stream_buf.empty() && on_frame_) {
            on_frame_(stream->stream_buf, stream->cid, stream->pts / 90, stream->dts / 90);
        }
    }
}

void PSDemuxer::DemuxPesPacket(PSStream* stream, const PesPacket* pes) {
    switch (stream->cid) {
        case PSStreamType::AAC:
        case PSStreamType::G711A:
        case PSStreamType::G711U:
            DemuxAudio(stream, pes);
            break;
        case PSStreamType::H264:
        case PSStreamType::H265:
            DemuxH26x(stream, pes);
            break;
        case PSStreamType::UNKNOWN:
            if (stream->pts != pes->pts) {
                stream->stream_buf.clear();
            }
            stream->stream_buf.insert(stream->stream_buf.end(),
                                     pes->pes_payload.begin(),
                                     pes->pes_payload.end());
            stream->pts = pes->pts;
            stream->dts = pes->dts;
            break;
    }
}

void PSDemuxer::DemuxAudio(PSStream* stream, const PesPacket* pes) {
    if (stream->pts != pes->pts && !stream->stream_buf.empty()) {
        if (on_frame_) {
            on_frame_(stream->stream_buf, stream->cid, stream->pts / 90, stream->dts / 90);
        }
        stream->stream_buf.clear();
    }
    stream->stream_buf.insert(stream->stream_buf.end(),
                             pes->pes_payload.begin(),
                             pes->pes_payload.end());
    stream->pts = pes->pts;
    stream->dts = pes->dts;
}

void PSDemuxer::DemuxH26x(PSStream* stream, const PesPacket* pes) {
    if (stream->stream_buf.empty()) {
        stream->pts = pes->pts;
        stream->dts = pes->dts;
    }
    
    stream->stream_buf.insert(stream->stream_buf.end(),
                             pes->pes_payload.begin(),
                             pes->pes_payload.end());
    
    // Simplified frame splitting - real implementation would need full NALU parsing
    // For now, we'll emit frames based on PTS changes
    if (on_frame_ && stream->pts != pes->pts && stream->stream_buf.size() > 1000) {
        on_frame_(stream->stream_buf, stream->cid, stream->pts / 90, stream->dts / 90);
        stream->stream_buf.clear();
        stream->pts = pes->pts;
        stream->dts = pes->dts;
    }
}

void PSDemuxer::GuessCodecId(PSStream* stream) {
    // Simplified codec detection
    if ((stream->sid & 0xE0) == static_cast<uint8_t>(PESStreamID::STREAM_AUDIO)) {
        stream->cid = PSStreamType::AAC;
    } else if ((stream->sid & 0xE0) == static_cast<uint8_t>(PESStreamID::STREAM_VIDEO)) {
        // Default to H264 for video
        stream->cid = PSStreamType::H264;
    }
}

} // namespace mpeg2
