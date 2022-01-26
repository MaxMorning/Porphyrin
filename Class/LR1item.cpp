//
// Project: Porphyrin
// File Name: LR1item.cpp.cc
// Author: Wang
// Description:
//
// Create Date: 2021/11/2
//

#include "LR1item.h"

vector<LR1item*> LR1item::all_lr_1_item_set;
vector<int> LR1item::foresee_char_buffer;

int LR1item::get_foresee_char()
{
    foresee_char_buffer.clear();
    bool flag = true;
    int v_now = dot_position;       // position of lex now
    while(flag) {
        flag = false;
        int v_nxt = v_now + 1;      // position of lex next
        // A -> [B·C, a], A -> [BC·, a] ( first(a) = a )
        if(v_nxt >= Nonterminal::all_nonterminal_chars[index].productions[production_index].size()) {
            foresee_char_buffer.push_back(foresee_char_index);
            return foresee_char_buffer.size();
        }
        Character* next_char = &Nonterminal::all_nonterminal_chars[index].productions[production_index][v_nxt];
        // A -> [B·Cc, a], A -> [BC·c, c] ( first(ca) = c )
        if(next_char->is_terminal) {   // terminator
            foresee_char_buffer.push_back(next_char->index);
        }
            // A -> [B·CD, a], A -> [BC·D, b] ( b = first(Da) )
        else if(!next_char->is_terminal) {  // not terminator
            Nonterminal* next_nonterminal_char = &Nonterminal::all_nonterminal_chars[next_char->index];
            for (int i = 0; i < next_nonterminal_char->first_set.size(); i ++) {
                foresee_char_buffer.push_back(next_nonterminal_char->first_set[i].index);
            }

            // D -> epsilon
            if(Nonterminal::all_nonterminal_chars[index].productions[production_index][v_nxt].epsilon_in_firstset) {
                ++v_now;
                flag = true;
            }
        }
    }
    return foresee_char_buffer.size();
}

LR1item::LR1item()
{
    this->dot_position = -1;
    this->is_reduction = false;
}


LR1item::LR1item(int nonterminal_index, int production_index, int dot_position, int foresee_char_index) {
    this->index = nonterminal_index;
    this->is_reduction = false;
    this->production_index = production_index;
    this->dot_position = dot_position;
    this->foresee_char_index = foresee_char_index;
}

void LR1item::print() {
    cout << Nonterminal::all_nonterminal_chars[this->index].token << " -> [ ";
    for(int i = 0; i < Nonterminal::all_nonterminal_chars[this->index].productions[this->production_index].size(); ++i) {
        if(dot_position == i) {
            cout << ". ";
        }
        cout << Nonterminal::all_nonterminal_chars[this->index].productions[this->production_index][i].token << " ";
    }
    if(dot_position == Nonterminal::all_nonterminal_chars[this->index].productions[this->production_index].size()) {
        cout << ". ";
    }
    cout << ", " << TerminalChar::all_terminal_chars[this->foresee_char_index].token << "]" << endl;
}

LR1item &LR1item::operator=(const LR1item& c)
{
    if (this == &c) {
        return *this;
    }
    this->production_index = c.production_index;
    this->foresee_char_index = c.foresee_char_index;
    this->dot_position = c.dot_position;
    this->index = c.index;
    this->is_reduction = c.is_reduction;
    return *this;
}

bool LR1item::operator==(const LR1item& c)
{
    if (this->index == c.index
        && this->production_index == c.production_index
        && this->dot_position == c.dot_position
        && this->foresee_char_index == c.foresee_char_index) {
        return true;
    }
    return false;
}

int LR1item::is_LR1item_exist(LR1item item) {
    for (int i = 0; i < LR1item::all_lr_1_item_set.size(); ++i) {
        if(*LR1item::all_lr_1_item_set[i] == item) {
            return i;
        }
    }
    return -1;
}