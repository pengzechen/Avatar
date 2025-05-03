
#include "syscall.h"

int main()
{
    while (1)
    {
        char c = getc();
        if(c == '\r') {
            putc('\r');
            putc('\n');
        }
        putc(c);
    }

    return 0;
}