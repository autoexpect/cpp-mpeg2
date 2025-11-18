#include "ps_demuxer.h"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.ps>" << std::endl;
        return 1;
    }

    // 打开 PS 文件
    std::ifstream infile(argv[1], std::ios::binary);
    if (!infile) {
        std::cerr << "Failed to open input file: " << argv[1] << std::endl;
        return 1;
    }

    // 创建 PS 解复用器
    mpeg2::PSDemuxer demuxer;
    
    // 设置帧回调
    size_t frame_count = 0;
    demuxer.SetOnFrame([&frame_count](const std::vector<uint8_t>& frame, 
                                        mpeg2::PSStreamType cid,
                                        uint64_t pts, uint64_t dts) {
        frame_count++;
        std::cout << "Frame #" << frame_count << ": ";
        
        switch (cid) {
            case mpeg2::PSStreamType::H264:
                std::cout << "H.264";
                break;
            case mpeg2::PSStreamType::H265:
                std::cout << "H.265";
                break;
            case mpeg2::PSStreamType::AAC:
                std::cout << "AAC";
                break;
            case mpeg2::PSStreamType::G711A:
                std::cout << "G.711A";
                break;
            case mpeg2::PSStreamType::G711U:
                std::cout << "G.711U";
                break;
            default:
                std::cout << "Unknown";
        }
        
        std::cout << ", size=" << frame.size() 
                  << ", pts=" << pts 
                  << ", dts=" << dts << std::endl;
    });

    // 读取并处理数据
    std::vector<uint8_t> buffer(4096);
    while (infile.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || infile.gcount() > 0) {
        size_t bytes_read = infile.gcount();
        int ret = demuxer.Input(buffer.data(), bytes_read);
        if (ret != 0) {
            std::cerr << "Demuxer error: " << ret << std::endl;
            break;
        }
    }

    // 刷新剩余数据
    demuxer.Flush();
    
    std::cout << "Total frames: " << frame_count << std::endl;
    return 0;
}
