//
// Project: Porphyrin
// File Name: Nonterminal.cpp
// Author: Morning
// Description:
//
// Create Date: 2021/11/2
//

#include "Nonterminal.h"

Nonterminal::Nonterminal(string token, int index) : Character(token, false, index) {

}

bool Nonterminal::add_first(Nonterminal& character) {
    this->epsilon_in_firstset = character.epsilon_in_firstset;

    bool have_change = false;

    for(int i = 0; i < character.first_set.size(); i ++) {
        if(!this->find_in_firstset(character.first_set[i])) {
            have_change = true;
            this->first_set.push_back(character.first_set[i]);
        }
    }
    return have_change;
}

bool Nonterminal::find_in_firstset(Character& character) {
    bool is_in = false;
    for (int i = 0; i < this->first_set.size(); ++i) {
        if(this->first_set[i].index == character.index) { // assume both side are terminal character
            is_in = true;
            break;
        }
    }

    return is_in;
}
