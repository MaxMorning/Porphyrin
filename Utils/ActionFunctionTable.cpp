//
// Project: Porphyrin
// File Name: ActionFunctionTable.cpp
// Author: Morning
// Description:
//
// Create Date: 2022/1/27
//

#include "Include/Semantic.h"

int always_return_0(int* return_values_ptr);

int Type_Conversion(DATA_TYPE_ENUM type_from, DATA_TYPE_ENUM type_to, int symbol_table_idx);
OP_CODE Op_Type_Conversion(OP_CODE op_from, DATA_TYPE_ENUM num_type);

int Source__Declarations_post_action(int* return_values_ptr);

//int VarDeclaration__Type_DeclaredVars_Semicolon_fore_action(int* return_values_ptr);
int VarDeclaration__Type_DeclaredVars_Semicolon_post_action(int* return_values_ptr);

int VarDeclaration_Const_Type_DeclaredVars_Semicolon_post_action(int* return_values_ptr);

//int DeclaredVar__Variable_fore_action(int* return_values_ptr);
int DeclaredVar__id_post_action(int* return_values_ptr);

int DeclaredVar__id_Indices_post_action(int* return_values_ptr);

//int DeclaredVar__Variable_Assignment_Expr_fore_action(int* return_values_ptr);
int DeclaredVar__id_Assignment_Expr_post_action(int* return_values_ptr);

int IdLeftBrace__id_LeftBrace_post_action(int* return_values_ptr);
int FunDeclaration__Type_FuncName_LeftBrace_HereIsParameter_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr);

//int FunDeclaration__Type_id_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_fore_action(int* return_values_ptr);
int FunDeclaration__Type_FuncName_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr);

//ActionFunction Type__int_fore_function = always_return_0;
int Type__int_post_function(int* return_values_ptr);

//ActionFunction Type__float_fore_function = always_return_0;
int Type__float_post_function(int* return_values_ptr);

//ActionFunction Type__void_fore_function = always_return_0;
int Type__void_post_function(int* return_values_ptr);

int Type__double_post_function(int* return_values_ptr);

int Type__bool_post_function(int* return_values_ptr);

//ActionFunction Function__id_fore_function = always_return_0;
int Function__id_post_function(int* return_values_ptr);

//ActionFunction HereIsParameter__void_fore_function = always_return_0;
int HereIsParameter__void_post_function(int* return_values_ptr);

//ActionFunction HereIsParameter__Parameters_fore_function = always_return_0;
int HereIsParameter__Parameters_post_function(int* return_values_ptr);

//ActionFunction Parameters__Parameter_Comma_Parameters_fore_function = always_return_0;
int Parameters__Parameter_Comma_Parameters_post_function(int* return_values_ptr);

//ActionFunction Parameters__Parameter_fore_function = always_return_0;
int Parameters__Parameter_post_function(int* return_values_ptr);

//int Parameter__Type_Variable_fore_function(int* return_values_ptr);
int Parameter__Type_id_post_function(int* return_values_ptr);
int Parameter__Type_id_Indices_post_function(int* return_values_ptr);

//ActionFunction Variable__id_fore_function = always_return_0;
int VariableUse__id_post_function(int* return_values_ptr);

//int Variable__id_Indices_fore_function(int* return_values_ptr);
int VariableUse__id_Indices_post_function(int* return_values_ptr);

int Indices__LeftSquareBrace_Expr_RightSquareBrace_Indices_fore_function(int* return_values_ptr);
int DecIndices__LeftSquareBrace_Expr_RightSquareBrace_DecIndices_post_function(int* return_values_ptr);
int DecIndices__LeftSquareBrace_Expr_RightSquareBrace_post_function(int* return_values_ptr);

int UseIndices__LeftSquareBrace_Expr_RightSquareBrace_UseIndices_post_function(int* return_values_ptr);
int UseIndices__LeftSquareBrace_Expr_RightSquareBrace_post_function(int* return_values_ptr);

//int Call__Function_LeftBrace_HereIsArgument_RightBrace_fore_function(int* return_values_ptr);
int Call__id_LeftBrace_HereIsArgument_RightBrace_post_function(int* return_values_ptr);

//ActionFunction Call__Function_LeftBrace_RightBrace_fore_function = Call__Function_LeftBrace_HereIsArgument_RightBrace_fore_function;
int Call__id_LeftBrace_RightBrace_post_function(int* return_values_ptr);

ActionFunction HereIsArgument__void_fore_function = always_return_0;
int HereIsArgument__void_post_function(int* return_values_ptr);

ActionFunction HereIsArgument__Arguments_fore_action = always_return_0;
int HereIsArgument__Arguments_post_action(int* return_values_ptr);

ActionFunction Arguments__Argument_Comma_Arguments_fore_function = always_return_0;
int Arguments__Argument_Comma_Arguments_post_function(int* return_values_ptr);

ActionFunction Arguments__Argument_fore_function = always_return_0;
int Arguments__Argument_post_function(int* return_values_ptr);

int Argument__Argument_Comma_Arguments_fore_function(int* return_values_ptr);
int Argument__Variable_post_function(int* return_values_ptr);

int If__if_LeftBrace_Expr_RightBrace_Statement_fore_action(int* return_values_ptr);
int If__if_LeftBrace_Expr_RightBrace_Statement_post_action(int* return_values_ptr);

int If__if_LeftBrace_Expr_RightBrace_Statement_else_Statement_fore_action(int* return_values_ptr);
int If__if_LeftBrace_Expr_RightBrace_Statement_else_Statement_post_action(int* return_values_ptr);

ActionFunction Statement__all_fore_action = always_return_0;
ActionFunction Statement__all_post_action = always_return_0;

ActionFunction While__while_LeftBrace_Expr_RightBrace_Statement_fore_action = If__if_LeftBrace_Expr_RightBrace_Statement_fore_action;
int While__while_LeftBrace_Expr_RightBrace_Statement_post_action(int* return_values_ptr);

int Item__VariableUse_post_action(int* return_values_ptr); // means use var

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

int Return__return_Expr_Semicolon_post_action(int* return_values_ptr);

int Return__return_Semicolon_post_action(int* return_values_ptr);

int Compound__LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr);

int Num__num_post_action(int* return_values_ptr);
int Num__true_post_action(int* return_values_ptr);
int Num__false_post_action(int* return_values_ptr);


int Const__const_post_action(int* return_values_ptr);

int IncLayer(int* return_values_ptr);

ActionFunction action_function_ptr[NONTERMINAL_CNT][MAX_REDUCTION][2] =
        {
                // Source
                {
                        {always_return_0, Source__Declarations_post_action},
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
                        {always_return_0, VarDeclaration__Type_DeclaredVars_Semicolon_post_action},
                        {always_return_0, VarDeclaration_Const_Type_DeclaredVars_Semicolon_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // FunDeclaration
                {
                        {always_return_0, FunDeclaration__Type_FuncName_LeftBrace_HereIsParameter_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action},
                        {always_return_0, FunDeclaration__Type_FuncName_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Const
                {
                        {always_return_0, Const__const_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Type
                {
                        {always_return_0, Type__int_post_function},
                        {always_return_0, Type__float_post_function},
                        {always_return_0, Type__double_post_function},
                        {always_return_0, Type__void_post_function},
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
                        {always_return_0, DeclaredVar__id_post_action},
                        {always_return_0, DeclaredVar__id_Indices_post_action},
                        {always_return_0, DeclaredVar__id_Assignment_Expr_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // DecIndices
                {
                        {always_return_0, DecIndices__LeftSquareBrace_Expr_RightSquareBrace_DecIndices_post_function},
                        {always_return_0, DecIndices__LeftSquareBrace_Expr_RightSquareBrace_post_function},
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

                // VariableUse
                {
                        {always_return_0, VariableUse__id_post_function},
                        {always_return_0, VariableUse__id_Indices_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // UseIndices
                {
                        {always_return_0, UseIndices__LeftSquareBrace_Expr_RightSquareBrace_UseIndices_post_function},
                        {always_return_0, UseIndices__LeftSquareBrace_Expr_RightSquareBrace_post_function}
                },

                // IdLeftBrace
                {
                        {always_return_0, IdLeftBrace__id_LeftBrace_post_action}
                },



//                // Function
//                {
//                        {Function__id_fore_function, Function__id_post_function},
//                        {always_return_0, always_return_0},
//                        {always_return_0, always_return_0},
//                        {always_return_0, always_return_0},
//                        {always_return_0, always_return_0},
//                        {always_return_0, always_return_0},
//                        {always_return_0, always_return_0},
//                        {always_return_0, always_return_0}
//                },

                // HereIsParameter
                {
                        {always_return_0, HereIsParameter__void_post_function},
                        {always_return_0, HereIsParameter__Parameters_post_function},
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
                        {always_return_0, Compound__LeftCurlyBrace_Statements_RightCurlyBrace_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Parameters
                {
                        {always_return_0, Parameters__Parameter_Comma_Parameters_post_function},
                        {always_return_0, Parameters__Parameter_post_function},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Parameter
                {
                        {always_return_0, Parameter__Type_id_post_function},
                        {always_return_0, Parameter__Type_id_Indices_post_function},
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
                        {always_return_0, If__if_LeftBrace_Expr_RightBrace_Statement_post_action},
                        {always_return_0, If__if_LeftBrace_Expr_RightBrace_Statement_else_Statement_post_action},
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

                // IncLayerFore
                {
                        {always_return_0, IncLayer}
                },

                // Comma
                {
                        {always_return_0, Comma__Comma_Comma_Assignment_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
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
                        {always_return_0, Item__VariableUse_post_action},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0},
                        {always_return_0, always_return_0}
                },

                // Call
                {
                        {always_return_0, Call__id_LeftBrace_HereIsArgument_RightBrace_post_function},
                        {always_return_0, Call__id_LeftBrace_RightBrace_post_function},
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
                        {always_return_0, Num__true_post_action},
                        {always_return_0, Num__false_post_action},
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
//                        {Argument__Argument_Comma_Arguments_fore_function, Argument__Variable_post_function},
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
