#!/bin/bash

make clean; make

./runtests.exp mylog.txt

find . -regex ".*\.c\|.*\.h" | xargs cpplint || true
