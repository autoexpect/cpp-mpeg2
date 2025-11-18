#include "ts_muxer.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>

// H.264 NALU 类型定义
enum H264NaluType
{
    H264_NAL_RESERVED = 0,
    H264_NAL_P_SLICE = 1,
    H264_NAL_SLICE_A = 2,
    H264_NAL_SLICE_B = 3,
    H264_NAL_SLICE_C = 4,
    H264_NAL_I_SLICE = 5,
    H264_NAL_SEI = 6,
    H264_NAL_SPS = 7,
    H264_NAL_PPS = 8,
    H264_NAL_AUD = 9
};

// 查找起始码 (0x00 0x00 0x01 或 0x00 0x00 0x00 0x01)
int FindStartCode(const uint8_t *data, size_t size, size_t offset, int &start_code_len)
{
    if (offset + 3 > size)
    {
        return -1;
    }

    for (size_t i = offset; i <= size - 3; i++)
    {
        if (data[i] == 0x00 && data[i + 1] == 0x00)
        {
            if (data[i + 2] == 0x01)
            {
                start_code_len = 3;
                return i;
            }
            else if (i + 3 < size && data[i + 2] == 0x00 && data[i + 3] == 0x01)
            {
                start_code_len = 4;
                return i;
            }
        }
    }
    return -1;
}

// 获取 NALU 类型（不包含起始码）
H264NaluType GetH264NaluType(const uint8_t *nalu)
{
    return static_cast<H264NaluType>(nalu[0] & 0x1F);
}

// 分割 H.264 帧数据（包含起始码）
void SplitH264FrameWithStartCode(const std::vector<uint8_t> &data,
                                 std::function<bool(const std::vector<uint8_t> &)> callback)
{
    size_t offset = 0;
    int start_code_len = 0;

    int beg = FindStartCode(data.data(), data.size(), offset, start_code_len);

    while (beg >= 0)
    {
        offset = beg + start_code_len;
        int sc_len = 0;
        int end = FindStartCode(data.data(), data.size(), offset, sc_len);

        if (end == -1)
        {
            // 最后一个 NALU
            if (offset < data.size())
            {
                std::vector<uint8_t> nalu(data.begin() + beg, data.end());
                callback(nalu);
            }
            break;
        }

        // 提取带起始码的 NALU
        if (offset < end)
        {
            std::vector<uint8_t> nalu(data.begin() + beg, data.begin() + end);
            if (!callback(nalu))
            {
                break;
            }
        }

        beg = end;
        start_code_len = sc_len;
    }
}

// 检查是否为 VCL NALU (视频编码层)
bool IsH264VCLNalu(H264NaluType nalu_type)
{
    return nalu_type >= H264_NAL_RESERVED && nalu_type <= H264_NAL_I_SLICE;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input.h264> <output.ts>" << std::endl;
        std::cerr << "  input.h264  - H.264 elementary stream with start codes (0x00 0x00 0x01)" << std::endl;
        std::cerr << "  output.ts   - Output MPEG-TS file" << std::endl;
        return 1;
    }

    // 打开输入文件
    std::ifstream infile(argv[1], std::ios::binary);
    if (!infile)
    {
        std::cerr << "Failed to open input file: " << argv[1] << std::endl;
        return 1;
    }

    // 读取整个 H.264 文件
    infile.seekg(0, std::ios::end);
    size_t file_size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    std::vector<uint8_t> h264_data(file_size);
    infile.read(reinterpret_cast<char *>(h264_data.data()), file_size);
    infile.close();

    std::cout << "Read " << file_size << " bytes from " << argv[1] << std::endl;

    // 打开输出文件
    std::ofstream outfile(argv[2], std::ios::binary);
    if (!outfile)
    {
        std::cerr << "Failed to open output file: " << argv[2] << std::endl;
        return 1;
    }

    // 创建 TS 封装器
    mpeg2::TSMuxer muxer;

    // 添加 H.264 视频流
    uint16_t video_pid = muxer.AddStream(mpeg2::TSStreamType::H264);
    std::cout << "Added H.264 video stream with PID: 0x" << std::hex << video_pid << std::dec << std::endl;

    // 设置包回调 - 写入文件
    muxer.SetOnPacket([&outfile](const std::vector<uint8_t> &packet)
                      { outfile.write(reinterpret_cast<const char *>(packet.data()), packet.size()); });

    // 处理 H.264 数据
    uint64_t pts = 0;
    uint64_t dts = 0;
    int frame_count = 0;
    int nalu_count = 0;

    // 缓存 SPS 和 PPS
    std::vector<uint8_t> cached_sps;
    std::vector<uint8_t> cached_pps;
    std::vector<uint8_t> pending_nalus; // 存储待发送的 NALU（包含 SPS/PPS + I帧）

    // 分割并写入每个 NALU
    SplitH264FrameWithStartCode(h264_data, [&](const std::vector<uint8_t> &nalu) -> bool
                                {
        nalu_count++;
        
        // 找到起始码长度
        int sc_len = 0;
        if (nalu.size() >= 4 && nalu[0] == 0x00 && nalu[1] == 0x00 && 
            nalu[2] == 0x00 && nalu[3] == 0x01) {
            sc_len = 4;
        } else if (nalu.size() >= 3 && nalu[0] == 0x00 && nalu[1] == 0x00 && 
                   nalu[2] == 0x01) {
            sc_len = 3;
        }
        
        // 获取 NALU 类型
        H264NaluType nalu_type = GetH264NaluType(nalu.data() + sc_len);
        
        // 缓存 SPS
        if (nalu_type == H264_NAL_SPS) {
            cached_sps = nalu;
            std::cout << "Cached SPS, size " << nalu.size() << " bytes" << std::endl;
            return true;
        }
        
        // 缓存 PPS
        if (nalu_type == H264_NAL_PPS) {
            cached_pps = nalu;
            std::cout << "Cached PPS, size " << nalu.size() << " bytes" << std::endl;
            return true;
        }
        
        // 如果是 VCL NALU（编码的图像数据），更新时间戳
        if (IsH264VCLNalu(nalu_type)) {
            pts += 40;  // 假设 25fps，每帧 40ms
            dts += 40;
            frame_count++;
            
            // 准备要发送的数据
            pending_nalus.clear();
            
            // 如果是 I 帧，前面加上 SPS 和 PPS
            if (nalu_type == H264_NAL_I_SLICE) {
                if (!cached_sps.empty()) {
                    pending_nalus.insert(pending_nalus.end(), cached_sps.begin(), cached_sps.end());
                    std::cout << "Prepending SPS to I-frame" << std::endl;
                }
                if (!cached_pps.empty()) {
                    pending_nalus.insert(pending_nalus.end(), cached_pps.begin(), cached_pps.end());
                    std::cout << "Prepending PPS to I-frame" << std::endl;
                }
            }
            
            // 添加当前帧数据
            pending_nalus.insert(pending_nalus.end(), nalu.begin(), nalu.end());
            
            std::cout << "Frame " << frame_count << ": NALU type " << static_cast<int>(nalu_type) 
                      << ", total size " << pending_nalus.size() << " bytes, PTS/DTS " << pts << "ms" << std::endl;
            
            // 打印前8个字节的16进制
            std::cout << "First 8 bytes: ";
            for (size_t i = 0; i < std::min(size_t(8), pending_nalus.size()); i++) {
                printf("%02X ", pending_nalus[i]);
            }
            std::cout << std::endl;
            
            // 写入合并后的数据到 TS 流
            int ret = muxer.Write(video_pid, pending_nalus, pts, dts);
            if (ret != 0) {
                std::cerr << "Failed to write NALU: " << ret << std::endl;
                return false;
            }
        } else {
            // 非 VCL NALU（如 SEI、AUD 等），直接发送
            std::cout << "Non-VCL NALU type " << static_cast<int>(nalu_type) << ", first 8 bytes: ";
            for (size_t i = 0; i < std::min(size_t(8), nalu.size()); i++) {
                printf("%02X ", nalu[i]);
            }
            std::cout << std::endl;
            
            int ret = muxer.Write(video_pid, nalu, pts, dts);
            if (ret != 0) {
                std::cerr << "Failed to write NALU: " << ret << std::endl;
                return false;
            }
        }
        
        return true; });

    std::cout << "\nTS muxing completed successfully" << std::endl;
    std::cout << "Total NALUs processed: " << nalu_count << std::endl;
    std::cout << "Total frames: " << frame_count << std::endl;
    std::cout << "Output file: " << argv[2] << std::endl;

    return 0;
}
