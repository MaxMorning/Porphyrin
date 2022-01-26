//
// Project: Porphyrin
// File Name: Character.cpp.cc
// Author: Morning
// Description:
//
// Create Date: 2021/11/2
//

#include "Character.h"

#include <utility>

Character::Character(string token, bool is_terminal, int index) {
    this->token = std::move(token);
    this->is_terminal = is_terminal;
    this->index = index;
    this->epsilon_in_firstset = false;
}
