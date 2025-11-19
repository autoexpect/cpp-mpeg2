#include "mpeg2/ps_muxer.h"
#include "mpeg2/h264_utils.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

using namespace mpeg2;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_h264_file> <output_ps_file>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open output file" << std::endl;
        return 1;
    }

    PSMuxer muxer;
    uint8_t stream_id = muxer.AddStream(PS_STREAM_H264);
    
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
        int type = H264Utils::GetNaluType(nalu, len);
        bool is_vcl = H264Utils::IsH264VCL(nalu, len);
        bool is_aud = H264Utils::IsH264AUD(nalu, len);
        bool is_sps = (type == H264_NAL_SPS);
        bool is_pps = (type == H264_NAL_PPS);

        bool new_au = false;
        if (is_aud) new_au = true;
        if (is_sps || is_pps) {
            if (has_vcl) new_au = true;
        }
        if (is_vcl) {
            if (has_vcl) {
                new_au = true;
            }
        }

        if (new_au && !au_buffer.empty()) {
            muxer.Write(stream_id, au_buffer.data(), au_buffer.size(), pts, dts);
            pts += 3600;
            dts += 3600;
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
        muxer.Write(stream_id, au_buffer.data(), au_buffer.size(), pts, dts);
    }

    return 0;
}
