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

#define INVALID_MEMORY_OFFSET 2147483647

#define ACTIVE_AT_EXIT -2
#define NON_ACTIVE -1

#define X64GPRCNT 14
#define X86GPRCNT 6

#define X64XMMCNT 16

#define SCORE_NEXT_USE_RATE 2
#define SCORE_VAR_IN_MEM_RATE 4


// architecture related variables
string ip_str;
string bp_str;
bool target_is_x64;

int max_gpr_cnt;
int max_xmm_cnt;

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

void init_symbol_table_offset(int target_arch) {
    for (SymbolEntry &symbol: symbol_table) {
        symbol.memory_offset = INVALID_MEMORY_OFFSET;
    }

    // process function parameter offset
    int address_length = target_arch == TARGET_ARCH_X64 ? 8 : 4;
    for (Function &function: Function::function_table) {
        // don't set parameter which will be stored in register
        // count parameter store in xmm
        int xmm_cnt = 0;
        for (int param_idx : function.parameter_index) {
            if (opr_is_stored_in_xmm(symbol_table[param_idx])) {
                ++xmm_cnt;
            }
        }

        int store_in_mem_xmm = max(0, xmm_cnt - max_xmm_cnt + 1); // float / double parameter which will store in memory
        int store_in_mem_gpr = max(0, (int)(function.parameter_index.size() - xmm_cnt) - max_gpr_cnt + 1); // other parameter which will store in memory

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

bool gen_entry_code(BaseBlock &base_block, vector<string> &target_text, int target_arch) {
    if (last_call_return_symbol != -1) {
        if (opr_is_stored_in_xmm(symbol_table[last_call_return_symbol])) {
            xmm_variable_map[0].insert(last_call_return_symbol);
        }
        else {
            gpr_variable_map[0].insert(last_call_return_symbol);
        }

        variable_reg_map[last_call_return_symbol] = 0;
        cout << "Last call " << base_block.start_index << "\t" << last_call_return_symbol << endl;
        last_call_return_symbol = -1;
    }

    for (int i = 0; i < Function::function_table.size(); ++i) {
        Function& function = Function::function_table[i];
        if (base_block.start_index == function.entry_address) {
            // generate entrance code
            cout << "Hit\t" << base_block.start_index << "\t" << base_block.end_index << endl;
            target_text.push_back('_' + function.name + ':');

            target_text.emplace_back("enter $?, $0");

            function_entrance_points.push_back(target_text.size() - 1);

            current_stack_top_addr = 0;
            last_calc_symbol = -1;
            current_function = i;

            par_usable_gpr_index = 1;
            par_usable_xmm_index = 1;

            // set up parameter - register relationship
            int proc_par_usable_gpr_index = 1;
            int proc_par_usable_xmm_index = 1;
            for (int param_idx = function.parameter_index.size() - 1; param_idx >= 0; --param_idx) {
                if (opr_is_stored_in_xmm(symbol_table[function.parameter_index[param_idx]])) {
                    xmm_variable_map[par_usable_xmm_index].insert(function.parameter_index[param_idx]);
                    variable_reg_map[function.parameter_index[param_idx]] = par_usable_xmm_index;
                    ++proc_par_usable_xmm_index;

                    if (proc_par_usable_xmm_index >= max_xmm_cnt) {
                        break;
                    }
                }
                else {
                    gpr_variable_map[par_usable_gpr_index].insert(function.parameter_index[param_idx]);
                    variable_reg_map[function.parameter_index[param_idx]] = par_usable_gpr_index;
                    ++proc_par_usable_gpr_index;

                    if (proc_par_usable_gpr_index >= max_gpr_cnt) {
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
        int arch_unit = target_is_x64 ? -8 : -4;
        if (symbol_table[store_var_idx].memory_offset == INVALID_MEMORY_OFFSET) {
            int target_mem_offset = current_stack_top_addr - symbol_table[store_var_idx].memory_size;
            if (target_mem_offset % BASE_DATA_TYPE_SIZE[symbol_table[store_var_idx].data_type] != 0) {
                target_mem_offset -= target_mem_offset % BASE_DATA_TYPE_SIZE[symbol_table[store_var_idx].data_type] + BASE_DATA_TYPE_SIZE[symbol_table[store_var_idx].data_type];
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
}

int search_free_register(bool is_xmm)
{
    if (is_xmm) {
        for (int i = 0; i < max_xmm_cnt; ++i) {
            if (xmm_variable_map[i].empty() && !xmm_locked[i]) {
                return i;
            }
        }

        return -1;
    }
    else {
        for (int i = 0; i < max_gpr_cnt; ++i) {
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

    for (int i = 0; i < max_gpr_cnt; ++i) {
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

    for (int i = 0; i < max_xmm_cnt; ++i) {
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
            if (variable_reg_map[current_quaternion.opr1] != -1) {
                if ((symbol.is_array && !symbol.is_temp) || symbol.data_type == DT_BOOL || symbol.data_type == DT_INT) {
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
            if (variable_reg_map[current_quaternion.opr2] != -1) {
                if ((symbol.is_array && !symbol.is_temp) || symbol.data_type == DT_BOOL || symbol.data_type == DT_INT) {
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
                if (symbol_table[symbol_index].value.float_value < 0) {
                    return ".RO_F_NEG_" + to_string(-symbol_table[symbol_index].value.float_value) + ip_str;
                }
                else {
                    return ".RO_F_" + to_string(symbol_table[symbol_index].value.float_value) + ip_str;
                }
                break;

            case DT_DOUBLE:
                if (symbol_table[symbol_index].value.double_value < 0) {
                    return ".RO_D_NEG_" + to_string(-symbol_table[symbol_index].value.double_value) + ip_str;
                }
                else {
                    return ".RO_D_" + to_string(symbol_table[symbol_index].value.double_value) + ip_str;
                }
                break;

            default:
                break;
        }
    }

    if (symbol_table[symbol_index].function_index == -1) {
        // is global var
        return ".GL_" + symbol_table[symbol_index].content + ip_str;
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

int cmp_int_code(int quaternion_idx, vector<string>& target_text, int target_arch)
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

int cmp_float_code(int quaternion_idx, vector<string>& target_text, int target_arch, bool& reverse)
{
    reverse = false;
    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];

    bool is_double = symbol_table[current_quaternion.opr1].data_type == DT_DOUBLE;

    string comp_str = is_double ? "ucomisd\t" : "ucomiss\t";
    string mov_str = is_double ? "movsd\t" : "movss\t";

    if (variable_reg_map[current_quaternion.opr1] != -1) {
        if (variable_reg_map[current_quaternion.opr2] != -1) {
            // two opr is in xmm
            target_text.push_back(comp_str + "%" + XMMRegStr[current_quaternion.opr2] + ", %" + XMMRegStr[current_quaternion.opr1]);
        }
        else {
            // opr1 in xmm, opr2 in mem
            target_text.push_back(comp_str + get_symbol_store_text(current_quaternion.opr2) + ", %" + XMMRegStr[current_quaternion.opr1]);
        }
    }
    else {
        if (variable_reg_map[current_quaternion.opr2] != -1) {
            // opr1 in mem, opr2 in xmm
            target_text.push_back(comp_str + get_symbol_store_text(current_quaternion.opr1) + ", %" + XMMRegStr[current_quaternion.opr2]);
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
        target_text.push_back("xchgl\t%" + GPRStr[reg_0][2] + ", %" + GPRStr[reg_1][2]);
    }
}

void lock_symbol_to_gpr(int symbol_idx, int target_reg, vector<string>& target_text, int quaternion_idx)
{
    cout << "LOCK " << variable_reg_map[symbol_idx] << '\t' << target_reg << endl;
    // symbol already in target reg
    if (variable_reg_map[symbol_idx] != target_reg) {
        if (gpr_variable_map[target_reg].empty()) {
            // target reg is empty
            target_text.push_back("movl\t" + get_symbol_store_text(symbol_idx) + ", %" + GPRStr[target_reg][2]);
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
                    target_text.push_back("movl\t%" + GPRStr[target_reg][2] + ", %" + GPRStr[allocated_gpr][2]);
                    // keep mapping
                    for (int symbol_in_target_gpr : gpr_variable_map[target_reg]) {
                        gpr_variable_map[allocated_gpr].insert(symbol_in_target_gpr);
                        variable_reg_map[symbol_in_target_gpr] = allocated_gpr;
                    }
                    gpr_variable_map[target_reg].clear();
                    gpr_locked[allocated_gpr] = false;
                }

                // copy symbol to target_reg
                target_text.push_back("movl\t" + get_symbol_store_text(symbol_idx) + ", %" + GPRStr[target_reg][(symbol_table[symbol_idx].data_type == DT_INT ? 2 : 0)]);
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
    cout << "LOCK " << variable_reg_map[symbol_idx] << '\t' << target_reg << endl;
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
    lock_symbol_to_gpr(current_quaternion.opr1, GeneralPR::AX, target_text, quaternion_idx);

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

void generate_quaternion_text(int quaternion_idx, vector<string> &target_text, int target_arch, BaseBlock& base_block) {
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
                            if (symbol_table[current_quaternion.opr1].value.float_value < 0) {
                                target_text.push_back(
                                        "movss\t.RO_F_NEG_" + to_string(-symbol_table[current_quaternion.opr1].value.float_value) +
                                        ip_str + ", %" +
                                        XMMRegStr[alloc_reg_index]);
                            }
                            else {
                                target_text.push_back(
                                        "movss\t.RO_F_" + to_string(symbol_table[current_quaternion.opr1].value.float_value) +
                                        ip_str + ", %" +
                                        XMMRegStr[alloc_reg_index]);
                            }

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
                            if (symbol_table[current_quaternion.opr1].value.double_value < 0) {
                                target_text.push_back("movsd\t.RO_D_NEG_" +
                                                      to_string(-symbol_table[current_quaternion.opr1].value.double_value) +
                                                      ip_str + ", %" +
                                                      XMMRegStr[alloc_reg_index]);
                            }
                            else {
                                target_text.push_back("movsd\t.RO_D_" +
                                                      to_string(symbol_table[current_quaternion.opr1].value.double_value) +
                                                      ip_str + ", %" +
                                                      XMMRegStr[alloc_reg_index]);
                            }

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
        {
            break;
        }

        case OP_JEQ: {
            throw "You need implement jeq instr now!!";

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

                cout << "Optimize " << quaternion_idx << " Hit" << endl;
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
                            break;

                        case DT_INT:
                            test_str = "testl\t$(-1), ";
                            break;

                        case DT_FLOAT:
                            test_str = "testl\t$(-1), ";
                            break;

                        case DT_DOUBLE:
                            test_str = "testq\t$(-1), ";
                            break;

                        default:
                            break;
                    }
                    target_text.push_back(test_str + get_symbol_store_text(current_quaternion.opr1));
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
            break;
        case OP_FETCH_INT:
        {
            // todo not implement
//            int result_reg = alloc_one_free_gpr(quaternion_idx, target_text);
//            if (symbol_table[current_quaternion.opr1].function_index == -1) {
//                // global array
//            }
            break;
        }
        case OP_FETCH_FLOAT:
            break;
        case OP_FETCH_DOUBLE:
            break;

        case OP_EQUAL_INT:
        {
            int result_reg = cmp_int_code(quaternion_idx, target_text, target_arch);

            target_text.push_back("sete\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_EQUAL_FLOAT:
        case OP_EQUAL_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, target_arch, reverse);

            target_text.push_back("sete\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_GREATER_INT:
        {
            int result_reg = cmp_int_code(quaternion_idx, target_text, target_arch);

            target_text.push_back("setg\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_GREATER_FLOAT:
        case OP_GREATER_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, target_arch, reverse);

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
            int result_reg = cmp_int_code(quaternion_idx, target_text, target_arch);

            target_text.push_back("setge\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_GREATER_EQUAL_FLOAT:
        case OP_GREATER_EQUAL_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, target_arch, reverse);

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
            int result_reg = cmp_int_code(quaternion_idx, target_text, target_arch);

            target_text.push_back("setl\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_LESS_FLOAT:
        case OP_LESS_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, target_arch, reverse);

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
            int result_reg = cmp_int_code(quaternion_idx, target_text, target_arch);

            target_text.push_back("setle\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_LESS_EQUAL_FLOAT:
        case OP_LESS_EQUAL_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, target_arch, reverse);

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
            int result_reg = cmp_int_code(quaternion_idx, target_text, target_arch);

            target_text.push_back("setne\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_NOT_EQUAL_FLOAT:
        case OP_NOT_EQUAL_DOUBLE:
        {
            bool reverse;
            int result_reg = cmp_float_code(quaternion_idx, target_text, target_arch, reverse);

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

                target_text.push_back("neg\t%" + GPRStr[opr1_reg][2]);

                remove_opr1_set_result(current_quaternion.opr1, opr1_reg, current_quaternion.result, gpr_variable_map);
            }
            break;
        }

        case OP_PAR: {
            if (opr_is_stored_in_xmm(symbol_table[current_quaternion.opr1])) {
                if (par_usable_xmm_index >= max_xmm_cnt) { // don't use all regs for passing arguments! ax not used to pass args
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
                        target_text.push_back("push" + string(target_arch == TARGET_ARCH_X64 ? "q" : "l") + "\t$" + get_symbol_store_text(current_quaternion.opr1));
                    }
                }
                else {
                    // use xmm[par_usable_xmm_index] to pass argument
                    lock_symbol_to_xmm(current_quaternion.opr1, par_usable_xmm_index, target_text, quaternion_idx);

                    ++par_usable_xmm_index;
                }
            }
            else {
                if (par_usable_gpr_index >= max_gpr_cnt) {
                    // gpr is not enough, stores in memory
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
                            target_text.push_back("pushl\t%" + GPRStr[variable_reg_map[current_quaternion.opr1]][2]);
                        }
                        else {
                            target_text.push_back("push" + string(target_arch == TARGET_ARCH_X64 ? "q" : "l") + "\t$" + get_symbol_store_text(current_quaternion.opr1));
                        }
                    }
                }
                else {
                    // use gpr[par_usable_gpr_index] to pass parameter
                    lock_symbol_to_gpr(current_quaternion.opr1, par_usable_gpr_index, target_text, quaternion_idx);

                    ++par_usable_gpr_index;
                }
            }
            /*
            if (symbol_table[current_quaternion.opr1].is_const) {
                switch (symbol_table[current_quaternion.opr1].data_type) {
                    case DT_BOOL:
                        if (symbol_table[current_quaternion.opr1].value.bool_value) {
                            target_text.emplace_back("push\t$1");
                        }
                        else {
                            target_text.emplace_back("push\t$0");
                        }
                        break;

                    case DT_INT:
                    case DT_FLOAT: // push float imm as a 32-bit integer, which can be parsed by loading
                        target_text.push_back("push\t$" + to_string(symbol_table[current_quaternion.opr1].value.int_value));
                        break;

                    case DT_DOUBLE:
                        target_text.push_back("push\t$" + to_string(*((long long*)(&symbol_table[current_quaternion.opr1].value))));
                        break;

                    default:
                        break;
                }
            }
            else {
                if (variable_reg_map[current_quaternion.opr1] != -1) {
                    // store in reg
                    if (opr_is_stored_in_xmm(symbol_table[current_quaternion.opr1])) {
                        target_text.push_back("pushq\t%" + XMMRegStr[variable_reg_map[current_quaternion.opr1]]);
                    }
                    else {
                        target_text.push_back("pushq\t%" + GPRStr[variable_reg_map[current_quaternion.opr1]][target_arch == TARGET_ARCH_X64 ? 3 : 2]);
                    }
                }
            }
             */
            break;
        }

        case OP_CALL: {
            // use ax to pass return value, so here need to store out active variables which use ax
            for (int symbol_use_ax : gpr_variable_map[0]) {
                if (base_block.out_set.find(symbol_use_ax) != base_block.out_set.end()) {
                    generate_store(0, symbol_use_ax, target_text);
                }
            }

            target_text.push_back("call\t_" + Function::function_table[current_quaternion.opr1].name);
            last_call_return_symbol = current_quaternion.result;

            // unlock all registers
            memset(gpr_locked, 0, X64GPRCNT * sizeof(bool));
            memset(xmm_locked, 0,  X64XMMCNT* sizeof(bool));
            par_usable_gpr_index = 1;
            par_usable_xmm_index = 1;

            // pop all parameter
            int target_arch_addr_length = target_arch == TARGET_ARCH_X64 ? 8 : 4;
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
            int para_size = target_arch_addr_length * (max(0, xmm_para_cnt - max_xmm_cnt + 1) + max(0, gpr_para_cnt - max_gpr_cnt + 1));


            if (para_size > 0) {
                if (target_arch == TARGET_ARCH_X64) {
                    target_text.push_back("addq\t$" + to_string(para_size) + ", %rsp");
                }
                else {
                    target_text.push_back("addl\t$" + to_string(para_size) + ", %esp");
                }
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
            break;

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
                            if (symbol_table[current_quaternion.opr1].value.float_value < 0) {
                                target_text.push_back("movss\t.RO_F_NEG_" + to_string(
                                        -symbol_table[current_quaternion.opr1].value.float_value) + ip_str + ", %xmm0");
                            }
                            else {
                                target_text.push_back("movss\t.RO_F_" + to_string(
                                        symbol_table[current_quaternion.opr1].value.float_value) + ip_str + ", %xmm0");
                            }
                            break;

                        case DT_DOUBLE:
                            // use xmm0 to pass return value
                            if (symbol_table[current_quaternion.opr1].value.double_value < 0) {
                                target_text.push_back("movsd\t.RO_D_NEG_" + to_string(
                                        -symbol_table[current_quaternion.opr1].value.double_value) + ip_str + ", %xmm0");
                            }
                            else {
                                target_text.push_back("movsd\t.RO_D_" + to_string(
                                        symbol_table[current_quaternion.opr1].value.double_value) + ip_str + ", %xmm0");
                            }
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
//    throw "Not Implemented!";
}

void generate_target_text_asm(BaseBlock &base_block, vector<string>& target_text, int target_arch) {
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

    gen_entry_code(base_block, target_text, target_arch);

    for (int i = base_block.start_index; i <= base_block.end_index; ++i) {
        generate_quaternion_text(i, target_text, target_arch, base_block);
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

    for (SymbolEntry& symbol : symbol_table) {
        if (symbol.is_const) {
            if (symbol.data_type == DT_FLOAT) {
                if (symbol.value.float_value < 0) {
                    target_data.push_back(".RO_F_NEG_" + to_string(-symbol.value.float_value) + ":\t.float\t" + to_string(symbol.value.float_value));
                }
                else {
                    target_data.push_back(".RO_F_" + to_string(symbol.value.float_value) + ":\t.float\t" +
                                          to_string(symbol.value.float_value));
                }
            }
            else if (symbol.data_type == DT_DOUBLE) {
                if (symbol.value.double_value < 0) {
                    target_data.push_back(".RO_D_NEG_" + to_string(-symbol.value.double_value) + ":\t.double\t" +
                                          to_string(symbol.value.double_value));
                }
                else {
                    target_data.push_back(".RO_D_" + to_string(symbol.value.double_value) + ":\t.double\t" +
                                          to_string(symbol.value.double_value));
                }
            }
        }
        else if (symbol.function_index == -1 && !symbol.is_array) {
            // global symbol
            switch (symbol.data_type) {
                case DT_BOOL:
                    target_data.push_back(".GL_" + symbol.content + ":\t.bool\t" + (symbol.value.bool_value ? '1' : '0'));
                    break;

                case DT_INT:
                    target_data.push_back(".GL_" + symbol.content + ":\t.int\t" + to_string(symbol.value.int_value));
                    break;

                case DT_FLOAT:
                    target_data.push_back(".GL_" + symbol.content + ":\t.float\t" + to_string(symbol.value.float_value));
                    break;

                case DT_DOUBLE:
                    target_data.push_back(".GL_" + symbol.content + ":\t.double\t" + to_string(symbol.value.double_value));
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
        if (symbol.is_array && !symbol.is_temp) {
            target_bss.push_back(symbol.content + "\t.fill\t" + to_string(symbol.memory_size));
        }
    }
}

void sp_sub_back_patch(vector<string> &target_text, int target_arch) {
    int* function_lowest_mem_offset = new int[Function::function_table.size()];
    int target_arch_offset = 0;
    for (int i = 0; i < Function::function_table.size(); ++i) {
        function_lowest_mem_offset[i] = target_arch_offset;
    }

    for (SymbolEntry& symbol : symbol_table) {
        if (symbol.memory_offset != INVALID_MEMORY_OFFSET) {
            cout << "UUU" << symbol.memory_offset << endl;
        }
        if (symbol.memory_offset != INVALID_MEMORY_OFFSET && symbol.function_index >= 0 && function_lowest_mem_offset[symbol.function_index] > (symbol.memory_offset)) {
            function_lowest_mem_offset[symbol.function_index] = symbol.memory_offset;
            cout << "Update " << symbol.memory_offset << endl;
        }
    }

    for (int i = 0; i < function_entrance_points.size(); ++i) {
        target_text[function_entrance_points[i]] = "enter\t$" + to_string(-1 * (function_lowest_mem_offset[i])) + ", $0";
    }

    for (int i = 0; i < function_leave_correction_points.size(); ++i) {
        if (function_lowest_mem_offset[leave_correction_function_idx[i]] == 0) {
            target_text[function_leave_correction_points[i]] = target_arch == TARGET_ARCH_X64 ? "pop\t\t%rbp" : "pop\t\t%ebp";
        }
    }

    delete[] function_lowest_mem_offset;
}

void generate_global_info(vector<string> &target_global_info)
{
    for (Function& function : Function::function_table) {
        target_global_info.push_back(".global _" + function.name);
    }

    for (SymbolEntry& symbol : symbol_table) {
        if (symbol.function_index == -1) {
            target_global_info.push_back(".global .GL_" + symbol.content);
        }
    }
}

void generate_target_asm(string &target_string_str, int target_arch) {
    ip_str = target_arch == TARGET_ARCH_X64 ? "(%rip)" : "(%eip)";
    bp_str = target_arch == TARGET_ARCH_X64 ? "(%rbp)" : "(%ebp)";
    target_is_x64 = target_arch == TARGET_ARCH_X64;
    max_gpr_cnt = target_arch == TARGET_ARCH_X64 ? X64GPRCNT : X86GPRCNT;
    max_xmm_cnt = target_arch == TARGET_ARCH_X64 ? 16 : 8;

    init_symbol_table_offset(target_arch);
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
        generate_target_text_asm(base_block, text_seg_str, target_arch);
    }

    sp_sub_back_patch(text_seg_str, target_arch);

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
