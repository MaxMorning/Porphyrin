//
// Project: Porphyrin
// File Name: Nonterminal.h
// Author: Morning
// Description:
//
// Create Date: 2021/11/2
//

#ifndef PORPHYRIN_NONTERMINAL_H
#define PORPHYRIN_NONTERMINAL_H

#include "Character.h"
#include "LR1item.h"
#include <vector>

using namespace std;

class Nonterminal : public Character {
public:
    // right part of productions
    vector<vector<Character>> productions;

    // all nonterminal character here
    static vector<Nonterminal> all_nonterminal_chars;

    Nonterminal(string token, int index);

    vector<Character> first_set;

    bool add_first(Nonterminal& character);

    bool find_in_firstset(Character &character);
};


#endif //PORPHYRIN_NONTERMINAL_H
