#!/bin/bash

echo "Compiling..."

gcc src/sc.c src/menus.c \
    src/input.c src/files.c -o sc

if [ $? -eq 0 ]; then
  echo "OK"
else
  echo "ERROR: $?"
fi
