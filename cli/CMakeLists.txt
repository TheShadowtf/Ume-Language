cmake_minimum_required(VERSION 3.20)

add_executable(ume src/main.cpp)
target_link_libraries(ume PRIVATE ume_compiler_lib ume_interpreter_lib)
target_compile_features(ume PRIVATE cxx_std_20)

install(TARGETS ume DESTINATION bin)
