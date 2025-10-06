/**
 * DolHook ISO Patcher
 * Injects DolHook runtime into GameCube ISO
 */

#include "gcm.h"
#include "dol.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>

using namespace dolhook;

struct PatcherConfig {
    std::string input_iso;
    std::string output_iso;
    std::string game_id;
    int log_level = 1; // 0=errors, 1=info, 2=debug
    bool dry_run = false;
    bool print_dol = false;
};

struct SymbolMap {
    std::map<std::string, uint32_t> symbols;
    
    bool load(const std::string& path) {
        std::ifstream file(path);
        if (!file) return false;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            std::istringstream iss(line);
            std::string name;
            uint32_t addr;
            
            if (iss >> name >> std::hex >> addr) {
                symbols[name] = addr;
            }
        }
        
        return !symbols.empty();
    }
    
    bool has(const std::string& name) const {
        return symbols.find(name) != symbols.end();
    }
    
    uint32_t get(const std::string& name) const {
        auto it = symbols.find(name);
        return it != symbols.end() ? it->second : 0;
    }
};

void print_usage(const char* prog) {
    std::cout << "DolHook ISO Patcher v1.0\n\n";
    std::cout << "Usage: " << prog << " INPUT.iso [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --out FILE        Output ISO path (default: modify input after backup)\n";
    std::cout << "  --id GAMEID       Override game ID\n";
    std::cout << "  --log LEVEL       Log level: 0=errors, 1=info, 2=debug (default: 1)\n";
    std::cout << "  --dry-run         Parse only, don't write\n";
    std::cout << "  --print-dol       Display DOL section table\n";
    std::cout << "  --help            Show this help\n";
}

bool parse_args(int argc, char** argv, PatcherConfig& cfg) {
    if (argc < 2) return false;
    
    cfg.input_iso = argv[1];
    
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            return false;
        } else if (arg == "--out" && i + 1 < argc) {
            cfg.output_iso = argv[++i];
        } else if (arg == "--id" && i + 1 < argc) {
            cfg.game_id = argv[++i];
        } else if (arg == "--log" && i + 1 < argc) {
            cfg.log_level = std::atoi(argv[++i]);
        } else if (arg == "--dry-run") {
            cfg.dry_run = true;
        } else if (arg == "--print-dol") {
            cfg.print_dol = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }
    
    return true;
}

static void write_be32(uint8_t* p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

int main(int argc, char** argv) {
    PatcherConfig cfg;
    
    if (!parse_args(argc, argv, cfg)) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Load ISO
    if (cfg.log_level >= 1) {
        std::cout << "Loading ISO: " << cfg.input_iso << "\n";
    }
    
    GCMFile iso;
    if (!iso.load(cfg.input_iso)) {
        std::cerr << "Error: Failed to load ISO\n";
        return 1;
    }
    
    if (cfg.log_level >= 1) {
        std::cout << iso.header().format() << "\n";
    }
    
    // Read DOL
    DOLFile dol = iso.read_dol();
    if (cfg.log_level >= 2 || cfg.print_dol) {
        std::cout << dol.format_header() << "\n";
    }
    
    // Load payload
    if (cfg.log_level >= 1) {
        std::cout << "Loading payload...\n";
    }
    
    std::ifstream payload_file("payload/payload.bin", std::ios::binary);
    if (!payload_file) {
        std::cerr << "Error: payload/payload.bin not found\n";
        std::cerr << "Build the runtime first with 'make runtime'\n";
        return 1;
    }
    
    payload_file.seekg(0, std::ios::end);
    size_t payload_size = payload_file.tellg();
    payload_file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> payload(payload_size);
    payload_file.read(reinterpret_cast<char*>(payload.data()), payload_size);
    
    if (cfg.log_level >= 1) {
        std::cout << "  Payload size: " << payload_size << " bytes\n";
    }
    
    // Load symbol map
    SymbolMap symbols;
    if (!symbols.load("payload/payload.sym")) {
        std::cerr << "Warning: payload.sym not found, using defaults\n";
        // Set reasonable defaults
        symbols.symbols["__dolhook_entry"] = 0x80400000;
        symbols.symbols["__dolhook_original_entry"] = 0x80400100;
    }
    
    if (!symbols.has("__dolhook_entry")) {
        std::cerr << "Error: __dolhook_entry symbol not found\n";
        return 1;
    }
    
    uint32_t hook_entry = symbols.get("__dolhook_entry");
    uint32_t orig_entry_slot = symbols.get("__dolhook_original_entry");
    
    if (cfg.log_level >= 2) {
        std::cout << "  Hook entry: 0x" << std::hex << hook_entry << "\n";
        std::cout << "  Original entry slot: 0x" << std::hex << orig_entry_slot << "\n";
    }
    
    // Save original entry
    uint32_t original_entry = dol.header().entry_point;
    
    if (cfg.log_level >= 1) {
        std::cout << "\nPatching:\n";
        std::cout << "  Original entry: 0x" << std::hex << original_entry << "\n";
        std::cout << "  New entry: 0x" << std::hex << hook_entry << "\n";
    }
    
    // Find offset of __dolhook_original_entry in payload
    // For simplicity, scan for the placeholder value 0x80003100
    uint32_t placeholder = 0x80003100;
    size_t entry_offset = 0;
    bool found_slot = false;
    
    for (size_t i = 0; i + 4 <= payload.size(); i += 4) {
        uint32_t val = (payload[i] << 24) | (payload[i+1] << 16) | 
                       (payload[i+2] << 8) | payload[i+3];
        if (val == placeholder) {
            entry_offset = i;
            found_slot = true;
            break;
        }
    }
    
    if (!found_slot && cfg.log_level >= 1) {
        std::cout << "  Warning: Placeholder not found, appending entry data\n";
        entry_offset = payload.size();
        payload.resize(payload.size() + 4);
    }
    
    // Write original entry to payload
    write_be32(payload.data() + entry_offset, original_entry);
    
    if (cfg.log_level >= 2) {
        std::cout << "  Wrote original entry at payload offset: 0x" 
                  << std::hex << entry_offset << "\n";
    }
    
    if (cfg.dry_run) {
        std::cout << "\nDry run - no changes written\n";
        return 0;
    }
    
    // Choose load address (after highest existing section)
    uint32_t load_addr = (dol.header().get_highest_addr() + 0xFF) & ~0xFF;
    if (load_addr < 0x80400000) load_addr = 0x80400000;
    
    if (cfg.log_level >= 1) {
        std::cout << "  Loading payload at: 0x" << std::hex << load_addr << "\n";
    }
    
    // Inject payload as text section
    if (!dol.inject_payload(payload, load_addr, true)) {
        std::cerr << "Error: Failed to inject payload\n";
        return 1;
    }
    
    // Update entry point
    dol.header().entry_point = hook_entry;
    
    if (cfg.log_level >= 2) {
        std::cout << "\nModified DOL:\n" << dol.format_header() << "\n";
    }
    
    // Create backup
    if (cfg.output_iso.empty()) {
        if (cfg.log_level >= 1) {
            std::cout << "Creating backup...\n";
        }
        iso.create_backup(cfg.input_iso);
        cfg.output_iso = cfg.input_iso;
    }
    
    // Try to write DOL in place first
    bool wrote_inline = iso.write_dol(dol);
    
    if (!wrote_inline) {
        if (cfg.log_level >= 1) {
            std::cout << "DOL too large, relocating to end of ISO...\n";
        }
        iso.relocate_dol(dol);
    }
    
    // Write ISO
    if (cfg.log_level >= 1) {
        std::cout << "Writing patched ISO: " << cfg.output_iso << "\n";
    }
    
    if (!iso.save(cfg.output_iso)) {
        std::cerr << "Error: Failed to write ISO\n";
        return 1;
    }
    
    if (cfg.log_level >= 1) {
        std::cout << "\n✓ Patch complete!\n";
        std::cout << "  Original entry: 0x" << std::hex << original_entry << "\n";
        std::cout << "  New entry: 0x" << std::hex << hook_entry << "\n";
        std::cout << "  Payload size: " << std::dec << payload_size << " bytes\n";
    }
    
    return 0;
}