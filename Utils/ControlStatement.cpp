#include "Include/Semantic.h"

//sub-function
int emit(OP_CODE op, int src1, int src2, int result)
{
    quaternion_sequence.push_back(Quaternion{ op, src1, src2, result });

    return quaternion_sequence.size() - 1;
}

int insert(int location, OP_CODE op, int src1, int src2, int result)
{
    quaternion_sequence.insert(quaternion_sequence.begin() + location, Quaternion{ op, src1, src2, result });

    return location;
}

int merge(int list1, int list2)
{
    if (list1 == list2)
        return -2;

    if (list1 > list2)
    {
        int temp = list1;
        list1 = list2;
        list2 = temp;
    }

    int relative_addr = quaternion_sequence[list2].result;
    int absolute_addr = list2 + relative_addr;
    while (true)
    {
        if (relative_addr == 0)
        {
            quaternion_sequence[absolute_addr].result = list1 - absolute_addr;//list1对于addr的相对地址
            return list2;
        }
        relative_addr = quaternion_sequence[absolute_addr].result;
        absolute_addr = absolute_addr + relative_addr;
    }
}

void back_patch(int list, int addr)
{
    while (true)
    {
        int relative_addr = quaternion_sequence[list].result;
        quaternion_sequence[list].result = addr - list; //相对地址
        if (relative_addr == 0)
            return;
        list += relative_addr;
    }
}

bool wont_be_backpatched()
{
    Node* current;
    Node* parent = Node::current_node;
    while (true)
    {
        current = parent;
        parent = current->parent;
        if (parent->child_nodes_ptr[0]->content == "(")
            continue;
        else if (parent->child_nodes_ptr.size() > 1)
            break;
    }
    string child0 = parent->child_nodes_ptr[0]->content;
    string chiid1 = parent->child_nodes_ptr[1]->content;
    if (child0 == "if" || child0 == "while" || child0 == "!" || chiid1 == "&&" || chiid1 == "||")
    {
        return 0;
    }
    else
    {
        return 1;
    }
}


//Semantic Action of Control Statement
int If__if_LeftBrace_Expr_RightBrace_Statement_fore_action(int* return_values_ptr)
{
    ++current_layer;

    return -1;
}

int If__if_LeftBrace_Expr_RightBrace_Statement_post_action(int* return_values_ptr)
{
    Node* E = Node::current_node->child_nodes_ptr[2];
    Node* S = Node::current_node->child_nodes_ptr[4];

    if (E->truelist == -1)
    {
        E->truelist = insert(E->nextlist, OP_CODE::OP_JNZ, return_values_ptr[2], -1, 0);
        E->falselist = insert(E->nextlist + 1, OP_CODE::OP_JMP, -1, -1, 0);
        E->nextlist += 2;
        S->quad += 2;
        S->nextlist += 2;
    }

    back_patch(E->truelist, S->quad);
    back_patch(E->falselist, S->nextlist);

    clear_symbol_stack();
    --current_layer;

    return -1;
}

int If__if_LeftBrace_Expr_RightBrace_Statement_else_Statement_fore_action(int* return_values_ptr)
{
    ++current_layer;

    return -1;
}

int If__if_LeftBrace_Expr_RightBrace_Statement_else_Statement_post_action(int* return_values_ptr)
{
    Node* E = Node::current_node->child_nodes_ptr[2];
    Node* S1 = Node::current_node->child_nodes_ptr[4];
    Node* S2 = Node::current_node->child_nodes_ptr[6];
    if (E->truelist == -1)
    {
        E->truelist = insert(E->nextlist, OP_CODE::OP_JNZ, return_values_ptr[2], -1, 0);
        E->falselist = insert(E->nextlist + 1, OP_CODE::OP_JMP, -1, -1, 0);
        E->nextlist += 2;
        S1->quad += 2;
        S1->nextlist += 2;
        S2->quad += 2;
        S2->nextlist += 2;
    }

    insert(S1->nextlist, OP_CODE::OP_JMP, -1, -1, 0);
    S2->quad += 1;
    S2->nextlist += 1;

    back_patch(E->truelist, S1->quad);
    back_patch(E->falselist, S2->quad);
    back_patch(S1->nextlist, S2->nextlist);

    clear_symbol_stack();
    --current_layer;

    return -1;
}

int While__while_LeftBrace_Expr_RightBrace_Statement_post_action(int* return_values_ptr)
{
    Node* E = Node::current_node->child_nodes_ptr[2];
    Node* S = Node::current_node->child_nodes_ptr[4];
    if (E->truelist == -1)
    {
        E->truelist = insert(E->nextlist, OP_CODE::OP_JNZ, return_values_ptr[2], -1, 0);
        E->falselist = insert(E->nextlist + 1, OP_CODE::OP_JMP, -1, -1, 0);
        E->nextlist += 2;
        S->quad += 2;
        S->nextlist += 2;
    }
    back_patch(E->truelist, S->quad);
    emit(OP_JMP, -1, -1, E->quad - S->nextlist);//E.quad对于S.nextlist的相对地址
    back_patch(E->falselist, S->nextlist + 1);

    clear_symbol_stack();
    --current_layer;

    return 0;
}

int LogicalOr__LogicalOr_LogicalOR_LogicalAnd_post_action(int* return_values_ptr)
{
    //calculate temp variable
    int ret = get_temp_symbol(DT_BOOL, symbol_table[return_values_ptr[0]].is_const && symbol_table[return_values_ptr[2]].is_const);
    /*
    int symbol_table_idx_1 = return_values_ptr[0];
    int symbol_table_idx_2 = return_values_ptr[2];
    DATA_TYPE_ENUM type1 = symbol_table[symbol_table_idx_1].data_type;
    DATA_TYPE_ENUM type2 = symbol_table[symbol_table_idx_2].data_type;
    if(type1 < DT_BOOL || type2 < DT_BOOL)
    {
        cerr << "Illegal Void Calculation" << endl;
        exit(-1);
    }
    symbol_table_idx_1 = Type_Conversion(type1, DT_BOOL, return_values_ptr[0]);
    symbol_table_idx_2 = Type_Conversion(type2, DT_BOOL, return_values_ptr[2]);
    int ret = get_temp_symbol(DT_BOOL);
    emit(OP_CODE::OP_LOGIC_OR, symbol_table_idx_1, symbol_table_idx_2, ret);
    */

    //back_patch
    Node* E1 = Node::current_node->child_nodes_ptr[0];
    Node* E2 = Node::current_node->child_nodes_ptr[2];
    if (E1->truelist == -1)
    {
        E1->truelist = insert(E1->nextlist, OP_CODE::OP_JNZ, return_values_ptr[0], -1, 0);
        E1->falselist = insert(E1->nextlist + 1, OP_CODE::OP_JMP, -1, -1, 0);
        E1->nextlist += 2;
        E2->quad += 2;
        E2->nextlist += 2;
    }
    if (E2->truelist == -1)
    {
        E2->truelist = insert(E2->nextlist, OP_CODE::OP_JNZ, return_values_ptr[2], -1, 0);
        E2->falselist = insert(E2->nextlist + 1, OP_CODE::OP_JMP, -1, -1, 0);
        E2->nextlist += 2;
    }
    back_patch(E1->falselist, E2->quad);
    Node::current_node->truelist = merge(E1->truelist, E2->truelist);
    Node::current_node->falselist = E2->falselist;
    if (wont_be_backpatched())
    {
        back_patch(Node::current_node->truelist, emit(OP_CODE::OP_LI_BOOL, 1, -1, ret));
        emit(OP_CODE::OP_JMP, -1, -1, 2);
        back_patch(Node::current_node->falselist, emit(OP_CODE::OP_LI_BOOL, 0, -1, ret));
    }

    return ret;
}

int LogicalAnd__LogicalAnd_LogicalAND_Comparison_post_action(int* return_values_ptr)
{
    //calculate temp variable
    int ret = get_temp_symbol(DT_BOOL, symbol_table[return_values_ptr[0]].is_const && symbol_table[return_values_ptr[2]].is_const);
    /*
    int symbol_table_idx_1 = return_values_ptr[0];
    int symbol_table_idx_2 = return_values_ptr[2];
    DATA_TYPE_ENUM type1 = symbol_table[symbol_table_idx_1].data_type;
    DATA_TYPE_ENUM type2 = symbol_table[symbol_table_idx_2].data_type;
    if(type1 < DT_BOOL || type2 < DT_BOOL)
    {
        cerr << "Illegal Void Calculation" << endl;
        exit(-1);
    }
    symbol_table_idx_1 = Type_Conversion(type1, DT_BOOL, return_values_ptr[0]);
    symbol_table_idx_2 = Type_Conversion(type2, DT_BOOL, return_values_ptr[2]);
    int ret = get_temp_symbol(DT_BOOL);
    emit(OP_CODE::OP_LOGIC_AND, symbol_table_idx_1, symbol_table_idx_2, ret);
    */

    //back_patch
    Node* E1 = Node::current_node->child_nodes_ptr[0];
    Node* E2 = Node::current_node->child_nodes_ptr[2];
    if (E1->truelist == -1)
    {
        E1->truelist = insert(E1->nextlist, OP_CODE::OP_JNZ, return_values_ptr[0], -1, 0);
        E1->falselist = insert(E1->nextlist + 1, OP_CODE::OP_JMP, -1, -1, 0);
        E1->nextlist += 2;
        E2->quad += 2;
        E2->nextlist += 2;
    }
    if (E2->truelist == -1)
    {
        E2->truelist = insert(E2->nextlist, OP_CODE::OP_JNZ, return_values_ptr[2], -1, 0);
        E2->falselist = insert(E2->nextlist + 1, OP_CODE::OP_JMP, -1, -1, 0);
        E2->nextlist += 2;
    }
    back_patch(E1->truelist, E2->quad);
    Node::current_node->truelist = E2->truelist;
    Node::current_node->falselist = merge(E1->falselist, E2->falselist);
    if (wont_be_backpatched())
    {
        back_patch(Node::current_node->truelist, emit(OP_CODE::OP_LI_BOOL, 1, -1, ret));
        emit(OP_CODE::OP_JMP, -1, -1, 2);
        back_patch(Node::current_node->falselist, emit(OP_CODE::OP_LI_BOOL, 0, -1, ret));
    }

    return ret;
}

int Item__LogicalNOT_Item_post_action(int* return_values_ptr)
{
    //calculate temp variable
    int ret = get_temp_symbol(DT_BOOL, symbol_table[return_values_ptr[1]].is_const);
    /*
    int symbol_table_idx_1 = return_values_ptr[1];
    DATA_TYPE_ENUM type1 = symbol_table[symbol_table_idx_1].data_type;
    if(type1 < DT_BOOL)
    {
        cerr << "Illegal Void Calculation" << endl;
        exit(-1);
    }
    int ret = get_temp_symbol(DT_BOOL);
    symbol_table_idx_1 = Type_Conversion(type1, DT_BOOL, return_values_ptr[0]);
    emit(OP_CODE::OP_LOGIC_NOT, symbol_table_idx_1, -1, ret);
    */

    //reverse lists
    Node* E = Node::current_node->child_nodes_ptr[1];
    if (E->truelist == -1)
    {
        E->truelist = insert(E->nextlist, OP_CODE::OP_JNZ, return_values_ptr[1], -1, 0);
        E->falselist = insert(E->nextlist + 1, OP_CODE::OP_JMP, -1, -1, 0);
        E->nextlist += 2;
    }
    Node::current_node->truelist = E->falselist;
    Node::current_node->falselist = E->truelist;
    if (wont_be_backpatched())
    {
        back_patch(Node::current_node->truelist, emit(OP_CODE::OP_LI_BOOL, 1, -1, ret));
        emit(OP_CODE::OP_JMP, -1, -1, 2);
        back_patch(Node::current_node->falselist, emit(OP_CODE::OP_LI_BOOL, 0, -1, ret));
    }

    return ret;
}
