#include "mpeg2/ts_muxer.h"
#include "mpeg2/h264_utils.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

using namespace mpeg2;

// Simple NALU reader
class NaluReader {
public:
    NaluReader(const std::string& filename) : file_(filename, std::ios::binary), buffer_(1024 * 1024) {
        if (file_) {
            file_.read((char*)buffer_.data(), buffer_.size());
            data_size_ = file_.gcount();
            ptr_ = buffer_.data();
        } else {
            data_size_ = 0;
            ptr_ = nullptr;
        }
    }

    bool GetNextNalu(const uint8_t*& nalu, size_t& len) {
        if (!ptr_ || ptr_ >= buffer_.data() + data_size_) return false;

        const uint8_t* start = ptr_;
        int start_code_len = 0;
        
        // Find start code
        if (ptr_ + 4 <= buffer_.data() + data_size_ && ptr_[0] == 0 && ptr_[1] == 0 && ptr_[2] == 0 && ptr_[3] == 1) {
            start_code_len = 4;
        } else if (ptr_ + 3 <= buffer_.data() + data_size_ && ptr_[0] == 0 && ptr_[1] == 0 && ptr_[2] == 1) {
            start_code_len = 3;
        } else {
            // Should not happen if we are aligned, but if we are not, search for it?
            // For simplicity assume aligned or search.
            // Let's search.
            const uint8_t* p = ptr_;
            while (p < buffer_.data() + data_size_ - 3) {
                if (p[0] == 0 && p[1] == 0 && p[2] == 1) {
                    start = p;
                    start_code_len = 3;
                    if (p > buffer_.data() && p[-1] == 0) {
                        start--;
                        start_code_len = 4;
                    }
                    break;
                }
                p++;
            }
            if (p >= buffer_.data() + data_size_ - 3) return false;
        }

        ptr_ = start + start_code_len;
        
        // Find next start code
        const uint8_t* next_start = nullptr;
        const uint8_t* p = ptr_;
        while (p < buffer_.data() + data_size_ - 3) {
            if (p[0] == 0 && p[1] == 0 && p[2] == 1) {
                next_start = p;
                if (p > buffer_.data() && p[-1] == 0) {
                    next_start--;
                }
                break;
            }
            p++;
        }

        if (next_start) {
            len = next_start - start;
            ptr_ = next_start;
        } else {
            len = (buffer_.data() + data_size_) - start;
            ptr_ = buffer_.data() + data_size_;
        }
        nalu = start;
        return true;
    }

private:
    std::ifstream file_;
    std::vector<uint8_t> buffer_; // Simple fixed buffer for now
    size_t data_size_;
    const uint8_t* ptr_;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_h264_file> <output_ts_file>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open output file" << std::endl;
        return 1;
    }

    TSMuxer muxer;
    uint16_t pid = muxer.AddStream(TS_STREAM_H264);
    
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
    
    // Split into frames (AUs)
    // Simple heuristic: New AU starts with SPS, PPS, IDR, or Non-IDR (if previous was complete).
    // For simplicity, treat each NALU as a frame? No, that's bad for TS (one PES per NALU is overhead).
    // Better: Group NALUs.
    // AU Boundary: AUD, SPS, PPS, SEI, IDR, SLICE.
    // Usually: [AUD] [SPS] [PPS] [SEI] [IDR/SLICE]
    // We can accumulate NALUs until we see a new AUD or VCL NALU (if we already have one).
    
    std::vector<uint8_t> au_buffer;
    bool has_vcl = false;

    H264Utils::SplitFrame(buffer.data(), size, [&](const uint8_t* nalu, size_t len) {
        int type = H264Utils::GetNaluType(nalu, len);
        bool is_vcl = H264Utils::IsH264VCL(nalu, len);
        bool is_aud = H264Utils::IsH264AUD(nalu, len);
        bool is_sps = (type == H264_NAL_SPS);
        bool is_pps = (type == H264_NAL_PPS);

        // Check if this NALU starts a new AU
        bool new_au = false;
        if (is_aud) new_au = true;
        if (is_sps || is_pps) {
            if (has_vcl) new_au = true;
        }
        if (is_vcl) {
            if (has_vcl) {
                // If we already have a VCL, this might be a second slice of same frame or new frame.
                // For simplicity assume 1 slice per frame for now, or check first_mb_in_slice?
                // Let's assume new frame if we see VCL and we already have VCL.
                new_au = true;
            }
        }

        if (new_au && !au_buffer.empty()) {
            muxer.Write(pid, au_buffer.data(), au_buffer.size(), pts, dts);
            pts += 3600; // 25 fps (90000 / 25 = 3600)
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

    // Write last AU
    if (!au_buffer.empty()) {
        muxer.Write(pid, au_buffer.data(), au_buffer.size(), pts, dts);
    }

    return 0;
}
