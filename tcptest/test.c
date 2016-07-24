#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    int max_buf_size = 256;

    char str[max_buf_size];
    char *str_send;
    int order;
    int clist[20];
    int i = 0;
    int len = 0;
    while (1)
    {
        i = 0;
        //printf("sscanf会阻塞么\n");
        if(fgets(str, max_buf_size, stdin))
        {
            len = strlen(str);
            while(1)
            {
                sscanf(str, "%d:%s", &order, str);
                if ((order != 0) && (len != strlen(str)))// && (strlen(str_send) > 3))
                {
                    clist[i++] = order;
                    printf("%d\n", order);
                    //printf("%s\n", str);
                    order = 0;
                    len = strlen(str);
                }
                else
                {
                    memset(str, 0, strlen(str));
                    break;
                }
                sleep(1);
            }
        }
        printf("over\n");
    }
    return 0;

}
