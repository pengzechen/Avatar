
#include "syscall.h"


// void test_shell() {
//     char c;
//     while (1) {
//         // 获取用户输入字符
//         c = getc();
        
//         // 如果输入回车键，换行
//         if (c == '\r') {
//             putc('\r');
//             putc('\n');
//             continue;
//         }
        
//         // 输出输入字符
//         putc(c);
        
//         // 如果输入是 'a'，fork 一个子进程执行 execve("add", ...)
//         if (c == 'a') {
//             int pid = fork();
//             if (pid == 0) {  // 子进程执行
//                 execve("add", (void*)0x40147000);
//             }
//             // 父进程继续循环等待下一次输入
//             sleep(1000000);
//         }
//         // 如果输入是 'b'，fork 一个子进程执行 execve("sub", ...)
//         else if (c == 'b') {
//             int pid = fork();
//             if (pid == 0) {  // 子进程执行
//                 execve("sub", (void*)0x40147000);
//             }
//             // 父进程继续循环等待下一次输入
//             sleep(1000000);
//         }
//     }
// }



int main() {
    // 启动测试 shell
    // test_shell();
    int pid = fork();
    if (pid == 0) {  // 子进程执行
        while(1) {
            for (int i = 0; i < 1000; i++) {
                uint64_t x = mutex_test_add();
            }
            mutex_test_print();
        }
        sleep(10000000);
    } else {
        while(1) {
            for (int i = 0; i < 1000; i++) {
                uint64_t x = mutex_test_minus();
            }
            mutex_test_print();
        }
        sleep(10000000);
    }
    return 0;
}