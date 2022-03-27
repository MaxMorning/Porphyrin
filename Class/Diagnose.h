//
// Project: Porphyrin
// File Name: Diagnose.h
// Author: Morning
// Description:
//
// Create Date: 2021/12/13
//

#ifndef PORPHYRIN_DIAGNOSE_H
#define PORPHYRIN_DIAGNOSE_H

#include <string>
#include <iostream>
#include <vector>
#include <sstream>

using namespace std;

class Diagnose {
public:
    static void printError(int error_idx, string information);
    static void printWarning(int error_idx, string information);
    static void setSource(string& origin_code);
    static void setProcessed(string& code);
    static void printStream(bool enable_print);

private:
    static string source_code;
    static string processed_code;
    static vector<int> origin_row_offset;
    static vector<int> processed_row_offset;
    static stringstream warning_stream;
};


#endif //PORPHYRIN_DIAGNOSE_H
