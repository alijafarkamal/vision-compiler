cmake_minimum_required(VERSION 3.20)
project(EdgeFlow CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR}) # for generated flex/bison headers

# Flex/Bison
FLEX_TARGET(EdgeScanner lexer/lexer.l  ${CMAKE_BINARY_DIR}/lex.yy.cpp)
BISON_TARGET(EdgeParser parser/parser.y ${CMAKE_BINARY_DIR}/parser.tab.cpp COMPILE_FLAGS "-d")
ADD_FLEX_BISON_DEPENDENCY(EdgeScanner EdgeParser)

# Executable
add_executable(edgeflow
    src/main.cpp
    src/AST.cpp
    src/CodeGen.cpp
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
