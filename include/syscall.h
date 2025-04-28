
#ifndef SYSCALL_H
#define SYSCALL_H
#include <aj_types.h>

char getc();
void putc(char c);
void sleep(uint64_t ms);
int execve(char *name, void *elf_addr) ;
int fork();

uint64_t mutex_test_add();
uint64_t mutex_test_minus();
void mutex_test_print ();

#endif // SYSCALL_H