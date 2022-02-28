//
// Project: Porphyrin
// File Name: Lexical.cpp.c
// Author: Morning
// Description:
//
// Create Date: 2021/10/3
//

#include "Include/Lexical.h"
#include "Class/Diagnose.h"

Lexicon get_next_lexicon(int& processing_idx, string processed_code)
{
    string buffer_str = "";
    size_t processed_length = processed_code.length();

    while (processing_idx < (int)processed_length) {
        ++processing_idx;
        if (is_valid_in_var_fun(processed_code[processing_idx])) {
            buffer_str += processed_code[processing_idx];
        }
        else {
            // parse current content in lexical buffer
            if (!buffer_str.empty()) {
                Lexicon temp_lex = Lexicon(buffer_str.c_str(), buffer_str.size(), processing_idx);
                buffer_str.clear();
                --processing_idx;
                return temp_lex;
            }

            switch (processed_code[processing_idx]) {
                case '=':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '=') {
                        ++processing_idx;
                        return Lexicon(EQUAL, "==", processing_idx - 1);
                    }
                    else {
                        return Lexicon(ASSIGNMENT, "=", processing_idx);
                    }
                    break;

                case '+':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '+') {
                        ++processing_idx;
                        return Lexicon(INCREASE, "++", processing_idx - 1);
                    }
                    else {
                        return Lexicon(ADD, "+", processing_idx);
                    }
                    break;

                case '-':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '-') {
                        ++processing_idx;
                        return Lexicon(DECREASE, "--", processing_idx - 1);
                    }
                    else {
                        return Lexicon(MINUS, "-", processing_idx);
                    }
                    break;

                case '*':
                    return Lexicon(STAR, "*", processing_idx);
                    break;

                case '/':
                    return Lexicon(DIV, "/", processing_idx);
                    break;

                case '>':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '=') {
                        ++processing_idx;
                        return Lexicon(GREAT_EQUAL, ">=", processing_idx - 1);
                    }
                    else {
                        return Lexicon(GREAT, ">", processing_idx);
                    }
                    break;

                case '<':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '=') {
                        ++processing_idx;
                        return Lexicon(LESS_EQUAL, "<=", processing_idx - 1);
                    }
                    else {
                        return Lexicon(LESS, "<", processing_idx);
                    }
                    break;

                case '!':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '=') {
                        ++processing_idx;
                        return Lexicon(NOT_EQUAL, "!=", processing_idx - 1);
                    }
                    else {
                        return Lexicon(LOGIC_NOT, "!", processing_idx);
                    }
                    break;

                case '&':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '&') {
                        ++processing_idx;
                        return Lexicon(LOGIC_AND, "&&", processing_idx - 1);
                    }
                    else {
                        return Lexicon(BIT_AND, "&", processing_idx);
                    }
                    break;

                case '|':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '|') {
                        ++processing_idx;
                        return Lexicon(LOGIC_OR, "||", processing_idx - 1);
                    }
                    else {
                        return Lexicon(BIT_OR, "|", processing_idx);
                    }
                    break;

                case ';':
                    return Lexicon(SEMICOLON, ";", processing_idx);
                    break;

                case ',':
                    return Lexicon(COMMA, ",", processing_idx);
                    break;

                case '(':
                    return Lexicon(LEFT_BRACE, "(", processing_idx);
                    break;

                case '[':
                    return Lexicon(LEFT_SQUARE_BRACE, "[", processing_idx);
                    break;

                case ']':
                    return Lexicon(RIGHT_SQUARE_BRACE, "]", processing_idx);
                    break;

                case ')':
                    return Lexicon(RIGHT_BRACE, ")", processing_idx);
                    break;

                case '{':
                    return Lexicon(LEFT_CURLY_BRACE, "{", processing_idx);
                    break;

                case '}':
                    return Lexicon(RIGHT_CURLY_BRACE, "}", processing_idx);
                    break;

                case '\n':
                    break;

                case '$':
                    return Lexicon(EPSILON, "$", processing_idx);
                    break;

                case '#':
                    return Lexicon(END_SIGNAL, "#", processing_idx);
                    // todo break the loop(not implemented because the PreProcessor is not implemented completely)
                    break;

                case ' ':
                    break;

                default:
                    Diagnose::printError(processing_idx, "Unrecognized character here.");
                    return Lexicon(END_SIGNAL, "#", processing_idx);
            }
        }
    }

    return Lexicon(END_SIGNAL, "###", processing_idx);
}

vector<Lexicon> lexical_analysis(string processed_code)
{
    vector<Lexicon> result_vector;
    size_t processed_length = processed_code.length();

    int processing_idx = 0;
    char lexical_buffer[LEXICAL_BUFFER_SIZE];
    int lexical_buffer_idx = 0;
    // Start parsing
    while (processing_idx < processed_length) {
        if (is_valid_in_var_fun(processed_code[processing_idx])) {
            if (lexical_buffer_idx < LEXICAL_BUFFER_SIZE) {
                lexical_buffer[lexical_buffer_idx] = processed_code[processing_idx];
                ++lexical_buffer_idx;
            }
            else {
                // todo throw error : TOKEN_TOO_LONG
            }
        }
        else {
            // parse current content in lexical buffer
            if (lexical_buffer_idx > 0) {
                result_vector.emplace_back(lexical_buffer, lexical_buffer_idx, processing_idx);
                lexical_buffer_idx = 0;
            }
            
            switch (processed_code[processing_idx]) {
                case '=':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '=') {
                        result_vector.emplace_back(EQUAL, "==", processing_idx);
                        ++processing_idx;
                    }
                    else {
                        result_vector.emplace_back(ASSIGNMENT, "=", processing_idx);
                    }
                    break;

                case '+':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '+') {
                        result_vector.emplace_back(INCREASE, "++", processing_idx);
                        ++processing_idx;
                    }
                    else {
                        result_vector.emplace_back(ADD, "+", processing_idx);
                    }
                    break;

                case '-':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '-') {
                        result_vector.emplace_back(DECREASE, "--", processing_idx);
                        ++processing_idx;
                    }
                    else {
                        result_vector.emplace_back(MINUS, "-", processing_idx);
                    }
                    break;

                case '*':
                    result_vector.emplace_back(STAR, "*", processing_idx);
                    break;

                case '/':
                    result_vector.emplace_back(DIV, "/", processing_idx);
                    break;

                case '>':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '=') {
                        result_vector.emplace_back(GREAT_EQUAL, ">=", processing_idx);
                        ++processing_idx;
                    }
                    else {
                        result_vector.emplace_back(GREAT, ">", processing_idx);
                    }
                    break;

                case '<':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '=') {
                        result_vector.emplace_back(LESS_EQUAL, "<=", processing_idx);
                        ++processing_idx;
                    }
                    else {
                        result_vector.emplace_back(LESS, "<", processing_idx);
                    }
                    break;

                case '!':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '=') {
                        result_vector.emplace_back(NOT_EQUAL, "!=", processing_idx);
                        ++processing_idx;
                    }
                    else {
                        result_vector.emplace_back(LOGIC_NOT, "!", processing_idx);
                    }
                    break;

                case '&':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '&') {
                        result_vector.emplace_back(LOGIC_AND, "&&", processing_idx);
                        ++processing_idx;
                    }
                    else {
                        result_vector.emplace_back(BIT_AND, "&", processing_idx);
                    }
                    break;

                case '|':
                    if (processing_idx + 1 < processed_length && processed_code[processing_idx + 1] == '|') {
                        result_vector.emplace_back(LOGIC_OR, "||", processing_idx);
                        ++processing_idx;
                    }
                    else {
                        result_vector.emplace_back(BIT_OR, "|", processing_idx);
                    }
                    break;

                case ';':
                    result_vector.emplace_back(SEMICOLON, ";", processing_idx);
                    break;

                case ',':
                    result_vector.emplace_back(COMMA, ",", processing_idx);
                    break;

                case '(':
                    result_vector.emplace_back(LEFT_BRACE, "(", processing_idx);
                    break;

                case '[':
                    result_vector.emplace_back(LEFT_SQUARE_BRACE, "[", processing_idx);
                    break;

                case ']':
                    result_vector.emplace_back(RIGHT_SQUARE_BRACE, "]", processing_idx);
                    break;

                case ')':
                    result_vector.emplace_back(RIGHT_BRACE, ")", processing_idx);
                    break;

                case '{':
                    result_vector.emplace_back(LEFT_CURLY_BRACE, "{", processing_idx);
                    break;

                case '}':
                    result_vector.emplace_back(RIGHT_CURLY_BRACE, "}", processing_idx);
                    break;

                case '\n':
                    break;

                case '$':
                    result_vector.emplace_back(EPSILON, "$", processing_idx);
                    break;

                case '#':
                    result_vector.emplace_back(END_SIGNAL, "#", processing_idx);
                    // todo break the loop(not implemented because the PreProcessor is not implemented completely)
                    break;

                case ' ':
                    break;

                default:
                    Diagnose::printError(processing_idx, "Unrecognized character here.");
                    return result_vector;
            }
        }
        ++processing_idx;
    }

    // process possible last token
    if (lexical_buffer_idx < LEXICAL_BUFFER_SIZE) {
        lexical_buffer[lexical_buffer_idx] = processed_code[processing_idx];
    }

    if (lexical_buffer_idx > 0) {
        result_vector.emplace_back(lexical_buffer, lexical_buffer_idx, processing_idx);
    }

    return result_vector;
}

bool is_digit(char c) {
    return (c >= '0' && c <= '9') || (c == '.');
}

bool is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_valid_in_var_fun(char c) {
    return is_digit(c) || is_letter(c) || c == '_';
}

Lexicon::Lexicon(const char* lexical_buffer, size_t buffer_size, int processing_idx) {
    // todo match key words
    if (buffer_size > 0) {
        lex_content = string(lexical_buffer, buffer_size);
        offset = processing_idx - buffer_size;
        if (is_digit(lexical_buffer[0])) {
            // might be number
            // check number
            int dot_cnt = 0;
            for (int i = 0; i < buffer_size; ++i) {
                if (!is_digit(lexical_buffer[i])) {
                    if (i == buffer_size - 1 && (lexical_buffer[i] == 'f' || lexical_buffer[i] == 'F')) {
                        // format eg. 4.42f
                        break;
                    }
                    else {
                        // todo throw error: INVALID_NUMBER_FORMAT
                        Diagnose::printError(offset, "Invalid number format.");
                        exit(-1);
                    }
                }
                else if (lexical_buffer[i] == '.') {
                    ++dot_cnt;
                }

                if (dot_cnt > 1) {
                    Diagnose::printError(offset, "Invalid number format.");
                    exit(-1);
                }
            }
            lex_type = KW_NUMBER;
        }
        else {
            // var / fun / key words
            // todo here can be binary search
            int idx = -1;
            for (int i = 0; i < KEYWORD_TYPE_CNT; ++i) {
                if (lex_content == KW_TYPES[i]) {
                    idx = i;
                }
            }
            if (idx == -1) {
                // check dot'.' is in it
                for (int i = 0; i < buffer_size; ++i) {
                    if (lexical_buffer[i] == '.') {
                        Diagnose::printError(offset + i, "Unexpected '.' here.");
                    }
                }
                lex_type = KW_VAR_FUN;
            }
            else {
                lex_type = -idx - 4;
                lex_content = KW_TYPES[idx];
            }
        }
    }
}

Lexicon::Lexicon(int type, string token, int processing_id) {
    lex_type = type;
    lex_content = std::move(token);
    offset = processing_id;
}

string Lexicon::get_type() const {
    string ret_lex_type;
    if (this->lex_type < 0) {
        if (this->lex_type == KW_NUMBER) {
            ret_lex_type = "NUMBER";
        }
        else if (this->lex_type == KW_VAR_FUN) {
            ret_lex_type = "VAR_FUN";
        }
        else if (this->lex_type <= -4 && this->lex_type > -4 - KEYWORD_TYPE_CNT) {
            // convert to upper
            ret_lex_type = "";
            for (char i : KW_TYPES[-(this->lex_type + 4)]) {
                // assume all i is lowercase
                ret_lex_type += (char)(i + 'A' - 'a');
            }
        }
    }
    else {
        switch (this->lex_type) {
            case ASSIGNMENT:
                ret_lex_type = "ASSIGNMENT";
                break;

            case ADD:
                ret_lex_type = "ADD";
                break;

            case MINUS:
                ret_lex_type = "MINUS";
                break;

            case STAR:
                ret_lex_type = "STAR";
                break;

            case DIV:
                ret_lex_type = "DIV";
                break;

            case EQUAL:
                ret_lex_type = "EQUAL";
                break;

            case GREAT:
                ret_lex_type = "GREAT";
                break;

            case GREAT_EQUAL:
                ret_lex_type = "GREAT_EQUAL";
                break;

            case LESS:
                ret_lex_type = "LESS";
                break;

            case LESS_EQUAL:
                ret_lex_type = "LESS_EQUAL";
                break;

            case NOT_EQUAL:
                ret_lex_type = "NOT_EQUAL";
                break;

            case INCREASE:
                ret_lex_type = "INCREASE";
                break;

            case DECREASE:
                ret_lex_type = "DECREASE";
                break;

            case SEMICOLON:
                ret_lex_type = "SEMICOLON";
                break;

            case COMMA:
                ret_lex_type = "COMMA";
                break;

            case LEFT_BRACE:
                ret_lex_type = "LEFT_BRACE";
                break;

            case RIGHT_BRACE:
                ret_lex_type = "RIGHT_BRACE";
                break;

            case LEFT_CURLY_BRACE:
                ret_lex_type = "LEFT_CURLY_BRACE";
                break;

            case RIGHT_CURLY_BRACE:
                ret_lex_type = "RIGHT_CURLY_BRACE";
                break;

            case LEFT_SQUARE_BRACE:
                ret_lex_type = "LEFT_SQUARE_BRACE";
                break;

            case RIGHT_SQUARE_BRACE:
                ret_lex_type = "RIGHT_SQUARE_BRACE";
                break;

            case LOGIC_NOT:
                ret_lex_type = "LOGIC_NOT";
                break;

            case BIT_AND:
                ret_lex_type = "BIT_AND";
                break;

            case LOGIC_AND:
                ret_lex_type = "LOGIC_AND";
                break;

            case BIT_OR:
                ret_lex_type = "BIT_OR";
                break;

            case LOGIC_OR:
                ret_lex_type = "LOGIC_OR";
                break;

            case EPSILON:
                ret_lex_type = "EPSILON";
                break;

            case END_SIGNAL:
                ret_lex_type = "END_SIGNAL";
                break;

            default:
                ret_lex_type = "NOT_LISTED";
                break;
        }
    }
    return ret_lex_type;
}

int Lexicon::convert_to_terminal_index() const
{
    // Key Word / Number / VarFunc mapping : -lex_type - 2
    // Signal mapping : lex_type + KEYWORD_TYPE_CNT + 2
    if (lex_type < 0) {
        return -lex_type - 2;
    }
    else {
        return lex_type + KEYWORD_TYPE_CNT + 2;
    }
};

void print_lexical_result(const vector<Lexicon>& result_vector)
{
    for (const Lexicon& lexicon : result_vector) {

        cout << '(' << lexicon.get_type() << ", \"" << lexicon.lex_content << "\")" << endl;
    }
}
