
//int func(int s, int t, int g)
//{
//    return s + 5;
//}
//
//int main()
//{
//    int a = 4 + 7;
//    int b = func(a + 8, 0, 0);
//    a = a + b;
//    return a;
//}

//int fib(int x) {
//    if (x <= 1) {
//        return 1;
//    }
//    else {
//        return fib(x - 1) + fib(x - 2);
//    }
//}
//
//
//int main()
//{
//    return fib(40);
//}

float func(float fin)
{
    return 3.3f;
}

int main()
{
    int g_a[43];

    g_a[3] = 5;
    g_a[4] = 4;
    g_a[6] = 3;

    int a, b, c, d;

    float ff = g_a[3];
    ff = ff + func(ff + g_a[6]);
    int i = ff / 2;
    return 2 + g_a[i];
}