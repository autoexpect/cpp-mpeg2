#include "mpeg2/codec_utils.h"
#include "mpeg2/bitstream.h"
#include <cstring>

namespace mpeg2 {

static int FindStartCode(const uint8_t* data, size_t size, int* start_code_len) {
    for (size_t i = 0; i < size; i++) {
        if (size - i >= 4 && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x00 && data[i+3] == 0x01) {
            *start_code_len = 4;
            return i;
        }
        if (size - i >= 3 && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x01) {
            *start_code_len = 3;
            return i;
        }
    }
    return -1;
}

void H264Utils::SplitFrame(const uint8_t* data, size_t size, std::function<bool(const uint8_t* nalu, size_t len)> on_nalu) {
    int start_code_len = 0;
    int pos = FindStartCode(data, size, &start_code_len);
    while (pos != -1) {
        const uint8_t* nalu_start = data + pos + start_code_len;
        size_t remaining = size - (pos + start_code_len);
        
        int next_start_code_len = 0;
        int next_pos = FindStartCode(nalu_start, remaining, &next_start_code_len);
        
        size_t nalu_len = (next_pos == -1) ? remaining : next_pos;
        
        if (!on_nalu(nalu_start, nalu_len)) {
            return;
        }
        
        if (next_pos == -1) {
            break;
        }
        
        data = nalu_start;
        size = remaining;
        pos = next_pos;
        start_code_len = next_start_code_len;
    }
}

int H264Utils::GetNaluType(const uint8_t* nalu, size_t len) {
    if (len == 0) return H264_NAL_UNKNOWN;
    return nalu[0] & 0x1F;
}

bool H264Utils::IsH264IDR(const uint8_t* nalu, size_t len) {
    int type = GetNaluType(nalu, len);
    return type == H264_NAL_IDR_SLICE;
}

bool H264Utils::IsH264AUD(const uint8_t* nalu, size_t len) {
    int type = GetNaluType(nalu, len);
    return type == H264_NAL_AUD;
}

bool H264Utils::IsH264VCL(const uint8_t* nalu, size_t len) {
    int type = GetNaluType(nalu, len);
    return type >= 1 && type <= 5;
}

bool H264Utils::IsH264FirstSlice(const uint8_t* nalu, size_t len) {
    if (!IsH264VCL(nalu, len)) return false;
    // Skip NALU header (1 byte)
    if (len < 2) return false;
    BitStream bs(nalu + 1, len - 1);
    // first_mb_in_slice is UE
    uint64_t first_mb = bs.ReadUE();
    return first_mb == 0;
}

int H264Utils::GetH265NaluType(const uint8_t* nalu, size_t len) {
    if (len == 0) return -1;
    return (nalu[0] >> 1) & 0x3F;
}

bool H264Utils::IsH265IDR(const uint8_t* nalu, size_t len) {
    int type = GetH265NaluType(nalu, len);
    return type == H265_NAL_IDR_W_RADL || type == H265_NAL_IDR_N_LP || type == H265_NAL_BLA_W_LP ||
           type == H265_NAL_BLA_W_RADL || type == H265_NAL_BLA_N_LP || type == H265_NAL_CRA_NUT;
}

bool H264Utils::IsH265AUD(const uint8_t* nalu, size_t len) {
    int type = GetH265NaluType(nalu, len);
    return type == H265_NAL_AUD;
}

bool H264Utils::IsH265VCL(const uint8_t* nalu, size_t len) {
    int type = GetH265NaluType(nalu, len);
    return type >= 0 && type <= 31;
}

bool H264Utils::IsH265FirstSlice(const uint8_t* nalu, size_t len) {
    if (!IsH265VCL(nalu, len)) return false;
    // Skip NALU header (2 bytes)
    if (len < 3) return false;
    // first_slice_segment_in_pic_flag is u(1)
    // It's the first bit of the slice header.
    // Slice header starts at byte 2.
    return (nalu[2] & 0x80) != 0;
}

} // namespace mpeg2
