#ifndef MPEG2_H264_UTILS_H
#define MPEG2_H264_UTILS_H

#include <cstdint>
#include <vector>
#include <functional>

namespace mpeg2 {

// H.264 NALU types
enum H264_NAL_TYPE {
    H264_NAL_UNKNOWN = 0,
    H264_NAL_SLICE = 1,
    H264_NAL_DPA = 2,
    H264_NAL_DPB = 3,
    H264_NAL_DPC = 4,
    H264_NAL_IDR_SLICE = 5,
    H264_NAL_SEI = 6,
    H264_NAL_SPS = 7,
    H264_NAL_PPS = 8,
    H264_NAL_AUD = 9,
    H264_NAL_END_SEQUENCE = 10,
    H264_NAL_END_STREAM = 11,
    H264_NAL_FILLER_DATA = 12,
    H264_NAL_SPS_EXT = 13,
    H264_NAL_AUXILIARY_SLICE = 19,
};

// H.265 NALU types
enum H265_NAL_TYPE {
    H265_NAL_TRAIL_N = 0,
    H265_NAL_TRAIL_R = 1,
    H265_NAL_TSA_N = 2,
    H265_NAL_TSA_R = 3,
    H265_NAL_STSA_N = 4,
    H265_NAL_STSA_R = 5,
    H265_NAL_RADL_N = 6,
    H265_NAL_RADL_R = 7,
    H265_NAL_RASL_N = 8,
    H265_NAL_RASL_R = 9,
    H265_NAL_BLA_W_LP = 16,
    H265_NAL_BLA_W_RADL = 17,
    H265_NAL_BLA_N_LP = 18,
    H265_NAL_IDR_W_RADL = 19,
    H265_NAL_IDR_N_LP = 20,
    H265_NAL_CRA_NUT = 21,
    H265_NAL_VPS = 32,
    H265_NAL_SPS = 33,
    H265_NAL_PPS = 34,
    H265_NAL_AUD = 35,
    H265_NAL_EOS_NUT = 36,
    H265_NAL_EOB_NUT = 37,
    H265_NAL_FD_NUT = 38,
    H265_NAL_SEI_PREFIX = 39,
    H265_NAL_SEI_SUFFIX = 40,
};

class H264Utils {
public:
    // Callback returns true to continue, false to stop
    static void SplitFrame(const uint8_t* data, size_t size, std::function<bool(const uint8_t* nalu, size_t len)> on_nalu);
    
    static int GetNaluType(const uint8_t* nalu, size_t len);
    static bool IsH264IDR(const uint8_t* nalu, size_t len);
    static bool IsH264AUD(const uint8_t* nalu, size_t len);
    static bool IsH264VCL(const uint8_t* nalu, size_t len);

    static int GetH265NaluType(const uint8_t* nalu, size_t len);
    static bool IsH265IDR(const uint8_t* nalu, size_t len);
    static bool IsH265AUD(const uint8_t* nalu, size_t len);
    static bool IsH265VCL(const uint8_t* nalu, size_t len);
};

} // namespace mpeg2

#endif // MPEG2_H264_UTILS_H
