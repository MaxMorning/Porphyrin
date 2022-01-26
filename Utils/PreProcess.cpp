//
// Project: Porphyrin
// File Name: PreProcess.cpp
// Author: Morning
// Description:
//
// Create Date: 2021/10/3
//

#include "Include/PreProcess.h"
#include <iostream>
#include <utility>

#define LOAD_BUFFER_SIZE 4096

string pre_process(string source_code)
{
    string processed_code(std::move(source_code));

    processed_code = process_include(processed_code);
    processed_code = remove_comment(processed_code);
    processed_code = define_replace(processed_code);
    processed_code.push_back('#');

    return processed_code;
}

string remove_comment(string source_code)
{
    string removed_code;
    size_t src_code_length = source_code.length();

    int processing_idx = 0;
    while (processing_idx < src_code_length - 1) {
        if (source_code[processing_idx] == '/' && source_code[processing_idx + 1] == '/') {
            // line comment
            processing_idx += 2;
            while (processing_idx < src_code_length && source_code[processing_idx] != '\n') {
                ++processing_idx;
            }
            removed_code.push_back('\n'); // keep enter
        }
        else if (source_code[processing_idx] == '/' && source_code[processing_idx + 1] == '*') {
            // block comment
            processing_idx += 2;
            int enter_cnt = 0;
            while (processing_idx < src_code_length - 1 && (source_code[processing_idx] != '*' || source_code[processing_idx + 1] != '/')) {
                if (source_code[processing_idx] == '\n') {
                    ++enter_cnt;
                }
                ++processing_idx;
            }
            if (processing_idx == src_code_length - 1) {
                // todo throw error: BLOCK_COMMENT_NOT_CLOSE
            }
            for (int i = 0; i < enter_cnt; ++i) {
                removed_code.push_back('\n');
            }
            ++processing_idx;
        }
        else {
            removed_code.push_back(source_code[processing_idx]);
        }

        ++processing_idx;
    }

    if (removed_code[removed_code.length() - 1] != '\n') {
        removed_code.push_back('\n');
    }

    return removed_code;
}

string define_replace(string removed_code)
{
    size_t removed_code_length = removed_code.length();
    // get all replace macros
    for (int i = 0; i < removed_code_length; ++i) {
        if ((i == 0 || removed_code[i - 1] == '\n') && removed_code[i] == '#') {
            // is a pre process line
            int start_idx = i;
            // jump spaces
            while (removed_code[i] == ' ') {
                ++i;
            }

            if (i < removed_code_length - 8 && removed_code.substr(i + 1, 7) == "define ") {
                i += 8;
                // is a define line
                string macro_name, macro_content;
                for (; i < removed_code_length && removed_code[i] != ' ' && removed_code[i] != '\n'; ++i) {
                    macro_name.push_back(removed_code[i]);
                }

                // jump spaces
                while (removed_code[i] == ' ') {
                    ++i;
                }

                for (; i < removed_code_length && removed_code[i] != ' ' && removed_code[i] != '\n'; ++i) {
                    macro_content.push_back(removed_code[i]);
                }

                define_replace_set.insert(pair<string, string>(macro_name, macro_content));
            }

            // finish this line
            while (removed_code[i] != '\n') {
                ++i;
            }

            int end_idx = i;
            // use space as placeholder
            for (int idx = start_idx; idx < end_idx; ++idx) {
                removed_code[idx] = ' ';
            }
        }
    }

    string replace_code(removed_code);
    for (pair<string, string> pair : define_replace_set) {
        // only for debug
#ifdef DEBUG
        cout << "Name : " << pair.first << " Content : " << pair.second << endl;
#endif

        if (pair.second.length() == 0) {
            continue;
        }

        size_t sub_string_idx = 0;
        vector<size_t> sub_string_idxes;
        while ((sub_string_idx = replace_code.find(pair.first, sub_string_idx)) != string::npos) {
            sub_string_idxes.push_back(sub_string_idx);
            ++sub_string_idx;
        }

        string new_replace_code;
        sub_string_idx = 0;
        for (int i = 0; i < replace_code.length(); ++i) {
            if (sub_string_idx < sub_string_idxes.size() && sub_string_idxes[sub_string_idx] == i) {
                ++sub_string_idx;
                new_replace_code += pair.second;
                i += pair.first.length() - 1;
            }
            else {
                new_replace_code.push_back(replace_code[i]);
            }
        }

        replace_code = new_replace_code;
    }

//    cout << replace_code << endl;
    return replace_code;
}

string process_include(string source_code)
{
    // get all include macros
    bool no_change = false;
    while (!no_change) {
        no_change = true;
        for (int i = 0; i < source_code.length(); ++i) {
            if ((i == 0 || source_code[i - 1] == '\n') && source_code[i] == '#') {
                // is a pre process line
                int start_idx = i;
                // jump spaces
                while (source_code[i] == ' ') {
                    ++i;
                }

                string include_file;
                if (i < source_code.length() - 8 && source_code.substr(i + 1, 7) == "include") {
                    i += 8;
                    // is a include line
                    no_change = false;
                    // jump spaces
                    while (source_code[i] == ' ') {
                        ++i;
                    }

                    char c = source_code[i];
                    ++i;
                    for (; i < source_code.length() && source_code[i] != c; ++i) {
                        include_file.push_back(source_code[i]);
                    }
                }

                // finish this line
                while (source_code[i] != '\n') {
                    ++i;
                }

                int end_idx = i;

                if (!no_change) {
                    // start replace
                    string include_file_content = load_file_all(include_file);
                    source_code.replace(start_idx, end_idx - start_idx, include_file_content);
                    break;
                }
            }
        }
    }
    return source_code;
}

string load_file_all(const string& path)
{
    FILE* file_ptr = fopen(path.c_str(), "r");
    if (file_ptr == nullptr) {
        cout << "File Not Exist" << endl;
    }
    string ret_str;
    size_t load_byte_cnt = LOAD_BUFFER_SIZE;
    char load_buffer[LOAD_BUFFER_SIZE];
    while (load_byte_cnt == LOAD_BUFFER_SIZE) {
        load_byte_cnt = fread(load_buffer, 1, LOAD_BUFFER_SIZE, file_ptr);
        ret_str += string(load_buffer, load_byte_cnt);
    }

    fclose(file_ptr);
    return ret_str;
}