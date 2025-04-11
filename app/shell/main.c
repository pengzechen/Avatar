
#include "syscall.h"

int main()
{
    // while (1)
    // {
    //     char c = getc();
    //     if(c == '\r') {
    //         putc('\r');
    //         putc('\n');
    //     }
    //     putc(c);
    // }
    while (1) {
        putc('A');
        putc('B');
        putc('C');
        putc('D');
        putc('E');
        putc('F');
        putc('G');
        putc('H');
        putc('I');
        putc('J');
        putc('K');
        putc('L');
        putc('M');
        putc('\r');
        putc('\n');
        sleep(1000);
    }

    return 0;
}