// qsort.c
// Quicksort implementation
// <stdlib.h>
// Author: Stre4K 
// Date: 2025-02-22

#include <stdlib.h>
#include <string.h>

const int INSERTION_SORT_THRESHOLD = 10;
void insertion_sort(void *ptr, size_t count, size_t size, int (*comp)(const void *, const void *));
int partition(void *ptr, size_t count, size_t size, int (*comp)(const void *, const void *));
void swap(void *a, void *b, size_t size);


void qsort(void *ptr, size_t count, size_t size, int (*comp)(const void *, const void *)) {
    if (ptr == NULL || count <= 1) {
        return;
    }
    if (count <= INSERTION_SORT_THRESHOLD) {
        insertion_sort(ptr, count, size, comp);
    } else {
        int pivot_index = partition(ptr, count, size, comp);
        qsort(ptr, pivot_index, size, comp);
        qsort((char*)ptr + (pivot_index + 1) * size, count - pivot_index - 1, size, comp);
    }
}


int partition(void *ptr, size_t count, size_t size, int (*comp)(const void *, const void *)) {
    char *arr = (char *)ptr;
    int high = count - 1;
    int pivot_index = high; // Choosing last element as pivot
    char *pivot = arr + pivot_index * size;

    int i = -1; // Position for smaller element
    for (int j = 0; j < high; j++) {
        if (comp(arr + j * size, pivot) < 0) {
            i++;
            swap(arr + i * size, arr + j * size, size);
        }
    }
    swap(arr + (i + 1) * size, pivot, size);
    return i + 1; // New pivot index
}


void swap(void *a, void *b, size_t size) {
    char buffer[size];
    memcpy(buffer, a, size);
    memcpy(a, b, size);
    memcpy(b, buffer, size);
       
}


void insertion_sort(void *ptr, size_t count, size_t size, int (*comp)(const void *, const void *)) {
    char *arr = (char *)ptr;

    for (size_t i = 1; i < count; i++) {
        size_t j = i;
        char temp[size];
        memcpy(temp, arr + i * size, size);

        while (j > 0 && comp(arr + (j - 1) * size, temp) > 0) {
            memcpy(arr + j * size, arr + (j - 1) * size, size);
            j--;
        }
        memcpy(arr + j * size, temp, size);
    }
}
