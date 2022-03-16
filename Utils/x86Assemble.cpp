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
        int current_offset = address_length;
        for (int param_idx: function.parameter_index) {
            if (symbol_table[param_idx].is_array) {
                current_offset += address_length;
            } else {
                assert(symbol_table[param_idx].data_type != DT_VOID);
                current_offset += BASE_DATA_TYPE_SIZE[symbol_table[param_idx].data_type];
            }
            symbol_table[param_idx].memory_offset = current_offset;
        }
    }
}

vector<QuaternionActiveInfo> quaternion_active_info_table;

void generate_active_info_table(BaseBlock &base_block) {
    quaternion_active_info_table.clear();

    int *symbol_status = new int[symbol_table.size()];

    // initial symbol status
    memset(symbol_status, -1, symbol_table.size() * sizeof(int));

    // set out active symbols
    for (unsigned int out_symbol_idx: base_block.out_set) {
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
    for (unsigned int i = reverse_info_table.size() - 1; i >= 0; --i) {
        quaternion_active_info_table.push_back(reverse_info_table[i]);
    }

    delete[] symbol_status;
}

vector<unsigned int> function_entrance_sp_sub_points;

int current_lowest_free_memory;
int last_calc_symbol;

bool gen_entry_code(BaseBlock &base_block, vector<string> &target_text, int target_arch) {
    for (Function &function: Function::function_table) {
        if (base_block.start_index == function.entry_address) {
            // generate entrance code
            target_text.push_back('_' + function.name + ':');

            if (target_arch == TARGET_ARCH_X64) {
                target_text.emplace_back("\tpush\t %rbp");
                target_text.emplace_back("\tmovq\t %rsp, %rbp");
                target_text.emplace_back("\tsubq\t $?, %rsp"); // keep sp point to the bottom of next frame
            } else {
                target_text.emplace_back("\tpush\t %ebp");
                target_text.emplace_back("\tmovl\t %esp, %ebp");
                target_text.emplace_back("\tsubl\t $?, %esp"); // keep sp point to the bottom of next frame
            }

            function_entrance_sp_sub_points.push_back(target_text.size() - 1);

            current_lowest_free_memory = target_arch == TARGET_ARCH_X64 ? -8 : -4;
            last_calc_symbol = -1;
            return true;
        }
    }

    // not an entrance
    target_text.push_back(".L" + to_string(base_block.start_index) + ':');

    return false;
}

unordered_set<int> gpr_variable_map[X64GPRCNT];
unordered_set<int> xmm_variable_map[8]; // the size of xmm regs
int *variable_reg_map;
int *variable_next_use;

void generate_store(int reg_idx, int store_var_idx, vector<string> &target_text, int target_arch) {
    int arch_unit = target_arch == TARGET_ARCH_X64 ? -8 : -4;
    if (symbol_table[store_var_idx].memory_offset == INVALID_MEMORY_OFFSET) {
        int target_mem_offset = current_lowest_free_memory + arch_unit - (current_lowest_free_memory %
                                                                          BASE_DATA_TYPE_SIZE[symbol_table[store_var_idx].data_type]);
        symbol_table[store_var_idx].memory_offset = target_mem_offset;
        current_lowest_free_memory = target_mem_offset - symbol_table[store_var_idx].memory_size;
    }

    string sp_base_str = target_arch == TARGET_ARCH_X64 ? "(%rsp)" : "(%esp)";

    switch (symbol_table[store_var_idx].data_type) {
        case DT_BOOL:
            target_text.push_back(
                    "movb\t%" + GPRStr[reg_idx][0] + ", " + to_string(symbol_table[store_var_idx].memory_offset) +
                    sp_base_str);
            break;

        case DT_INT:
            target_text.push_back(
                    "movl\t%" + GPRStr[reg_idx][2] + ", " + to_string(symbol_table[store_var_idx].memory_offset) +
                    sp_base_str);
            break;

        case DT_FLOAT:
            target_text.push_back(
                    "movss\t%" + XMMRegStr[reg_idx] + ", " + to_string(symbol_table[store_var_idx].memory_offset) +
                    sp_base_str);
            break;

        case DT_DOUBLE:
            target_text.push_back(
                    "movsd\t%" + XMMRegStr[reg_idx] + ", " + to_string(symbol_table[store_var_idx].memory_offset) +
                    sp_base_str);
            break;

        default:
            break;
    }
}

int get_gpr_two_opr_with_result(int quaternion_idx, vector<string> &target_text, int target_arch) {
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];
    QuaternionActiveInfo &current_active_info = quaternion_active_info_table[quaternion_idx];

    int opr1_reg = variable_reg_map[current_quaternion.opr1];
    bool opr1_is_float_double = symbol_table[current_quaternion.opr1].data_type == DT_FLOAT ||
                                symbol_table[current_quaternion.opr1].data_type == DT_DOUBLE;
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
                if (opr1_reg != -1) {
                    xmm_variable_map[opr1_reg].erase(current_quaternion.opr1);
                }
                xmm_variable_map[i].insert(current_quaternion.opr1);
                variable_reg_map[current_quaternion.opr1] = i;

                if (symbol_table[current_quaternion.opr1].data_type == DT_FLOAT) {
                    target_text.push_back("movss\t" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +
                                          (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") + XMMRegStr[i]);
                } else {
                    target_text.push_back("movsd\t%" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +
                                          (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") + XMMRegStr[i]);
                }
                return i;
            }
        }
    } else {
        // find free gpr
        int max_gpr_cnt = target_arch == TARGET_ARCH_X64 ? X64GPRCNT : X86GPRCNT;
        for (int i = 0; i < max_gpr_cnt; ++i) {
            if (gpr_variable_map[i].empty()) {
                assert(symbol_table[current_quaternion.opr1].memory_offset != INVALID_MEMORY_OFFSET);
                if (opr1_reg != -1) {
                    gpr_variable_map[opr1_reg].erase(current_quaternion.opr1);
                }
                gpr_variable_map[i].insert(current_quaternion.opr1);
                variable_reg_map[current_quaternion.opr1] = i;

                if (symbol_table[current_quaternion.opr1].data_type == DT_INT) {
                    target_text.push_back("movl\t" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +
                                          (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") + GPRStr[i][2]);
                } else {
                    target_text.push_back("movb\t%" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +
                                          (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") + GPRStr[i][0]);
                }
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
            target_text.push_back("movss\t" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +
                                  (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") +
                                  XMMRegStr[chosen_reg_idx]);
        } else {
            target_text.push_back("movsd\t%" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +
                                  (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") +
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
            target_text.push_back("movl\t" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +
                                  (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") +
                                  GPRStr[chosen_reg_idx][2]);
        } else {
            target_text.push_back("movb\t%" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +
                                  (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") +
                                  GPRStr[chosen_reg_idx][0]);
        }

        gpr_variable_map[chosen_reg_idx].clear();
        gpr_variable_map[chosen_reg_idx].insert(current_quaternion.opr1);
        return chosen_reg_idx;
    }
}

int alloc_one_free_reg(int quaternion_idx, int symbol_idx, vector<string> &target_text, int target_arch) {
    bool opr1_is_float_double =
            symbol_table[symbol_idx].data_type == DT_FLOAT || symbol_table[symbol_idx].data_type == DT_DOUBLE;

    // find free reg
    if (opr1_is_float_double) {
        // find free xmm reg
        for (int i = 0; i < 8; ++i) {
            if (xmm_variable_map[i].empty()) {
                xmm_variable_map[i].insert(symbol_idx);
                variable_reg_map[symbol_idx] = i;

                if (symbol_table[symbol_idx].data_type == DT_FLOAT) {
                    target_text.push_back("movss\t" + to_string(symbol_table[symbol_idx].memory_offset) +
                                          (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") + XMMRegStr[i]);
                } else {
                    target_text.push_back("movsd\t%" + to_string(symbol_table[symbol_idx].memory_offset) +
                                          (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") + XMMRegStr[i]);
                }
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

                if (symbol_table[symbol_idx].data_type == DT_INT) {
                    target_text.push_back("movl\t" + to_string(symbol_table[symbol_idx].memory_offset) +
                                          (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") + GPRStr[i][2]);
                } else {
                    target_text.push_back("movb\t%" + to_string(symbol_table[symbol_idx].memory_offset) +
                                          (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %") + GPRStr[i][0]);
                }
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

void remove_opr1_set_result(int opr1_idx, int opr1_reg_idx, int result_idx, unordered_set<int> *reg_variable_map) {
    assert(gpr_variable_map[opr1_reg_idx].size() == 1);
    reg_variable_map[opr1_reg_idx].clear();
    variable_reg_map[opr1_idx] = -1;
    reg_variable_map[opr1_reg_idx].insert(result_idx);
    variable_reg_map[result_idx] = opr1_reg_idx;
}

void generate_quaternion_text(int quaternion_idx, vector<string> &target_text, int target_arch) {
    Quaternion &current_quaternion = optimized_sequence[quaternion_idx];
    update_next_use_table(quaternion_idx);

    string sp_str = (target_arch == TARGET_ARCH_X64 ? "(%rsp), %" : "(%esp), %");
    string ip_str = (target_arch == TARGET_ARCH_X64 ? "(%rip), %" : "(%eip), %");

    if (OP_CODE_OPR_USAGE[current_quaternion.result][2] == USAGE_VAR) {
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
                                    ", %" + GPRStr[alloc_reg_index][0]);
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
            } else {
                // assign to a variable
                if (variable_reg_map[current_quaternion.opr1] == -1) {
                    // opr1 in memory
                    switch (symbol_table[current_quaternion.result].data_type) {
                        case DT_BOOL:
                            target_text.push_back(
                                    "movb\t" + to_string(symbol_table[current_quaternion.opr1].memory_offset) + sp_str +
                                    GPRStr[alloc_reg_index][0]);
                            break;

                        case DT_INT:
                            target_text.push_back(
                                    "movl\t" + to_string(symbol_table[current_quaternion.opr1].memory_offset) + sp_str +
                                    GPRStr[alloc_reg_index][2]);
                            break;

                        case DT_FLOAT:
                            target_text.push_back(
                                    "movss\t" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +
                                    sp_str + XMMRegStr[alloc_reg_index]);
                            break;

                        case DT_DOUBLE:
                            target_text.push_back(
                                    "movsd\t" + to_string(symbol_table[current_quaternion.opr1].memory_offset) +
                                    sp_str + XMMRegStr[alloc_reg_index]);
                            break;

                        default:
                            break;
                    }
                } else {
                    variable_reg_map[current_quaternion.result] = alloc_reg_index;
                    if (symbol_table[current_quaternion.result].data_type == DT_FLOAT ||
                        symbol_table[current_quaternion.result].data_type == DT_DOUBLE) {
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
                        "addl\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) + ", " +
                        GPRStr[opr1_reg_idx][2]);
            }
            else if (variable_reg_map[current_quaternion.opr2] == -1) {
                // opr2 in memory
                target_text.push_back(
                        "addl\t" + to_string(symbol_table[current_quaternion.opr2].memory_offset) + sp_str +
                        GPRStr[opr1_reg_idx][2]);
            } else {
                // opr2 in register
                target_text.push_back("addl%\t" + GPRStr[variable_reg_map[current_quaternion.opr2]][2] + ", %" +
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
                        "addss\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.float_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else if (variable_reg_map[current_quaternion.opr2] == -1) {
                // opr2 in memory
                target_text.push_back(
                        "addss\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.float_value) + "" +
                        XMMRegStr[opr1_reg_idx]);
            } else {
                // opr2 in register
                target_text.push_back("addss%\t" + XMMRegStr[variable_reg_map[current_quaternion.opr2]] + ", %" +
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
                        "addsd\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.double_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else if (variable_reg_map[current_quaternion.opr2] == -1) {
                // opr2 in memory
                target_text.push_back(
                        "addsd\t" + to_string(symbol_table[current_quaternion.opr2].memory_offset) + sp_str +
                        XMMRegStr[opr1_reg_idx]);
            } else {
                // opr2 in register
                target_text.push_back("addsd%\t" + XMMRegStr[variable_reg_map[current_quaternion.opr2]] + ", %" +
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
                        "subl\t$" + to_string(symbol_table[current_quaternion.opr2].value.int_value) + ", " +
                        GPRStr[opr1_reg_idx][2]);
            }
            else if (variable_reg_map[current_quaternion.opr2] == -1) {
                // opr2 in memory
                target_text.push_back(
                        "subl\t" + to_string(symbol_table[current_quaternion.opr2].memory_offset) + sp_str +
                        GPRStr[opr1_reg_idx][2]);
            } else {
                // opr2 in register
                target_text.push_back("subl%\t" + GPRStr[variable_reg_map[current_quaternion.opr2]][2] + ", %" +
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
                        "subss\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.float_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else if (variable_reg_map[current_quaternion.opr2] == -1) {
                // opr2 in memory
                target_text.push_back(
                        "subss\t" + to_string(symbol_table[current_quaternion.opr2].memory_offset) + sp_str +
                        XMMRegStr[opr1_reg_idx]);
            } else {
                // opr2 in register
                target_text.push_back("subss%\t" + XMMRegStr[variable_reg_map[current_quaternion.opr2]] + ", %" +
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
                        "subsd\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.double_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else if (variable_reg_map[current_quaternion.opr2] == -1) {
                // opr2 in memory
                target_text.push_back(
                        "subsd\t" + to_string(symbol_table[current_quaternion.opr2].memory_offset) + sp_str +
                        XMMRegStr[opr1_reg_idx]);
            } else {
                // opr2 in register
                target_text.push_back("subsd%\t" + XMMRegStr[variable_reg_map[current_quaternion.opr2]] + ", %" +
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
            else if (variable_reg_map[current_quaternion.opr2] == -1) {
                // opr2 in memory
                target_text.push_back(
                        "imul\t" + to_string(symbol_table[current_quaternion.opr2].memory_offset) + sp_str +
                        GPRStr[opr1_reg_idx][2]);
            } else {
                // opr2 in register
                target_text.push_back("imul%\t" + GPRStr[variable_reg_map[current_quaternion.opr2]][2] + ", %" +
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
                        "mulss\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.float_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else if (variable_reg_map[current_quaternion.opr2] == -1) {
                // opr2 in memory
                target_text.push_back(
                        "mulss\t" + to_string(symbol_table[current_quaternion.opr2].memory_offset) + sp_str +
                        XMMRegStr[opr1_reg_idx]);
            } else {
                // opr2 in register
                target_text.push_back("mulss%\t" + XMMRegStr[variable_reg_map[current_quaternion.opr2]] + ", %" +
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
                        "mulsd\t.RO_" + to_string(symbol_table[current_quaternion.opr1].value.double_value) + ip_str +
                        XMMRegStr[opr1_reg_idx]);
            }
            else if (variable_reg_map[current_quaternion.opr2] == -1) {
                // opr2 in memory
                target_text.push_back(
                        "mulsd\t" + to_string(symbol_table[current_quaternion.opr2].memory_offset) + sp_str +
                        XMMRegStr[opr1_reg_idx]);
            } else {
                // opr2 in register
                target_text.push_back("mulsd%\t" + XMMRegStr[variable_reg_map[current_quaternion.opr2]] + ", %" +
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
        case OP_JEQ:
            break;
        case OP_JNZ:
            break;
        case OP_JMP:
            break;
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
        case OP_GREATER_INT:
            break;
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
        case OP_LESS_INT:
            break;
        case OP_LESS_FLOAT:
            break;
        case OP_LESS_DOUBLE:
            break;
        case OP_LESS_EQUAL_INT:
            break;
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
        case OP_PAR:
            break;
        case OP_CALL:
            break;
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
        case OP_RETURN:
            break;
        case OP_NOP:
            break;
        case OP_INVALID:
            break;
    }
//    throw "Not Implemented!";
}

void generate_target_text_asm(BaseBlock &base_block, vector<string> target_text, int target_arch) {
    variable_reg_map = new int[symbol_table.size()];
    memset(variable_reg_map, -1, symbol_table.size() * sizeof(int));

    variable_next_use = new int[symbol_table.size()];
    memset(variable_next_use, -1, symbol_table.size() * sizeof(int));

    generate_active_info_table(base_block);
    target_text.emplace_back("\n.text:");

    gen_entry_code(base_block, target_text, target_arch);

    for (int i = base_block.start_index; i <= base_block.end_index; ++i) {
        generate_quaternion_text(i, target_text, target_arch);
    }

    delete[] variable_next_use;
    delete[] variable_reg_map;
}

// return string contains ".data"
void generate_target_data_asm(vector<string> target_data) {
    target_data.emplace_back("\t.data");

    for (SymbolEntry& symbol : symbol_table) {
        if (symbol.is_const) {
            if (symbol.data_type == DT_FLOAT) {
                target_data.push_back(".RO_" + to_string(symbol.value.float_value) + ":\t.float\t" + to_string(symbol.value.float_value));
            }
            else if (symbol.data_type == DT_DOUBLE) {
                target_data.push_back(".RO_" + to_string(symbol.value.double_value) + ":\t.double\t" + to_string(symbol.value.double_value));
            }
        }
    }
}

// return string contains ".bss"
void generate_target_bss_asm(vector<string> target_bss) {
    target_bss.emplace_back("\t.bss");

    for (SymbolEntry& symbol : symbol_table) {
        if (symbol.is_array && !symbol.is_temp) {
            target_bss.push_back(symbol.content + "\t.fill\t" + to_string(symbol.memory_size));
        }
    }
}

void sp_sub_back_patch(vector<string> &target_text, int target_arch) {
    int* function_lowest_mem_offset = new int[Function::function_table.size()];
    int target_arch_offset = target_arch == TARGET_ARCH_X64 ? -8 : -4;
    for (int i = 0; i < Function::function_table.size(); ++i) {
        function_lowest_mem_offset[i] = target_arch_offset;
    }

    for (SymbolEntry& symbol : symbol_table) {
        if (function_lowest_mem_offset[symbol.function_index] > (symbol.memory_offset - symbol.memory_size)) {
            function_lowest_mem_offset[symbol.function_index] = symbol.memory_offset - symbol.memory_size;
        }
    }

    if (target_arch == TARGET_ARCH_X64) {
        for (int i = 0; i < function_entrance_sp_sub_points.size(); ++i) {
            target_text[function_entrance_sp_sub_points[i]] = "\taddq\t$" + to_string(function_lowest_mem_offset[i]) + ", %rsp";
        }
    }
    else {
        for (int i = 0; i < function_entrance_sp_sub_points.size(); ++i) {
            target_text[function_entrance_sp_sub_points[i]] = "\taddl\t$" + to_string(function_lowest_mem_offset[i]) + ", %esp";
        }
    }


    delete[] function_lowest_mem_offset;
}

void generate_target_asm(string &target_string_str, int target_arch) {
    init_symbol_table_offset(target_arch);
    split_base_blocks(optimized_sequence);
    calculate_active_symbol_sets();

    vector<string> text_seg_str;
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
