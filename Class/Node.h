//
// Project: Porphyrin
// File Name: Node.h
// Author: Morning
// Description:
//
// Create Date: 2021/11/3
//

#ifndef PORPHYRIN_NODE_H
#define PORPHYRIN_NODE_H

#include "Character.h"
#include "Nonterminal.h"
#include <vector>
#include <fstream>
#include <unordered_map>


using namespace std;

#define boolVarUsageAttr                (-1)
#define boolFunctionNameUsageAttr       (-2)
#define boolTypeUsageAttr               (-3)

#define boolVarUsage_Declare            0
#define boolVarUsage_Reference          1

#define boolFunctionNameUsage_Declare   0
#define boolFunctionNameUsage_Call      1

#define boolTypeUsage_FuncReturn        0
#define boolTypeUsage_VarDeclare        1

class Node : public Character {
public:
    Node* parent;
    vector<Node*> child_nodes_ptr;
    string content; // only valid in terminal char, the name of var / func or symbols(==, etc.)
    unordered_map<int, int> attributes;

    int non_terminal_idx;
    int reduction_idx;

    int offset; // offset in processed code

    Node(const Nonterminal& nonterminal, int reduction_idx);
    Node(const Lexicon& lexicon);

    void print_tree();

    void build_parent();

    void free_memory();

    // Semantic part
    static Node* current_node;
    int quad, nextlist;
    int truelist, falselist;
    int semantic_action();

    int get_attribute_value(int key);

    // visualize part
    int bias = 0;
    int half = 0;

    int x = 0;
    int y = 0;
    Node* leftThread = nullptr;
    Node* rightThread = nullptr;

    static ofstream fout;

    void setOccupancy();
    void setLayout(int depth);
    void adjustTree(int offset);
    
    int bottom = 0;
	void setThread();
    Node* getLeftThreadNode(Node* leftNode);
    Node* getRightThreadNode(Node* rightNode);

    void printTreeInfo();
    void printTreeInfoIntoTxt(string filename = "");
};


#endif //PORPHYRIN_NODE_H
