//
// Project: Porphyrin
// File Name: x86Assemble.cpp
// Author: Morning
// Description:
//
// Create Date: 2022/2/26
//

#include "Include/x86Assemble.h"
#include "Include/Optimize.h"
#include <cassert>

#define INVALID_MEMORY_OFFSET 2147483647

#define ACTIVE_AT_EXIT -2
#define NON_ACTIVE -1

#define X64GPRCNT 14

#define X64XMMCNT 16

#define SCORE_NEXT_USE_RATE 2
#define SCORE_VAR_IN_MEM_RATE 4


// architecture related variables
string ip_str;
string bp_str;

string function_name_prefix;

const unordered_map<string, string> condition_negative_map = {
        {"ne", "e"},
        {"e", "ne"},
        {"g", "le"},
        {"ge", "l"},
        {"l", "ge"},
        {"le", "g"},
        {"a", "be"},
        {"ae", "b"},
        {"b", "ae"},
        {"be", "a"}
};

bool inline opr_is_stored_in_xmm(SymbolEntry& symbol);

void init_symbol_table_offset() {
    for (SymbolEntry &symbol: symbol_table) {
        symbol.memory_offset = INVALID_MEMORY_OFFSET;
    }

    // process function parameter offset
    int address_length = 8;
    for (Function &function: Function::function_table) {
        // don't set parameter which will be stored in register
        // count parameter store in xmm
        int xmm_cnt = 0;
        for (int param_idx : function.parameter_index) {
            if (opr_is_stored_in_xmm(symbol_table[param_idx])) {
                ++xmm_cnt;
            }
        }

        int store_in_mem_xmm = max(0, xmm_cnt - X64XMMCNT + 1); // float / double parameter which will store in memory
        int store_in_mem_gpr = max(0, (int)(function.parameter_index.size() - xmm_cnt) - X64GPRCNT + 1); // other parameter which will store in memory

        int mem_xmm_cnt = 0; // processed float/double parameter which will store in memory count
        int mem_gpr_cnt = 0; // processed other parameter which will store in memory count
        int current_offset = address_length * 2;
        for (int param_idx: function.parameter_index) {
            if (opr_is_stored_in_xmm(symbol_table[param_idx])) {
                ++mem_xmm_cnt;
            }
            else {
                ++mem_gpr_cnt;
            }

            if (store_in_mem_xmm < mem_xmm_cnt || store_in_mem_gpr < mem_gpr_cnt) {
                break;
            }

            symbol_table[param_idx].memory_offset = current_offset;
            current_offset += address_length;
//            if (symbol_table[param_idx].is_array) {
//                current_offset += address_length;
//            } else {
//                assert(symbol_table[param_idx].data_type != DT_VOID);
//                current_offset += BASE_DATA_TYPE_SIZE[symbol_table[param_idx].data_type];
//            }
        }
    }
}

string get_symbol_store_text(int symbol_index);

vector<QuaternionActiveInfo> quaternion_active_info_table;

void generate_active_info_table(BaseBlock &base_block) {

    int *symbol_status = new int[symbol_table.size()];

    // initial symbol status
    memset(symbol_status, -1, symbol_table.size() * sizeof(int));

    // set out active symbols
    for (int out_symbol_idx: base_block.out_set) {
        symbol_status[out_symbol_idx] = ACTIVE_AT_EXIT;
    }

    // traverse quaternion sequence in reverse order
    vector<QuaternionActiveInfo> reverse_info_table;
    for (int i = base_block.end_index; i >= base_block.start_index; --i) {
        Quaternion &current_quaternion = optimized_sequence[i];
        QuaternionActiveInfo current_active_info{NON_ACTIVE, NON_ACTIVE, NON_ACTIVE};
        if (OP_CODE_OPR_USAGE[current_quaternion.op_code][2] == USAGE_ARRAY ||
            OP_CODE_OPR_USAGE[current_quaternion.op_code][2] == USAGE_VAR) {
            current_active_info.result_active_info = symbol_status[current_quaternion.result];
            symbol_status[current_quaternion.result] = NON_ACTIVE;
        }

        bool opr1_is_var_array = OP_CODE_OPR_USAGE[current_quaternion.op_code][0] == USAGE_ARRAY ||
                                 OP_CODE_OPR_USAGE[current_quaternion.op_code][0] == USAGE_VAR;
        bool opr2_is_var_array = OP_CODE_OPR_USAGE[current_quaternion.op_code][1] == USAGE_ARRAY ||
                                 OP_CODE_OPR_USAGE[current_quaternion.op_code][1] == USAGE_VAR;
        if (opr1_is_var_array) {
            current_active_info.opr1_active_info = symbol_status[current_quaternion.opr1];
        }

        if (opr2_is_var_array) {
            current_active_info.opr2_active_info = symbol_status[current_quaternion.opr2];
        }

        if (opr1_is_var_array) {
            symbol_status[current_quaternion.opr1] = i;
        }

        if (opr2_is_var_array) {
            symbol_status[current_quaternion.opr2] = i;
        }

        reverse_info_table.push_back(current_active_info);
    }

    // reverse it
    for (int i = reverse_info_table.size() - 1; i >= 0; --i) {
        quaternion_active_info_table.push_back(reverse_info_table[i]);
    }

    delete[] symbol_status;

#ifdef TARGET_DEBUG
    cout << "Active info" << endl;
    for (int i = 0; i < quaternion_active_info_table.size(); ++i) {
        cout << i + base_block.start_index << '\t' << quaternion_active_info_table[i].opr1_active_info << '\t' << quaternion_active_info_table[i].opr2_active_info
            << '\t' << quaternion_active_info_table[i].result_active_info << endl;
    }
#endif
}

vector<int> function_entrance_points;
vector<int> function_leave_correction_points;
vector<int> leave_correction_function_idx;

int current_stack_top_addr;
int last_calc_symbol;
int current_function;
int last_call_return_symbol;

unordered_set<int> gpr_variable_map[X64GPRCNT];
unordered_set<int> xmm_variable_map[16]; // the size of xmm regs

// if a register is locked, it can't be reallocated
bool gpr_locked[X64GPRCNT];
bool xmm_locked[16];

int *variable_reg_map;
int *variable_next_use;

bool inline opr_is_stored_in_xmm(SymbolEntry& symbol)
{
    return (!(symbol.is_array && !symbol.is_temp) && (symbol.data_type == DT_FLOAT || symbol.data_type == DT_DOUBLE));
}

int par_usable_gpr_index;
int par_usable_xmm_index;

bool gen_entry_code(BaseBlock &base_block, vector<string> &target_text) {
    if (last_call_return_symbol != -1) {
        if (opr_is_stored_in_xmm(symbol_table[last_call_return_symbol])) {
            xmm_variable_map[0].insert(last_call_return_symbol);
        }
        else {
            gpr_variable_map[0].insert(last_call_return_symbol);
        }

        variable_reg_map[last_call_return_symbol] = 0;
        last_call_return_symbol = -1;
    }

    for (int i = 0; i < Function::function_table.size(); ++i) {
        Function& function = Function::function_table[i];
        if (base_block.start_index == function.entry_address) {
            // generate entrance code
            target_text.push_back(function_name_prefix + function.name + ':');

            target_text.emplace_back("enter $?, $0");

            function_entrance_points.push_back(target_text.size() - 1);

            // initial array offset
            current_stack_top_addr = 0;
            for (int symbol_idx = 0; symbol_idx < symbol_table.size(); ++symbol_idx) {
                SymbolEntry& symbol = symbol_table[symbol_idx];

                if (symbol.function_index == i && symbol.is_array && !symbol.is_temp) {
                    // check if is param array
                    bool is_param = false;
                    for (int param : Function::function_table[symbol.function_index].parameter_index) {
                        if (param == symbol_idx) {
                            is_param = true;
                            break;
                        }
                    }

                    if (!is_param) {
                        int arch_unit = 8;
                        int free_space = (current_stack_top_addr) % int(BASE_DATA_TYPE_SIZE[symbol_table[symbol_idx].data_type]);

                        if (free_space != 0) {
                            current_stack_top_addr -= free_space + int(BASE_DATA_TYPE_SIZE[symbol_table[symbol_idx].data_type]);
                        }

                        current_stack_top_addr -= symbol.memory_size;

                        symbol.memory_offset = current_stack_top_addr;
                    }
                }
            }

            last_calc_symbol = -1;
            current_function = i;

            par_usable_gpr_index = 1;
            par_usable_xmm_index = 1;

            // set up parameter - register relationship
            int proc_par_usable_gpr_index = 1;
            int proc_par_usable_xmm_index = 1;
            for (int param_idx = function.parameter_index.size() - 1; param_idx >= 0; --param_idx) {
                if (opr_is_stored_in_xmm(symbol_table[function.parameter_index[param_idx]])) {
                    xmm_variable_map[proc_par_usable_xmm_index].insert(function.parameter_index[param_idx]);
                    variable_reg_map[function.parameter_index[param_idx]] = proc_par_usable_xmm_index;
                    xmm_locked[proc_par_usable_xmm_index] = false;
                    ++proc_par_usable_xmm_index;

                    if (proc_par_usable_xmm_index >= X64XMMCNT) {
                        break;
                    }
                }
                else {
                    gpr_variable_map[proc_par_usable_gpr_index].insert(function.parameter_index[param_idx]);
                    variable_reg_map[function.parameter_index[param_idx]] = proc_par_usable_gpr_index;
                    gpr_locked[proc_par_usable_gpr_index] = false;
                    ++proc_par_usable_gpr_index;

                    if (proc_par_usable_gpr_index >= X64GPRCNT) {
                        break;
                    }
                }
            }

            return true;
        }
    }

    // not an entrance
    target_text.push_back(".L" + to_string(base_block.start_index) + ':');

    return false;
}

void generate_store(int reg_idx, int store_var_idx, vector<string> &target_text) {
    string dst_str;
    if (symbol_table[store_var_idx].function_index == -1) {
        // global variable
        dst_str = symbol_table[store_var_idx].content + ip_str;
    }
    else {
        int arch_unit = 8;
        if (symbol_table[store_var_idx].memory_offset == INVALID_MEMORY_OFFSET) {
            int target_mem_offset = current_stack_top_addr - symbol_table[store_var_idx].memory_size;
            int free_space = (current_stack_top_addr) % int(BASE_DATA_TYPE_SIZE[symbol_table[store_var_idx].data_type]);
            if (free_space != 0) {
                target_mem_offset -= free_space + BASE_DATA_TYPE_SIZE[symbol_table[store_var_idx].data_type];
            }

            symbol_table[store_var_idx].memory_offset = target_mem_offset;
            current_stack_top_addr = target_mem_offset;
        }

        dst_str = to_string(symbol_table[store_var_idx].memory_offset) + bp_str;
    }


    switch (symbol_table[store_var_idx].data_type) {
        case DT_BOOL:
            target_text.push_back("movb\t%" + GPRStr[reg_idx][0] + ", " + dst_str);
            gpr_variable_map[reg_idx].erase(store_var_idx);
            break;

        case DT_INT:
            target_text.push_back("movl\t%" + GPRStr[reg_idx][2] + ", " + dst_str);
            gpr_variable_map[reg_idx].erase(store_var_idx);
            break;

        case DT_FLOAT:
            target_text.push_back("movss\t%" + XMMRegStr[reg_idx] + ", " + dst_str);
            xmm_variable_map[reg_idx].erase(store_var_idx);
            break;

        case DT_DOUBLE:
            target_text.push_back("movsd\t%" + XMMRegStr[reg_idx] + ", " + dst_str);
            xmm_variable_map[reg_idx].erase(store_var_idx);
            break;

        default:
            break;
    }

    variable_reg_map[store_var_idx] = -1;
    if (opr_is_stored_in_xmm(symbol_table[store_var_idx])) {
        xmm_locked[reg_idx] = false;
    }
    else {
        gpr_locked[reg_idx] = false;
    }
}

int search_free_register(bool is_xmm)
{
    if (is_xmm) {
        for (int i = 0; i < X64XMMCNT; ++i) {
            if (xmm_variable_map[i].empty() && !xmm_locked[i]) {
                return i;
            }
        }

        return -1;
    }
    else {
        for (int i = 0; i < X64GPRCNT; ++i) {
            if (gpr_variable_map[i].empty() && !gpr_locked[i]) {
                return i;
            }
        }

        return -1;
    }
}

int search_unlucky_gpr(int quaternion_idx)
{
    int max_score = -3;
    int chosen_reg_idx = -1;

    for (int i = 0; i < X64GPRCNT; ++i) {
        if (gpr_locked[i]) {
            continue;
        }

        // check min usage position
        int min_next_use = 2147483647;
        // find var in memory
        int var_in_memory_cnt = 0;
        for (int var_idx: gpr_variable_map[i]) {
            if (variable_next_use[var_idx] < min_next_use) {
                min_next_use = variable_next_use[var_idx];
            }

            if (symbol_table[var_idx].memory_offset != INVALID_MEMORY_OFFSET) {
                ++var_in_memory_cnt;
            }
        }

        min_next_use -= quaternion_idx;
        int current_score = min_next_use * SCORE_NEXT_USE_RATE +
                            var_in_memory_cnt * SCORE_VAR_IN_MEM_RATE;

        if (max_score < current_score) {
            max_score = current_score;
            chosen_reg_idx = i;
        }
    }

    assert(chosen_reg_idx >= 0);
    return chosen_reg_idx;
}

int search_unlucky_xmm(int quaternion_idx)
{
    int max_score = -3;
    int chosen_reg_idx = -1;

    for (int i = 0; i < X64XMMCNT; ++i) {
        // check min usage position
        int min_next_use = 2147483647;
        // find var in memory
        int var_in_memory_cnt = 0;
        for (int var_idx: xmm_variable_map[i]) {
            if (variable_next_use[var_idx] < min_next_use) {
                min_next_use = variable_next_use[var_idx];
            }

            if (symbol_table[var_idx].memory_offset != INVALID_MEMORY_OFFSET) {
                ++var_in_memory_cnt;
            }
        }

        min_next_use -= quaternion_idx;
        int current_score = min_next_use * SCORE_NEXT_USE_RATE +
                            var_in_memory_cnt * SCORE_VAR_IN_MEM_RATE;

        if (max_score < current_score) {
            max_score = current_score;
            chosen_reg_idx = i;
        }
    }

    assert(chosen_reg_idx >= 0);
    return chosen_reg_idx;
}

void backup_gpr_to_mem(int gpr_idx, int quaternion_idx, vector<string> &target_text, bool ignore_quaternion_result)
{
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];

    // generate data store code
    for (int var_idx: gpr_variable_map[gpr_idx]) {
        if (!(ignore_quaternion_result && var_idx == current_quaternion.result) && variable_next_use[var_idx] > quaternion_idx) {
            generate_store(gpr_idx, var_idx, target_text);
        }
    }
}

void backup_xmm_to_mem(int xmm_idx, int quaternion_idx, vector<string> &target_text, bool ignore_quaternion_result)
{
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];

    // generate data store code
    for (int var_idx: xmm_variable_map[xmm_idx]) {
        if (!(ignore_quaternion_result && var_idx == current_quaternion.result) && variable_next_use[var_idx] > quaternion_idx) {
            generate_store(xmm_idx, var_idx, target_text);
        }
    }
}

void move_to_gpr(int gpr_idx, int symbol_idx, vector<string> &target_text)
{
//    assert(!symbol_table[symbol_idx].is_const);

    target_text.push_back("movl\t" + get_symbol_store_text(symbol_idx) + ", %" + GPRStr[gpr_idx][2]);

    if (variable_reg_map[symbol_idx] != -1) {
        gpr_variable_map[variable_reg_map[symbol_idx]].erase(symbol_idx);
    }

    gpr_variable_map[gpr_idx].insert(symbol_idx);
    variable_reg_map[symbol_idx] = gpr_idx;
}

void move_to_xmm(int xmm_idx, int symbol_idx, vector<string> &target_text)
{
//    assert(!symbol_table[symbol_idx].is_const);

    if (symbol_table[symbol_idx].data_type == DT_FLOAT) {
        target_text.push_back("movss\t" + get_symbol_store_text(symbol_idx) + ", %" + XMMRegStr[xmm_idx]);
    }
    else {
        target_text.push_back("movsd\t" + get_symbol_store_text(symbol_idx) + ", %" + XMMRegStr[xmm_idx]);
    }

    if (variable_reg_map[symbol_idx] != -1) {
        xmm_variable_map[variable_reg_map[symbol_idx]].erase(symbol_idx);
    }

    xmm_variable_map[xmm_idx].insert(symbol_idx);
    variable_reg_map[symbol_idx] = xmm_idx;
}

// these two function load symbol to regs
int get_gpr_two_opr_with_result(int quaternion_idx, vector<string> &target_text) {
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];
    QuaternionActiveInfo &current_active_info = quaternion_active_info_table[quaternion_idx];

    int opr1_reg = variable_reg_map[current_quaternion.opr1];
    if (opr1_reg != -1) {
        // some reg keeps opr1
        if (gpr_variable_map[opr1_reg].size() == 1) {
            // this gpr only store opr1
            if (current_active_info.opr1_active_info != NON_ACTIVE) {
                generate_store(opr1_reg, current_quaternion.opr1, target_text);
                gpr_variable_map[opr1_reg].insert(current_quaternion.opr1);
                variable_reg_map[current_quaternion.opr1] = opr1_reg;
            }

            return opr1_reg;
        }
        else {
            // try to find free gpr
            int free_gpr = search_free_register(false);
            if (free_gpr != -1) {
                move_to_gpr(free_gpr, current_quaternion.opr1, target_text);

                return free_gpr;
            }

            int unlucky_reg = search_unlucky_gpr(quaternion_idx);
            bool opr1_in_chosen_reg = variable_reg_map[current_quaternion.opr1] != unlucky_reg;

            backup_gpr_to_mem(unlucky_reg, quaternion_idx, target_text, true);

            // copy opr1 value
            if (opr1_in_chosen_reg) {
                move_to_gpr(unlucky_reg, current_quaternion.opr1, target_text);
            }

            return unlucky_reg;
        }
    }

    // find free reg
    // find free gpr
    int free_gpr = search_free_register(false);
    if (free_gpr == -1) {
        // not find free reg
        free_gpr = search_unlucky_gpr(quaternion_idx);
        backup_gpr_to_mem(free_gpr, quaternion_idx, target_text, true);
    }

    assert(symbol_table[current_quaternion.opr1].memory_offset != INVALID_MEMORY_OFFSET || symbol_table[current_quaternion.opr1].is_const);

    move_to_gpr(free_gpr, current_quaternion.opr1, target_text);

    return free_gpr;
}

int get_xmm_two_opr_with_result(int quaternion_idx, vector<string> &target_text) {
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];
    QuaternionActiveInfo &current_active_info = quaternion_active_info_table[quaternion_idx];

    int opr1_reg = variable_reg_map[current_quaternion.opr1];
    if (opr1_reg != -1) {
        // some reg keeps opr1
        if (xmm_variable_map[opr1_reg].size() == 1) {
            // this xmm only store opr1
            if (current_active_info.opr1_active_info != NON_ACTIVE) {
                generate_store(opr1_reg, current_quaternion.opr1, target_text);
                xmm_variable_map[opr1_reg].insert(current_quaternion.opr1);
                variable_reg_map[current_quaternion.opr1] = opr1_reg;
            }

            return opr1_reg;
        }
        else {
            // try to find free xmm
            int free_xmm = search_free_register(true);
            if (free_xmm != -1) {
                move_to_xmm(free_xmm, current_quaternion.opr1, target_text);

                return free_xmm;
            }

            int unlucky_reg = search_unlucky_xmm(quaternion_idx);
            bool opr1_in_chosen_reg = variable_reg_map[current_quaternion.opr1] != unlucky_reg;

            backup_xmm_to_mem(unlucky_reg, quaternion_idx, target_text, true);

            // copy opr1 value
            if (opr1_in_chosen_reg) {
                move_to_xmm(unlucky_reg, current_quaternion.opr1, target_text);
            }

            return unlucky_reg;
        }
    }

    // find free reg
    // find free xmm
    int free_xmm = search_free_register(true);
    if (free_xmm == -1) {
        // not find free reg
        free_xmm = search_unlucky_xmm(quaternion_idx);
        backup_xmm_to_mem(free_xmm, quaternion_idx, target_text, true);
    }

    assert(symbol_table[current_quaternion.opr1].memory_offset != INVALID_MEMORY_OFFSET || symbol_table[current_quaternion.opr1].is_const);

    move_to_xmm(free_xmm, current_quaternion.opr1, target_text);

    return free_xmm;
}

int alloc_one_free_gpr(int quaternion_idx, vector<string> &target_text)
{
    // find free gpr
    int allocated_gpr = search_free_register(false);
    if (allocated_gpr == -1) {
        // not find
        allocated_gpr = search_unlucky_gpr(quaternion_idx);
        backup_gpr_to_mem(allocated_gpr, 0, target_text, false);
    }

    gpr_locked[allocated_gpr] = true;

    return allocated_gpr;
}

void gpr_set_mapping(int symbol_idx, int gpr_idx)
{
    variable_reg_map[symbol_idx] = gpr_idx;
    gpr_variable_map[gpr_idx].insert(symbol_idx);
    gpr_locked[gpr_idx] = false;
}

int alloc_one_free_xmm(int quaternion_idx, vector<string> &target_text) {
    // find free xmm
    int allocated_xmm = search_free_register(true);
    if (allocated_xmm == -1) {
        // not find
        allocated_xmm = search_unlucky_xmm(quaternion_idx);
        backup_xmm_to_mem(allocated_xmm, 0, target_text, false);
    }

    xmm_locked[allocated_xmm] = true;

    return allocated_xmm;
}

void xmm_set_mapping(int symbol_idx, int xmm_idx)
{
    variable_reg_map[symbol_idx] = xmm_idx;
    xmm_variable_map[xmm_idx].insert(symbol_idx);
    xmm_locked[xmm_idx] = false;
}

void update_next_use_table(int quaternion_idx) {
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];
    QuaternionActiveInfo &current_info = quaternion_active_info_table[quaternion_idx];
    if (OP_CODE_OPR_USAGE[current_quaternion.op_code][0] == USAGE_VAR ||
        OP_CODE_OPR_USAGE[current_quaternion.op_code][0] == USAGE_ARRAY) {
        variable_next_use[current_quaternion.opr1] = current_info.opr1_active_info;
    }

    if (OP_CODE_OPR_USAGE[current_quaternion.op_code][1] == USAGE_VAR ||
        OP_CODE_OPR_USAGE[current_quaternion.op_code][1] == USAGE_ARRAY) {
        variable_next_use[current_quaternion.opr2] = current_info.opr2_active_info;
    }

    if (OP_CODE_OPR_USAGE[current_quaternion.op_code][2] == USAGE_VAR ||
        OP_CODE_OPR_USAGE[current_quaternion.op_code][2] == USAGE_ARRAY) {
        variable_next_use[current_quaternion.result] = current_info.result_active_info;
    }
}

void release_reg(int quaternion_idx) {
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];
    QuaternionActiveInfo &current_info = quaternion_active_info_table[quaternion_idx];
    if (OP_CODE_OPR_USAGE[current_quaternion.op_code][0] == USAGE_VAR ||
        OP_CODE_OPR_USAGE[current_quaternion.op_code][0] == USAGE_ARRAY) {
        if (current_info.opr1_active_info == NON_ACTIVE) {
            // release reg
            SymbolEntry &symbol = symbol_table[current_quaternion.opr1];
            bool opr1_equal_result = (OP_CODE_OPR_USAGE[current_quaternion.op_code][2] == USAGE_VAR ||
                                      OP_CODE_OPR_USAGE[current_quaternion.op_code][2] == USAGE_ARRAY)
                                              && (current_quaternion.opr1 == current_quaternion.result);
            if (variable_reg_map[current_quaternion.opr1] != -1 && !opr1_equal_result) {
                if (!opr_is_stored_in_xmm(symbol)) {
                    gpr_variable_map[variable_reg_map[current_quaternion.opr1]].erase(current_quaternion.opr1);
                }
                else {
                    xmm_variable_map[variable_reg_map[current_quaternion.opr1]].erase(current_quaternion.opr1);
                }
                variable_reg_map[current_quaternion.opr1] = -1;
            }
        }
    }

    if (OP_CODE_OPR_USAGE[current_quaternion.op_code][1] == USAGE_VAR ||
        OP_CODE_OPR_USAGE[current_quaternion.op_code][1] == USAGE_ARRAY) {
        if (current_info.opr2_active_info == NON_ACTIVE) {
            // release reg
            SymbolEntry &symbol = symbol_table[current_quaternion.opr2];
            bool opr2_equal_result = (OP_CODE_OPR_USAGE[current_quaternion.op_code][2] == USAGE_VAR ||
                                      OP_CODE_OPR_USAGE[current_quaternion.op_code][2] == USAGE_ARRAY)
                                     && (current_quaternion.opr2 == current_quaternion.result);
            if (variable_reg_map[current_quaternion.opr2] != -1 && !opr2_equal_result) {
                if (!opr_is_stored_in_xmm(symbol)) {
                    gpr_variable_map[variable_reg_map[current_quaternion.opr2]].erase(current_quaternion.opr2);
                }
                else {
                    xmm_variable_map[variable_reg_map[current_quaternion.opr2]].erase(current_quaternion.opr2);
                }
                variable_reg_map[current_quaternion.opr2] = -1;
            }
        }
    }
}

void remove_opr1_set_result(int opr1_idx, int opr1_reg_idx, int result_idx, unordered_set<int> *reg_variable_map) {
    assert(reg_variable_map[opr1_reg_idx].size() == 1 || symbol_table[opr1_idx].is_const);
    reg_variable_map[opr1_reg_idx].clear();
    variable_reg_map[opr1_idx] = -1;
    reg_variable_map[opr1_reg_idx].insert(result_idx);
    variable_reg_map[result_idx] = opr1_reg_idx;
}

string get_symbol_store_text(int symbol_index)
{
    if (symbol_table[symbol_index].is_const) {
        switch (symbol_table[symbol_index].data_type) {
            case DT_BOOL:
                if (symbol_table[symbol_index].value.bool_value) {
                    return "$1";
                }
                else {
                    return "$0";
                }
                break;

            case DT_INT:
                return '$' + to_string(symbol_table[symbol_index].value.int_value);
                break;

            case DT_FLOAT:
                return ".RO_F_" + to_string(symbol_index) + ip_str;
                break;

            case DT_DOUBLE:
                return ".RO_D_" + to_string(symbol_index) + ip_str;
                break;

            default:
                break;
        }
    }

    if (symbol_table[symbol_index].is_array && !symbol_table[symbol_index].is_temp) {
        if (variable_reg_map[symbol_index] == -1) {
            if (symbol_table[symbol_index].function_index == -1) {
                // is global array
                return symbol_table[symbol_index].content + ip_str;
            }
            else {
                return to_string(symbol_table[symbol_index].memory_offset) + bp_str;
            }
        }
        else {
            return '%' + GPRStr[variable_reg_map[symbol_index]][3];
        }
    }

    if (symbol_table[symbol_index].function_index == -1) {
        // is global var
        return ".GL_" + to_string(symbol_index) + ip_str;
    }
    else {
        if (variable_reg_map[symbol_index] == -1) {
            // no reg stores, need load from memory
            assert(symbol_table[symbol_index].memory_offset != INVALID_MEMORY_OFFSET);
            return to_string(symbol_table[symbol_index].memory_offset) + bp_str;
        }
        else {
            // store in reg
            switch (symbol_table[symbol_index].data_type) {
                case DT_BOOL:
                    return '%' + GPRStr[variable_reg_map[symbol_index]][0];
                    break;

                case DT_INT:
                    return '%' + GPRStr[variable_reg_map[symbol_index]][2];
                    break;

                case DT_FLOAT:
                case DT_DOUBLE:
                    return '%' + XMMRegStr[variable_reg_map[symbol_index]];
                    break;

                default:
                    return "INVALID!!!";
                    break;
            }
        }
    }
}

int cmp_int_code(int quaternion_idx, vector<string>& target_text)
{
    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];

    if (symbol_table[current_quaternion.opr1].is_const) {
        assert(!symbol_table[current_quaternion.opr2].is_const);
        target_text.push_back("cmpl\t" + get_symbol_store_text(current_quaternion.opr2) +
                              ", $" + to_string(symbol_table[current_quaternion.opr1].value.int_value));
    }
    else if (symbol_table[current_quaternion.opr2].is_const) {
        assert(!symbol_table[current_quaternion.opr1].is_const);
        target_text.push_back("cmpl\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) +
                              ", " + get_symbol_store_text(current_quaternion.opr1));
    }
    else if (variable_reg_map[current_quaternion.opr1] == -1 && variable_reg_map[current_quaternion.opr2] == -1){
        // two opr is in memory
        // load opr2 from memory
        int opr2_reg = alloc_one_free_gpr(quaternion_idx, target_text);

        target_text.push_back("movl\t" + get_symbol_store_text(current_quaternion.opr2) + ", %" + GPRStr[opr2_reg][2]);
        target_text.push_back("cmpl\t%" + GPRStr[opr2_reg][2] + ", " + get_symbol_store_text(current_quaternion.opr1));

        gpr_set_mapping(current_quaternion.opr2, opr2_reg);
    }
    else {
        target_text.push_back("cmpl\t" + get_symbol_store_text(current_quaternion.opr2) + ", " + get_symbol_store_text(current_quaternion.opr1));
    }

    int result_reg = alloc_one_free_gpr(quaternion_idx, target_text);
    gpr_set_mapping(current_quaternion.result, result_reg);

    return result_reg;
}

int cmp_float_code(int quaternion_idx, vector<string>& target_text, bool& reverse)
{
    reverse = false;
    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];

    bool is_double = symbol_table[current_quaternion.opr1].data_type == DT_DOUBLE;

    string comp_str = is_double ? "ucomisd\t" : "ucomiss\t";
    string mov_str = is_double ? "movsd\t" : "movss\t";

    if (variable_reg_map[current_quaternion.opr1] != -1) {
        if (variable_reg_map[current_quaternion.opr2] != -1) {
            // two opr is in xmm
            target_text.push_back(comp_str + "%" + XMMRegStr[variable_reg_map[current_quaternion.opr2]] + ", %" + XMMRegStr[variable_reg_map[current_quaternion.opr1]]);
        }
        else {
            // opr1 in xmm, opr2 in mem
            target_text.push_back(comp_str + get_symbol_store_text(current_quaternion.opr2) + ", %" + XMMRegStr[variable_reg_map[current_quaternion.opr1]]);
        }
    }
    else {
        if (variable_reg_map[current_quaternion.opr2] != -1) {
            // opr1 in mem, opr2 in xmm
            target_text.push_back(comp_str + get_symbol_store_text(current_quaternion.opr1) + ", %" + XMMRegStr[variable_reg_map[current_quaternion.opr2]]);
            reverse = true;
        }
        else {
            // opr1 in mem, opr2 in mem
            // load opr1 from memory
            int opr1_reg = alloc_one_free_xmm(quaternion_idx, target_text);

            target_text.push_back(mov_str + get_symbol_store_text(current_quaternion.opr1) + ", %" + XMMRegStr[opr1_reg]);
            target_text.push_back(comp_str + get_symbol_store_text(current_quaternion.opr2) + ", %" + XMMRegStr[opr1_reg]);

            xmm_set_mapping(current_quaternion.opr1, opr1_reg);
        }
    }

    int result_reg = alloc_one_free_xmm(quaternion_idx, target_text);
    xmm_set_mapping(current_quaternion.result, result_reg);

    return result_reg;
}

void gen_swap_reg_text(int reg_0, int reg_1, vector<string>& target_text, bool is_xmm)
{
    if (reg_0 == reg_1) {
        return ;
    }

    if (is_xmm) {
        target_text.push_back("pxor\t%" + XMMRegStr[reg_0] + ", %" + XMMRegStr[reg_1]);
        target_text.push_back("pxor\t%" + XMMRegStr[reg_1] + ", %" + XMMRegStr[reg_0]);
        target_text.push_back("pxor\t%" + XMMRegStr[reg_0] + ", %" + XMMRegStr[reg_1]);
    }
    else {
        target_text.push_back("xchgq\t%" + GPRStr[reg_0][3] + ", %" + GPRStr[reg_1][3]);
    }
}

void lock_symbol_to_gpr(int symbol_idx, int target_reg, vector<string>& target_text, int quaternion_idx, int bit_length)
{
    string suffix = bit_length == 64 ? "q" : "l";
    int gpr_str_offset = bit_length == 64 ? 3 : 2;

    // symbol already in target reg
    if (variable_reg_map[symbol_idx] != target_reg) {
        if (gpr_variable_map[target_reg].empty()) {
            // target reg is empty
            target_text.push_back("mov" + suffix + '\t' + get_symbol_store_text(symbol_idx) + ", %" + GPRStr[target_reg][gpr_str_offset]);
            // keep mapping
            gpr_variable_map[target_reg].insert(symbol_idx);
            variable_reg_map[symbol_idx] = target_reg;
        }
        else {
            if (variable_reg_map[symbol_idx] != -1 && !gpr_locked[variable_reg_map[symbol_idx]]) {
                // symbol stores in register and this register is unlocked
                gen_swap_reg_text(variable_reg_map[symbol_idx], target_reg, target_text, false);
            }
            else {
                // try to get a free register r2
                int allocated_gpr = alloc_one_free_gpr(quaternion_idx, target_text);
                assert(gpr_variable_map[allocated_gpr].empty());
                if (allocated_gpr != target_reg) {
                    // copy target reg values to allocated_gpr
                    target_text.push_back("mov" + suffix + "\t%" + GPRStr[target_reg][gpr_str_offset] + ", %" + GPRStr[allocated_gpr][gpr_str_offset]);
                    // keep mapping
                    for (int symbol_in_target_gpr : gpr_variable_map[target_reg]) {
                        gpr_variable_map[allocated_gpr].insert(symbol_in_target_gpr);
                        variable_reg_map[symbol_in_target_gpr] = allocated_gpr;
                    }
                    gpr_variable_map[target_reg].clear();
                    gpr_locked[allocated_gpr] = false;
                }

                // copy symbol to target_reg
                target_text.push_back("mov" + suffix + "\t" + get_symbol_store_text(symbol_idx) + ", %" + GPRStr[target_reg][gpr_str_offset]);
                // keep mapping
                if (!symbol_table[symbol_idx].is_const) {
                    gpr_variable_map[target_reg].insert(symbol_idx);
                    gpr_variable_map[variable_reg_map[symbol_idx]].erase(symbol_idx);
                    variable_reg_map[symbol_idx] = target_reg;
                }
            }
        }
    }


    gpr_locked[target_reg] = true;
}

void lock_symbol_to_xmm(int symbol_idx, int target_reg, vector<string>& target_text, int quaternion_idx)
{
    string mov_op = symbol_table[symbol_idx].data_type == DT_FLOAT ? "movss\t" : "movsd\t";

    // symbol already in target reg
    if (variable_reg_map[symbol_idx] != target_reg) {
        if (xmm_variable_map[target_reg].empty()) {
            // target reg is empty
            target_text.push_back(mov_op + get_symbol_store_text(symbol_idx) + ", %" + XMMRegStr[target_reg]);
            // keep mapping
            xmm_variable_map[target_reg].insert(symbol_idx);
            variable_reg_map[symbol_idx] = target_reg;
        }
        else {
            if (variable_reg_map[symbol_idx] != -1 && !xmm_locked[variable_reg_map[symbol_idx]]) {
                // symbol stores in register and this register is unlocked
                gen_swap_reg_text(variable_reg_map[symbol_idx], target_reg, target_text, false);
            }
            else {
                // try to get a free register r2
                int allocated_xmm = alloc_one_free_xmm(quaternion_idx, target_text);
                assert(xmm_variable_map[allocated_xmm].empty());
                if (allocated_xmm != target_reg) {
                    // copy target reg values to allocated_xmm
                    target_text.push_back(mov_op + "%" + XMMRegStr[target_reg] + ", %" + XMMRegStr[allocated_xmm]);
                    // keep mapping
                    for (int symbol_in_target_xmm : xmm_variable_map[target_reg]) {
                        xmm_variable_map[allocated_xmm].insert(symbol_in_target_xmm);
                        variable_reg_map[symbol_in_target_xmm] = allocated_xmm;
                    }
                    xmm_locked[allocated_xmm] = false;
                    xmm_variable_map[target_reg].clear();
                }

                // copy symbol to target_reg
                target_text.push_back(mov_op + get_symbol_store_text(symbol_idx) + ", %" + XMMRegStr[target_reg]);
                // keep mapping
                if (!symbol_table[symbol_idx].is_const) {
                    xmm_variable_map[target_reg].insert(symbol_idx);
                    xmm_variable_map[variable_reg_map[symbol_idx]].erase(symbol_idx);
                    variable_reg_map[symbol_idx] = target_reg;
                }
            }
        }
    }


    xmm_locked[target_reg] = true;
}

void div_gpr_code(int quaternion_idx, vector<string>& target_text)
{
    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];
    QuaternionActiveInfo& active_info = quaternion_active_info_table[quaternion_idx];

    // backup DX content
    bool DX_is_locked = gpr_locked[GeneralPR::DX];
    int DX_content_symbol;
    if (DX_is_locked) {
        DX_content_symbol = *(gpr_variable_map[GeneralPR::DX].begin());
    }

    backup_gpr_to_mem(GeneralPR::DX, quaternion_idx, target_text, false);
    gpr_locked[GeneralPR::DX] = true;

    // backup opr1 to mem if it will be used later and stored in gpr
    int opr1_gpr = variable_reg_map[current_quaternion.opr1];
    if (active_info.opr1_active_info != NON_ACTIVE && opr1_gpr != -1) {
        generate_store(opr1_gpr, current_quaternion.opr1, target_text);
    }

    // move opr1 to ax
    lock_symbol_to_gpr(current_quaternion.opr1, GeneralPR::AX, target_text, quaternion_idx, 32);

    // generate div instruction
    if (symbol_table[current_quaternion.opr2].is_const) {
        // use a gpr to store imm
        int temp_gpr = alloc_one_free_gpr(quaternion_idx, target_text);
        target_text.push_back("movl\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) + ", %" + GPRStr[temp_gpr][2]);
        target_text.emplace_back("cdq");
        target_text.push_back("idivl\t%" + GPRStr[temp_gpr][2]);
        gpr_locked[temp_gpr] = false;
    }
    else {
        target_text.emplace_back("cdq");
        target_text.push_back("idivl\t" + get_symbol_store_text(current_quaternion.opr2));
    }

    if (DX_is_locked) {
        // load locked data back
        target_text.push_back("movl\t" + get_symbol_store_text(DX_content_symbol) + ", %edx");
    }
    gpr_locked[GeneralPR::DX] = DX_is_locked;
    gpr_locked[GeneralPR::AX] = false;

    // now result in ax
    assert(gpr_variable_map[GeneralPR::AX].size() == 1 ||
            (gpr_variable_map[GeneralPR::AX].empty() && symbol_table[current_quaternion.opr1].is_const));

    gpr_variable_map[GeneralPR::AX].clear();
    gpr_variable_map[GeneralPR::AX].insert(current_quaternion.result);
    variable_reg_map[current_quaternion.result] = GeneralPR::AX;
}

void xmm_algo_calc_code(int quaternion_idx, const string& instr_op, vector<string>& target_text)
{
    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];

    int opr1_reg_idx = get_xmm_two_opr_with_result(quaternion_idx, target_text);
    target_text.push_back(instr_op + get_symbol_store_text(current_quaternion.opr2) + ", %" +
                            XMMRegStr[opr1_reg_idx]);

    remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, xmm_variable_map);
}

void cast_float_double_to_bool(int quaternion_idx, const string& length_spec, vector<string>& target_text)
{
    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];
    int const_0_xmm = alloc_one_free_xmm(quaternion_idx, target_text);
    target_text.push_back("xorp" + length_spec + "\t%" + XMMRegStr[const_0_xmm] + ", %" + XMMRegStr[const_0_xmm]);

    int opr1_xmm = variable_reg_map[current_quaternion.opr1];
    if (opr1_xmm == -1) {
        opr1_xmm = alloc_one_free_xmm(quaternion_idx, target_text);
        target_text.push_back("movs" + length_spec + "\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + XMMRegStr[opr1_xmm]);

        xmm_variable_map[opr1_xmm].insert(current_quaternion.opr1);
        variable_reg_map[current_quaternion.opr1] = opr1_xmm;
    }

    target_text.push_back("ucomis" + length_spec + "\t%" + XMMRegStr[const_0_xmm] + ", %" + XMMRegStr[opr1_xmm]);

    xmm_locked[const_0_xmm] = false;

    // alloc a gpr to store ne result
    int result_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

    // alloc a gpr to store p result
    int p_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

    target_text.push_back("setne\t%" + GPRStr[result_gpr][0]);
    target_text.push_back("setp\t%" + GPRStr[p_gpr][0]);
    target_text.push_back("orb\t\t%" + GPRStr[p_gpr][0] + ", %" + GPRStr[result_gpr][0]);
    target_text.push_back("andb\t$1, %" + GPRStr[result_gpr][0]);

    gpr_set_mapping(current_quaternion.result, result_gpr);
    gpr_locked[p_gpr] = false;

    // release opr1
    if (quaternion_active_info_table[quaternion_idx].opr1_active_info == NON_ACTIVE) {
        xmm_variable_map[variable_reg_map[current_quaternion.opr1]].erase(variable_reg_map[current_quaternion.opr1]);
        variable_reg_map[current_quaternion.opr1] = -1;
    }
}

void fetch_gpr_from_array(int quaternion_idx, vector<string>& target_text, char size)
{
    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];

    string mov_op = symbol_table[current_quaternion.result].data_type == DT_BOOL ? "movb" : "movl";
    int result_gpr = alloc_one_free_gpr(quaternion_idx, target_text);
    int offset_gpr = variable_reg_map[current_quaternion.opr2];
    if (offset_gpr == -1) {
        // load from mem
        offset_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

        move_to_gpr(offset_gpr, current_quaternion.opr2, target_text);
    }

    if (symbol_table[current_quaternion.opr1].function_index == -1) {
        // global array
        // ask a gpr to store address of array
        int base_gpr = variable_reg_map[current_quaternion.opr1];
        if (base_gpr == -1) {
            base_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

            target_text.push_back("leaq\t" + symbol_table[current_quaternion.opr1].content + ip_str + ", %" + GPRStr[base_gpr][3]);
        }

        target_text.push_back("andq\t$0x0000ffff, %" + GPRStr[offset_gpr][3]);
        target_text.push_back(mov_op + "\t(%" + GPRStr[base_gpr][3] + ", %" + GPRStr[offset_gpr][3] + ", " + size + "), %" + GPRStr[result_gpr][2]);

        gpr_set_mapping(current_quaternion.opr1, base_gpr);
    }
    else {
        // local array & parameter array
        int base_gpr = variable_reg_map[current_quaternion.opr1];
        if (base_gpr == -1) {
            // local array or parameter array which address not store in mem
            target_text.push_back("andq\t$0x0000ffff, %" + GPRStr[offset_gpr][3]);
            target_text.push_back(mov_op + "\t" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +"(%rbp, %" + GPRStr[offset_gpr][3] + ", " + size + "), %" + GPRStr[result_gpr][2]);
        }
        else {
            // in register , always be a parameter array
            target_text.push_back("andq\t$0x0000ffff, %" + GPRStr[offset_gpr][3]);
            target_text.push_back(mov_op + "\t(%" + GPRStr[base_gpr][3] + ", %" + GPRStr[offset_gpr][3] + ", " + size + "), %" + GPRStr[result_gpr][2]);
        }
    }

    gpr_set_mapping(current_quaternion.opr2, offset_gpr);
    gpr_set_mapping(current_quaternion.result, result_gpr);
}

void fetch_xmm_from_array(int quaternion_idx, vector<string>& target_text, char size)
{
    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];

    string mov_op = symbol_table[current_quaternion.result].data_type == DT_DOUBLE ? "movsd" : "movss";
    int result_xmm = alloc_one_free_xmm(quaternion_idx, target_text);
    int offset_gpr = variable_reg_map[current_quaternion.opr2];
    if (offset_gpr == -1) {
        // load from mem
        offset_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

        move_to_gpr(offset_gpr, current_quaternion.opr2, target_text);
    }

    if (symbol_table[current_quaternion.opr1].function_index == -1) {
        // global array
        // ask a gpr to store address of array
        int base_gpr = variable_reg_map[current_quaternion.opr1];

        if (base_gpr == -1) {
            base_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

            target_text.push_back("leaq\t" + symbol_table[current_quaternion.opr1].content + ip_str + ", %" + GPRStr[base_gpr][3]);

        }

        target_text.push_back("andq\t$0x0000ffff, %" + GPRStr[offset_gpr][3]);
        target_text.push_back(mov_op + "\t(%" + GPRStr[base_gpr][3] + ", %" + GPRStr[offset_gpr][3] + ", " + size + "), %" + GPRStr[result_xmm][2]);

        gpr_set_mapping(current_quaternion.opr1, base_gpr);
    }
    else {
        // local array & parameter array
        int base_gpr = variable_reg_map[current_quaternion.opr1];
        if (base_gpr == -1) {
            // local array or parameter array which address not store in mem
            target_text.push_back("andq\t$0x0000ffff, %" + GPRStr[offset_gpr][3]);
            target_text.push_back(mov_op + "\t" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +"(%rbp, %" + GPRStr[offset_gpr][3] + ", " + size + "), %" + GPRStr[result_xmm][2]);

        }
        else {
            // in register , always be a parameter array
            target_text.push_back("andq\t$0x0000ffff, %" + GPRStr[offset_gpr][3]);
            target_text.push_back(mov_op + "\t(%" + GPRStr[base_gpr][3] + ", %" + GPRStr[offset_gpr][3] + ", " + size + "), %" + GPRStr[result_xmm][2]);

        }
        // local array

    }

    gpr_set_mapping(current_quaternion.opr2, offset_gpr);
    gpr_set_mapping(current_quaternion.result, result_xmm);
}

void store_gpr_to_array(int quaternion_idx, vector<string>& target_text, char size)
{
    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];

    string mov_op = symbol_table[current_quaternion.result].data_type == DT_BOOL ? "movb" : "movl";
    int opr1_gpr = variable_reg_map[current_quaternion.opr1];
    if (opr1_gpr == -1) {
        opr1_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

        move_to_gpr(opr1_gpr, current_quaternion.opr1, target_text);
    }

    int offset_gpr = variable_reg_map[current_quaternion.opr2];
    if (offset_gpr == -1) {
        // load from mem
        offset_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

        move_to_gpr(offset_gpr, current_quaternion.opr2, target_text);
    }

    if (symbol_table[current_quaternion.opr1].function_index == -1) {
        // global array
        // ask a gpr to store address of array
        int base_gpr = variable_reg_map[current_quaternion.opr1];
        if (base_gpr == -1) {
            base_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

            target_text.push_back("leaq\t" + symbol_table[current_quaternion.opr1].content + ip_str + ", %" + GPRStr[base_gpr][3]);
        }

        target_text.push_back("andq\t$0x0000ffff, %" + GPRStr[offset_gpr][3]);
        target_text.push_back(mov_op + "\t %" + GPRStr[opr1_gpr][2] + ", (%" + GPRStr[base_gpr][3] + ", %" + GPRStr[offset_gpr][3] + ", " + size + ")");

        gpr_set_mapping(current_quaternion.opr1, base_gpr);
    }
    else {
        // local array
        target_text.push_back("andq\t$0x0000ffff, %" + GPRStr[offset_gpr][3]);
        target_text.push_back(mov_op + "\t %" + GPRStr[opr1_gpr][2] + ", " + to_string(symbol_table[current_quaternion.result].memory_offset) + "(%rbp, %" + GPRStr[offset_gpr][3] + ", " + size + ")");
    }

    gpr_set_mapping(current_quaternion.opr2, offset_gpr);
    gpr_set_mapping(current_quaternion.opr1, opr1_gpr);
}

void store_xmm_to_array(int quaternion_idx, vector<string>& target_text, char size)
{
    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];

    string mov_op = symbol_table[current_quaternion.result].data_type == DT_DOUBLE ? "movsd" : "movss";
    int result_gpr = alloc_one_free_gpr(quaternion_idx, target_text);
    int offset_gpr = variable_reg_map[current_quaternion.opr2];
    if (offset_gpr == -1) {
        // load from mem
        offset_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

        move_to_gpr(offset_gpr, current_quaternion.opr2, target_text);
    }

    if (symbol_table[current_quaternion.opr1].function_index == -1) {
        // global array
        // ask a gpr to store address of array
        int base_gpr = variable_reg_map[current_quaternion.opr1];
        if (base_gpr == -1) {
            base_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

            target_text.push_back("leaq\t" + symbol_table[current_quaternion.opr1].content + ip_str + ", %" + GPRStr[base_gpr][3]);
        }

        target_text.push_back("andq\t$0x0000ffff, %" + GPRStr[offset_gpr][3]);
        target_text.push_back(mov_op + "\t %" + GPRStr[result_gpr][2] + ", (%" + GPRStr[base_gpr][3] + ", %" + GPRStr[offset_gpr][3] + ", " + size + ")");

        gpr_set_mapping(current_quaternion.opr1, base_gpr);
    }
    else {
        // local array
        target_text.push_back("andq\t$0x0000ffff, %" + GPRStr[offset_gpr][3]);
        target_text.push_back(mov_op + "\t %" + GPRStr[result_gpr][2] + ", " + to_string(symbol_table[current_quaternion.result].memory_offset) +"(%rbp, %" + GPRStr[offset_gpr][3] + ", " + size + ")");
    }

    gpr_set_mapping(current_quaternion.opr2, offset_gpr);
    gpr_set_mapping(current_quaternion.result, result_gpr);
}

void generate_quaternion_text(int quaternion_idx, vector<string> &target_text, BaseBlock& base_block) {
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];
    update_next_use_table(quaternion_idx);

    if (OP_CODE_OPR_USAGE[current_quaternion.op_code][2] == USAGE_VAR) {
        last_calc_symbol = current_quaternion.result;
    }

    switch (current_quaternion.op_code) {
        case OP_ASSIGNMENT: {
            if (symbol_table[current_quaternion.opr1].is_const) {
                // load imm
                switch (symbol_table[current_quaternion.result].data_type) {
                    case DT_BOOL: {
                        int alloc_reg_index = alloc_one_free_gpr(quaternion_idx, target_text);
                        if (!symbol_table[current_quaternion.opr1].value.bool_value) {
                            target_text.push_back("xorb\t%" + GPRStr[alloc_reg_index][0] + ", %" +
                                                  GPRStr[alloc_reg_index][0]); // xor to optimize
                        } else {
                            target_text.push_back("movb\t$1, %" + GPRStr[alloc_reg_index][0]);
                        }

                        gpr_set_mapping(current_quaternion.result, alloc_reg_index);
                        break;
                    }

                    case DT_INT: {
                        int alloc_reg_index = alloc_one_free_gpr(quaternion_idx, target_text);

                        if (symbol_table[current_quaternion.opr1].value.int_value == 0) {
                            target_text.push_back("xorl\t%" + GPRStr[alloc_reg_index][2] + ", %" +
                                                  GPRStr[alloc_reg_index][2]); // xor to optimize
                        } else {
                            target_text.push_back(
                                    "movl\t$" + to_string(symbol_table[current_quaternion.opr1].value.int_value) +
                                    ", %" + GPRStr[alloc_reg_index][2]);
                        }

                        gpr_set_mapping(current_quaternion.result, alloc_reg_index);
                        break;
                    }

                    case DT_FLOAT: {
                        int alloc_reg_index = alloc_one_free_xmm(quaternion_idx, target_text);

                        if (symbol_table[current_quaternion.opr1].value.float_value == 0.0f) {
                            target_text.push_back("xorps\t%" + XMMRegStr[alloc_reg_index] + ", %" +
                                                  XMMRegStr[alloc_reg_index]); // xor to optimize
                        } else {
                            target_text.push_back(
                                    "movss\t.RO_F_" + to_string(current_quaternion.opr1) + ip_str + ", %" + XMMRegStr[alloc_reg_index]);

                        }

                        xmm_set_mapping(current_quaternion.result, alloc_reg_index);

                        break;
                    }

                    case DT_DOUBLE: {
                        int alloc_reg_index = alloc_one_free_xmm(quaternion_idx, target_text);
                        if (symbol_table[current_quaternion.opr1].value.double_value == 0.0) {
                            target_text.push_back("xorpd\t%" + XMMRegStr[alloc_reg_index] + ", %" +
                                                  XMMRegStr[alloc_reg_index]); // xor to optimize
                        } else {
                            target_text.push_back("movsd\t.RO_D_" + to_string(current_quaternion.opr1) + ip_str + ", %" + XMMRegStr[alloc_reg_index]);

                        }

                        xmm_set_mapping(current_quaternion.result, alloc_reg_index);

                        break;
                    }

                    default:
                        break;
                }
            }
            else {
                // assign from a variable
                if (variable_reg_map[current_quaternion.opr1] == -1) {
                    // opr1 in memory
                    switch (symbol_table[current_quaternion.result].data_type) {
                        case DT_BOOL: {
                            int alloc_reg_index = alloc_one_free_gpr(quaternion_idx, target_text);

                            target_text.push_back(
                                    "movb\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" +
                                    GPRStr[alloc_reg_index][0]);

                            gpr_set_mapping(current_quaternion.result, alloc_reg_index);

                            break;
                        }

                        case DT_INT: {
                            int alloc_reg_index = alloc_one_free_gpr(quaternion_idx, target_text);

                            target_text.push_back(
                                    "movl\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" +
                                    GPRStr[alloc_reg_index][2]);

                            gpr_set_mapping(current_quaternion.result, alloc_reg_index);

                            break;
                        }

                        case DT_FLOAT: {
                            int alloc_reg_index = alloc_one_free_xmm(quaternion_idx, target_text);

                            target_text.push_back(
                                    "movss\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" +
                                    XMMRegStr[alloc_reg_index]);

                            xmm_set_mapping(current_quaternion.result, alloc_reg_index);
                            break;
                        }

                        case DT_DOUBLE: {
                            int alloc_reg_index = alloc_one_free_xmm(quaternion_idx, target_text);

                            target_text.push_back(
                                    "movsd\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" +
                                    XMMRegStr[alloc_reg_index]);

                            xmm_set_mapping(current_quaternion.result, alloc_reg_index);

                            break;
                        }

                        default:
                            break;
                    }
                }
                else {
                    variable_reg_map[current_quaternion.result] = variable_reg_map[current_quaternion.opr1];
                    if (opr_is_stored_in_xmm(symbol_table[current_quaternion.result])) {
                        xmm_variable_map[variable_reg_map[current_quaternion.opr1]].insert(current_quaternion.result);
                    } else {
                        gpr_variable_map[variable_reg_map[current_quaternion.opr1]].insert(current_quaternion.result);
                    }
                }
            }
            break;
        }

        case OP_ADD_INT: {
//            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "addl\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }
            else {
                // opr2 in memory or reg
                target_text.push_back(
                        "addl\t" + get_symbol_store_text(current_quaternion.opr2) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, gpr_variable_map);
            break;
        }

        case OP_ADD_FLOAT: {
            xmm_algo_calc_code(quaternion_idx, "addss\t", target_text);
            break;
        }

        case OP_ADD_DOUBLE: {
            xmm_algo_calc_code(quaternion_idx, "addsd\t", target_text);
            break;
        }

        case OP_MINUS_INT: {
//            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "subl\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }
            else {
                // opr2 in memory or reg
                target_text.push_back(
                        "subl\t" + get_symbol_store_text(current_quaternion.opr2) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, gpr_variable_map);
            break;
        }

        case OP_MINUS_FLOAT: {
            xmm_algo_calc_code(quaternion_idx, "subss\t", target_text);
            break;
        }

        case OP_MINUS_DOUBLE: {
            xmm_algo_calc_code(quaternion_idx, "subsd\t", target_text);
            break;
        }

        case OP_MULTI_INT: {
//            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "imul\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }
            else {
                // opr2 in memory or reg
                target_text.push_back(
                        "imul\t" + get_symbol_store_text(current_quaternion.opr2) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, gpr_variable_map);
            break;
        }

        case OP_MULTI_FLOAT: {
            xmm_algo_calc_code(quaternion_idx, "mulss\t", target_text);
            break;
        }

        case OP_MULTI_DOUBLE: {
            xmm_algo_calc_code(quaternion_idx, "mulsd\t", target_text);
            break;
        }

        case OP_DIV_INT: {
            div_gpr_code(quaternion_idx, target_text);
            break;
        }

        case OP_DIV_FLOAT:
        {
            xmm_algo_calc_code(quaternion_idx, "divss\t", target_text);
            break;
        }

        case OP_DIV_DOUBLE:
        {
            xmm_algo_calc_code(quaternion_idx, "divsd\t", target_text);
            break;
        }

        case OP_COMMA:
        case OP_LOGIC_OR:
        case OP_LOGIC_AND:
        case OP_LOGIC_NOT:
        case OP_JEQ:
        {
            break;
        }

        case OP_JNZ: {
            int prev_set_instr_like_point = target_text.size() - 1;
            // get first \t
            int c = 3;
            for (; c < target_text.back().length(); ++c) {
                if (target_text.back()[c] == '\t') {
                    break;
                }
            }
            string prev_set_instr_op = target_text.back().substr(3, c - 3);

            // save out set variables
            for (int out_sym : base_block.out_set) {
                if (variable_reg_map[out_sym] != -1) {
                    // stores in reg, need write back
                    generate_store(variable_reg_map[out_sym], out_sym, target_text);
                }
            }

            // optimize
            // quaternion_active_info_table[quaternion_idx - 1].result_active_info == quaternion_idx because current ir (jnz) is the exit
            // of base block, the opr1 of jnz ir will not be referenced by later ir, because there will be no ir follow jnz in the same base block

            if (quaternion_idx > 0 && current_quaternion.opr1 == optimized_sequence[quaternion_idx - 1].result &&
                quaternion_active_info_table[quaternion_idx - 1].result_active_info == quaternion_idx &&
                optimized_sequence[quaternion_idx - 1].op_code >= OP_EQUAL_INT && optimized_sequence[quaternion_idx - 1].op_code <= OP_NOT_EQUAL_DOUBLE) {
                assert(target_text[prev_set_instr_like_point].substr(0, 3) == "set");

                target_text[prev_set_instr_like_point] = "";

                // optimize jz
                if (current_quaternion.result == 2 && quaternion_idx + 1 < optimized_sequence.size() &&
                    optimized_sequence[quaternion_idx + 1].op_code == OP_JMP) {
                    current_quaternion.result = optimized_sequence[quaternion_idx + 1].result + 1;
                    optimized_sequence[quaternion_idx + 1] = {OP_NOP, -1, -1, -1};

                    string jump_dest = to_string(current_quaternion.result + quaternion_idx);

                    // generate neg instr via condition_negative_map
                    // use prev set instr to decide
                    target_text.push_back("j" + condition_negative_map.find(prev_set_instr_op)->second + "\t\t.L" + jump_dest);
                }
                else {
                    // generate normal instr
                    string jump_dest = to_string(current_quaternion.result + quaternion_idx);

                    // use prev set instr to decide
                    target_text.push_back("j" + prev_set_instr_op + "\t\t.L" + jump_dest);
                }
            }
            else {
                if (last_calc_symbol != current_quaternion.opr1) {
                    string test_str;
                    switch (symbol_table[current_quaternion.opr1].data_type) {
                        case DT_BOOL:
                            test_str = "testb\t$(-1), ";
                            target_text.push_back(test_str + get_symbol_store_text(current_quaternion.opr1));
                            break;

                        case DT_INT:
                            test_str = "testl\t$(-1), ";
                            target_text.push_back(test_str + get_symbol_store_text(current_quaternion.opr1));
                            break;

                        case DT_FLOAT:
                        {
                            int zero_xmm = alloc_one_free_xmm(quaternion_idx, target_text);
                            target_text.push_back("xorps\t%" + XMMRegStr[zero_xmm] + ", %" + XMMRegStr[zero_xmm]);
                            target_text.push_back("ucomiss\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + XMMRegStr[zero_xmm]);

                            xmm_locked[zero_xmm] = false;
                            break;
                        }

                        case DT_DOUBLE:
                        {
                            int zero_xmm = alloc_one_free_xmm(quaternion_idx, target_text);
                            target_text.push_back("xorps\t%" + XMMRegStr[zero_xmm] + ", %" + XMMRegStr[zero_xmm]);
                            target_text.push_back("ucomisd\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + XMMRegStr[zero_xmm]);

                            xmm_locked[zero_xmm] = false;
                            break;
                        }

                        default:
                            break;
                    }
                    last_calc_symbol = current_quaternion.opr1;
                }

                // optimize jz
                if (current_quaternion.result == 2 && quaternion_idx + 1 < optimized_sequence.size() &&
                    optimized_sequence[quaternion_idx + 1].op_code == OP_JMP) {
                    current_quaternion.result = optimized_sequence[quaternion_idx + 1].result + 1;
                    optimized_sequence[quaternion_idx + 1] = {OP_NOP, -1, -1, -1};
                    string jump_dest = to_string(current_quaternion.result + quaternion_idx);

                    // generate jz instr
                    target_text.push_back("jz\t\t.L" + jump_dest);
                }
                else {
                    // generate jnz instr
                    string jump_dest = to_string(current_quaternion.result + quaternion_idx);

                    target_text.push_back("jnz\t\t.L" + jump_dest);
                }
            }

            break;
        }

        case OP_JMP: {
            // save out set variables
            for (int out_sym : base_block.out_set) {
                if (variable_reg_map[out_sym] != -1) {
                    // stores in reg, need write back
                    generate_store(variable_reg_map[out_sym], out_sym, target_text);
                }
            }

            target_text.push_back("jmp\t\t.L" + to_string(current_quaternion.result + quaternion_idx));
            break;
        }

        case OP_LI_BOOL:
        case OP_LI_INT:
        case OP_LI_FLOAT:
        case OP_LI_DOUBLE:
            break;

        case OP_FETCH_BOOL:
        {
            fetch_gpr_from_array(quaternion_idx, target_text, '1');
            break;
        }

        case OP_FETCH_INT:
        {
            fetch_gpr_from_array(quaternion_idx, target_text, '4');
            break;
        }

        case OP_FETCH_FLOAT:
        {
            fetch_xmm_from_array(quaternion_idx, target_text, '4');
            break;
        }

        case OP_FETCH_DOUBLE:
        {
            fetch_xmm_from_array(quaternion_idx, target_text, '8');
            break;
        }

        case OP_EQUAL_INT:
        {
            int result_reg = cmp_int_code(quaternion_idx, target_text);

            target_text.push_back("sete\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_EQUAL_FLOAT:
        case OP_EQUAL_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, reverse);

            target_text.push_back("sete\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_GREATER_INT:
        {
            int result_reg = cmp_int_code(quaternion_idx, target_text);

            target_text.push_back("setg\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_GREATER_FLOAT:
        case OP_GREATER_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, reverse);

            if (!reverse) {
                target_text.push_back("seta\t%" + GPRStr[result_reg][0]);
            }
            else {
                target_text.push_back("setbe\t%" + GPRStr[result_reg][0]);
            }

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_GREATER_EQUAL_INT:{
            int result_reg = cmp_int_code(quaternion_idx, target_text);

            target_text.push_back("setge\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_GREATER_EQUAL_FLOAT:
        case OP_GREATER_EQUAL_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, reverse);

            if (!reverse) {
                target_text.push_back("setae\t%" + GPRStr[result_reg][0]);
            }
            else {
                target_text.push_back("setb\t%" + GPRStr[result_reg][0]);
            }

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_LESS_INT: {
            int result_reg = cmp_int_code(quaternion_idx, target_text);

            target_text.push_back("setl\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_LESS_FLOAT:
        case OP_LESS_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, reverse);

            if (!reverse) {
                target_text.push_back("setb\t%" + GPRStr[result_reg][0]);
            }
            else {
                target_text.push_back("setae\t%" + GPRStr[result_reg][0]);
            }

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_LESS_EQUAL_INT: {
            int result_reg = cmp_int_code(quaternion_idx, target_text);

            target_text.push_back("setle\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_LESS_EQUAL_FLOAT:
        case OP_LESS_EQUAL_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, reverse);

            if (!reverse) {
                target_text.push_back("setbe\t%" + GPRStr[result_reg][0]);
            }
            else {
                target_text.push_back("seta\t%" + GPRStr[result_reg][0]);
            }

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_NOT_EQUAL_INT:
        {
            int result_reg = cmp_int_code(quaternion_idx, target_text);

            target_text.push_back("setne\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_NOT_EQUAL_FLOAT:
        case OP_NOT_EQUAL_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, reverse);

            target_text.push_back("setne\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_NEG:
        {
            if (opr_is_stored_in_xmm(symbol_table[current_quaternion.opr1])) {
                // alloc a gpr to transfer
                int trans_gpr = alloc_one_free_gpr(quaternion_idx, target_text);

                if (variable_reg_map[current_quaternion.opr1] != -1) {
                    // backup xmm
                    backup_xmm_to_mem(variable_reg_map[current_quaternion.opr1], quaternion_idx, target_text, true);
                }

                target_text.push_back("movd\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + GPRStr[trans_gpr][2]);
                target_text.push_back("xorl\t$0x80000000, %" + GPRStr[trans_gpr][2]);
                target_text.push_back("movd\t%" + GPRStr[trans_gpr][2] + ", " + get_symbol_store_text(current_quaternion.opr1));

                gpr_locked[trans_gpr] = false;

                if (variable_reg_map[current_quaternion.opr1] != -1) {
                    remove_opr1_set_result(current_quaternion.opr1, variable_reg_map[current_quaternion.opr1], current_quaternion.result, xmm_variable_map);
                }
            }
            else {
                // stores in gpr
                int opr1_reg = get_gpr_two_opr_with_result(quaternion_idx, target_text);

                target_text.push_back("neg\t\t%" + GPRStr[opr1_reg][2]);
            }

            // don't need release, return directly
            return;
            break;
        }

        case OP_PAR: {
            if (opr_is_stored_in_xmm(symbol_table[current_quaternion.opr1])) {
                if (par_usable_xmm_index >= X64XMMCNT) { // don't use all regs for passing arguments! ax not used to pass args
                    // xmm is not enough, stores in memory
                    if (symbol_table[current_quaternion.opr1].is_const) {
                        if (symbol_table[current_quaternion.opr1].data_type == DT_FLOAT) {
                            target_text.push_back("push\t$" + to_string(symbol_table[current_quaternion.opr1].value.int_value));
                        }
                        else {
                            target_text.push_back("push\t$" + to_string(*((long long*)(&symbol_table[current_quaternion.opr1].value))));
                        }
                    }
                    else {
                        target_text.push_back("pushq\t$" + get_symbol_store_text(current_quaternion.opr1));
                    }
                }
                else {
                    // use xmm[par_usable_xmm_index] to pass argument
                    lock_symbol_to_xmm(current_quaternion.opr1, par_usable_xmm_index, target_text, quaternion_idx);

                    ++par_usable_xmm_index;
                }
            }
            else {
                if (par_usable_gpr_index >= X64GPRCNT) {
                    // gpr is not enough, stores in memory
                    if (symbol_table[current_quaternion.opr1].is_array && !symbol_table[current_quaternion.opr1].is_temp) {
                        int address_reg = alloc_one_free_gpr(quaternion_idx, target_text);


                        target_text.push_back("leaq\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + GPRStr[address_reg][3]);
                        target_text.push_back("push\t%" + GPRStr[address_reg][3]);

                        gpr_locked[address_reg] = false;
                    }
                    else {
                        if (symbol_table[current_quaternion.opr1].is_const) {
                            if (symbol_table[current_quaternion.opr1].data_type == DT_BOOL) {
                                if (symbol_table[current_quaternion.opr1].value.bool_value) {
                                    target_text.emplace_back("push\t$1");
                                } else {
                                    target_text.emplace_back("push\t$0");
                                }
                            }
                            else {
                                target_text.push_back("push\t$" + to_string(symbol_table[current_quaternion.opr1].value.int_value));
                            }
                        }
                        else {
                            if (variable_reg_map[current_quaternion.opr1] != -1) {
                                // store in reg
                                target_text.push_back("push\t%" + GPRStr[variable_reg_map[current_quaternion.opr1]][2]);
                            }
                            else {
                                target_text.push_back("pushq\t$" + get_symbol_store_text(current_quaternion.opr1));
                            }
                        }
                    }
                }
                else {
                    if (symbol_table[current_quaternion.opr1].is_array && !symbol_table[current_quaternion.opr1].is_temp) {
                        int address_reg = alloc_one_free_gpr(quaternion_idx, target_text);

                        target_text.push_back("leaq\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + GPRStr[address_reg][3]);
                        gpr_set_mapping(current_quaternion.opr1, address_reg);

                        lock_symbol_to_gpr(current_quaternion.opr1, par_usable_gpr_index, target_text, quaternion_idx, 64);
                        ++par_usable_gpr_index;
                    }
                    else {
                        // use gpr[par_usable_gpr_index] to pass parameter
                        lock_symbol_to_gpr(current_quaternion.opr1, par_usable_gpr_index, target_text, quaternion_idx, 32);

                        ++par_usable_gpr_index;
                    }
                }
            }
            break;
        }

        case OP_CALL: {
            // save out set variables
            for (int out_sym : base_block.out_set) {
                if (variable_reg_map[out_sym] != -1) {
                    // stores in reg, need write back
                    generate_store(variable_reg_map[out_sym], out_sym, target_text);
                }
            }

            target_text.push_back("call\t" + function_name_prefix + Function::function_table[current_quaternion.opr1].name);
            last_call_return_symbol = current_quaternion.result;

            // unlock all registers
            memset(gpr_locked, 0, X64GPRCNT * sizeof(bool));
            memset(xmm_locked, 0,  X64XMMCNT* sizeof(bool));
            par_usable_gpr_index = 1;
            par_usable_xmm_index = 1;

            // pop all parameter
            int target_arch_addr_length = 8;
            int gpr_para_cnt = 0;
            int xmm_para_cnt = 0;
            for (int para_idx : Function::function_table[current_quaternion.opr1].parameter_index) {
                if (opr_is_stored_in_xmm(symbol_table[para_idx])) {
                    ++xmm_para_cnt;
                }
                else {
                    ++gpr_para_cnt;
                }
            }
            int para_size = target_arch_addr_length * (max(0, xmm_para_cnt - X64XMMCNT + 1) + max(0, gpr_para_cnt - X64GPRCNT + 1));


            if (para_size > 0) {
                target_text.push_back("addq\t$" + to_string(para_size) + ", %rsp");
            }

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
            break;

        // r/m8 to r32
        case OP_BOOL_TO_INT:
        {
//            int result_reg = get_gpr_two_opr_with_result(quaternion_idx, target_text);
            int result_reg = alloc_one_free_gpr(quaternion_idx, target_text);
            target_text.push_back("movzbl\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + GPRStr[result_reg][2]);

            gpr_set_mapping(current_quaternion.result, result_reg);

            release_reg(quaternion_idx);
            break;
        }

        // r/m8 to xmm
        case OP_BOOL_TO_FLOAT:
        {
            int temp_gpr = alloc_one_free_gpr(quaternion_idx, target_text);
            target_text.push_back("movzbl\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + GPRStr[temp_gpr][2]);

            int result_xmm = alloc_one_free_xmm(quaternion_idx, target_text);
            target_text.push_back("cvtsi2ss\t%" + GPRStr[temp_gpr][2] + ", %" + XMMRegStr[result_xmm]);

            xmm_set_mapping(current_quaternion.result, result_xmm);

            gpr_locked[temp_gpr] = false;
            // release opr1
            if (quaternion_active_info_table[quaternion_idx].opr1_active_info == NON_ACTIVE) {
                gpr_variable_map[variable_reg_map[current_quaternion.opr1]].erase(variable_reg_map[current_quaternion.opr1]);
                variable_reg_map[current_quaternion.opr1] = -1;
            }
            break;
        }

            // r/m8 to xmm
        case OP_BOOL_TO_DOUBLE:
        {
            int temp_gpr = alloc_one_free_gpr(quaternion_idx, target_text);
            target_text.push_back("movzbl\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + GPRStr[temp_gpr][2]);

            int result_xmm = alloc_one_free_xmm(quaternion_idx, target_text);
            target_text.push_back("cvtsi2sd\t%" + GPRStr[temp_gpr][2] + ", %" + XMMRegStr[result_xmm]);

            xmm_set_mapping(current_quaternion.result, result_xmm);
            gpr_locked[temp_gpr] = false;
            // release opr1
            if (quaternion_active_info_table[quaternion_idx].opr1_active_info == NON_ACTIVE) {
                gpr_variable_map[variable_reg_map[current_quaternion.opr1]].erase(variable_reg_map[current_quaternion.opr1]);
                variable_reg_map[current_quaternion.opr1] = -1;
            }
            break;
        }

        // r32 to r8, m32 to m8
        case OP_INT_TO_BOOL:
        {
            if (variable_reg_map[current_quaternion.opr1] == -1) {
                // opr1 in mem
                symbol_table[current_quaternion.result].memory_offset = symbol_table[current_quaternion.opr1].memory_offset;
            }
            else {
                // opr1 in gpr
                backup_gpr_to_mem(variable_reg_map[current_quaternion.opr1], quaternion_idx, target_text, true);

                remove_opr1_set_result(current_quaternion.opr1, variable_reg_map[current_quaternion.opr1], current_quaternion.result, gpr_variable_map);
            }
            break;
        }

        // r/m32 to xmm
        case OP_INT_TO_FLOAT:
        {
            int result_xmm = alloc_one_free_xmm(quaternion_idx, target_text);
            target_text.push_back("cvtsi2ss\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + XMMRegStr[result_xmm]);

            xmm_set_mapping(current_quaternion.result, result_xmm);
            // release opr1
            if (quaternion_active_info_table[quaternion_idx].opr1_active_info == NON_ACTIVE) {
                gpr_variable_map[variable_reg_map[current_quaternion.opr1]].erase(variable_reg_map[current_quaternion.opr1]);
                variable_reg_map[current_quaternion.opr1] = -1;
            }
            break;
        }

        // r/m32 to xmm
        case OP_INT_TO_DOUBLE:
        {
            int result_xmm = alloc_one_free_xmm(quaternion_idx, target_text);
            target_text.push_back("cvtsi2sd\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + XMMRegStr[result_xmm]);

            xmm_set_mapping(current_quaternion.result, result_xmm);

            // release opr1
            if (quaternion_active_info_table[quaternion_idx].opr1_active_info == NON_ACTIVE) {
                gpr_variable_map[variable_reg_map[current_quaternion.opr1]].erase(variable_reg_map[current_quaternion.opr1]);
                variable_reg_map[current_quaternion.opr1] = -1;
            }
            break;
        }

        // xmm/m32 to r8
        case OP_FLOAT_TO_BOOL:
        {
            cast_float_double_to_bool(quaternion_idx, "s", target_text);
            break;
        }

        // xmm/m32 to r32
        case OP_FLOAT_TO_INT:
        {
            int result_gpr = alloc_one_free_gpr(quaternion_idx, target_text);
            target_text.push_back("cvttss2si\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + GPRStr[result_gpr][2]);

            gpr_set_mapping(current_quaternion.result, result_gpr);
            break;
        }

        case OP_FLOAT_TO_DOUBLE:
        {
            int opr1_xmm = get_xmm_two_opr_with_result(quaternion_idx, target_text);

            target_text.push_back("cvtss2sd\t%" + XMMRegStr[opr1_xmm] + ", %" + XMMRegStr[opr1_xmm]);

            remove_opr1_set_result(current_quaternion.opr1, opr1_xmm, current_quaternion.result, xmm_variable_map);
            break;
        }

        // xmm/m64 to r8
        case OP_DOUBLE_TO_BOOL:
        {
            cast_float_double_to_bool(quaternion_idx, "d", target_text);
            break;
        }

        // xmm/m64 to r32
        case OP_DOUBLE_TO_INT:
        {
            int result_gpr = alloc_one_free_gpr(quaternion_idx, target_text);
            target_text.push_back("cvttsd2si\t" + get_symbol_store_text(current_quaternion.opr1) + ", %" + GPRStr[result_gpr][2]);

            gpr_set_mapping(current_quaternion.result, result_gpr);

            break;
        }

        case OP_DOUBLE_TO_FLOAT:
        {
            int opr1_xmm = get_xmm_two_opr_with_result(quaternion_idx, target_text);

            target_text.push_back("cvtsd2ss\t%" + XMMRegStr[opr1_xmm] + ", %" + XMMRegStr[opr1_xmm]);

            remove_opr1_set_result(current_quaternion.opr1, opr1_xmm, current_quaternion.result, xmm_variable_map);
            break;
        }

        case OP_ARRAY_STORE:
        {
            switch (symbol_table[current_quaternion.result].data_type) {
                case DT_BOOL:
                {
                    store_gpr_to_array(quaternion_idx, target_text, '1');
                    break;
                }

                case DT_INT:
                {
                    store_gpr_to_array(quaternion_idx, target_text, '4');
                    break;
                }

                case DT_FLOAT:
                {
                    store_xmm_to_array(quaternion_idx, target_text, '4');
                    break;
                }

                case DT_DOUBLE:
                {
                    store_xmm_to_array(quaternion_idx, target_text, '8');
                    break;
                }

                default:
                    break;
            }
            break;
        }

        case OP_RETURN: {
            // store out active variables (it should be global vars)
            for (int symbol_idx : base_block.out_set) {
                if (variable_reg_map[symbol_idx] != -1) {
                    generate_store(variable_reg_map[symbol_idx], symbol_idx, target_text);
                }
            }

            if (current_quaternion.opr1 != -1) {
                // will return a value
                if (symbol_table[current_quaternion.opr1].is_const) {
                    // return a const
                    switch (symbol_table[current_quaternion.opr1].data_type) {
                        case DT_BOOL:
                            if (symbol_table[current_quaternion.opr1].value.bool_value) {
                                target_text.emplace_back("movb\t$1, %al");
                            }
                            else {
                                target_text.emplace_back("movb\t$0, %al");
                            }
                            break;

                        case DT_INT:
                            // use eax to pass return value
                            target_text.push_back("movl\t$" + to_string(symbol_table[current_quaternion.opr1].value.int_value) + ", %eax");
                            break;

                        case DT_FLOAT:
                            // use xmm0 to pass return value
                            target_text.push_back("movss\t.RO_F_" + to_string(current_quaternion.opr1) + ip_str + ", %xmm0");
                            break;

                        case DT_DOUBLE:
                            // use xmm0 to pass return value
                            target_text.push_back("movsd\t.RO_D_" + to_string(current_quaternion.opr1) + ip_str + ", %xmm0");
                            break;

                        default:
                            break;
                    }
                }
                else if (variable_reg_map[current_quaternion.opr1] != 0) {
                    switch (symbol_table[current_quaternion.opr1].data_type) {
                        case DT_BOOL:
                            // use al to pass return value
                            target_text.push_back("movb\t" + get_symbol_store_text(current_quaternion.opr1) + ", %al");
                            break;

                        case DT_INT:
                            // use eax to pass return value
                            target_text.push_back("movl\t" + get_symbol_store_text(current_quaternion.opr1) + ", %eax");
                            break;

                        case DT_FLOAT:
                            // use xmm0 to pass return value
                            target_text.push_back("movss\t" + get_symbol_store_text(current_quaternion.opr1) + ", %xmm0");
                            break;

                        case DT_DOUBLE:
                            // use xmm0 to pass return value
                            target_text.push_back("movsd\t" + get_symbol_store_text(current_quaternion.opr1) + ", %xmm0");
                            break;

                        default:
                            break;
                    }
                }
            }
            function_leave_correction_points.push_back(target_text.size());
            leave_correction_function_idx.push_back(current_function);
            target_text.emplace_back("leave");
            target_text.emplace_back("ret");
            break;
        }

        case OP_NOP:
        case OP_INVALID:
            break;
    }

    release_reg(quaternion_idx);
}

void generate_target_text_asm(BaseBlock &base_block, vector<string>& target_text) {
    variable_reg_map = new int[symbol_table.size()];
    memset(variable_reg_map, -1, symbol_table.size() * sizeof(int));

    variable_next_use = new int[symbol_table.size()];
    memset(variable_next_use, -1, symbol_table.size() * sizeof(int));

    for (auto & i : gpr_variable_map) {
        i.clear();
    }

    for (auto & i : xmm_variable_map) {
        i.clear();
    }

    generate_active_info_table(base_block);

    gen_entry_code(base_block, target_text);

    for (int i = base_block.start_index; i <= base_block.end_index; ++i) {
        generate_quaternion_text(i, target_text, base_block);
    }

    // generate out store instr
    // if last ir is call || jnz, ax value already stored.
    if (optimized_sequence[base_block.end_index].op_code != OP_CALL &&
            optimized_sequence[base_block.end_index].op_code != OP_JNZ &&
            optimized_sequence[base_block.end_index].op_code != OP_JMP) {
        for (int out_sym : base_block.out_set) {
            if (variable_reg_map[out_sym] != -1) {
                // stores in reg, need write back
                generate_store(variable_reg_map[out_sym], out_sym, target_text);
            }
        }
    }

    delete[] variable_next_use;
    delete[] variable_reg_map;
}

// return string contains ".data"
void generate_target_data_asm(vector<string>& target_data) {
    target_data.emplace_back(".data");

    for (int i = 0; i < symbol_table.size(); ++i) {
        SymbolEntry& symbol = symbol_table[i];

        if (symbol.is_const) {
            if (symbol.data_type == DT_FLOAT) {
                target_data.push_back(".RO_F_" + to_string(i) + ":\t.float\t" + to_string(symbol.value.float_value));
            }
            else if (symbol.data_type == DT_DOUBLE) {
                target_data.push_back(".RO_D_" + to_string(i) + ":\t.double\t" + to_string(symbol.value.double_value));
            }
        }
        else if (symbol.function_index == -1 && !symbol.is_array) {
            // global symbol
            switch (symbol.data_type) {
                case DT_BOOL:
                    target_data.push_back(".GL_" + to_string(i) + ":\t.bool\t" + (symbol.value.bool_value ? '1' : '0'));
                    break;

                case DT_INT:
                    target_data.push_back(".GL_" + to_string(i) + ":\t.int\t" + to_string(symbol.value.int_value));
                    break;

                case DT_FLOAT:
                    target_data.push_back(".GL_" + to_string(i) + ":\t.float\t" + to_string(symbol.value.float_value));
                    break;

                case DT_DOUBLE:
                    target_data.push_back(".GL_" + to_string(i) + ":\t.double\t" + to_string(symbol.value.double_value));
                    break;

                default:
                    break;
            }
        }
    }
}

// return string contains ".bss"
void generate_target_bss_asm(vector<string>& target_bss) {
    target_bss.emplace_back(".bss");

    for (SymbolEntry& symbol : symbol_table) {
        if (symbol.function_index == -1 && symbol.is_array && !symbol.is_temp) {
            target_bss.push_back(symbol.content + ":\t.fill\t" + to_string(symbol.memory_size));
        }
    }
}

void sp_sub_back_patch(vector<string> &target_text) {
    int* function_lowest_mem_offset = new int[Function::function_table.size()];
    int target_arch_offset = 0;
    for (int i = 0; i < Function::function_table.size(); ++i) {
        function_lowest_mem_offset[i] = target_arch_offset;
    }

    for (SymbolEntry& symbol : symbol_table) {
        if (symbol.memory_offset != INVALID_MEMORY_OFFSET && symbol.function_index >= 0 && function_lowest_mem_offset[symbol.function_index] > (symbol.memory_offset)) {
            function_lowest_mem_offset[symbol.function_index] = symbol.memory_offset;
        }
    }

    for (int i = 0; i < function_entrance_points.size(); ++i) {
        target_text[function_entrance_points[i]] = "enter\t$" + to_string(-1 * (function_lowest_mem_offset[i])) + ", $0";
    }

    for (int i = 0; i < function_leave_correction_points.size(); ++i) {
        if (function_lowest_mem_offset[leave_correction_function_idx[i]] == 0) {
            target_text[function_leave_correction_points[i]] = "pop\t\t%rbp";
        }
    }

    delete[] function_lowest_mem_offset;
}

void generate_global_info(vector<string> &target_global_info)
{
    for (Function& function : Function::function_table) {
        target_global_info.push_back(".global " + function_name_prefix + function.name);
    }

    for (SymbolEntry& symbol : symbol_table) {
        if (symbol.function_index == -1) {
            target_global_info.push_back(".global .GL_" + symbol.content);
        }
    }
}

void generate_target_asm(string &target_string_str, bool gen_win_style_asm) {
    ip_str = "(%rip)";
    bp_str = "(%rbp)";

    if (gen_win_style_asm) {
        function_name_prefix = "";
    }
    else {
        function_name_prefix = "_";
    }

    init_symbol_table_offset();
    split_base_blocks(optimized_sequence);
    calculate_active_symbol_sets(false);
    last_calc_symbol = -1;
    last_call_return_symbol = -1;
    quaternion_active_info_table.clear();

    vector<string> global_info_str;
    generate_global_info(global_info_str);

    vector<string> text_seg_str;
    text_seg_str.emplace_back(".text");
    for (BaseBlock &base_block: base_blocks) {
        generate_target_text_asm(base_block, text_seg_str);
    }

    sp_sub_back_patch(text_seg_str);

    vector<string> data_seg_str;
    generate_target_data_asm(data_seg_str);

    vector<string> bss_seg_str;
    generate_target_bss_asm(bss_seg_str);

    // concat all strings
    target_string_str = "";
    for (string &global_info: global_info_str) {
        target_string_str += global_info + '\n';
    }

    target_string_str += '\n';

    for (string &data_str: data_seg_str) {
        target_string_str += data_str + '\n';
    }

    target_string_str += '\n';

    for (string &bss_str: bss_seg_str) {
        target_string_str += bss_str + '\n';
    }

    target_string_str += '\n';

    for (string &text_str: text_seg_str) {
        target_string_str += text_str + '\n';
    }

    target_string_str += '\n';
}

void write_target_code(string &target_string_str, ofstream &output_file) {
    output_file << target_string_str << endl;
}
