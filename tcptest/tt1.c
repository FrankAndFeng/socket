#include <stdio.h>
#include <stdlib.h>

int main()
{
    int a = 4, b = 5, c = 6;
    char str[20];
    sprintf(str, "%02d:", a);
    sprintf(str+3, "%02d:", b);
    sprintf(str+6, "%02d:", c);
    printf("a = %d, strig = %s\n", a, str);
    return 0;
}
