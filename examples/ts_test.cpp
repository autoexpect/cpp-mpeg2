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

    H264Utils::SplitFrame(buffer.data(), size, [&](const uint8_t* nalu, size_t len) {
        bool new_au = false;
        bool is_vcl = false;
        bool is_aud = false;
        bool is_sps_pps = false;

        if (is_h265) {
            int type = H264Utils::GetH265NaluType(nalu, len);
            is_vcl = H264Utils::IsH265VCL(nalu, len);
            is_aud = H264Utils::IsH265AUD(nalu, len);
            if (type == H265_NAL_VPS || type == H265_NAL_SPS || type == H265_NAL_PPS) {
                is_sps_pps = true;
            }
            
            if (is_aud) new_au = true;
            if (is_sps_pps) {
                if (has_vcl) new_au = true;
            }
            if (is_vcl) {
                if (has_vcl) {
                    // Check if it is the first slice of a new picture
                    if (H264Utils::IsH265FirstSlice(nalu, len)) {
                        new_au = true;
                    }
                }
            }
        } else {
            int type = H264Utils::GetNaluType(nalu, len);
            is_vcl = H264Utils::IsH264VCL(nalu, len);
            is_aud = H264Utils::IsH264AUD(nalu, len);
            if (type == H264_NAL_SPS || type == H264_NAL_PPS) {
                is_sps_pps = true;
            }

            if (is_aud) new_au = true;
            if (is_sps_pps) {
                if (has_vcl) new_au = true;
            }
            if (is_vcl) {
                if (has_vcl) {
                    // Check if it is the first slice of a new picture
                    if (H264Utils::IsH264FirstSlice(nalu, len)) {
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
