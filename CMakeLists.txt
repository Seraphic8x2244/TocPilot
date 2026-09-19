cmake_minimum_required(VERSION 3.24)

project(TocPilot VERSION 0.1.1 LANGUAGES CXX)

add_executable(TocPilot WIN32
    src/main.cpp
    src/update.cpp
)

target_compile_features(TocPilot PRIVATE cxx_std_20)
target_compile_definitions(TocPilot PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    set_property(TARGET TocPilot PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_options(TocPilot PRIVATE /W4 /permissive- /utf-8)
endif()

target_include_directories(TocPilot PRIVATE src)
target_link_libraries(TocPilot PRIVATE winhttp bcrypt shell32 comctl32)

set_target_properties(TocPilot PROPERTIES
    OUTPUT_NAME "TocPilot"
)
