#!/usr/bin/env bash
# check_clang_format.sh — run the same C format check as CI.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if ! command -v clang-format-18 >/dev/null 2>&1; then
	echo "ERROR: clang-format-18 not found. Install with: sudo apt install clang-format-18" >&2
	exit 1
fi

mapfile -t files < <(
	find app/src app/include app/tests/src -type f \( -name '*.c' -o -name '*.h' \) 2>/dev/null
)
if [[ ${#files[@]} -eq 0 ]]; then
	echo "No C sources found"
	exit 0
fi

echo "Using: $(clang-format-18 --version)"
clang-format-18 --dry-run --Werror "${files[@]}"
