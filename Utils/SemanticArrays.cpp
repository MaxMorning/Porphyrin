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

// not decided means the result is same as opr1
const DATA_TYPE_ENUM OP_CODE_RESULT_DT[] = {
        DT_NOT_DECIDED,             // =
        DT_INT,                // +
        DT_FLOAT,              // +
        DT_DOUBLE,             // +
        DT_INT,              // -
        DT_FLOAT,            // -
        DT_DOUBLE,           // -
        DT_INT,              // *
        DT_FLOAT,            // *
        DT_DOUBLE,           // *
        DT_INT,                // /
        DT_FLOAT,              // /
        DT_DOUBLE,             // /
        DT_VOID,                  // ,
        DT_BOOL,               // Logic or
        DT_BOOL,              // Logic and
        DT_BOOL,              // Logic not
        DT_VOID,                    // Jump when equal
        DT_VOID,                    // Jump not equal zero
        DT_VOID,                    // unconditionally jump

        DT_BOOL,                // load immediate
        DT_INT,                 // load immediate
        DT_FLOAT,               // load immediate
        DT_DOUBLE,              // load immediate

        DT_BOOL,             // fetch an element from array
        DT_INT,              // fetch an element from array
        DT_FLOAT,            // fetch an element from array
        DT_DOUBLE,           // fetch an element from array

        DT_INT,              // ==
        DT_FLOAT,            // ==
        DT_DOUBLE,           // ==
        DT_INT,            // >
        DT_FLOAT,          // >
        DT_DOUBLE,         // >
        DT_INT,      // >=
        DT_FLOAT,    // >=
        DT_DOUBLE,   // >=
        DT_INT,               // <
        DT_FLOAT,             // <
        DT_DOUBLE,            // <
        DT_INT,         // <=
        DT_FLOAT,       // <=
        DT_DOUBLE,      // <=
        DT_INT,          // !=
        DT_FLOAT,        // !=
        DT_DOUBLE,       // !=

        DT_NOT_DECIDED,                     // -

        DT_NOT_DECIDED,                    // add parameter for function call
        DT_VOID,                    // function call

        DT_CHAR,           // bool to char
        DT_INT,            // bool to int
        DT_FLOAT,          // bool to float
        DT_DOUBLE,         // bool to double
        DT_BOOL,           // char to bool
        DT_INT,            // char to int
        DT_FLOAT,          // char to float
        DT_DOUBLE,         // char to double
        DT_BOOL,            // int to bool
        DT_CHAR,            // int to char
        DT_FLOAT,           // int to float
        DT_DOUBLE,          // int to double
        DT_BOOL,          // float to bool
        DT_CHAR,          // float to char
        DT_INT,           // float to int
        DT_DOUBLE,        // float to double
        DT_BOOL,         // double to bool
        DT_CHAR,         // double to char
        DT_INT,          // double to int
        DT_FLOAT,        // double to float

        DT_NOT_DECIDED,            // store value to array

        DT_VOID,                 // return value
        DT_VOID,                    // do nothing
        DT_VOID                 // invalid op
};

const OPR_USAGE_ENUM OP_CODE_OPR_USAGE[][3] = {
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},                 // =
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},                // +
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},              // +
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},             // +
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},              // -
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},            // -
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},           // -
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},              // *
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},            // *
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},           // *
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},                // /
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},              // /
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},             // /
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},                  // ,
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},               // ||
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},              // &&
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},              // !
        {USAGE_VAR, USAGE_VAR, USAGE_INST},                    // Jump when equal
        {USAGE_VAR, USAGE_INVALID, USAGE_INST},                    // Jump not equal zero
        {USAGE_INVALID, USAGE_INVALID, USAGE_INST},                    // unconditionally jump

        {USAGE_VAR, USAGE_INVALID, USAGE_INST},                // load immediate
        {USAGE_VAR, USAGE_INVALID, USAGE_INST},                 // load immediate
        {USAGE_VAR, USAGE_INVALID, USAGE_INST},               // load immediate
        {USAGE_VAR, USAGE_INST, USAGE_INST},              // load immediate

        {USAGE_ARRAY, USAGE_VAR, USAGE_VAR},             // fetch an element from array
        {USAGE_ARRAY, USAGE_VAR, USAGE_VAR},              // fetch an element from array
        {USAGE_ARRAY, USAGE_VAR, USAGE_VAR},            // fetch an element from array
        {USAGE_ARRAY, USAGE_VAR, USAGE_VAR},           // fetch an element from array

        {USAGE_VAR, USAGE_VAR, USAGE_VAR},              // ==
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},            // ==
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},           // ==
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},            // >
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},          // >
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},         // >
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},      // >=
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},    // >=
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},   // >=
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},               // <
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},             // <
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},            // <
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},         // <=
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},       // <=
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},      // <=
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},          // !=
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},        // !=
        {USAGE_VAR, USAGE_VAR, USAGE_VAR},       // !=

        {USAGE_VAR, USAGE_INVALID, USAGE_INVALID},                    // -

        {USAGE_VAR, USAGE_INVALID, USAGE_INVALID},                    // add parameter for function call
        {USAGE_INST, USAGE_INST, USAGE_VAR},                   // function call

        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},           // bool to char
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},            // bool to int
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},          // bool to float
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},         // bool to double
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},           // char to bool
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},            // char to int
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},          // char to float
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},         // char to double
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},            // int to bool
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},            // int to char
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},           // int to float
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},          // int to double
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},          // float to bool
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},          // float to char
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},           // float to int
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},        // float to double
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},         // double to bool
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},         // double to char
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},          // double to int
        {USAGE_VAR, USAGE_INVALID, USAGE_VAR},        // double to float

        {USAGE_VAR, USAGE_VAR, USAGE_ARRAY},            // store value to array

        {USAGE_VAR, USAGE_INVALID, USAGE_INVALID},                 // return value
        {USAGE_INVALID, USAGE_INVALID, USAGE_INVALID},                    // do nothing
        {USAGE_INVALID, USAGE_INVALID, USAGE_INVALID}                 // invalid op
};