#include<stdio.h>
int main(int argc, char **argv) {
int x = 1;
printf("Hello x = %d\n", x);
asm("movl %1, %%eax;"
        "addl $1, %%eax;"
        "movl %%eax, %0;"
        :"=r"(x)
        :"r"(x)
        :"%eax"
);
printf("Hello x = %d after increment\n", x);
if(x == 2) printf("OK\n");
else printf("ERROR\n");
}
