# MPEG-2 C++ Library

这是 go-mpeg2 的 C++ 重写版本，提供了 MPEG-2 PS/TS 流的封装(mux)和解封装(demux)功能。

## ✅ 实现状态

**完成度: 100%**

所有核心功能已从 Go 版本完整移植，项目可编译运行。详见 [STATUS.md](STATUS.md)

## 功能特性

- ✅ MPEG-2 PS (Program Stream) 封装和解封装
- ✅ MPEG-2 TS (Transport Stream) 封装和解封装
- ✅ 支持 H.264/H.265 视频编码
- ✅ 支持 AAC/MP3 音频编码
- ✅ 比特流读写工具类
- ✅ PES 包编解码
- ✅ PAT/PMT 表生成和解析
- ✅ PCR 时间戳处理

## 项目结构

```
cpp/
├── include/           # 头文件
│   ├── bitstream.h   # 比特流读写工具
│   ├── pes_proto.h   # PES 协议定义
│   ├── ps_proto.h    # PS 协议定义
│   ├── ts_proto.h    # TS 协议定义
│   ├── ps_demuxer.h  # PS 解复用器
│   ├── ps_muxer.h    # PS 复用器
│   ├── ts_demuxer.h  # TS 解复用器
│   └── ts_muxer.h    # TS 复用器
├── src/              # 源文件 (2000+ 行)
├── examples/         # 示例程序
│   ├── example_ps_muxer.cpp
│   ├── example_ps_demuxer.cpp
│   ├── example_ts_muxer.cpp
│   └── example_ts_demuxer.cpp
├── CMakeLists.txt    # CMake 构建文件
├── README.md         # 本文件
├── STATUS.md         # 详细实现状态
└── IMPLEMENTATION.md # 实现文档
```

## 快速开始

### 编译

```bash
cd cpp
mkdir build
cd build
cmake ..
make
```

### 编译输出

- `libmpeg2.a` - 静态库
- `example_ps_muxer` - PS 封装示例
- `example_ps_demuxer` - PS 解封装示例  
- `example_ts_muxer` - TS 封装示例
- `example_ts_demuxer` - TS 解封装示例

### 运行示例

```bash
# PS 封装: 将 H.264 文件封装为 PS 流
./example_ps_muxer input.h264 output.ps

# PS 解封装: 从 PS 流提取 H.264
./example_ps_demuxer input.ps output.h264

# TS 封装: 将 H.264 文件封装为 TS 流
./example_ts_muxer input.h264 output.ts

# TS 解封装: 从 TS 流提取 H.264
./example_ts_demuxer input.ts output.h264
```

## 使用示例

### PS 解复用

```cpp
#include "ps_demuxer.h"
#include <fstream>

int main() {
    mpeg2::PSDemuxer demuxer;
    
    // 设置包回调
    demuxer.on_packet_ = [](const std::vector<uint8_t>& frame, 
                             mpeg2::PSStreamType cid,
                             uint64_t pts, uint64_t dts) {
        std::cout << "Got frame, codec: " << static_cast<int>(cid) 
                  << ", size: " << frame.size()
                  << ", pts: " << pts << "ms" << std::endl;
        // 处理解复用后的帧数据
    });
    
    // 读取 PS 文件
    std::ifstream input("input.ps", std::ios::binary);
    std::vector<uint8_t> buffer(4096);
    
    while (input.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) {
        demuxer.Input(buffer.data(), input.gcount());
    }
    
    return 0;
}
```

### PS 封装

```cpp
#include "ps_muxer.h"
#include <fstream>

int main() {
    mpeg2::PSMuxer muxer;
    
    // 初始化 H.264 视频流
    muxer.Init(mpeg2::PSStreamType::H264, 224);
    
    // 设置包回调
    muxer.on_packet_ = [](const std::vector<uint8_t>& packet) {
        // 写入文件或发送到网络
        std::cout << "PS packet: " << packet.size() << " bytes" << std::endl;
    });
    
    // 写入 H.264 帧
    std::vector<uint8_t> frame_data = ...;  // 读取 H.264 帧
    uint64_t timestamp_ms = 0;
    muxer.Write(mpeg2::PSStreamType::H264, frame_data, timestamp_ms);
    
    return 0;
}
```

### TS 解复用

```cpp
#include "ts_demuxer.h"
#include <fstream>

int main() {
    mpeg2::TSDemuxer demuxer;
    
    // 设置帧回调
    demuxer.on_frame_ = [](mpeg2::TSStreamType cid,
                           const std::vector<uint8_t>& frame,
                           uint64_t pts, uint64_t dts) {
        std::cout << "Got frame, codec: " << static_cast<int>(cid)
                  << ", size: " << frame.size()
                  << ", pts: " << pts << "ms" << std::endl;
    });
    
    // 读取 TS 文件
    std::ifstream input("input.ts", std::ios::binary);
    std::vector<uint8_t> buffer(4096);
    
    while (input.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) {
        demuxer.Input(buffer.data(), input.gcount());
    }
    
    return 0;
}
```

### TS 封装

```cpp
#include "ts_muxer.h"
#include <fstream>

int main() {
    mpeg2::TSMuxer muxer;
    
    // 初始化 H.264 视频流
    muxer.Init(mpeg2::TSStreamType::H264, 256);  // PID 256
    
    // 设置包回调
    muxer.on_packet_ = [](const std::vector<uint8_t>& packet) {
        // 写入文件或发送到网络
        std::cout << "TS packet: " << packet.size() << " bytes" << std::endl;
    });
    
    // 写入 H.264 帧
    std::vector<uint8_t> frame_data = ...;  // 读取 H.264 帧
    uint64_t timestamp_ms = 0;
    muxer.Write(mpeg2::TSStreamType::H264, frame_data, timestamp_ms);
    
    return 0;
}
```

## API 说明

### 编码类型

#### PS Stream Types
- `PSStreamType::UNKNOWN` - 未知类型
- `PSStreamType::AAC` - AAC 音频
- `PSStreamType::H264` - H.264 视频
- `PSStreamType::H265` - H.265/HEVC 视频
- `PSStreamType::G711A` - G.711 A-law 音频
- `PSStreamType::G711U` - G.711 μ-law 音频

#### TS Stream Types
- `TSStreamType::AUDIO_MPEG1` - MPEG-1 音频
- `TSStreamType::AUDIO_MPEG2` - MPEG-2 音频
- `TSStreamType::AAC` - AAC 音频
- `TSStreamType::H264` - H.264 视频
- `TSStreamType::H265` - H.265/HEVC 视频

### 时间戳
- PTS/DTS 时间戳单位为毫秒(ms)
- 内部自动转换为 90kHz 时钟 (MPEG-2 标准)

### 回调函数

#### PS Demuxer
```cpp
std::function<void(const std::vector<uint8_t>& frame,
                   PSStreamType cid,
                   uint64_t pts,
                   uint64_t dts)> on_packet_;
```

#### PS Muxer
```cpp
std::function<void(const std::vector<uint8_t>& packet)> on_packet_;
```

#### TS Demuxer
```cpp
std::function<void(TSStreamType cid,
                   const std::vector<uint8_t>& frame,
                   uint64_t pts,
                   uint64_t dts)> on_frame_;
                   
std::function<void(const TSPacket& packet)> on_ts_packet_;
```

#### TS Muxer
```cpp
std::function<void(const std::vector<uint8_t>& packet)> on_packet_;
```

## 技术细节

### 实现特点
- **C++17 标准**: 使用现代 C++ 特性
- **内存安全**: `std::vector` 和 `std::unique_ptr` 管理内存
- **类型安全**: `enum class` 强类型枚举
- **位操作**: 自定义 BitStream 类进行精确的位级操作

### 与 Go 版本对比
- ✅ 核心算法完全一致
- ✅ API 设计保持相似
- ✅ 性能优化: 预分配缓冲区，减少内存拷贝
- ✅ 类型安全: 编译期类型检查

### 性能
- 比特流操作经过优化
- 最小化内存分配和拷贝
- 适合实时流媒体处理

## 文档

- [README.md](README.md) - 本文件，使用说明
- [STATUS.md](STATUS.md) - 详细实现状态
- [IMPLEMENTATION.md](IMPLEMENTATION.md) - 实现细节文档
- [PROJECT_SUMMARY.md](PROJECT_SUMMARY.md) - 项目总结

## 贡献

欢迎提交 Issue 和 Pull Request！

## 许可证

与 go-mpeg2 保持一致

MIT License

## 从 Go 版本迁移

该 C++ 版本与原 Go 版本保持相似的 API 设计，主要差异：

1. 使用 `std::vector<uint8_t>` 代替 Go 的 `[]byte`
2. 使用 `std::function` 代替 Go 的函数类型
3. 使用类和方法代替 Go 的结构体和函数
4. 错误处理使用返回值而非 Go 的 error 接口

## 参考资料

- ISO/IEC 13818-1: MPEG-2 Systems
- ITU-T H.222.0
