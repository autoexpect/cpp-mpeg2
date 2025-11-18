#ifndef MPEG2_TS_MUXER_H
#define MPEG2_TS_MUXER_H

#include <cstdint>
#include <vector>
#include <functional>
#include <memory>
#include "ts_proto.h"

namespace mpeg2 {

class TSMuxer {
public:
    using OnPacketCallback = std::function<void(const std::vector<uint8_t>&)>;

    TSMuxer();
    ~TSMuxer();

    uint16_t AddStream(TSStreamType cid);
    int Write(uint16_t pid, const std::vector<uint8_t>& data, uint64_t pts, uint64_t dts);
    
    void SetOnPacket(OnPacketCallback cb) { on_packet_ = cb; }

private:
    struct PESStream {
        uint16_t pid;
        uint8_t cc;
        TSStreamType stream_type;
    };

    struct TablePMT {
        uint16_t pid;
        uint8_t cc;
        uint16_t pcr_pid;
        uint8_t version_number;
        uint16_t pm;
        std::vector<std::unique_ptr<PESStream>> streams;
    };

    struct TablePAT {
        uint8_t cc;
        uint8_t version_number;
        std::vector<std::unique_ptr<TablePMT>> pmts;
    };

    void WritePAT();
    void WritePMT(TablePMT* pmt);
    void WritePES(PESStream* pes, TablePMT* pmt, const std::vector<uint8_t>& data,
                  uint64_t pts, uint64_t dts, bool idr_flag, bool with_aud);

    std::unique_ptr<TablePAT> pat_;
    uint16_t stream_pid_;
    uint16_t pmt_pid_;
    uint64_t pat_period_;
    OnPacketCallback on_packet_;
};

} // namespace mpeg2

#endif // MPEG2_TS_MUXER_H
