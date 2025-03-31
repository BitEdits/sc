#!/bin/bash

echo "Compiling..."

gcc src/sc.c src/menus.c src/input.c src/files.c -o sc

echo "OK"
