/**
 * DOL Parser Implementation
 */

#include "dol.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace dolhook {

// Read big-endian uint32
static uint32_t read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

// Write big-endian uint32
static void write_be32(uint8_t* p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

bool DOLHeader::parse(const uint8_t* data) {
    // Text section offsets (0x00)
    for (size_t i = 0; i < MAX_TEXT_SECTIONS; i++) {
        text_offsets[i] = read_be32(data + i * 4);
    }
    
    // Data section offsets (0x48)
    for (size_t i = 0; i < MAX_DATA_SECTIONS; i++) {
        data_offsets[i] = read_be32(data + 0x48 + i * 4);
    }
    
    // Text section addresses (0x74)
    for (size_t i = 0; i < MAX_TEXT_SECTIONS; i++) {
        text_addrs[i] = read_be32(data + 0x74 + i * 4);
    }
    
    // Data section addresses (0xBC)
    for (size_t i = 0; i < MAX_DATA_SECTIONS; i++) {
        data_addrs[i] = read_be32(data + 0xBC + i * 4);
    }
    
    // Text section sizes (0xE8)
    for (size_t i = 0; i < MAX_TEXT_SECTIONS; i++) {
        text_sizes[i] = read_be32(data + 0xE8 + i * 4);
    }
    
    // Data section sizes (0x130)
    for (size_t i = 0; i < MAX_DATA_SECTIONS; i++) {
        data_sizes[i] = read_be32(data + 0x130 + i * 4);
    }
    
    // BSS (0x15C)
    bss_addr = read_be32(data + 0x15C);
    bss_size = read_be32(data + 0x160);
    
    // Entry point (0x164)
    entry_point = read_be32(data + 0x164);
    
    return is_valid();
}

void DOLHeader::serialize(uint8_t* data) const {
    std::memset(data, 0, 0x100);
    
    for (size_t i = 0; i < MAX_TEXT_SECTIONS; i++) {
        write_be32(data + i * 4, text_offsets[i]);
    }
    
    for (size_t i = 0; i < MAX_DATA_SECTIONS; i++) {
        write_be32(data + 0x48 + i * 4, data_offsets[i]);
    }
    
    for (size_t i = 0; i < MAX_TEXT_SECTIONS; i++) {
        write_be32(data + 0x74 + i * 4, text_addrs[i]);
    }
    
    for (size_t i = 0; i < MAX_DATA_SECTIONS; i++) {
        write_be32(data + 0xBC + i * 4, data_addrs[i]);
    }
    
    for (size_t i = 0; i < MAX_TEXT_SECTIONS; i++) {
        write_be32(data + 0xE8 + i * 4, text_sizes[i]);
    }
    
    for (size_t i = 0; i < MAX_DATA_SECTIONS; i++) {
        write_be32(data + 0x130 + i * 4, data_sizes[i]);
    }
    
    write_be32(data + 0x15C, bss_addr);
    write_be32(data + 0x160, bss_size);
    write_be32(data + 0x164, entry_point);
}

std::vector<DOLSection> DOLHeader::get_sections() const {
    std::vector<DOLSection> sections;
    
    for (size_t i = 0; i < MAX_TEXT_SECTIONS; i++) {
        if (text_sizes[i] > 0) {
            sections.push_back({text_offsets[i], text_addrs[i], text_sizes[i], true});
        }
    }
    
    for (size_t i = 0; i < MAX_DATA_SECTIONS; i++) {
        if (data_sizes[i] > 0) {
            sections.push_back({data_offsets[i], data_addrs[i], data_sizes[i], false});
        }
    }
    
    return sections;
}

uint32_t DOLHeader::get_highest_addr() const {
    uint32_t highest = 0;
    
    for (size_t i = 0; i < MAX_TEXT_SECTIONS; i++) {
        if (text_sizes[i] > 0) {
            uint32_t end = text_addrs[i] + text_sizes[i];
            if (end > highest) highest = end;
        }
    }
    
    for (size_t i = 0; i < MAX_DATA_SECTIONS; i++) {
        if (data_sizes[i] > 0) {
            uint32_t end = data_addrs[i] + data_sizes[i];
            if (end > highest) highest = end;
        }
    }
    
    if (bss_size > 0) {
        uint32_t bss_end = bss_addr + bss_size;
        if (bss_end > highest) highest = bss_end;
    }
    
    return highest;
}

bool DOLHeader::is_valid() const {
    // Entry point should be in valid range
    if (entry_point < 0x80000000 || entry_point > 0x81800000) {
        return false;
    }
    
    // Check section alignment and ranges
    for (size_t i = 0; i < MAX_TEXT_SECTIONS; i++) {
        if (text_sizes[i] > 0) {
            if (text_offsets[i] < 0x100) return false; // Before header
            if (text_addrs[i] < 0x80000000) return false;
        }
    }
    
    return true;
}

bool DOLHeader::add_section(const DOLSection& sec) {
    if (sec.is_text) {
        for (size_t i = 0; i < MAX_TEXT_SECTIONS; i++) {
            if (text_sizes[i] == 0) {
                text_offsets[i] = sec.file_offset;
                text_addrs[i] = sec.load_addr;
                text_sizes[i] = sec.size;
                return true;
            }
        }
    } else {
        for (size_t i = 0; i < MAX_DATA_SECTIONS; i++) {
            if (data_sizes[i] == 0) {
                data_offsets[i] = sec.file_offset;
                data_addrs[i] = sec.load_addr;
                data_sizes[i] = sec.size;
                return true;
            }
        }
    }
    return false; // No free slots
}

bool DOLFile::load(const std::vector<uint8_t>& data) {
    if (data.size() < 0x100) {
        return false;
    }
    
    if (!header_.parse(data.data())) {
        return false;
    }
    
    data_ = data;
    return true;
}

std::vector<uint8_t> DOLFile::save() const {
    std::vector<uint8_t> result = data_;
    
    // Ensure header is at least 0x100 bytes
    if (result.size() < 0x100) {
        result.resize(0x100);
    }
    
    // Write header
    header_.serialize(result.data());
    
    return result;
}

std::vector<uint8_t> DOLFile::get_section_data(const DOLSection& sec) const {
    if (sec.file_offset + sec.size > data_.size()) {
        return {};
    }
    
    return std::vector<uint8_t>(
        data_.begin() + sec.file_offset,
        data_.begin() + sec.file_offset + sec.size
    );
}

bool DOLFile::inject_payload(const std::vector<uint8_t>& payload,
                             uint32_t load_addr,
                             bool is_text) {
    // Align file offset
    uint32_t file_offset = (data_.size() + 31) & ~31;
    
    // Expand data buffer
    data_.resize(file_offset + payload.size());
    
    // Copy payload
    std::memcpy(data_.data() + file_offset, payload.data(), payload.size());
    
    // Add section to header
    DOLSection sec;
    sec.file_offset = file_offset;
    sec.load_addr = load_addr;
    sec.size = payload.size();
    sec.is_text = is_text;
    
    return header_.add_section(sec);
}

std::string DOLFile::format_header() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    oss << "DOL Header:\n";
    oss << "  Entry Point: 0x" << std::setw(8) << header_.entry_point << "\n";
    oss << "  BSS: 0x" << std::setw(8) << header_.bss_addr 
        << " - 0x" << std::setw(8) << (header_.bss_addr + header_.bss_size)
        << " (size: 0x" << std::setw(8) << header_.bss_size << ")\n\n";
    
    oss << "Text Sections:\n";
    for (size_t i = 0; i < DOLHeader::MAX_TEXT_SECTIONS; i++) {
        if (header_.text_sizes[i] > 0) {
            oss << "  [" << i << "] File:0x" << std::setw(8) << header_.text_offsets[i]
                << " -> Addr:0x" << std::setw(8) << header_.text_addrs[i]
                << " Size:0x" << std::setw(8) << header_.text_sizes[i] << "\n";
        }
    }
    
    oss << "\nData Sections:\n";
    for (size_t i = 0; i < DOLHeader::MAX_DATA_SECTIONS; i++) {
        if (header_.data_sizes[i] > 0) {
            oss << "  [" << i << "] File:0x" << std::setw(8) << header_.data_offsets[i]
                << " -> Addr:0x" << std::setw(8) << header_.data_addrs[i]
                << " Size:0x" << std::setw(8) << header_.data_sizes[i] << "\n";
        }
    }
    
    return oss.str();
}

} // namespace dolhook