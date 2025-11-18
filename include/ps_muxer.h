#ifndef MPEG2_PS_MUXER_H
#define MPEG2_PS_MUXER_H

#include <cstdint>
#include <vector>
#include <functional>
#include <memory>
#include "ps_proto.h"
#include "bitstream.h"

namespace mpeg2 {

class PSMuxer {
public:
    using OnPacketCallback = std::function<void(const std::vector<uint8_t>&)>;

    PSMuxer();
    ~PSMuxer();

    uint8_t AddStream(PSStreamType cid);
    int Write(uint8_t sid, const std::vector<uint8_t>& frame, uint64_t pts, uint64_t dts);
    
    void SetOnPacket(OnPacketCallback cb) { on_packet_ = cb; }

private:
    std::unique_ptr<SystemHeader> system_;
    std::unique_ptr<ProgramStreamMap> psm_;
    OnPacketCallback on_packet_;
    bool first_frame_;
};

} // namespace mpeg2

#endif // MPEG2_PS_MUXER_H
