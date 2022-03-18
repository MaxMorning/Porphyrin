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

#define SCORE_NEXT_USE_RATE 2
#define SCORE_VAR_IN_MEM_RATE 4

void init_symbol_table_offset(int target_arch) {
    for (SymbolEntry &symbol: symbol_table) {
        symbol.memory_offset = INVALID_MEMORY_OFFSET;
    }

    // process function parameter offset
    int address_length = target_arch == TARGET_ARCH_X64 ? 8 : 4;
    for (Function &function: Function::function_table) {
        int current_offset = address_length * 2;
        for (int param_idx: function.parameter_index) {
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

string get_symbol_store_text(int symbol_index, int target_arch);

vector<QuaternionActiveInfo> quaternion_active_info_table;

void generate_active_info_table(BaseBlock &base_block) {
    quaternion_active_info_table.clear();

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
unordered_set<int> xmm_variable_map[8]; // the size of xmm regs
int *variable_reg_map;
int *variable_next_use;

bool inline opr_is_stored_in_xmm(SymbolEntry& symbol)
{
    return (!(symbol.is_array && !symbol.is_temp) && (symbol.data_type == DT_FLOAT || symbol.data_type == DT_DOUBLE));
}

bool gen_entry_code(BaseBlock &base_block, vector<string> &target_text, int target_arch) {
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
            cout << "Hit\t" << base_block.start_index << "\t" << base_block.end_index << endl;
            target_text.push_back('_' + function.name + ':');

            target_text.emplace_back("enter $?, $0");

            function_entrance_points.push_back(target_text.size() - 1);

            current_stack_top_addr = 0;
            last_calc_symbol = -1;
            current_function = i;
            return true;
        }
    }

    // not an entrance
    target_text.push_back(".L" + to_string(base_block.start_index) + ':');

    return false;
}

void generate_store(int reg_idx, int store_var_idx, vector<string> &target_text, int target_arch) {
    int arch_unit = target_arch == TARGET_ARCH_X64 ? -8 : -4;
    if (symbol_table[store_var_idx].memory_offset == INVALID_MEMORY_OFFSET) {
        int target_mem_offset = current_stack_top_addr - symbol_table[store_var_idx].memory_size;
        if (target_mem_offset % BASE_DATA_TYPE_SIZE[symbol_table[store_var_idx].data_type] != 0) {
            target_mem_offset -= target_mem_offset % BASE_DATA_TYPE_SIZE[symbol_table[store_var_idx].data_type] + BASE_DATA_TYPE_SIZE[symbol_table[store_var_idx].data_type];
        }
        symbol_table[store_var_idx].memory_offset = target_mem_offset;
        current_stack_top_addr = target_mem_offset;
    }

    string bp_base_str = target_arch == TARGET_ARCH_X64 ? "(%rbp)" : "(%ebp)";

    switch (symbol_table[store_var_idx].data_type) {
        case DT_BOOL:
            target_text.push_back(
                    "movb\t%" + GPRStr[reg_idx][0] + ", " + to_string(symbol_table[store_var_idx].memory_offset) +
                    bp_base_str);
            break;

        case DT_INT:
            target_text.push_back(
                    "movl\t%" + GPRStr[reg_idx][2] + ", " + to_string(symbol_table[store_var_idx].memory_offset) +
                    bp_base_str);
            break;

        case DT_FLOAT:
            target_text.push_back(
                    "movss\t%" + XMMRegStr[reg_idx] + ", " + to_string(symbol_table[store_var_idx].memory_offset) +
                    bp_base_str);
            break;

        case DT_DOUBLE:
            target_text.push_back(
                    "movsd\t%" + XMMRegStr[reg_idx] + ", " + to_string(symbol_table[store_var_idx].memory_offset) +
                    bp_base_str);
            break;

        default:
            break;
    }
}

int get_gpr_two_opr_with_result(int quaternion_idx, vector<string> &target_text, int target_arch) {
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];
    QuaternionActiveInfo &current_active_info = quaternion_active_info_table[quaternion_idx];

    int opr1_reg = variable_reg_map[current_quaternion.opr1];
    bool opr1_is_float_double = opr_is_stored_in_xmm(symbol_table[current_quaternion.opr1]);
    if (opr1_reg != -1) {
        // some reg keeps opr1
        if (opr1_is_float_double) {
            // store in xmm reg
            if (xmm_variable_map[opr1_reg].size() == 1) {
                // this xmm reg only store opr1
                if (current_active_info.opr1_active_info == NON_ACTIVE ||
                    current_quaternion.opr1 == current_quaternion.result) {
                    return opr1_reg;
                }
            } else {
                // try to find free xmm reg
                for (int i = 0; i < 8; ++i) {
                    if (xmm_variable_map[i].empty()) {
                        xmm_variable_map[opr1_reg].erase(current_quaternion.opr1);
                        xmm_variable_map[i].insert(current_quaternion.opr1);
                        variable_reg_map[current_quaternion.opr1] = i;

                        if (symbol_table[current_quaternion.opr1].data_type == DT_FLOAT) {
                            target_text.push_back("movss\t%" + XMMRegStr[opr1_reg] + ", %" + XMMRegStr[i]);
                        } else {
                            target_text.push_back("movsd\t%" + XMMRegStr[opr1_reg] + ", %" + XMMRegStr[i]);
                        }
                        return i;
                    }
                }

                // find a used xmm reg
                int max_score = -3;
                int chosen_reg_idx = -1;

                for (int i = 0; i < 8; ++i) {
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

                bool opr1_in_chosen_reg = variable_reg_map[current_quaternion.opr1] != chosen_reg_idx;

                // generate data store code
                for (int var_idx: xmm_variable_map[chosen_reg_idx]) {
                    if (var_idx != current_quaternion.result && variable_next_use[var_idx] > quaternion_idx) {
                        generate_store(chosen_reg_idx, var_idx, target_text, target_arch);
                        variable_reg_map[var_idx] = -1;
                    }
                }

                // copy opr1 value
                if (opr1_in_chosen_reg) {
                    if (symbol_table[current_quaternion.opr1].data_type == DT_FLOAT) {
                        target_text.push_back("movss\t%" + XMMRegStr[opr1_reg] + ", %" + XMMRegStr[chosen_reg_idx]);
                    } else {
                        target_text.push_back("movsd\t%" + XMMRegStr[opr1_reg] + ", %" + XMMRegStr[chosen_reg_idx]);
                    }
                }

                xmm_variable_map[chosen_reg_idx].clear();
                xmm_variable_map[chosen_reg_idx].insert(current_quaternion.opr1);
                return chosen_reg_idx;
            }
        } else {
            // store in gpr
            if (gpr_variable_map[opr1_reg].size() == 1) {
                // this gpr only store opr1
                if (current_active_info.opr1_active_info == NON_ACTIVE ||
                    current_quaternion.opr1 == current_quaternion.result) {
                    return opr1_reg;
                }
            } else {
                // try to find free gpr
                int max_gpr_cnt = target_arch == TARGET_ARCH_X64 ? X64GPRCNT : X86GPRCNT;
                for (int i = 0; i < max_gpr_cnt; ++i) {
                    if (gpr_variable_map[i].empty()) {
                        gpr_variable_map[opr1_reg].erase(current_quaternion.opr1);
                        gpr_variable_map[i].insert(current_quaternion.opr1);
                        variable_reg_map[current_quaternion.opr1] = i;

                        if (symbol_table[current_quaternion.opr1].data_type == DT_INT) {
                            target_text.push_back("movl\t%" + GPRStr[opr1_reg][2] + ", %" + GPRStr[i][2]);
                        } else {
                            target_text.push_back("movb\t%" + GPRStr[opr1_reg][0] + ", %" + GPRStr[i][0]);
                        }
                        return i;
                    }
                }

                // find a used gpr
                int max_score = -3;
                int chosen_reg_idx = -1;

                for (int i = 0; i < max_gpr_cnt; ++i) {
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

                bool opr1_in_chosen_reg = variable_reg_map[current_quaternion.opr1] != chosen_reg_idx;
                // generate data store code
                for (int var_idx: gpr_variable_map[chosen_reg_idx]) {
                    if (var_idx != current_quaternion.result && variable_next_use[var_idx] > quaternion_idx) {
                        generate_store(chosen_reg_idx, var_idx, target_text, target_arch);
                        variable_reg_map[var_idx] = -1;
                    }
                }

                // copy opr1 value
                if (opr1_in_chosen_reg) {
                    if (symbol_table[current_quaternion.opr1].data_type == DT_INT) {
                        target_text.push_back("movl\t%" + GPRStr[opr1_reg][2] + ", %" + GPRStr[chosen_reg_idx][2]);
                    } else {
                        target_text.push_back("movb\t%" + GPRStr[opr1_reg][0] + ", %" + GPRStr[chosen_reg_idx][0]);
                    }
                }

                gpr_variable_map[chosen_reg_idx].clear();
                gpr_variable_map[chosen_reg_idx].insert(current_quaternion.opr1);
                return chosen_reg_idx;
            }
        }
    }

    // find free reg
    if (opr1_is_float_double) {
        // find free xmm reg
        for (int i = 0; i < 8; ++i) {
            if (xmm_variable_map[i].empty()) {
                assert(symbol_table[current_quaternion.opr1].memory_offset != INVALID_MEMORY_OFFSET);

                if (symbol_table[current_quaternion.opr1].data_type == DT_FLOAT) {
                    target_text.push_back("movss\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" + XMMRegStr[i]);
                } else {
                    target_text.push_back("movsd\t%" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" + XMMRegStr[i]);
                }

                if (opr1_reg != -1) {
                    xmm_variable_map[opr1_reg].erase(current_quaternion.opr1);
                }
                xmm_variable_map[i].insert(current_quaternion.opr1);
                variable_reg_map[current_quaternion.opr1] = i;
                return i;
            }
        }
    } else {
        // find free gpr
        int max_gpr_cnt = target_arch == TARGET_ARCH_X64 ? X64GPRCNT : X86GPRCNT;
        for (int i = 0; i < max_gpr_cnt; ++i) {
            if (gpr_variable_map[i].empty()) {
                assert(symbol_table[current_quaternion.opr1].memory_offset != INVALID_MEMORY_OFFSET);

                if (symbol_table[current_quaternion.opr1].data_type == DT_INT) {
                    target_text.push_back("movl\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" + GPRStr[i][2]);
                } else {
                    target_text.push_back("movb\t%" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" + GPRStr[i][0]);
                }

                if (opr1_reg != -1) {
                    gpr_variable_map[opr1_reg].erase(current_quaternion.opr1);
                }
                gpr_variable_map[i].insert(current_quaternion.opr1);
                variable_reg_map[current_quaternion.opr1] = i;

                return i;
            }
        }
    }

    // try to alloc a used reg
    int max_score = -3;
    int chosen_reg_idx = -1;
    if (opr1_is_float_double) {
        for (int i = 0; i < 8; ++i) {
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

        // generate data store code
        for (int var_idx: xmm_variable_map[chosen_reg_idx]) {
            if (var_idx != current_quaternion.result && variable_next_use[var_idx] > quaternion_idx) {
                generate_store(chosen_reg_idx, var_idx, target_text, target_arch);
                variable_reg_map[var_idx] = -1;
            }
        }

        // load opr1 value
        assert(variable_reg_map[current_quaternion.opr1] != chosen_reg_idx);
        if (symbol_table[current_quaternion.opr1].data_type == DT_FLOAT) {
            target_text.push_back("movss\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" +
                                  XMMRegStr[chosen_reg_idx]);
        } else {
            target_text.push_back("movsd\t%" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" +
                                  XMMRegStr[chosen_reg_idx]);
        }

        xmm_variable_map[chosen_reg_idx].clear();
        xmm_variable_map[chosen_reg_idx].insert(current_quaternion.opr1);
        return chosen_reg_idx;
    } else {
        int max_gpr_cnt = target_arch == TARGET_ARCH_X64 ? X64GPRCNT : X86GPRCNT;
        for (int i = 0; i < max_gpr_cnt; ++i) {
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

        // generate data store code
        for (int var_idx: gpr_variable_map[chosen_reg_idx]) {
            if (var_idx != current_quaternion.result && var_idx != current_quaternion.opr1 &&
                variable_next_use[var_idx] > quaternion_idx) {
                generate_store(chosen_reg_idx, var_idx, target_text, target_arch);
                variable_reg_map[var_idx] = -1;
            }
        }

        // load opr1 value
        assert(variable_reg_map[current_quaternion.opr1] != chosen_reg_idx);
        if (symbol_table[current_quaternion.opr1].data_type == DT_INT) {
            target_text.push_back("movl\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" +
                                  GPRStr[chosen_reg_idx][2]);
        } else {
            target_text.push_back("movb\t%" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" +
                                  GPRStr[chosen_reg_idx][0]);
        }

        gpr_variable_map[chosen_reg_idx].clear();
        gpr_variable_map[chosen_reg_idx].insert(current_quaternion.opr1);
        return chosen_reg_idx;
    }
}

int alloc_one_free_reg(int quaternion_idx, int symbol_idx, vector<string> &target_text, int target_arch) {
    bool opr1_is_float_double = opr_is_stored_in_xmm(symbol_table[symbol_idx]);

    // find free reg
    if (opr1_is_float_double) {
        // find free xmm reg
        for (int i = 0; i < 8; ++i) {
            if (xmm_variable_map[i].empty()) {
                xmm_variable_map[i].insert(symbol_idx);
                variable_reg_map[symbol_idx] = i;

                return i;
            }
        }
    } else {
        // find free gpr
        int max_gpr_cnt = target_arch == TARGET_ARCH_X64 ? X64GPRCNT : X86GPRCNT;
        for (int i = 0; i < max_gpr_cnt; ++i) {
            if (gpr_variable_map[i].empty()) {
                gpr_variable_map[i].insert(symbol_idx);
                variable_reg_map[symbol_idx] = i;

                return i;
            }
        }
    }

    // try to alloc a used reg
    int max_score = -3;
    int chosen_reg_idx = -1;
    if (opr1_is_float_double) {
        for (int i = 0; i < 8; ++i) {
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

        // generate data store code
        for (int var_idx: xmm_variable_map[chosen_reg_idx]) {
            if (var_idx != symbol_idx && variable_next_use[var_idx] > quaternion_idx) {
                generate_store(chosen_reg_idx, var_idx, target_text, target_arch);
                variable_reg_map[var_idx] = -1;
            }
        }

        xmm_variable_map[chosen_reg_idx].clear();
        xmm_variable_map[chosen_reg_idx].insert(symbol_idx);
        return chosen_reg_idx;
    } else {
        int max_gpr_cnt = target_arch == TARGET_ARCH_X64 ? X64GPRCNT : X86GPRCNT;
        for (int i = 0; i < max_gpr_cnt; ++i) {
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

        // generate data store code
        for (int var_idx: gpr_variable_map[chosen_reg_idx]) {
            if (var_idx != symbol_idx && variable_next_use[var_idx] > quaternion_idx) {
                generate_store(chosen_reg_idx, var_idx, target_text, target_arch);
                variable_reg_map[var_idx] = -1;
            }
        }
        gpr_variable_map[chosen_reg_idx].clear();
        gpr_variable_map[chosen_reg_idx].insert(symbol_idx);
        return chosen_reg_idx;
    }
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
    assert(gpr_variable_map[opr1_reg_idx].size() == 1);
    reg_variable_map[opr1_reg_idx].clear();
    variable_reg_map[opr1_idx] = -1;
    reg_variable_map[opr1_reg_idx].insert(result_idx);
    variable_reg_map[result_idx] = opr1_reg_idx;
}

string get_symbol_store_text(int symbol_index, int target_arch)
{
    string ip_str = target_arch == TARGET_ARCH_X64 ? "(%rip)" : "(%eip)";
    string bp_str = target_arch == TARGET_ARCH_X64 ? "(%rbp)" : "(%ebp)";
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
    int result_reg = alloc_one_free_reg(quaternion_idx, current_quaternion.result, target_text, target_arch);
    if (symbol_table[current_quaternion.opr1].is_const) {
        assert(!symbol_table[current_quaternion.opr2].is_const);
        target_text.push_back("cmpl\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) +
                              ", $" + to_string(symbol_table[current_quaternion.opr1].value.int_value));
    }
    else if (symbol_table[current_quaternion.opr2].is_const) {
        assert(!symbol_table[current_quaternion.opr1].is_const);
        target_text.push_back("cmpl\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) +
                              ", " + get_symbol_store_text(current_quaternion.opr1, target_arch));
    }
    else if (variable_reg_map[current_quaternion.opr1] == -1 && variable_reg_map[current_quaternion.opr2] == -1){
        // two opr is in memory
        // load opr2 from memory
        int opr2_reg = alloc_one_free_reg(quaternion_idx, current_quaternion.result, target_text, target_arch);
        target_text.push_back("movl\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" + GPRStr[opr2_reg][2]);
        target_text.push_back("cmpl\t%" + GPRStr[opr2_reg][2] + ", " + get_symbol_store_text(current_quaternion.opr1, target_arch));
    }
    else {
        target_text.push_back("cmpl\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", " + get_symbol_store_text(current_quaternion.opr1, target_arch));
    }

    return result_reg;
}

int cmp_float_code(int quaternion_idx, vector<string>& target_text, int target_arch)
{
    // todo not implemented
    string ip_str = target_arch == TARGET_ARCH_X64 ? "(%rip)" : "(%eip)";

    Quaternion& current_quaternion = optimized_sequence[quaternion_idx];
    int result_reg = alloc_one_free_reg(quaternion_idx, current_quaternion.result, target_text, target_arch);
    if (symbol_table[current_quaternion.opr1].is_const) {
        assert(!symbol_table[current_quaternion.opr2].is_const);
        target_text.push_back("ucomiss\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) +
                              ", .RO_" + to_string(symbol_table[current_quaternion.opr1].value.float_value) + ip_str);
    }
    else if (symbol_table[current_quaternion.opr2].is_const) {
        assert(!symbol_table[current_quaternion.opr1].is_const);
        target_text.push_back("ucomiss\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.float_value) + ip_str +
                              ", " + get_symbol_store_text(current_quaternion.opr1, target_arch));
    }
    else if (variable_reg_map[current_quaternion.opr1] == -1 && variable_reg_map[current_quaternion.opr2] == -1){
        // two opr is in memory
        // load opr2 from memory
        int opr2_reg = alloc_one_free_reg(quaternion_idx, current_quaternion.result, target_text, target_arch);
        target_text.push_back("movl\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" + GPRStr[opr2_reg][2]);
        target_text.push_back("cmpl\t%" + GPRStr[opr2_reg][2] + ", " + get_symbol_store_text(current_quaternion.opr1, target_arch));
    }
    else {
        target_text.push_back("cmpl\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", " + get_symbol_store_text(current_quaternion.opr1, target_arch));
    }

    return result_reg;
}


void generate_quaternion_text(int quaternion_idx, vector<string> &target_text, int target_arch, BaseBlock& base_block) {
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];
    update_next_use_table(quaternion_idx);

    string sp_str = (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %");
    string ip_str = (target_arch == TARGET_ARCH_X64 ? "(%rip), %" : "(%eip), %");

    if (OP_CODE_OPR_USAGE[current_quaternion.op_code][2] == USAGE_VAR) {
        last_calc_symbol = current_quaternion.result;
    }

    switch (current_quaternion.op_code) {
        case OP_ASSIGNMENT: {
            int alloc_reg_index = alloc_one_free_reg(quaternion_idx, current_quaternion.result, target_text,
                                                     target_arch);
            if (symbol_table[current_quaternion.opr1].is_const) {
                // load imm
                switch (symbol_table[current_quaternion.result].data_type) {
                    case DT_BOOL:
                        if (!symbol_table[current_quaternion.opr1].value.bool_value) {
                            target_text.push_back("xorb\t%" + GPRStr[alloc_reg_index][0] + ", %" +
                                                  GPRStr[alloc_reg_index][0]); // xor to optimize
                        } else {
                            target_text.push_back("movb\t$1, %" + GPRStr[alloc_reg_index][0]);
                        }
                        break;

                    case DT_INT:
                        if (symbol_table[current_quaternion.opr1].value.int_value == 0) {
                            target_text.push_back("xorl\t%" + GPRStr[alloc_reg_index][2] + ", %" +
                                                  GPRStr[alloc_reg_index][2]); // xor to optimize
                        } else {
                            target_text.push_back(
                                    "movl\t$" + to_string(symbol_table[current_quaternion.opr1].value.int_value) +
                                    ", %" + GPRStr[alloc_reg_index][2]);
                        }
                        break;

                    case DT_FLOAT:
                        if (symbol_table[current_quaternion.opr1].value.float_value == 0.0f) {
                            target_text.push_back("xorps\t%" + XMMRegStr[alloc_reg_index] + ", %" +
                                                  XMMRegStr[alloc_reg_index]); // xor to optimize
                        } else {
                            target_text.push_back(
                                    "movss\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.float_value) +
                                    (target_arch == TARGET_ARCH_X64 ? "(%rip), %" : "(%eip), %") +
                                    XMMRegStr[alloc_reg_index]);
                        }
                        break;

                    case DT_DOUBLE:
                        if (symbol_table[current_quaternion.opr1].value.double_value == 0.0) {
                            target_text.push_back("xorpd\t%" + XMMRegStr[alloc_reg_index] + ", %" +
                                                  XMMRegStr[alloc_reg_index]); // xor to optimize
                        } else {
                            target_text.push_back("movsd\t.RO_" +
                                                  to_string(symbol_table[current_quaternion.opr1].value.double_value) +
                                                  (target_arch == TARGET_ARCH_X64 ? "(%rip), %" : "(%eip), %") +
                                                  XMMRegStr[alloc_reg_index]);
                        }
                        break;

                    default:
                        break;
                }
            }
            else {
                // assign to a variable
                if (variable_reg_map[current_quaternion.opr1] == -1) {
                    // opr1 in memory
                    switch (symbol_table[current_quaternion.result].data_type) {
                        case DT_BOOL:
                            target_text.push_back(
                                    "movb\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" +
                                    GPRStr[alloc_reg_index][0]);
                            break;

                        case DT_INT:
                            target_text.push_back(
                                    "movl\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" +
                                    GPRStr[alloc_reg_index][2]);
                            break;

                        case DT_FLOAT:
                            target_text.push_back(
                                    "movss\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" + XMMRegStr[alloc_reg_index]);
                            break;

                        case DT_DOUBLE:
                            target_text.push_back(
                                    "movsd\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %" + XMMRegStr[alloc_reg_index]);
                            break;

                        default:
                            break;
                    }
                } else {
                    variable_reg_map[current_quaternion.result] = alloc_reg_index;
                    if (opr_is_stored_in_xmm(symbol_table[current_quaternion.result])) {
                        xmm_variable_map[alloc_reg_index].insert(current_quaternion.result);
                    } else {
                        gpr_variable_map[alloc_reg_index].insert(current_quaternion.result);
                    }
                }
            }
            break;
        }

        case OP_ADD_INT: {
            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text, target_arch);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "addl\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }
            else {
                // opr2 in memory or reg
                target_text.push_back(
                        "addl\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, gpr_variable_map);
            break;
        }

        case OP_ADD_FLOAT: {
            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text, target_arch);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "addss\t.RO_" + to_string(symbol_table[current_quaternion.opr2].value.float_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else {
                // opr2 in register or mem
                target_text.push_back("addss%\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" +
                                      XMMRegStr[opr1_reg_idx]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, xmm_variable_map);
            break;
        }

        case OP_ADD_DOUBLE: {
            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text, target_arch);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "addsd\t.RO_" + to_string(symbol_table[current_quaternion.opr2].value.double_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else {
                // opr2 in register or mem
                target_text.push_back("addsd%\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" +
                                      XMMRegStr[opr1_reg_idx]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, xmm_variable_map);
            break;
        }

        case OP_MINUS_INT: {
            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text, target_arch);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "subl\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }
            else {
                // opr2 in memory or reg
                target_text.push_back(
                        "subl\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, gpr_variable_map);
            break;
        }

        case OP_MINUS_FLOAT: {
            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text, target_arch);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "subss\t.RO_" + to_string(symbol_table[current_quaternion.opr2].value.float_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else {
                // opr2 in register or mem
                target_text.push_back("subss%\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" +
                                      XMMRegStr[opr1_reg_idx]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, xmm_variable_map);
            break;
        }

        case OP_MINUS_DOUBLE: {
            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text, target_arch);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "subsd\t.RO_" + to_string(symbol_table[current_quaternion.opr2].value.double_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else {
                // opr2 in register or mem
                target_text.push_back("subsd%\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" +
                                      XMMRegStr[opr1_reg_idx]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, xmm_variable_map);
            break;
        }

        case OP_MULTI_INT: {
            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text, target_arch);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "imul\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) + ", " +
                        GPRStr[opr1_reg_idx][2]);
            }
            else {
                // opr2 in memory or reg
                target_text.push_back(
                        "imul\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" +
                        GPRStr[opr1_reg_idx][2]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, gpr_variable_map);
            break;
        }

        case OP_MULTI_FLOAT: {
            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text, target_arch);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "mulss\t.RO_" + to_string(symbol_table[current_quaternion.opr2].value.float_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else {
                // opr2 in register or mem
                target_text.push_back("mulss%\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" +
                                      XMMRegStr[opr1_reg_idx]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, xmm_variable_map);
            break;
        }

        case OP_MULTI_DOUBLE: {
            assert(!symbol_table[current_quaternion.opr1].is_const);
            int opr1_reg_idx = get_gpr_two_opr_with_result(quaternion_idx, target_text, target_arch);
            if (symbol_table[current_quaternion.opr2].is_const) {
                // opr2 is const
                target_text.push_back(
                        "mulsd\t.RO_" + to_string(symbol_table[current_quaternion.opr2].value.double_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else {
                // opr2 in register or mem
                target_text.push_back("mulsd%\t" + get_symbol_store_text(current_quaternion.opr2, target_arch) + ", %" +
                                      XMMRegStr[opr1_reg_idx]);
            }

            remove_opr1_set_result(current_quaternion.opr1, opr1_reg_idx, current_quaternion.result, xmm_variable_map);
            break;
        }

        case OP_DIV_INT:
            break;
        case OP_DIV_FLOAT:
            break;
        case OP_DIV_DOUBLE:
            break;
        case OP_COMMA:
            break;
        case OP_LOGIC_OR:
            break;
        case OP_LOGIC_AND:
            break;
        case OP_LOGIC_NOT:
            break;
        case OP_JEQ: {
            throw "You need implement jeq instr now!!";

            break;
        }

        case OP_JNZ: {
            int prev_set_instr_like_point = target_text.size() - 1;

            // save out set variables
            for (int out_sym : base_block.out_set) {
                if (variable_reg_map[out_sym] != -1) {
                    // stores in reg, need write back
                    generate_store(variable_reg_map[out_sym], out_sym, target_text, target_arch);
                }
            }

            // optimize
            // quaternion_active_info_table[quaternion_idx - 1].result_active_info == quaternion_idx because current ir (jnz) is the exit
            // of base block, the opr1 of jnz ir will not be referenced by later ir, because there will be no ir follow jnz in the same base block

            if (quaternion_idx > 0 && current_quaternion.opr1 == optimized_sequence[quaternion_idx - 1].result &&
                quaternion_active_info_table[quaternion_idx - 1].result_active_info == quaternion_idx &&
                optimized_sequence[quaternion_idx - 1].op_code >= OP_EQUAL_INT && optimized_sequence[quaternion_idx - 1].op_code <= OP_NOT_EQUAL_DOUBLE) {
                assert(target_text[prev_set_instr_like_point].substr(0, 3) == "set");

//                cout << "Optimize " << quaternion_idx << " Hit" << endl;
                target_text[prev_set_instr_like_point] = "";

                // optimize jz
                if (current_quaternion.result == 2 && quaternion_idx + 1 < optimized_sequence.size() &&
                    optimized_sequence[quaternion_idx + 1].op_code == OP_JMP) {
                    current_quaternion.result = optimized_sequence[quaternion_idx + 1].result + 1;
                    optimized_sequence[quaternion_idx + 1] = {OP_NOP, -1, -1, -1};

                    string jump_dest = to_string(current_quaternion.result + quaternion_idx);

                    // generate neg instr
                    switch (optimized_sequence[quaternion_idx - 1].op_code) {
                        case OP_EQUAL_INT:
                        case OP_EQUAL_FLOAT:
                        case OP_EQUAL_DOUBLE:
                            target_text.push_back("jne\t\t.L" + jump_dest);
                            break;

                        case OP_GREATER_INT:
                        case OP_GREATER_FLOAT:
                        case OP_GREATER_DOUBLE:
                            target_text.push_back("jle\t\t.L" + jump_dest);
                            break;

                        case OP_GREATER_EQUAL_INT:
                        case OP_GREATER_EQUAL_FLOAT:
                        case OP_GREATER_EQUAL_DOUBLE:
                            target_text.push_back("jl\t\t.L" + jump_dest);
                            break;

                        case OP_LESS_INT:
                        case OP_LESS_FLOAT:
                        case OP_LESS_DOUBLE:
                            target_text.push_back("jge\t\t.L" + jump_dest);
                            break;

                        case OP_LESS_EQUAL_INT:
                        case OP_LESS_EQUAL_FLOAT:
                        case OP_LESS_EQUAL_DOUBLE:
                            target_text.push_back("jg\t\t.L" + jump_dest);
                            break;

                        case OP_NOT_EQUAL_INT:
                        case OP_NOT_EQUAL_FLOAT:
                        case OP_NOT_EQUAL_DOUBLE:
                            target_text.push_back("jeq\t\t.L" + jump_dest);
                            break;

                        default:
                            // here should not be executed!
                            break;
                    }
                }
                else {
                    // generate normal instr
                    string jump_dest = to_string(current_quaternion.result + quaternion_idx);

                    switch (optimized_sequence[quaternion_idx - 1].op_code) {
                        case OP_EQUAL_INT:
                        case OP_EQUAL_FLOAT:
                        case OP_EQUAL_DOUBLE:
                            target_text.push_back("jeq\t\t.L" + jump_dest);
                            break;

                        case OP_GREATER_INT:
                        case OP_GREATER_FLOAT:
                        case OP_GREATER_DOUBLE:
                            target_text.push_back("jg\t\t.L" + jump_dest);
                            break;

                        case OP_GREATER_EQUAL_INT:
                        case OP_GREATER_EQUAL_FLOAT:
                        case OP_GREATER_EQUAL_DOUBLE:
                            target_text.push_back("jge\t\t.L" + jump_dest);
                            break;

                        case OP_LESS_INT:
                        case OP_LESS_FLOAT:
                        case OP_LESS_DOUBLE:
                            target_text.push_back("jl\t\t.L" + jump_dest);
                            break;

                        case OP_LESS_EQUAL_INT:
                        case OP_LESS_EQUAL_FLOAT:
                        case OP_LESS_EQUAL_DOUBLE:
                            target_text.push_back("jle\t\t.L" + jump_dest);
                            break;

                        case OP_NOT_EQUAL_INT:
                        case OP_NOT_EQUAL_FLOAT:
                        case OP_NOT_EQUAL_DOUBLE:
                            target_text.push_back("jne\t\t.L" + jump_dest);
                            break;

                        default:
                            // here should not be executed!
                            break;
                    }
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
                    target_text.push_back(test_str + get_symbol_store_text(current_quaternion.opr1, target_arch));
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
                    generate_store(variable_reg_map[out_sym], out_sym, target_text, target_arch);
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
            break;
        case OP_FETCH_FLOAT:
            break;
        case OP_FETCH_DOUBLE:
            break;
        case OP_EQUAL_INT:
            break;
        case OP_EQUAL_FLOAT:
            break;
        case OP_EQUAL_DOUBLE:
            break;
        case OP_GREATER_INT: {
            int result_reg = cmp_int_code(quaternion_idx, target_text, target_arch);

            target_text.push_back("setg\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_GREATER_FLOAT:
            break;
        case OP_GREATER_DOUBLE:
            break;
        case OP_GREATER_EQUAL_INT:
            break;
        case OP_GREATER_EQUAL_FLOAT:
            break;
        case OP_GREATER_EQUAL_DOUBLE:
            break;

        case OP_LESS_INT: {
            int result_reg = cmp_int_code(quaternion_idx, target_text, target_arch);

            target_text.push_back("setl\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_LESS_FLOAT:
            break;
        case OP_LESS_DOUBLE:
            break;
        case OP_LESS_EQUAL_INT: {
            int result_reg = cmp_int_code(quaternion_idx, target_text, target_arch);

            target_text.push_back("setle\t%" + GPRStr[result_reg][0]);

            last_calc_symbol = current_quaternion.result;
            break;
        }

        case OP_LESS_EQUAL_FLOAT:
            break;
        case OP_LESS_EQUAL_DOUBLE:
            break;
        case OP_NOT_EQUAL_INT:
            break;
        case OP_NOT_EQUAL_FLOAT:
            break;
        case OP_NOT_EQUAL_DOUBLE:
            break;
        case OP_NEG:
            break;
        case OP_PAR: {
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
            break;
        }

        case OP_CALL: {
            // use ax to pass return value, so here need to store out active variables which use ax
            for (int symbol_use_ax : gpr_variable_map[0]) {
                if (base_block.out_set.find(symbol_use_ax) != base_block.out_set.end()) {
                    generate_store(0, symbol_use_ax, target_text, target_arch);
                }
            }

            target_text.push_back("call\t_" + Function::function_table[current_quaternion.opr1].name);
            last_call_return_symbol = current_quaternion.result;

            // pop all parameter
            int target_arch_addr_length = target_arch == TARGET_ARCH_X64 ? 8 : 4;
            int para_size = target_arch_addr_length * Function::function_table[current_quaternion.opr1].parameter_index.size();


            if (target_arch == TARGET_ARCH_X64) {
                target_text.push_back("addq\t$" + to_string(para_size) + ", %rsp");
            }
            else {
                target_text.push_back("addl\t$" + to_string(para_size) + ", %esp");
            }

            break;
        }
        case OP_BOOL_TO_CHAR:
            break;
        case OP_BOOL_TO_INT:
            break;
        case OP_BOOL_TO_FLOAT:
            break;
        case OP_BOOL_TO_DOUBLE:
            break;
        case OP_CHAR_TO_BOOL:
            break;
        case OP_CHAR_TO_INT:
            break;
        case OP_CHAR_TO_FLOAT:
            break;
        case OP_CHAR_TO_DOUBLE:
            break;
        case OP_INT_TO_BOOL:
            break;
        case OP_INT_TO_CHAR:
            break;
        case OP_INT_TO_FLOAT:
            break;
        case OP_INT_TO_DOUBLE:
            break;
        case OP_FLOAT_TO_BOOL:
            break;
        case OP_FLOAT_TO_CHAR:
            break;
        case OP_FLOAT_TO_INT:
            break;
        case OP_FLOAT_TO_DOUBLE:
            break;
        case OP_DOUBLE_TO_BOOL:
            break;
        case OP_DOUBLE_TO_CHAR:
            break;
        case OP_DOUBLE_TO_INT:
            break;
        case OP_DOUBLE_TO_FLOAT:
            break;
        case OP_ARRAY_STORE:
            break;

        case OP_RETURN: {
            // store out active variables (it should be global vars)
            for (int symbol_idx : base_block.out_set) {
                if (variable_reg_map[symbol_idx] != -1) {
                    generate_store(variable_reg_map[symbol_idx], symbol_idx, target_text, target_arch);
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
                            target_text.push_back("movss\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.float_value) + ip_str + ", %xmm0");
                            break;

                        case DT_DOUBLE:
                            // use xmm0 to pass return value
                            target_text.push_back("movsd\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.double_value) + ip_str + ", %xmm0");
                            break;

                        default:
                            break;
                    }
                }
                else if (variable_reg_map[current_quaternion.opr1] != 0) {
                    switch (symbol_table[current_quaternion.opr1].data_type) {
                        case DT_BOOL:
                            // use al to pass return value
                            target_text.push_back("movb\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %al");
                            break;

                        case DT_INT:
                            // use eax to pass return value
                            target_text.push_back("movl\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %eax");
                            break;

                        case DT_FLOAT:
                            // use xmm0 to pass return value
                            target_text.push_back("movss\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %xmm0");
                            break;

                        case DT_DOUBLE:
                            // use xmm0 to pass return value
                            target_text.push_back("movsd\t" + get_symbol_store_text(current_quaternion.opr1, target_arch) + ", %xmm0");
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
            break;
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
    // if last ir is call, ax value already stored.
    if (optimized_sequence[base_block.end_index].op_code != OP_CALL) {
        for (int out_sym : base_block.out_set) {
            if (variable_reg_map[out_sym] != -1) {
                // stores in reg, need write back
                generate_store(variable_reg_map[out_sym], out_sym, target_text, target_arch);
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
                target_data.push_back(".RO_" + to_string(symbol.value.float_value) + ":\t.float\t" + to_string(symbol.value.float_value));
            }
            else if (symbol.data_type == DT_DOUBLE) {
                target_data.push_back(".RO_" + to_string(symbol.value.double_value) + ":\t.double\t" + to_string(symbol.value.double_value));
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
    init_symbol_table_offset(target_arch);
    split_base_blocks(optimized_sequence);
    calculate_active_symbol_sets(false);
    last_calc_symbol = -1;
    last_call_return_symbol = -1;

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
