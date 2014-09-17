#include <stdio.h>
#include <stdlib.h>

void bin(unsigned n)
{
        unsigned i;
	for (i = 1 << 31; i > 0; i = i / 2)
	    (n & i)? printf("1"): printf("0");
}

void bin1(unsigned n)
{
    if (n > 1)
	bin1(n/2);
    printf("%d", n % 2);
}


int main()
{
    int n = 9;
    bin1(n);
    return 0;
}
