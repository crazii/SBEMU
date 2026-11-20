#!/bin/sh
cd src || exit 1
test -f main.c
make CC=gcc CXX=g++
test -f output/sbemu.exe
