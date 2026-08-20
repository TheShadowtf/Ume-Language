cmake_minimum_required(VERSION 3.20)

add_library(ume_interpreter_lib STATIC
    src/environment.cpp
    src/evaluator.cpp
    src/interpreter.cpp
    src/graphics_interpreter_bindings.cpp
)

target_include_directories(ume_interpreter_lib PUBLIC include)
target_compile_features(ume_interpreter_lib PUBLIC cxx_std_20)
target_link_libraries(ume_interpreter_lib PUBLIC ume_compiler_lib ume_graphics_lib)
