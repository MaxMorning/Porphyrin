#define ARRAY_SIZE 16384

int fun(float arr[ARRAY_SIZE], int low, int high) {
    float key;
    key = arr[low];
    while (low < high) {
        while (low < high && arr[high] >= key) {
            high = high - 1;
        }

        if (low < high) {
            arr[low] = arr[high];
            low = low + 1;
        }

        while (low < high && arr[low] <= key) {
            low = low + 1;
        }

        if (low < high) {
            arr[high] = arr[low];

            high = high - 1;
        }
    }

    arr[low] = key;
    return low;
}

void quickSort(float arr[ARRAY_SIZE], int start, int end) {
    int pos;
    if (start < end) {
        pos = fun(arr, start, end);
        quickSort(arr, start, pos - 1);
        quickSort(arr, pos + 1, end);
    }
}

double gmm_main() {
    // gen array content
    float arr[ARRAY_SIZE];

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