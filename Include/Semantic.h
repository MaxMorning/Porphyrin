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

    OP_RETURN,                 // return value
    OP_NOP,                    // do nothing
    OP_INVALID                 // invalid op
};

static const string OP_TOKEN[] = {
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

    "RETURN",                 // return value
    "NOP",                    // do nothing
    "INVALID"                 // invalid op
};

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

static const string DATA_TYPE_TOKEN[] = {
    "NotDecided",
    "void",
    "bool",
    "char",
    "int",
    "float",
    "double"
};

static const OP_CODE auto_cast_table[DATA_TYPE_CNT][DATA_TYPE_CNT] = {
    //DT_NOT_DECIDED,DT_VOID,       DT_BOOL,            DT_CHAR,            DT_INT,         DT_FLOAT,           DT_DOUBLE
    {OP_INVALID,    OP_INVALID,     OP_INVALID,         OP_INVALID,         OP_INVALID,     OP_INVALID,         OP_INVALID}, // DT_NOT_DECIDED
    {OP_INVALID,    OP_INVALID,     OP_INVALID,         OP_INVALID,         OP_INVALID,         OP_INVALID,         OP_INVALID}, // DT_VOID
    {OP_INVALID,    OP_INVALID,     OP_INVALID,         OP_BOOL_TO_CHAR,    OP_BOOL_TO_INT, OP_BOOL_TO_FLOAT,   OP_BOOL_TO_DOUBLE}, // DT_BOOL
    {OP_INVALID,    OP_INVALID,     OP_CHAR_TO_BOOL,    OP_INVALID,         OP_CHAR_TO_INT,         OP_CHAR_TO_FLOAT,   OP_CHAR_TO_DOUBLE}, // DT_CHAR
    {OP_INVALID,    OP_INVALID,     OP_INT_TO_BOOL,     OP_INT_TO_CHAR,     OP_INVALID,     OP_INT_TO_FLOAT,    OP_INT_TO_DOUBLE}, // DT_INT
    {OP_INVALID,    OP_INVALID,     OP_FLOAT_TO_BOOL,   OP_FLOAT_TO_CHAR,   OP_FLOAT_TO_INT,    OP_INVALID,         OP_FLOAT_TO_DOUBLE}, // DT_FLOAT
    {OP_INVALID,    OP_INVALID,     OP_DOUBLE_TO_BOOL,  OP_DOUBLE_TO_CHAR,  OP_DOUBLE_TO_INT,   OP_DOUBLE_TO_FLOAT, OP_INVALID} // DT_DOUBLE
};

struct Quaternion
{
    OP_CODE op_code;
    int opr1; // the index in symbol table
    int opr2; // the index in symbol table
    int result; // the index in symbol table or jump destination
};

struct SymbolEntry {
    string content;
    bool is_temp;
    bool is_const;
    DATA_TYPE_ENUM data_type;
    int function_index; // -1 means global symbols
    bool is_initial;
    bool is_used;
    Node* node_ptr;
    int last_use_offset;
};

struct Function {
    string name;
    int entry_address;
    vector<DATA_TYPE_ENUM> parameter_types;
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

void semantic_analysis(Node* root);

void check_unused();

void print_semantic_result();

extern unsigned int temp_symbol_cnt;
int get_temp_symbol(DATA_TYPE_ENUM data_type);

void backpatch(int list, int addr);

int emit(OP_CODE op, int src1, int src2, int result);

void clear_symbol_stack();

void write_semantic_result();

// action function definition
// return value ptr is nullptr for fore action

extern vector<int> semantic_action_stack; // for arg passing

typedef int (*ActionFunction)(int*);

int always_return_0(int* return_values_ptr);

int Type_Conversion(DATA_TYPE_ENUM type_from, DATA_TYPE_ENUM type_to, int symbol_table_idx);
OP_CODE Op_Type_Conversion(OP_CODE op_from, DATA_TYPE_ENUM num_type);

int Source__Declarations_fore_action(int* return_values_ptr);
int Source__Declarations_post_action(int* return_values_ptr);

int VarDeclaration__Type_DeclaredVars_Semicolon_fore_action(int* return_values_ptr);
int VarDeclaration__Type_DeclaredVars_Semicolon_post_action(int* return_values_ptr);

int DeclaredVar__Variable_fore_action(int* return_values_ptr);
int DeclaredVar__Variable_post_action(int* return_values_ptr);

int DeclaredVar__Variable_Assignment_Expr_fore_action(int* return_values_ptr);
int DeclaredVar__Variable_Assignment_Expr_post_action(int* return_values_ptr);

int FunDeclaration__Type_Function_LeftBrace_HereIsParameter_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_fore_action(int* return_values_ptr);
int FunDeclaration__Type_Function_LeftBrace_HereIsParameter_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr);

int FunDeclaration__Type_Function_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_fore_action(int* return_values_ptr);
int FunDeclaration__Type_Function_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr);

static ActionFunction Type__int_fore_function = always_return_0;
int Type__int_post_function(int* return_values_ptr);

static ActionFunction Type__float_fore_function = always_return_0;
int Type__float_post_function(int* return_values_ptr);

static ActionFunction Type__void_fore_function = always_return_0;
int Type__void_post_function(int* return_values_ptr);

int Type__double_post_function(int* return_values_ptr);

int Type__bool_post_function(int* return_values_ptr);

static ActionFunction Function__id_fore_function = always_return_0;
int Function__id_post_function(int* return_values_ptr);

static ActionFunction HereIsParameter__void_fore_function = always_return_0;
int HereIsParameter__void_post_function(int* return_values_ptr);

static ActionFunction HereIsParameter__Parameters_fore_function = always_return_0;
int HereIsParameter__Parameters_post_function(int* return_values_ptr);

static ActionFunction Parameters__Parameter_Comma_Parameters_fore_function = always_return_0;
int Parameters__Parameter_Comma_Parameters_post_function(int* return_values_ptr);

static ActionFunction Parameters__Parameter_fore_function = always_return_0;
int Parameters__Parameter_post_function(int* return_values_ptr);

int Parameter__Type_Variable_fore_function(int* return_values_ptr);
int Parameter__Type_Variable_post_function(int* return_values_ptr);

static ActionFunction Variable__id_fore_function = always_return_0;
int Variable__id_post_function(int* return_values_ptr);

int Call__Function_LeftBrace_HereIsArgument_RightBrace_fore_function(int* return_values_ptr);
int Call__Function_LeftBrace_HereIsArgument_RightBrace_post_function(int* return_values_ptr);

static ActionFunction Call__Function_LeftBrace_RightBrace_fore_function = Call__Function_LeftBrace_HereIsArgument_RightBrace_fore_function;
int Call__Function_LeftBrace_RightBrace_post_function(int* return_values_ptr);

static ActionFunction HereIsArgument__void_fore_function = always_return_0;
int HereIsArgument__void_post_function(int* return_values_ptr);

static ActionFunction HereIsArgument__Arguments_fore_action = always_return_0;
int HereIsArgument__Arguments_post_action(int* return_values_ptr);

static ActionFunction Arguments__Argument_Comma_Arguments_fore_function = always_return_0;
int Arguments__Argument_Comma_Arguments_post_function(int* return_values_ptr);

static ActionFunction Arguments__Argument_fore_function = always_return_0;
int Arguments__Argument_post_function(int* return_values_ptr);

int Argument__Variable_fore_function(int* return_values_ptr);
int Argument__Variable_post_function(int* return_values_ptr);

int If__if_LeftBrace_Expr_RightBrace_Statement_fore_action(int* return_values_ptr);
int If__if_LeftBrace_Expr_RightBrace_Statement_post_action(int* return_values_ptr);

int If__if_LeftBrace_Expr_RightBrace_Statement_else_Statement_fore_action(int* return_values_ptr);
int If__if_LeftBrace_Expr_RightBrace_Statement_else_Statement_post_action(int* return_values_ptr);

static ActionFunction Statement__all_fore_action = always_return_0;
static ActionFunction Statement__all_post_action = always_return_0;

static ActionFunction While__while_LeftBrace_Expr_RightBrace_Statement_fore_action = If__if_LeftBrace_Expr_RightBrace_Statement_fore_action;
int While__while_LeftBrace_Expr_RightBrace_Statement_post_action(int* return_values_ptr);

static ActionFunction Item__Variable_fore_action = Argument__Variable_fore_function; // means use var

int LogicalOr__LogicalOr_LogicalOR_LogicalAnd_post_action(int* return_values_ptr);

int LogicalAnd__LogicalAnd_LogicalAND_Comparison_post_action(int* return_values_ptr);

int Item__LogicalNOT_Item_post_action(int* return_values_ptr);

int Comparison__Comparison_CmpSymbol_Addition_post_action(int* return_values_ptr);

int CmpSymbol__Equal_post_action(int* return_values_ptr);

int CmpSymbol__Greater_post_action(int* return_values_ptr);

int CmpSymbol__GreaterEqual_post_action(int* return_values_ptr);

int CmpSymbol__Less_post_action(int* return_values_ptr);

int CmpSymbol__LessEqual_post_action(int* return_values_ptr);

int CmpSymbol__NotEqual_post_action(int* return_values_ptr);

int Addition__Addition_AddSymbol_Multiplication_post_action(int* return_values_ptr);

int AddSymbol__Add_post_action(int* return_values_ptr);

int AddSymbol__Minus_post_action(int* return_values_ptr);

int Multiplication__Multiplication_MulSymbol_Item_post_action(int* return_values_ptr);

int MulSymbol__Multi_post_action(int* return_values_ptr);

int MulSymbol__DIV_post_action(int* return_values_ptr);

int Item__Neg_Item_post_action(int* return_values_ptr);

int Item__LeftBrace_Expr_RightBrace_post_function(int* return_values_ptr);

int Comma__Comma_Comma_Assignment_post_action(int* return_values_ptr);

int Left__LeftBrace_Comma_Comma_Left_RightBrace_post_action(int* return_values_ptr);

int Assignment__LogicalOr_ASSIGNMENT_Assignment_post_action(int* return_values_ptr);

int Left__LeftBrace_Left_ASSIGNMENT_Expr_RightBrace_post_action(int* return_values_ptr);

static ActionFunction Left__Variable_fore_action = Argument__Variable_fore_function;

int Return__return_Expr_Semicolon_post_action(int* return_values_ptr);

int Return__return_Semicolon_post_action(int* return_values_ptr);

int Compound__LeftCurlyBrace_Statements_RightCurlyBrace_fore_action(int* return_values_ptr);
int Compound__LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr);

int Num__num_post_action(int* return_values_ptr);

static ActionFunction action_function_ptr[NONTERMINAL_CNT][MAX_REDUCTION][2] =
        {
                // Source
                {
                        {Source__Declarations_fore_action, Source__Declarations_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Declarations
                {
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Declaration
                {
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // VarDeclaration
                {
                        {VarDeclaration__Type_DeclaredVars_Semicolon_fore_action, VarDeclaration__Type_DeclaredVars_Semicolon_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // FunDeclaration
                {
                        {FunDeclaration__Type_Function_LeftBrace_HereIsParameter_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_fore_action, FunDeclaration__Type_Function_LeftBrace_HereIsParameter_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action},
                        {FunDeclaration__Type_Function_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_fore_action, FunDeclaration__Type_Function_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Type
                {
                        {Type__int_fore_function, Type__int_post_function},
                        {Type__float_fore_function, Type__float_post_function},
                        {always_return_0, Type__double_post_function},
                        {Type__void_fore_function, Type__void_post_function},
                        {always_return_0, Type__bool_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // DeclaredVars
                {
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // DeclaredVar
                {
                        {DeclaredVar__Variable_fore_action, DeclaredVar__Variable_post_action},
                        {DeclaredVar__Variable_Assignment_Expr_fore_action, DeclaredVar__Variable_Assignment_Expr_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Variable
                {
                        {Variable__id_fore_function, Variable__id_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Expr
                {
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Function
                {
                        {Function__id_fore_function, Function__id_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // HereIsParameter
                {
                        {HereIsParameter__void_fore_function, HereIsParameter__void_post_function},
                        {HereIsParameter__Parameters_fore_function, HereIsParameter__Parameters_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Compound
                {
                        {always_return_0, always_return_0},
                        {Compound__LeftCurlyBrace_Statements_RightCurlyBrace_fore_action, Compound__LeftCurlyBrace_Statements_RightCurlyBrace_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Parameters
                {
                        {Parameters__Parameter_Comma_Parameters_fore_function, Parameters__Parameter_Comma_Parameters_post_function},
                        {Parameters__Parameter_fore_function, Parameters__Parameter_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Parameter
                {
                        {Parameter__Type_Variable_fore_function, Parameter__Type_Variable_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Statements
                {
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Statement
                {
                        {Statement__all_fore_action, Statement__all_post_action},
                        {Statement__all_fore_action, Statement__all_post_action},
                        {Statement__all_fore_action, Statement__all_post_action},
                        {Statement__all_fore_action, Statement__all_post_action},
                        {Statement__all_fore_action, Statement__all_post_action},
                        {Statement__all_fore_action, Statement__all_post_action},
                        {Statement__all_fore_action, Statement__all_post_action},
                        {always_return_0, always_return_0}
                },

                // Expression
                {
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // If
                {
                        {If__if_LeftBrace_Expr_RightBrace_Statement_fore_action, If__if_LeftBrace_Expr_RightBrace_Statement_post_action},
                        {If__if_LeftBrace_Expr_RightBrace_Statement_else_Statement_fore_action, If__if_LeftBrace_Expr_RightBrace_Statement_else_Statement_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // While
                {
                        {While__while_LeftBrace_Expr_RightBrace_Statement_fore_action, While__while_LeftBrace_Expr_RightBrace_Statement_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Return
                {
                        {always_return_0, Return__return_Expr_Semicolon_post_action},
                        {always_return_0, Return__return_Semicolon_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Comma
                {
                        {always_return_0, Comma__Comma_Comma_Assignment_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, Left__LeftBrace_Comma_Comma_Left_RightBrace_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Assignment
                {
                        {always_return_0, Assignment__LogicalOr_ASSIGNMENT_Assignment_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // LogicalOr
                {
                        {always_return_0, LogicalOr__LogicalOr_LogicalOR_LogicalAnd_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // LogicalAnd
                {
                        {always_return_0, LogicalAnd__LogicalAnd_LogicalAND_Comparison_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Comparison
                {
                        {always_return_0, Comparison__Comparison_CmpSymbol_Addition_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // CmpSymbol
                {
                        {always_return_0, CmpSymbol__Equal_post_action},
                        {always_return_0, CmpSymbol__Greater_post_action},
                        {always_return_0, CmpSymbol__GreaterEqual_post_action},
                        {always_return_0, CmpSymbol__Less_post_action},
                        {always_return_0, CmpSymbol__LessEqual_post_action},
                        {always_return_0, CmpSymbol__NotEqual_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Addition
                {
                        {always_return_0, Addition__Addition_AddSymbol_Multiplication_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // AddSymbol
                {
                        {always_return_0, AddSymbol__Add_post_action},
                        {always_return_0, AddSymbol__Minus_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Multiplication
                {
                        {always_return_0, Multiplication__Multiplication_MulSymbol_Item_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // MulSymbol
                {
                        {always_return_0, MulSymbol__Multi_post_action},
                        {always_return_0, MulSymbol__DIV_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Item
                {
                        {always_return_0, Item__LogicalNOT_Item_post_action},
                        {always_return_0, Item__Neg_Item_post_action},
                        {always_return_0, Item__LeftBrace_Expr_RightBrace_post_function},
                        {Item__Variable_fore_action, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Call
                {
                        {Call__Function_LeftBrace_HereIsArgument_RightBrace_fore_function, Call__Function_LeftBrace_HereIsArgument_RightBrace_post_function},
                        {Call__Function_LeftBrace_RightBrace_fore_function, Call__Function_LeftBrace_RightBrace_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Num
                {
                        {always_return_0, Num__num_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // HereIsArgument
                {
                        {HereIsArgument__void_fore_function, HereIsArgument__void_post_function},
                        {HereIsArgument__Arguments_fore_action, HereIsArgument__Arguments_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Arguments
                {
                        {Arguments__Argument_Comma_Arguments_fore_function, Arguments__Argument_Comma_Arguments_post_function},
                        {Arguments__Argument_fore_function, Arguments__Argument_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Argument
                {
//                        {Argument__Variable_fore_function, Argument__Variable_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                }
        };

#endif //PORPHYRIN_SEMANTIC_H
