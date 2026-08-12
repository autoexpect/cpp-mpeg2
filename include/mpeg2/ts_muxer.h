#ifndef MPEG2_TS_MUXER_H
#define MPEG2_TS_MUXER_H

#include <functional>
#include <vector>

#include "mpeg2/bitstream.h"
#include "mpeg2/mpeg2_def.h"

namespace mpeg2
{

    struct PmtStream
    {
        uint8_t stream_type = 0;
        uint16_t pid = 0;
        uint8_t cc = 0; // Continuity counter for this stream
    };

    struct Pmt
    {
        uint16_t pid = 0;
        uint8_t cc = 0;
        uint16_t pcr_pid = 0;
        uint8_t version_number = 0;
        uint16_t program_number = 0;
        std::vector<PmtStream> streams;
    };

    struct Pat
    {
        uint8_t cc = 0;
        uint8_t version_number = 0;
        std::vector<Pmt> pmts;
    };

    class TSMuxer
    {
    public:
        TSMuxer();
        ~TSMuxer();

        // Registers an elementary stream and returns the PID assigned to it,
        // or TS_INVALID_PID if no PID is available.
        uint16_t AddStream(TS_STREAM_TYPE type);

        // Writes one access unit (a complete video frame or audio frame).
        //
        // `pts` and `dts` are in 90 kHz units (see TS_CLOCK_HZ). Pass pts == dts
        // for streams without B-frames. Video is expected in Annex-B byte-stream
        // format, AAC in ADTS format.
        void Write(uint16_t pid, const uint8_t *data, size_t size, uint64_t pts, uint64_t dts);

        // Invoked once per finished 188-byte transport packet.
        void SetOnPacket(std::function<void(const std::vector<uint8_t> &)> callback);

    private:
        void WritePat();
        void WritePmt(Pmt &pmt);
        void WritePes(PmtStream &stream, Pmt &pmt, const uint8_t *data, size_t size, uint64_t pts,
                      uint64_t dts, bool idr_flag, bool with_aud);

        // Back-patches section_length, appends the CRC-32, pads and emits the packet.
        void FinishSection(BitStreamWriter &bsw, size_t length_loc, size_t section_start);
        // Emits a packet, rejecting anything that is not exactly 188 bytes.
        void EmitPacket(BitStreamWriter &bsw);

        Pat pat_;
        uint16_t stream_pid_counter_;
        uint16_t pmt_pid_counter_;
        uint64_t last_psi_dts_; // DTS at which PAT/PMT were last emitted
        bool psi_sent_;

        std::function<void(const std::vector<uint8_t> &)> on_packet_;
    };

} // namespace mpeg2

#endif // MPEG2_TS_MUXER_H
