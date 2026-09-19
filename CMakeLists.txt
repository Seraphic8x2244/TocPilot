cmake_minimum_required(VERSION 3.24)

project(TocPilot VERSION 0.1.4 LANGUAGES CXX)

add_executable(TocPilot WIN32
    src/add_package_dialog.cpp
    src/main.cpp
    src/provider.cpp
    src/state.cpp
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

include(CTest)

if(BUILD_TESTING)
    add_executable(TocPilotProviderTests
        tests/provider_tests.cpp
        src/provider.cpp
    )
    target_compile_features(TocPilotProviderTests PRIVATE cxx_std_20)
    target_include_directories(TocPilotProviderTests PRIVATE src)

    if(MSVC)
        target_compile_options(TocPilotProviderTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME provider-url-normalization COMMAND TocPilotProviderTests)

    add_executable(TocPilotStateTests
        tests/state_tests.cpp
        src/state.cpp
    )
    target_compile_features(TocPilotStateTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotStateTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotStateTests PRIVATE src)

    if(MSVC)
        target_compile_options(TocPilotStateTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME state-package-roundtrip COMMAND TocPilotStateTests)
endif()
