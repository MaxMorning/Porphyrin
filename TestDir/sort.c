#define ARRAY_SIZE 32768

void quickSort(int arr[ARRAY_SIZE], int first, int last) {
    int i, j, pivot;
    int temp;
    if (first < last) {
        pivot = first;
        i = first;
        j = last;
        while (i < j) {
            while (arr[i] <= arr[pivot] && i < last) {
                i = i + 1;
            }

            while (arr[j] > arr[pivot]) {
                j = j + 1;
            }

            if (i < j) {
                temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
        temp = arr[pivot];
        arr[pivot] = arr[j];
        arr[j] = temp;
        quickSort(arr, first, j - 1);
        quickSort(arr, j + 1, last);
    }
}

double gmm_main() {
    // gen array content
    int arr[ARRAY_SIZE];

    int i = 0;
    while (i < ARRAY_SIZE) {
        arr[i] = ARRAY_SIZE - i;

        i = i + 1;
    }

    // sort
    quickSort(arr, 0, ARRAY_SIZE - 1);

    // check correction
    i = 0;
    while (i < ARRAY_SIZE - 1) {
        if (arr[i] > arr[i + 1]) {
            return 1;
        }

        i = i + 1;
    }

    return 0;
}