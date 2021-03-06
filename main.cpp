//
// Project: Porphyrin
// File Name: main.cpp
// Author: Morning
// Description:
//      main code of Porphyrin Compiler
// Create Date: 2021/10/3
//

#include <iostream>
#include <sstream>
#include <cstdio>
#include "Include/PreProcess.h"
#include "Include/Lexical.h"
#include "Include/Syntax.h"
#include "Include/Semantic.h"
#include "Include/Optimize.h"
#include "Class/Diagnose.h"
#include "Include/x86Assemble.h"

#define LOAD_BUFFER_SIZE 4096

using namespace std;

int main(int argc, char* argv[])
{
    char* source_code_path = nullptr;

    string grammar_path = "../Grammar/G.txt";
    string target_path = "target.s";
    bool enable_print = false;
    bool gen_win_style_asm = false;

    // process compile options
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '\0') {
                cerr << "Invalid Compile Option." << endl;
                return -1;
            }
            switch (argv[i][1]) {
                case 'o':
                    if (i + 1 >= argc) {
                        cerr << "No matching parameter" << endl;
                        return -1;
                    }
                    // set target file
                    target_path = argv[i + 1];
                    ++i;
                    break;

                case 'p':
                    enable_print = true;
                    break;

                case 't':
                    if (argv[i][2] == 'w' && argv[i][3] == 'i' && argv[i][4] == 'n' && argv[i][5] == '\0') {
                        gen_win_style_asm = true;
                    }
                    else {
                        cerr << "Invalid Target Platform." << endl;
                    }
                    break;

                case 'v':
                    cout << "G-- 1.0.1" << endl;
                    break;

                default:
                    cerr << "Invalid Compile Option" << endl;
                    return -1;
            }
        }
        else {
            source_code_path = argv[i];
        }
    }

    // Open src code file
    ifstream src_file_in(source_code_path);
    if (!src_file_in.is_open()) {
        cerr << "FILE NOT FOUND" << endl;
        return -1;
    }

    // Load buffer
    vector<Lexicon> lex_result_vector;

    stringstream load_stream;
    load_stream << src_file_in.rdbuf();
    string load_buffer(load_stream.str());

    load_buffer.push_back('\n');

    src_file_in.close();

    // Process
    Diagnose::setSource(load_buffer);
    string processed_code_bytes = pre_process(load_buffer);
    Diagnose::setProcessed(processed_code_bytes);

//    // Lexical Analysis
//    lex_result_vector = lexical_analysis(processed_code_bytes);
//
//    if (!lex_result_vector.empty() && lex_result_vector.back().lex_type != END_SIGNAL) {
//        cerr << "LEX CAN'T RECOGNIZE" << endl;
//        return -1;
//    }

#ifdef DEBUG
//    lex_result_vector.clear();
//
//    lex_result_vector = lexical_analysis("a a 0 a 0 #");
    print_lexical_result(lex_result_vector);
#endif
    // syntax analysis
    TerminalChar::init_terminal_char_set(); // the function might be called in prev codes

//    load_grammar_from_disk(grammar_path);
    load_grammar_built_in();
#ifdef DEBUG
    cout << "Grammar load done." << endl;
#endif

    if (!generate_lr1_table()) {
        cerr << "INVALID_GRAMMAR" << endl;
        return -1;
    }

#ifdef DEBUG

    print_first_set();

    print_action_goto_table();

#endif

    Node* root = nullptr; // initial with anything
    if (!syntax_analysis(processed_code_bytes, root)) {
        cerr << "CODE_WRONG" << endl;
        return -1;
    }


    // visualize part
    if (enable_print) {
        root->setLayout(0);
        root->printTreeInfoIntoTxt("Layout.txt");
    }

#ifdef DEBUG
    root->print_tree();
#endif

    // Semantic part
    semantic_analysis_post(enable_print);

#ifdef SEMANTIC_DEBUG
    print_quaternion_sequence(quaternion_sequence);
#endif

//    write_semantic_result();

    // optimize
    optimize_IR(quaternion_sequence);


#ifdef OPTIMIZE_DEBUG
    print_optimize_sequence();
#endif

    if (enable_print) {
        write_optimize_result();
    }
    // generate target code (x86 asm)
    string target_string_str;
    generate_target_asm(target_string_str, gen_win_style_asm);

    ofstream fout(target_path);
    write_target_code(target_string_str, fout);

    cout << "Acc!" << endl;

    // free memory
    root->free_memory();

    for (LR1item* lr_1_item_ptr : LR1item::all_lr_1_item_set) {
        delete lr_1_item_ptr;
    }

    for (int i = 0; i < TerminalChar::all_terminal_chars.size(); ++i) {
        delete[] LR1Table::transfer_table[i];
        delete[] LR1Table::reduction_result_table[i];
    }

    for (int i = 0; i < Nonterminal::all_nonterminal_chars.size(); ++i) {
        delete[] LR1Table::nonterminal_transfer_table[i];
    }

    delete[] LR1Table::transfer_table;
    delete[] LR1Table::reduction_result_table;
    delete[] LR1Table::nonterminal_transfer_table;

    return 0;
}