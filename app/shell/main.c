
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
    for (int i = 0; i < 1000; i++) {
        
        uint64_t x = mutex_test_add();
        // putc('A');
        // putc('B');
        // putc('C');
        // putc('D');
        // putc('E');
        // putc('F');
        // putc('G');
        // putc('H');
        // putc('I');
        // putc('J');
        // putc('K');
        // putc('L');
        // putc('M');
        // putc('\r');
        // putc('\n');
        sleep(20);
        
    }

    mutex_test_print();
    sleep(10000000);

    return 0;
}