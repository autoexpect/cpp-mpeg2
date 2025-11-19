#include "mpeg2/ts_muxer.h"
#include "mpeg2/codec_utils.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

using namespace mpeg2;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_ts_file> [interval_ms]" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    bool is_h265 = false;
    if (input_file.size() >= 5 && input_file.substr(input_file.size() - 5) == ".h265") {
        is_h265 = true;
    }

    int interval = 40; // Default 40ms (25fps)
    if (argc >= 4) {
        interval = std::stoi(argv[3]);
    }

    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open output file" << std::endl;
        return 1;
    }

    TSMuxer muxer;
    uint16_t pid = muxer.AddStream(is_h265 ? TS_STREAM_H265 : TS_STREAM_H264);
    
    muxer.SetOnPacket([&](const std::vector<uint8_t>& packet) {
        out.write((const char*)packet.data(), packet.size());
    });

    // Read file content
    std::ifstream in(input_file, std::ios::binary | std::ios::ate);
    if (!in) {
        std::cerr << "Failed to open input file" << std::endl;
        return 1;
    }
    size_t size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    in.read((char*)buffer.data(), size);

    uint64_t pts = 0;
    uint64_t dts = 0;
    
    std::vector<uint8_t> au_buffer;
    bool has_vcl = false;

    CodecUtils::SplitFrame(buffer.data(), size, [&](const uint8_t* nalu, size_t len) {
        bool new_au = false;
        bool is_vcl = false;
        bool is_aud = false;
        bool is_sps_pps = false;
        bool is_idr = false;

        // Cache for parameter sets
        static std::vector<uint8_t> last_vps;
        static std::vector<uint8_t> last_sps;
        static std::vector<uint8_t> last_pps;
        
        // Flags for current AU
        static bool au_has_vps = false;
        static bool au_has_sps = false;
        static bool au_has_pps = false;

        if (is_h265) {
            int type = CodecUtils::GetH265NaluType(nalu, len);
            is_vcl = CodecUtils::IsH265VCL(nalu, len);
            is_aud = CodecUtils::IsH265AUD(nalu, len);
            is_idr = CodecUtils::IsH265IDR(nalu, len);

            if (type == H265_NAL_VPS) {
                last_vps.assign(nalu, nalu + len);
                au_has_vps = true;
                is_sps_pps = true;
            } else if (type == H265_NAL_SPS) {
                last_sps.assign(nalu, nalu + len);
                au_has_sps = true;
                is_sps_pps = true;
            } else if (type == H265_NAL_PPS) {
                last_pps.assign(nalu, nalu + len);
                au_has_pps = true;
                is_sps_pps = true;
            }
            
            if (is_aud) new_au = true;
            if (is_sps_pps) {
                if (has_vcl) new_au = true;
            }
            if (is_vcl) {
                if (has_vcl) {
                    // Check if it is the first slice of a new picture
                    if (CodecUtils::IsH265FirstSlice(nalu, len)) {
                        new_au = true;
                    }
                }
            }
        } else {
            int type = CodecUtils::GetNaluType(nalu, len);
            is_vcl = CodecUtils::IsH264VCL(nalu, len);
            is_aud = CodecUtils::IsH264AUD(nalu, len);
            is_idr = CodecUtils::IsH264IDR(nalu, len);

            if (type == H264_NAL_SPS) {
                last_sps.assign(nalu, nalu + len);
                au_has_sps = true;
                is_sps_pps = true;
            } else if (type == H264_NAL_PPS) {
                last_pps.assign(nalu, nalu + len);
                au_has_pps = true;
                is_sps_pps = true;
            }

            if (is_aud) new_au = true;
            if (is_sps_pps) {
                if (has_vcl) new_au = true;
            }
            if (is_vcl) {
                if (has_vcl) {
                    // Check if it is the first slice of a new picture
                    if (CodecUtils::IsH264FirstSlice(nalu, len)) {
                        new_au = true;
                    }
                }
            }
        }

        if (new_au && !au_buffer.empty()) {
            muxer.Write(pid, au_buffer.data(), au_buffer.size(), pts, dts);
            pts += interval;
            dts += interval;
            au_buffer.clear();
            has_vcl = false;
            
            // Reset AU flags for new AU
            au_has_vps = false;
            au_has_sps = false;
            au_has_pps = false;
        }

        // If this is an IDR frame, check if we need to insert parameter sets
        if (is_idr) {
            if (is_h265) {
                if (!au_has_vps && !last_vps.empty()) {
                    au_buffer.push_back(0x00); au_buffer.push_back(0x00); au_buffer.push_back(0x00); au_buffer.push_back(0x01);
                    au_buffer.insert(au_buffer.end(), last_vps.begin(), last_vps.end());
                    au_has_vps = true;
                }
            }
            if (!au_has_sps && !last_sps.empty()) {
                au_buffer.push_back(0x00); au_buffer.push_back(0x00); au_buffer.push_back(0x00); au_buffer.push_back(0x01);
                au_buffer.insert(au_buffer.end(), last_sps.begin(), last_sps.end());
                au_has_sps = true;
            }
            if (!au_has_pps && !last_pps.empty()) {
                au_buffer.push_back(0x00); au_buffer.push_back(0x00); au_buffer.push_back(0x00); au_buffer.push_back(0x01);
                au_buffer.insert(au_buffer.end(), last_pps.begin(), last_pps.end());
                au_has_pps = true;
            }
        }

        au_buffer.push_back(0x00);
        au_buffer.push_back(0x00);
        au_buffer.push_back(0x00);
        au_buffer.push_back(0x01);
        au_buffer.insert(au_buffer.end(), nalu, nalu + len);
        if (is_vcl) has_vcl = true;

        return true;
    });

    if (!au_buffer.empty()) {
        muxer.Write(pid, au_buffer.data(), au_buffer.size(), pts, dts);
    }

    return 0;
}
