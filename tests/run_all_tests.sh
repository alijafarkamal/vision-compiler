#!/usr/bin/env bash
# =============================================================
# EdgeFlow — Unified Master Test Suite (Phase 1 & Phase 2)
# =============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=========================================================="
echo " Running EdgeFlow Unified Test Suite (Phase 1 & Phase 2) "
echo "=========================================================="
echo ""

echo "--- 1. Testing Phase 1: Pure Flex + Bison Interpreter ---"
bash "$ROOT_DIR/tests/run_phase1_tests.sh"

echo ""
echo "--- 2. Testing Phase 2: Ahead-Of-Time (AOT) LLVM Compiler ---"
bash "$ROOT_DIR/tests/run_phase2_tests.sh"

echo ""
echo "=========================================================="
echo "  ALL PHASE 1 AND PHASE 2 TESTS PASSED SUCCESSFULLY!     "
echo "=========================================================="
