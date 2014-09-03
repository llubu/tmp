#include <stdio.h>
#include <stdlib.h>

void insert_sort(int *arr, int len)
{
    int i = 0, j =0;

    for ( i = 0; i<n; i++) {
	for (j =i; j > 0; j--) {
	    if (arr[j-1] > arr[j])
		swap(arr, j-1, j);
	}
}
