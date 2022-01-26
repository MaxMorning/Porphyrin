//
// Project: Porphyrin
// File Name: LR1item.h
// Author: Wang
// Description:
//
// Create Date: 2021/11/2
//

#ifndef PORPHYRIN_LR1ITEM_H
#define PORPHYRIN_LR1ITEM_H

#include <vector>
#include "Character.h"
#include "TerminalChar.h"
#include "Nonterminal.h"

using namespace std;

class LR1item {
public:
    int production_index; // the index of Nonterminal::all_nonterminal[index].production
    int dot_position; // the position of dot in the item
    int index; // the index of Nonterminal::all_nonterminal
    bool is_reduction;

    int foresee_char_index; // the index of TerminalChar::all_terminal

    int get_foresee_char();

    LR1item();
//    LR1item(const LR1item& src);
    LR1item(int nonterminal_index, int production_index, int dot_position, int foresee_char_index);

    static vector<int> foresee_char_buffer; // store the index of terminal chars

    static vector<LR1item*> all_lr_1_item_set;

    bool operator==(const LR1item& c);

    LR1item &operator = (const LR1item& c);

    void print();

    static int is_LR1item_exist(LR1item item);
};


#endif //PORPHYRIN_LR1ITEM_H
