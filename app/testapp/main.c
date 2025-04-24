
#include "syscall.h"


int main() {

    int x = execve("add", (void*)0x42620c00);
    return 0;
}