#!/usr/bin/env bash
# =============================================================
# EdgeFlow Phase 2 — Automated Test Suite (AOT LLVM Compiler)
# Run from the project root after building:
#   cmake -S . -B build -G Ninja && cmake --build build
#   bash tests/run_phase2_tests.sh
# =============================================================

COMPILER="./build/edgeflow"
RUNTIME_DIR="./build"
EXAMPLES="./examples"
PASS=0
FAIL=0
RED='\033[0;31m'
GRN='\033[0;32m'
NC='\033[0m'

if [ ! -f "$COMPILER" ]; then
    echo -e "${RED}[ERROR]${NC} Compiler executable $COMPILER not found. Run 'cmake --build build' first."
    exit 1
fi

OPENCV_LIBS=$(pkg-config --libs opencv4 2>/dev/null || echo "-lopencv_core -lopencv_imgproc -lopencv_imgcodecs")

run_test() {
    local name="$1"
    local file="$2"
    local expected="$3"
    local ll_file="/tmp/${name}.ll"
    local bin_file="/tmp/${name}_bin"

    "$COMPILER" "$file" -o "$ll_file" 2>/dev/null
    if [ ! -f "$ll_file" ]; then
        echo -e "${RED}[FAIL]${NC} $name (Code generation failed)"
        FAIL=$((FAIL + 1))
        return
    fi

    clang++ "$ll_file" -o "$bin_file" -L"$RUNTIME_DIR" -ledgeruntime $OPENCV_LIBS 2>/dev/null
    if [ ! -f "$bin_file" ]; then
        echo -e "${RED}[FAIL]${NC} $name (Clang linking failed)"
        FAIL=$((FAIL + 1))
        rm -f "$ll_file"
        return
    fi

    actual=$("$bin_file" 2>/dev/null | tr -d '\r')
    rm -f "$ll_file" "$bin_file"

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

echo "=== Running Phase 2 AOT Compiler Tests ==="
run_test "basic_arithmetic" "$EXAMPLES/basic.edge" "50
1"

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

run_test "functions_recursion" "$EXAMPLES/functions.edge" "15
120"

run_test "arrays" "$EXAMPLES/arrays.edge" "99
5"

run_test "insertion_sort" "$EXAMPLES/insertion_sort.edge" "1
3
5
7
8"

TOTAL=$((PASS + FAIL))
echo ""
echo "======================================="
echo "  $PASS/$TOTAL Phase 2 tests passed"
echo "======================================="
[ $FAIL -eq 0 ] && exit 0 || exit 1
