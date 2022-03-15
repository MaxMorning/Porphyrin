//
// Project: Porphyrin
// File Name: x86Assemble.h
// Author: Morning
// Description:
//
// Create Date: 2022/2/26
//

#ifndef PORPHYRIN_X86ASSEMBLE_H
#define PORPHYRIN_X86ASSEMBLE_H

#include <fstream>
#include <string>

#define TARGET_ARCH_X86 0
#define TARGET_ARCH_X64 1

#define X86UsableGPRCnt 6
#define X64UsableGPRCnt 14

using namespace std;

enum X86GPR {
    EAX,
    EBX,
    ECX,
    EDX,

    ESI,
    EDI
};

enum X64GPR {
    RAX,
    RBX,
    RCX,
    RDX,

    RSI,
    RDI,

    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15
};

enum PointerRegister {
    SP,
    BP,

    IP
};

const string GPRStr[] = {
        "AX",
        "BX",
        "CX",
        "DX",

        "SI",
        "DI"
};

const string PointerRegisterStr[] = {
        "SP",
        "BP",

        "IP"
};

enum SegmentRegister {
    ES,
    SS,
    DS,
    CS,
    FS,
    GS
};

void generate_target_asm(string& target_string_str, int target_arch);

void write_target_code(string& target_string_stream, ofstream& output_file);

#endif //PORPHYRIN_X86ASSEMBLE_H
