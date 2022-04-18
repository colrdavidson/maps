clang -O3 src/emu.c -o emu
clang -O3 -Wno-void-pointer-to-enum-cast src/asm.c -o asm
