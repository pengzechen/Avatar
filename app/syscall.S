
.global putc
.global getc

putc:
    mov x8, #0
    svc #0
    ret

getc:
    mov x8, #1
    svc #0
    ret