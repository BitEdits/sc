#!/bin/bash

echo "Compiling..."

gcc src/tv.c src/utf8.c -o tv

if [ $? -eq 0 ]; then
  echo "OK"
else
  echo "ERROR: $?"
fi
