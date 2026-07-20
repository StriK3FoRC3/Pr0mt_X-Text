cmake_minimum_required(VERSION 3.16)
project(Pr0mtXText VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Pr0mt_X Text is a native Windows application.")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/appicon.ico")
    message(FATAL_ERROR "Required application icon not found: appicon.ico")
endif()

add_executable(Pr0mtXText WIN32 src/main.cpp app.rc)
target_compile_features(Pr0mtXText PRIVATE cxx_std_17)
target_compile_definitions(Pr0mtXText PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(Pr0mtXText PRIVATE comctl32 shell32 user32 gdi32 dwmapi uxtheme)

if(MSVC)
    target_compile_options(Pr0mtXText PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(Pr0mtXText PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(Pr0mtXText PROPERTIES
    OUTPUT_NAME "Pr0mt_X Text"
)
