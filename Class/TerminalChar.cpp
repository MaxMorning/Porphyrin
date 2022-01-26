//
// Project: Porphyrin
// File Name: TerminalChar.cpp.cc
// Author: Morning
// Description:
//
// Create Date: 2021/11/2
//

#include "TerminalChar.h"

vector<TerminalChar> TerminalChar::all_terminal_chars;

void TerminalChar::init_terminal_char_set() {
    Lexicon temp_lex = Lexicon(0, "", 0);
    int index = 0;
    for (int i = -2; i > -KEYWORD_TYPE_CNT - 4; --i) {
        temp_lex.lex_type = i;
        TerminalChar::all_terminal_chars.emplace_back(temp_lex.get_type(), index);
        ++index;
    }

    for (int i = 0; i < END_SIGNAL + 1; ++i) {
        temp_lex.lex_type = i;
        TerminalChar::all_terminal_chars.emplace_back(temp_lex.get_type(), index);
        ++index;
    }
}

TerminalChar::TerminalChar(string token, int index) : Character(token, true, index) {

}

int TerminalChar::get_index(const string& token) {
    for (int i = 0; i < TerminalChar::all_terminal_chars.size(); ++i) {
        if (TerminalChar::all_terminal_chars[i].token == token) {
            return i;
        }
    }
    return -1;
}
