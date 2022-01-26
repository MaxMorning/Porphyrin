//
// Project: Porphyrin
// File Name: Closure.h
// Author: Morning
// Description:
//
// Create Date: 2021/11/2
//

#ifndef PORPHYRIN_CLOSURE_H
#define PORPHYRIN_CLOSURE_H

#include <vector>
#include <unordered_set>
#include <algorithm>
#include "LR1item.h"

using namespace std;

struct Transfer {
    int transfer_type;      // 1 for move in , 0 for reduction
    int transfer_next_status;    // move in
    int transfer_reduction_index; // index of reduction of each nonterminal
    int transfer_nonterminal_target; // index of reduction result
};

class Closure {
public:
    int index;

    Closure();

    Closure(const Closure& src_closure);

    ~Closure();

    Transfer* trans;
    // add LR(1) item into this closure
    // return true when item is added successfully
    void add_item(int item_number);

    void sort_items();

    void update();

    void print();

    // check two closure is equal
    bool operator==(const Closure& opr2);

    // LR(1) items
    vector<int> items_number;

    static vector<Closure> all_closure_set;
};


#endif //PORPHYRIN_CLOSURE_H
