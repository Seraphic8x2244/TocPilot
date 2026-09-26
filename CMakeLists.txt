cmake_minimum_required(VERSION 3.24)

project(TocPilot VERSION 0.3.10 LANGUAGES C CXX)

# miniz 3.1.2, pinned from richgel999/miniz commit
# 77d0dce8627735138c51770d1799a1ef48f2117d.
add_library(TocPilotMiniz STATIC
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
)
target_include_directories(TocPilotMiniz PUBLIC third_party/miniz)
target_compile_definitions(TocPilotMiniz PRIVATE
    MINIZ_NO_ZLIB_COMPATIBLE_NAMES
)

if(MSVC)
    set_property(TARGET TocPilotMiniz PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_options(TocPilotMiniz PRIVATE /W3 /utf-8)
endif()

add_executable(TocPilot WIN32
    src/addon_scan.cpp
    src/adoption.cpp
    src/add_package_dialog.cpp
    src/archive.cpp
    src/branch_dialog.cpp
    src/direct_dll.cpp
    src/github_api.cpp
    src/github_release.cpp
    src/git_refs.cpp
    src/gitlab_api.cpp
    src/install.cpp
    src/library_dialog.cpp
    src/main.cpp
    src/provider.cpp
    src/refresh_freshness.cpp
    src/removal_prompt.cpp
    src/state.cpp
    src/splash.cpp
    src/update.cpp
    src/update_all.cpp
    src/ui_dialog.cpp
    resources/TocPilot.rc
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

target_include_directories(TocPilot PRIVATE src resources)
target_link_libraries(TocPilot PRIVATE TocPilotMiniz winhttp bcrypt shell32 comctl32 gdiplus ole32)

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

    add_executable(TocPilotGitHubApiTests
        tests/github_api_tests.cpp
        src/github_api.cpp
        src/git_refs.cpp
    )
    target_compile_features(TocPilotGitHubApiTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotGitHubApiTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotGitHubApiTests PRIVATE src)
    target_link_libraries(TocPilotGitHubApiTests PRIVATE winhttp)

    if(MSVC)
        target_compile_options(TocPilotGitHubApiTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME github-transport COMMAND TocPilotGitHubApiTests)

    add_executable(TocPilotGitLabApiTests
        tests/gitlab_api_tests.cpp
        src/gitlab_api.cpp
    )
    target_compile_features(TocPilotGitLabApiTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotGitLabApiTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotGitLabApiTests PRIVATE src)
    target_link_libraries(TocPilotGitLabApiTests PRIVATE winhttp)

    if(MSVC)
        target_compile_options(TocPilotGitLabApiTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME gitlab-archive-path COMMAND TocPilotGitLabApiTests)

    add_executable(TocPilotGitRefsTests
        tests/git_refs_tests.cpp
        src/git_refs.cpp
    )
    target_compile_features(TocPilotGitRefsTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotGitRefsTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotGitRefsTests PRIVATE src)
    target_link_libraries(TocPilotGitRefsTests PRIVATE winhttp)

    if(MSVC)
        target_compile_options(TocPilotGitRefsTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME git-smart-http-ref-parsing COMMAND TocPilotGitRefsTests)
    add_test(
        NAME git-smart-http-live-github
        COMMAND TocPilotGitRefsTests --live-github
    )

    add_executable(TocPilotArchiveTests
        tests/archive_tests.cpp
        src/archive.cpp
    )
    target_compile_features(TocPilotArchiveTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotArchiveTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotArchiveTests PRIVATE src)
    target_link_libraries(TocPilotArchiveTests PRIVATE TocPilotMiniz)

    if(MSVC)
        target_compile_options(TocPilotArchiveTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME archive-inspection COMMAND TocPilotArchiveTests)

    add_executable(TocPilotInstallTests
        tests/install_tests.cpp
        src/install.cpp
    )
    target_compile_features(TocPilotInstallTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotInstallTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotInstallTests PRIVATE src)
    target_link_libraries(TocPilotInstallTests PRIVATE ole32)

    if(MSVC)
        target_compile_options(TocPilotInstallTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME addon-install-transaction COMMAND TocPilotInstallTests)

    add_executable(TocPilotUpdateAllTests
        tests/update_all_tests.cpp
        src/update_all.cpp
    )
    target_compile_features(TocPilotUpdateAllTests PRIVATE cxx_std_20)
    target_include_directories(TocPilotUpdateAllTests PRIVATE src)

    if(MSVC)
        target_compile_options(TocPilotUpdateAllTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME update-all-orchestration COMMAND TocPilotUpdateAllTests)

    add_executable(TocPilotUpdateTests
        tests/update_tests.cpp
        src/update.cpp
        src/github_release.cpp
    )
    target_compile_features(TocPilotUpdateTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotUpdateTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotUpdateTests PRIVATE src)
    target_link_libraries(TocPilotUpdateTests PRIVATE winhttp bcrypt)

    if(MSVC)
        target_compile_options(TocPilotUpdateTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME self-update-discovery COMMAND TocPilotUpdateTests)
    add_test(
        NAME self-update-live-latest
        COMMAND TocPilotUpdateTests --live-latest
    )

    add_executable(TocPilotGitHubReleaseTests
        tests/github_release_tests.cpp
        src/github_release.cpp
    )
    target_compile_features(TocPilotGitHubReleaseTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotGitHubReleaseTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotGitHubReleaseTests PRIVATE src)
    target_link_libraries(TocPilotGitHubReleaseTests PRIVATE winhttp)

    if(MSVC)
        target_compile_options(TocPilotGitHubReleaseTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME github-release-metadata COMMAND TocPilotGitHubReleaseTests)

    add_executable(TocPilotDirectDllTests
        tests/direct_dll_tests.cpp
        src/direct_dll.cpp
        src/github_release.cpp
        src/state.cpp
    )
    target_compile_features(TocPilotDirectDllTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotDirectDllTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotDirectDllTests PRIVATE src)
    target_link_libraries(TocPilotDirectDllTests PRIVATE winhttp bcrypt)

    if(MSVC)
        target_compile_options(TocPilotDirectDllTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME direct-dll-policy COMMAND TocPilotDirectDllTests)

    add_executable(TocPilotAdoptionTests
        tests/adoption_tests.cpp
        src/adoption.cpp
        src/provider.cpp
    )
    target_compile_features(TocPilotAdoptionTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotAdoptionTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotAdoptionTests PRIVATE src)

    if(MSVC)
        target_compile_options(TocPilotAdoptionTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME git-addon-adoption COMMAND TocPilotAdoptionTests)

    add_executable(TocPilotAddonScanTests
        tests/addon_scan_tests.cpp
        src/addon_scan.cpp
    )
    target_compile_features(TocPilotAddonScanTests PRIVATE cxx_std_20)
    target_compile_definitions(TocPilotAddonScanTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TocPilotAddonScanTests PRIVATE src)

    if(MSVC)
        target_compile_options(TocPilotAddonScanTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME addon-folder-classification COMMAND TocPilotAddonScanTests)

    add_executable(TocPilotRefreshFreshnessTests
        tests/refresh_freshness_tests.cpp
        src/refresh_freshness.cpp
    )
    target_compile_features(TocPilotRefreshFreshnessTests PRIVATE cxx_std_20)
    target_include_directories(TocPilotRefreshFreshnessTests PRIVATE src)

    if(MSVC)
        target_compile_options(TocPilotRefreshFreshnessTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME package-refresh-freshness COMMAND TocPilotRefreshFreshnessTests)

    add_executable(TocPilotRemovalPromptTests
        tests/removal_prompt_tests.cpp
        src/removal_prompt.cpp
    )
    target_compile_features(TocPilotRemovalPromptTests PRIVATE cxx_std_20)
    target_include_directories(TocPilotRemovalPromptTests PRIVATE src)

    if(MSVC)
        target_compile_options(TocPilotRemovalPromptTests PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_test(NAME removal-prompt-model COMMAND TocPilotRemovalPromptTests)

endif()
