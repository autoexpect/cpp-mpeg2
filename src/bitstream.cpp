#include "mpeg2/bitstream.h"
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace mpeg2 {

static const uint8_t BitMask[8] = {0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF};

BitStream::BitStream(const uint8_t* buf, size_t size)
    : bits_(buf), size_(size), bytesOffset_(0), bitsOffset_(0) {}

BitStream::~BitStream() {}

uint8_t BitStream::GetBit() {
    if (bytesOffset_ >= size_) {
        throw std::out_of_range("OUT OF RANGE");
    }
    uint8_t ret = (bits_[bytesOffset_] >> (7 - bitsOffset_)) & 0x01;
    bitsOffset_++;
    if (bitsOffset_ >= 8) {
        bytesOffset_++;
        bitsOffset_ = 0;
    }
    return ret;
}

uint64_t BitStream::GetBits(int n) {
    if (bytesOffset_ >= size_) {
        throw std::out_of_range("OUT OF RANGE");
    }
    uint64_t ret = 0;
    if (8 - bitsOffset_ >= n) {
        ret = (uint64_t)((bits_[bytesOffset_] >> (8 - bitsOffset_ - n)) & BitMask[n - 1]);
        bitsOffset_ += n;
        if (bitsOffset_ == 8) {
            bytesOffset_++;
            bitsOffset_ = 0;
        }
    } else {
        ret = (uint64_t)(bits_[bytesOffset_] & BitMask[8 - bitsOffset_ - 1]);
        bytesOffset_++;
        n -= 8 - bitsOffset_;
        bitsOffset_ = 0;
        while (n > 0) {
            if (bytesOffset_ >= size_) {
                throw std::out_of_range("OUT OF RANGE");
            }
            if (n >= 8) {
                ret = (ret << 8) | (uint64_t)(bits_[bytesOffset_]);
                bytesOffset_++;
                n -= 8;
            } else {
                ret = (ret << n) | (uint64_t)((bits_[bytesOffset_] >> (8 - n)) & BitMask[n - 1]);
                bitsOffset_ = n;
                break;
            }
        }
    }
    return ret;
}

void BitStream::SkipBits(int n) {
    int bytecount = n / 8;
    int bitscount = n % 8;
    bytesOffset_ += bytecount;
    if (bitsOffset_ + bitscount < 8) {
        bitsOffset_ += bitscount;
    } else {
        bytesOffset_ += 1;
        bitsOffset_ += bitscount - 8;
    }
}

uint8_t BitStream::Uint8(int n) {
    return (uint8_t)GetBits(n);
}

uint16_t BitStream::Uint16(int n) {
    return (uint16_t)GetBits(n);
}

uint32_t BitStream::Uint32(int n) {
    return (uint32_t)GetBits(n);
}

std::vector<uint8_t> BitStream::GetBytes(int n) {
    if (bytesOffset_ + n > size_) {
        throw std::out_of_range("OUT OF RANGE");
    }
    if (bitsOffset_ != 0) {
        throw std::runtime_error("invalid operation: bitsOffset != 0");
    }
    std::vector<uint8_t> data(bits_ + bytesOffset_, bits_ + bytesOffset_ + n);
    bytesOffset_ += n;
    return data;
}

uint64_t BitStream::ReadUE() {
    int leadingZeroBits = 0;
    while (GetBit() == 0) {
        leadingZeroBits++;
    }
    if (leadingZeroBits == 0) {
        return 0;
    }
    uint64_t info = GetBits(leadingZeroBits);
    return ((uint64_t)1 << leadingZeroBits) - 1 + info;
}

int64_t BitStream::ReadSE() {
    uint64_t v = ReadUE();
    if (v % 2 == 0) {
        return -1 * (int64_t)(v / 2);
    } else {
        return (int64_t)(v + 1) / 2;
    }
}

int BitStream::RemainBits() {
    if (bitsOffset_ > 0) {
        return (size_ - bytesOffset_ - 1) * 8 + (8 - bitsOffset_);
    } else {
        return (size_ - bytesOffset_) * 8;
    }
}

int BitStream::RemainBytes() {
    if (bitsOffset_ > 0) {
        return size_ - bytesOffset_ - 1;
    } else {
        return size_ - bytesOffset_;
    }
}

BitStreamWriter::BitStreamWriter(int n)
    : bits_(n), byteOffset_(0), bitOffset_(0) {
        std::fill(bits_.begin(), bits_.end(), 0);
    }

BitStreamWriter::~BitStreamWriter() {}

void BitStreamWriter::expandSpace(int n) {
    if ((bits_.size() - byteOffset_ - 1) * 8 + 8 - bitOffset_ < (size_t)n) {
        size_t newlen = 0;
        if (bits_.size() * 8 < (size_t)n) {
            newlen = bits_.size() + n / 8 + 1;
        } else {
            newlen = bits_.size() * 2;
        }
        bits_.resize(newlen, 0);
    }
}

void BitStreamWriter::PutByte(uint8_t v) {
    expandSpace(8);
    if (bitOffset_ == 0) {
        bits_[byteOffset_] = v;
        byteOffset_++;
    } else {
        bits_[byteOffset_] |= v >> bitOffset_;
        byteOffset_++;
        bits_[byteOffset_] = v & BitMask[bitOffset_ - 1];
    }
}

void BitStreamWriter::PutBytes(const uint8_t* v, size_t size) {
    if (bitOffset_ != 0) {
        throw std::runtime_error("bsw.bitsoffset > 0");
    }
    expandSpace(8 * size);
    std::memcpy(&bits_[byteOffset_], v, size);
    byteOffset_ += size;
}

void BitStreamWriter::PutBytes(const std::vector<uint8_t>& v) {
    PutBytes(v.data(), v.size());
}

void BitStreamWriter::PutUint8(uint8_t v, int n) {
    PutUint64((uint64_t)v, n);
}

void BitStreamWriter::PutUint16(uint16_t v, int n) {
    PutUint64((uint64_t)v, n);
}

void BitStreamWriter::PutUint32(uint32_t v, int n) {
    PutUint64((uint64_t)v, n);
}

void BitStreamWriter::PutUint64(uint64_t v, int n) {
    expandSpace(n);
    if (8 - bitOffset_ >= n) {
        bits_[byteOffset_] |= ((uint8_t)(v) & BitMask[n - 1]) << (8 - bitOffset_ - n);
        bitOffset_ += n;
        if (bitOffset_ == 8) {
            bitOffset_ = 0;
            byteOffset_++;
        }
    } else {
        bits_[byteOffset_] |= (uint8_t)(v >> (n - (8 - bitOffset_))) & BitMask[8 - bitOffset_ - 1];
        byteOffset_++;
        n -= 8 - bitOffset_;
        while (n - 8 >= 0) {
            bits_[byteOffset_] = (uint8_t)(v >> (n - 8)) & 0xFF;
            byteOffset_++;
            n -= 8;
        }
        bitOffset_ = n;
        if (n > 0) {
            bits_[byteOffset_] |= ((uint8_t)(v) & BitMask[n - 1]) << (8 - n);
        }
    }
}

void BitStreamWriter::SetByte(uint8_t v, size_t where) {
    if (where < bits_.size()) {
        bits_[where] = v;
    }
}

void BitStreamWriter::SetUint16(uint16_t v, size_t where) {
    if (where + 1 < bits_.size()) {
        bits_[where] = (v >> 8) & 0xFF;
        bits_[where + 1] = v & 0xFF;
    }
}

void BitStreamWriter::FillRemainData(uint8_t v) {
    for (size_t i = byteOffset_; i < bits_.size(); i++) {
        bits_[i] = v;
    }
    byteOffset_ = bits_.size();
    bitOffset_ = 0;
}

void BitStreamWriter::Reset() {
    std::fill(bits_.begin(), bits_.end(), 0);
    bitOffset_ = 0;
    byteOffset_ = 0;
}

const std::vector<uint8_t>& BitStreamWriter::Bits() const {
    return bits_;
}

size_t BitStreamWriter::Size() const {
    return byteOffset_ + (bitOffset_ > 0 ? 1 : 0);
}

} // namespace mpeg2
