cmake_minimum_required(VERSION 3.10)
project(MyLLVMCompiler)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ✅ Генерация compile_commands.json для CLion / clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 🔍 Подключаем LLVM через CMake config
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# ⚙️ Получаем флаги компиляции из llvm-config
execute_process(
        COMMAND llvm-config --cxxflags
        OUTPUT_VARIABLE LLVM_CXX_FLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 📦 Получаем библиотеки LLVM (опционально, если вручную линкуешь)
execute_process(
        COMMAND llvm-config --libs
        OUTPUT_VARIABLE LLVM_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 📌 Общие include'ы и флаги
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_compile_options(${LLVM_CXX_FLAGS})

# 🔧 lab2
add_executable(lab2 lab2.cpp)
target_include_directories(lab2 PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_options(lab2 PRIVATE ${LLVM_CXX_FLAGS})
target_link_libraries(lab2 LLVM)

# 🔧 lab3
add_executable(lab3 lab3.cpp)
target_include_directories(lab3 PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_options(lab3 PRIVATE ${LLVM_CXX_FLAGS})
target_link_libraries(lab3 LLVM)
