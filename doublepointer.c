#include <stdio.h>

int main(void) {
    int a = 10;
    int b = 20;
    int* p = &a;
    int** pp = &p;

    printf("a = %d\n", a);
    printf("*p = %d\n", *p);
    printf("**pp = %d\n", **pp);

    *pp = &b;

    printf("a = %d\n", a);
    printf("*p = %d\n", *p);
    printf("**pp = %d\n", **pp);

    printf("%p\n", (int*)NULL+1);
    printf("%p\n", (int**)NULL+1);
    printf("%p\n", (int***)NULL+1);

    printf("%p\n", (void*)NULL+1);
    printf("%p\n", (void**)NULL+1);
    printf("%p\n", (void***)NULL+1);

    return 0;
}