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

vector<Function> Function::function_table;
vector<Quaternion> quaternion_sequence;
vector<SymbolEntry> symbol_table;
vector<StackEntry> analyse_symbol_stack;
int current_layer = 0;
bool in_if_while_condition = false;
//vector<int> semantic_action_stack;

// Semantic part
Node* Node::current_node;
int Node::semantic_action()
{
    Node::current_node = this;
    int* return_values = new int[child_nodes_ptr.size()];
    // ignore return value
    this->quad = quaternion_sequence.size();
//    (*action_function_ptr[this->non_terminal_idx][this->reduction_idx][0])(nullptr);

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

int Node::semantic_action_one_pass(int* child_return_value)
{
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
    int ret = (*action_function_ptr[this->non_terminal_idx][this->reduction_idx][1])(child_return_value);
    this->nextlist = quaternion_sequence.size();

    return ret;
}

void semantic_analysis(Node* root)
{
    for (Nonterminal& nonterminal : Nonterminal::all_nonterminal_chars) {
        cout << nonterminal.token << endl;
    }
    root->semantic_action();
    check_unused();
    Diagnose::printStream();
}

void semantic_analysis_post()
{
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


void print_quaternion_sequence(vector<Quaternion> sequence)
{
    // print quaternion
    cout << "Quaternion sequence" << endl;
    int cnt = 0;
    for (int i = 0; i < sequence.size(); ++i) {
        Quaternion& quaternion = sequence[i];
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

//            case OP_FETCH_BOOL:
//            case OP_FETCH_INT:
//            case OP_FETCH_FLOAT:
//            case OP_FETCH_DOUBLE:
//            case OP_ARRAY_STORE:
//            {
//                cout << symbol_table[quaternion.opr1].content << "\t" << quaternion.opr2 << "\t" << symbol_table[quaternion.result].content << endl;
//                break;
//            };

            default:
                cout << (quaternion.opr1 < 0 ? "-" : symbol_table[quaternion.opr1].content) << '[' << quaternion.opr1 << ']' << "\t" << (quaternion.opr2 < 0 ? "-" : symbol_table[quaternion.opr2].content) << '[' << quaternion.opr2 << ']' << "\t" << (quaternion.result < 0 ? "-" : symbol_table[quaternion.result].content)  << '[' << quaternion.result << ']' << endl;
                break;
        }
        ++cnt;
    }

    // print symbol table
    cout << "Symbol table" << endl;
    cnt = 0;
    for (SymbolEntry& symbolEntry : symbol_table) {
        string value_str;
        switch (symbolEntry.data_type) {
            case DT_BOOL:
                value_str = symbolEntry.value.bool_value ? "true" : "false";
                break;

            case DT_INT:
                value_str = to_string(symbolEntry.value.int_value);
                break;

            case DT_FLOAT:
                value_str = to_string(symbolEntry.value.float_value);
                break;

            case DT_DOUBLE:
                value_str = to_string(symbolEntry.value.double_value);
                break;

            default:
                value_str = "";
        }
        cout << setiosflags(ios::left) << setw(6) << cnt << setiosflags(ios::left) << setw(12) << symbolEntry.content << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << DATA_TYPE_TOKEN[symbolEntry.data_type] << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_array ? "Array" : "Scalar") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << symbolEntry.memory_size << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_temp ? "Temp" : "Declare") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) <<
             (symbolEntry.function_index < 0 ? "Global" : Function::function_table[symbolEntry.function_index].name) << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_used ? "Used" : "Unused") << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << (symbolEntry.is_initial ? "Initialed" : "Uninitiated") << '\t' << (symbolEntry.is_const ? "Const" : "Variable") << '\t' <<  value_str << endl;
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

            case OP_LI_FLOAT: {
                float float_num = *((float*)(&quaternion.opr1));
                fout << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << float_num << setiosflags(ios::left) << setw(DISPLAY_WIDTH) << symbol_table[quaternion.result].content << endl;
                break;
            }

//            case OP_FETCH_BOOL:
//            case OP_FETCH_INT:
//            case OP_FETCH_FLOAT:
//            case OP_FETCH_DOUBLE:
//            case OP_ARRAY_STORE:
//            {
//                fout << symbol_table[quaternion.opr1].content << "\t" << quaternion.opr2 << "\t" << symbol_table[quaternion.result].content << endl;
//                break;
//            };

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
int get_temp_symbol(DATA_TYPE_ENUM data_type, bool is_const)
{
    SymbolEntry temp_symbol;
    temp_symbol.content = "temp_" + to_string(temp_symbol_cnt);
    ++temp_symbol_cnt;
    temp_symbol.is_temp = true;
    temp_symbol.is_const = is_const;
    temp_symbol.data_type = data_type;
    temp_symbol.memory_size = BASE_DATA_TYPE_SIZE[(int)data_type];
    temp_symbol.is_array = false;
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

int LeftCurlyBraceFore_left_curly_brace_post_action(int* return_values_ptr)
{
    ++current_layer;
    return 0;
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
        int new_symbol_idx = get_temp_symbol(dst_data_type, symbol_table[src_index].is_const);
        quaternion_sequence.push_back(Quaternion{cast_op, src_index, -1, new_symbol_idx});
        return new_symbol_idx;
    }
}

int Type_Conversion(DATA_TYPE_ENUM type_from, DATA_TYPE_ENUM type_to, int symbol_table_idx)
{
    if(type_from == type_to)        // no need to converse
        return symbol_table_idx;
    int ret = get_temp_symbol(type_to, symbol_table[symbol_table_idx].is_const);

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

bool current_type_is_const;
//int VarDeclaration__Type_DeclaredVars_Semicolon_fore_action(int* return_values_ptr)
//{
//    Node::current_node->child_nodes_ptr[0]->attributes.emplace(boolTypeUsageAttr, boolTypeUsage_VarDeclare);
//    current_type_is_const = false;
//    return -1;
//}

int VarDeclaration_Const_Type_DeclaredVars_Semicolon_post_action(int* return_values_ptr)
{
    current_type_is_const = false;

    return -1;
}

int VarDeclaration__Type_DeclaredVars_Semicolon_post_action(int* return_values_ptr)
{
    current_type_is_const = false;
    if (return_values_ptr[0] == DT_VOID) {
        Diagnose::printError(Node::current_node->offset, "Cannot use void as a data type of variable.");
    }
    // fill all the undecided vars
//    int search_idx = analyse_symbol_stack.size() - 1;
//
//    while (search_idx >= 0 && symbol_table[analyse_symbol_stack[search_idx].symbol_index].data_type == DT_NOT_DECIDED) {
//        symbol_table[analyse_symbol_stack[search_idx].symbol_index].data_type = static_cast<DATA_TYPE_ENUM>(return_values_ptr[0]);
//        --search_idx;
//    }

    return 0;
}

//int DeclaredVar__Variable_fore_action(int* return_values_ptr)
//{
//    Node::current_node->child_nodes_ptr[0]->attributes.emplace(boolVarUsageAttr, boolVarUsage_Declare);
////    semantic_action_stack.push_back(0); // means create a new var
//    return 0;
//}

int create_new_symbol(Node* id_ptr, DATA_TYPE_ENUM data_type, bool is_const)
{
    // add new var
    SymbolEntry new_symbol;
    new_symbol.content = id_ptr->content;
    cout << id_ptr->content << " Content" << endl;
    new_symbol.is_temp = false;
    new_symbol.is_const = is_const;
    new_symbol.is_array = false;
    new_symbol.data_type = data_type;
    new_symbol.memory_size = BASE_DATA_TYPE_SIZE[(int)data_type];
//        Node::current_node->attributes.emplace(intMemorySizeAttr, BASE_DATA_TYPE_SIZE[(int)current_data_type]);

    new_symbol.function_index = (current_layer == 0 ? -1 : int(Function::function_table.size() - 1));
    new_symbol.is_used = false;
    new_symbol.is_initial = false;
    new_symbol.node_ptr = id_ptr;
    new_symbol.last_use_offset = new_symbol.node_ptr->offset;

    symbol_table.push_back(new_symbol);

    return symbol_table.size() - 1;
}

int DeclaredVar__id_post_action(int* return_values_ptr)
{
//    symbol_table[return_values_ptr[0]].data_type = current_data_type;
//    symbol_table[return_values_ptr[0]].memory_size = Node::current_node->child_nodes_ptr[0]->get_attribute_value(intMemorySizeAttr);
    if (current_type_is_const) {
        Diagnose::printError(Node::current_node->offset, "Uninitialized const " + symbol_table[return_values_ptr[0]].content + '.');
    }

    int new_idx = create_new_symbol(Node::current_node->child_nodes_ptr[0], current_data_type, current_type_is_const);

    analyse_symbol_stack.push_back(StackEntry{new_idx, current_layer});
    return new_idx;
}

void build_array_index_vector(Node* variable_node, vector<int>& index_vector);

int DeclaredVar__id_Indices_post_action(int* return_values_ptr)
{
    // declare a array
    SymbolEntry new_symbol;
    new_symbol.content = Node::current_node->child_nodes_ptr[0]->content;
    new_symbol.is_temp = false;
    new_symbol.is_const = false;
    new_symbol.is_array = true;

    new_symbol.function_index = (current_layer == 0 ? -1 : int(Function::function_table.size() - 1));
    new_symbol.is_used = false;
    new_symbol.is_initial = false;
    new_symbol.node_ptr = Node::current_node;
    new_symbol.last_use_offset = new_symbol.node_ptr->offset;

    // build index vector
    build_array_index_vector(Node::current_node, new_symbol.index_record);

    // calc memory size
//        unsigned int current_size = BASE_DATA_TYPE_SIZE[(int)current_data_type];
//        for (int index_of_index : new_symbol.index_record) {
//            current_size *= symbol_table[index_of_index].value.int_value;
//        }

    new_symbol.data_type = current_data_type;
//        new_symbol.memory_size = current_size;
    new_symbol.memory_size = -1; // the size of array will be calculated in DAG optimization part

    symbol_table.push_back(new_symbol);

    analyse_symbol_stack.push_back({(int)(symbol_table.size() - 1), current_layer});

    return symbol_table.size() - 1;
}

//int DeclaredVar__Variable_Assignment_Expr_fore_action(int* return_values_ptr)
//{
//    Node::current_node->child_nodes_ptr[0]->attributes.emplace(boolVarUsageAttr, boolVarUsage_Declare);
////    semantic_action_stack.push_back(0); // means create a new var
//    return 0;
//}

int DeclaredVar__id_Assignment_Expr_post_action(int* return_values_ptr)
{
    int new_idx = create_new_symbol(Node::current_node->child_nodes_ptr[0], current_data_type, current_type_is_const);

    analyse_symbol_stack.push_back(StackEntry{new_idx, current_layer});

    if (!symbol_table[new_idx].is_array) {
        symbol_table[new_idx].memory_size = BASE_DATA_TYPE_SIZE[(int) current_data_type]; // assign to array is not supported
    }
    else {
        Diagnose::printError(Node::current_node->offset, "Assign to array is not supported");
    }

    symbol_table[new_idx].is_initial = true;

    // check if need auto cast
    if (symbol_table[return_values_ptr[2]].data_type == symbol_table[new_idx].data_type) {
        quaternion_sequence.push_back(Quaternion{OP_ASSIGNMENT, return_values_ptr[2], -1, new_idx});
    }
    else {
        // need auto cast
        // check can cast
        int cast_result_idx = auto_cast(return_values_ptr[2], symbol_table[new_idx].data_type);
        if (cast_result_idx >= 0) {
            // can cast
            quaternion_sequence.push_back(Quaternion{OP_ASSIGNMENT, cast_result_idx, -1, new_idx});
        }
        else {
            // cannot cast
            // todo throw error : CAST ERROR
            Diagnose::printError(Node::current_node->offset, "Can not cast " + DATA_TYPE_TOKEN[symbol_table[new_idx].data_type] + " from " + DATA_TYPE_TOKEN[symbol_table[return_values_ptr[2]].data_type] + '.');
            exit(-1);
        }
    }
    return new_idx;
}

int IdLeftBrace__id_LeftBrace_post_action(int* return_values_ptr)
{
    // create function table
    Function new_function;
    new_function.entry_address = quaternion_sequence.size();
    Function::function_table.push_back(new_function);

//    semantic_action_stack.push_back(0); // passing arg to Function nonterminal,means it is declaration
//    Node::current_node->child_nodes_ptr[1]->attributes.emplace(boolFunctionNameUsageAttr, boolFunctionNameUsage_Declare);

//    semantic_action_stack.push_back(1); // passing arg to Type, means it is function return type
//    Node::current_node->child_nodes_ptr[0]->attributes.emplace(boolTypeUsageAttr, boolTypeUsage_FuncReturn);

    ++current_layer;

    Function::function_table.back().return_data_type = current_data_type;

    Function::function_table.back().name = Node::current_node->child_nodes_ptr[0]->content;

    Function& last_function = Function::function_table.back();
    // check dup
    for (int i = 0; i < Function::function_table.size() - 1; ++i) {
        if (Function::function_table[i].name == last_function.name) {
            Diagnose::printError(Node::current_node->offset, "Function " + last_function.name + " already defined.");
        }
    }
    return 0;
}

void clear_symbol_stack()
{
    while (!analyse_symbol_stack.empty() && analyse_symbol_stack.back().code_segment_layer == current_layer) {
        analyse_symbol_stack.pop_back();
    }
}

int FunDeclaration__Type_FuncName_LeftBrace_HereIsParameter_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr)
{
//    Function::function_table.back().return_data_type = static_cast<DATA_TYPE_ENUM>(return_values_ptr[0]);

    // clear stack on current layer
    clear_symbol_stack();

    --current_layer;

//    Function new_function;
//    new_function.name = Node::current_node->child_nodes_ptr[1]->content;
//
//    Function::function_table.push_back(new_function);

    quaternion_sequence.push_back(Quaternion{OP_RETURN, -1, -1, -1});
    return Function::function_table.size() - 1;
}

//int FunDeclaration__Type_id_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_fore_action(int* return_values_ptr)
//{
//    // create function table
//    Function new_function;
//    new_function.entry_address = quaternion_sequence.size();
//    Function::function_table.push_back(new_function);
//
////    semantic_action_stack.push_back(0); // passing arg to Function nonterminal,means it is declaration
////    Node::current_node->child_nodes_ptr[1]->attributes.emplace(boolFunctionNameUsageAttr, boolFunctionNameUsage_Declare);
//
////    semantic_action_stack.push_back(1); // passing arg to Type, means it is function return type
////    Node::current_node->child_nodes_ptr[0]->attributes.emplace(boolTypeUsageAttr, boolTypeUsage_FuncReturn);
//
//    Function::function_table.back().parameter_types.clear();
//    ++current_layer;
//    return 0;
//}

int FunDeclaration__Type_FuncName_LeftBrace_RightBrace_LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr)
{
//    Function::function_table.back().return_data_type = static_cast<DATA_TYPE_ENUM>(return_values_ptr[0]);
//    Function::function_table.back().return_data_type = static_cast<DATA_TYPE_ENUM>(Node::current_node->child_nodes_ptr[0]->get_attribute_value(enumTypeValueAttr));

    // clear stack on current layer
    clear_symbol_stack();

    --current_layer;
    quaternion_sequence.push_back(Quaternion{OP_RETURN, -1, -1, -1});
    return Function::function_table.size() - 1;
}

int FuncName__id(int* return_values_ptr)
{
    Function::function_table.back().name = Node::current_node->child_nodes_ptr[0]->content;

    Function& last_function = Function::function_table.back();
    // check dup
    for (int i = 0; i < Function::function_table.size() - 1; ++i) {
        if (Function::function_table[i].name == last_function.name) {
            Diagnose::printError(Node::current_node->offset, "Function " + last_function.name + " already defined.");
        }
    }
    return 0;
}

int Type__int_post_function(int* return_values_ptr)
{
//    int arg = Node::current_node->get_attribute_value(boolTypeUsageAttr);
////    int arg = semantic_action_stack.back();
////    assert(!semantic_action_stack.empty());
////    semantic_action_stack.pop_back();
//
//    if (arg == boolTypeUsage_FuncReturn) {
//        // function return type
//        Function::function_table.back().return_data_type = DT_INT;
//    }
//    else {
        current_data_type = DT_INT;
//    }
    Node::current_node->attributes.emplace(enumTypeValueAttr, DT_INT);

    return DT_INT;
}

int Type__float_post_function(int* return_values_ptr)
{
//    int arg = Node::current_node->get_attribute_value(boolTypeUsageAttr);
////    int arg = semantic_action_stack.back();
////    assert(!semantic_action_stack.empty());
////    semantic_action_stack.pop_back();
//
//    if (arg == boolTypeUsage_FuncReturn) {
//        // function return type
//        Function::function_table.back().return_data_type = DT_FLOAT;
//    }
//    else {
        current_data_type = DT_FLOAT;
//    }
    Node::current_node->attributes.emplace(enumTypeValueAttr, DT_FLOAT);

    return DT_FLOAT;
}

int Type__void_post_function(int* return_values_ptr)
{
//    int arg = Node::current_node->get_attribute_value(boolTypeUsageAttr);
////    int arg = semantic_action_stack.back();
////    assert(!semantic_action_stack.empty());
////    semantic_action_stack.pop_back();
//
//    if (arg == boolTypeUsage_FuncReturn) {
//        // function return type
//        Function::function_table.back().return_data_type = DT_VOID;
//    }
//    else {
        current_data_type = DT_VOID;
//    }
    Node::current_node->attributes.emplace(enumTypeValueAttr, DT_VOID);

    return DT_VOID;
}

int Type__double_post_function(int* return_values_ptr)
{
//    int arg = Node::current_node->get_attribute_value(boolTypeUsageAttr);
////    int arg = semantic_action_stack.back();
////    assert(!semantic_action_stack.empty());
////    semantic_action_stack.pop_back();
//
//    if (arg == boolTypeUsage_FuncReturn) {
//        // function return type
//        Function::function_table.back().return_data_type = DT_DOUBLE;
//    }
//    else {
        current_data_type = DT_DOUBLE;
//    }

    Node::current_node->attributes.emplace(enumTypeValueAttr, DT_DOUBLE);
    return DT_DOUBLE;
}

int Type__bool_post_function(int* return_values_ptr)
{
//    int arg = Node::current_node->get_attribute_value(boolTypeUsageAttr);
////    int arg = semantic_action_stack.back();
////    assert(!semantic_action_stack.empty());
////    semantic_action_stack.pop_back();
//
//    if (arg == boolTypeUsage_FuncReturn) {
//        // function return type
//        Function::function_table.back().return_data_type = DT_BOOL;
//    }
//    else {
        current_data_type = DT_BOOL;
//    }
    Node::current_node->attributes.emplace(enumTypeValueAttr, DT_BOOL);
    return DT_BOOL;
}

int Function__id_post_function(int* return_values_ptr)
{
//    int arg = semantic_action_stack.back();
    int arg = Node::current_node->get_attribute_value(boolFunctionNameUsageAttr);
//    assert(!semantic_action_stack.empty());
//    semantic_action_stack.pop_back();

    if (arg == boolFunctionNameUsage_Declare) {
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
//int Parameter__Type_Variable_fore_function(int* return_values_ptr)
//{
////    semantic_action_stack.push_back(0); // means variable is creating
//    Node::current_node->child_nodes_ptr[1]->attributes.emplace(boolVarUsageAttr, boolVarUsage_Declare);
//
////    semantic_action_stack.push_back(0);  // means Type is define var, not function return value
//    Node::current_node->child_nodes_ptr[0]->attributes.emplace(boolTypeUsageAttr, boolTypeUsage_VarDeclare);
//    return 0;
//}

int Parameter__Type_id_post_function(int* return_values_ptr)
{
    if (return_values_ptr[0] == DT_VOID) {
        Diagnose::printError(Node::current_node->offset, "Cannot use void as a data type of parameter.");
    }
    int new_idx = create_new_symbol(Node::current_node->child_nodes_ptr[1], current_data_type, current_type_is_const);

//    analyse_symbol_stack.push_back(StackEntry{new_idx, current_layer});

    // add new parameter
    // write type to the top of symbol table
    symbol_table[new_idx].data_type = static_cast<DATA_TYPE_ENUM>(return_values_ptr[0]);

    if (!symbol_table[new_idx].is_array) {
        symbol_table[new_idx].memory_size = BASE_DATA_TYPE_SIZE[return_values_ptr[0]];
    }

    analyse_symbol_stack.push_back(StackEntry{new_idx, current_layer});

    return symbol_table.size() - 1;
}

int Parameter__Type_id_Indices_post_function(int* return_values_ptr)
{
    // declare a array
    SymbolEntry new_symbol;
    new_symbol.content = Node::current_node->child_nodes_ptr[1]->content;
    new_symbol.is_temp = false;
    new_symbol.is_const = false;
    new_symbol.is_array = true;

    new_symbol.function_index = (current_layer == 0 ? -1 : int(Function::function_table.size() - 1));
    new_symbol.is_used = false;
    new_symbol.is_initial = false;
    new_symbol.node_ptr = Node::current_node;
    new_symbol.last_use_offset = new_symbol.node_ptr->offset;

    // build index vector
    build_array_index_vector(Node::current_node, new_symbol.index_record);

    // calc memory size
//        unsigned int current_size = BASE_DATA_TYPE_SIZE[(int)current_data_type];
//        for (int index_of_index : new_symbol.index_record) {
//            current_size *= symbol_table[index_of_index].value.int_value;
//        }

    new_symbol.data_type = static_cast<DATA_TYPE_ENUM>(return_values_ptr[0]);
//        new_symbol.memory_size = current_size;
    new_symbol.memory_size = -1; // the size of array will be calculated in DAG optimization part

    symbol_table.push_back(new_symbol);

    return symbol_table.size() - 1;
}

int VariableUse__id_post_function(int* return_values_ptr)
{
    // find var index in stack
    int searching_idx = analyse_symbol_stack.size() - 1;
    while (searching_idx >= 0) {
        if (symbol_table[analyse_symbol_stack[searching_idx].symbol_index].content == Node::current_node->child_nodes_ptr[0]->content) {
            // find
            SymbolEntry& symbolEntry = symbol_table[analyse_symbol_stack[searching_idx].symbol_index];
//               if (!symbolEntry.is_temp && !symbolEntry.is_initial) {
//                   Diagnose::printWarning(symbolEntry.node_ptr->offset, "Variable " + symbolEntry.content + " used without initialized.");
//               }
            symbolEntry.is_used = true;
            symbolEntry.last_use_offset = Node::current_node->offset;
            return analyse_symbol_stack[searching_idx].symbol_index;
        }
        --searching_idx;
    }

    // not found
    // todo throw error : SYMBOL NOT FOUND
    // print symbol table
    print_quaternion_sequence(quaternion_sequence);

    Diagnose::printError(Node::current_node->offset, "Symbol " + Node::current_node->child_nodes_ptr[0]->content + " not defined.");
    exit(-1);
    return -2;
}

//int Variable__id_Indices_fore_function(int* return_values_ptr)
//{
//    int arg = Node::current_node->get_attribute_value(boolVarUsageAttr);
//
//    if (arg == boolVarUsage_Declare) {
//        // declare a array
//        if (current_type_is_const) {
//            Diagnose::printError(Node::current_node->offset, "Const array is not supported.");
//        }
//        Node::current_node->child_nodes_ptr[1]->attributes.emplace(boolIndicesUsageAttr, boolIndicesUsage_Declare);
//    }
//    else {
//        // get a reference of a array
//        Node::current_node->child_nodes_ptr[1]->attributes.emplace(boolIndicesUsageAttr, boolIndicesUsage_Reference);
//    }
//
//    return 0;
//}

void build_array_index_vector(Node* variable_node, vector<int>& index_vector)
{
    Node* current_index_node_pointer = variable_node->child_nodes_ptr[1];

    while (true) {
        index_vector.push_back(current_index_node_pointer->get_attribute_value(intIndicesSizeIndexAttr));
        if (current_index_node_pointer->child_nodes_ptr.size() == 4) {
            current_index_node_pointer = current_index_node_pointer->child_nodes_ptr[3];
        }
        else {
            break;
        }
    }
}

// this function will generate quaternions to calc the offset
// return the temp var which is offset
//vector<Quaternion> offset_calc_sequence;
int calc_array_element_offset(vector<int> index_vector, vector<int> length_vector)
{
    assert(index_vector.size() == length_vector.size());

    int new_var = get_temp_symbol(DT_INT, false);

    quaternion_sequence.push_back(Quaternion{OP_ASSIGNMENT, index_vector[0], -1, new_var});

    for (int i = 0; i < length_vector.size() - 1; ++i) {
        quaternion_sequence.push_back(Quaternion{OP_MULTI_INT, new_var, length_vector[i + 1], new_var});

        quaternion_sequence.push_back(Quaternion{OP_ADD_INT, new_var, index_vector[i + 1], new_var});
    }

//    int calc_result = index_vector.back();
//
//    for (int i = length_vector.size() - 1; i >= 1; --i) {
//        calc_result += length_vector[i] * index_vector[i - 1];
//    }

    return new_var;
}

int VariableUse__id_Indices_post_function(int* return_values_ptr)
{

    // reference an element of an array
    int searching_idx = analyse_symbol_stack.size() - 1;
    while (searching_idx >= 0) {
        if (symbol_table[analyse_symbol_stack[searching_idx].symbol_index].content == Node::current_node->child_nodes_ptr[0]->content) {
            // find array
            SymbolEntry& symbolEntry = symbol_table[analyse_symbol_stack[searching_idx].symbol_index];

            // check index valid & calc offset
            vector<int> indices_vector;
            build_array_index_vector(Node::current_node, indices_vector);

            if (indices_vector.size() != symbolEntry.index_record.size()) {
                Diagnose::printError(Node::current_node->offset, "Invalid index/indices to fetch an element from array.");
            }

            int offset_calc_start_index = quaternion_sequence.size();
            int memory_offset_var_index = calc_array_element_offset(indices_vector, symbolEntry.index_record);
            int offset_calc_end_index = quaternion_sequence.size();

            symbolEntry.is_used = true;
            symbolEntry.last_use_offset = Node::current_node->offset;

            int array_index = analyse_symbol_stack[searching_idx].symbol_index;
            int new_symbol = get_temp_symbol(symbolEntry.data_type, false);
            symbol_table[new_symbol].is_array = true; // when a symbol is both array and temp, means it is a reference of an element, maybe assigned

            // index_record[0..1] represent the quaternion which calc offset
            symbol_table[new_symbol].value.int_value = quaternion_sequence.size();
            symbol_table[new_symbol].index_record.push_back(offset_calc_start_index);
            symbol_table[new_symbol].index_record.push_back(offset_calc_end_index);

            Quaternion fetch_quaternion{OP_INVALID, array_index, memory_offset_var_index, new_symbol};
            switch (symbolEntry.data_type) {
                case DT_BOOL:
                    fetch_quaternion.op_code = OP_FETCH_BOOL;
                    break;

                case DT_INT:
                    fetch_quaternion.op_code = OP_FETCH_INT;
                    break;

                case DT_FLOAT:
                    fetch_quaternion.op_code = OP_FETCH_FLOAT;
                    break;

                case DT_DOUBLE:
                    fetch_quaternion.op_code = OP_FETCH_DOUBLE;
                    break;

                default:
                    throw "DEBUG: Invalid Data Type!";
                    break;
            }
            quaternion_sequence.push_back(fetch_quaternion);

            return new_symbol;
        }
        --searching_idx;
    }

    // not found
    Diagnose::printError(Node::current_node->offset, "Symbol " + Node::current_node->child_nodes_ptr[0]->content + " not defined.");
    exit(-1);
    return -2;
}

//int Indices__LeftSquareBrace_Expr_RightSquareBrace_Indices_fore_function(int* return_values_ptr)
//{
//    Node::current_node->child_nodes_ptr[3]->attributes.emplace(boolIndicesUsageAttr, Node::current_node->get_attribute_value(boolIndicesUsageAttr));
//    return 0;
//}

int DecIndices__LeftSquareBrace_Expr_RightSquareBrace_DecIndices_post_function(int* return_values_ptr)
{

    SymbolEntry& symbol = symbol_table[return_values_ptr[1]];

    // in declaration
    bool is_const_expr = symbol.is_const;
    if (is_const_expr) {
        if (symbol.data_type != DT_INT) {
            Diagnose::printError(Node::current_node->child_nodes_ptr[1]->offset, "Cannot declare an array with a non-integer expression.");
        }
        else {
            int expr_value = symbol.value.int_value;
            if (expr_value <= 0) {
                Diagnose::printError(Node::current_node->child_nodes_ptr[1]->offset, "Cannot declare an array with a non-positive expression.");
            }
            else {
                Node::current_node->attributes.emplace(intIndicesSizeIndexAttr, return_values_ptr[1]);
            }
        }
    }
    else {
        Diagnose::printError(Node::current_node->child_nodes_ptr[1]->offset, "Cannot declare an array with a non-const expression.");
    }
    return -1;
}

int DecIndices__LeftSquareBrace_Expr_RightSquareBrace_post_function(int* return_values_ptr)
{
    return DecIndices__LeftSquareBrace_Expr_RightSquareBrace_DecIndices_post_function(return_values_ptr);
}

int UseIndices__LeftSquareBrace_Expr_RightSquareBrace_UseIndices_post_function(int* return_values_ptr)
{
    SymbolEntry& symbol = symbol_table[return_values_ptr[1]];

    // in reference
    if (symbol.data_type == DT_FLOAT || symbol.data_type == DT_DOUBLE || symbol.data_type == DT_BOOL || symbol.data_type == DT_VOID || symbol.data_type == DT_NOT_DECIDED) {
        Diagnose::printError(Node::current_node->child_nodes_ptr[1]->offset, "Cannot access an array with a non-integer expression.");
    }
    else {
        Node::current_node->attributes.emplace(intIndicesSizeIndexAttr, return_values_ptr[1]);
    }
    return -1;
}

int UseIndices__LeftSquareBrace_Expr_RightSquareBrace_post_function(int* return_values_ptr)
{
    return UseIndices__LeftSquareBrace_Expr_RightSquareBrace_UseIndices_post_function(return_values_ptr);
}


//int Call__Function_LeftBrace_HereIsArgument_RightBrace_fore_function(int* return_values_ptr)
//{
//    Node::current_node->child_nodes_ptr[0]->attributes.emplace(boolFunctionNameUsageAttr, boolFunctionNameUsage_Call);
//    // push 1 into stack means call a function
////    semantic_action_stack.push_back(1);
//
//    return 0;
//}

int find_function(Node* id_ptr)
{
    // find function index according to id's content
    string call_name = id_ptr->content;
    for (int i = Function::function_table.size() - 1; i >= 0; --i) {
        if (Function::function_table[i].name == call_name) {
            // find
            return i;
        }
    }

    // not found
    Diagnose::printError(id_ptr->offset, "Function " + id_ptr->content + " not found.");
    return -2;
}

int Call__id_LeftBrace_HereIsArgument_RightBrace_post_function(int* return_values_ptr)
{
    // check args are matching parameters in definition
    int func_idx = find_function(Node::current_node->child_nodes_ptr[0]);
    Function& current_function = Function::function_table[func_idx];
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
            int new_symbol_idx = get_temp_symbol(current_function.parameter_types[i], symbol_table[current_quaternion.opr1].is_const);
            quaternion_sequence.insert(quaternion_sequence.end() - i - 1 - offset, Quaternion{cast_op, current_quaternion.opr1, -1, new_symbol_idx});
            assert(quaternion_sequence[quaternion_sequence.size() - 1 - i - offset].op_code == OP_PAR);
            quaternion_sequence[quaternion_sequence.size() - 1 - i - offset].opr1 = new_symbol_idx;
            ++offset;
        }
    }

    // all matched
    // create a return temp var
    int return_temp_var = get_temp_symbol(Function::function_table[func_idx].return_data_type, false);

    Quaternion call_quaternion{OP_CALL, func_idx, return_values_ptr[2], return_temp_var};
    quaternion_sequence.push_back(call_quaternion);

    return return_temp_var;
}

int Call__id_LeftBrace_RightBrace_post_function(int* return_values_ptr)
{
    int func_idx = find_function(Node::current_node->child_nodes_ptr[0]);
    Function& current_function = Function::function_table[func_idx];

    if (!Function::function_table[func_idx].parameter_types.empty()) {
        Diagnose::printError(Node::current_node->offset, "Function " + Function::function_table[func_idx].name + " requires arguments.");
    }

    // create a return temp var
    int return_temp_var = get_temp_symbol(Function::function_table[func_idx].return_data_type, false);

    Quaternion call_quaternion{OP_CALL, func_idx, 0, return_temp_var};
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

//int Argument__Argument_Comma_Arguments_fore_function(int* return_values_ptr)
//{
//    Node::current_node->child_nodes_ptr[0]->attributes.emplace(boolVarUsageAttr, boolVarUsage_Reference); // means use var
////    semantic_action_stack.push_back(1); // means use var
//    return 0;
//}

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
    if(type1 <= DT_BOOL || type2 <= DT_BOOL)
    {
        Diagnose::printError(Node::current_node->offset, "Unsupported Calculation.");
        exit(-1);
    }
    int ret = get_temp_symbol(DT_BOOL, symbol_table[return_values_ptr[0]].is_const && symbol_table[return_values_ptr[2]].is_const);
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
    if(type1 <= DT_BOOL || type2 <= DT_BOOL)
    {
        Diagnose::printError(Node::current_node->offset, "Unsupported Calculation.");
        exit(-1);
    }
    int ret;
    DATA_TYPE_ENUM dt_type = (DATA_TYPE_ENUM) max(type1, type2);
    symbol_table_idx_1 = Type_Conversion(type1, dt_type, return_values_ptr[0]);
    symbol_table_idx_2 = Type_Conversion(type2, dt_type, return_values_ptr[2]);
    ret = get_temp_symbol(dt_type, symbol_table[symbol_table_idx_1].is_const && symbol_table[symbol_table_idx_2].is_const);
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
    if(type1 <= DT_BOOL || type2 <= DT_BOOL)
    {
        Diagnose::printError(Node::current_node->offset, "Unsupported Calculation.");
        exit(-1);
    }
    int ret;
    DATA_TYPE_ENUM dt_type = (DATA_TYPE_ENUM) max(type1, type2);
    symbol_table_idx_1 = Type_Conversion(type1, dt_type, return_values_ptr[0]);
    symbol_table_idx_2 = Type_Conversion(type2, dt_type, return_values_ptr[2]);
    ret = get_temp_symbol(dt_type, symbol_table[return_values_ptr[0]].is_const && symbol_table[return_values_ptr[2]].is_const);
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
    int ret = get_temp_symbol(symbol_table[return_values_ptr[1]].data_type, symbol_table[return_values_ptr[1]].is_const);
    emit(OP_NEG, return_values_ptr[1], -1, ret);
    return ret;
}

int Item__LeftBrace_Expr_RightBrace_post_function(int* return_values_ptr)
{
    return return_values_ptr[1];
}

int Item__VariableUse_post_action(int* return_values_ptr)
{
    return return_values_ptr[0];
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

    if (!symbol_table[return_values_ptr[0]].is_array && symbol_table[return_values_ptr[0]].is_temp) {
        Diagnose::printError(Node::current_node->offset, "Lvalue required as left operand of assignment.");
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

        if (symbol_table[symbol_table_idx_1].is_array && symbol_table[symbol_table_idx_1].is_temp) {
            // assign to array elem
            Quaternion& related_quaternion = quaternion_sequence[symbol_table[symbol_table_idx_1].value.int_value];
            int array_index = related_quaternion.opr1;
            int offset = related_quaternion.opr2;

            // move offset calculation to the last
            int delta = symbol_table[symbol_table_idx_1].index_record[1] - symbol_table[symbol_table_idx_1].index_record[0] + 1;
            vector<Quaternion> temp_sequence;
            for (int i = symbol_table[symbol_table_idx_1].index_record[0]; i <= symbol_table[symbol_table_idx_1].index_record[1]; ++i) {
                temp_sequence.push_back(quaternion_sequence[i]);
            }
            for (int i = symbol_table[symbol_table_idx_1].index_record[1] + 1; i < quaternion_sequence.size(); ++i) {
                quaternion_sequence[i - delta] = quaternion_sequence[i];
            }

            assert(temp_sequence.back().op_code == OP_FETCH_BOOL || temp_sequence.back().op_code == OP_FETCH_INT || temp_sequence.back().op_code == OP_FETCH_FLOAT || temp_sequence.back().op_code == OP_FETCH_DOUBLE);

            for (int i = quaternion_sequence.size() - delta, index2 = 0; i < quaternion_sequence.size() - 1; ++i, ++index2) {
                quaternion_sequence[i] = temp_sequence[index2];
            }

            Quaternion array_store_quaternion{OP_CODE::OP_ARRAY_STORE, cast_result_index, offset, array_index};
            quaternion_sequence[quaternion_sequence.size() - 1] = array_store_quaternion;

        }

        emit(OP_CODE::OP_ASSIGNMENT, cast_result_index, -1, symbol_table_idx_1);
    }
    else {
        if (symbol_table[symbol_table_idx_1].is_array && symbol_table[symbol_table_idx_1].is_temp) {
            // assign to array elem
            Quaternion& related_quaternion = quaternion_sequence[symbol_table[symbol_table_idx_1].value.int_value];
            int array_index = related_quaternion.opr1;
            int offset = related_quaternion.opr2;

            // move offset calculation to the last
            int delta = symbol_table[symbol_table_idx_1].index_record[1] - symbol_table[symbol_table_idx_1].index_record[0] + 1;
            vector<Quaternion> temp_sequence;
            for (int i = symbol_table[symbol_table_idx_1].index_record[0]; i <= symbol_table[symbol_table_idx_1].index_record[1]; ++i) {
                temp_sequence.push_back(quaternion_sequence[i]);
            }
            for (int i = symbol_table[symbol_table_idx_1].index_record[1] + 1; i < quaternion_sequence.size(); ++i) {
                quaternion_sequence[i - delta] = quaternion_sequence[i];
            }

            assert(temp_sequence.back().op_code == OP_FETCH_BOOL || temp_sequence.back().op_code == OP_FETCH_INT || temp_sequence.back().op_code == OP_FETCH_FLOAT || temp_sequence.back().op_code == OP_FETCH_DOUBLE);

            for (int i = quaternion_sequence.size() - delta, index2 = 0; i < quaternion_sequence.size() - 1; ++i, ++index2) {
                quaternion_sequence[i] = temp_sequence[index2];
            }

            Quaternion array_store_quaternion{OP_CODE::OP_ARRAY_STORE, symbol_table_idx_2, offset, array_index};
            quaternion_sequence[quaternion_sequence.size() - 1] = array_store_quaternion;
        }

        emit(OP_CODE::OP_ASSIGNMENT, symbol_table_idx_2, -1, symbol_table_idx_1);
    }
    return symbol_table_idx_1;
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

//int Compound__LeftCurlyBrace_Statements_RightCurlyBrace_fore_action(int* return_values_ptr)
//{
//    ++current_layer;
//
//    return 0;
//}

int Compound__LeftCurlyBrace_Statements_RightCurlyBrace_post_action(int* return_values_ptr)
{
    clear_symbol_stack();

    --current_layer;

    return return_values_ptr[2];
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
        ret = get_temp_symbol(DT_INT, true);
//        emit(OP_LI_INT, num, -1, ret);
        symbol_table[ret].value.int_value = num;
    }
    else if (str.back() == 'f' || str.back() == 'F')
    {
        float num = stof(str);
        int int_num = *((int*)&num);

        ret = get_temp_symbol(DT_FLOAT, true);
//        emit(OP_LI_FLOAT, int_num, -1, ret);
        symbol_table[ret].value.float_value = num;
    }
    else {
        double num = stod(str);
        int arr[2];
        arr[0] = *((int *)&num);
        arr[1] = *((int *)&num + 1);

        ret = get_temp_symbol(DT_DOUBLE, true);
//        emit(OP_LI_DOUBLE, arr[0], arr[1], ret);
        symbol_table[ret].value.double_value = num;
    }
    return ret;
}

int Num__true_post_action(int* return_values_ptr)
{
    int ret = get_temp_symbol(DT_BOOL, true);
//    emit(OP_LI_BOOL, true, -1, ret);
    symbol_table[ret].value.bool_value = true;
    return ret;
}

int Num__false_post_action(int* return_values_ptr)
{
    int ret = get_temp_symbol(DT_BOOL, true);
//    emit(OP_LI_BOOL, false, -1, ret);
    symbol_table[ret].value.bool_value = false;
    return ret;
}

int Const__const_post_action(int* return_values_ptr)
{
    current_type_is_const = true;

    return -1;
}