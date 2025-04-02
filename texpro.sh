#!/bin/bash

echo "Compiling..."

gcc src/texpro.c -o texpro

if [ $? -eq 0 ]; then
  echo "OK"
else
  echo "ERROR: $?"
fi
