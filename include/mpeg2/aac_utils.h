#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace mpeg2
{

    struct ADTS_Fix_Header
    {
        uint8_t ID;
        uint8_t Layer;
        uint8_t Protection_absent;
        uint8_t Profile;
        uint8_t Sampling_frequency_index;
        uint8_t Private_bit;
        uint8_t Channel_configuration;
        uint8_t Originalorcopy;
        uint8_t Home;
    };

    struct ADTS_Variable_Header
    {
        uint8_t Copyright_identification_bit;
        uint8_t copyright_identification_start;
        uint16_t Frame_length;
        uint16_t Adts_buffer_fullness;
        uint8_t Number_of_raw_data_blocks_in_frame;
    };

    struct ADTS_Frame_Header
    {
        ADTS_Fix_Header Fix_Header;
        ADTS_Variable_Header Variable_Header;

        // `aac` must point at ADTS_HEADER_MIN_SIZE readable bytes.
        void Decode(const uint8_t *aac);
    };

    // Smallest possible ADTS header (9 bytes when the CRC is present).
    const size_t ADTS_HEADER_MIN_SIZE = 7;

    // Sample rate in Hz for an ADTS sampling_frequency_index, or 0 if reserved.
    int ADTSSampleRate(uint8_t sampling_frequency_index);

    // Offset of the next ADTS syncword at or after `offset`, or -1 if there is none.
    ptrdiff_t FindSyncword(const uint8_t *data, size_t size, size_t offset);

    // Calls `onFrame` for each complete ADTS frame, header included.
    void SplitAACFrame(const uint8_t *data, size_t size,
                       std::function<void(const uint8_t *, size_t)> onFrame);

} // namespace mpeg2
