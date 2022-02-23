//
// Project: Porphyrin
// File Name: Optimize.cpp
// Author: Morning
// Description:
//
// Create Date: 2022/2/21
//


#include "Include/Optimize.h"

#define IS_NOT_BRANCH_IR -5
const unsigned int MAX_UNSIGNED_INT = 0xFFFFFFFF;


vector<Quaternion> optimized_sequence;


int is_branch_IR(const Quaternion& quaternion, int quaternion_index)
{
    OP_CODE op_code = quaternion.op_code;

    if (    op_code == OP_JEQ  ||
            op_code == OP_JNZ  ||
            op_code == OP_JMP
       ) {
        return quaternion_index + quaternion.result;
    }
    else if (op_code == OP_CALL) {
//        return Function::function_table[quaternion.opr1].entry_address;
        return quaternion_index + 1;
    }
    else if (op_code == OP_RETURN) {
        return -1;
    }
    else {
        return IS_NOT_BRANCH_IR;
    }
}

vector<BaseBlock> base_blocks;

unsigned int search_base_block(unsigned int entrance_index)
{
    for (unsigned int i = 0; i < base_blocks.size(); ++i) {
        if (base_blocks[i].start_index == entrance_index) {
            return i;
        }
    }

    return -1;
}

void split_base_blocks()
{
    bool* is_base_block_entry_array = new bool[quaternion_sequence.size()];
    memset(is_base_block_entry_array, 0, quaternion_sequence.size() * sizeof(bool));

    // first instr is always the entrance of a base block
    is_base_block_entry_array[0] = true;

    for (int i = 0; i < quaternion_sequence.size(); ++i) {
        int reachable_dst = is_branch_IR(quaternion_sequence[i], i);
        if (reachable_dst != IS_NOT_BRANCH_IR) {
            // is a branch IR
            is_base_block_entry_array[i + 1] = true;
            is_base_block_entry_array[reachable_dst] = true;
        }
    }

    is_base_block_entry_array[quaternion_sequence.size()] = true;


    // generate base block objects
    base_blocks.push_back(BaseBlock{0, (unsigned int)(quaternion_sequence.size() - 1), MAX_UNSIGNED_INT, MAX_UNSIGNED_INT, false});
    for (int index = 1; index < quaternion_sequence.size(); ++index) {
        if (is_base_block_entry_array[index]) {
            base_blocks.back().end_index = index - 1;
            base_blocks.push_back(BaseBlock{(unsigned int)index, (unsigned int)(quaternion_sequence.size() - 1), MAX_UNSIGNED_INT, MAX_UNSIGNED_INT, false});
        }
    }

    delete[] is_base_block_entry_array;

    // build links between base blocks
    for (int i = 0; i < base_blocks.size(); ++i) {
        int dst_quaternion_index = is_branch_IR(quaternion_sequence[base_blocks[i].end_index], base_blocks[i].end_index);

        if (dst_quaternion_index >= 0) {
            base_blocks[i].next_block_0_ptr = search_base_block(dst_quaternion_index);

            if (quaternion_sequence[base_blocks[i].end_index].op_code != OP_JMP
            && quaternion_sequence[base_blocks[i].end_index].op_code != OP_CALL) {
                base_blocks[i].next_block_1_ptr = i + 1;
            }
        }
    }

    // check if is reachable
    base_blocks[0].is_reachable = true;

    // all entrance of function are reachable
    for (Function& function : Function::function_table) {
        base_blocks[search_base_block(function.entry_address)].is_reachable = true;
    }

    for (int i = 0; i < base_blocks.size(); ++i) {
        if (base_blocks[i].is_reachable) {
            if (base_blocks[i].next_block_0_ptr != MAX_UNSIGNED_INT) {
                base_blocks[base_blocks[i].next_block_0_ptr].is_reachable = true;
            }
            if (base_blocks[i].next_block_1_ptr != MAX_UNSIGNED_INT) {
                base_blocks[base_blocks[i].next_block_1_ptr].is_reachable = true;
            }
        }
    }


#ifdef OPTIMIZE_DEBUG
    // print base block info
    for (int i = 0; i < base_blocks.size(); ++i) {
        cout << i << '\t' << base_blocks[i].start_index << '\t' << base_blocks[i].end_index << '\t' << base_blocks[i].is_reachable << '\t' << "Link to " << base_blocks[i].next_block_0_ptr << " and " << base_blocks[i].next_block_1_ptr << endl;
    }
#endif
}


vector<DAGNode*> dag_nodes;
int* variable_dag_node_map; // symbol_table[i] is represented by dag_nodes[variable_dag_node_map[i]], -1 means not represented

int create_dag_node(int symbol_index, OP_CODE op)
{
    int dag_node_index = dag_nodes.size();
    variable_dag_node_map[symbol_index] = dag_node_index;

    DAGNode* new_dag_node = new DAGNode;
    new_dag_node->op = op;
    new_dag_node->represent_variables.emplace(symbol_index);
    new_dag_node->is_const = symbol_table[symbol_index].is_const;
    new_dag_node->const_value = symbol_table[symbol_index].value;
    new_dag_node->opr1_ptr = MAX_UNSIGNED_INT;
    new_dag_node->opr2_ptr = MAX_UNSIGNED_INT;
    new_dag_node->is_visited = false;

    dag_nodes.push_back(new_dag_node);
    return dag_node_index;
}

int search_matched_node(OP_CODE op, unsigned int opr1_node_index, unsigned int opr2_node_index, bool can_swap)
{
    for (int i = 0; i < dag_nodes.size(); ++i) {
        if (dag_nodes[i]->op == op) {
            if (dag_nodes[i]->opr1_ptr == opr1_node_index && dag_nodes[i]->opr2_ptr == opr2_node_index) {
                return i;
            }
            else if (can_swap && dag_nodes[i]->opr1_ptr == opr2_node_index && dag_nodes[i]->opr2_ptr == opr1_node_index) {
                return i;
            }
        }
    }

    return -1;
}

void dual_opr_preset(int& opr1_node_index, int& opr2_node_index, const Quaternion& quaternion)
{
    // get opr1 node
    opr1_node_index = variable_dag_node_map[quaternion.opr1];

    if (opr1_node_index == -1) {
        // opr1 node not created
        opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
    }

    // get opr2 node
    opr2_node_index = variable_dag_node_map[quaternion.opr2];

    if (opr2_node_index == -1) {
        // opr2 node not created
        opr2_node_index = create_dag_node(quaternion.opr2, OP_INVALID);
    }

    // erase current records
//    int current_node_index = variable_dag_node_map[quaternion.result];
//    if (current_node_index != -1) {
//        // need erase
//        dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//    }
}

void calculate_use_def_sets()
{
    bool symbol_status[symbol_table.size()][2]; // 0 - use ; 1 - def


    for (BaseBlock& base_block : base_blocks) {
        memset(symbol_status, 0, symbol_table.size() * 2 * sizeof(bool));
        unordered_set<unsigned int> temp_use_set;
        unordered_set<unsigned int> temp_def_set;
        for (int quat_index = base_block.start_index; quat_index <= base_block.end_index; ++quat_index) {
            Quaternion& quaternion = quaternion_sequence[quat_index];

            assert((quaternion.op_code - OP_LI_BOOL > 3) || (quaternion.op_code - OP_LI_BOOL < 0));

            switch (quaternion.op_code) {
                case OP_ASSIGNMENT:
                case OP_NEG:
                case OP_BOOL_TO_CHAR:
                case OP_BOOL_TO_INT:
                case OP_BOOL_TO_FLOAT:
                case OP_BOOL_TO_DOUBLE:
                case OP_CHAR_TO_BOOL:
                case OP_CHAR_TO_INT:
                case OP_CHAR_TO_FLOAT:
                case OP_CHAR_TO_DOUBLE:
                case OP_INT_TO_BOOL:
                case OP_INT_TO_CHAR:
                case OP_INT_TO_FLOAT:
                case OP_INT_TO_DOUBLE:
                case OP_FLOAT_TO_BOOL:
                case OP_FLOAT_TO_CHAR:
                case OP_FLOAT_TO_INT:
                case OP_FLOAT_TO_DOUBLE:
                case OP_DOUBLE_TO_BOOL:
                case OP_DOUBLE_TO_CHAR:
                case OP_DOUBLE_TO_INT:
                case OP_DOUBLE_TO_FLOAT:
                {
                    if ((!symbol_status[quaternion.opr1][0]) && (!symbol_status[quaternion.opr1][1])) {
                        symbol_status[quaternion.opr1][0] = true;
                        temp_use_set.emplace(quaternion.opr1);
                    }

                    if ((!symbol_status[quaternion.result][0]) && (!symbol_status[quaternion.result][1])) {
                        symbol_status[quaternion.result][1] = true;
                        temp_def_set.emplace(quaternion.result);
                    }
                    break;
                }

                case OP_JEQ:
                case OP_ARRAY_STORE:
                {
                    if ((!symbol_status[quaternion.opr1][0]) && (!symbol_status[quaternion.opr1][1])) {
                        symbol_status[quaternion.opr1][0] = true;
                        temp_use_set.emplace(quaternion.opr1);
                    }

                    if ((!symbol_status[quaternion.opr2][0]) && (!symbol_status[quaternion.opr2][1])) {
                        symbol_status[quaternion.opr2][0] = true;
                        temp_use_set.emplace(quaternion.opr2);
                    }
                    break;
                }

                case OP_JNZ:
                case OP_PAR:
                {
                    if ((!symbol_status[quaternion.opr1][0]) && (!symbol_status[quaternion.opr1][1])) {
                        symbol_status[quaternion.opr1][0] = true;
                        temp_use_set.emplace(quaternion.opr1);
                    }

                    break;
                }

                case OP_JMP:
                case OP_CALL:
                case OP_NOP:
                case OP_INVALID:
                {
                    // do nothing
                    break;
                }

                case OP_FETCH_BOOL:
                case OP_FETCH_INT:
                case OP_FETCH_FLOAT:
                case OP_FETCH_DOUBLE:
                case OP_COMMA:
                {
                    if ((!symbol_status[quaternion.opr2][0]) && (!symbol_status[quaternion.opr2][1])) {
                        symbol_status[quaternion.opr2][0] = true;
                        temp_use_set.emplace(quaternion.opr2);
                    }

                    if ((!symbol_status[quaternion.result][0]) && (!symbol_status[quaternion.result][1])) {
                        symbol_status[quaternion.result][1] = true;
                        temp_def_set.emplace(quaternion.result);
                    }
                    break;
                }

                case OP_RETURN:
                {
                    if (quaternion.result != -1) {
                        if ((!symbol_status[quaternion.result][0]) && (!symbol_status[quaternion.result][1])) {
                            symbol_status[quaternion.result][1] = true;
                            temp_def_set.emplace(quaternion.result);
                        }
                    }

                    break;
                }

                default:
                {
                    if ((!symbol_status[quaternion.opr1][0]) && (!symbol_status[quaternion.opr1][1])) {
                        symbol_status[quaternion.opr1][0] = true;
                        temp_use_set.emplace(quaternion.opr1);
                    }

                    if ((!symbol_status[quaternion.opr2][0]) && (!symbol_status[quaternion.opr2][1])) {
                        symbol_status[quaternion.opr2][0] = true;
                        temp_use_set.emplace(quaternion.opr2);
                    }

                    if ((!symbol_status[quaternion.result][0]) && (!symbol_status[quaternion.result][1])) {
                        symbol_status[quaternion.result][1] = true;
                        temp_def_set.emplace(quaternion.result);
                    }
                    break;
                }
            }
        }

        // remove const var
        for (unsigned int symbol_index : temp_use_set) {
            if (!symbol_table[symbol_index].is_const) {
                base_block.use_set.emplace(symbol_index);
            }
        }


        for (unsigned int symbol_index : temp_def_set) {
            if (!symbol_table[symbol_index].is_const) {
                base_block.def_set.emplace(symbol_index);
            }
        }
    }
}

void calculate_active_symbol_sets()
{
    calculate_use_def_sets();

    // calculate in_set and out_set
    // referenced from ISBN 978-7-302-38141-9  P263
    for (BaseBlock& baseBlock : base_blocks) {
        baseBlock.out_set.clear();
        baseBlock.in_set = unordered_set<unsigned int>(baseBlock.use_set);
    }

    bool change = true;
    while (change) {
        change = false;

        for (BaseBlock& baseBlock : base_blocks) {
            bool single_change = false;
            if (baseBlock.next_block_0_ptr != MAX_UNSIGNED_INT) {
                for (unsigned int symbol_index : base_blocks[baseBlock.next_block_0_ptr].in_set) {
                    auto find_iter = baseBlock.out_set.find(symbol_index);
                    if (find_iter == baseBlock.out_set.end()) {
                        // not exist
                        baseBlock.out_set.emplace(symbol_index);
                        single_change = true;
                    }
                }
            }

            if (baseBlock.next_block_1_ptr != MAX_UNSIGNED_INT) {
                for (unsigned int symbol_index : base_blocks[baseBlock.next_block_1_ptr].in_set) {
                    auto find_iter = baseBlock.out_set.find(symbol_index);
                    if (find_iter == baseBlock.out_set.end()) {
                        // not exist
                        baseBlock.out_set.emplace(symbol_index);
                        single_change = true;
                    }
                }
            }

            if (single_change) {
                change = true;

                for (unsigned int symbol_index : baseBlock.out_set) {
                    auto find_iter = baseBlock.def_set.find(symbol_index);
                    if (find_iter == baseBlock.def_set.end()) {
                        // not in def set
                        baseBlock.in_set.emplace(symbol_index);
                    }
                }
            }
        }
    }

    // process return value
    for (BaseBlock& baseBlock : base_blocks) {
        if (quaternion_sequence[baseBlock.end_index].op_code == OP_RETURN && quaternion_sequence[baseBlock.end_index].result != -1) {
            baseBlock.out_set.emplace(quaternion_sequence[baseBlock.end_index].result);
        }
    }

    // process PAR
    for (BaseBlock& baseBlock : base_blocks) {
        for (int i = baseBlock.start_index; i <= baseBlock.end_index; ++i) {
            if (quaternion_sequence[i].op_code == OP_PAR) {
                baseBlock.out_set.emplace(quaternion_sequence[i].opr1);
            }
        }
    }

#ifdef OPTIMIZE_DEBUG
    cout << "Base Block Symbol Info" << endl;
    cout << endl;
    for (int i = 0; i < base_blocks.size(); ++i) {
        cout << i << endl;

        cout << "Use\t";
        for (unsigned int symbol_index : base_blocks[i].use_set) {
            cout << symbol_table[symbol_index].content << '\t';
        }
        cout << endl;

        cout << "Def\t";
        for (unsigned int symbol_index : base_blocks[i].def_set) {
            cout << symbol_table[symbol_index].content << '\t';
        }
        cout << endl;

        cout << "In\t";
        for (unsigned int symbol_index : base_blocks[i].in_set) {
            cout << symbol_table[symbol_index].content << '\t';
        }
        cout << endl;

        cout << "Out\t";
        for (unsigned int symbol_index : base_blocks[i].out_set) {
            cout << symbol_table[symbol_index].content << '\t';
        }
        cout << endl;
    }
#endif
}

DATA_TYPE_ENUM get_result_data_type(OP_CODE op, DATA_TYPE_ENUM opr1_dt, DATA_TYPE_ENUM opr2_dt)
{
    DATA_TYPE_ENUM result_data_type = OP_CODE_RESULT_DT[op];

    if (result_data_type == DT_NOT_DECIDED) {
        return opr1_dt;
    }
    else if (op == OP_COMMA) {
        return opr2_dt;
    }
    else {
        return result_data_type;
    }
}

int generate_quaternions(DAGNode& dag_node, vector<Quaternion>& target_sequence)
{
    if (dag_node.is_visited) {
        return dag_node.symbol_index;
    }

    int opr1_index, opr2_index;
    if (dag_node.opr1_ptr != MAX_UNSIGNED_INT && !dag_nodes[dag_node.opr1_ptr]->is_visited) {
        opr1_index = generate_quaternions(*dag_nodes[dag_node.opr1_ptr], target_sequence);
    }

    if (dag_node.opr2_ptr != MAX_UNSIGNED_INT && !dag_nodes[dag_node.opr2_ptr]->is_visited) {
        opr2_index = generate_quaternions(*dag_nodes[dag_node.opr2_ptr], target_sequence);
    }

    dag_node.is_visited = true;

    if (dag_node.op == OP_INVALID) {
        // this node is a const / active variable
        dag_node.symbol_index = *(dag_node.represent_variables.begin());

        return dag_node.symbol_index;
    }
    else {
        dag_node.symbol_index = get_temp_symbol(get_result_data_type(dag_node.op, (dag_node.opr1_ptr != MAX_UNSIGNED_INT ? symbol_table[opr1_index].data_type : DT_VOID),
                                                                     (dag_node.opr2_ptr != MAX_UNSIGNED_INT ? symbol_table[opr1_index].data_type : DT_VOID) ), false);
        target_sequence.push_back({dag_node.op, (dag_node.opr1_ptr != MAX_UNSIGNED_INT ? opr1_index : -1),
                                   (dag_node.opr2_ptr != MAX_UNSIGNED_INT ? opr2_index : -1), dag_node.symbol_index});

        return dag_node.symbol_index;
    }
}

void optimize_base_block(BaseBlock& base_block)
{
    if (!base_block.is_reachable) {
        return;
    }

    dag_nodes.clear();
    variable_dag_node_map = new int[symbol_table.size()];
    memset(variable_dag_node_map, -1, symbol_table.size() * sizeof(unsigned int));

    for (int i = base_block.start_index; i <= base_block.end_index; ++i) {
        Quaternion& quaternion = quaternion_sequence[i];

        switch (quaternion.op_code) {
            case OP_ASSIGNMENT:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // the result node and the opr1 node are the same node
                dag_nodes[opr1_node_index]->represent_variables.emplace(quaternion.result);
                variable_dag_node_map[quaternion.result] = opr1_node_index;
                break;
            }

            case OP_ADD_INT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.int_value = dag_nodes[opr1_node_index]->const_value.int_value + dag_nodes[opr2_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_ADD_FLOAT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.float_value = dag_nodes[opr1_node_index]->const_value.float_value + dag_nodes[opr2_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_ADD_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.double_value = dag_nodes[opr1_node_index]->const_value.double_value + dag_nodes[opr2_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_MINUS_INT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.int_value = dag_nodes[opr1_node_index]->const_value.int_value - dag_nodes[opr2_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_MINUS_FLOAT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.float_value = dag_nodes[opr1_node_index]->const_value.float_value - dag_nodes[opr2_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_MINUS_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.double_value = dag_nodes[opr1_node_index]->const_value.double_value - dag_nodes[opr2_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_MULTI_INT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.int_value = dag_nodes[opr1_node_index]->const_value.int_value * dag_nodes[opr2_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_MULTI_FLOAT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.float_value = dag_nodes[opr1_node_index]->const_value.float_value * dag_nodes[opr2_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_MULTI_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.double_value = dag_nodes[opr1_node_index]->const_value.double_value * dag_nodes[opr2_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_DIV_INT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.int_value = dag_nodes[opr1_node_index]->const_value.int_value / dag_nodes[opr2_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_DIV_FLOAT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.float_value = dag_nodes[opr1_node_index]->const_value.float_value / dag_nodes[opr2_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_DIV_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.double_value = dag_nodes[opr1_node_index]->const_value.double_value / dag_nodes[opr2_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_COMMA:
            {
                // get opr2 node
                int opr2_node_index = variable_dag_node_map[quaternion.opr2];

                if (opr2_node_index == -1) {
                    // opr2 node not created
                    opr2_node_index = create_dag_node(quaternion.opr2, OP_INVALID);
                }

                // the result node and the opr2 node are the same node
                dag_nodes[opr2_node_index]->represent_variables.emplace(quaternion.result);
                variable_dag_node_map[quaternion.result] = opr2_node_index;
                break;
            }

            case OP_JEQ:
            case OP_JNZ:
            case OP_JMP:
            case OP_LI_BOOL:
            case OP_LI_INT:
            case OP_LI_FLOAT:
            case OP_LI_DOUBLE:
            {
                // do nothing
                break;
            }

            case OP_FETCH_BOOL:
            case OP_FETCH_INT:
            case OP_FETCH_FLOAT:
            case OP_FETCH_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // try find common expr
                int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                if (search_common_expr_index == -1) {
                    // no such common expr
                    int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                    dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                    dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                }
                else {
                    // find common expr
                    dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                    variable_dag_node_map[quaternion.result] = search_common_expr_index;
                }
                break;
            }

            case OP_EQUAL_INT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.int_value == dag_nodes[opr2_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_EQUAL_FLOAT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.float_value == dag_nodes[opr2_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_EQUAL_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.double_value == dag_nodes[opr2_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_GREATER_INT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.int_value > dag_nodes[opr2_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_GREATER_FLOAT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.float_value > dag_nodes[opr2_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_GREATER_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.double_value > dag_nodes[opr2_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_GREATER_EQUAL_INT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.int_value >= dag_nodes[opr2_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_GREATER_EQUAL_FLOAT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.float_value >= dag_nodes[opr2_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_GREATER_EQUAL_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.double_value >= dag_nodes[opr2_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_LESS_INT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.int_value < dag_nodes[opr2_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_LESS_FLOAT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.float_value < dag_nodes[opr2_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_LESS_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.double_value < dag_nodes[opr2_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_LESS_EQUAL_INT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.int_value <= dag_nodes[opr2_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_LESS_EQUAL_FLOAT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.float_value <= dag_nodes[opr2_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_LESS_EQUAL_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.double_value <= dag_nodes[opr2_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_NOT_EQUAL_INT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.int_value != dag_nodes[opr2_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_NOT_EQUAL_FLOAT:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.float_value != dag_nodes[opr2_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_NOT_EQUAL_DOUBLE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = dag_nodes[opr1_node_index]->const_value.double_value != dag_nodes[opr2_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, true);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }

                break;
            }

            case OP_NEG:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

//                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    switch (symbol_table[quaternion.opr1].data_type) {
                        case DT_INT:
                            dag_nodes[result_node_index]->const_value.int_value = - dag_nodes[opr1_node_index]->const_value.int_value;
                            break;

                        case DT_FLOAT:
                            dag_nodes[result_node_index]->const_value.float_value = - dag_nodes[opr1_node_index]->const_value.float_value;
                            break;

                        case DT_DOUBLE:
                            dag_nodes[result_node_index]->const_value.double_value = - dag_nodes[opr1_node_index]->const_value.double_value;
                            break;
                        default:
                            break;
                    }

                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_PAR:
            case OP_CALL:
            case OP_RETURN:
            case OP_NOP:
            {
                // do nothing
                break;
            }

            case OP_BOOL_TO_CHAR:
            case OP_INT_TO_CHAR:
            case OP_FLOAT_TO_CHAR:
            case OP_DOUBLE_TO_CHAR:

            case OP_CHAR_TO_BOOL:
            case OP_CHAR_TO_INT:
            case OP_CHAR_TO_FLOAT:
            case OP_CHAR_TO_DOUBLE:
            case OP_INVALID:
            {
                throw "Not supported!";
                break;
            }

            case OP_BOOL_TO_INT:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.int_value = (int)dag_nodes[opr1_node_index]->const_value.bool_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_BOOL_TO_FLOAT:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.float_value = (float)dag_nodes[opr1_node_index]->const_value.bool_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_BOOL_TO_DOUBLE:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.double_value = (double)dag_nodes[opr1_node_index]->const_value.bool_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_INT_TO_BOOL:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = (bool)dag_nodes[opr1_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_INT_TO_FLOAT:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.float_value = (float)dag_nodes[opr1_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_INT_TO_DOUBLE:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.double_value = (double)dag_nodes[opr1_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_FLOAT_TO_BOOL:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = (bool)dag_nodes[opr1_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_FLOAT_TO_INT:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.int_value = (int)dag_nodes[opr1_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_FLOAT_TO_DOUBLE:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.double_value = (double)dag_nodes[opr1_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_DOUBLE_TO_BOOL:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.bool_value = (bool)dag_nodes[opr1_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_DOUBLE_TO_INT:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.int_value = (int)dag_nodes[opr1_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_DOUBLE_TO_FLOAT:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                if (opr1_node_index == -1) {
                    // opr1 node not created
                    opr1_node_index = create_dag_node(quaternion.opr1, OP_INVALID);
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID);
                    dag_nodes[result_node_index]->const_value.float_value = (float)dag_nodes[opr1_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_UNSIGNED_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_UNSIGNED_INT;
                    }
                    else {
                        // find common expr
                        dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                        variable_dag_node_map[quaternion.result] = search_common_expr_index;
                    }
                }
                break;
            }

            case OP_ARRAY_STORE:
            {
                int opr1_node_index, opr2_node_index;

                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

                int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, false);
                if (search_common_expr_index == -1) {
                    // no such common expr
                    int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                    dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                    dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
                }
                else {
                    // find common expr
                    dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
                    variable_dag_node_map[quaternion.result] = search_common_expr_index;
                }

                break;
            }
        }
    }


    // generate quaternion sequence
    for (unsigned int out_active_symbol : base_block.out_set) {
        if (variable_dag_node_map[out_active_symbol] != -1) {
            // check if this variable is const and generate assign instr
            if (dag_nodes[variable_dag_node_map[out_active_symbol]]->op == OP_INVALID) {
                // get a const value == this variable
                int value_temp_var = -1;

                for (unsigned int symbol_index : dag_nodes[variable_dag_node_map[out_active_symbol]]->represent_variables) {
                    if (symbol_table[symbol_index].is_const) {
                        value_temp_var = symbol_index;
                        break;
                    }
                }

                assert(value_temp_var >= 0);
                base_block.block_quaternion_sequence.push_back({OP_ASSIGNMENT, value_temp_var, -1, (int)(out_active_symbol)});
            }
            generate_quaternions(*dag_nodes[variable_dag_node_map[out_active_symbol]], base_block.block_quaternion_sequence);
        }
    }

    // generate PAR / return
    for (int i = base_block.start_index; i <= base_block.end_index; ++i) {
        if (quaternion_sequence[i].op_code == OP_PAR || quaternion_sequence[i].op_code == OP_RETURN || quaternion_sequence[i].op_code == OP_JNZ) {
            generate_quaternions(*dag_nodes[variable_dag_node_map[quaternion_sequence[i].opr1]], base_block.block_quaternion_sequence);
        }
        else if (quaternion_sequence[i].op_code == OP_JEQ) {
            generate_quaternions(*dag_nodes[variable_dag_node_map[quaternion_sequence[i].opr1]], base_block.block_quaternion_sequence);
            generate_quaternions(*dag_nodes[variable_dag_node_map[quaternion_sequence[i].opr2]], base_block.block_quaternion_sequence);
        }
    }


    // insert nop instructions to match jump offset
    int nop_instr_cnt = base_block.end_index - base_block.start_index - base_block.block_quaternion_sequence.size();
    for (int i = 0; i < nop_instr_cnt; ++i) {
        base_block.block_quaternion_sequence.push_back({OP_NOP, -1, -1, -1});
    }

    // add last one
    base_block.block_quaternion_sequence.push_back(quaternion_sequence[base_block.end_index]);

    delete[] variable_dag_node_map;
//    throw "Not implemented!";
}

void optimize_IR(vector<Quaternion> quaternion_sequence)
{
    split_base_blocks();

    calculate_active_symbol_sets();

    for (BaseBlock& baseBlock : base_blocks) {
        optimize_base_block(baseBlock);
    }


    optimized_sequence.clear();
    for (BaseBlock& baseBlock : base_blocks) {
        for (Quaternion& quaternion : baseBlock.block_quaternion_sequence) {
            optimized_sequence.push_back(quaternion);
        }
    }

//    throw "Not implemented!";
}


void print_optimize_sequence()
{
    print_quaternion_sequence(optimized_sequence);
}