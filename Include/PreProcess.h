//
// Project: Porphyrin
// File Name: PreProcess.h
// Author: Morning
// Description:
//
// Create Date: 2021/10/3
//

#ifndef PORPHYRIN_PREPROCESS_H
#define PORPHYRIN_PREPROCESS_H
#include <cstring>
#include <string>
#include <cstdio>
#include <unordered_map>
#include <vector>

using namespace std;

static unordered_map<string, string> define_replace_set;

// Pre Process the code (macro etc.)
// cut code until last \n & add last #(END_SIGNAL)
// remove comments will be processed in Lexical Analysis
// assume there is no # in processed_code
string pre_process(string source_code);

string remove_comment(string source_code);

string define_replace(string removed_code);

string process_include(string source_code);

string load_file_all(const string& path);
#endif //PORPHYRIN_PREPROCESS_H
