#!/usr/bin/env bash
# Generate an app-only HTML coverage report from a twister --coverage run.
#
# Workflow:
#   1. Run twister with --coverage --coverage-tool lcov from the workspace root
#      so file paths in the tracefile are absolute and match $PWD/app/src/*.
#   2. This script extracts only app/src/* and app/include/* from the
#      tracefile, then renders HTML.
#
# Pattern follows the standard Zephyr application coverage flow:
#   https://docs.zephyrproject.org/latest/develop/test/coverage.html
#   https://github.com/nrfconnect/Asset-Tracker-Template (sonarcloud.yml)
#   https://github.com/nrfconnect/sdk-sidewalk (run_tests.yml)
#
# Usage: scripts/coverage_report.sh [twister-out-dir]
#   Default twister-out-dir: twister-out
set -euo pipefail

WORKSPACE="$(cd "$(dirname "$0")/.." && pwd)"
TWISTER_OUT="${1:-${WORKSPACE}/twister-out}"
TRACEFILE="${TWISTER_OUT}/coverage.info"
APP_INFO="${TWISTER_OUT}/coverage_app.info"
HTML_DIR="${TWISTER_OUT}/coverage_app"

if [[ ! -f "${TRACEFILE}" ]]; then
    echo "ERROR: ${TRACEFILE} not found." >&2
    echo "Run twister first, e.g.:" >&2
    echo "  cd ${WORKSPACE}" >&2
    echo "  ./modules/zephyr/scripts/twister -T app/tests --coverage \\" >&2
    echo "      -p native_sim --coverage-tool lcov \\" >&2
    echo "      --coverage-basedir \"\$PWD\" --inline-logs" >&2
    exit 1
fi

cd "${WORKSPACE}"

echo "[coverage] Extracting app/src and app/include from ${TRACEFILE}"
lcov --quiet \
    --extract "${TRACEFILE}" \
        "${WORKSPACE}/app/src/*" \
        "${WORKSPACE}/app/include/*" \
    --output-file "${APP_INFO}" \
    --rc lcov_branch_coverage=1 \
    --ignore-errors unused

echo "[coverage] Summary:"
lcov --summary "${APP_INFO}" --rc lcov_branch_coverage=1 || true

echo "[coverage] Generating HTML at ${HTML_DIR}"
genhtml --quiet \
    --output-directory "${HTML_DIR}" \
    --branch-coverage \
    --legend \
    --title "Thingy:52 Env Monitor - app/src" \
    "${APP_INFO}"

# Audit: list app/src/*.c files NOT exercised by any test (no .gcda produced).
# These are the files the report cannot include because no native_sim test
# binary linked them.
echo ""
echo "[coverage] Files in app/src/ NOT linked by any native_sim test:"
ALL_SRC=$(find "${WORKSPACE}/app/src" -name '*.c' | sort)
COVERED_SRC=$(grep -E '^SF:' "${APP_INFO}" | sed 's/^SF://' | sort -u)
UNCOVERED=$(comm -23 <(echo "${ALL_SRC}") <(echo "${COVERED_SRC}"))
if [[ -z "${UNCOVERED}" ]]; then
    echo "  (none -- all app/src/*.c files are exercised)"
else
    echo "${UNCOVERED}" | sed "s|${WORKSPACE}/||" | sed 's/^/  - /'
fi

echo ""
echo "[coverage] Open: ${HTML_DIR}/index.html"
