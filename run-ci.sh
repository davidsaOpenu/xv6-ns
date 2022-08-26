#!/bin/bash

make clean; make

./runtests.exp mylog.txt

find . -regex ".*\.c\|.*\.h" | xargs /usr/local/bin/cpplint --output=vs7 || true
