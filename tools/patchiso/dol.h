/**
 * DOL (Dolphin Executable) Format Parser
 * GameCube/Wii executable format
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace dolhook {

struct DOLSection {
    uint32_t file_offset;  // Offset in DOL file
    uint32_t load_addr;    // Virtual address to load at
    uint32_t size;         // Section size
    bool is_text;          // Text or data section
};

struct DOLHeader {
    static constexpr size_t MAX_TEXT_SECTIONS = 18;
    static constexpr size_t MAX_DATA_SECTIONS = 11;
    
    uint32_t text_offsets[MAX_TEXT_SECTIONS];
    uint32_t data_offsets[MAX_DATA_SECTIONS];
    uint32_t text_addrs[MAX_TEXT_SECTIONS];
    uint32_t data_addrs[MAX_DATA_SECTIONS];
    uint32_t text_sizes[MAX_TEXT_SECTIONS];
    uint32_t data_sizes[MAX_DATA_SECTIONS];
    uint32_t bss_addr;
    uint32_t bss_size;
    uint32_t entry_point;
    
    // Parse from big-endian buffer
    bool parse(const uint8_t* data);
    
    // Serialize to big-endian buffer
    void serialize(uint8_t* data) const;
    
    // Get all sections
    std::vector<DOLSection> get_sections() const;
    
    // Find highest address used
    uint32_t get_highest_addr() const;
    
    // Validation
    bool is_valid() const;
    
    // Add new section
    bool add_section(const DOLSection& sec);
};

class DOLFile {
public:
    DOLFile() = default;
    
    // Load from buffer
    bool load(const std::vector<uint8_t>& data);
    
    // Save to buffer
    std::vector<uint8_t> save() const;
    
    // Getters
    const DOLHeader& header() const { return header_; }
    DOLHeader& header() { return header_; }
    const std::vector<uint8_t>& data() const { return data_; }
    
    // Get section data
    std::vector<uint8_t> get_section_data(const DOLSection& sec) const;
    
    // Inject new code/data sections
    bool inject_payload(const std::vector<uint8_t>& payload,
                       uint32_t load_addr,
                       bool is_text);
    
    // Print header for debugging
    std::string format_header() const;
    
private:
    DOLHeader header_;
    std::vector<uint8_t> data_;
};

} // namespace dolhook