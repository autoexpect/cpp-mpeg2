#ifndef MPEG2_BITSTREAM_H
#define MPEG2_BITSTREAM_H

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <cstring>

namespace mpeg2 {

class BitStream {
public:
    BitStream(const uint8_t* data, size_t size);
    BitStream(const std::vector<uint8_t>& data);
    
    uint32_t Uint8(uint32_t bits);
    uint32_t Uint16(uint32_t bits);
    uint32_t Uint32(uint32_t bits);
    uint64_t Uint64(uint32_t bits);
    
    uint32_t NextBits(uint32_t bits);
    void SkipBits(uint32_t bits);
    
    size_t RemainBytes() const;
    size_t RemainBits() const;
    const uint8_t* RemainData() const;
    bool EOS() const;
    
private:
    const uint8_t* data_;
    size_t size_;
    size_t byte_pos_;
    uint8_t bit_pos_;
};

class BitStreamWriter {
public:
    BitStreamWriter(size_t capacity = 1024);
    
    void PutByte(uint8_t val);
    void PutUint8(uint8_t val, uint32_t bits);
    void PutUint16(uint16_t val, uint32_t bits);
    void PutUint32(uint32_t val, uint32_t bits);
    void PutUint64(uint64_t val, uint32_t bits);
    void PutBytes(const uint8_t* data, size_t size);
    
    const std::vector<uint8_t>& Bits() const { return buffer_; }
    std::vector<uint8_t>& Bits() { return buffer_; }
    std::vector<uint8_t> Bytes();
    void Reset();
    void FillRemainData(uint8_t value);
    
private:
    void FlushByte();
    
private:
    std::vector<uint8_t> buffer_;
    uint8_t current_byte_;
    uint8_t bit_pos_;
};

} // namespace mpeg2

#endif // MPEG2_BITSTREAM_H
