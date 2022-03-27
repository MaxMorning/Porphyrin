//
// Project: Porphyrin
// File Name: Diagnose.cpp.cc
// Author: Morning
// Description:
//
// Create Date: 2021/12/13
//

#include "Diagnose.h"
#include <iomanip>
#include <fstream>

string Diagnose::source_code;
string Diagnose::processed_code;
vector<int> Diagnose::origin_row_offset;
vector<int> Diagnose::processed_row_offset;
stringstream Diagnose::warning_stream;

void Diagnose::printError(int error_idx, string information)
{
    // get row index
    int row_idx = -1;
    for (int i = 0; i < processed_row_offset.size(); ++i) {
        if (processed_row_offset[i] > error_idx) {
            row_idx = i - 1;
            break;
        }
    }

    int col_idx = error_idx - processed_row_offset[row_idx];

    cerr << "[ERROR] " << row_idx + 1 << ':' << col_idx + 1 << ": " << information << endl;

    // print
    for (int i = origin_row_offset[row_idx]; i < origin_row_offset[row_idx + 1]; ++i) {
        cerr << source_code[i];
    }

    for (int i = 0; i < col_idx; ++i) {
        cerr << ' ';
    }

    cerr << '^';

    exit(-1);
}

void Diagnose::printWarning(int error_idx, string information)
{
    // get row index
    int row_idx = -1;
    for (int i = 0; i < processed_row_offset.size(); ++i) {
        if (processed_row_offset[i] > error_idx) {
            row_idx = i - 1;
            break;
        }
    }

    int col_idx = error_idx - processed_row_offset[row_idx];

    warning_stream << "[WARNING] " << row_idx + 1 << ':' << col_idx + 1 << ": " << information << endl;

    // print
    for (int i = origin_row_offset[row_idx]; i < origin_row_offset[row_idx + 1]; ++i) {
        warning_stream << source_code[i];
    }

    for (int i = 0; i < col_idx; ++i) {
        warning_stream << ' ';
    }

    warning_stream << "^\n";
}

void Diagnose::printStream(bool enable_print)
{
    string warning_string = warning_stream.str();
    cout << warning_string << endl;

    if (enable_print) {
        ofstream fout("Warning.txt");
        fout << warning_string << endl;
        fout.close();
    }
}

void Diagnose::setSource(string& origin_code)
{
    source_code.assign(origin_code);

    // build offset index
    origin_row_offset.clear();

    origin_row_offset.push_back(0);
    for (int i = 0; i < origin_code.size(); ++i) {
        if (origin_code[i] == '\n') {
            origin_row_offset.push_back(i + 1);
        }
    }

    origin_row_offset.push_back(origin_code.size());
}

void Diagnose::setProcessed(string &code)
{
    processed_code.assign(code);

    // build offset index
    processed_row_offset.clear();

    processed_row_offset.push_back(0);
    for (int i = 0; i < code.size(); ++i) {
        if (code[i] == '\n') {
            processed_row_offset.push_back(i + 1);
        }
    }

    processed_row_offset.push_back(code.size());
}

