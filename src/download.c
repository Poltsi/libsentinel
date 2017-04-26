#include <stdio.h>
#include "libsentinel.h"
 
int main(void)
{
    puts("This is a shared library test...");
    int fd = connect_sentinel();
    printf("We have a connection: %d\n", fd);
    return 0;
}
