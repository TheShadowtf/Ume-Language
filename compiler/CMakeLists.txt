cmake_minimum_required(VERSION 3.20)

add_library(ume_compiler_lib STATIC
    src/lexer.cpp
    src/ast.cpp
    src/parser.cpp
    src/semantic.cpp
    src/codegen.cpp
    src/llvm_codegen.cpp
    src/package_resolver.cpp
    src/compiler.cpp
    src/diagnostics.cpp
)

target_include_directories(ume_compiler_lib PUBLIC include)
target_compile_features(ume_compiler_lib PUBLIC cxx_std_20)
