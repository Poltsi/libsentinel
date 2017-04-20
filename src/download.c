#include <stdio.h>
#include "libsentinel.h"
 
int main(void)
{
    puts("This is a shared library test...");
    char *connection;
    connect_sentinel(connection);
    return 0;
}
