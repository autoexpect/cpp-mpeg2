#pragma once

#include <cstdint>
#include <vector>
#include <functional>

namespace mpeg2 {

struct ADTS_Fix_Header {
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

struct ADTS_Variable_Header {
    uint8_t Copyright_identification_bit;
    uint8_t copyright_identification_start;
    uint16_t Frame_length;
    uint16_t Adts_buffer_fullness;
    uint8_t Number_of_raw_data_blocks_in_frame;
};

struct ADTS_Frame_Header {
    ADTS_Fix_Header Fix_Header;
    ADTS_Variable_Header Variable_Header;

    void Decode(const uint8_t* aac);
};

int FindSyncword(const uint8_t* data, int size, int offset);
void SplitAACFrame(const uint8_t* data, int size, std::function<void(const uint8_t*, int)> onFrame);

} // namespace mpeg2
