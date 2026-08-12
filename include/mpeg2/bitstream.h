#ifndef MPEG2_BITSTREAM_H
#define MPEG2_BITSTREAM_H

#include <cstdint>
#include <string>
#include <vector>

namespace mpeg2
{

    class BitStream
    {
    public:
        BitStream(const uint8_t *buf, size_t size);
        ~BitStream();

        uint8_t GetBit();
        uint64_t GetBits(int n);
        void SkipBits(int n);

        uint8_t Uint8(int n);
        uint16_t Uint16(int n);
        uint32_t Uint32(int n);

        std::vector<uint8_t> GetBytes(int n);

        uint64_t ReadUE();
        int64_t ReadSE();

        int RemainBits();
        int RemainBytes();

    private:
        const uint8_t *bits_;
        size_t size_;
        size_t bytesOffset_;
        int bitsOffset_;
    };

    class BitStreamWriter
    {
    public:
        BitStreamWriter(int n);
        ~BitStreamWriter();

        void PutByte(uint8_t v);
        void PutBytes(const uint8_t *v, size_t size);
        void PutBytes(const std::vector<uint8_t> &v);

        void PutUint8(uint8_t v, int n);
        void PutUint16(uint16_t v, int n);
        void PutUint32(uint32_t v, int n);
        void PutUint64(uint64_t v, int n);

        void SetByte(uint8_t v, size_t where);
        void SetUint16(uint16_t v, size_t where);

        void FillRemainData(uint8_t v);
        void Reset();

        const std::vector<uint8_t> &Bits() const;
        size_t Size() const;

    private:
        void expandSpace(int n);

        std::vector<uint8_t> bits_;
        size_t byteOffset_;
        int bitOffset_;
    };

} // namespace mpeg2

#endif // MPEG2_BITSTREAM_H
