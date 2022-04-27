float to_float(int i)
{
    return i;
}

double gmm_main()
{
    int i = 1;
    double sum = 0;
    int signal = 1;

    while (i < 214748360) {
        sum = sum + signal / to_float(i);
        i = i + 2;
        signal = -signal;
    }

    sum = sum * 4;

    return sum;
}
