//
// Project: Porphyrin
// File Name: Syntax.cpp.c
// Author: Morning
// Description:
//
// Create Date: 2021/11/2
//

#include "Include/Syntax.h"
#include "Include/Lexical.h"
#include <vector>
#include "Class/Character.h"
#include "Class/Node.h"
#include "Class/Closure.h"
#include "Class/LR1item.h"
#include "Class/Diagnose.h"
#include <stack>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <assert.h>
#include "Include/Semantic.h"

using namespace std;

void load_grammar_from_disk(const string& path)
{
    ifstream file_in(path);
    if (!file_in.is_open()) {
        cerr << "FILE_OPEN_FAILED" << endl;
        return;
    }

    unordered_map<string, int> nonterminal_mapping;
    string load_line;
    int index = 0;

    while (getline(file_in, load_line)) {
        if (load_line.length() <= 1) {
            // empty line
            continue;
        }

        while (load_line.back() == '\r' || load_line.back() == '\n') {
            load_line.pop_back();
        }

        stringstream str_stream;
        str_stream << load_line;

        string char_name;

        str_stream >> char_name;
        // assume first character is nonterminal
        int target_nonterminal_index;
        auto result_iterator = nonterminal_mapping.find(char_name);
        if (result_iterator == nonterminal_mapping.end()) {
            // new nonterminal
            nonterminal_mapping[char_name] = index;

            Nonterminal::all_nonterminal_chars.emplace_back(char_name, index);
            target_nonterminal_index = index;
            ++index;
        }
        else {
            target_nonterminal_index = result_iterator->second;
        }

        vector<Character> current_production;

        str_stream >> char_name; // remove @

        while (!str_stream.eof()) {
            str_stream >> char_name;

            if (char_name[0] < 'A' || char_name[0] > 'Z') {
                // terminal char
                // use lexical analysis to convert content('+') to lex_type(ADD)
                vector<Lexicon> terminal_lex;
                if (char_name == "num") {
                    terminal_lex = lexical_analysis("0");
                }
                else {
                    terminal_lex = lexical_analysis(char_name);
                }

                // the size of terminal_lex should be 1.
                current_production.push_back(TerminalChar::all_terminal_chars[terminal_lex[0].convert_to_terminal_index()]);
                continue;
            }
            result_iterator = nonterminal_mapping.find(char_name);
            int nonterminal_in_production_idx;
            if (result_iterator == nonterminal_mapping.end()) {
                nonterminal_mapping[char_name] = index;

                Nonterminal::all_nonterminal_chars.emplace_back(char_name, index);
                nonterminal_in_production_idx = index;
                ++index;
            }
            else {
                nonterminal_in_production_idx = result_iterator->second;
            }
            current_production.push_back(Nonterminal::all_nonterminal_chars[nonterminal_in_production_idx]);
        }

        // set epsilon in first set
        if (current_production.size() == 1 && current_production[0].is_terminal && current_production[0].index == TerminalChar::get_index("EPSILON")) {
            Nonterminal::all_nonterminal_chars[target_nonterminal_index].epsilon_in_firstset = true;
            current_production.clear();
        }

        Nonterminal::all_nonterminal_chars[target_nonterminal_index].productions.push_back(current_production);
    }

    file_in.close();

#ifdef DEBUG
    cout << "Productions : " << endl;
    for (Nonterminal& nonterminal : Nonterminal::all_nonterminal_chars) {
        for (vector<Character>& production : nonterminal.productions) {
            cout << nonterminal.token << " -> ";

            for (Character& character : production) {
                cout << character.token << ' ';
            }
            cout << endl;
        }
    }
    cout << endl;
#endif
}

void get_first()
{
    bool done = true;       // if first set update , check again
    int pos;                // pos = 0 , if [pos] = epsilon pos ++
    bool isempty;

    while(done) {
        done = false;
        for (Nonterminal& nonterminal : Nonterminal::all_nonterminal_chars) {
            for (vector<Character>& production : nonterminal.productions) {
                pos = 0;
                isempty = true;

                while (isempty && pos < production.size()) {
                    isempty = false;

                    // like A -> BC
                    if (!production[pos].is_terminal) {
                        bool have_change = nonterminal.add_first(Nonterminal::all_nonterminal_chars[production[pos].index]);
                        if (have_change) {
                            done = true;
                        }

                        // like B -> epsilon
                        if (production[pos].epsilon_in_firstset) { // @ for epsilon
                            isempty = true;
                            ++pos;             // first set of C join first set of A
                        }
                    }
                    // like A -> aB
                    else if (!nonterminal.find_in_firstset(production[pos])) {
                        done = true;
                        nonterminal.first_set.push_back(production[pos]);
                    }
                }
            }


        }
    }
}

void print_first_set()
{
    ofstream first_set_file("first_set.txt");
    for (Nonterminal& nonterminal : Nonterminal::all_nonterminal_chars) {
        first_set_file << "first(" << nonterminal.token << ") = {";
        for (Character& character : nonterminal.first_set) {
            first_set_file << character.token << ' ';
        }
        first_set_file << '}' << endl;
    }

    first_set_file.close();
}

int is_closure_exist() {
    for(int i = 0; i < Closure::all_closure_set.size() - 1; i ++) {
        if(Closure::all_closure_set[i] == Closure::all_closure_set.back()) {
            return i;
        }
    }
    return -1;
}

bool check_is_epsilon_production(int nonterminal_char_index, int production_index)
{
    auto& production = Nonterminal::all_nonterminal_chars[nonterminal_char_index].productions[production_index];
    if (production.empty()) {
        return true;
    }
    else {
        return false;
    }
}

void get_closure() {

    Closure tmp_closure;

    // S‘ -> [·S, #]
    // assume s' is the first nonterminal
    // assume # is the last terminal
    LR1item::all_lr_1_item_set.push_back(new LR1item(0, 0, 0, TerminalChar::all_terminal_chars.size() - 1));

    int closure_idx = 0;
    Closure::all_closure_set.push_back(tmp_closure);
    Closure::all_closure_set[closure_idx].index = closure_idx;
    Closure::all_closure_set[closure_idx].add_item(0);
    Closure::all_closure_set[closure_idx].update();

#ifdef DEBUG
    Closure::all_closure_set[closure_idx].print();
#endif

    ++closure_idx;


    vector<int> new_item;
    for(int i = 0; i < Closure::all_closure_set.size(); ++i) {
        // Terminal char part
        for (TerminalChar& terminal_char : TerminalChar::all_terminal_chars) {
            new_item.clear();

            for (int k = 0; k < Closure::all_closure_set[i].items_number.size(); ++k) {
                LR1item* l_now = LR1item::all_lr_1_item_set[Closure::all_closure_set[i].items_number[k]];
                // A -> [B·c, a]
                // C -> [D·c, b]
                if (l_now->dot_position < Nonterminal::all_nonterminal_chars[l_now->index].productions[l_now->production_index].size() && Nonterminal::all_nonterminal_chars[l_now->index].productions[l_now->production_index][l_now->dot_position].is_terminal && Nonterminal::all_nonterminal_chars[l_now->index].productions[l_now->production_index][l_now->dot_position].index == terminal_char.index) {
                    LR1item tmp_LR1item(l_now->index, l_now->production_index, l_now->dot_position + 1, l_now->foresee_char_index);
                    if (check_is_epsilon_production(l_now->index, l_now->production_index)) {
                        tmp_LR1item.dot_position = 0;
                        tmp_LR1item.is_reduction = true;
                    }
                    else {
                        tmp_LR1item.is_reduction = (l_now->dot_position == Nonterminal::all_nonterminal_chars[l_now->index].productions[l_now->production_index].size() - 1);
                    }

                    int position = LR1item::is_LR1item_exist(tmp_LR1item);
                    if (position == -1) {
                        LR1item::all_lr_1_item_set.push_back(new LR1item(tmp_LR1item));
                        position = (int)LR1item::all_lr_1_item_set.size() - 1;
                    }
                    // A -> [Bc·, a]
                    // C -> [Dc·, b]
                    new_item.push_back(position);
                }
            }

            if (!new_item.empty()) {
                Closure::all_closure_set.push_back(tmp_closure);
                Closure::all_closure_set.back().index = (int)Closure::all_closure_set.size() - 1;
                for (int k = 0; k < new_item.size(); ++k) {
                    Closure::all_closure_set.back().add_item(new_item[k]);
                }
                Closure::all_closure_set.back().update();

                int next_state = is_closure_exist();
                if (next_state != -1) {                     // itemset have existed
                    Closure::all_closure_set.pop_back();                // delete new itemset
                }
                else {
#ifdef DEBUG
                    Closure::all_closure_set.back().print();
#endif
                    next_state = (int)Closure::all_closure_set.size() - 1;
                }

                // build transfer relationship
                Closure::all_closure_set[i].trans[terminal_char.index].transfer_type = 1;    // move in
                Closure::all_closure_set[i].trans[terminal_char.index].transfer_next_status = next_state;
            }
        }

        // nonterminal char part
        for (Nonterminal& nonterminal : Nonterminal::all_nonterminal_chars) {
            new_item.clear();

            for (int k = 0; k < Closure::all_closure_set[i].items_number.size(); ++k) {
                LR1item* l_now = LR1item::all_lr_1_item_set[Closure::all_closure_set[i].items_number[k]];
                // A -> [B·c, a]
                // C -> [D·c, b]
                if (l_now->dot_position < Nonterminal::all_nonterminal_chars[l_now->index].productions[l_now->production_index].size() && !Nonterminal::all_nonterminal_chars[l_now->index].productions[l_now->production_index][l_now->dot_position].is_terminal && Nonterminal::all_nonterminal_chars[l_now->index].productions[l_now->production_index][l_now->dot_position].index == nonterminal.index) {
                    LR1item tmp_LR1item(l_now->index, l_now->production_index, l_now->dot_position + 1, l_now->foresee_char_index);

                    if (check_is_epsilon_production(l_now->index, l_now->production_index)) {
                        tmp_LR1item.dot_position = 0;
                        tmp_LR1item.is_reduction = true;
                    }
                    else {
                        tmp_LR1item.is_reduction = (l_now->dot_position == Nonterminal::all_nonterminal_chars[l_now->index].productions[l_now->production_index].size() - 1);
                    }
                    int position = LR1item::is_LR1item_exist(tmp_LR1item);
                    if (position == -1) {
                        LR1item::all_lr_1_item_set.push_back(new LR1item(tmp_LR1item));
                        position = (int)LR1item::all_lr_1_item_set.size() - 1;
                    }
                    // A -> [Bc·, a]
                    // C -> [Dc·, b]
                    new_item.push_back(position);
                }
            }

            if (!new_item.empty()) {
                Closure::all_closure_set.push_back(tmp_closure);
                Closure::all_closure_set.back().index = (int)Closure::all_closure_set.size() - 1;
                for (int k = 0; k < new_item.size(); ++k) {
                    Closure::all_closure_set.back().add_item(new_item[k]);
                }
                Closure::all_closure_set.back().update();

                int next_state = is_closure_exist();
                if (next_state != -1) {                     // itemset have existed
                    Closure::all_closure_set.pop_back();                // delete new itemset
                }
                else {
#ifdef DEBUG
                    Closure::all_closure_set.back().print();
#endif
                    next_state = (int)Closure::all_closure_set.size() - 1;
                }

                // build transfer relationship
                Closure::all_closure_set[i].trans[nonterminal.index + TerminalChar::all_terminal_chars.size()].transfer_type = 1;    // move in
                Closure::all_closure_set[i].trans[nonterminal.index + TerminalChar::all_terminal_chars.size()].transfer_next_status = next_state;
            }
        }
    }


#ifdef DEBUG
    cout << "Trans: " << endl;
    for (Closure& closure : Closure::all_closure_set) {
        cout << closure.index << "\t" << closure.trans << "\t";

        for (int i = 0; i < TerminalChar::all_terminal_chars.size() + Nonterminal::all_nonterminal_chars.size(); ++i) {
            cout << closure.trans[i].transfer_next_status << ' ';
        }
        cout << endl;
    }
#endif
}

void get_action()
{
    for (int i = 0; i < Closure::all_closure_set.size(); ++i) {
        for (int j = 0; j < Closure::all_closure_set[i].items_number.size(); ++j) {
            LR1item* l_now = LR1item::all_lr_1_item_set[Closure::all_closure_set[i].items_number[j]];
            // reduction
            if (Nonterminal::all_nonterminal_chars[l_now->index].token == "B") {
                cout << "B : " << l_now->is_reduction << endl;
            }
            if(l_now->is_reduction) {
                Closure::all_closure_set[i].trans[l_now->foresee_char_index].transfer_type = 0;
                Closure::all_closure_set[i].trans[l_now->foresee_char_index].transfer_reduction_index = l_now->production_index;
                Closure::all_closure_set[i].trans[l_now->foresee_char_index].transfer_nonterminal_target = l_now->index;
                // No. of project for reduction
            }
        }
    }
}

void print_action_goto_table() {
    int width = 30;
    cout << setw(width) << " ";
    for (TerminalChar& terminalChar : TerminalChar::all_terminal_chars) {
        cout << setw(width) << terminalChar.token;
    }
    for (Nonterminal& nonterminal : Nonterminal::all_nonterminal_chars) {
        cout << setw(width) << nonterminal.token;
    }
    cout << endl;

    for(int i = 0; i < Closure::all_closure_set.size(); i ++) {
        cout << setw(width) << i;
        for (int j = 0; j < TerminalChar::all_terminal_chars.size(); ++j) {
            Transfer tmp_transfer = Closure::all_closure_set[i].trans[j];
            if(tmp_transfer.transfer_type == 1) {
                cout << setw(width) << "s" + to_string(tmp_transfer.transfer_next_status);
            }
            else if(tmp_transfer.transfer_type == 0) {
                cout << setw(width) << "r" + to_string(tmp_transfer.transfer_reduction_index);
            }
            else {
                cout << setw(width) << "-1";
            }
        }

        for (int j = 0; j < Nonterminal::all_nonterminal_chars.size(); ++j) {
            Transfer tmp_transfer = Closure::all_closure_set[i].trans[j + TerminalChar::all_terminal_chars.size()];
            if(tmp_transfer.transfer_type == 1) {
                cout << setw(width) << "s" + to_string(tmp_transfer.transfer_next_status);
            }
            else if(tmp_transfer.transfer_type == 0) {
                cout << setw(width) << "r" + to_string(tmp_transfer.transfer_reduction_index);
            }
            else {
                cout << setw(width) << "-1";
            }
        }
        cout << endl;
    }
}

void convert_to_LR1_table()
{

    LR1Table::transfer_table = new int*[Closure::all_closure_set.size()];
    LR1Table::reduction_result_table = new int*[Closure::all_closure_set.size()];
    LR1Table::nonterminal_transfer_table = new int*[Closure::all_closure_set.size()];


    for (int i = 0; i < Closure::all_closure_set.size(); ++i) {
        LR1Table::transfer_table[i] = new int[TerminalChar::all_terminal_chars.size()];
        LR1Table::reduction_result_table[i] = new int[TerminalChar::all_terminal_chars.size()];
        for (int j = 0; j < TerminalChar::all_terminal_chars.size(); ++j) {
            Transfer& tmp_transfer = Closure::all_closure_set[i].trans[j];
            if(tmp_transfer.transfer_type == 1) {
                LR1Table::transfer_table[i][j] = tmp_transfer.transfer_next_status;
                LR1Table::reduction_result_table[i][j] = -1;
            }
            else if(tmp_transfer.transfer_type == 0) {
                LR1Table::transfer_table[i][j] = tmp_transfer.transfer_reduction_index;
                LR1Table::reduction_result_table[i][j] = tmp_transfer.transfer_nonterminal_target;
            }
            else {
                LR1Table::transfer_table[i][j] = -1;
                LR1Table::reduction_result_table[i][j] = -1;
            }
        }


        LR1Table::nonterminal_transfer_table[i] = new int[Nonterminal::all_nonterminal_chars.size()];

        for (int j = 0; j < Nonterminal::all_nonterminal_chars.size(); ++j) {
            Transfer tmp_transfer = Closure::all_closure_set[i].trans[j + TerminalChar::all_terminal_chars.size()];
            if(tmp_transfer.transfer_type == 1) {
                LR1Table::nonterminal_transfer_table[i][j] = tmp_transfer.transfer_next_status;
            }
            else if(tmp_transfer.transfer_type == 0) {
                LR1Table::nonterminal_transfer_table[i][j] = -1;
            }
            else {
                LR1Table::nonterminal_transfer_table[i][j] = -1;
            }
        }

    }

#ifdef DEBUG
    cout << "Transfer table: " << endl;

    for (int i = 0; i < Closure::all_closure_set.size(); ++i) {
        for (int j = 0; j < TerminalChar::all_terminal_chars.size(); ++j) {
            cout << setw(5) << LR1Table::transfer_table[i][j];
        }
        cout << endl;
    }

    cout << endl;

    cout << "reduction result table: " << endl;

    for (int i = 0; i < Closure::all_closure_set.size(); ++i) {
        for (int j = 0; j < TerminalChar::all_terminal_chars.size(); ++j) {
            cout << setw(5) << LR1Table::reduction_result_table[i][j];
        }
        cout << endl;
    }

    cout << endl;

    cout << "non terminal transfer table: " << endl;

    for (int i = 0; i < Closure::all_closure_set.size(); ++i) {
        for (int j = 0; j < Nonterminal::all_nonterminal_chars.size(); ++j) {
            cout << setw(5) << LR1Table::nonterminal_transfer_table[i][j];
        }
        cout << endl;
    }
    cout << endl;
#endif
}

bool generate_lr1_table()
{
    get_first();

    get_closure();

    get_action();

    convert_to_LR1_table();

    return true;
}

bool syntax_analysis(vector<Lexicon> lex_ana_result, Node*& root)
{
    // assume the entrance of DFA is Closure 0, of which index is 0
    int current_state = 0;
    int ana_index = 0;

    int reduction_idx;
    Nonterminal* reduction_result_ptr;

    vector<Character> analysis_stack;
    vector<int> state_stack;
    vector<int> semantic_value_stack;

    state_stack.push_back(0);

    // root of parse tree
    // assume S' is the first nonterminal
    vector<Node*> node_stack;

    while (current_state >= 0 && ana_index < lex_ana_result.size()) {
        // TODO NOTICE
        // lex_ana_result[ana_index].lex_type is only for debugging
        // for real use, it should be lex_ana_result[ana_index].convert_to_terminal_index
        current_state = LR1Table::analysis(state_stack.back(), TerminalChar::all_terminal_chars[lex_ana_result[ana_index].convert_to_terminal_index()],
                                           reduction_idx, reduction_result_ptr);
        if (current_state == -2) {
            break;
        }

        if (reduction_idx < 0) {
            // push
            // TODO NOTICE
            // lex_ana_result[ana_index].lex_type is only for debugging
            // for real use, it should be lex_ana_result[ana_index].convert_to_terminal_index
            analysis_stack.push_back(TerminalChar::all_terminal_chars[lex_ana_result[ana_index].convert_to_terminal_index()]);
            state_stack.push_back(current_state);
            node_stack.push_back(new Node(lex_ana_result[ana_index]));
            semantic_value_stack.push_back(-1);
            ++ana_index;
        }
        else {
            // reduction
            unsigned long reduction_cnt = reduction_result_ptr->productions[reduction_idx].size();
            Node* temp_parent_node_ptr = new Node(*reduction_result_ptr, reduction_idx);
            for (unsigned long i = 0; i < reduction_cnt; ++i) {
                analysis_stack.pop_back();
                state_stack.pop_back();
                temp_parent_node_ptr->child_nodes_ptr.push_back(node_stack.back());
                node_stack.pop_back();
            }

            if (reduction_cnt == 0) {
                // reduction of an epsilon production
                temp_parent_node_ptr->child_nodes_ptr.push_back(new Node(Lexicon("$", 1, temp_parent_node_ptr->offset)));
            }
            // reverse child nodes
            reverse(temp_parent_node_ptr->child_nodes_ptr.begin(), temp_parent_node_ptr->child_nodes_ptr.end());
            node_stack.push_back(temp_parent_node_ptr);

            // set offset
            assert(!temp_parent_node_ptr->child_nodes_ptr.empty());
            temp_parent_node_ptr->offset = temp_parent_node_ptr->child_nodes_ptr[0]->offset;

            analysis_stack.push_back(*reduction_result_ptr);

            current_state = LR1Table::nonterminal_transfer_table[state_stack.back()][reduction_result_ptr->index];
            state_stack.push_back(current_state);


            // semantic action
            if (temp_parent_node_ptr->child_nodes_ptr.size() == 1 && temp_parent_node_ptr->child_nodes_ptr[0]->is_terminal) {
                temp_parent_node_ptr->quad = quaternion_sequence.size();
            }
            else {
                // find left child non terminal
                temp_parent_node_ptr->quad = -1;
                for (Node* child_ptr : temp_parent_node_ptr->child_nodes_ptr) {
                    if (!child_ptr->is_terminal) {
                        temp_parent_node_ptr->quad = child_ptr->quad;
                        break;
                    }
                }
                if (temp_parent_node_ptr->quad == -1) {
                    temp_parent_node_ptr->quad = quaternion_sequence.size();
                }
            }

            int ret_value = temp_parent_node_ptr->semantic_action_one_pass(&(semantic_value_stack[semantic_value_stack.size() - temp_parent_node_ptr->child_nodes_ptr.size()]));
            for (int i = 0; i < temp_parent_node_ptr->child_nodes_ptr.size(); ++i) {
                assert(!semantic_value_stack.empty());
                semantic_value_stack.pop_back();
            }
            semantic_value_stack.push_back(ret_value);

#ifdef DEBUG
            cout << reduction_result_ptr->token << " <- ";
            for (Character& c : reduction_result_ptr->productions[reduction_idx]) {
                cout << c.token << ' ';
            }

            cout << endl;
#endif

            if (analysis_stack.size() == 1 && analysis_stack[0].index == 0 && !analysis_stack[0].is_terminal
                && ana_index == lex_ana_result.size() - 1 && lex_ana_result[ana_index].lex_type == END_SIGNAL) {
                current_state = -2;
                break;
            }
        }

#ifdef DEBUG
        for (int state : state_stack) {
            cout << state << ' ';
        }

        cout << "\t\t\t\t\tStack: ";

        for (Character& c : analysis_stack) {
            cout << c.token << ' ';
        }

        cout << "\t\t\t\t\tRemain: ";

        for (int i = ana_index; i < lex_ana_result.size(); ++i) {
            cout << lex_ana_result[i].lex_content << ' ';
        }

        cout << "\t\t\t\tNode: ";
        for (Node* node : node_stack) {
            cout << node->token << ' ';
        }
        cout << endl;
#endif
    }

    if (current_state == -2) {
        // analysis success
        root = node_stack[0];

        // Build parent relation
        root->build_parent();
        return true;
    }
    else {
        assert(!node_stack.empty());
        Diagnose::printError(node_stack.back()->offset, "Syntax Error.");
        return false;
    }
}