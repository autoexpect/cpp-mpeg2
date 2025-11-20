#include "mpeg2/aac_utils.h"
#include <cstring>

namespace mpeg2 {

void ADTS_Frame_Header::Decode(const uint8_t* aac) {
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
    Variable_Header.Frame_length = ((uint16_t)(aac[3] & 0x03) << 11) | ((uint16_t)aac[4] << 3) | ((aac[5] >> 5) & 0x07);
    Variable_Header.Adts_buffer_fullness = ((uint16_t)(aac[5] & 0x1F) << 6) | ((aac[6] >> 2) & 0x3F);
    Variable_Header.Number_of_raw_data_blocks_in_frame = aac[6] & 0x03;
}

int FindSyncword(const uint8_t* data, int size, int offset) {
    for (int i = offset; i < size - 1; i++) {
        if (data[i] == 0xFF && (data[i + 1] & 0xF0) == 0xF0) {
            return i;
        }
    }
    return -1;
}

void SplitAACFrame(const uint8_t* data, int size, std::function<void(const uint8_t*, int)> onFrame) {
    ADTS_Frame_Header adts;
    int start = FindSyncword(data, size, 0);
    while (start >= 0) {
        if (start + 7 > size) { // Header size is at least 7 bytes
            break;
        }
        adts.Decode(data + start);
        int frame_len = adts.Variable_Header.Frame_length;
        
        if (frame_len == 0) {
             // Avoid infinite loop if frame length is 0 (should not happen in valid ADTS)
             start = FindSyncword(data, size, start + 1);
             continue;
        }

        if (start + frame_len <= size) {
            onFrame(data + start, frame_len);
            start = FindSyncword(data, size, start + frame_len);
        } else {
            break;
        }
    }
}

} // namespace mpeg2
