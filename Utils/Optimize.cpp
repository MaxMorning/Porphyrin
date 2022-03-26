//
// Project: Porphyrin
// File Name: Optimize.cpp
// Author: Morning
// Description:
//
// Create Date: 2022/2/21
//


#include "Include/Optimize.h"
#include <cassert>

#define IS_NOT_BRANCH_IR -5
const int MAX_INT = 2147483647;


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
    else if (op_code == OP_CALL || op_code == OP_ARRAY_STORE) {
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

int search_base_block(int entrance_index)
{
    for (int i = 0; i < base_blocks.size(); ++i) {
        if (base_blocks[i].start_index == entrance_index) {
            return i;
        }
    }

    return -1;
}

void split_base_blocks(vector<Quaternion>& quaternion_sequence)
{
    base_blocks.clear();
    bool* is_base_block_entry_array = new bool[quaternion_sequence.size() + 1];
    memset(is_base_block_entry_array, 0, (quaternion_sequence.size() + 1) * sizeof(bool));

    // first instr is always the entrance of a base block
    is_base_block_entry_array[0] = true;

    for (int i = 0; i < quaternion_sequence.size(); ++i) {
        int reachable_dst = is_branch_IR(quaternion_sequence[i], i);
        if (reachable_dst == -1) {
            is_base_block_entry_array[i + 1] = true;
        }
        else if (reachable_dst != IS_NOT_BRANCH_IR) {
            // is a branch IR
            is_base_block_entry_array[i + 1] = true;
            is_base_block_entry_array[reachable_dst] = true;
        }
    }

    is_base_block_entry_array[quaternion_sequence.size()] = true;


    // generate base block objects
    base_blocks.push_back(BaseBlock{0, (int)(quaternion_sequence.size() - 1), MAX_INT, MAX_INT, false});
    for (int index = 1; index < quaternion_sequence.size(); ++index) {
        if (is_base_block_entry_array[index]) {
            base_blocks.back().end_index = index - 1;
            base_blocks.push_back(BaseBlock{(int)index, (int)(quaternion_sequence.size() - 1), MAX_INT, MAX_INT, false});
        }
    }

    delete[] is_base_block_entry_array;

    // build links between base blocks
    for (int i = 0; i < base_blocks.size(); ++i) {
        int dst_quaternion_index = is_branch_IR(quaternion_sequence[base_blocks[i].end_index], base_blocks[i].end_index);

        if (dst_quaternion_index >= 0) {
            base_blocks[i].next_block_0_ptr = search_base_block(dst_quaternion_index);

            if (quaternion_sequence[base_blocks[i].end_index].op_code != OP_JMP) {
                base_blocks[i].next_block_1_ptr = i + 1;
            }
        }
        else if (dst_quaternion_index == IS_NOT_BRANCH_IR) {
            base_blocks[i].next_block_1_ptr = i + 1;
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
            if (base_blocks[i].next_block_0_ptr != MAX_INT) {
                base_blocks[base_blocks[i].next_block_0_ptr].is_reachable = true;
            }
            if (base_blocks[i].next_block_1_ptr != MAX_INT) {
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

int create_dag_node(int symbol_index, OP_CODE op, bool force_const = false)
{
    int dag_node_index = dag_nodes.size();
    variable_dag_node_map[symbol_index] = dag_node_index;

    DAGNode* new_dag_node = new DAGNode;
    new_dag_node->op = op;
    new_dag_node->represent_variables.emplace(symbol_index);
    new_dag_node->is_const = force_const || symbol_table[symbol_index].is_const;
    new_dag_node->const_value = symbol_table[symbol_index].value;
    new_dag_node->opr1_ptr = MAX_INT;
    new_dag_node->opr2_ptr = MAX_INT;
    new_dag_node->is_visited = false;
    new_dag_node->symbol_index = symbol_index;

    dag_nodes.push_back(new_dag_node);
    return dag_node_index;
}

int search_matched_node(OP_CODE op, int opr1_node_index, int opr2_node_index, bool can_swap)
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

void set_const_value(Quaternion& quaternion, ValueType& dst_value, ValueType& opr1_value, ValueType& opr2_value)
{
    switch (quaternion.op_code) {
        case OP_ADD_INT:
            dst_value.int_value = opr1_value.int_value + opr2_value.int_value;
            break;

        case OP_ADD_FLOAT:
            dst_value.float_value = opr1_value.float_value + opr2_value.float_value;
            break;

        case OP_ADD_DOUBLE:
            dst_value.double_value = opr1_value.double_value + opr2_value.double_value;
            break;

        case OP_MINUS_INT:
            dst_value.int_value = opr1_value.int_value - opr2_value.int_value;
            break;

        case OP_MINUS_FLOAT:
            dst_value.float_value = opr1_value.float_value - opr2_value.float_value;
            break;

        case OP_MINUS_DOUBLE:
            dst_value.double_value = opr1_value.double_value - opr2_value.double_value;
            break;

        case OP_MULTI_INT:
            dst_value.int_value = opr1_value.int_value * opr2_value.int_value;
            break;

        case OP_MULTI_FLOAT:
            dst_value.float_value = opr1_value.float_value * opr2_value.float_value;
            break;

        case OP_MULTI_DOUBLE:
            dst_value.double_value = opr1_value.double_value * opr2_value.double_value;
            break;

        case OP_DIV_INT:
            dst_value.int_value = opr1_value.int_value / opr2_value.int_value;
            break;

        case OP_DIV_FLOAT:
            dst_value.float_value = opr1_value.float_value / opr2_value.float_value;
            break;

        case OP_DIV_DOUBLE:
            dst_value.double_value = opr1_value.double_value / opr2_value.double_value;
            break;

        case OP_EQUAL_INT:
            dst_value.bool_value = opr1_value.int_value == opr2_value.int_value;
            break;

        case OP_EQUAL_FLOAT:
            dst_value.bool_value = opr1_value.float_value == opr2_value.float_value;
            break;

        case OP_EQUAL_DOUBLE:
            dst_value.bool_value = opr1_value.double_value == opr2_value.double_value;
            break;

        case OP_GREATER_INT:
            dst_value.bool_value = opr1_value.int_value > opr2_value.int_value;
            break;

        case OP_GREATER_FLOAT:
            dst_value.bool_value = opr1_value.float_value > opr2_value.float_value;
            break;

        case OP_GREATER_DOUBLE:
            dst_value.bool_value = opr1_value.double_value > opr2_value.double_value;
            break;

        case OP_GREATER_EQUAL_INT:
            dst_value.bool_value = opr1_value.int_value >= opr2_value.int_value;
            break;

        case OP_GREATER_EQUAL_FLOAT:
            dst_value.bool_value = opr1_value.float_value >= opr2_value.float_value;
            break;

        case OP_GREATER_EQUAL_DOUBLE:
            dst_value.bool_value = opr1_value.double_value >= opr2_value.double_value;
            break;

        case OP_LESS_INT:
            dst_value.bool_value = opr1_value.int_value < opr2_value.int_value;
            break;

        case OP_LESS_FLOAT:
            dst_value.bool_value = opr1_value.float_value < opr2_value.float_value;
            break;

        case OP_LESS_DOUBLE:
            dst_value.bool_value = opr1_value.double_value < opr2_value.double_value;
            break;

        case OP_LESS_EQUAL_INT:
            dst_value.bool_value = opr1_value.int_value <= opr2_value.int_value;
            break;

        case OP_LESS_EQUAL_FLOAT:
            dst_value.bool_value = opr1_value.float_value <= opr2_value.float_value;
            break;

        case OP_LESS_EQUAL_DOUBLE:
            dst_value.bool_value = opr1_value.double_value <= opr2_value.double_value;
            break;

        case OP_NOT_EQUAL_INT:
            dst_value.bool_value = opr1_value.int_value != opr2_value.int_value;
            break;

        case OP_NOT_EQUAL_FLOAT:
            dst_value.bool_value = opr1_value.float_value != opr2_value.float_value;
            break;

        case OP_NOT_EQUAL_DOUBLE:
            dst_value.bool_value = opr1_value.double_value != opr2_value.double_value;
            break;

        default:
            throw "This should not be executed.";
            break;
    }
}

void dual_opr_instr_generate_node(Quaternion& quaternion, bool can_swap)
{
    int opr1_node_index, opr2_node_index;

    dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);

    // if two operands are const, can directly calc it
    if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
        int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
        set_const_value(quaternion, dag_nodes[result_node_index]->const_value, dag_nodes[opr1_node_index]->const_value, dag_nodes[opr2_node_index]->const_value);
    }
    else {
        int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index, can_swap);
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
}

void calculate_use_def_sets()
{
    bool** symbol_status = new bool*[symbol_table.size()]; // 0 - use ; 1 - def
    for (int i = 0; i < symbol_table.size(); ++i) {
        symbol_status[i] = new bool[2];
    }

    for (BaseBlock& base_block : base_blocks) {

        for (int i = 0; i < symbol_table.size(); ++i) {
            symbol_status[i][0] = false;
            symbol_status[i][1] = false;
        }

        unordered_set<int> temp_use_set;
        unordered_set<int> temp_def_set;
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
        for (int symbol_index : temp_use_set) {
            if (!symbol_table[symbol_index].is_const && !(symbol_table[symbol_index].is_array && !symbol_table[symbol_index].is_temp)) {
                base_block.use_set.emplace(symbol_index);
            }
        }


        for (int symbol_index : temp_def_set) {
            if (!symbol_table[symbol_index].is_const && !(symbol_table[symbol_index].is_array && !symbol_table[symbol_index].is_temp)) {
                base_block.def_set.emplace(symbol_index);
            }
        }
    }

    for (int i = 0; i < symbol_table.size(); ++i) {
        delete[] symbol_status[i];
    }
    delete[] symbol_status;
}

void calculate_active_symbol_sets(bool need_process_call_par)
{
    calculate_use_def_sets();

    // calculate in_set and out_set
    // referenced from ISBN 978-7-302-38141-9  P263
    for (BaseBlock& baseBlock : base_blocks) {
        baseBlock.out_set.clear();
        baseBlock.in_set = unordered_set<int>(baseBlock.use_set);
    }

    bool change = true;
    while (change) {
        change = false;

        for (BaseBlock& baseBlock : base_blocks) {
            bool single_change = false;
            if (baseBlock.next_block_0_ptr != MAX_INT) {
                for (int symbol_index : base_blocks[baseBlock.next_block_0_ptr].in_set) {
                    auto find_iter = baseBlock.out_set.find(symbol_index);
                    if (find_iter == baseBlock.out_set.end()) {
                        // not exist
                        baseBlock.out_set.emplace(symbol_index);
                        single_change = true;
                    }
                }
            }

            if (baseBlock.next_block_1_ptr != MAX_INT) {
                for (int symbol_index : base_blocks[baseBlock.next_block_1_ptr].in_set) {
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

                for (int symbol_index : baseBlock.out_set) {
                    auto find_iter = baseBlock.def_set.find(symbol_index);
                    if (find_iter == baseBlock.def_set.end()) {
                        // not in def set
                        baseBlock.in_set.emplace(symbol_index);
                    }
                }
            }
        }
    }

    if (need_process_call_par) {
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
    }

#ifdef OPTIMIZE_DEBUG
    cout << "Base Block Symbol Info" << endl;
    cout << endl;
    for (int i = 0; i < base_blocks.size(); ++i) {
        cout << i << endl;

        cout << "Use\t";
        for (int symbol_index : base_blocks[i].use_set) {
            cout << symbol_table[symbol_index].content << '\t';
        }
        cout << endl;

        cout << "Def\t";
        for (int symbol_index : base_blocks[i].def_set) {
            cout << symbol_table[symbol_index].content << '\t';
        }
        cout << endl;

        cout << "In\t";
        for (int symbol_index : base_blocks[i].in_set) {
            cout << symbol_table[symbol_index].content << '\t';
        }
        cout << endl;

        cout << "Out\t";
        for (int symbol_index : base_blocks[i].out_set) {
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
    if (dag_node.opr1_ptr != MAX_INT) {
        opr1_index = generate_quaternions(*dag_nodes[dag_node.opr1_ptr], target_sequence);
    }

    if (dag_node.opr2_ptr != MAX_INT) {
        opr2_index = generate_quaternions(*dag_nodes[dag_node.opr2_ptr], target_sequence);
    }

    dag_node.is_visited = true;

    if (dag_node.op == OP_INVALID) {
        // this node is a const / active variable
//        dag_node.symbol_index = *(dag_node.represent_variables.begin());

        return dag_node.symbol_index;
    }
    else {
//        dag_node.symbol_index = get_temp_symbol(get_result_data_type(dag_node.op, (dag_node.opr1_ptr != MAX_INT ? symbol_table[opr1_index].data_type : DT_VOID),
//                                                                     (dag_node.opr2_ptr != MAX_INT ? symbol_table[opr1_index].data_type : DT_VOID) ), false);
        target_sequence.push_back({dag_node.op, (dag_node.opr1_ptr != MAX_INT ? opr1_index : -1),
                                   (dag_node.opr2_ptr != MAX_INT ? opr2_index : -1), dag_node.symbol_index});

        return dag_node.symbol_index;
    }
}

void print_dag_nodes()
{
    cout << endl;

    cout << "Block DAG node info" << endl;
    for (int i = 0; i < dag_nodes.size(); ++i) {
        DAGNode* dag_node_ptr = dag_nodes[i];
        cout << i << '\t' << OP_TOKEN[dag_node_ptr->op] << '\t' << symbol_table[dag_node_ptr->symbol_index].content << '\t' << (dag_node_ptr->is_const ? "Const" : "Var") << '\t' << dag_node_ptr->opr1_ptr << '\t' << dag_node_ptr->opr2_ptr << endl;
    }

    cout << "End DAG node info" << endl;

    cout << endl;
}

void optimize_base_block(BaseBlock& base_block)
{
    if (!base_block.is_reachable) {
        // generate nop instr
        for (int i = base_block.start_index; i <= base_block.end_index; ++i) {
            base_block.block_quaternion_sequence.push_back(quaternion_sequence[i]);
        }
        return;
    }

    variable_dag_node_map = new int[symbol_table.size()];
    memset(variable_dag_node_map, -1, symbol_table.size() * sizeof(int));

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
            case OP_ADD_FLOAT:
            case OP_ADD_DOUBLE:
            case OP_MULTI_INT:
            case OP_MULTI_FLOAT:
            case OP_MULTI_DOUBLE:

            case OP_EQUAL_INT:
            case OP_EQUAL_FLOAT:
            case OP_EQUAL_DOUBLE:

            case OP_NOT_EQUAL_INT:
            case OP_NOT_EQUAL_FLOAT:
            case OP_NOT_EQUAL_DOUBLE:
            {
                dual_opr_instr_generate_node(quaternion, true);
                break;
            }

            case OP_MINUS_INT:
            case OP_MINUS_FLOAT:
            case OP_MINUS_DOUBLE:
            case OP_DIV_INT:
            case OP_DIV_FLOAT:
            case OP_DIV_DOUBLE:

            case OP_GREATER_INT:
            case OP_GREATER_FLOAT:
            case OP_GREATER_DOUBLE:
            case OP_GREATER_EQUAL_INT:
            case OP_GREATER_EQUAL_FLOAT:
            case OP_GREATER_EQUAL_DOUBLE:
            case OP_LESS_INT:
            case OP_LESS_FLOAT:
            case OP_LESS_DOUBLE:
            case OP_LESS_EQUAL_INT:
            case OP_LESS_EQUAL_FLOAT:
            case OP_LESS_EQUAL_DOUBLE:
            {
                dual_opr_instr_generate_node(quaternion, false);
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
            {
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];
                int opr2_node_index = variable_dag_node_map[quaternion.opr2];

                assert(opr1_node_index != -1 && opr2_node_index != -1);

                // if two operands are const, can directly calc it
                if (dag_nodes[opr1_node_index]->is_const && dag_nodes[opr2_node_index]->is_const) {
                    if (dag_nodes[opr1_node_index]->const_value.double_value == dag_nodes[opr2_node_index]->const_value.double_value) {
                        // can directly jump
                        quaternion_sequence[i] = {OP_JMP, -1, -1, quaternion.result};
                    }
                    else {
                        quaternion_sequence[i] = {OP_NOP, -1, -1, -1};
                    }
                }
                else {
                    base_block.out_set.emplace(quaternion.opr1);
                    base_block.out_set.emplace(quaternion.opr2);
                }

                break;
            }

            case OP_JNZ:
            {
                // get opr1 node
                int opr1_node_index = variable_dag_node_map[quaternion.opr1];

                assert(opr1_node_index != -1);

                if (dag_nodes[opr1_node_index]->is_const) {
                    switch (symbol_table[quaternion.opr1].data_type) {
                        case DT_BOOL:
                            if (dag_nodes[opr1_node_index]->const_value.bool_value != 0) {
                                // can directly jump
                                quaternion_sequence[i] = {OP_JMP, -1, -1, quaternion.result};
                            }
                            else {
                                quaternion_sequence[i] = {OP_NOP, -1, -1, -1};
                            }
                            break;

                        case DT_INT:
                            if (dag_nodes[opr1_node_index]->const_value.int_value != 0) {
                                // can directly jump
                                quaternion_sequence[i] = {OP_JMP, -1, -1, quaternion.result};
                            }
                            else {
                                quaternion_sequence[i] = {OP_NOP, -1, -1, -1};
                            }
                            break;

                        case DT_FLOAT:
                            if (dag_nodes[opr1_node_index]->const_value.float_value != 0.0f) {
                                // can directly jump
                                quaternion_sequence[i] = {OP_JMP, -1, -1, quaternion.result};
                            }
                            else {
                                quaternion_sequence[i] = {OP_NOP, -1, -1, -1};
                            }

                            break;

                        case DT_DOUBLE:
                            if (dag_nodes[opr1_node_index]->const_value.double_value != 0.0) {
                                // can directly jump
                                quaternion_sequence[i] = {OP_JMP, -1, -1, quaternion.result};
                            }
                            else {
                                quaternion_sequence[i] = {OP_NOP, -1, -1, -1};
                            }
                            break;

                        default:
                            break;
                    }

                }
                else {
                    base_block.out_set.emplace(quaternion.opr1);
                }
                break;
            }
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

                // try to find common expr
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
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
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
            {
                base_block.out_set.insert(quaternion.opr1);
                break;
            }

            case OP_RETURN:
            {
                if (quaternion.opr1 != -1) {
                    base_block.out_set.insert(quaternion.opr1);
                }

                break;
            }

            case OP_CALL:
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.int_value = (int)dag_nodes[opr1_node_index]->const_value.bool_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.float_value = (float)dag_nodes[opr1_node_index]->const_value.bool_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.double_value = (double)dag_nodes[opr1_node_index]->const_value.bool_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.bool_value = (bool)dag_nodes[opr1_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
//                    cout << i << '\t' << opr1_node_index << " INT TO FLOAT Hit\t" << symbol_table[quaternion.opr1].content << endl;
                }

                // erase current records
//                int current_node_index = variable_dag_node_map[quaternion.result];
//                if (current_node_index != -1) {
//                    // need erase
//                    dag_nodes[current_node_index].represent_variables.erase(quaternion.result);
//                }

                if (dag_nodes[opr1_node_index]->is_const) {
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.float_value = (float)dag_nodes[opr1_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.double_value = (double)dag_nodes[opr1_node_index]->const_value.int_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.bool_value = (bool)dag_nodes[opr1_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.int_value = (int)dag_nodes[opr1_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.double_value = (double)dag_nodes[opr1_node_index]->const_value.float_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.bool_value = (bool)dag_nodes[opr1_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.int_value = (int)dag_nodes[opr1_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                    int result_node_index = create_dag_node(quaternion.result, OP_INVALID, true);
                    dag_nodes[result_node_index]->const_value.float_value = (float)dag_nodes[opr1_node_index]->const_value.double_value;
                }
                else {
                    int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, MAX_INT, false);
                    if (search_common_expr_index == -1) {
                        // no such common expr
                        int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
                        dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
                        dag_nodes[result_node_index]->opr2_ptr = MAX_INT;
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
                base_block.out_set.insert(quaternion.opr1);
                base_block.out_set.insert(quaternion.opr2);
//                int opr1_node_index, opr2_node_index;
//
//                dual_opr_preset(opr1_node_index, opr2_node_index, quaternion);
//
//                int search_common_expr_index = search_matched_node(quaternion.op_code, opr1_node_index, opr2_node_index,
//                                                                   false);
//                if (search_common_expr_index == -1) {
//                    // no such common expr
//                    int result_node_index = create_dag_node(quaternion.result, quaternion.op_code);
//                    dag_nodes[result_node_index]->opr1_ptr = opr1_node_index;
//                    dag_nodes[result_node_index]->opr2_ptr = opr2_node_index;
//                    base_block.array_store_dags.insert(result_node_index);
//                }
//                else {
//                    // find common expr
//                    dag_nodes[search_common_expr_index]->represent_variables.emplace(quaternion.result);
//                    variable_dag_node_map[quaternion.result] = search_common_expr_index;
//                }

                break;
            }

            default:
//                throw "Not Supported!";
                break;
        }
    }

    // give value to const values in symbol table
    for (int i = 0; i < symbol_table.size(); ++i) {
//        if (symbol_table[i].is_const && variable_dag_node_map[i] != -1) {
        if (variable_dag_node_map[i] != -1) { // not checking is_const, because a variable may have a const value
            symbol_table[i].value = dag_nodes[variable_dag_node_map[i]]->const_value;

            // if a (// non-out active) temp symbol is var but here node is const, assign it to const
            if (!symbol_table[i].is_const && symbol_table[i].is_temp && !symbol_table[i].is_array && dag_nodes[variable_dag_node_map[i]]->is_const) {
                symbol_table[i].is_const = true;
//                auto find_result = base_block.out_set.find(i);
//                if (find_result == base_block.out_set.end()) {
//                    symbol_table[i].is_const = true;
//                }
//                else {
//                    // is an out active temp symbol
//                }
            }
        }
    }


    // set the symbol_index of all dag node which represents out active symbol as out active symbol index respectively
    for (int out_active_symbol : base_block.out_set) {
        if (variable_dag_node_map[out_active_symbol] != -1) {
            dag_nodes[variable_dag_node_map[out_active_symbol]]->symbol_index = out_active_symbol;
        }
    }

    // add used / assigned arr to out set
//    for (int i = base_block.start_index; i <= base_block.end_index; ++i) {
//        Quaternion& quaternion = quaternion_sequence[i];
//        if (quaternion.op_code == OP_ARRAY_STORE) {
//            base_block.out_set.emplace(quaternion.result);
//        }
//        else if (quaternion.op_code >= OP_FETCH_BOOL && quaternion.op_code <= OP_FETCH_DOUBLE) {
//            base_block.out_set.emplace(quaternion.opr1);
//        }
//    }

#ifdef OPTIMIZE_DEBUG
    print_dag_nodes();
#endif

    // generate quaternion sequence
    for (int out_active_symbol : base_block.out_set) {
        if (variable_dag_node_map[out_active_symbol] != -1) {
            // check if this variable is const and generate assign instr
            if (dag_nodes[variable_dag_node_map[out_active_symbol]]->op == OP_INVALID) {
                // get a const value == this variable
                int value_temp_var = -1;

                for (int symbol_index : dag_nodes[variable_dag_node_map[out_active_symbol]]->represent_variables) {
                    if (symbol_table[symbol_index].is_const || (base_block.in_set.find(symbol_index) != base_block.in_set.end())) {
                        value_temp_var = symbol_index;
                        break;
                    }
                }

                if (value_temp_var >= 0 && value_temp_var != out_active_symbol) {
                    base_block.block_quaternion_sequence.push_back({OP_ASSIGNMENT, value_temp_var, -1, (int)(out_active_symbol)});
                }
            }
            generate_quaternions(*dag_nodes[variable_dag_node_map[out_active_symbol]], base_block.block_quaternion_sequence);
        }
    }

//    // generate array store
//    for (int array_store_dag_idx : base_block.array_store_dags) {
//        generate_quaternions(*dag_nodes[array_store_dag_idx], base_block.block_quaternion_sequence);
//    }

    // process 2 out active var use one DAG node
    for (int out_active_symbol : base_block.out_set) {
        if (variable_dag_node_map[out_active_symbol] != -1 && dag_nodes[variable_dag_node_map[out_active_symbol]]->symbol_index != out_active_symbol) {
            base_block.block_quaternion_sequence.push_back({OP_ASSIGNMENT, dag_nodes[variable_dag_node_map[out_active_symbol]]->symbol_index,
                                                            -1, (int)out_active_symbol});
        }
    }

    // insert PAR instr
    for (int i = base_block.start_index; i <= base_block.end_index; ++i) {
        if (quaternion_sequence[i].op_code == OP_PAR) {
            if (variable_dag_node_map[quaternion_sequence[i].opr1] == -1) {
                base_block.block_quaternion_sequence.push_back({OP_PAR, quaternion_sequence[i].opr1, -1, -1});
            }
            else {
                base_block.block_quaternion_sequence.push_back({OP_PAR, dag_nodes[variable_dag_node_map[quaternion_sequence[i].opr1]]->symbol_index, -1, -1});
            }
        }
    }

    // insert nop instructions to match jump offset
    int nop_instr_cnt = base_block.end_index - base_block.start_index - base_block.block_quaternion_sequence.size();
    for (int i = 0; i < nop_instr_cnt; ++i) {
        base_block.block_quaternion_sequence.push_back({OP_NOP, -1, -1, -1});
    }

    // add last one
    Quaternion post_process_last_quaternion = quaternion_sequence[base_block.end_index];
    if (post_process_last_quaternion.op_code == OP_CALL || post_process_last_quaternion.op_code == OP_JMP ||
            post_process_last_quaternion.op_code == OP_JNZ || post_process_last_quaternion.op_code == OP_JEQ
            || post_process_last_quaternion.op_code == OP_RETURN || post_process_last_quaternion.op_code == OP_ARRAY_STORE) {
        if (post_process_last_quaternion.opr1 != -1 && OP_CODE_OPR_USAGE[post_process_last_quaternion.op_code][0] == USAGE_VAR
        && variable_dag_node_map[post_process_last_quaternion.opr1] != -1) {
            post_process_last_quaternion.opr1 = dag_nodes[variable_dag_node_map[post_process_last_quaternion.opr1]]->symbol_index;
        }

        if (post_process_last_quaternion.opr2 != -1 && OP_CODE_OPR_USAGE[post_process_last_quaternion.op_code][1] == USAGE_VAR
        && variable_dag_node_map[post_process_last_quaternion.opr2] != -1) {
            post_process_last_quaternion.opr2 = dag_nodes[variable_dag_node_map[post_process_last_quaternion.opr2]]->symbol_index;
        }

        base_block.block_quaternion_sequence.push_back(post_process_last_quaternion);
    }
    else if (base_block.end_index - base_block.start_index + 1 != base_block.block_quaternion_sequence.size()){
        base_block.block_quaternion_sequence.push_back({OP_NOP, -1, -1, -1});
    }


    // optimize JNZ (x86 optimize)
    if (base_block.block_quaternion_sequence.back().op_code == OP_JNZ) {
        // find last def of opr1
        for (int i = base_block.block_quaternion_sequence.size() - 1; i >= 0; --i) {
            int last_opr1 = base_block.block_quaternion_sequence.back().opr1;
            if (OP_CODE_OPR_USAGE[base_block.block_quaternion_sequence[i].op_code][2] == USAGE_VAR && base_block.block_quaternion_sequence[i].result == last_opr1) {
                // check used in below
                bool can_optimize = true;
                for (int idx = i + 1; idx < base_block.block_quaternion_sequence.size() - 1; ++idx) {
                    Quaternion& quaternion = base_block.block_quaternion_sequence[idx];

                    if (quaternion.opr1 == last_opr1 && OP_CODE_OPR_USAGE[quaternion.op_code][0] == USAGE_VAR) {
                        can_optimize = false;
                        break;
                    }

                    if (quaternion.opr2 == last_opr1 && OP_CODE_OPR_USAGE[quaternion.op_code][1] == USAGE_VAR) {
                        can_optimize = false;
                        break;
                    }
                }

                if (can_optimize) {
                    // put instr which def opr1 to the last two
                    Quaternion def_quaternion = base_block.block_quaternion_sequence[i];
                    for (int idx = i + 1; idx < base_block.block_quaternion_sequence.size() - 1; ++idx) {
                        base_block.block_quaternion_sequence[idx - 1] = base_block.block_quaternion_sequence[idx];
                    }

                    base_block.block_quaternion_sequence[base_block.block_quaternion_sequence.size() - 2] = def_quaternion;
                }
                break;
            }
        }
    }

    // free memory
    for (DAGNode* dag_node : dag_nodes) {
        delete dag_node;
    }
    dag_nodes.clear();

    delete[] variable_dag_node_map;
//    throw "Not implemented!";
}

void unreachable_block_set_nop(vector<Quaternion>& target_sequence)
{
    for (BaseBlock& base_block : base_blocks) {
        if (!base_block.is_reachable) {
            for (int i = base_block.start_index; i <= base_block.end_index; ++i) {
                target_sequence[i] = {OP_NOP, -1, -1, -1};
            }
        }
    }
}

// clean up optimized_sequence
void code_clean_up()
{
    // prefix
    int* nop_instr_cnt = new int[base_blocks.size() + 1];

    int cnt = 0;

    nop_instr_cnt[0] = 0;
    for (int i = 0; i < base_blocks.size(); ++i) {
        for (int idx = base_blocks[i].start_index; idx <= base_blocks[i].end_index; ++idx) {
            if (optimized_sequence[idx].op_code == OP_NOP) {
                ++cnt;
            }
        }

        nop_instr_cnt[i + 1] = cnt;
    }

#ifdef OPTIMIZE_DEBUG
    for (int i = 0; i <= base_blocks.size(); ++i) {
        cout << nop_instr_cnt[i] << '\t';
    }
    cout << endl;
#endif

    // remove nop & adjust jmp / jnz / jeq target
    vector<Quaternion> removed_sequence;
    for (int block_idx = 0; block_idx < base_blocks.size(); ++block_idx) {
        BaseBlock& base_block = base_blocks[block_idx];
        for (int i = base_block.start_index; i < base_block.end_index; ++i) {
            if (optimized_sequence[i].op_code != OP_NOP) {
                removed_sequence.push_back(optimized_sequence[i]);
            }
        }

        Quaternion last_quaternion = optimized_sequence[base_block.end_index];
        if (last_quaternion.op_code == OP_JMP || last_quaternion.op_code == OP_JNZ || last_quaternion.op_code == OP_JEQ) {
            // adjust jump target
            int dst_block_idx = search_base_block(base_block.end_index + last_quaternion.result);
            last_quaternion.result -= nop_instr_cnt[dst_block_idx] - nop_instr_cnt[block_idx + 1];

            removed_sequence.push_back(last_quaternion);
        }
        else {
            if (last_quaternion.op_code != OP_NOP) {
                removed_sequence.push_back(last_quaternion);
            }
        }
    }

    optimized_sequence = removed_sequence;

    // update function entrance
    for (Function& function : Function::function_table) {
        int base_block_idx = search_base_block(function.entry_address);
        assert(base_block_idx >= 0);
        function.entry_address -= nop_instr_cnt[base_block_idx];
    }

    delete[] nop_instr_cnt;
}

void symbol_clean_up()
{
    int* old_new_mapping = new int[symbol_table.size()];
    memset(old_new_mapping, -1, symbol_table.size() * sizeof(int));

    for (SymbolEntry& symbol_entry : symbol_table) {
        symbol_entry.is_used = false;
    }

    for (Quaternion& quaternion : optimized_sequence) {
        if (quaternion.opr1 != -1 && (OP_CODE_OPR_USAGE[quaternion.op_code][0] == USAGE_VAR || OP_CODE_OPR_USAGE[quaternion.op_code][0] == USAGE_ARRAY)) {
            symbol_table[quaternion.opr1].is_used = true;
        }

        if (quaternion.opr2 != -1 && (OP_CODE_OPR_USAGE[quaternion.op_code][1] == USAGE_VAR || OP_CODE_OPR_USAGE[quaternion.op_code][1] == USAGE_ARRAY)) {
            symbol_table[quaternion.opr2].is_used = true;
        }

        if (quaternion.result != -1 && (OP_CODE_OPR_USAGE[quaternion.op_code][2] == USAGE_VAR || OP_CODE_OPR_USAGE[quaternion.op_code][2] == USAGE_ARRAY)) {
            symbol_table[quaternion.result].is_used = true;
        }
    }

    // all function parameter is used
    for (Function& function : Function::function_table) {
        for (int symbol : function.parameter_index) {
            symbol_table[symbol].is_used = true;
        }
    }

    vector<SymbolEntry> used_temp_symbol_entry;
    for (int i = 0; i < symbol_table.size(); ++i) {
        if (symbol_table[i].is_used) {
            old_new_mapping[i] = used_temp_symbol_entry.size();
            used_temp_symbol_entry.push_back(symbol_table[i]);
        }
    }

    symbol_table = used_temp_symbol_entry;

    for (Quaternion& quaternion : optimized_sequence) {
        if (quaternion.opr1 != -1 && (OP_CODE_OPR_USAGE[quaternion.op_code][0] == USAGE_VAR || OP_CODE_OPR_USAGE[quaternion.op_code][0] == USAGE_ARRAY)) {
            quaternion.opr1 = old_new_mapping[quaternion.opr1];
        }

        if (quaternion.opr2 != -1 && (OP_CODE_OPR_USAGE[quaternion.op_code][1] == USAGE_VAR || OP_CODE_OPR_USAGE[quaternion.op_code][1] == USAGE_ARRAY)) {
            quaternion.opr2 = old_new_mapping[quaternion.opr2];
        }

        if (quaternion.result != -1 && (OP_CODE_OPR_USAGE[quaternion.op_code][2] == USAGE_VAR || OP_CODE_OPR_USAGE[quaternion.op_code][2] == USAGE_ARRAY)) {
            quaternion.result = old_new_mapping[quaternion.result];
        }
    }

    // update function param table
    for (Function& function : Function::function_table) {
        for (int i = 0; i < function.parameter_index.size(); ++i) {
            function.parameter_index[i] = old_new_mapping[function.parameter_index[i]];
        }
    }

    delete[] old_new_mapping;
// todo not implemented
}

void set_array_size()
{
    for (SymbolEntry& symbol_entry : symbol_table) {
        if (symbol_entry.is_array && !symbol_entry.is_temp) {
            int size = 1;
            Node* indices_node;
            if (symbol_entry.node_ptr->content == "Parameter") {
                indices_node = symbol_entry.node_ptr->child_nodes_ptr[2];
            }
            else {
                indices_node = symbol_entry.node_ptr->child_nodes_ptr[1];
            }

            while (indices_node->child_nodes_ptr.size() == 4) {
                size *= symbol_table[indices_node->get_attribute_value(intIndicesSizeIndexAttr)].value.int_value;
                indices_node = indices_node->child_nodes_ptr[3];
            }
            size *= symbol_table[indices_node->get_attribute_value(intIndicesSizeIndexAttr)].value.int_value;
            symbol_entry.memory_size = size * BASE_DATA_TYPE_SIZE[symbol_entry.data_type];
        }
    }
}

void optimize_IR(vector<Quaternion>& quaternion_sequence)
{
    split_base_blocks(quaternion_sequence);

    calculate_active_symbol_sets(true);

    for (BaseBlock& baseBlock : base_blocks) {
        optimize_base_block(baseBlock);
    }

    set_array_size();

    optimized_sequence.clear();
    for (BaseBlock& baseBlock : base_blocks) {
        for (Quaternion& quaternion : baseBlock.block_quaternion_sequence) {
            optimized_sequence.push_back(quaternion);
        }
    }

    // split base block again
    split_base_blocks(optimized_sequence);
    unreachable_block_set_nop(optimized_sequence);

    code_clean_up();
    symbol_clean_up();

    quaternion_sequence.clear();
    quaternion_sequence.assign(optimized_sequence.begin(), optimized_sequence.end());
//    quaternion_sequence = optimized_sequence;
}

void print_optimize_sequence()
{
    print_quaternion_sequence(optimized_sequence);
}

void write_optimize_result()
{
    // write quaternion
    ofstream fout("Quaternion.txt");
    int cnt = 0;
    for (int i = 0; i < optimized_sequence.size(); ++i) {
        Quaternion& quaternion = optimized_sequence[i];
        fout << setiosflags(ios::left) << setw(6) << cnt << setiosflags(ios::left) << setw(12) << OP_TOKEN[quaternion.op_code] << "\t";

        switch (quaternion.op_code) {
            case OP_CALL:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << Function::function_table[quaternion.opr1].name << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << quaternion.opr2 << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "-" << endl;
                break;

            case OP_JMP:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "-" << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "-" << quaternion.result + i << endl;
                break;

            case OP_JEQ:
            case OP_JNZ:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << symbol_table[quaternion.opr1].content << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (quaternion.opr2 < 0 ? "-" : symbol_table[quaternion.opr2].content) << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << quaternion.result + i << endl;
                break;

            case OP_LI_BOOL:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (quaternion.opr1 ? "true" : "false") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "-"  << setiosflags(ios::left) << setw(DISPLAY_WIDTH)<< symbol_table[quaternion.result].content << endl;
                break;

            case OP_LI_INT:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << quaternion.opr1 << "" << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "-" << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << symbol_table[quaternion.result].content << endl;
                break;

            case OP_LI_DOUBLE: {
                double double_num;
                int *int_ptr = (int *) &double_num;
                int_ptr[0] = quaternion.opr1;
                int_ptr[1] = quaternion.opr2;
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << double_num << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << symbol_table[quaternion.result].content << endl;
                break;
            }

            case OP_LI_FLOAT: {
                float float_num = *((float*)(&quaternion.opr1));
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << float_num << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << symbol_table[quaternion.result].content << endl;
                break;
            }

//            case OP_FETCH_BOOL:
//            case OP_FETCH_INT:
//            case OP_FETCH_FLOAT:
//            case OP_FETCH_DOUBLE:
//            case OP_ARRAY_STORE:
//            {
//                fout << symbol_table[quaternion.opr1].content << "\t" << quaternion.opr2 << "\t" << symbol_table[quaternion.result].content << endl;
//                break;
//            };

            default:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (quaternion.opr1 < 0 ? "-" : symbol_table[quaternion.opr1].content) << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (quaternion.opr2 < 0 ? "-" : symbol_table[quaternion.opr2].content) << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (quaternion.result < 0 ? "-" : symbol_table[quaternion.result].content) << endl;
                break;
        }
        ++cnt;
    }
    fout.close();

    // write symbol table
    fout.open("SymbolTable.txt");
    cnt = 0;
    for (SymbolEntry& symbolEntry : symbol_table) {
        fout << setiosflags(ios::left) << setw(6) << cnt << setiosflags(ios::left) << setw(12) << symbolEntry.content << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << DATA_TYPE_TOKEN[symbolEntry.data_type] << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_temp ? "Temp" : "Declare") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) <<
             (symbolEntry.function_index < 0 ? "Global" : Function::function_table[symbolEntry.function_index].name) << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_used ? "Used" : "Unused") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_initial ? "Initialed" : "Uninitiated")   << endl;
        ++cnt;
    }
    fout.close();

    // write function table
    fout.open("FunctionTable.txt");
    cnt = 0;
    for (Function& function : Function::function_table) {
        fout << setiosflags(ios::left) << setw(6) << cnt << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << function.name << "EntryAddr : " << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << function.entry_address << '\t' << "ReturnData: " << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << DATA_TYPE_TOKEN[function.return_data_type] << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "Args: ";

        for (int parameter_index : function.parameter_index) {
            fout << DATA_TYPE_TOKEN[symbol_table[parameter_index].data_type] << '\t';
        }
        fout << endl;
        ++cnt;
    }
    fout.close();
}
