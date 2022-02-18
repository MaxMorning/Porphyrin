//
// Project: Porphyrin
// File Name: SemanticArrays.cpp
// Author: Morning
// Description:
//
// Create Date: 2022/2/16
//

#include "Include/Semantic.h"

const string OP_TOKEN[] = {
        "ASSIGN",                 // =
        "ADD_INT",                // +
        "ADD_FLOAT",              // +
        "ADD_DOUBLE",             // +
        "MINUS_INT",              // -
        "MINUS_FLOAT",            // -
        "MINUS_DOUBLE",           // -
        "MULTI_INT",              // *
        "MULTI_FLOAT",            // *
        "MULTI_DOUBLE",           // *
        "DIV_INT",                // /
        "DIV_FLOAT",              // /
        "DIV_DOUBLE",             // /
        "COMMA",                  // ,
        "LOGIC_OR",               // ||
        "LOGIC_AND",              // &&
        "LOGIC_NOT",              // !
        "JEQ",                    // Jump when equal
        "JNZ",                    // Jump not equal zero
        "JMP",                    // unconditionally jump

        "LI_BOOL",                // load immediate
        "LI_INT",                 // load immediate
        "LI_FLOAT",               // load immediate
        "LI_DOUBLE",              // load immediate

        "FETCH_BOOL",             // fetch an element from array
        "FETCH_INT",              // fetch an element from array
        "FETCH_FLOAT",            // fetch an element from array
        "FETCH_DOUBLE",           // fetch an element from array

        "EQUAL_INT",              // ==
        "EQUAL_FLOAT",            // ==
        "EQUAL_DOUBLE",           // ==
        "GREATER_INT",            // >
        "GREATER_FLOAT",          // >
        "GREATER_DOUBLE",         // >
        "GREATER_EQUAL_INT",      // >=
        "GREATER_EQUAL_FLOAT",    // >=
        "GREATER_EQUAL_DOUBLE",   // >=
        "LESS_INT",               // <
        "LESS_FLOAT",             // <
        "LESS_DOUBLE",            // <
        "LESS_EQUAL_INT",         // <=
        "LESS_EQUAL_FLOAT",       // <=
        "LESS_EQUAL_DOUBLE",      // <=
        "NOT_EQUAL_INT",          // !=
        "NOT_EQUAL_FLOAT",        // !=
        "NOT_EQUAL_DOUBLE",       // !=

        "NEG",                    // -

        "PAR",                    // add parameter for function call
        "CALL",                   // function call

        "BOOL_TO_CHAR",           // bool to char
        "BOOL_TO_INT",            // bool to int
        "BOOL_TO_FLOAT",          // bool to float
        "BOOL_TO_DOUBLE",         // bool to double
        "CHAR_TO_BOOL",           // char to bool
        "CHAR_TO_INT",            // char to int
        "CHAR_TO_FLOAT",          // char to float
        "CHAR_TO_DOUBLE",         // char to double
        "INT_TO_BOOL",            // int to bool
        "INT_TO_CHAR",            // int to char
        "INT_TO_FLOAT",           // int to float
        "INT_TO_DOUBLE",          // int to double
        "FLOAT_TO_BOOL",          // float to bool
        "FLOAT_TO_CHAR",          // float to char
        "FLOAT_TO_INT",           // float to int
        "FLOAT_TO_DOUBLE",        // float to double
        "DOUBLE_TO_BOOL",         // double to bool
        "DOUBLE_TO_CHAR",         // double to char
        "DOUBLE_TO_INT",          // double to int
        "DOUBLE_TO_FLOAT",        // double to float

        "ARRAY_STORE",            // store value to array

        "RETURN",                 // return value
        "NOP",                    // do nothing
        "INVALID"                 // invalid op
};

const string DATA_TYPE_TOKEN[] = {
        "NotDecided",
        "void",
        "bool",
        "char",
        "int",
        "float",
        "double"
};

const unsigned int BASE_DATA_TYPE_SIZE[] = {
        0,
        0,
        1,
        1,
        4,
        4,
        8
};

const OP_CODE auto_cast_table[DATA_TYPE_CNT][DATA_TYPE_CNT] = {
        //DT_NOT_DECIDED,DT_VOID,       DT_BOOL,            DT_CHAR,            DT_INT,         DT_FLOAT,           DT_DOUBLE
        {OP_INVALID,    OP_INVALID,     OP_INVALID,         OP_INVALID,         OP_INVALID,     OP_INVALID,         OP_INVALID}, // DT_NOT_DECIDED
        {OP_INVALID,    OP_INVALID,     OP_INVALID,         OP_INVALID,         OP_INVALID,         OP_INVALID,         OP_INVALID}, // DT_VOID
        {OP_INVALID,    OP_INVALID,     OP_INVALID,         OP_BOOL_TO_CHAR,    OP_BOOL_TO_INT, OP_BOOL_TO_FLOAT,   OP_BOOL_TO_DOUBLE}, // DT_BOOL
        {OP_INVALID,    OP_INVALID,     OP_CHAR_TO_BOOL,    OP_INVALID,         OP_CHAR_TO_INT,         OP_CHAR_TO_FLOAT,   OP_CHAR_TO_DOUBLE}, // DT_CHAR
        {OP_INVALID,    OP_INVALID,     OP_INT_TO_BOOL,     OP_INT_TO_CHAR,     OP_INVALID,     OP_INT_TO_FLOAT,    OP_INT_TO_DOUBLE}, // DT_INT
        {OP_INVALID,    OP_INVALID,     OP_FLOAT_TO_BOOL,   OP_FLOAT_TO_CHAR,   OP_FLOAT_TO_INT,    OP_INVALID,         OP_FLOAT_TO_DOUBLE}, // DT_FLOAT
        {OP_INVALID,    OP_INVALID,     OP_DOUBLE_TO_BOOL,  OP_DOUBLE_TO_CHAR,  OP_DOUBLE_TO_INT,   OP_DOUBLE_TO_FLOAT, OP_INVALID} // DT_DOUBLE
};