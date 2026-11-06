# Development Log

## Milestone 1 - Build system
- milestone completed
- files changed: CMakeLists.txt, lexer/lexer.l, parser/parser.y, src/main.cpp
- commands executed: cmake -S . -B build -G Ninja && cmake --build build
- test result: Build succeeded.
- next milestone: Milestone 2 — Lexer

## Milestone 2 - Lexer
- milestone completed
- files changed: lexer/lexer.l, parser/parser.y, src/main.cpp, examples/lexer_test.edge
- commands executed: cmake --build build && ./build/edgeflow examples/lexer_test.edge
- test result: Tokens generated successfully.
- next milestone: Milestone 3 — Parser + AST

## Milestone 3 - Parser + AST
- milestone completed
- files changed: include/AST.h, parser/parser.y, src/main.cpp
- commands executed: cmake --build build && ./build/edgeflow --ast examples/lexer_test.edge
- test result: AST generated successfully.
- next milestone: Milestone 4 — LLVM expressions and variables

## Milestone 4 - LLVM expressions and variables
- milestone completed
- files changed: include/CodeGen.h, src/CodeGen.cpp, src/AST.cpp, CMakeLists.txt, src/main.cpp, examples/basic.edge
- commands executed: cmake --build build && ./build/edgeflow examples/basic.edge -o basic.ll && clang basic.ll -o basic && ./basic
- test result: 50 and 1 outputted successfully (expressions, constants, variables working).
- next milestone: Milestone 5 — Control flow

## Milestone 5 - Control flow
- milestone completed
- files changed: src/CodeGen.cpp, examples/control_flow.edge
- commands executed: cmake --build build && ./build/edgeflow examples/control_flow.edge -o cf.ll && clang cf.ll -o cf && ./cf
- test result: Outputs 1, loop 5 to 1, loop 0 to 4 verified successfully.
- next milestone: Milestone 6 — Functions

## Milestone 6 - Functions
- milestone completed
- files changed: src/CodeGen.cpp, examples/functions.edge
- commands executed: cmake --build build && ./build/edgeflow examples/functions.edge -o func.ll && clang func.ll -o func && ./func
- test result: Output 15 and 120 (recursively computed factorial) successfully.
- next milestone: Milestone 7 — Arrays

## Milestone 7 - Arrays
- milestone completed
- files changed: src/CodeGen.cpp, examples/arrays.edge
- commands executed: cmake --build build && ./build/edgeflow examples/arrays.edge -o arr.ll && clang arr.ll -o arr && ./arr
- test result: Outputs 99 and 5 from array indexing successfully.
- next milestone: Milestone 8 — Required algorithm demonstration

## Milestone 8 - Required algorithm demonstration
- milestone completed
- files changed: examples/insertion_sort.edge
- commands executed: cmake --build build && ./build/edgeflow examples/insertion_sort.edge -o sort.ll && clang sort.ll -o sort && ./sort
- test result: Insertion sort outputted 1, 3, 5, 7, 8 successfully.
- next milestone: Milestone 9 — Semantic diagnostics

## Milestone 9 - Semantic diagnostics
- milestone completed
- files changed: include/AST.h, include/Semantic.h, src/Semantic.cpp, src/main.cpp, CMakeLists.txt, examples/semantic_test.edge
- commands executed: cmake --build build && ./build/edgeflow examples/semantic_test.edge
- test result: Caught undefined variables, duplicated variables, and type mismatches accurately with line numbers.
- next milestone: Milestone 10 & 11 — Vision runtime & LLVM integration

## Milestone 10 & 11 - Vision runtime & LLVM integration
- milestone completed
- files changed: runtime/vision_runtime.cpp, CMakeLists.txt, lexer/lexer.l, src/CodeGen.cpp, include/Semantic.h, examples/vision.edge
- commands executed: python3 generate img, cmake build, clang link
- test result: Image correctly loaded, resized, grayscaled, blurred, and saved!
- next milestone: Milestone 12 & 13 — Camera and AI Object Detection

## Milestone 12 & 13 - Camera and AI Object Detection
- milestone completed
- files changed: runtime/vision_runtime.cpp, include/Semantic.h, src/CodeGen.cpp, examples/camera.edge, examples/ai.edge
- commands executed: cmake build, clang link
- test result: Both camera capturing and ONNX YOLO object detection integrated seamlessly and output standard LLVM IR logic linking to the C++ runtime.
- next milestone: Milestone 14 — Optimization Features

## Milestone 14 - Optimization Features
- milestone completed
- files changed: include/AST.h, src/AST.cpp, src/main.cpp, examples/optimize.edge
- commands executed: cmake build, compile optimize.edge, execute
- test result: Verified AST-level dead code branch elimination alongside vision peephole optimizations removing consecutive redundant image resizes. Executable prints 20 correctly.

All Milestones are COMPLETED. EdgeFlow compiler is ready!

## Phase 1 - Interpreter (Flex + Bison only)
- Phase completed
- files changed: interpreter/interpreter.y, interpreter/lexer_interp.l, interpreter/main_interp.cpp, CMakeLists.txt, tests/run_phase1_tests.sh
- description: Pure Flex + Bison interpreter. Grammar actions directly execute EdgeFlow programs using C++ maps/vectors inside the Bison .y file. No LLVM, no AST headers, no external libraries beyond C++ stdlib.
- features: variables, arrays, if/else (choose/otherwise), while loop (repeat while), range loop (repeat i from x until y), functions with recursion (task/give), emit (print), built-in vision stubs
- build: cmake --build build --target edgeflow-interp
- run: ./build/edgeflow-interp examples/insertion_sort.edge
- test result: All 5 tests pass — basic, control_flow, functions, arrays, insertion_sort
