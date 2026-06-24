#!/bin/bash
# Exact online simulation: template (lab6) + student zip overlay + Release build + lab6 tests
set -u

TEMPLATE=/home/stu/tiger-compiler/tiger-compiler
ZIP=/home/stu/tiger-compiler/tiger-compiler-26sp/lab6-answer.zip
SIM=/tmp/online_sim

rm -rf "$SIM"
cp -r "$TEMPLATE" "$SIM"
cd "$SIM" || exit 1
rm -rf .git build *.zip *.pdf
# Convert everything to LF (matches the real online Linux env)
find "$SIM" -type f \( -name '*.cc' -o -name '*.h' -o -name '*.lex' -o -name '*.y' -o -name '*.sh' -o -name 'CMakeLists.txt' -o -name '*.py' -o -name '*.c' -o -name '*.in' -o -name '*.out' -o -name '*.txt' -o -name '*.tig' \) -exec sed -i 's/\r$//' {} +

# Extract student zip and overlay each file by basename (find-and-replace)
rm -rf /tmp/zipfiles && mkdir -p /tmp/zipfiles
unzip -o "$ZIP" -d /tmp/zipfiles >/dev/null
echo "=== Overlaying student files ==="
for f in /tmp/zipfiles/*; do
  bn=$(basename "$f")
  # find matching file in src tree
  match=$(find "$SIM/src" -name "$bn" | head -1)
  if [ -z "$match" ]; then
    echo "  NO MATCH for $bn (skipped)"
  else
    cp "$f" "$match"
    echo "  $bn -> ${match#$SIM/}"
  fi
done

# Build (Release, exactly like grade.sh)
echo ""
echo "=== BUILD ==="
rm -rf build && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. >/tmp/sim_cmake.log 2>&1
if [ $? -ne 0 ]; then
  echo "CMAKE FAILED:"; tail -30 /tmp/sim_cmake.log; exit 1
fi
make tiger-compiler -j >/tmp/sim_build.log 2>&1
if [ $? -ne 0 ]; then
  echo "BUILD FAILED (last 50 lines):"; tail -50 /tmp/sim_build.log; exit 1
fi
echo "BUILD OK"

# Run lab6 tests via grade.sh
echo ""
echo "=== LAB6 TESTS ==="
cd "$SIM"
bash scripts/grade.sh lab6 2>&1 | tail -40
