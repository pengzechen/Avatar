
.global putc
.global getc
.global sleep

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