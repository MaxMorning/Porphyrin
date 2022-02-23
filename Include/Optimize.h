//
// Project: Porphyrin
// File Name: Optimize.h
// Author: Morning
// Description:
//
// Create Date: 2022/2/21
//

#ifndef PORPHYRIN_OPTIMIZE_H
#define PORPHYRIN_OPTIMIZE_H


#include <vector>
#include <unordered_set>

#include "Include/Semantic.h"

using namespace std;


struct BaseBlock {
    unsigned int start_index;
    unsigned int end_index;

    unsigned int next_block_0_ptr;
    unsigned int next_block_1_ptr;

    bool is_reachable;

    // active symbol calc
    unordered_set<unsigned int> use_set;
    unordered_set<unsigned int> def_set;
    unordered_set<unsigned int> in_set;

    unordered_set<unsigned int> out_set;

    vector<Quaternion> block_quaternion_sequence;
};


struct DAGNode {
    OP_CODE op;
    unordered_set<unsigned int> represent_variables;

    bool is_const;
    ValueType const_value; // for OP_ARRAY_STORE, int_value means the index of array

    unsigned int opr1_ptr;
    unsigned int opr2_ptr;

    bool is_visited;
    int symbol_index;
};


extern vector<Quaternion> optimized_sequence;
void optimize_IR(vector<Quaternion> quaternion_sequence);


void print_optimize_sequence();

#endif //PORPHYRIN_OPTIMIZE_H
