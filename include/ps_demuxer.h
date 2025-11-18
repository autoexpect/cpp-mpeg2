#ifndef MPEG2_PS_DEMUXER_H
#define MPEG2_PS_DEMUXER_H

#include <cstdint>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include "ps_proto.h"
#include "bitstream.h"

namespace mpeg2 {

class PSDemuxer {
public:
    using OnFrameCallback = std::function<void(const std::vector<uint8_t>&, PSStreamType, uint64_t, uint64_t)>;
    using OnPacketCallback = std::function<void(int)>;

    PSDemuxer();
    ~PSDemuxer();

    int Input(const uint8_t* data, size_t size);
    void Flush();
    
    void SetOnFrame(OnFrameCallback cb) { on_frame_ = cb; }
    void SetOnPacket(OnPacketCallback cb) { on_packet_ = cb; }

private:
    struct PSStream {
        uint8_t sid;
        PSStreamType cid;
        uint64_t pts;
        uint64_t dts;
        std::vector<uint8_t> stream_buf;
        
        PSStream(uint8_t s, PSStreamType c)
            : sid(s), cid(c), pts(0), dts(0) {}
    };

    void DemuxPesPacket(PSStream* stream, const PesPacket* pes);
    void DemuxAudio(PSStream* stream, const PesPacket* pes);
    void DemuxH26x(PSStream* stream, const PesPacket* pes);
    void GuessCodecId(PSStream* stream);

    std::map<uint8_t, std::unique_ptr<PSStream>> stream_map_;
    PSPacket pkg_;
    bool mpeg1_;
    std::vector<uint8_t> cache_;
    OnFrameCallback on_frame_;
    OnPacketCallback on_packet_;
};

} // namespace mpeg2

#endif // MPEG2_PS_DEMUXER_H
