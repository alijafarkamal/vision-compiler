#!/usr/bin/env bash
# =============================================================
# EdgeFlow Phase 1 — Automated Test Suite
# Run from the project root after building:
#   cmake -S . -B build -G Ninja && cmake --build build
#   bash tests/run_phase1_tests.sh
# =============================================================

INTERP="./build/edgeflow-interp"
if [ ! -f "$INTERP" ]; then
    if [ -f "./build/edgeflow-interp.exe" ]; then
        INTERP="./build/edgeflow-interp.exe"
    elif [ -f "./build/Debug/edgeflow-interp.exe" ]; then
        INTERP="./build/Debug/edgeflow-interp.exe"
    elif [ -f "./build/Release/edgeflow-interp.exe" ]; then
        INTERP="./build/Release/edgeflow-interp.exe"
    fi
fi
EXAMPLES="./examples"
PASS=0
FAIL=0
RED='\033[0;31m'
GRN='\033[0;32m'
NC='\033[0m'

run_test() {
    local name="$1"
    local file="$2"
    local expected="$3"

    actual=$("$INTERP" "$file" 2>/dev/null | tr -d '\r')
    if [ "$actual" = "$expected" ]; then
        echo -e "${GRN}[PASS]${NC} $name"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}[FAIL]${NC} $name"
        echo "  Expected: $(echo "$expected" | head -3)"
        echo "  Got:      $(echo "$actual"   | head -3)"
        FAIL=$((FAIL + 1))
    fi
}

# ---- Test 1: Basic arithmetic & expressions ----
run_test "basic_arithmetic" "$EXAMPLES/basic.edge" "50
1"

# ---- Test 2: Control flow (if/else + while + range loop) ----
run_test "control_flow" "$EXAMPLES/control_flow.edge" "1
5
4
3
2
1
0
1
2
3
4"

# ---- Test 3: Functions & recursion ----
run_test "functions_recursion" "$EXAMPLES/functions.edge" "15
120"

# ---- Test 4: Arrays ----
run_test "arrays" "$EXAMPLES/arrays.edge" "99
5"

# ---- Test 5: Insertion Sort ----
run_test "insertion_sort" "$EXAMPLES/insertion_sort.edge" "1
3
5
7
8"

# ---- Summary ----
TOTAL=$((PASS + FAIL))
echo ""
echo "======================================="
echo "  $PASS/$TOTAL tests passed"
echo "======================================="
[ $FAIL -eq 0 ] && exit 0 || exit 1
