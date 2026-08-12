#include "mpeg2/aac_utils.h"

#include <cstring>

namespace mpeg2
{

    void ADTS_Frame_Header::Decode(const uint8_t *aac)
    {
        // aac[0] is syncword 0xFF
        // aac[1] is syncword 0xF0 (top 4 bits)

        Fix_Header.ID = (aac[1] >> 3) & 0x01;
        Fix_Header.Layer = (aac[1] >> 1) & 0x03;
        Fix_Header.Protection_absent = aac[1] & 0x01;
        Fix_Header.Profile = (aac[2] >> 6) & 0x03;
        Fix_Header.Sampling_frequency_index = (aac[2] >> 2) & 0x0F;
        Fix_Header.Private_bit = (aac[2] >> 1) & 0x01;
        Fix_Header.Channel_configuration = ((aac[2] & 0x01) << 2) | ((aac[3] >> 6) & 0x03);
        Fix_Header.Originalorcopy = (aac[3] >> 5) & 0x01;
        Fix_Header.Home = (aac[3] >> 4) & 0x01;

        Variable_Header.Copyright_identification_bit = (aac[3] >> 3) & 0x01;
        Variable_Header.copyright_identification_start = (aac[3] >> 2) & 0x01;
        Variable_Header.Frame_length =
            ((uint16_t)(aac[3] & 0x03) << 11) | ((uint16_t)aac[4] << 3) | ((aac[5] >> 5) & 0x07);
        Variable_Header.Adts_buffer_fullness =
            ((uint16_t)(aac[5] & 0x1F) << 6) | ((aac[6] >> 2) & 0x3F);
        Variable_Header.Number_of_raw_data_blocks_in_frame = aac[6] & 0x03;
    }

    int ADTSSampleRate(uint8_t sampling_frequency_index)
    {
        static const int kRates[16] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                       16000, 12000, 11025, 8000,  7350,  0,     0,     0};
        return kRates[sampling_frequency_index & 0x0F];
    }

    ptrdiff_t FindSyncword(const uint8_t *data, size_t size, size_t offset)
    {
        for (size_t i = offset; i + 1 < size; i++)
        {
            // 12-bit syncword 0xFFF plus layer == '00', which rules out MPEG audio
            // frames and most random 0xFF runs.
            if (data[i] == 0xFF && (data[i + 1] & 0xF6) == 0xF0)
            {
                return (ptrdiff_t)i;
            }
        }
        return -1;
    }

    void SplitAACFrame(const uint8_t *data, size_t size,
                       std::function<void(const uint8_t *, size_t)> onFrame)
    {
        if (data == nullptr || !onFrame)
        {
            return;
        }

        ADTS_Frame_Header adts;
        ptrdiff_t found = FindSyncword(data, size, 0);
        while (found >= 0)
        {
            const size_t start = (size_t)found;
            if (start + ADTS_HEADER_MIN_SIZE > size)
            {
                break; // not enough bytes left to even hold a header
            }

            adts.Decode(data + start);
            const size_t frame_len = adts.Variable_Header.Frame_length;
            const size_t min_len = adts.Fix_Header.Protection_absent ? 7u : 9u;

            // A frame that cannot hold its own header means we locked onto a false
            // syncword; resync one byte on rather than trusting the length.
            if (frame_len < min_len)
            {
                found = FindSyncword(data, size, start + 1);
                continue;
            }

            if (start + frame_len > size)
            {
                break; // trailing frame is truncated
            }

            onFrame(data + start, frame_len);
            found = FindSyncword(data, size, start + frame_len);
        }
    }

} // namespace mpeg2
