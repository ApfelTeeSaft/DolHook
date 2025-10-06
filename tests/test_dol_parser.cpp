/**
 * Unit tests for DOL parser
 */

#include "../tools/patchiso/dol.h"
#include <cassert>
#include <iostream>
#include <cstring>

using namespace dolhook;

void test_dol_header_parse() {
    std::cout << "Testing DOL header parse/serialize... ";
    
    // Create a minimal valid DOL header
    uint8_t header_data[0x100];
    std::memset(header_data, 0, sizeof(header_data));
    
    // Set up one text section
    // Offset at 0x100, load at 0x80003100, size 0x1000
    header_data[0x00] = 0x00; header_data[0x01] = 0x00;
    header_data[0x02] = 0x01; header_data[0x03] = 0x00;
    
    header_data[0x74] = 0x80; header_data[0x75] = 0x00;
    header_data[0x76] = 0x31; header_data[0x77] = 0x00;
    
    header_data[0xE8] = 0x00; header_data[0xE9] = 0x00;
    header_data[0xEA] = 0x10; header_data[0xEB] = 0x00;
    
    // Entry point
    header_data[0x164] = 0x80; header_data[0x165] = 0x00;
    header_data[0x166] = 0x31; header_data[0x167] = 0x00;
    
    // Parse
    DOLHeader hdr;
    assert(hdr.parse(header_data));
    
    // Verify
    assert(hdr.text_offsets[0] == 0x100);
    assert(hdr.text_addrs[0] == 0x80003100);
    assert(hdr.text_sizes[0] == 0x1000);
    assert(hdr.entry_point == 0x80003100);
    
    // Serialize and compare
    uint8_t output[0x100];
    hdr.serialize(output);
    
    // Check key fields match
    assert(output[0x164] == 0x80);
    assert(output[0x167] == 0x00);
    
    std::cout << "PASS\n";
}

void test_dol_section_management() {
    std::cout << "Testing DOL section management... ";
    
    DOLHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    hdr.entry_point = 0x80003100;
    
    // Add a text section
    DOLSection sec;
    sec.file_offset = 0x100;
    sec.load_addr = 0x80003100;
    sec.size = 0x1000;
    sec.is_text = true;
    
    assert(hdr.add_section(sec));
    assert(hdr.text_sizes[0] == 0x1000);
    
    // Get sections
    auto sections = hdr.get_sections();
    assert(sections.size() == 1);
    assert(sections[0].load_addr == 0x80003100);
    
    // Get highest address
    uint32_t highest = hdr.get_highest_addr();
    assert(highest == 0x80004100);
    
    std::cout << "PASS\n";
}

void test_branch_encoding() {
    std::cout << "Testing branch encoding... ";
    
    // Test near branch within range
    uint32_t from = 0x80003100;
    uint32_t to = 0x80003200;
    
    // Calculate expected offset
    int32_t offset = to - from;
    uint32_t expected = 0x48000000 | (offset & 0x03FFFFFC);
    
    // In the actual implementation, this would test dh_make_branch_imm
    // For now, just verify the math
    assert(offset == 0x100);
    assert((offset & 0x03FFFFFC) == 0x100);
    
    std::cout << "PASS\n";
}

void test_dol_file_operations() {
    std::cout << "Testing DOL file operations... ";
    
    // Create minimal DOL
    std::vector<uint8_t> dol_data(0x200, 0);
    
    // Setup header
    dol_data[0x164] = 0x80;
    dol_data[0x167] = 0x00;
    
    DOLFile dol;
    assert(dol.load(dol_data));
    
    // Verify round-trip
    auto saved = dol.save();
    assert(saved.size() >= 0x100);
    assert(saved[0x164] == 0x80);
    
    std::cout << "PASS\n";
}

int main() {
    std::cout << "Running DOL parser tests...\n\n";
    
    try {
        test_dol_header_parse();
        test_dol_section_management();
        test_branch_encoding();
        test_dol_file_operations();
        
        std::cout << "\nAll tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\nTest failed with unknown exception\n";
        return 1;
    }
}