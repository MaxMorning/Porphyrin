//
// Project: Porphyrin
// File Name: Semantic.h
// Author: Morning
// Description:
//
// Create Date: 2021/12/8
//

#ifndef PORPHYRIN_SEMANTIC_H
#define PORPHYRIN_SEMANTIC_H
#include <string>
#include <vector>
#include <iomanip>
#include "Class/Node.h"

#define NONTERMINAL_CNT 64
#define MAX_REDUCTION 8
#define DISPLAY_WIDTH 12

using namespace std;

enum OP_CODE {
    OP_ASSIGNMENT,             // =
    OP_ADD_INT,                // +
    OP_ADD_FLOAT,              // +
    OP_ADD_DOUBLE,             // +
    OP_MINUS_INT,              // -
    OP_MINUS_FLOAT,            // -
    OP_MINUS_DOUBLE,           // -
    OP_MULTI_INT,              // *
    OP_MULTI_FLOAT,            // *
    OP_MULTI_DOUBLE,           // *
    OP_DIV_INT,                // /
    OP_DIV_FLOAT,              // /
    OP_DIV_DOUBLE,             // /
    OP_COMMA,                  // ,
    OP_LOGIC_OR,               // Logic or
    OP_LOGIC_AND,              // Logic and
    OP_LOGIC_NOT,              // Logic not
    OP_JEQ,                    // Jump when equal
    OP_JNZ,                    // Jump not equal zero
    OP_JMP,                    // unconditionally jump

    OP_LI_BOOL,                // load immediate
    OP_LI_INT,                 // load immediate
    OP_LI_FLOAT,               // load immediate
    OP_LI_DOUBLE,              // load immediate

    OP_FETCH_BOOL,             // fetch an element from array
    OP_FETCH_INT,              // fetch an element from array
    OP_FETCH_FLOAT,            // fetch an element from array
    OP_FETCH_DOUBLE,           // fetch an element from array

    OP_EQUAL_INT,              // ==
    OP_EQUAL_FLOAT,            // ==
    OP_EQUAL_DOUBLE,           // ==
    OP_GREATER_INT,            // >
    OP_GREATER_FLOAT,          // >
    OP_GREATER_DOUBLE,         // >
    OP_GREATER_EQUAL_INT,      // >=
    OP_GREATER_EQUAL_FLOAT,    // >=
    OP_GREATER_EQUAL_DOUBLE,   // >=
    OP_LESS_INT,               // <
    OP_LESS_FLOAT,             // <
    OP_LESS_DOUBLE,            // <
    OP_LESS_EQUAL_INT,         // <=
    OP_LESS_EQUAL_FLOAT,       // <=
    OP_LESS_EQUAL_DOUBLE,      // <=
    OP_NOT_EQUAL_INT,          // !=
    OP_NOT_EQUAL_FLOAT,        // !=
    OP_NOT_EQUAL_DOUBLE,       // !=

    OP_NEG,                     // -

    OP_PAR,                    // add parameter for function call
    OP_CALL,                    // function call

    OP_BOOL_TO_CHAR,           // bool to char
    OP_BOOL_TO_INT,            // bool to int
    OP_BOOL_TO_FLOAT,          // bool to float
    OP_BOOL_TO_DOUBLE,         // bool to double
    OP_CHAR_TO_BOOL,           // char to bool
    OP_CHAR_TO_INT,            // char to int
    OP_CHAR_TO_FLOAT,          // char to float
    OP_CHAR_TO_DOUBLE,         // char to double
    OP_INT_TO_BOOL,            // int to bool
    OP_INT_TO_CHAR,            // int to char
    OP_INT_TO_FLOAT,           // int to float
    OP_INT_TO_DOUBLE,          // int to double
    OP_FLOAT_TO_BOOL,          // float to bool
    OP_FLOAT_TO_CHAR,          // float to char
    OP_FLOAT_TO_INT,           // float to int
    OP_FLOAT_TO_DOUBLE,        // float to double
    OP_DOUBLE_TO_BOOL,         // double to bool
    OP_DOUBLE_TO_CHAR,         // double to char
    OP_DOUBLE_TO_INT,          // double to int
    OP_DOUBLE_TO_FLOAT,        // double to float

    OP_ARRAY_STORE,            // store value to array

    OP_RETURN,                 // return value
    OP_NOP,                    // do nothing
    OP_INVALID                 // invalid op
};

extern const string OP_TOKEN[];

#define DATA_TYPE_CNT 7
enum DATA_TYPE_ENUM {               // guaranteed sorting
    DT_NOT_DECIDED,
    DT_VOID,
    DT_BOOL,
    DT_CHAR,
    DT_INT,
    DT_FLOAT,
    DT_DOUBLE
};

enum OPR_USAGE_ENUM {
    USAGE_INVALID,
    USAGE_VAR,
    USAGE_INST,
    USAGE_ARRAY
};

extern const OPR_USAGE_ENUM OP_CODE_OPR_USAGE[][3];

extern const DATA_TYPE_ENUM OP_CODE_RESULT_DT[];

extern const string DATA_TYPE_TOKEN[];

extern const unsigned int BASE_DATA_TYPE_SIZE[];

extern const OP_CODE auto_cast_table[DATA_TYPE_CNT][DATA_TYPE_CNT];

struct Quaternion
{
    OP_CODE op_code;
    int opr1; // the index in symbol table
    int opr2; // the index in symbol table
    int result; // the index in symbol table or jump destination
};

union ValueType {
    bool bool_value;
    int int_value;
    float float_value;
    double double_value;
};

struct SymbolEntry {
    string content;
    bool is_temp;
    bool is_const;
    DATA_TYPE_ENUM data_type;
    bool is_array;
    int memory_size;
    int function_index; // -1 means global symbols
    bool is_initial;
    bool is_used;
    Node* node_ptr;
    int last_use_offset;

    // valid in array var
    vector<int> index_record;

    // only valid in const scalar var
    ValueType value; // use 64 bit to store the value, can be any data type

    // i386 arch
    // if is local var or array var, can get variable in [ebp + memory_offset] / [rbp + memory_offset] .
    // if is global, it will be stored in DataSegment, which is decided by assembler.
    int memory_offset;
};

struct Function {
    string name;
    int entry_address;
    vector<int> parameter_index;
    DATA_TYPE_ENUM return_data_type;

    static vector<Function> function_table;
};

extern vector<Quaternion> quaternion_sequence;
extern vector<SymbolEntry> symbol_table;

//static Node* current_node;

struct StackEntry {
    int symbol_index;
    int code_segment_layer;
};
extern int current_layer;
extern vector<StackEntry> analyse_symbol_stack;

void semantic_analysis_post(bool enable_print);

void check_unused();

void print_quaternion_sequence(vector<Quaternion> sequence);

extern unsigned int temp_symbol_cnt;
int get_temp_symbol(DATA_TYPE_ENUM data_type, bool is_const);

void back_patch(int list, int addr);

extern bool in_if_while_condition;

bool back_patch_in_general(int return_symbol_index);

int emit(OP_CODE op, int src1, int src2, int result);

void clear_symbol_stack();

void write_semantic_result();

// action function definition
// return value ptr is nullptr for fore action

//extern vector<int> semantic_action_stack; // for arg passing

typedef int (*ActionFunction)(int*);

extern ActionFunction action_function_ptr[NONTERMINAL_CNT][MAX_REDUCTION][2];

#endif //PORPHYRIN_SEMANTIC_H
