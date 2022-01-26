//
// Project: Porphyrin
// File Name: LR1Table.cpp.cc
// Author: Morning
// Description:
//
// Create Date: 2021/11/3
//

#include "LR1Table.h"

int** LR1Table::transfer_table;

int** LR1Table::reduction_result_table;

int** LR1Table::nonterminal_transfer_table;

vector<Nonterminal> Nonterminal::all_nonterminal_chars;

int LR1Table::analysis(int current_state, const TerminalChar& next_char, int &reduction_idx, Nonterminal*& reduction_result) {
    int reduction_result_idx = reduction_result_table[current_state][next_char.index];

    if (reduction_result_idx >= 0) {
        // reduction
        reduction_idx = transfer_table[current_state][next_char.index];
        reduction_result = &Nonterminal::all_nonterminal_chars[reduction_result_idx];
    }
    else {
        // push
        reduction_idx = -1;
        reduction_result = nullptr;
    }
    return transfer_table[current_state][next_char.index];
}
