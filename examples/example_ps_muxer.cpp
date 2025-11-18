#include "ps_muxer.h"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <output.ps>" << std::endl;
        return 1;
    }

    // 打开输出文件
    std::ofstream outfile(argv[1], std::ios::binary);
    if (!outfile) {
        std::cerr << "Failed to open output file: " << argv[1] << std::endl;
        return 1;
    }

    // 创建 PS 封装器
    mpeg2::PSMuxer muxer;
    
    // 添加 H.264 视频流
    uint8_t video_sid = muxer.AddStream(mpeg2::PSStreamType::H264);
    std::cout << "Added video stream with SID: " << static_cast<int>(video_sid) << std::endl;
    
    // 添加 AAC 音频流
    uint8_t audio_sid = muxer.AddStream(mpeg2::PSStreamType::AAC);
    std::cout << "Added audio stream with SID: " << static_cast<int>(audio_sid) << std::endl;
    
    // 设置包回调 - 写入文件
    muxer.SetOnPacket([&outfile](const std::vector<uint8_t>& packet) {
        outfile.write(reinterpret_cast<const char*>(packet.data()), packet.size());
    });

    // 模拟写入一些帧数据
    // 实际应用中，这里应该是真实的视频/音频帧数据
    std::vector<uint8_t> video_frame(1024, 0x00); // 模拟视频帧
    std::vector<uint8_t> audio_frame(256, 0x00);  // 模拟音频帧
    
    for (int i = 0; i < 10; i++) {
        uint64_t timestamp = i * 40; // 40ms间隔
        
        // 写入视频帧
        int ret = muxer.Write(video_sid, video_frame, timestamp, timestamp);
        if (ret != 0) {
            std::cerr << "Failed to write video frame: " << ret << std::endl;
            return 1;
        }
        
        // 写入音频帧
        ret = muxer.Write(audio_sid, audio_frame, timestamp, timestamp);
        if (ret != 0) {
            std::cerr << "Failed to write audio frame: " << ret << std::endl;
            return 1;
        }
        
        std::cout << "Written frame " << i << " at timestamp " << timestamp << "ms" << std::endl;
    }

    std::cout << "PS muxing completed successfully" << std::endl;
    return 0;
}
