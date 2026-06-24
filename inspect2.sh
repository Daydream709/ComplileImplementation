#!/bin/bash
cd /home/stu/tiger-compiler/tiger-compiler-26sp/build || exit 1
TC=/home/stu/tiger-compiler/tiger-compiler-26sp/testdata/lab5or6/testcases/trec.tig
./tiger-compiler "$TC" 2>/tmp/tiger_err.txt
echo "EXIT=$?"
echo "--- .s head ---"
head -30 "$TC.s" 2>/dev/null
echo "--- gcc link ---"
gcc -Wl,--wrap,getchar -m64 "$TC.s" /home/stu/tiger-compiler/tiger-compiler-26sp/src/tiger/runtime/runtime.c -o /tmp/test.out 2>&1 | head -20
