https://softool.cn/read/embedded_linux/20072402.html
https://sourceware.org/ftp/newlib/index.html
newlib-2.2.0.20150924


以下配置可以正常使用printf  版本 4.1
./configure     \
--host=aarch64      \
--prefix=/home/ajax-osdev/nlbuild     \
--enable-option-checking     \
--enable-newlib-global-stdio-streams     \
--enable-newlib-reent-small     \
--disable-newlib-supplied-syscalls     \
--disable-multilib     \
--disable-newlib-io-float     \
CC=aarch64-linux-gnu-gcc     \
CFLAGS='-O0 -g -fno-stack-protector -DPREFER_SIZE_OVER_SPEED'