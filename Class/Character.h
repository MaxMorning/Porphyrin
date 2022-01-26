//
// Project: Porphyrin
// File Name: Character.h
// Author: Morning, Wang
// Description:
//
// Create Date: 2021/11/2
//

#ifndef PORPHYRIN_CHARACTER_H
#define PORPHYRIN_CHARACTER_H

#include <vector>
#include <string>

using namespace std;

class Character {
public:
    string token;
    bool is_terminal;
    int index;

    // this field may ba move to Nonterminal
    bool epsilon_in_firstset;

    Character(string token, bool is_terminal, int index);
};


#endif //PORPHYRIN_CHARACTER_H
