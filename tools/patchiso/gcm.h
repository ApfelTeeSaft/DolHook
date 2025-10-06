/**
 * GCM (GameCube Master) ISO Format
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "dol.h"

namespace dolhook {

struct GCMHeader {
    static constexpr size_t SIZE = 0x2440;
    
    char game_code[6];
    char maker_code[2];
    uint8_t disc_id;
    uint8_t version;
    uint8_t audio_streaming;
    uint8_t stream_buf_size;
    uint8_t unused[18];
    char game_name[0x3E0];
    uint32_t dol_offset;        // Offset to main.dol (0x420)
    uint32_t fst_offset;        // File system table offset (0x424)
    uint32_t fst_size;          // FST size (0x428)
    uint32_t fst_max_size;      // FST max size (0x42C)
    
    // Parse from buffer
    bool parse(const uint8_t* data);
    
    // Serialize to buffer
    void serialize(uint8_t* data) const;
    
    // Validation
    bool is_valid() const;
    
    // Format for display
    std::string format() const;
};

class GCMFile {
public:
    GCMFile() = default;
    
    // Load from file
    bool load(const std::string& path);
    
    // Save to file
    bool save(const std::string& path);
    
    // Create backup
    bool create_backup(const std::string& original_path);
    
    // Getters
    const GCMHeader& header() const { return header_; }
    GCMHeader& header() { return header_; }
    size_t size() const { return data_.size(); }
    
    // Read DOL from ISO
    DOLFile read_dol() const;
    
    // Write DOL back to ISO
    bool write_dol(const DOLFile& dol);
    
    // Write DOL to new location (end of ISO)
    bool relocate_dol(const DOLFile& dol);
    
    // Read arbitrary data
    std::vector<uint8_t> read(uint32_t offset, uint32_t size) const;
    
    // Write arbitrary data
    bool write(uint32_t offset, const std::vector<uint8_t>& data);
    
private:
    GCMHeader header_;
    std::vector<uint8_t> data_;
    std::string path_;
};

} // namespace dolhook