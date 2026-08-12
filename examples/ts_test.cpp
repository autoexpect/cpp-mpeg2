#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "mpeg2/aac_utils.h"
#include "mpeg2/codec_utils.h"
#include "mpeg2/ts_muxer.h"

using namespace mpeg2;

namespace
{

    bool ReadFile(const std::string &path, std::vector<uint8_t> &out)
    {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in)
            return false;
        const std::streamoff size = in.tellg();
        if (size < 0)
            return false;
        in.seekg(0, std::ios::beg);
        out.resize((size_t)size);
        if (size > 0 && !in.read((char *)out.data(), size))
            return false;
        return true;
    }

    void AppendAnnexB(std::vector<uint8_t> &dst, const uint8_t *nalu, size_t len)
    {
        static const uint8_t kStartCode[4] = {0x00, 0x00, 0x00, 0x01};
        dst.insert(dst.end(), kStartCode, kStartCode + 4);
        dst.insert(dst.end(), nalu, nalu + len);
    }

    bool HasSuffix(const std::string &s, const std::string &suffix)
    {
        return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input_video> <output_ts> [interval_ms] [input_aac]"
                  << std::endl;
        return 1;
    }

    const std::string input_file = argv[1];
    const std::string output_file = argv[2];
    const bool is_h265 = HasSuffix(input_file, ".h265") || HasSuffix(input_file, ".hevc");

    int interval = 40; // ms per frame, default 25 fps
    if (argc >= 4)
    {
        interval = std::atoi(argv[3]);
        if (interval <= 0)
        {
            std::cerr << "Invalid interval_ms: " << argv[3] << std::endl;
            return 1;
        }
    }

    // Audio is optional: only muxed when a readable AAC file is given.
    std::vector<uint8_t> aac_content;
    std::vector<std::pair<size_t, size_t>> aac_frames; // offset, length into aac_content
    int aac_sample_rate = 0;
    if (argc >= 5)
    {
        if (!ReadFile(argv[4], aac_content))
        {
            std::cerr << "Failed to read " << argv[4] << std::endl;
            return 1;
        }
        SplitAACFrame(aac_content.data(), aac_content.size(),
                      [&](const uint8_t *frame, size_t size)
                      {
                          if (aac_frames.empty())
                          {
                              ADTS_Frame_Header hdr;
                              hdr.Decode(frame);
                              aac_sample_rate = ADTSSampleRate(hdr.Fix_Header.Sampling_frequency_index);
                          }
                          aac_frames.push_back(
                              std::make_pair((size_t)(frame - aac_content.data()), size));
                      });
        if (aac_frames.empty() || aac_sample_rate == 0)
        {
            std::cerr << "No usable ADTS frames in " << argv[4] << std::endl;
            return 1;
        }
        std::cout << "AAC frames: " << aac_frames.size() << " @ " << aac_sample_rate << " Hz"
                  << std::endl;
    }

    std::vector<uint8_t> video;
    if (!ReadFile(input_file, video))
    {
        std::cerr << "Failed to read " << input_file << std::endl;
        return 1;
    }

    std::ofstream out(output_file, std::ios::binary);
    if (!out)
    {
        std::cerr << "Failed to open output file " << output_file << std::endl;
        return 1;
    }

    TSMuxer muxer;
    const uint16_t video_pid = muxer.AddStream(is_h265 ? TS_STREAM_H265 : TS_STREAM_H264);
    const uint16_t audio_pid = aac_frames.empty() ? TS_INVALID_PID : muxer.AddStream(TS_STREAM_AAC);
    if (video_pid == TS_INVALID_PID)
    {
        std::cerr << "Failed to allocate a video PID" << std::endl;
        return 1;
    }

    muxer.SetOnPacket([&](const std::vector<uint8_t> &packet)
                      { out.write((const char *)packet.data(), packet.size()); });

    // Access unit assembly state. Timestamps are in 90 kHz units.
    const uint64_t frame_ticks = (uint64_t)interval * 90;
    uint64_t pts = 0;
    uint64_t audio_samples = 0; // total AAC samples emitted, drives the audio clock
    size_t aac_idx = 0;

    std::vector<uint8_t> au_buffer;
    std::vector<uint8_t> last_vps, last_sps, last_pps;
    bool has_vcl = false;
    bool au_has_vps = false, au_has_sps = false, au_has_pps = false;

    auto flush_au = [&]()
    {
        if (au_buffer.empty())
            return;
        muxer.Write(video_pid, au_buffer.data(), au_buffer.size(), pts, pts);

        // Interleave every audio frame that belongs before the next video frame.
        while (audio_pid != TS_INVALID_PID && !aac_frames.empty())
        {
            const uint64_t apts = audio_samples * TS_CLOCK_HZ / (uint64_t)aac_sample_rate;
            if (apts >= pts)
                break;
            const std::pair<size_t, size_t> &f = aac_frames[aac_idx];
            muxer.Write(audio_pid, aac_content.data() + f.first, f.second, apts, apts);
            audio_samples += 1024; // one AAC access unit
            if (++aac_idx >= aac_frames.size())
                aac_idx = 0; // loop the audio track to cover the whole video
        }

        pts += frame_ticks;
        au_buffer.clear();
        has_vcl = false;
        au_has_vps = au_has_sps = au_has_pps = false;
    };

    CodecUtils::SplitFrame(
        video.data(), video.size(),
        [&](const uint8_t *nalu, size_t len)
        {
            bool new_au = false;
            bool is_parameter_set = false;
            const int type = is_h265 ? CodecUtils::GetH265NaluType(nalu, len)
                                     : CodecUtils::GetNaluType(nalu, len);
            const bool is_vcl =
                is_h265 ? CodecUtils::IsH265VCL(nalu, len) : CodecUtils::IsH264VCL(nalu, len);
            const bool is_aud =
                is_h265 ? CodecUtils::IsH265AUD(nalu, len) : CodecUtils::IsH264AUD(nalu, len);
            const bool is_idr =
                is_h265 ? CodecUtils::IsH265IDR(nalu, len) : CodecUtils::IsH264IDR(nalu, len);

            if (is_h265 && type == H265_NAL_VPS)
            {
                last_vps.assign(nalu, nalu + len);
                au_has_vps = true;
                is_parameter_set = true;
            }
            else if (type == (is_h265 ? (int)H265_NAL_SPS : (int)H264_NAL_SPS))
            {
                last_sps.assign(nalu, nalu + len);
                au_has_sps = true;
                is_parameter_set = true;
            }
            else if (type == (is_h265 ? (int)H265_NAL_PPS : (int)H264_NAL_PPS))
            {
                last_pps.assign(nalu, nalu + len);
                au_has_pps = true;
                is_parameter_set = true;
            }

            if (is_aud)
                new_au = true;
            if (is_parameter_set && has_vcl)
                new_au = true;
            if (is_vcl && has_vcl)
            {
                const bool first_slice = is_h265 ? CodecUtils::IsH265FirstSlice(nalu, len)
                                                 : CodecUtils::IsH264FirstSlice(nalu, len);
                if (first_slice)
                    new_au = true;
            }

            if (new_au)
            {
                flush_au();
            }

            // Repeat the parameter sets ahead of every IDR so the stream stays
            // decodable from any random access point.
            if (is_idr)
            {
                if (is_h265 && !au_has_vps && !last_vps.empty())
                {
                    AppendAnnexB(au_buffer, last_vps.data(), last_vps.size());
                    au_has_vps = true;
                }
                if (!au_has_sps && !last_sps.empty())
                {
                    AppendAnnexB(au_buffer, last_sps.data(), last_sps.size());
                    au_has_sps = true;
                }
                if (!au_has_pps && !last_pps.empty())
                {
                    AppendAnnexB(au_buffer, last_pps.data(), last_pps.size());
                    au_has_pps = true;
                }
            }

            AppendAnnexB(au_buffer, nalu, len);
            if (is_vcl)
                has_vcl = true;
            return true;
        });

    flush_au();

    out.flush();
    if (!out)
    {
        std::cerr << "Failed to write " << output_file << std::endl;
        return 1;
    }
    return 0;
}
