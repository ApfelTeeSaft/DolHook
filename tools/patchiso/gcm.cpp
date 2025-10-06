/**
 * GCM ISO Parser Implementation
 */

#include "gcm.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace dolhook {

static uint32_t read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static void write_be32(uint8_t* p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

bool GCMHeader::parse(const uint8_t* data) {
    std::memcpy(game_code, data, 6);
    std::memcpy(maker_code, data + 6, 2);
    disc_id = data[8];
    version = data[9];
    audio_streaming = data[10];
    stream_buf_size = data[11];
    
    // Game name at 0x20
    std::memcpy(game_name, data + 0x20, sizeof(game_name));
    game_name[sizeof(game_name) - 1] = 0; // Ensure null termination
    
    // Main.dol offset at 0x420
    dol_offset = read_be32(data + 0x420);
    fst_offset = read_be32(data + 0x424);
    fst_size = read_be32(data + 0x428);
    fst_max_size = read_be32(data + 0x42C);
    
    return is_valid();
}

void GCMHeader::serialize(uint8_t* data) const {
    std::memcpy(data, game_code, 6);
    std::memcpy(data + 6, maker_code, 2);
    data[8] = disc_id;
    data[9] = version;
    data[10] = audio_streaming;
    data[11] = stream_buf_size;
    
    std::memcpy(data + 0x20, game_name, sizeof(game_name));
    
    write_be32(data + 0x420, dol_offset);
    write_be32(data + 0x424, fst_offset);
    write_be32(data + 0x428, fst_size);
    write_be32(data + 0x42C, fst_max_size);
}

bool GCMHeader::is_valid() const {
    // Check magic/game code sanity
    if (game_code[0] == 0) return false;
    
    // DOL offset should be reasonable
    if (dol_offset < SIZE || dol_offset > 0x10000000) return false;
    
    // FST offset should be after DOL
    if (fst_offset < dol_offset || fst_offset > 0x10000000) return false;
    
    return true;
}

std::string GCMHeader::format() const {
    std::ostringstream oss;
    oss << "GCM Header:\n";
    oss << "  Game: " << std::string(game_name, strnlen(game_name, sizeof(game_name))) << "\n";
    oss << "  Code: " << std::string(game_code, 4) << "\n";
    oss << "  Maker: " << std::string(maker_code, 2) << "\n";
    oss << std::hex << std::setfill('0');
    oss << "  DOL Offset: 0x" << std::setw(8) << dol_offset << "\n";
    oss << "  FST Offset: 0x" << std::setw(8) << fst_offset << "\n";
    oss << "  FST Size: 0x" << std::setw(8) << fst_size << "\n";
    return oss.str();
}

bool GCMFile::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    data_.resize(size);
    file.read(reinterpret_cast<char*>(data_.data()), size);
    
    if (!file) return false;
    
    path_ = path;
    return header_.parse(data_.data());
}

bool GCMFile::save(const std::string& path) {
    // Update header in data
    if (data_.size() < GCMHeader::SIZE) {
        return false;
    }
    header_.serialize(data_.data());
    
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    
    file.write(reinterpret_cast<const char*>(data_.data()), data_.size());
    
    return file.good();
}

bool GCMFile::create_backup(const std::string& original_path) {
    std::string backup_path = original_path + ".bak";
    
    try {
        std::filesystem::copy_file(
            original_path,
            backup_path,
            std::filesystem::copy_options::skip_existing
        );
        return true;
    } catch (...) {
        return false;
    }
}

DOLFile GCMFile::read_dol() const {
    DOLFile dol;
    
    // Read DOL data starting at offset
    uint32_t dol_start = header_.dol_offset;
    
    // Read at least header first
    if (dol_start + 0x100 > data_.size()) {
        return dol;
    }
    
    // Parse header to determine full size
    DOLHeader temp_header;
    if (!temp_header.parse(data_.data() + dol_start)) {
        return dol;
    }
    
    // Find end of DOL (highest file offset + size)
    uint32_t dol_end = 0x100; // At least header
    auto sections = temp_header.get_sections();
    for (const auto& sec : sections) {
        uint32_t sec_end = sec.file_offset + sec.size;
        if (sec_end > dol_end) dol_end = sec_end;
    }
    
    // Extract DOL data
    std::vector<uint8_t> dol_data(
        data_.begin() + dol_start,
        data_.begin() + dol_start + dol_end
    );
    
    dol.load(dol_data);
    return dol;
}

bool GCMFile::write_dol(const DOLFile& dol) {
    auto dol_data = dol.save();
    uint32_t dol_start = header_.dol_offset;
    
    // Check if it fits in place
    uint32_t available = header_.fst_offset - dol_start;
    if (dol_data.size() > available) {
        return false; // Need to relocate
    }
    
    // Write in place
    std::memcpy(data_.data() + dol_start, dol_data.data(), dol_data.size());
    
    return true;
}

bool GCMFile::relocate_dol(const DOLFile& dol) {
    auto dol_data = dol.save();
    
    // Align to 0x8000 boundary at end of ISO
    uint32_t new_offset = (data_.size() + 0x7FFF) & ~0x7FFF;
    
    // Expand ISO
    data_.resize(new_offset + dol_data.size());
    
    // Write DOL
    std::memcpy(data_.data() + new_offset, dol_data.data(), dol_data.size());
    
    // Update header
    header_.dol_offset = new_offset;
    
    return true;
}

std::vector<uint8_t> GCMFile::read(uint32_t offset, uint32_t size) const {
    if (offset + size > data_.size()) {
        return {};
    }
    return std::vector<uint8_t>(data_.begin() + offset, data_.begin() + offset + size);
}

bool GCMFile::write(uint32_t offset, const std::vector<uint8_t>& data) {
    if (offset + data.size() > data_.size()) {
        data_.resize(offset + data.size());
    }
    std::memcpy(data_.data() + offset, data.data(), data.size());
    return true;
}

} // namespace dolhook