#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>

uint32_t crc32_table[256];

void init_crc32() {
    uint32_t poly = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ poly;
            } else {
                crc = crc >> 1;
            }
        }
        crc32_table[i] = crc;
    }
}

uint32_t calculate_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc; // No final XOR for MPEG-2? Or yes?
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <ts_file>" << std::endl;
        return 1;
    }

    init_crc32();

    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open file" << std::endl;
        return 1;
    }

    std::vector<uint8_t> packet(188);
    int packet_idx = 0;
    while (in.read((char*)packet.data(), 188)) {
        if (packet[0] != 0x47) {
            std::cerr << "Sync byte missing at packet " << packet_idx << std::endl;
            continue;
        }

        uint16_t pid = ((packet[1] & 0x1F) << 8) | packet[2];
        bool pusi = (packet[1] & 0x40) != 0;

        if (pusi && (pid == 0 || pid == 0x200)) { // PAT or PMT
            // Parse section
            int offset = 4;
            if ((packet[3] & 0x20)) { // Adaptation field
                offset += packet[4] + 1;
            }
            if (packet[3] & 0x10) { // Payload
                offset += packet[offset] + 1; // Pointer field
                
                uint8_t table_id = packet[offset];
                uint16_t section_length = ((packet[offset + 1] & 0x0F) << 8) | packet[offset + 2];
                
                std::cout << "Packet " << packet_idx << " PID " << pid << " TableID " << (int)table_id << " Len " << section_length << std::endl;

                if (offset + 3 + section_length > 188) {
                    std::cout << "Section spans multiple packets, skipping CRC check" << std::endl;
                    continue;
                }

                // CRC is last 4 bytes of section
                // Calculate CRC over section data (excluding CRC field? No, including, should be 0? Or excluding and compare?)
                // MPEG-2: CRC is calculated over the entire section.
                // If we calculate over (section - 4 bytes), result should match CRC field.
                
                uint32_t calc = calculate_crc32(&packet[offset], section_length - 1); // -4 bytes CRC + 3 bytes header = length - 1?
                // section_length includes header? No.
                // Section syntax:
                // table_id (1)
                // section_length (2)
                // ... data ...
                // CRC (4)
                // section_length is number of bytes immediately following section_length field.
                // So total section size = 3 + section_length.
                
                size_t total_len = 3 + section_length;
                uint32_t calc_crc = calculate_crc32(&packet[offset], total_len - 4);
                
                uint32_t file_crc = (packet[offset + total_len - 4] << 24) |
                                    (packet[offset + total_len - 3] << 16) |
                                    (packet[offset + total_len - 2] << 8) |
                                    packet[offset + total_len - 1]; // Big Endian? No, Little Endian in file?
                                    
                // In file dump: e5 15 a3 5b.
                // My code writes: PutByte(crc & 0xFF)...
                // So Little Endian.
                uint32_t file_crc_le = (packet[offset + total_len - 1] << 24) |
                                       (packet[offset + total_len - 2] << 16) |
                                       (packet[offset + total_len - 3] << 8) |
                                       packet[offset + total_len - 4];

                std::cout << "Calculated CRC: " << std::hex << calc_crc << std::endl;
                std::cout << "File CRC (LE): " << std::hex << file_crc_le << std::endl;
                
                if (calc_crc == file_crc_le) {
                    std::cout << "CRC MATCH!" << std::endl;
                } else {
                    std::cout << "CRC MISMATCH!" << std::endl;
                }
            }
        }
        packet_idx++;
    }
    return 0;
}
