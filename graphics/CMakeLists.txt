cmake_minimum_required(VERSION 3.20)

add_library(ume_graphics_lib STATIC
    src/graphics_bindings.cpp
)

target_include_directories(ume_graphics_lib PUBLIC include PRIVATE ${CMAKE_SOURCE_DIR}/vendor/stb_image)
target_compile_features(ume_graphics_lib PUBLIC cxx_std_20)

# Link against GLFW and GLAD (these are built as DLLs separately)
target_link_libraries(ume_graphics_lib PUBLIC glfw glad)
