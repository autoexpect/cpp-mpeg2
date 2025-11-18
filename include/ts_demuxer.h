#ifndef MPEG2_TS_DEMUXER_H
#define MPEG2_TS_DEMUXER_H

#include <cstdint>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include "ts_proto.h"
#include "pes_proto.h"

namespace mpeg2 {

class TSDemuxer {
public:
    using OnFrameCallback = std::function<void(TSStreamType, const std::vector<uint8_t>&, uint64_t, uint64_t)>;
    using OnTSPacketCallback = std::function<void(const TSPacket&)>;

    TSDemuxer();
    ~TSDemuxer();

    int Input(const uint8_t* data, size_t size);
    
    void SetOnFrame(OnFrameCallback cb) { on_frame_ = cb; }
    void SetOnTSPacket(OnTSPacketCallback cb) { on_ts_packet_ = cb; }

private:
    struct TSStream {
        TSStreamType cid;
        PESStreamID pes_sid;
        std::unique_ptr<PesPacket> pes_pkg;
        std::vector<uint8_t> payload;
        uint64_t pts;
        uint64_t dts;
    };

    struct TSProgram {
        uint16_t pn;
        std::map<uint16_t, std::unique_ptr<TSStream>> streams;
    };

    void DoAudioPesPacket(TSStream* stream, uint8_t start_indicator);
    void DoVideoPesPacket(TSStream* stream, uint8_t start_indicator);
    int Probe(const uint8_t* data, size_t size, size_t& offset);

    std::map<uint16_t, std::unique_ptr<TSProgram>> programs_;
    OnFrameCallback on_frame_;
    OnTSPacketCallback on_ts_packet_;
};

} // namespace mpeg2

#endif // MPEG2_TS_DEMUXER_H
