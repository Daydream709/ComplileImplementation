#!/bin/bash
cd /tmp/online_sim/build || exit 1
TC=/tmp/online_sim/testdata/lab5or6/testcases/trec.tig
./tiger-compiler "$TC" 2>/tmp/tiger_err.txt
echo "TIGER_EXIT=$?"
echo "--- tiger stderr ---"
head -20 /tmp/tiger_err.txt
echo "--- generated .s head ---"
head -50 "$TC.s" 2>/dev/null
echo "--- .s line count ---"
wc -l "$TC.s" 2>/dev/null
echo "--- gcc link attempt ---"
gcc -Wl,--wrap,getchar -m64 "$TC.s" /tmp/online_sim/src/tiger/runtime/runtime.c -o /tmp/test.out 2>&1 | head -30
echo "GCC_EXIT=${PIPESTATUS[0]}"
ls -la /tmp/test.out 2>/dev/null
