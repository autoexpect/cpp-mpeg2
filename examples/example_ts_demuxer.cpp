#include "ts_demuxer.h"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.ts>" << std::endl;
        return 1;
    }

    // 打开 TS 文件
    std::ifstream infile(argv[1], std::ios::binary);
    if (!infile) {
        std::cerr << "Failed to open input file: " << argv[1] << std::endl;
        return 1;
    }

    // 创建 TS 解复用器
    mpeg2::TSDemuxer demuxer;
    
    // 设置帧回调
    size_t frame_count = 0;
    demuxer.SetOnFrame([&frame_count](mpeg2::TSStreamType cid,
                                        const std::vector<uint8_t>& frame,
                                        uint64_t pts, uint64_t dts) {
        frame_count++;
        std::cout << "Frame #" << frame_count << ": ";
        
        switch (cid) {
            case mpeg2::TSStreamType::H264:
                std::cout << "H.264";
                break;
            case mpeg2::TSStreamType::H265:
                std::cout << "H.265";
                break;
            case mpeg2::TSStreamType::AAC:
                std::cout << "AAC";
                break;
            case mpeg2::TSStreamType::AUDIO_MPEG1:
                std::cout << "MPEG1 Audio";
                break;
            case mpeg2::TSStreamType::AUDIO_MPEG2:
                std::cout << "MPEG2 Audio";
                break;
        }
        
        std::cout << ", size=" << frame.size() 
                  << ", pts=" << pts 
                  << ", dts=" << dts << std::endl;
    });

    // 读取并处理数据
    std::vector<uint8_t> buffer(mpeg2::TS_PACKET_SIZE * 10); // 一次读取10个TS包
    while (infile.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || infile.gcount() > 0) {
        size_t bytes_read = infile.gcount();
        int ret = demuxer.Input(buffer.data(), bytes_read);
        if (ret != 0) {
            std::cerr << "Demuxer error: " << ret << std::endl;
            break;
        }
    }
    
    std::cout << "Total frames: " << frame_count << std::endl;
    return 0;
}
