#define LOOP_TIME 131072

int fun(int p0, int p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8) {
    return p0 + p1 * p2 + p3 / p4 - p5 * p6 + p7 + p8;
}

double gmm_main() {
    int i = 0;
    double sum = 0;
    while (i < LOOP_TIME) {
        sum = fun(2, 1, 5, 9, 35, 2, 124, 1, 42);
        i = i + 1;
    }

    return sum;
}