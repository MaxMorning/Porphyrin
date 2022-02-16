//
// Project: Porphyrin
// File Name: Semantic.cpp
// Author: Morning
// Description:
//
// Create Date: 2021/12/8
//

#include "Include/Semantic.h"
#include <assert.h>
#include "Class/Diagnose.h"
#include <fstream>
#include <iomanip>

#define DISPLAY_WIDTH 12

vector<Function> Function::function_table;
vector<Quaternion> quaternion_sequence;
vector<SymbolEntry> symbol_table;
vector<StackEntry> analyse_symbol_stack;
int current_layer = 0;
vector<int> semantic_action_stack;

// Semantic part
// uses semantic_action_stack for arg passing
Node* Node::current_node;
int Node::semantic_action()
{
    Node::current_node = this;
    int* return_values = new int[child_nodes_ptr.size()];
    // fore action function
    // ignore return value
    this->quad = quaternion_sequence.size();
    (*action_function_ptr[this->non_terminal_idx][this->reduction_idx][0])(nullptr);

    for (int i = 0; i < child_nodes_ptr.size(); ++i) {
        if (!child_nodes_ptr[i]->is_terminal) {
            return_values[i] = child_nodes_ptr[i]->semantic_action();
        }
    }

    // post
    Node::current_node = this;
    if (this->child_nodes_ptr[0]->content == "(")
    {
        this->truelist = this->child_nodes_ptr[1]->truelist;
        this->falselist = this->child_nodes_ptr[1]->falselist;
    }
    else if (this->child_nodes_ptr[0]->is_terminal)
    {
        this->truelist = -1;
        this->falselist = -1;
    }
    else
    {
        this->truelist = this->child_nodes_ptr[0]->truelist;
        this->falselist = this->child_nodes_ptr[0]->falselist;
    }
    int ret = (*action_function_ptr[this->non_terminal_idx][this->reduction_idx][1])(return_values);
    this->nextlist = quaternion_sequence.size();

    delete[] return_values;
    return ret;
}

void semantic_analysis(Node* root)
{
//    for (Nonterminal& nonterminal : Nonterminal::all_nonterminal_chars) {
//        cout << nonterminal.token << endl;
//    }
    root->semantic_action();
    check_unused();
    Diagnose::printStream();
}

void check_unused()
{
    for (SymbolEntry& symbolEntry : symbol_table) {
        if (!symbolEntry.is_temp && !symbolEntry.is_used) {
            Diagnose::printWarning(symbolEntry.node_ptr->offset, "Variable " + symbolEntry.content + " is unused.");
        }
    }
}


void print_semantic_result()
{
    // print quaternion
    cout << "Quaternion sequence" << endl;
    int cnt = 0;
    for (int i = 0; i < quaternion_sequence.size(); ++i) {
        Quaternion& quaternion = quaternion_sequence[i];
        cout << cnt << "\t" << OP_TOKEN[quaternion.op_code] << "\t";
        if (OP_TOKEN[quaternion.op_code].size() < 8)
        {
            cout << "\t";
        }
        switch (quaternion.op_code) {
            case OP_CALL:
                cout << Function::function_table[quaternion.opr1].name  << "\t" << quaternion.opr2 << "\t" << symbol_table[quaternion.result].content << endl;
                break;

            case OP_JMP:
                cout << "-\t-\t" << quaternion.result + i << endl;
                break;

            case OP_JEQ:
            case OP_JNZ:
                cout << symbol_table[quaternion.opr1].content << "\t" << (quaternion.opr2 < 0 ? "-" : symbol_table[quaternion.opr2].content) << "\t" << quaternion.result + i << endl;
                break;

            case OP_LI_BOOL:
                cout << (quaternion.opr1 ? "true" : "false") << "\t-\t" << symbol_table[quaternion.result].content << endl;
                break;

            case OP_LI_INT:
                cout << quaternion.opr1 << "\t-\t" << symbol_table[quaternion.result].content << endl;
                break;

            case OP_LI_FLOAT: {
                float float_num = *((float *) (&quaternion.opr1));
                cout << float_num << "\t-\t" << symbol_table[quaternion.result].content << endl;
                break;
            }

            case OP_LI_DOUBLE: {
                double double_num;
                int *int_ptr = (int *) &double_num;
                int_ptr[0] = quaternion.opr1;
                int_ptr[1] = quaternion.opr2;
                cout << double_num << "\t" << symbol_table[quaternion.result].content << endl;
                break;
            }

            default:
                cout << (quaternion.opr1 < 0 ? "-" : symbol_table[quaternion.opr1].content) << "\t" << (quaternion.opr2 < 0 ? "-" : symbol_table[quaternion.opr2].content) << "\t" << (quaternion.result < 0 ? "-" : symbol_table[quaternion.result].content) << endl;
                break;
        }
        ++cnt;
    }

    // print symbol table
    cout << "Symbol table" << endl;
    cnt = 0;
    for (SymbolEntry& symbolEntry : symbol_table) {
        cout << setiosflags(ios::left) << setw(6) << cnt << setiosflags(ios::left) << setw(12) << symbolEntry.content << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << DATA_TYPE_TOKEN[symbolEntry.data_type] << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_temp ? "Temp" : "Declare") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) <<
             (symbolEntry.function_index < 0 ? "Global" : Function::function_table[symbolEntry.function_index].name) << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_used ? "Used" : "Unused") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_initial ? "Initialed" : "Uninitiated")   << endl;
        ++cnt;
    }

    // print function table
    cout << "Function table" << endl;
    cnt = 0;
    for (Function& function : Function::function_table) {
        cout << cnt << "\t" << function.name << "\t\t EntryAddr : " << function.entry_address << "\tReturnData: " << DATA_TYPE_TOKEN[function.return_data_type] << "\tArgs: ";

        for (DATA_TYPE_ENUM& data_type : function.parameter_types) {
            cout << DATA_TYPE_TOKEN[data_type] << '\t';
        }
        cout << endl;
        ++cnt;
    }
    cout << "Semantic Done." << endl;
}

void write_semantic_result()
{
    // write quaternion
    ofstream fout("Quaternion.txt");
    int cnt = 0;
    for (int i = 0; i < quaternion_sequence.size(); ++i) {
        Quaternion& quaternion = quaternion_sequence[i];
        fout << setiosflags(ios::left) << setw(6) << cnt << setiosflags(ios::left) << setw(12) << OP_TOKEN[quaternion.op_code] << "\t";

        switch (quaternion.op_code) {
            case OP_CALL:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << Function::function_table[quaternion.opr1].name << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << quaternion.opr2 << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "-" << endl;
                break;

            case OP_JMP:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "-" << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "-" << quaternion.result + i << endl;
                break;

            case OP_JEQ:
            case OP_JNZ:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << symbol_table[quaternion.opr1].content << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (quaternion.opr2 < 0 ? "-" : symbol_table[quaternion.opr2].content) << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << quaternion.result + i << endl;
                break;

            case OP_LI_BOOL:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (quaternion.opr1 ? "true" : "false") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "-"  << setiosflags(ios::left) << setw(DISPLAY_WIDTH)<< symbol_table[quaternion.result].content << endl;
                break;

            case OP_LI_INT:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << quaternion.opr1 << "" << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "-" << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << symbol_table[quaternion.result].content << endl;
                break;

            case OP_LI_DOUBLE: {
                double double_num;
                int *int_ptr = (int *) &double_num;
                int_ptr[0] = quaternion.opr1;
                int_ptr[1] = quaternion.opr2;
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << double_num << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << symbol_table[quaternion.result].content << endl;
                break;
            }

            default:
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (quaternion.opr1 < 0 ? "-" : symbol_table[quaternion.opr1].content) << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (quaternion.opr2 < 0 ? "-" : symbol_table[quaternion.opr2].content) << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (quaternion.result < 0 ? "-" : symbol_table[quaternion.result].content) << endl;
                break;
        }
        ++cnt;
    }
    fout.close();

    // write symbol table
    fout.open("SymbolTable.txt");
    cnt = 0;
    for (SymbolEntry& symbolEntry : symbol_table) {
        fout << setiosflags(ios::left) << setw(6) << cnt << setiosflags(ios::left) << setw(12) << symbolEntry.content << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << DATA_TYPE_TOKEN[symbolEntry.data_type] << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_temp ? "Temp" : "Declare") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) <<
             (symbolEntry.function_index < 0 ? "Global" : Function::function_table[symbolEntry.function_index].name) << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_used ? "Used" : "Unused") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_initial ? "Initialed" : "Uninitiated")   << endl;
        ++cnt;
    }
    fout.close();

    // write function table
    fout.open("FunctionTable.txt");
    cnt = 0;
    for (Function& function : Function::function_table) {
        fout << setiosflags(ios::left) << setw(6) << cnt << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << function.name << "EntryAddr : " << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << function.entry_address << '\t' << "ReturnData: " << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << DATA_TYPE_TOKEN[function.return_data_type] << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << "Args: ";

        for (DATA_TYPE_ENUM& data_type : function.parameter_types) {
            fout << DATA_TYPE_TOKEN[data_type] << '\t';
        }
        fout << endl;
        ++cnt;
    }
    fout.close();
}

unsigned int temp_symbol_cnt = 0;
int get_temp_symbol(DATA_TYPE_ENUM data_type)
{
    SymbolEntry temp_symbol;
    temp_symbol.content = "temp_" + to_string(temp_symbol_cnt);
    ++temp_symbol_cnt;
    temp_symbol.is_temp = true;
    temp_symbol.is_const = true;
    temp_symbol.data_type = data_type;
    temp_symbol.function_index = Function::function_table.size() - 1;
    temp_symbol.is_used = false;
    temp_symbol.is_initial = true;
    temp_symbol.node_ptr = nullptr;
    temp_symbol.last_use_offset = Node::current_node->offset;

    symbol_table.push_back(temp_symbol);

    return symbol_table.size() - 1;
}

int always_return_0(int* return_values_ptr)
{
    if (return_values_ptr == nullptr) {
        return 0;
    }
    else {
        return return_values_ptr[0];
    }
}

// this function will add quaternion into sequence if inputs can be cast
int auto_cast(int src_index, DATA_TYPE_ENUM dst_data_type)
{
    OP_CODE cast_op = auto_cast_table[symbol_table[src_index].data_type][dst_data_type];
    if (cast_op == OP_INVALID) {
        return -1;
    }
    else {
        if (symbol_table[src_index].data_type > dst_data_type) {
            Diagnose::printWarning(symbol_table[src_index].last_use_offset, "Narrow conversion from " + DATA_TYPE_TOKEN[symbol_table[src_index].data_type] + " to " + DATA_TYPE_TOKEN[dst_data_type] + '.');
        }
        int new_symbol_idx = get_temp_symbol(dst_data_type);
        quaternion_sequence.push_back(Quaternion{cast_op, src_index, -1, new_symbol_idx});
        return new_symbol_idx;
    }
}

int Type_Conversion(DATA_TYPE_ENUM type_from, DATA_TYPE_ENUM type_to, int symbol_table_idx)
{
    if(type_from == type_to)        // no need to converse
        return symbol_table_idx;
    int ret = get_temp_symbol(type_to);

    if (type_from > type_to) {
        Diagnose::printWarning(symbol_table[symbol_table_idx].last_use_offset, "Narrow conversion from " + DATA_TYPE_TOKEN[type_from] + " to " + DATA_TYPE_TOKEN[type_to] + '.');
    }
    emit(auto_cast_table[type_from][type_to], symbol_table_idx, -1, ret);
    return ret;
}

OP_CODE Op_Type_Conversion(OP_CODE op_from, DATA_TYPE_ENUM num_type)
{
    OP_CODE op_to;
    if(op_from >= OP_ADD_INT && op_from <= OP_ADD_DOUBLE)   // ADD +
    {
        op_to = (OP_CODE)( OP_ADD_INT + (num_type - DT_INT) );
    }
    else if(op_from >= OP_MINUS_INT && op_from <= OP_MINUS_DOUBLE)  // MINUS -
    {
        op_to = (OP_CODE)( OP_MINUS_INT + (num_type - DT_INT) );
    }
    else if(op_from >= OP_MULTI_INT && op_from <= OP_MULTI_DOUBLE)  // MULTI *
    {
        op_to = (OP_CODE)( OP_MULTI_INT + (num_type - DT_INT) );
    }
    else if(op_from >= OP_DIV_INT && op_from <= OP_DIV_DOUBLE)  // DIV /
    {
        op_to = (OP_CODE)( OP_DIV_INT + (num_type - DT_INT) );
    }
    else if(op_from >= OP_EQUAL_INT && op_from <= OP_EQUAL_DOUBLE)  // EQUAL ==
    {
        op_to = (OP_CODE)( OP_EQUAL_INT + (num_type - DT_INT) );
    }
    else if(op_from >= OP_GREATER_INT && op_from <= OP_GREATER_DOUBLE)  // GREATER >
    {
        op_to = (OP_CODE)( OP_GREATER_INT + (num_type - DT_INT) );
    }
    else if(op_from >= OP_GREATER_EQUAL_INT && op_from <= OP_GREATER_EQUAL_DOUBLE)  // GREATER_EQUAL >=
    {
        op_to = (OP_CODE)( OP_GREATER_EQUAL_INT + (num_type - DT_INT) );
    }
    else if(op_from >= OP_LESS_INT && op_from <= OP_LESS_DOUBLE)    // LESS <
    {
        op_to = (OP_CODE)( OP_LESS_INT + (num_type - DT_INT) );
    }
    else if(op_from >= OP_LESS_EQUAL_INT && op_from <= OP_LESS_EQUAL_DOUBLE)    // LESS_EQUAL <=
    {
        op_to = (OP_CODE)( OP_LESS_EQUAL_INT + (num_type - DT_INT) );
    }
    else if(op_from >= OP_NOT_EQUAL_INT && op_from <= OP_NOT_EQUAL_DOUBLE)  // NOT_EQUAL !=
    {
        op_to = (OP_CODE)( OP_NOT_EQUAL_INT + (num_type - DT_INT) );
    }
    return op_to;
}

int Source__Declarations_fore_action(int* return_values_ptr)
{
    return 0;
}

int Source__Declarations_post_action(int* return_values_ptr)
{
    // find main function
    int main_entrance = -1;
    for (Function& function : Function::function_table) {
        if (function.name == "main") {
            main_entrance = function.entry_address;
            break;
        }
    }
    if (main_entrance > -1) {
        // have main
        for (Function& function : Function::function_table) {
            ++function.entry_address;
        }
    }

    quaternion_sequence.insert(quaternion_sequence.begin(), Quaternion{OP_JMP, -1, -1, main_entrance + 1});

    return 0;
}

static DATA_TYPE_ENUM current_data_type;

int VarDeclaration__Type_DeclaredVars_Semicolon_fore_action(int* return_values_ptr)
{
    semantic_action_stack.push_back(0); // means Type is define var, not function return value
    return -1;
}

int VarDeclaration__Type_DeclaredVars_Semicolon_post_action(int* return_values_ptr)
{
    // fill all the undecided vars
//    int search_idx = analyse_symbol_stack.size() - 1;
//
//    while (search_idx >= 0 && symbol_table[analyse_symbol_stack[search_idx].symbol_index].data_type == DT_NOT_DECIDED) {
//        symbol_table[analyse_symbol_stack[search_idx].symbol_index].data_type = static_cast<DATA_TYPE_ENUM>(return_values_ptr[0]);
//        --search_idx;
//    }

    return 0;
}

int DeclaredVar__Variable_fore_action(int* return_values_ptr)
{
    semantic_action_stack.push_back(0); // means create a new var
    return 0;
}

int DeclaredVar__Variable_post_action(int* return_values_ptr)
{
    symbol_table[return_values_ptr[0]].data_type = current_data_type;
    analyse_symbol_stack.push_back(StackEntry{return_values_ptr[0], current_layer});
    return return_values_ptr[0];
}

int DeclaredVar__Variable_Assignment_Expr_fore_action(int* return_values_ptr)
{
    semantic_action_stack.push_back(0); // means create a new var
    return 0;
}

int DeclaredVar__Variable_Assignment_Expr_post_action(int* return_values_ptr)
{
    symbol_table[return_values_ptr[0]].data_type = current_data_type;
    symbol_table[return_values_ptr[0]].is_initial = true;
    analyse_symbol_stack.push_back(StackEntry{return_values_ptr[0], current_layer});

    // check if need auto cast
    if (symbol_table[return_values_ptr[2]].data_type == symbol_table[return_values_ptr[0]].data_type) {
        quaternion_sequence.push_back(Quaternion{OP_ASSIGNMENT, return_values_ptr[2], -1, return_values_ptr[0]});
    }
    else {
        // need auto cast
        // check can cast
        int cast_result_idx = auto_cast(return_values_ptr[2], symbol_table[return_values_ptr[0]].data_type);
        if (cast_result_idx >= 0) {
            // can cast
            quaternion_sequence.push_back(Quaternion{OP_ASSIGNMENT, cast_result_idx, -1, return_values_ptr[0]});
        }
        else {
            // cannot cast
            // todo throw error : CAST ERROR
            Diagnose::printError(Node::current_node->offset, "Can not cast " + DATA_TYPE_TOKEN[symbol_table[return_values_ptr[0]].data_type] + " from " + DATA_TYPE_TOKEN[symbol_table[return_values_ptr[2]].data_type] + '.');
            exit(-1);
        }
    }
    return return_values_ptr[0];
}

int FunDeclaration__Type_Function_LeftBrace_HereIsParameter_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_fore_action(int* return_values_ptr)
{
    // create function table
    Function new_function;
    new_function.entry_address = quaternion_sequence.size();
    Function::function_table.push_back(new_function);

    semantic_action_stack.push_back(0); // passing arg to Function nonterminal,means it is declaration
    semantic_action_stack.push_back(1); // passing arg to Type, means it is function return type

    ++current_layer;
    return 0;
}

void clear_symbol_stack()
{
    while (!analyse_symbol_stack.empty() && analyse_symbol_stack.back().code_segment_layer == current_layer) {
        analyse_symbol_stack.pop_back();
    }
}

int FunDeclaration__Type_Function_LeftBrace_HereIsParameter_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr)
{
//    Function::function_table.back().return_data_type = static_cast<DATA_TYPE_ENUM>(return_values_ptr[0]);

    // clear stack on current layer
    clear_symbol_stack();

    --current_layer;

    quaternion_sequence.push_back(Quaternion{OP_RETURN, -1, -1, -1});
    return Function::function_table.size() - 1;
}

int FunDeclaration__Type_Function_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_fore_action(int* return_values_ptr)
{
    // create function table
    Function new_function;
    new_function.entry_address = quaternion_sequence.size();
    Function::function_table.push_back(new_function);

    semantic_action_stack.push_back(0); // passing arg to Function nonterminal,means it is declaration
    semantic_action_stack.push_back(1); // passing arg to Type, means it is function return type

    Function::function_table.back().parameter_types.clear();
    ++current_layer;
    return 0;
}

int FunDeclaration__Type_Function_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr)
{
//    Function::function_table.back().return_data_type = static_cast<DATA_TYPE_ENUM>(return_values_ptr[0]);

    // clear stack on current layer
    clear_symbol_stack();

    --current_layer;
    quaternion_sequence.push_back(Quaternion{OP_RETURN, -1, -1, -1});
    return Function::function_table.size() - 1;
}

int Type__int_post_function(int* return_values_ptr)
{
    int arg = semantic_action_stack.back();
    assert(!semantic_action_stack.empty());
    semantic_action_stack.pop_back();

    if (arg == 1) {
        // function return type
        Function::function_table.back().return_data_type = DT_INT;
    }
    else {
        current_data_type = DT_INT;
    }
    return DT_INT;
}

int Type__float_post_function(int* return_values_ptr)
{
    int arg = semantic_action_stack.back();
    assert(!semantic_action_stack.empty());
    semantic_action_stack.pop_back();

    if (arg == 1) {
        // function return type
        Function::function_table.back().return_data_type = DT_FLOAT;
    }
    else {
        current_data_type = DT_FLOAT;
    }
    return DT_FLOAT;
}

int Type__void_post_function(int* return_values_ptr)
{
    int arg = semantic_action_stack.back();
    assert(!semantic_action_stack.empty());
    semantic_action_stack.pop_back();

    if (arg == 1) {
        // function return type
        Function::function_table.back().return_data_type = DT_VOID;
    }
    else {
        current_data_type = DT_VOID;
    }
    return DT_VOID;
}

int Type__double_post_function(int* return_values_ptr)
{
    int arg = semantic_action_stack.back();
    assert(!semantic_action_stack.empty());
    semantic_action_stack.pop_back();

    if (arg == 1) {
        // function return type
        Function::function_table.back().return_data_type = DT_DOUBLE;
    }
    else {
        current_data_type = DT_DOUBLE;
    }
    return DT_DOUBLE;
}

int Type__bool_post_function(int* return_values_ptr)
{
    int arg = semantic_action_stack.back();
    assert(!semantic_action_stack.empty());
    semantic_action_stack.pop_back();

    if (arg == 1) {
        // function return type
        Function::function_table.back().return_data_type = DT_BOOL;
    }
    else {
        current_data_type = DT_BOOL;
    }
    return DT_BOOL;
}

int Function__id_post_function(int* return_values_ptr)
{
    int arg = semantic_action_stack.back();
    assert(!semantic_action_stack.empty());
    semantic_action_stack.pop_back();

    if (arg == 0) {
        // it is declare
        Function::function_table.back().name = Node::current_node->child_nodes_ptr[0]->content;
        return 0;
    }
    else {
        // it is call
        // find function index according to id's content
        string call_name = Node::current_node->child_nodes_ptr[0]->content;
        for (int i = Function::function_table.size() - 1; i >= 0; --i) {
            if (Function::function_table[i].name == call_name) {
                // find
                return i;
            }
        }

        // not found
        // todo throw error FUNCTION NAME NOT FOUND
        return -2;
    }
}

int HereIsParameter__void_post_function(int* return_values_ptr)
{
    Function::function_table.back().parameter_types.clear();
    return 0;
}

int HereIsParameter__Parameters_post_function(int* return_values_ptr)
{
    int param = 0;

    vector<DATA_TYPE_ENUM> reverse_param_type;
    for (int i = analyse_symbol_stack.size() - 1; i >= 0; --i) {
        if (analyse_symbol_stack[i].code_segment_layer != current_layer) {
            break;
        }
        reverse_param_type.push_back(symbol_table[analyse_symbol_stack[i].symbol_index].data_type);
        ++param;
    }

    assert(Function::function_table.back().parameter_types.empty());
    for (int i = reverse_param_type.size() - 1; i >= 0; --i) {
        Function::function_table.back().parameter_types.push_back(reverse_param_type[i]);
    }

    return param;
}

int Parameters__Parameter_Comma_Parameters_post_function(int* return_values_ptr)
{
    return return_values_ptr[0];
}

int Parameters__Parameter_post_function(int* return_values_ptr)
{
    return return_values_ptr[0];
}
int Parameter__Type_Variable_fore_function(int* return_values_ptr)
{
    semantic_action_stack.push_back(0); // means variable is creating
    semantic_action_stack.push_back(0);  // means Type is define var, not function return value

    return 0;
}

int Parameter__Type_Variable_post_function(int* return_values_ptr)
{
    // add new parameter
    // write type to the top of symbol table
    symbol_table[return_values_ptr[1]].data_type = static_cast<DATA_TYPE_ENUM>(return_values_ptr[0]);
    analyse_symbol_stack.push_back(StackEntry{return_values_ptr[1], current_layer});

    return symbol_table.size() - 1;
}

int Variable__id_post_function(int* return_values_ptr)
{
    int arg = semantic_action_stack.back();
    assert(!semantic_action_stack.empty());
    semantic_action_stack.pop_back();

    if (arg == 0) {
        // add new var
        SymbolEntry new_symbol;
        new_symbol.content = Node::current_node->child_nodes_ptr[0]->content;
        new_symbol.is_temp = false;
        new_symbol.is_const = false;
        new_symbol.is_array = false;
        new_symbol.function_index = (current_layer == 0 ? -1 : int(Function::function_table.size() - 1));
        new_symbol.is_used = false;
        new_symbol.is_initial = false;
        new_symbol.node_ptr = Node::current_node->child_nodes_ptr[0];
        new_symbol.last_use_offset = new_symbol.node_ptr->offset;

        symbol_table.push_back(new_symbol);

        return symbol_table.size() - 1;
    }
    else {
        // find var index in stack
        int searching_idx = analyse_symbol_stack.size() - 1;
        while (searching_idx >= 0) {
            if (symbol_table[analyse_symbol_stack[searching_idx].symbol_index].content == Node::current_node->child_nodes_ptr[0]->content) {
                // find
                SymbolEntry& symbolEntry = symbol_table[analyse_symbol_stack[searching_idx].symbol_index];
//                if (!symbolEntry.is_temp && !symbolEntry.is_initial) {
//                    Diagnose::printWarning(symbolEntry.node_ptr->offset, "Variable " + symbolEntry.content + " used without initialized.");
//                }
                symbolEntry.is_used = true;
                symbolEntry.last_use_offset = Node::current_node->offset;
                return analyse_symbol_stack[searching_idx].symbol_index;
            }
            --searching_idx;
        }

        // not found
        // todo throw error : SYMBOL NOT FOUND
        Diagnose::printError(Node::current_node->offset, "Symbol " + Node::current_node->child_nodes_ptr[0]->content + " not defined.");
        exit(-1);
        return -2;
    }

}

int Variable__Variable_LeftSquareBrace_Variable_RightSquareBrace_post_function(int* return_values_ptr)
{
    int arg = semantic_action_stack.back();
    assert(!semantic_action_stack.empty());
    semantic_action_stack.pop_back();

    if (arg == 0) {
        // add new var
        SymbolEntry new_symbol;
        new_symbol.content = Node::current_node->child_nodes_ptr[0]->content;
        new_symbol.is_temp = false;
        new_symbol.is_const = false;
        new_symbol.is_array = true;
        new_symbol.function_index = (current_layer == 0 ? -1 : int(Function::function_table.size() - 1));
        new_symbol.is_used = false;
        new_symbol.is_initial = false;
        new_symbol.node_ptr = Node::current_node->child_nodes_ptr[0];
        new_symbol.last_use_offset = new_symbol.node_ptr->offset;

        symbol_table.push_back(new_symbol);

        return symbol_table.size() - 1;
    }
    else {
        // find var index in stack
        int searching_idx = analyse_symbol_stack.size() - 1;
        while (searching_idx >= 0) {
            if (symbol_table[analyse_symbol_stack[searching_idx].symbol_index].content == Node::current_node->child_nodes_ptr[0]->content) {
                // find
                SymbolEntry& symbolEntry = symbol_table[analyse_symbol_stack[searching_idx].symbol_index];
//                if (!symbolEntry.is_temp && !symbolEntry.is_initial) {
//                    Diagnose::printWarning(symbolEntry.node_ptr->offset, "Variable " + symbolEntry.content + " used without initialized.");
//                }
                symbolEntry.is_used = true;
                symbolEntry.last_use_offset = Node::current_node->offset;
                return analyse_symbol_stack[searching_idx].symbol_index;
            }
            --searching_idx;
        }

        // not found
        // todo throw error : SYMBOL NOT FOUND
        Diagnose::printError(Node::current_node->offset, "Symbol " + Node::current_node->child_nodes_ptr[0]->content + " not defined.");
        exit(-1);
        return -2;
    }
}

int Call__Function_LeftBrace_HereIsArgument_RightBrace_fore_function(int* return_values_ptr)
{
    // push 1 into stack means call a function
    semantic_action_stack.push_back(1);

    return 0;
}

int Call__Function_LeftBrace_HereIsArgument_RightBrace_post_function(int* return_values_ptr)
{
    // check args are matching parameters in definition
    Function& current_function = Function::function_table[return_values_ptr[0]];
//    cout << current_function.name << " provide args " << return_values_ptr[2] << endl;
    if (return_values_ptr[2] != current_function.parameter_types.size()) {
        // todo throw error: NOT MATCH FUNCTION DECLARE
        Diagnose::printError(Node::current_node->offset, "Function arguments not matched.");
        exit(-1);
    }
    // check type matching
    int offset = 0;
    for (int i = 0; i < return_values_ptr[2]; ++i) {
        Quaternion& current_quaternion = quaternion_sequence[quaternion_sequence.size() - 1 - i - offset];
        if (current_quaternion.op_code != OP_PAR || symbol_table[current_quaternion.opr1].data_type != current_function.parameter_types[i]) {
            // auto cast
            OP_CODE cast_op = auto_cast_table[symbol_table[current_quaternion.opr1].data_type][current_function.parameter_types[i]];
            if (cast_op == OP_INVALID) {
                // todo throw error: NOT MATCH FUNCTION DECLARE
                Diagnose::printError(Node::current_node->offset, "Function declaration not matched.");
                exit(-1);
            }
            else if (symbol_table[current_quaternion.opr1].data_type > current_function.parameter_types[i]) {
                Diagnose::printWarning(symbol_table[current_quaternion.opr1].last_use_offset, "Narrow conversion from " + DATA_TYPE_TOKEN[symbol_table[current_quaternion.opr1].data_type] + " to " + DATA_TYPE_TOKEN[current_function.parameter_types[i]] + '.');
            }

            // can be auto cast
            int new_symbol_idx = get_temp_symbol(current_function.parameter_types[i]);
            quaternion_sequence.insert(quaternion_sequence.end() - i - 1 - offset, Quaternion{cast_op, current_quaternion.opr1, -1, new_symbol_idx});
            assert(quaternion_sequence[quaternion_sequence.size() - 1 - i - offset].op_code == OP_PAR);
            quaternion_sequence[quaternion_sequence.size() - 1 - i - offset].opr1 = new_symbol_idx;
            ++offset;
        }
    }

    // all matched
    // create a return temp var
    int return_temp_var = get_temp_symbol(Function::function_table[return_values_ptr[0]].return_data_type);

    Quaternion call_quaternion{OP_CALL, return_values_ptr[0], return_values_ptr[2], return_temp_var};
    quaternion_sequence.push_back(call_quaternion);

    return return_temp_var;
}

int Call__Function_LeftBrace_RightBrace_post_function(int* return_values_ptr)
{
    if (!Function::function_table[return_values_ptr[0]].parameter_types.empty()) {
        Diagnose::printError(Node::current_node->offset, "Function " + Function::function_table[return_values_ptr[0]].name + " requires arguments.");
    }

    // create a return temp var
    int return_temp_var = get_temp_symbol(Function::function_table[return_values_ptr[0]].return_data_type);

    Quaternion call_quaternion{OP_CALL, return_values_ptr[0], 0, return_temp_var};
    quaternion_sequence.push_back(call_quaternion);

    return return_temp_var;
}

int HereIsArgument__void_post_function(int* return_values_ptr)
{
    return 0; // return the num of args provided
}

int HereIsArgument__Arguments_post_action(int* return_values_ptr)
{
    return return_values_ptr[0]; // return the num of args provided
}

int Arguments__Argument_Comma_Arguments_post_function(int* return_values_ptr)
{
    // emit PAR quaternion
    // Argument post function returns the index of symbol
    Quaternion new_par_quaternion{OP_PAR, return_values_ptr[0], -1, -1};
    quaternion_sequence.push_back(new_par_quaternion);

    return return_values_ptr[2] + 1;
}

int Arguments__Argument_post_function(int* return_values_ptr)
{
    // emit PAR quaternion
    // Argument post function returns the index of symbol
    Quaternion new_par_quaternion{OP_PAR, return_values_ptr[0], -1, -1};
    quaternion_sequence.push_back(new_par_quaternion);

    return 1;
}

int Argument__Variable_fore_function(int* return_values_ptr)
{
    semantic_action_stack.push_back(1); // means use var
    return 0;
}

int Argument__Variable_post_function(int* return_values_ptr)
{
    return return_values_ptr[0];
}

int Comparison__Comparison_CmpSymbol_Addition_post_action(int* return_values_ptr)
{
    int symbol_table_idx_1 = return_values_ptr[0];
    int symbol_table_idx_2 = return_values_ptr[2];
    DATA_TYPE_ENUM type1 = symbol_table[symbol_table_idx_1].data_type;
    DATA_TYPE_ENUM type2 = symbol_table[symbol_table_idx_2].data_type;
    OP_CODE op_type = (OP_CODE)return_values_ptr[1];
    if(type1 < DT_BOOL || type2 < DT_BOOL)
    {
        Diagnose::printError(Node::current_node->offset, "Illegal Void Calculation.");
        exit(-1);
    }
    int ret = get_temp_symbol(DT_BOOL);
    DATA_TYPE_ENUM dt_type = (DATA_TYPE_ENUM) max(type1, type2);
    symbol_table_idx_1 = Type_Conversion(type1, dt_type, return_values_ptr[0]);
    symbol_table_idx_2 = Type_Conversion(type2, dt_type, return_values_ptr[2]);
    op_type = Op_Type_Conversion(op_type, dt_type);
    emit(op_type, symbol_table_idx_1, symbol_table_idx_2, ret);
    return ret;
}

int CmpSymbol__Equal_post_action(int* return_values_ptr)
{
    return OP_EQUAL_INT;
}

int CmpSymbol__Greater_post_action(int* return_values_ptr)
{
    return OP_GREATER_INT;
}

int CmpSymbol__GreaterEqual_post_action(int* return_values_ptr)
{
    return OP_GREATER_EQUAL_INT;
}

int CmpSymbol__Less_post_action(int* return_values_ptr)
{
    return OP_LESS_INT;
}

int CmpSymbol__LessEqual_post_action(int* return_values_ptr)
{
    return OP_LESS_EQUAL_INT;
}

int CmpSymbol__NotEqual_post_action(int* return_values_ptr)
{
    return OP_NOT_EQUAL_INT;
}

int Addition__Addition_AddSymbol_Multiplication_post_action(int* return_values_ptr)
{
    int symbol_table_idx_1 = return_values_ptr[0];
    int symbol_table_idx_2 = return_values_ptr[2];
    DATA_TYPE_ENUM type1 = symbol_table[symbol_table_idx_1].data_type;
    DATA_TYPE_ENUM type2 = symbol_table[symbol_table_idx_2].data_type;
    OP_CODE op_type = (OP_CODE)return_values_ptr[1];
    if(type1 < DT_BOOL || type2 < DT_BOOL)
    {
        Diagnose::printError(Node::current_node->offset, "Illegal Void Calculation.");
        exit(-1);
    }
    int ret;
    DATA_TYPE_ENUM dt_type = (DATA_TYPE_ENUM) max(type1, type2);
    symbol_table_idx_1 = Type_Conversion(type1, dt_type, return_values_ptr[0]);
    symbol_table_idx_2 = Type_Conversion(type2, dt_type, return_values_ptr[2]);
    ret = get_temp_symbol(dt_type);
    op_type = Op_Type_Conversion(op_type, dt_type);
    emit(op_type, symbol_table_idx_1, symbol_table_idx_2, ret);
    return ret;
}

int AddSymbol__Add_post_action(int* return_values_ptr)
{
    return OP_ADD_INT;
}

int AddSymbol__Minus_post_action(int* return_values_ptr)
{
    return OP_MINUS_INT;
}

int Multiplication__Multiplication_MulSymbol_Item_post_action(int* return_values_ptr)
{
    int symbol_table_idx_1 = return_values_ptr[0];
    int symbol_table_idx_2 = return_values_ptr[2];
    DATA_TYPE_ENUM type1 = symbol_table[symbol_table_idx_1].data_type;
    DATA_TYPE_ENUM type2 = symbol_table[symbol_table_idx_2].data_type;
    OP_CODE op_type = (OP_CODE)return_values_ptr[1];
    if(type1 < DT_BOOL || type2 < DT_BOOL)
    {
        Diagnose::printError(Node::current_node->offset, "Illegal Void Calculation.");
        exit(-1);
    }
    int ret;
    DATA_TYPE_ENUM dt_type = (DATA_TYPE_ENUM) max(type1, type2);
    symbol_table_idx_1 = Type_Conversion(type1, dt_type, return_values_ptr[0]);
    symbol_table_idx_2 = Type_Conversion(type2, dt_type, return_values_ptr[2]);
    ret = get_temp_symbol(dt_type);
    op_type = Op_Type_Conversion(op_type, dt_type);
    emit(op_type, symbol_table_idx_1, symbol_table_idx_2, ret);
    return ret;
}

int MulSymbol__Multi_post_action(int* return_values_ptr)
{
    return OP_MULTI_INT;
}

int MulSymbol__DIV_post_action(int* return_values_ptr)
{
    return OP_DIV_INT;
}

int Item__Neg_Item_post_action(int* return_values_ptr)
{
    int ret = get_temp_symbol(symbol_table[return_values_ptr[1]].data_type);
    emit(OP_NEG, return_values_ptr[1], -1, ret);
    return ret;
}

int Item__LeftBrace_Expr_RightBrace_post_function(int* return_values_ptr)
{
    return return_values_ptr[1];
}

int Comma__Comma_Comma_Assignment_post_action(int* return_values_ptr)
{
    return return_values_ptr[2];
}

int Assignment__LogicalOr_ASSIGNMENT_Assignment_post_action(int* return_values_ptr)
{
    if (symbol_table[return_values_ptr[0]].is_const) {
        Diagnose::printError(Node::current_node->offset, "Expression is not assignable.");
    }

    int symbol_table_idx_1 = return_values_ptr[0];
    symbol_table[symbol_table_idx_1].is_initial = true;
    int symbol_table_idx_2 = return_values_ptr[2];
    DATA_TYPE_ENUM type1 = symbol_table[symbol_table_idx_1].data_type;
    DATA_TYPE_ENUM type2 = symbol_table[symbol_table_idx_2].data_type;
    if (type1 != type2) {
        // need cast
        int cast_result_index = auto_cast(symbol_table_idx_2, type1);
        if (cast_result_index < 0) {
            Diagnose::printError(Node::current_node->offset, "Can not cast " + DATA_TYPE_TOKEN[type1] + " from " + DATA_TYPE_TOKEN[type2] + '.');
            exit(-1);
        }
        emit(OP_CODE::OP_ASSIGNMENT, cast_result_index, -1, symbol_table_idx_1);
    }
    else {
        emit(OP_CODE::OP_ASSIGNMENT, symbol_table_idx_2, -1, symbol_table_idx_1);
    }
    return symbol_table_idx_1;
}

int Left__LeftBrace_Left_ASSIGNMENT_Expr_RightBrace_post_action(int* return_values_ptr)
{
    int symbol_table_idx_1 = return_values_ptr[1];
    int symbol_table_idx_2 = return_values_ptr[3];
    DATA_TYPE_ENUM type1 = symbol_table[symbol_table_idx_1].data_type;
    DATA_TYPE_ENUM type2 = symbol_table[symbol_table_idx_2].data_type;
    if (type1 != type2) {
        // need cast
        int cast_result_index = auto_cast(symbol_table_idx_2, type1);
        if (cast_result_index < 0) {
            Diagnose::printError(Node::current_node->offset, "Can not cast " + DATA_TYPE_TOKEN[type1] + " from " + DATA_TYPE_TOKEN[type2] + '.');
            exit(-1);
        }
        emit(OP_CODE::OP_ASSIGNMENT, cast_result_index, -1, symbol_table_idx_1);
    }
    else {
        emit(OP_CODE::OP_ASSIGNMENT, symbol_table_idx_2, -1, symbol_table_idx_1);
    }
    return symbol_table_idx_1;
}

int Left__LeftBrace_Comma_Comma_Left_RightBrace_post_action(int* return_values_ptr)
{
    return return_values_ptr[3];
}

int Return__return_Expr_Semicolon_post_action(int* return_values_ptr)
{
    if (symbol_table[return_values_ptr[1]].data_type != Function::function_table.back().return_data_type) {
        // need cast
        int cast_result_index = auto_cast(return_values_ptr[1], Function::function_table.back().return_data_type);
        if (cast_result_index < 0) {
            Diagnose::printError(Node::current_node->offset, "Can not cast " + DATA_TYPE_TOKEN[Function::function_table.back().return_data_type] + " from " + DATA_TYPE_TOKEN[symbol_table[return_values_ptr[1]].data_type] + '.');
            exit(-1);
        }
        emit(OP_CODE::OP_RETURN, cast_result_index, -1, -1);
    }
    else {
        emit(OP_CODE::OP_RETURN, return_values_ptr[1], -1, -1);
    }
    return return_values_ptr[1];
}

int Return__return_Semicolon_post_action(int* return_values_ptr)
{
    if (Function::function_table.back().return_data_type != DT_VOID) {
        Diagnose::printError(Node::current_node->offset, "Function " + Function::function_table.back().name + " needs a return value " + DATA_TYPE_TOKEN[Function::function_table.back().return_data_type] + '.');
//        exit(-1);
    }

    emit(OP_CODE::OP_RETURN, -1, -1, -1);
    return -1;
}

int Compound__LeftCurlyBrace_Statements_RightCurlyBrace_fore_action(int* return_values_ptr)
{
    ++current_layer;

    return 0;
}

int Compound__LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr)
{
    clear_symbol_stack();

    --current_layer;

    return return_values_ptr[1];
}

int Num__num_post_action(int* return_values_ptr)
{
    string str = Node::current_node->child_nodes_ptr[0]->content;
    bool is_INT = true;
    for(int i = 0; i < str.length(); i++)
    {
        if(str[i] == '.')
        {
            is_INT = false;
            break;
        }
    }
    /*
    double num_double = 123.456;
    int arr[2];
    arr[0] = *((int *)&num_double);
    arr[1] = *((int *)&num_double + 1);
    double new_num_double = *((double *)arr);
    */


    int ret;
    if(is_INT)
    {
        int num = stoi(str);
        ret = get_temp_symbol(DT_INT);
        emit(OP_LI_INT, num, -1, ret);
    }
    else if (str.back() == 'f' || str.back() == 'F')
    {
        float num = stof(str);
        int int_num = *((int*)&num);

        ret = get_temp_symbol(DT_FLOAT);
        emit(OP_LI_FLOAT, int_num, -1, ret);

    }
    else {
        double num = stod(str);
        int arr[2];
        arr[0] = *((int *)&num);
        arr[1] = *((int *)&num + 1);

        ret = get_temp_symbol(DT_DOUBLE);
        emit(OP_LI_DOUBLE, arr[0], arr[1], ret);
    }
    return ret;
}

int Num__true_post_action(int* return_values_ptr)
{
    int ret = get_temp_symbol(DT_BOOL);
    emit(OP_LI_BOOL, true, -1, ret);

    return ret;
}

int Num__false_post_action(int* return_values_ptr)
{
    int ret = get_temp_symbol(DT_BOOL);
    emit(OP_LI_BOOL, false, -1, ret);

    return ret;
}