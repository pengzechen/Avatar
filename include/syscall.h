
#ifndef SYSCALL_H
#define SYSCALL_H
#include <aj_types.h>

char getc();
void putc(char c);
void sleep(uint64_t ms);

#endif // SYSCALL_H