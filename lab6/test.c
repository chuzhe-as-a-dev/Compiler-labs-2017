#include <stdio.h>

void f2() {
    printf("xixi");
}

void f1() {
    f2();
    printf("haha");
}


int main() {
    f1();
    f2();
    printf("hehe\n\t");

    return 0;
}