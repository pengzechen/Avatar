/*
 * Copyright (c) 2024 Avatar Project
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 *
 * @file main.c
 * @brief Implementation of main.c
 * @author Avatar Project Team
 * @date 2024
 */


#include "syscall.h"

int
_start()
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
        for (int i = 0; i < 1000; i++) {
            // uint64_t x = mutex_test_add();
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
            putc('a');
            putc('\r');
            putc('\n');
            sleep(100);
            // while(1);
            // ;
        }
        // mutex_test_print();
    }
    sleep(10000000);

    return 0;
}