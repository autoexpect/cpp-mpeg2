#include "ts_demuxer.h"
#include <cstring>

namespace mpeg2 {

TSDemuxer::TSDemuxer() = default;

TSDemuxer::~TSDemuxer() = default;

int TSDemuxer::Input(const uint8_t* data, size_t size) {
    size_t offset = 0;
    
    while (offset + TS_PACKET_SIZE <= size) {
        // Probe for sync byte
        if (data[offset] != 0x47) {
            // Try to find sync
            bool found = false;
            for (size_t i = offset; i < size - TS_PACKET_SIZE; i++) {
                if (data[i] == 0x47 && data[i + TS_PACKET_SIZE] == 0x47) {
                    offset = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                return static_cast<int>(Mpeg2Error::PARSE_ERROR);
            }
        }
        
        BitStream bs(data + offset, TS_PACKET_SIZE);
        TSPacket pkg;
        
        int ret = pkg.DecodeHeader(bs);
        if (ret != 0) {
            offset += TS_PACKET_SIZE;
            continue;
        }
        
        // Handle PAT
        if (pkg.pid == static_cast<uint16_t>(TSPID::PAT)) {
            if (pkg.payload_unit_start_indicator == 1) {
                bs.SkipBits(8); // pointer field
            }
            
            // Skip 0xFF padding
            while (bs.NextBits(8) == 0xFF && bs.RemainBytes() > 0) {
                bs.SkipBits(8);
            }
            
            PAT pat;
            ret = pat.Decode(bs);
            if (ret == 0) {
                for (const auto& pmt_pair : pat.pmts) {
                    if (pmt_pair.program_number != 0x0000) {
                        if (programs_.find(pmt_pair.pid) == programs_.end()) {
                            auto program = std::make_unique<TSProgram>();
                            program->pn = 0;
                            programs_[pmt_pair.pid] = std::move(program);
                        }
                    }
                }
            }
        }
        // Handle PMT or streams
        else if (pkg.pid != static_cast<uint16_t>(TSPID::NIL)) {
            bool is_pmt = false;
            TSProgram* program = nullptr;
            
            // Check if it's a PMT
            for (auto& pair : programs_) {
                if (pair.first == pkg.pid) {
                    is_pmt = true;
                    program = pair.second.get();
                    break;
                }
            }
            
            if (is_pmt) {
                // Parse PMT
                if (pkg.payload_unit_start_indicator == 1) {
                    bs.SkipBits(8); // pointer field
                }
                
                PMT pmt;
                ret = pmt.Decode(bs);
                if (ret == 0) {
                    program->pn = pmt.program_number;
                    for (const auto& ps : pmt.streams) {
                        if (program->streams.find(ps.elementary_pid) == program->streams.end()) {
                            auto stream = std::make_unique<TSStream>();
                            stream->cid = static_cast<TSStreamType>(ps.stream_type);
                            stream->pes_sid = PESStreamID::STREAM_VIDEO; // Simplified
                            stream->pes_pkg = std::make_unique<PesPacket>();
                            stream->pts = 0;
                            stream->dts = 0;
                            program->streams[ps.elementary_pid] = std::move(stream);
                        }
                    }
                }
            } else {
                // Check if it's a stream we're tracking
                for (auto& prog_pair : programs_) {
                    auto it = prog_pair.second->streams.find(pkg.pid);
                    if (it != prog_pair.second->streams.end()) {
                        auto& stream = it->second;
                        
                        if (pkg.payload_unit_start_indicator == 1) {
                            // New PES packet
                            ret = stream->pes_pkg->Decode(bs);
                            
                            if (ret == 0 || ret == static_cast<int>(Mpeg2Error::NEED_MORE)) {
                                // Process previous packet
                                if (stream->cid == TSStreamType::AAC ||
                                    stream->cid == TSStreamType::AUDIO_MPEG1 ||
                                    stream->cid == TSStreamType::AUDIO_MPEG2) {
                                    DoAudioPesPacket(stream.get(), pkg.payload_unit_start_indicator);
                                } else if (stream->cid == TSStreamType::H264 ||
                                          stream->cid == TSStreamType::H265) {
                                    DoVideoPesPacket(stream.get(), pkg.payload_unit_start_indicator);
                                }
                            }
                        } else {
                            // Continuation of PES packet
                            const uint8_t* payload_data = bs.RemainData();
                            if (payload_data && bs.RemainBytes() > 0) {
                                stream->pes_pkg->pes_payload.insert(
                                    stream->pes_pkg->pes_payload.end(),
                                    payload_data,
                                    payload_data + bs.RemainBytes()
                                );
                            }
                            
                            if (stream->cid == TSStreamType::H264 ||
                                stream->cid == TSStreamType::H265) {
                                DoVideoPesPacket(stream.get(), pkg.payload_unit_start_indicator);
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        if (on_ts_packet_) {
            on_ts_packet_(pkg);
        }
        
        offset += TS_PACKET_SIZE;
    }
    
    return static_cast<int>(Mpeg2Error::SUCCESS);
}

void TSDemuxer::DoAudioPesPacket(TSStream* stream, uint8_t start_indicator) {
    if (!stream || !stream->pes_pkg) {
        return;
    }
    
    if (!stream->payload.empty() && 
        (start_indicator == 1 || stream->pes_pkg->pts != stream->pts)) {
        if (on_frame_) {
            on_frame_(stream->cid, stream->payload, stream->pts / 90, stream->dts / 90);
        }
        stream->payload.clear();
    }
    
    stream->payload.insert(stream->payload.end(),
                          stream->pes_pkg->pes_payload.begin(),
                          stream->pes_pkg->pes_payload.end());
    stream->pts = stream->pes_pkg->pts;
    stream->dts = stream->pes_pkg->dts;
}

void TSDemuxer::DoVideoPesPacket(TSStream* stream, uint8_t start_indicator) {
    if (!stream || !stream->pes_pkg) {
        return;
    }
    
    if (stream->payload.empty()) {
        stream->pts = stream->pes_pkg->pts;
        stream->dts = stream->pes_pkg->dts;
    }
    
    stream->payload.insert(stream->payload.end(),
                          stream->pes_pkg->pes_payload.begin(),
                          stream->pes_pkg->pes_payload.end());
    
    // Simplified frame detection - emit when we have enough data
    // Real implementation would need full NALU parsing
    if (stream->payload.size() > 100000) {
        if (on_frame_) {
            on_frame_(stream->cid, stream->payload, stream->pts / 90, stream->dts / 90);
        }
        stream->payload.clear();
    }
}

int TSDemuxer::Probe(const uint8_t* data, size_t size, size_t& offset) {
    // Find sync byte
    for (size_t i = 0; i < size - TS_PACKET_SIZE; i++) {
        if (data[i] == 0x47 && data[i + TS_PACKET_SIZE] == 0x47) {
            offset = i;
            return static_cast<int>(Mpeg2Error::SUCCESS);
        }
    }
    
    return static_cast<int>(Mpeg2Error::PARSE_ERROR);
}

} // namespace mpeg2
