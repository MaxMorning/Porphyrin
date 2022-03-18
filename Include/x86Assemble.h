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

//enum GPR32bit {
//    EAX,
//    EBX,
//    ECX,
//    EDX,
//
//    ESI,
//    EDI,
//
//    R8D,
//    R9D,
//    R10D,
//    R11D,
//    R12D,
//    R13D,
//    R14D,
//    R15D
//};

enum GPR64bit {
    AX,
    BX,
    CX,
    DX,

    SI,
    DI,

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

#define GPR8bIDX    0
#define GPR16bIDX   1
#define GPR32bIDX   2
#define GPR64bIDX   3

const string GPRStr[][4] = {
        {"al", "ax", "eax", "rax"},
        {"bl", "bx", "ebx", "rbx"},
        {"cl", "cx", "ecx", "rcx"},
        {"dl", "dx", "edx", "rdx"},

        {"sil", "si", "esi", "rsi"},
        {"dil", "di", "edi", "rdi"},

        {"r8l", "r8w", "r8d", "r8"},
        {"r9l", "r9w", "r9d", "r9"},
        {"r10l", "r10w", "r10d", "r10"},
        {"r11l", "r11w", "r11d", "r11"},
        {"r12l", "r12w", "r12d", "r12"},
        {"r13l", "r13w", "r13d", "r13"},
        {"r14l", "r14w", "r14d", "r14"},
        {"r15l", "r15w", "r15d", "r15"}
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

enum SSEReg {
    XMM0,
    XMM1,
    XMM2,
    XMM3,
    XMM4,
    XMM5,
    XMM6,
    XMM7
};

const string XMMRegStr[] = {
        "xmm0",
        "xmm1",
        "xmm2",
        "xmm3",
        "xmm4",
        "xmm5",
        "xmm6",
        "xmm7",
        "xmm8",
        "xmm9",
        "xmm10",
        "xmm11",
        "xmm12",
        "xmm13",
        "xmm14",
        "xmm15"
};

struct QuaternionActiveInfo {
    int opr1_active_info;
    int opr2_active_info;
    int result_active_info;
};

void generate_target_asm(string& target_string_str, int target_arch);

void write_target_code(string& target_string_stream, ofstream& output_file);

#endif //PORPHYRIN_X86ASSEMBLE_H
