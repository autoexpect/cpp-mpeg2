#include "bitstream.h"

namespace mpeg2 {

BitStream::BitStream(const uint8_t* data, size_t size)
    : data_(data), size_(size), byte_pos_(0), bit_pos_(0) {
}

BitStream::BitStream(const std::vector<uint8_t>& data)
    : data_(data.data()), size_(data.size()), byte_pos_(0), bit_pos_(0) {
}

uint32_t BitStream::Uint8(uint32_t bits) {
    if (bits > 8 || bits == 0) {
        throw std::runtime_error("Invalid bit count for Uint8");
    }
    return static_cast<uint32_t>(Uint32(bits));
}

uint32_t BitStream::Uint16(uint32_t bits) {
    if (bits > 16 || bits == 0) {
        throw std::runtime_error("Invalid bit count for Uint16");
    }
    return static_cast<uint32_t>(Uint32(bits));
}

uint32_t BitStream::Uint32(uint32_t bits) {
    if (bits > 32 || bits == 0) {
        throw std::runtime_error("Invalid bit count for Uint32");
    }
    
    uint32_t result = 0;
    for (uint32_t i = 0; i < bits; i++) {
        if (byte_pos_ >= size_) {
            throw std::runtime_error("BitStream: insufficient data");
        }
        
        uint8_t bit = (data_[byte_pos_] >> (7 - bit_pos_)) & 1;
        result = (result << 1) | bit;
        
        bit_pos_++;
        if (bit_pos_ == 8) {
            bit_pos_ = 0;
            byte_pos_++;
        }
    }
    
    return result;
}

uint64_t BitStream::Uint64(uint32_t bits) {
    if (bits > 64 || bits == 0) {
        throw std::runtime_error("Invalid bit count for Uint64");
    }
    
    uint64_t result = 0;
    for (uint32_t i = 0; i < bits; i++) {
        if (byte_pos_ >= size_) {
            throw std::runtime_error("BitStream: insufficient data");
        }
        
        uint8_t bit = (data_[byte_pos_] >> (7 - bit_pos_)) & 1;
        result = (result << 1) | bit;
        
        bit_pos_++;
        if (bit_pos_ == 8) {
            bit_pos_ = 0;
            byte_pos_++;
        }
    }
    
    return result;
}

uint32_t BitStream::NextBits(uint32_t bits) {
    size_t saved_byte_pos = byte_pos_;
    uint8_t saved_bit_pos = bit_pos_;
    
    uint32_t result = Uint32(bits);
    
    byte_pos_ = saved_byte_pos;
    bit_pos_ = saved_bit_pos;
    
    return result;
}

void BitStream::SkipBits(uint32_t bits) {
    for (uint32_t i = 0; i < bits; i++) {
        if (byte_pos_ >= size_) {
            throw std::runtime_error("BitStream: insufficient data");
        }
        
        bit_pos_++;
        if (bit_pos_ == 8) {
            bit_pos_ = 0;
            byte_pos_++;
        }
    }
}

size_t BitStream::RemainBytes() const {
    if (byte_pos_ >= size_) {
        return 0;
    }
    size_t remain = size_ - byte_pos_;
    if (bit_pos_ > 0) {
        remain--;
    }
    return remain;
}

size_t BitStream::RemainBits() const {
    if (byte_pos_ >= size_) {
        return 0;
    }
    size_t remain_bytes = size_ - byte_pos_;
    return remain_bytes * 8 - bit_pos_;
}

const uint8_t* BitStream::RemainData() const {
    if (byte_pos_ >= size_) {
        return nullptr;
    }
    return data_ + byte_pos_;
}

bool BitStream::EOS() const {
    return byte_pos_ >= size_ || (byte_pos_ == size_ - 1 && bit_pos_ >= 8);
}

// BitStreamWriter implementation

BitStreamWriter::BitStreamWriter(size_t capacity)
    : current_byte_(0), bit_pos_(0) {
    buffer_.reserve(capacity);
}

void BitStreamWriter::PutByte(uint8_t val) {
    if (bit_pos_ != 0) {
        FlushByte();
    }
    buffer_.push_back(val);
}

void BitStreamWriter::PutUint8(uint8_t val, uint32_t bits) {
    if (bits > 8 || bits == 0) {
        throw std::runtime_error("Invalid bit count for PutUint8");
    }
    PutUint32(val, bits);
}

void BitStreamWriter::PutUint16(uint16_t val, uint32_t bits) {
    if (bits > 16 || bits == 0) {
        throw std::runtime_error("Invalid bit count for PutUint16");
    }
    PutUint32(val, bits);
}

void BitStreamWriter::PutUint32(uint32_t val, uint32_t bits) {
    if (bits > 32 || bits == 0) {
        throw std::runtime_error("Invalid bit count for PutUint32");
    }
    
    for (int i = bits - 1; i >= 0; i--) {
        uint8_t bit = (val >> i) & 1;
        current_byte_ = (current_byte_ << 1) | bit;
        bit_pos_++;
        
        if (bit_pos_ == 8) {
            buffer_.push_back(current_byte_);
            current_byte_ = 0;
            bit_pos_ = 0;
        }
    }
}

void BitStreamWriter::PutUint64(uint64_t val, uint32_t bits) {
    if (bits > 64 || bits == 0) {
        throw std::runtime_error("Invalid bit count for PutUint64");
    }
    
    for (int i = bits - 1; i >= 0; i--) {
        uint8_t bit = (val >> i) & 1;
        current_byte_ = (current_byte_ << 1) | bit;
        bit_pos_++;
        
        if (bit_pos_ == 8) {
            buffer_.push_back(current_byte_);
            current_byte_ = 0;
            bit_pos_ = 0;
        }
    }
}

void BitStreamWriter::PutBytes(const uint8_t* data, size_t size) {
    if (bit_pos_ != 0) {
        FlushByte();
    }
    buffer_.insert(buffer_.end(), data, data + size);
}

void BitStreamWriter::Reset() {
    buffer_.clear();
    current_byte_ = 0;
    bit_pos_ = 0;
}

void BitStreamWriter::FlushByte() {
    if (bit_pos_ > 0) {
        current_byte_ <<= (8 - bit_pos_);
        buffer_.push_back(current_byte_);
        current_byte_ = 0;
        bit_pos_ = 0;
    }
}

std::vector<uint8_t> BitStreamWriter::Bytes() {
    FlushByte();
    return buffer_;
}

void BitStreamWriter::FillRemainData(uint8_t value) {
    FlushByte();
    // Fill any remaining capacity with the specified value
    // This is used for TS packet stuffing
    size_t current_size = buffer_.size();
    if (buffer_.capacity() > current_size) {
        buffer_.resize(buffer_.capacity(), value);
    }
}

} // namespace mpeg2
