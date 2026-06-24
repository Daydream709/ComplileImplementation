#!/bin/bash
# Compare student (tiger-compiler-26sp) vs reference (tiger-compiler lab6) implementation files
REF=/home/stu/tiger-compiler/tiger-compiler
STU=/home/stu/tiger-compiler/tiger-compiler-26sp

files=(
  src/tiger/parse/tiger.y
  src/tiger/lex/tiger.lex
  src/tiger/lex/scanner.h
  src/tiger/semant/semant.cc
  src/tiger/semant/semant.h
  src/tiger/escape/escape.cc
  src/tiger/escape/escape.h
  src/tiger/frame/frame.h
  src/tiger/frame/temp.cc
  src/tiger/frame/temp.h
  src/tiger/frame/x64frame.cc
  src/tiger/frame/x64frame.h
  src/tiger/translate/translate.cc
  src/tiger/translate/translate.h
  src/tiger/translate/tree.cc
  src/tiger/translate/tree.h
  src/tiger/canon/canon.cc
  src/tiger/canon/canon.h
  src/tiger/codegen/assem.cc
  src/tiger/codegen/assem.h
  src/tiger/codegen/codegen.cc
  src/tiger/codegen/codegen.h
  src/tiger/regalloc/color.cc
  src/tiger/regalloc/color.h
  src/tiger/regalloc/regalloc.cc
  src/tiger/regalloc/regalloc.h
  src/tiger/liveness/flowgraph.cc
  src/tiger/liveness/flowgraph.h
  src/tiger/liveness/liveness.cc
  src/tiger/liveness/liveness.h
  src/tiger/util/graph.h
  src/tiger/util/table.h
)

printf "%-45s %8s %8s %8s\n" "FILE" "REF_len" "STU_len" "DIFFlines"
echo "-------------------------------------------------------------------------"
for f in "${files[@]}"; do
  if [ ! -f "$REF/$f" ] || [ ! -f "$STU/$f" ]; then
    printf "%-45s %s\n" "$f" "MISSING"
    continue
  fi
  rl=$(tr -d '\r' < "$REF/$f" | wc -l)
  sl=$(tr -d '\r' < "$STU/$f" | wc -l)
  dl=$(diff <(tr -d '\r' < "$REF/$f") <(tr -d '\r' < "$STU/$f") 2>/dev/null | grep -c '^[<>]')
  flag=""
  if [ "$dl" -gt 0 ]; then flag="  <-- DIFFERS"; fi
  printf "%-45s %8s %8s %8s%s\n" "$f" "$rl" "$sl" "$dl" "$flag"
done
