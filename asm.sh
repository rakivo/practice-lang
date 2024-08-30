#!/usr/bin/bash

if [ "$ASM" = "nasm" ]; then
  nasm $FILENAME.asm $ASMFLAGS -f elf64 -g -F dwarf
elif [ "$ASM" = "fasm" ]; then
  fasm $FILENAME.asm $ASMFLAGS
else
  echo 'UNEXPECTED $ASM VALUE'
  exit 1
fi

if [ -z "$FILENAME" ]; then
  echo 'DEFINE $FILENAME'
  exit 1
fi

ld -o $FILENAME $FILENAME.o
time ./$FILENAME
