cmake_minimum_required(VERSION 3.20)
project(EdgeFlow CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

# ============================================================
# Phase 1: Interpreter (Flex + Bison only, NO LLVM)
# ============================================================
FLEX_TARGET(InterpScanner
    interpreter/lexer_interp.l
    ${CMAKE_BINARY_DIR}/lex_interp.yy.cpp
)
BISON_TARGET(InterpParser
    interpreter/interpreter.y
    ${CMAKE_BINARY_DIR}/interp.tab.cpp
    COMPILE_FLAGS "-d"
    DEFINES_FILE ${CMAKE_BINARY_DIR}/interp.tab.hpp
)
ADD_FLEX_BISON_DEPENDENCY(InterpScanner InterpParser)

add_executable(edgeflow-interp
    interpreter/main_interp.cpp
    ${BISON_InterpParser_OUTPUTS}
    ${FLEX_InterpScanner_OUTPUTS}
)
# No LLVM, no OpenCV — pure interpreter
target_include_directories(edgeflow-interp PRIVATE ${CMAKE_BINARY_DIR})

# ============================================================
# ============================================================
# Phase 2: AOT Compiler (Flex + Bison + LLVM + OpenCV)
# ============================================================
find_package(LLVM CONFIG QUIET)
find_package(OpenCV QUIET)

if(LLVM_FOUND AND OpenCV_FOUND)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
    include_directories(${LLVM_INCLUDE_DIRS})
    add_definitions(${LLVM_DEFINITIONS})
    include_directories(${OpenCV_INCLUDE_DIRS})

    # Include directories
    include_directories(${CMAKE_SOURCE_DIR}/include)
    include_directories(${CMAKE_BINARY_DIR}) # for generated flex/bison headers

    # Flex/Bison for Phase 2
    FLEX_TARGET(EdgeScanner lexer/lexer.l  ${CMAKE_BINARY_DIR}/lex.yy.cpp)
    BISON_TARGET(EdgeParser parser/parser.y ${CMAKE_BINARY_DIR}/parser.tab.cpp COMPILE_FLAGS "-d")
    ADD_FLEX_BISON_DEPENDENCY(EdgeScanner EdgeParser)

    # Phase 2 Executable
    add_executable(edgeflow
        src/main.cpp
        # Use lowercase paths to avoid case-only source collisions on Windows.
        src/ast.cpp
        src/codegen.cpp
        src/Semantic.cpp
        ${BISON_EdgeParser_OUTPUTS}
        ${FLEX_EdgeScanner_OUTPUTS}
    )

    llvm_map_components_to_libnames(llvm_libs core irreader support bitwriter target bitreader native)
    target_link_libraries(edgeflow PRIVATE
        ${OpenCV_LIBS}
        ${llvm_libs}
    )

    add_library(edgeruntime STATIC runtime/vision_runtime.cpp)
    target_link_libraries(edgeruntime PRIVATE ${OpenCV_LIBS})
else()
    message(STATUS "LLVM or OpenCV not found — skipping Phase 2 AOT compiler (Phase 1 interpreter is ready to build).")
endif()
