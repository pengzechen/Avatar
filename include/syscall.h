
#ifndef SYSCALL_H
#define SYSCALL_H
#include <aj_types.h>

char getc();
void putc(char c);
void sleep(uint64_t ms);

uint64_t mutex_test_add();
uint64_t mutex_test_minus();
void mutex_test_print ();

#endif // SYSCALL_H