//int program(int a, int b, int c) {
//    int i;
//    int j;
//    i = 0;
//    if (a > (b + c)) {
//        j = a + (b * c + 1);
//    } else {
//        j = a;
//    }
//    while (i <= 100) {
//        i = j * 2;
//        j = j * 2;
//    }
//    return i;
//}
//
//int demo(int a) {
//    a = a + 2;
//    return a * 2;
//}
//
//int main() {
//    int a[2][2];
//    a[0][0] = 3;
//    a[0][1] = a[0][0] + 1;
//    a[1][0] = a[0][0] + a[0][1];
//    a[1][1] = program(a[0][0], a[0][1], demo(a[1][0]));
//    return a[1][1];
//}

float to_float(int i)
{
    return i;
}

double calc_pi()
{
    int i = 1;
    double sum = 0;
    int signal = 1;

    while (i < 40) {
        sum = sum + signal * to_float(i);
        i = i + 2;
        signal = -signal;
    }

    sum = sum * 4;

    return 5.2;
}