#include "mpeg2/codec_utils.h"

#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "mpeg2/bitstream.h"

namespace mpeg2
{

    // Returns the offset of the next Annex-B start code, or -1. `size` is a size_t,
    // so the return type must be able to hold any offset within it.
    static ptrdiff_t FindStartCode(const uint8_t *data, size_t size, int *start_code_len)
    {
        for (size_t i = 0; i + 3 <= size; i++)
        {
            if (data[i] != 0x00 || data[i + 1] != 0x00)
            {
                continue;
            }
            if (data[i + 2] == 0x01)
            {
                *start_code_len = 3;
                return (ptrdiff_t)i;
            }
            if (data[i + 2] == 0x00 && i + 4 <= size && data[i + 3] == 0x01)
            {
                *start_code_len = 4;
                return (ptrdiff_t)i;
            }
        }
        return -1;
    }

    void CodecUtils::SplitFrame(const uint8_t *data, size_t size,
                                std::function<bool(const uint8_t *nalu, size_t len)> on_nalu)
    {
        if (data == nullptr || !on_nalu)
        {
            return;
        }

        int start_code_len = 0;
        ptrdiff_t pos = FindStartCode(data, size, &start_code_len);
        while (pos != -1)
        {
            const uint8_t *nalu_start = data + pos + start_code_len;
            size_t remaining = size - ((size_t)pos + start_code_len);

            int next_start_code_len = 0;
            ptrdiff_t next_pos = FindStartCode(nalu_start, remaining, &next_start_code_len);

            size_t nalu_len = (next_pos == -1) ? remaining : (size_t)next_pos;

            if (nalu_len > 0 && !on_nalu(nalu_start, nalu_len))
            {
                return;
            }

            if (next_pos == -1)
            {
                break;
            }

            data = nalu_start;
            size = remaining;
            pos = next_pos;
            start_code_len = next_start_code_len;
        }
    }

    int CodecUtils::GetNaluType(const uint8_t *nalu, size_t len)
    {
        if (len == 0)
            return H264_NAL_UNKNOWN;
        return nalu[0] & 0x1F;
    }

    bool CodecUtils::IsH264IDR(const uint8_t *nalu, size_t len)
    {
        int type = GetNaluType(nalu, len);
        return type == H264_NAL_IDR_SLICE;
    }

    bool CodecUtils::IsH264AUD(const uint8_t *nalu, size_t len)
    {
        int type = GetNaluType(nalu, len);
        return type == H264_NAL_AUD;
    }

    bool CodecUtils::IsH264VCL(const uint8_t *nalu, size_t len)
    {
        int type = GetNaluType(nalu, len);
        return type >= 1 && type <= 5;
    }

    bool CodecUtils::IsH264FirstSlice(const uint8_t *nalu, size_t len)
    {
        if (!IsH264VCL(nalu, len))
            return false;
        // Skip NALU header (1 byte)
        if (len < 2)
            return false;
        try
        {
            BitStream bs(nalu + 1, len - 1);
            // first_mb_in_slice is UE
            return bs.ReadUE() == 0;
        }
        catch (const std::exception &)
        {
            // Truncated or corrupt slice header: not a usable AU boundary.
            return false;
        }
    }

    int CodecUtils::GetH265NaluType(const uint8_t *nalu, size_t len)
    {
        if (len == 0)
            return -1;
        return (nalu[0] >> 1) & 0x3F;
    }

    bool CodecUtils::IsH265IDR(const uint8_t *nalu, size_t len)
    {
        int type = GetH265NaluType(nalu, len);
        return type == H265_NAL_IDR_W_RADL || type == H265_NAL_IDR_N_LP || type == H265_NAL_BLA_W_LP ||
               type == H265_NAL_BLA_W_RADL || type == H265_NAL_BLA_N_LP || type == H265_NAL_CRA_NUT;
    }

    bool CodecUtils::IsH265AUD(const uint8_t *nalu, size_t len)
    {
        int type = GetH265NaluType(nalu, len);
        return type == H265_NAL_AUD;
    }

    bool CodecUtils::IsH265VCL(const uint8_t *nalu, size_t len)
    {
        int type = GetH265NaluType(nalu, len);
        return type >= 0 && type <= 31;
    }

    bool CodecUtils::IsH265FirstSlice(const uint8_t *nalu, size_t len)
    {
        if (!IsH265VCL(nalu, len))
            return false;
        // Skip NALU header (2 bytes)
        if (len < 3)
            return false;
        // first_slice_segment_in_pic_flag is u(1)
        // It's the first bit of the slice header.
        // Slice header starts at byte 2.
        return (nalu[2] & 0x80) != 0;
    }

} // namespace mpeg2
