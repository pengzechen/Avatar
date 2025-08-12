# GDB 
```bash
wget https://ftp.gnu.org/gnu/gdb/gdb-14.2.tar.xz
tar -xf gdb-14.2.tar.xz
cd gdb-14.2
../configure --target=aarch64-linux-gnu \
             --prefix=/opt/gdb-14 \
             --with-expat \
             --with-python \
             --enable-tui
make -j$(nproc)
make install
```

# GCC/G++ Version: 
gcc (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
Copyright (C) 2023 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
Copyright (C) 2023 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Qemu
QEMU emulator version 9.2.4
Copyright (c) 2003-2024 Fabrice Bellard and the QEMU Project developers

# aarch64-linux-musl-gcc 

```bash
wget https://musl.cc/aarch64-linux-musl-cross.tgz
tar zxf aarch64-linux-musl-cross.tgz
```
aarch64-linux-musl-gcc (GCC) 11.2.1 20211120
Copyright (C) 2021 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.