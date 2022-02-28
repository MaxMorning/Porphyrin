//
// Project: Porphyrin
// File Name: Syntax.h
// Author: Morning
// Description:
//
// Create Date: 2021/11/2
//

#ifndef PORPHYRIN_SYNTAX_H
#define PORPHYRIN_SYNTAX_H

#include <string>
#include <vector>
#include "Class/LR1item.h"
#include "Class/Character.h"
#include "Lexical.h"
#include "Class/LR1Table.h"
#include "Class/Node.h"

using namespace std;

//vector<Character> get_first_set(LR1item character);

// load chars & productions from disk files
void load_grammar_from_disk(const string& path);

// generate LR(1) analysis table
// get first set
void get_first();
void print_first_set();

// get closure
void get_closure();
void get_action();
void print_action_goto_table();

void convert_to_LR1_table();

bool generate_lr1_table();

// syntax analysis

bool syntax_analysis(string processed_code, Node*& root);

#endif //PORPHYRIN_SYNTAX_H
