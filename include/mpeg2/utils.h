#ifndef MPEG2_UTILS_H
#define MPEG2_UTILS_H

#include <cstdint>
#include <vector>

namespace mpeg2 {

uint32_t CalcCrc32(uint32_t crc, const uint8_t* data, size_t len);
uint32_t CalcCrc32(uint32_t crc, const std::vector<uint8_t>& data);

} // namespace mpeg2

#endif // MPEG2_UTILS_H
