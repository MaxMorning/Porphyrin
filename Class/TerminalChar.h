//
// Project: Porphyrin
// File Name: TerminalChar.h
// Author: Morning
// Description:
//
// Create Date: 2021/11/2
//

#ifndef PORPHYRIN_TERMINALCHAR_H
#define PORPHYRIN_TERMINALCHAR_H

#include <vector>
#include <unordered_map>
#include "Character.h"
#include "Include/Lexical.h"

using namespace std;

class TerminalChar : public Character{
public:
    // convert index in Lexicon
    static void init_terminal_char_set();

    // all terminal character here
    static vector<TerminalChar> all_terminal_chars;

    static int get_index(const string& token);

    TerminalChar(string token, int index);
};


#endif //PORPHYRIN_TERMINALCHAR_H
