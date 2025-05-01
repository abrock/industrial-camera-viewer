#!/bin/bash
set -euo pipefail

git-stuff/clang-version || (echo "Refusing to run clang-format because the version check failed."; exit 1)
find . -type f -print0 | grep -ziE '\.(c|cc|cpp|h|hh|hpp)$' | xargs -0 -n1 -P$(nproc) clang-format --style=file:.clang-format -i
