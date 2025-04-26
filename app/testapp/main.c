
#include "syscall.h"


int main() {

    int x = execve("add", (void*)0x40147000);
    return 0;
}