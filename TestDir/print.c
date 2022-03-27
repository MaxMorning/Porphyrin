//
// Project: Porphyrin
// File Name: print.c
// Author: Morning
// Description:
//
// Create Date: 2022/3/26
//

#include <stdio.h>
#include <time.h>

double gmm_main();

int main()
{
    clock_t start_time = clock();

    double result = gmm_main();

    clock_t end_time = clock();

    printf("Result : %.16lf\n", result);

    printf("Time : %ld", (end_time - start_time));

    return 0;
}