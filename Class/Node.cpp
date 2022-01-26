//
// Project: Porphyrin
// File Name: Node.cpp.cc
// Author: Morning
// Description:
//
// Create Date: 2021/11/3
//

#include "Node.h"
#include <queue>

ofstream Node::fout;

Node::Node(const Nonterminal& nonterminal, int reduction_idx) : Character(nonterminal.token, false, -1) {
    parent = nullptr;
    content = nonterminal.token;
    non_terminal_idx = nonterminal.index;
    this->reduction_idx = reduction_idx;
    offset = -1;
}

// TODO NOTICE
// lexicon.get_type() is correct
// lexicon.lex_content for test
Node::Node(const Lexicon &lexicon) : Character(lexicon.lex_content, true, -1) {
    parent = nullptr;
    content = lexicon.lex_content;
    offset = lexicon.offset;
}

void Node::print_tree() {
    queue<Node*> print_queue;
    if (this->parent == nullptr) {
        cout << "Parent Addr: " << "nullptr" << "\tToken: " << "NULL";
    }
    else {
        cout << "Parent Addr: " << this->parent << "\tToken: " << this->parent->token;
    }
    cout << "\t\tThis Addr: " << this << "\tToken: " << this->token << endl;
    unsigned long current_child_cnt = this->child_nodes_ptr.size();
    for (unsigned long i = 0; i < current_child_cnt; ++i) {
        print_queue.push(this->child_nodes_ptr[i]);
    }

    while (!print_queue.empty()) {
        Node* temp_node_ptr = print_queue.front();
        print_queue.pop();
        cout << "Parent Addr: " << temp_node_ptr->parent << "\tToken: " << temp_node_ptr->parent->token;
        cout << "\t\tThis Addr: " << temp_node_ptr << "\tToken: " << temp_node_ptr->token;
        cout << "\tChildCnt: " << temp_node_ptr->child_nodes_ptr.size() << endl;

        for (Node* child_ptr : temp_node_ptr->child_nodes_ptr) {
            print_queue.push(child_ptr);
        }
    }
}

void Node::build_parent() {
    queue<Node*> build_queue;
    build_queue.push(this);
    for (Node* child_node : this->child_nodes_ptr) {
        build_queue.push(child_node);
    }

    while (!build_queue.empty()) {
        Node* current_ptr = build_queue.front();
        build_queue.pop();

        for (Node* child_node : current_ptr->child_nodes_ptr) {
            child_node->parent = current_ptr;
            build_queue.push(child_node);
        }
    }
}

void Node::free_memory() {
    queue<Node*> free_queue;
    free_queue.push(this);

    while (!free_queue.empty()) {
        Node* current_ptr = free_queue.front();
        free_queue.pop();

        for (Node* child_node : current_ptr->child_nodes_ptr) {
            free_queue.push(child_node);
        }

        delete current_ptr;
    }
}

// Semantic part
// uses semantic_action_stack for arg passing
// implemented in Semantic.cpp
//Node* Node::current_node;
//int Node::semantic_action()

// visualize part
void Node::printTreeInfo()
{
    printf("\"%s\"，中心=%d，半径=%d", this->content.c_str(), this->x, this->half);
    if (this->parent)
        printf(" 父=\"%s\"\n", this->parent->content.c_str());
    else
        printf("\n");
    int n = this->child_nodes_ptr.size();
    for (int i = 0; i < n; ++i)
        this->child_nodes_ptr[i]->printTreeInfo();
}

void Node::printTreeInfoIntoTxt(string filename)
{
    int left_up_X = this->x - this->half + 1;
    int left_up_Y = this->y;

    if (!this->parent) {
        fout.open(filename, ios::out);
        if (!fout.is_open()) {
            std::cerr << "cannot write into the file";
        }

        //内容，左上坐标
        fout << this->content << " " << this->bias << " " << left_up_X << " " << left_up_Y << endl;

        int n = this->child_nodes_ptr.size();
        for (int i = 0; i < n; ++i)
            this->child_nodes_ptr[i]->printTreeInfoIntoTxt();
        fout.close();
    }
    else {

        //内容，偏移（是奇数个字符？），左上坐标（横坐标和纵坐标）
        fout << this->content << " " << this->bias << " " << left_up_X << " " << left_up_Y << endl;
        //线段坐标1，线段坐标2
        fout << this->x << " " << this->y << " " << this->parent->x << " " << this->parent->y << endl;

        int n = this->child_nodes_ptr.size();
        for (int i = 0; i < n; ++i)
            this->child_nodes_ptr[i]->printTreeInfoIntoTxt();
    }
}

void Node::setOccupancy()
{
    int len = this->content.length();
    this->half = len / 2 + 1;
    if (len % 2 == 1) {
        this->half += 1;
        this->bias = 1;
    }
}

void Node::setLayout(int depth)
{
    this->setOccupancy();
    this->x = half;
    this->y = depth;
    this->bottom = depth;

    //是叶子结点，函数返回
    int n = this->child_nodes_ptr.size();
    if (n == 0)
        return;

    //递归，先布局子结点
    for (int i = 0; i < n; ++i)
        this->child_nodes_ptr[i]->setLayout(depth + 1);

    //重新计算所有子树中的结点的横坐标x，避免子树重叠
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n;++j){
            Node* left = this->child_nodes_ptr[i];
            Node* right = this->child_nodes_ptr[j];
            while (left && right && left != right) {
                int offset = left->x + left->half + right->half - right->x;
                if (offset > 0) {
                    //printf("\n因为\"%s\"和\"%s\"重叠\n", left->content.c_str(), right->content.c_str());
                    //printf("所以以\"%s\"为跟的树右移%d\n", this->child_nodes_ptr[i]->content.c_str(), offset);
                    //printf("此时left位置=%d，半径=%d；right位置=%d，半径=%d\n", left->x, left->half, right->x, right->half);
                    this->child_nodes_ptr[j]->adjustTree(offset);
                }
                left = left->rightThread;
                right = right->leftThread;
            }
        }
    }


    //根据子结点中最左和最右的位置调整本结点
    int leftmostChildX = this->child_nodes_ptr.front()->x;
    int rightmostChildX = this->child_nodes_ptr.back()->x;
    this->x = (leftmostChildX + rightmostChildX) / 2;

    //完善线索，左、右线索需要一直指向最深处的最左、最右结点
    this->setThread();
}

void Node::setThread()
{
    //左、右线索指向最左、最右孩子
    this->leftThread = this->child_nodes_ptr.front();
    this->rightThread = this->child_nodes_ptr.back();

    //确认左底、右底
    int n = this->child_nodes_ptr.size();
    for (int i = 0; i < n; ++i) {
        if (this->child_nodes_ptr[i]->bottom > this->bottom)
            this->bottom = this->child_nodes_ptr[i]->bottom;
        if (this->child_nodes_ptr[i]->bottom >= this->bottom)
            this->bottom = this->child_nodes_ptr[i]->bottom;
    }

    //完善左、右线索，使得沿着左、右线索可以直接遍历到底部的叶子结点
    Node* leftNode = this->child_nodes_ptr.front();
    Node* rightNode = this->child_nodes_ptr.back();
    while (leftNode->y < this->bottom) {
        if (!leftNode->leftThread)
            leftNode->leftThread = this->getLeftThreadNode(leftNode);
        leftNode = leftNode->leftThread;
    }
    while (rightNode->y < this->bottom) {
        if (!rightNode->rightThread)
            rightNode->rightThread = this->getRightThreadNode(rightNode);
        rightNode = rightNode->rightThread;
    }
}

//找到this树中深度比leftNode大1的最左侧结点
Node* Node::getLeftThreadNode(Node* leftNode)
{
    if (this->y == leftNode->y + 1)
        return this;
    else if (this->y > leftNode->y + 1)
        return NULL;
    else {
        int n = this->child_nodes_ptr.size();
        for (int i = 0; i < n; ++i) {
            Node* temp = this->child_nodes_ptr[i]->getLeftThreadNode(leftNode);
            if (temp && temp->y == leftNode->y + 1)
                return temp;
        }
        return NULL;
    }
}

Node* Node::getRightThreadNode(Node* rightNode)
{
    if (this->y == rightNode->y + 1)
        return this;
    else if (this->y > rightNode->y + 1)
        return NULL;
    else {
        int n = this->child_nodes_ptr.size();
        for (int i = n - 1; i >= 0; --i) {
            Node* temp = this->child_nodes_ptr[i]->getRightThreadNode(rightNode);
            if (temp && temp->y == rightNode->y + 1)
                return temp;
        }
        return NULL;
    }
}

void Node::adjustTree(int offset)
{
    int n = this->child_nodes_ptr.size();
    for (int i = 0; i < n; ++i)
        this->child_nodes_ptr[i]->adjustTree(offset);
    this->x += offset;
    //cout << "\"" << this->content << "\"" << "右移" << offset << endl;
}
