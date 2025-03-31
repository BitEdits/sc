#!/bin/bash

echo "Compiling..."

gcc src/start.c src/menus.c src/input.c src/files.c -o sc -lws2_32 -fpermissive

echo "OK"
