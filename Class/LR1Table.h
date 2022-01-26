//
// Project: Porphyrin
// File Name: LR1Table.h
// Author: Morning
// Description:
//
// Create Date: 2021/11/3
//

#ifndef PORPHYRIN_LR1TABLE_H
#define PORPHYRIN_LR1TABLE_H

#include "Nonterminal.h"
#include "LR1item.h"

class LR1Table {
public:
    // current_state aka closure index
    // return -1 means invalid code
    // return -2 means acc
    // return -3 means reduction
    static int analysis(int current_state, const TerminalChar& next_char, int& reduction_idx, Nonterminal*& reduction_result);

    // for push opr, means next status
    // for reduction, means the index in Nonterminal::productions
    static int** transfer_table;

    static int** nonterminal_transfer_table;

    // the index in Nonterminal::all_nonterminal_chars
    static int** reduction_result_table;
};


#endif //PORPHYRIN_LR1TABLE_H
