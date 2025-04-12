
.global putc
.global getc
.global sleep
.global mutex_test_add
.global mutex_test_minus
.global mutex_test_print

putc:
    mov x8, #0
    svc #0
    ret

getc:
    mov x8, #1
    svc #0
    ret

sleep:
    mov x8, #2
    svc #0
    ret

mutex_test_add:
    mov x8, #253
    svc #0
    ret

mutex_test_minus:
    mov x8, #254
    svc #0
    ret

mutex_test_print:
    mov x8, #252
    svc #0
    ret