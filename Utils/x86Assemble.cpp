//
// Project: Porphyrin
// File Name: x86Assemble.cpp
// Author: Morning
// Description:
//
// Create Date: 2022/2/26
//

#include "Include/x86Assemble.h"
#include "Include/Optimize.h"

#define INVALID_MEMORY_OFFSET 2147483647

void init_symbol_table_offset()
{
    for (SymbolEntry& symbol : symbol_table) {
        symbol.memory_offset = INVALID_MEMORY_OFFSET;
    }
}

void generate_target_text_asm(BaseBlock& base_block, string& target_text_str, int target_arch)
{
    throw "Not Implemented!";
}

// return string contains ".data"
void generate_target_data_asm(string& target_data_str)
{
    throw "Not Implemented!";
}

// return string contains ".bss"
void generate_target_bss_asm(string& target_bss_str)
{
    throw "Not Implemented!";
}

void generate_target_asm(string& target_string_str, int target_arch)
{
    init_symbol_table_offset();
    split_base_blocks(optimized_sequence);

    string text_seg_str;
    for (BaseBlock& base_block : base_blocks) {
        generate_target_text_asm(base_block, text_seg_str, target_arch);
    }

    string data_seg_str;
    generate_target_data_asm(data_seg_str);

    string bss_seg_str;
    generate_target_bss_asm(bss_seg_str);

    // concat all strings
    target_string_str = data_seg_str;
    target_string_str += '\n';

    target_string_str += bss_seg_str;
    target_string_str += '\n';

    target_string_str += text_seg_str;
    target_string_str += '\n';
}

void write_target_code(string& target_string_str, ofstream& output_file)
{
    throw "Not Implemented!";
}
