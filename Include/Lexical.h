//
// Project: Porphyrin
// File Name: Lexical.h
// Author: Morning
// Description:
//
// Create Date: 2021/10/3
//

#ifndef PORPHYRIN_LEXICAL_H
#define PORPHYRIN_LEXICAL_H

#include <string>
#include <vector>
#include <iostream>

#define LEXICAL_BUFFER_SIZE 256

using namespace std;

// TOKENS
enum TYPE_ENUM {
    ASSIGNMENT,             // =
    ADD,                    // +
    MINUS,                  // -
    STAR,                   // *
    DIV,                    // /
    EQUAL,                  // ==
    GREAT,                  // >
    GREAT_EQUAL,            // >=
    LESS,                   // <
    LESS_EQUAL,             // <=
    NOT_EQUAL,              // !=
    INCREASE,               // ++
    DECREASE,               // --
    SEMICOLON,              // ;
    COMMA,                  // ,
    LEFT_BRACE,             // (
    RIGHT_BRACE,            // )
    LEFT_CURLY_BRACE,       // {
    RIGHT_CURLY_BRACE,      // }
    LEFT_SQUARE_BRACE,      // [
    RIGHT_SQUARE_BRACE,     // ]
    LOGIC_NOT,              // !
    BIT_AND,                // &
    LOGIC_AND,              // &&
    BIT_OR,                 // |
    LOGIC_OR,               // ||
    EPSILON,                // $
    END_SIGNAL              // #, END_SIGNAL should be the last of enum
};

// key words & TOKENS define
#define KW_NUMBER       (-2)
#define KW_VAR_FUN      (-3)

#define KEYWORD_TYPE_CNT 11
// mapping: -idx - 4
const string KW_TYPES[] = {
        "else",
        "if",
        "int",
        "float",
        "bool",
        "double",
        "return",
        "void",
        "while",
        "true",
        "false"
};

struct Lexicon {
    int lex_type;
    string lex_content;
    int offset;

    Lexicon(char* lexical_buffer, size_t buffer_size, int processing_idx);
    Lexicon(int type, string token, int processing_idx);

    string get_type() const;
    int convert_to_terminal_index() const;
};

vector<Lexicon> lexical_analysis(string processed_code);
bool is_digit(char c);
bool is_letter(char c);
bool is_valid_in_var_fun(char c);

void print_lexical_result(const vector<Lexicon>& result_vector);
#endif //PORPHYRIN_LEXICAL_H
