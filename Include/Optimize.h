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
    int start_index;
    int end_index;

    int next_block_0_ptr;
    int next_block_1_ptr;

    bool is_reachable;

    // active symbol calc
    unordered_set<int> use_set;
    unordered_set<int> def_set;
    unordered_set<int> in_set;

    unordered_set<int> out_set;

    vector<Quaternion> block_quaternion_sequence;
};


struct DAGNode {
    OP_CODE op;
    unordered_set<int> represent_variables;

    bool is_const;
    ValueType const_value; // for OP_ARRAY_STORE, int_value means the index of array

    int opr1_ptr;
    int opr2_ptr;

    bool is_visited;
    int symbol_index;
};

extern vector<BaseBlock> base_blocks;

void split_base_blocks(vector<Quaternion> quaternion_sequence);
void calculate_active_symbol_sets(bool need_process_call_par);

extern vector<Quaternion> optimized_sequence;
void optimize_IR(vector<Quaternion>& quaternion_sequence);


void print_optimize_sequence();

void write_optimize_result();

#endif //PORPHYRIN_OPTIMIZE_H
