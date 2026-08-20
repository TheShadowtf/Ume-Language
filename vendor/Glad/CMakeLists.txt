cmake_minimum_required(VERSION 3.20)

project(glad LANGUAGES C)

# Build GLAD as a shared library (DLL on Windows)
add_library(glad SHARED
    src/glad.c
)

target_include_directories(glad PUBLIC include)

# Windows-specific settings
if(MSVC)
    # When building the DLL, export all symbols
    target_compile_definitions(glad PRIVATE GLAD_GLAPI_EXPORT GLAD_GLAPI_EXPORT_BUILD)
    # When consuming the DLL, import symbols
    target_compile_definitions(glad PUBLIC GLAD_GLAPI_EXPORT)
    target_link_libraries(glad PRIVATE opengl32)
    
    # Set the correct C runtime for Visual Studio
    set_target_properties(glad PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

# Set output directory for DLL
set_target_properties(glad PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

install(TARGETS glad RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
