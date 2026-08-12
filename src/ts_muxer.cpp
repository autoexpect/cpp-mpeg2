#include "mpeg2/ts_muxer.h"

#include <algorithm>
#include <iostream>

#include "mpeg2/codec_utils.h"
#include "mpeg2/crc32.hpp"

namespace mpeg2
{

    namespace
    {
        // PAT/PMT are repeated at least this often (400 ms at 90 kHz).
        const uint64_t kPsiIntervalTicks = 400 * 90;

        // Elementary stream PIDs are handed out from kFirstStreamPid upwards and
        // PMT PIDs from kFirstPmtPid; the two ranges must not meet.
        const uint16_t kFirstStreamPid = 0x100;
        const uint16_t kFirstPmtPid = 0x200;

        const uint8_t kTsSyncByte = 0x47;
        const uint8_t kTableIdPat = 0x00;
        const uint8_t kTableIdPmt = 0x02;

        const uint8_t kAudH264[] = {0x00, 0x00, 0x00, 0x01, 0x09, 0xF0};
        const uint8_t kAudH265[] = {0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x50};

        // A 33-bit PTS/DTS field, split by marker bits, as per ISO/IEC 13818-1 2.4.3.7.
        void PutPesTimestamp(BitStreamWriter &bsw, uint8_t prefix, uint64_t ts)
        {
            bsw.PutUint8(prefix, 4);
            bsw.PutUint8((ts >> 30) & 0x07, 3);
            bsw.PutUint8(1, 1); // marker_bit
            bsw.PutUint16((ts >> 15) & 0x7FFF, 15);
            bsw.PutUint8(1, 1); // marker_bit
            bsw.PutUint16(ts & 0x7FFF, 15);
            bsw.PutUint8(1, 1); // marker_bit
        }

        const PmtStream *FindStream(const Pmt &pmt, uint16_t pid)
        {
            for (const auto &stream : pmt.streams)
            {
                if (stream.pid == pid)
                {
                    return &stream;
                }
            }
            return nullptr;
        }
    } // namespace

    TSMuxer::TSMuxer()
        : stream_pid_counter_(kFirstStreamPid), pmt_pid_counter_(kFirstPmtPid), last_psi_dts_(0),
          psi_sent_(false)
    {
    }

    TSMuxer::~TSMuxer() {}

    uint16_t TSMuxer::AddStream(TS_STREAM_TYPE type)
    {
        if (stream_pid_counter_ >= kFirstPmtPid)
        {
            return TS_INVALID_PID; // stream PID range exhausted
        }

        if (pat_.pmts.empty())
        {
            Pmt pmt;
            pmt.pid = pmt_pid_counter_++;
            pmt.program_number = 1;
            pat_.pmts.push_back(pmt);
        }

        uint16_t pid = stream_pid_counter_++;
        PmtStream stream;
        stream.stream_type = static_cast<uint8_t>(type);
        stream.pid = pid;

        pat_.pmts[0].streams.push_back(stream);

        // Adding a stream after the tables went out changes the PMT contents, so
        // receivers must be told to re-parse it.
        if (psi_sent_)
        {
            pat_.pmts[0].version_number = (pat_.pmts[0].version_number + 1) & 0x1F;
        }
        return pid;
    }

    void TSMuxer::SetOnPacket(std::function<void(const std::vector<uint8_t> &)> callback)
    {
        on_packet_ = std::move(callback);
    }

    void TSMuxer::Write(uint16_t pid, const uint8_t *data, size_t size, uint64_t pts, uint64_t dts)
    {
        if (data == nullptr || size == 0)
        {
            return;
        }

        Pmt *whichpmt = nullptr;
        PmtStream *whichstream = nullptr;

        for (auto &pmt : pat_.pmts)
        {
            for (auto &stream : pmt.streams)
            {
                if (stream.pid == pid)
                {
                    whichpmt = &pmt;
                    whichstream = &stream;
                    break;
                }
            }
            if (whichstream)
            {
                break;
            }
        }

        if (!whichpmt || !whichstream)
        {
            return;
        }

        // The PCR rides on a video stream when there is one; otherwise on whichever
        // stream shows up first. Once a video stream owns it we never hand it back,
        // so two video streams cannot make it oscillate.
        const bool is_video = TSIsVideoStreamType(whichstream->stream_type);
        if (whichpmt->pcr_pid != pid)
        {
            const PmtStream *pcr_stream = FindStream(*whichpmt, whichpmt->pcr_pid);
            if (pcr_stream == nullptr || (is_video && !TSIsVideoStreamType(pcr_stream->stream_type)))
            {
                whichpmt->pcr_pid = pid;
                if (psi_sent_)
                {
                    whichpmt->version_number = (whichpmt->version_number + 1) & 0x1F;
                }
            }
        }

        bool with_aud = false;
        bool idr_flag = false;

        if (is_video)
        {
            const bool h265 = (whichstream->stream_type == TS_STREAM_H265);
            CodecUtils::SplitFrame(data, size,
                                   [&](const uint8_t *nalu, size_t len)
                                   {
                                       if (h265)
                                       {
                                           if (CodecUtils::IsH265AUD(nalu, len))
                                               with_aud = true;
                                           if (CodecUtils::IsH265IDR(nalu, len))
                                               idr_flag = true;
                                       }
                                       else
                                       {
                                           if (CodecUtils::IsH264AUD(nalu, len))
                                               with_aud = true;
                                           if (CodecUtils::IsH264IDR(nalu, len))
                                               idr_flag = true;
                                       }
                                       return true;
                                   });
        }

        // Re-emit the tables periodically, and immediately after a large jump
        // backwards (a discontinuity) so a receiver still locks on. Interleaved
        // streams routinely run slightly behind one another, so only a gap wider
        // than the interval itself counts either way. Both differences are taken
        // on unsigned values, so the two directions must be tested separately.
        bool need_psi = !psi_sent_;
        if (psi_sent_)
        {
            const uint64_t delta = (dts >= last_psi_dts_) ? (dts - last_psi_dts_) : (last_psi_dts_ - dts);
            need_psi = (delta >= kPsiIntervalTicks);
        }
        if (need_psi)
        {
            last_psi_dts_ = dts;
            WritePat();
            for (auto &pmt : pat_.pmts)
            {
                WritePmt(pmt);
            }
            psi_sent_ = true;
        }

        WritePes(*whichstream, *whichpmt, data, size, pts, dts, idr_flag, with_aud);
    }

    void TSMuxer::FinishSection(BitStreamWriter &bsw, size_t length_loc, size_t section_start)
    {
        // section_length counts everything after the field itself, CRC included.
        const size_t section_length = bsw.Size() - section_start + 4;
        // section_syntax_indicator '1' + '0' + reserved '11' => 0xB000
        bsw.SetUint16(static_cast<uint16_t>((section_length & 0x0FFF) | 0xB000), length_loc);

        // The CRC covers the section from table_id up to (not including) the CRC.
        const size_t crc_start = length_loc - 1;
        const uint32_t crc = crc32<IEEE8023_CRC32_POLYNOMIAL>(0xFFFFFFFF, bsw.Bits().data() + crc_start,
                                                              bsw.Size() - crc_start);
        bsw.PutByte((crc >> 24) & 0xFF);
        bsw.PutByte((crc >> 16) & 0xFF);
        bsw.PutByte((crc >> 8) & 0xFF);
        bsw.PutByte(crc & 0xFF);

        bsw.FillRemainData(0xFF);
        EmitPacket(bsw);
    }

    void TSMuxer::EmitPacket(BitStreamWriter &bsw)
    {
        // A section that outgrew one packet would silently corrupt the stream;
        // drop it loudly instead. Sections are never split across packets, which
        // caps a program at 33 elementary streams ((188 - 21) / 5).
        if (bsw.Size() != static_cast<size_t>(TS_PACKET_SIZE) ||
            bsw.Bits().size() != static_cast<size_t>(TS_PACKET_SIZE))
        {
            std::cerr << "mpeg2: dropping malformed TS packet of " << bsw.Size() << " bytes"
                      << std::endl;
            return;
        }
        if (on_packet_)
        {
            on_packet_(bsw.Bits());
        }
    }

    void TSMuxer::WritePat()
    {
        BitStreamWriter bsw(TS_PACKET_SIZE);

        // TS Header
        bsw.PutByte(kTsSyncByte);
        bsw.PutUint8(0, 1);            // transport_error_indicator
        bsw.PutUint8(1, 1);            // payload_unit_start_indicator
        bsw.PutUint8(0, 1);            // transport_priority
        bsw.PutUint16(TS_PID_PAT, 13); // PID 0 for PAT
        bsw.PutUint8(0, 2);            // transport_scrambling_control
        bsw.PutUint8(1, 2);            // adaptation_field_control (payload only)
        bsw.PutUint8(pat_.cc, 4);
        pat_.cc = (pat_.cc + 1) & 0x0F;

        bsw.PutByte(0x00); // pointer_field

        bsw.PutByte(kTableIdPat);
        const size_t length_loc = bsw.Size(); // section_length field position
        bsw.PutUint8(1, 1);                   // section_syntax_indicator (patched by FinishSection)
        bsw.PutUint8(0, 1);                   // '0'
        bsw.PutUint8(3, 2);                   // reserved
        bsw.PutUint16(0, 12);                 // section_length placeholder

        const size_t section_start = bsw.Size();

        bsw.PutUint16(1, 16); // transport_stream_id
        bsw.PutUint8(3, 2);   // reserved
        bsw.PutUint8(pat_.version_number, 5);
        bsw.PutUint8(1, 1); // current_next_indicator
        bsw.PutUint8(0, 8); // section_number
        bsw.PutUint8(0, 8); // last_section_number

        for (const auto &pmt : pat_.pmts)
        {
            bsw.PutUint16(pmt.program_number, 16);
            bsw.PutUint8(7, 3); // reserved
            bsw.PutUint16(pmt.pid, 13);
        }

        FinishSection(bsw, length_loc, section_start);
    }

    void TSMuxer::WritePmt(Pmt &pmt)
    {
        BitStreamWriter bsw(TS_PACKET_SIZE);

        // TS Header
        bsw.PutByte(kTsSyncByte);
        bsw.PutUint8(0, 1);
        bsw.PutUint8(1, 1);
        bsw.PutUint8(0, 1);
        bsw.PutUint16(pmt.pid, 13);
        bsw.PutUint8(0, 2);
        bsw.PutUint8(1, 2);
        bsw.PutUint8(pmt.cc, 4);
        pmt.cc = (pmt.cc + 1) & 0x0F;

        bsw.PutByte(0x00); // pointer_field

        bsw.PutByte(kTableIdPmt);
        const size_t length_loc = bsw.Size();
        bsw.PutUint8(1, 1);   // section_syntax_indicator
        bsw.PutUint8(0, 1);   // '0'
        bsw.PutUint8(3, 2);   // reserved
        bsw.PutUint16(0, 12); // section_length placeholder

        const size_t section_start = bsw.Size();

        bsw.PutUint16(pmt.program_number, 16);
        bsw.PutUint8(3, 2); // reserved
        bsw.PutUint8(pmt.version_number, 5);
        bsw.PutUint8(1, 1); // current_next_indicator
        bsw.PutUint8(0, 8); // section_number
        bsw.PutUint8(0, 8); // last_section_number
        bsw.PutUint8(7, 3); // reserved
        bsw.PutUint16(pmt.pcr_pid, 13);
        bsw.PutUint8(0x0F, 4); // reserved
        bsw.PutUint16(0, 12);  // program_info_length

        for (const auto &stream : pmt.streams)
        {
            bsw.PutUint8(stream.stream_type, 8);
            bsw.PutUint8(7, 3); // reserved
            bsw.PutUint16(stream.pid, 13);
            bsw.PutUint8(0x0F, 4); // reserved
            bsw.PutUint16(0, 12);  // ES_info_length
        }

        FinishSection(bsw, length_loc, section_start);
    }

    void TSMuxer::WritePes(PmtStream &stream, Pmt &pmt, const uint8_t *data, size_t size,
                           uint64_t pts, uint64_t dts, bool idr_flag, bool with_aud)
    {
        const bool is_video = TSIsVideoStreamType(stream.stream_type);
        const bool with_dts = (pts != dts);
        const uint8_t header_data_len = with_dts ? 10 : 5; // PTS [+ DTS], 5 bytes each

        // Frames that carry no access unit delimiter get one synthesized, so every
        // PES payload starts on a boundary a decoder can recognise.
        const uint8_t *aud = nullptr;
        size_t aud_len = 0;
        if (!with_aud)
        {
            if (stream.stream_type == TS_STREAM_H264)
            {
                aud = kAudH264;
                aud_len = sizeof(kAudH264);
            }
            else if (stream.stream_type == TS_STREAM_H265)
            {
                aud = kAudH265;
                aud_len = sizeof(kAudH265);
            }
        }

        BitStreamWriter pes(32);
        pes.PutByte(0x00); // packet_start_code_prefix
        pes.PutByte(0x00);
        pes.PutByte(0x01);
        pes.PutByte(is_video ? 0xE0 : 0xC0); // stream_id: video 0 / audio 0

        // PES_packet_length counts every byte after this field. Video streams may
        // declare 0 ("unbounded"), which avoids the 64 KiB ceiling entirely.
        const size_t pes_len = size + aud_len + 3 + header_data_len;
        pes.PutUint16((is_video || pes_len > 0xFFFF) ? 0 : static_cast<uint16_t>(pes_len), 16);

        pes.PutUint8(0x02, 2); // '10'
        pes.PutUint8(0, 2);    // PES_scrambling_control
        pes.PutUint8(0, 1);    // PES_priority
        pes.PutUint8(1, 1);    // data_alignment_indicator: payload starts at an AU
        pes.PutUint8(0, 1);    // copyright
        pes.PutUint8(0, 1);    // original_or_copy

        pes.PutUint8(with_dts ? 0x03 : 0x02, 2); // PTS_DTS_flags
        pes.PutUint8(0, 1);                      // ESCR_flag
        pes.PutUint8(0, 1);                      // ES_rate_flag
        pes.PutUint8(0, 1);                      // DSM_trick_mode_flag
        pes.PutUint8(0, 1);                      // additional_copy_info_flag
        pes.PutUint8(0, 1);                      // PES_CRC_flag
        pes.PutUint8(0, 1);                      // PES_extension_flag

        pes.PutByte(header_data_len);
        PutPesTimestamp(pes, with_dts ? 0x03 : 0x02, pts);
        if (with_dts)
        {
            PutPesTimestamp(pes, 0x01, dts);
        }

        // Everything that precedes the elementary stream data in the first packet.
        std::vector<uint8_t> header(pes.Bits().begin(), pes.Bits().begin() + pes.Size());
        if (aud_len > 0)
        {
            header.insert(header.end(), aud, aud + aud_len);
        }

        size_t header_off = 0;
        size_t data_off = 0;
        bool first = true;
        BitStreamWriter bsw(TS_PACKET_SIZE);

        while (first || header_off < header.size() || data_off < size)
        {
            bsw.Reset();

            const size_t remaining = (header.size() - header_off) + (size - data_off);

            // The adaptation field is sized before anything is written, so the
            // payload length is exact and no back-patching is needed.
            const bool want_pcr = first && (stream.pid == pmt.pcr_pid);
            const bool want_rai = first && idr_flag;
            bool has_af = want_pcr || want_rai;
            size_t af_body = has_af ? (1 + (want_pcr ? 6u : 0u)) : 0; // flags [+ PCR]
            size_t af_total = has_af ? (1 + af_body) : 0;             // + length byte
            size_t space = TS_PACKET_SIZE - 4 - af_total;

            if (remaining < space)
            {
                // Short payload: pad via adaptation field stuffing, never by
                // appending filler after the payload.
                const size_t stuffing = space - remaining;
                if (!has_af)
                {
                    has_af = true;
                    af_total = stuffing; // the length byte itself absorbs one byte
                    af_body = af_total - 1;
                }
                else
                {
                    af_body += stuffing;
                    af_total += stuffing;
                }
                space = TS_PACKET_SIZE - 4 - af_total; // now equals `remaining`
            }

            // TS Header
            bsw.PutByte(kTsSyncByte);
            bsw.PutUint8(0, 1);             // transport_error_indicator
            bsw.PutUint8(first ? 1 : 0, 1); // payload_unit_start_indicator
            bsw.PutUint8(0, 1);             // transport_priority
            bsw.PutUint16(stream.pid, 13);
            bsw.PutUint8(0, 2); // transport_scrambling_control
            bsw.PutUint8(has_af ? 0x03 : 0x01, 2);
            bsw.PutUint8(stream.cc, 4);
            stream.cc = (stream.cc + 1) & 0x0F;

            if (has_af)
            {
                bsw.PutByte(static_cast<uint8_t>(af_body));
                if (af_body > 0)
                {
                    uint8_t flags = 0;
                    if (want_rai)
                        flags |= 0x40; // random_access_indicator
                    if (want_pcr)
                        flags |= 0x10; // PCR_flag
                    bsw.PutByte(flags);

                    if (want_pcr)
                    {
                        const uint64_t pcr = (dts != 0) ? dts : pts;
                        bsw.PutUint64(pcr & 0x1FFFFFFFFULL, 33); // program_clock_reference_base
                        bsw.PutUint8(0x3F, 6);                   // reserved
                        bsw.PutUint16(0, 9); // extension: the 27 MHz remainder is always 0 here
                    }
                    for (size_t i = 1 + (want_pcr ? 6u : 0u); i < af_body; i++)
                    {
                        bsw.PutByte(0xFF); // stuffing_byte
                    }
                }
            }

            size_t left = space;
            if (header_off < header.size())
            {
                const size_t n = std::min(left, header.size() - header_off);
                bsw.PutBytes(header.data() + header_off, n);
                header_off += n;
                left -= n;
            }
            if (left > 0 && data_off < size)
            {
                const size_t n = std::min(left, size - data_off);
                bsw.PutBytes(data + data_off, n);
                data_off += n;
                left -= n;
            }

            EmitPacket(bsw);
            first = false;
        }
    }

} // namespace mpeg2
