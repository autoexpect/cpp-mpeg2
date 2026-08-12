#ifndef MPEG2_DEF_H
#define MPEG2_DEF_H

#include <cstdint>

namespace mpeg2
{

    enum TS_PID
    {
        TS_PID_PAT = 0x0000,
        TS_PID_CAT = 0x0001,
        TS_PID_TSDT = 0x0002,
        TS_PID_IPMP = 0x0003,
        TS_PID_NIL = 0x1FFF
    };

    enum TS_STREAM_TYPE
    {
        TS_STREAM_AUDIO_MPEG1 = 0x03,
        TS_STREAM_AUDIO_MPEG2 = 0x04,
        TS_STREAM_AAC = 0x0F,
        TS_STREAM_H264 = 0x1B,
        TS_STREAM_H265 = 0x24
    };

    const int TS_PACKET_SIZE = 188;

    // Returned by TSMuxer::AddStream when no PID could be allocated.
    const uint16_t TS_INVALID_PID = 0xFFFF;

    // All timestamps handed to / produced by the muxer use the 90 kHz system clock.
    const uint64_t TS_CLOCK_HZ = 90000;

    inline bool TSIsVideoStreamType(uint8_t type)
    {
        return type == TS_STREAM_H264 || type == TS_STREAM_H265;
    }

} // namespace mpeg2

#endif // MPEG2_DEF_H
