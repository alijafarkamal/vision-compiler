# EdgeFlow — Next Steps

Based on the current project log and directory tree, the **core compiler pipeline is already working**:

`EdgeFlow source -> Flex -> Bison -> AST -> Semantic Analysis -> LLVM IR -> Clang -> Native Executable`

## What is already done

- [x] CMake/Ninja build setup
- [x] Flex lexer
- [x] Bison parser
- [x] AST construction and `--ast` mode
- [x] LLVM IR generation
- [x] Variables and arithmetic
- [x] `if/else` control flow
- [x] Loops
- [x] Functions, parameters, return values, recursion
- [x] Arrays and indexed access/update
- [x] Insertion Sort written in EdgeFlow and compiled to native code
- [x] Basic semantic-analysis pass
- [x] OpenCV-based vision runtime
- [x] LLVM calls into the C++ vision runtime
- [x] Image-processing example
- [x] Camera API compiled
- [x] AI/model API skeleton using OpenCV DNN
- [x] Basic AST-level optimization pass
- [x] Development log

---

# Must do before calling the project fully finished

## 1. Add automated regression tests — HIGH PRIORITY

The `tests/` directory currently appears empty.

Create `tests/run_tests.sh` that automatically checks:

- arithmetic
- variables
- if/else
- loops
- functions
- recursion
- arrays
- insertion sort
- expected syntax error
- expected semantic error
- vision image output
- generated LLVM validity using `llvm-as`

Success should end with something like:

```text
[PASS] basic
[PASS] control_flow
[PASS] functions
[PASS] arrays
[PASS] insertion_sort
[PASS] semantic_error
[PASS] vision
7/7 tests passed
```

## 2. Verify generated LLVM IR explicitly

For every important demo:

```bash
llvm-as program.ll -o /tmp/program.bc
```

This proves that EdgeFlow is producing valid LLVM IR rather than relying only on Clang accepting it.

## 3. Test the camera on a real machine

The camera program was compiled, but the log does not show a successful real webcam execution.

Run:

```bash
./cam
```

Verify:

- webcam opens
- frames are captured
- window/display works
- program exits cleanly

Keep camera support optional for headless testing.

## 4. Replace `dummy.onnx` with a real model before claiming AI detection

The current AI test used:

```bash
touch dummy.onnx
./ai
```

That only tests the fallback/error path. It does **not** prove real object detection works.

Before presenting AI detection as completed:

- obtain a small valid ONNX object-detection model
- load it successfully
- run inference on one image
- draw at least one real detection
- save the resulting image

If this is not completed, describe AI detection as an **extension/API prototype**, not a finished feature.

## 5. Clean duplicate source files

The tree contains apparent duplicate/old files:

```text
include/AST.h
include/ast.hpp

include/CodeGen.h
include/codegen.hpp

lexer/edgeflow.l
lexer/lexer.l

parser/edgeflow.y
parser/parser.y

src/AST.cpp
src/ast.cpp

src/CodeGen.cpp
src/codegen.cpp
```

Determine which set CMake actually uses. Remove or archive the unused copies so the repository has one clear implementation.

Do this carefully only after confirming the active files in `CMakeLists.txt`.

## 6. Clean generated binaries from the repository root

These are build/demo artifacts rather than source:

```text
ai
ai.ll
arr
arr.ll
basic
basic.ll
cam
cam.ll
cf
cf.ll
func
func.ll
opt
opt.ll
sort
sort.ll
vis
vis.ll
image.jpg
result.jpg
dummy.onnx
```

Prefer putting generated output under:

```text
build/
build/generated/
outputs/
```

Add a `.gitignore` for build artifacts.

## 7. Make one simple compile-and-run command

Create a helper script such as:

```bash
./edgeflow-run examples/insertion_sort.edge
```

Internally it can do:

```bash
./build/edgeflow "$1" -o build/program.ll
llvm-as build/program.ll -o build/program.bc
clang++ build/program.ll -o build/program \
  -Lbuild -ledgeruntime $(pkg-config --libs opencv4)
./build/program
```

This will make the final demonstration much cleaner.

## 8. Update README for final presentation

README should clearly contain:

1. What EdgeFlow is
2. Language syntax
3. Compiler architecture
4. Installation dependencies
5. Build commands
6. How to compile/run `.edge` files
7. Insertion Sort example
8. Vision example
9. Optimization examples
10. Current limitations
11. Optional/future features

---

# Nice-to-have extensions

Only do these after the items above are stable.

- [ ] Real YOLO/ONNX inference
- [ ] webcam + detection
- [ ] more semantic/type errors
- [ ] constant folding
- [ ] dead-code elimination beyond constant `if`
- [ ] redundant vision-stage elimination
- [ ] Merge Sort example
- [ ] `--run` option directly inside the EdgeFlow compiler
- [ ] better compiler error messages
- [ ] LLVM optimization pipeline (`mem2reg`, `instcombine`, etc.)
- [ ] simple pipeline/AST visualization
- [ ] JIT compilation
- [ ] AI copilot that generates EdgeFlow source

---

# Recommended final-demo sequence

Do not begin the presentation with YOLO.

Show the compiler foundations first:

1. **EdgeFlow source code**
2. `--ast` output
3. generated `.ll`
4. native execution
5. Insertion Sort
6. semantic-error example
7. image-processing program
8. optimizer example
9. webcam / AI detection only if genuinely working

This demonstrates that the project is a real compiler first and a vision DSL second.

---

# Build the project

From the project root:

```bash
cd ~/compiler_project

cmake -S . -B build -G Ninja
cmake --build build -j$(nproc)
```

The compiler should then be:

```bash
./build/edgeflow
```

---

# Run the existing examples

## Basic language

```bash
./build/edgeflow examples/basic.edge -o basic.ll
llvm-as basic.ll -o /tmp/basic.bc
clang++ basic.ll -o basic -Lbuild -ledgeruntime $(pkg-config --libs opencv4)
./basic
```

## Control flow

```bash
./build/edgeflow examples/control_flow.edge -o cf.ll
llvm-as cf.ll -o /tmp/cf.bc
clang++ cf.ll -o cf -Lbuild -ledgeruntime $(pkg-config --libs opencv4)
./cf
```

## Functions

```bash
./build/edgeflow examples/functions.edge -o func.ll
llvm-as func.ll -o /tmp/func.bc
clang++ func.ll -o func -Lbuild -ledgeruntime $(pkg-config --libs opencv4)
./func
```

## Arrays

```bash
./build/edgeflow examples/arrays.edge -o arr.ll
llvm-as arr.ll -o /tmp/arr.bc
clang++ arr.ll -o arr -Lbuild -ledgeruntime $(pkg-config --libs opencv4)
./arr
```

## Insertion Sort

```bash
./build/edgeflow examples/insertion_sort.edge -o sort.ll
llvm-as sort.ll -o /tmp/sort.bc
clang++ sort.ll -o sort -Lbuild -ledgeruntime $(pkg-config --libs opencv4)
./sort
```

## AST view

```bash
./build/edgeflow --ast examples/insertion_sort.edge
```

## Semantic-error test

```bash
./build/edgeflow examples/semantic_test.edge
```

This test may intentionally fail; confirm that it prints the intended semantic diagnostic.

## Vision

```bash
./build/edgeflow examples/vision.edge -o vis.ll
llvm-as vis.ll -o /tmp/vis.bc
clang++ vis.ll -o vis -Lbuild -ledgeruntime $(pkg-config --libs opencv4)
./vis
```

Then inspect the expected output image, for example:

```bash
xdg-open result.jpg
```

## Camera

```bash
./build/edgeflow examples/camera.edge -o cam.ll
llvm-as cam.ll -o /tmp/cam.bc
clang++ cam.ll -o cam -Lbuild -ledgeruntime $(pkg-config --libs opencv4)
./cam
```

## Optimizer demo

```bash
./build/edgeflow examples/optimize.edge -o opt.ll
llvm-as opt.ll -o /tmp/opt.bc
clang++ opt.ll -o opt -Lbuild -ledgeruntime $(pkg-config --libs opencv4)
./opt
```

---

# The most important remaining checkpoint

A strong definition of “finished” is:

```text
Fresh clone
    |
    v
cmake configure
    |
    v
build succeeds
    |
    v
automated tests all pass
    |
    +--> insertion sort produces correct output
    |
    +--> invalid program produces correct compiler error
    |
    +--> vision program creates processed image
    |
    `--> generated LLVM passes llvm-as
```

If all of that works reproducibly, EdgeFlow is already a strong Compiler Construction submission. Real-time detection, JIT, GUI, and AI assistance can remain bonus features.
