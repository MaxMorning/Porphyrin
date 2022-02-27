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

enum X86Register {
    EAX,
    EBX,
    ECX,
    EDX,

    ESI,
    EDI,

    ESP,
    EBP,

    EIP
};

enum X86SegmentRegister {
    ES,
    SS,
    DS,
    CS,
    FS,
    GS
};
#endif //PORPHYRIN_X86ASSEMBLE_H
