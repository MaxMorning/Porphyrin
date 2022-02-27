//
// Project: Porphyrin
// File Name: Closure.cpp.cc
// Author: Morning
// Description:
//
// Create Date: 2021/11/2
//

#include "Closure.h"

vector<Closure> Closure::all_closure_set;

bool Closure::operator==(const Closure &opr2) {
    if(this->items_number.size() != opr2.items_number.size()) {
        return false;
    }
    for(int i = 0; i < opr2.items_number.size(); i ++) {
        if(this->items_number[i] != opr2.items_number[i]) {
            return false;
        }
    }
    return true;
}

Closure::Closure()
{
    trans = new Transfer[TerminalChar::all_terminal_chars.size() + Nonterminal::all_nonterminal_chars.size()];
    for (int i = 0; i < TerminalChar::all_terminal_chars.size() + Nonterminal::all_nonterminal_chars.size(); ++i) {
        trans[i].transfer_type = -1;
        trans[i].transfer_next_status = -1;
        trans[i].transfer_nonterminal_target = -1;
        trans[i].transfer_reduction_index = -1;
    }
}

void Closure::add_item(int item_number) {
    for (int i = 0; i < items_number.size(); i ++) {
        if(items_number[i] == item_number) {
            return ;
        }
    }
    items_number.push_back(item_number);
}

void Closure::sort_items()
{
    sort(items_number.begin(), items_number.end());
}

void Closure::update()
{
    LR1item tmp_LR1item;
    for (int i = 0; i < items_number.size(); i ++) {
        LR1item* l_now = LR1item::all_lr_1_item_set[items_number[i]];  // LR(1)item todo
        if(l_now->dot_position >= Nonterminal::all_nonterminal_chars[l_now->index].productions[l_now->production_index].size()) {
            continue;
        }
        Character* c_now = &Nonterminal::all_nonterminal_chars[l_now->index].productions[l_now->production_index][l_now->dot_position];
        // find project join in item set
        // A -> [B·CD, a]
        if(!c_now->is_terminal) {   // not terminator
            for (int j = 0; j < Nonterminal::all_nonterminal_chars[c_now->index].productions.size(); ++j) {
                // C -> EF
                int foresee_chars_number = l_now->get_foresee_char();
                for(int k = 0; k < foresee_chars_number; k ++) {
                    tmp_LR1item.index = c_now->index;
                    tmp_LR1item.production_index = j;
                    tmp_LR1item.dot_position = 0;
                    tmp_LR1item.foresee_char_index = LR1item::foresee_char_buffer[k];

                    // check is epsilon production
                    if (Nonterminal::all_nonterminal_chars[tmp_LR1item.index].productions[tmp_LR1item.production_index].empty()) {
                        tmp_LR1item.is_reduction = true;
                    }

                    int position = LR1item::is_LR1item_exist(tmp_LR1item);
                    if(position == -1) {
                        LR1item::all_lr_1_item_set.push_back(new LR1item(tmp_LR1item));
                        position = LR1item::all_lr_1_item_set.size() - 1;
                    }
                    add_item(position);
                }
            }
        }
    }
    sort_items();
}

void Closure::print() {
    cout << "I" << index << ":" << endl;
    for(int i = 0; i < items_number.size(); i++) {
        LR1item::all_lr_1_item_set[items_number[i]]->print();
    }
    cout << "--------------------------------------------------" << endl;
}

Closure::~Closure() {
    delete[] trans;
}

Closure::Closure(const Closure& src_closure)
{
    index = src_closure.index;
    unsigned long all_char_cnt = TerminalChar::all_terminal_chars.size() + Nonterminal::all_nonterminal_chars.size();
    trans = new Transfer[all_char_cnt];

    memcpy(trans, src_closure.trans, sizeof(Transfer) * all_char_cnt);

    for (int i : src_closure.items_number) {
        items_number.push_back(i);
    }
}

