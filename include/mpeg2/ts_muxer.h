#ifndef MPEG2_TS_MUXER_H
#define MPEG2_TS_MUXER_H

#include "mpeg2/mpeg2_def.h"
#include "mpeg2/bitstream.h"
#include <vector>
#include <functional>
#include <map>

namespace mpeg2 {

struct PmtStream {
    uint8_t stream_type;
    uint16_t pid;
    uint8_t cc; // Continuity counter for this stream
};

struct Pmt {
    uint16_t pid;
    uint8_t cc;
    uint16_t pcr_pid;
    uint8_t version_number;
    uint16_t program_number;
    std::vector<PmtStream> streams;
};

struct Pat {
    uint8_t cc;
    uint8_t version_number;
    std::vector<Pmt> pmts;
};

class TSMuxer {
public:
    TSMuxer();
    ~TSMuxer();

    uint16_t AddStream(TS_STREAM_TYPE type);
    void Write(uint16_t pid, const uint8_t* data, size_t size, uint64_t pts, uint64_t dts);
    
    void SetOnPacket(std::function<void(const std::vector<uint8_t>&)> callback);

private:
    void WritePat();
    void WritePmt(Pmt& pmt);
    void WritePes(PmtStream& stream, Pmt& pmt, const uint8_t* data, size_t size, uint64_t pts, uint64_t dts, bool idr_flag, bool with_aud);

    Pat pat_;
    uint16_t stream_pid_counter_;
    uint16_t pmt_pid_counter_;
    uint64_t pat_period_;
    
    std::function<void(const std::vector<uint8_t>&)> on_packet_;
};

} // namespace mpeg2

#endif // MPEG2_TS_MUXER_H
