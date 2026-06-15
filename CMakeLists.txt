cmake_minimum_required(VERSION 3.20)

project(
    MWRender
    VERSION 0.1.0
    DESCRIPTION "A lightweight Markdown to HTML rendering engine"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CTest)
include(CMakePackageConfigHelpers)

option(MWRENDER_BUILD_CLI "Build the mwrender command-line application" ON)
option(MWRENDER_BUILD_TESTS "Build MWRender tests" ON)
option(MWRENDER_ENABLE_YAML "Enable YAML front matter support" OFF)
option(MWRENDER_ENABLE_SANITIZER "Enable an HTML sanitizer backend" OFF)

file(READ
    "${CMAKE_CURRENT_SOURCE_DIR}/themes/github-light/content.css"
    MWRENDER_GITHUB_LIGHT_CSS
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/builtin_theme.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/builtin_theme.hpp"
    @ONLY
)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/js/highlight.min.js" MWRENDER_HIGHLIGHT_JS)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/css/github.min.css" MWRENDER_GITHUB_CSS)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/js/mathjax-tex-svg.js" MWRENDER_MATHJAX_JS)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/js/mermaid.min.js" MWRENDER_MERMAID_JS)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/builtin_resources.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/builtin_resources.hpp"
    @ONLY
)

add_library(mwrender_core
    src/analysis/document_analysis.cpp
    src/engine.cpp
    src/parser/parser.cpp
    src/render/html_renderer.cpp
    src/theme/theme_manager.cpp
    src/support/document_text.cpp
    src/support/json.cpp
)
add_library(MWRender::Core ALIAS mwrender_core)
set_target_properties(mwrender_core PROPERTIES EXPORT_NAME Core)

target_compile_features(mwrender_core PUBLIC cxx_std_20)
target_include_directories(mwrender_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

if(MSVC)
    target_compile_options(mwrender_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(mwrender_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_compile_definitions(mwrender_core
    PUBLIC
        MWRENDER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        MWRENDER_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        MWRENDER_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(MWRENDER_BUILD_CLI)
    add_executable(mwrender apps/mwrender/main.cpp)
    target_compile_features(mwrender PRIVATE cxx_std_20)
    target_include_directories(mwrender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(mwrender PRIVATE MWRender::Core)
endif()

if(BUILD_TESTING AND MWRENDER_BUILD_TESTS)
    add_executable(mwrender_smoke_tests tests/smoke_test.cpp)
    target_compile_features(mwrender_smoke_tests PRIVATE cxx_std_20)
    target_link_libraries(mwrender_smoke_tests PRIVATE MWRender::Core)
    add_test(NAME mwrender.smoke COMMAND mwrender_smoke_tests)

    add_executable(mwrender_edge_tests tests/edge_test.cpp)
    target_compile_features(mwrender_edge_tests PRIVATE cxx_std_20)
    target_link_libraries(mwrender_edge_tests PRIVATE MWRender::Core)
    add_test(NAME mwrender.edge COMMAND mwrender_edge_tests)

    # Phase 8: CommonMark/GFM conformance tests
    add_executable(mwrender_conformance_tests tests/conformance_test.cpp)
    target_compile_features(mwrender_conformance_tests PRIVATE cxx_std_20)
    target_link_libraries(mwrender_conformance_tests PRIVATE MWRender::Core)
    add_test(NAME mwrender.conformance COMMAND mwrender_conformance_tests)

    # Phase 8: Stress / robustness tests
    add_executable(mwrender_stress_tests tests/stress_test.cpp)
    target_compile_features(mwrender_stress_tests PRIVATE cxx_std_20)
    target_link_libraries(mwrender_stress_tests PRIVATE MWRender::Core)
    add_test(NAME mwrender.stress COMMAND mwrender_stress_tests)
    set_tests_properties(mwrender.stress PROPERTIES TIMEOUT 60)

    # Phase 8: Security regression tests
    add_executable(mwrender_security_tests tests/security_test.cpp)
    target_compile_features(mwrender_security_tests PRIVATE cxx_std_20)
    target_link_libraries(mwrender_security_tests PRIVATE MWRender::Core)
    add_test(NAME mwrender.security COMMAND mwrender_security_tests)

    # Phase 8: Performance benchmarks (longer timeout; not a correctness gate)
    add_executable(mwrender_benchmark tests/benchmark.cpp)
    target_compile_features(mwrender_benchmark PRIVATE cxx_std_20)
    target_link_libraries(mwrender_benchmark PRIVATE MWRender::Core)
    add_test(NAME mwrender.benchmark COMMAND mwrender_benchmark)
    set_tests_properties(mwrender.benchmark PROPERTIES TIMEOUT 120)

    if(MWRENDER_BUILD_CLI)
        add_test(NAME mwrender.cli.version COMMAND mwrender --version)
        set_tests_properties(
            mwrender.cli.version
            PROPERTIES PASS_REGULAR_EXPRESSION "mwrender 0.1.0"
        )

        add_test(NAME mwrender.cli.themes COMMAND mwrender --list-themes)
        set_tests_properties(
            mwrender.cli.themes
            PROPERTIES PASS_REGULAR_EXPRESSION "github-light"
        )

        add_test(
            NAME mwrender.cli.config
            COMMAND
                mwrender
                "${CMAKE_CURRENT_SOURCE_DIR}/examples/basic.md"
                --config
                "${CMAKE_CURRENT_SOURCE_DIR}/examples/mwrender.json"
                --fragment
        )
        set_tests_properties(
            mwrender.cli.config
            PROPERTIES PASS_REGULAR_EXPRESSION "mw-task-list"
        )
    endif()
endif()

install(
    TARGETS mwrender_core
    EXPORT MWRenderTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(MWRENDER_BUILD_CLI)
    install(TARGETS mwrender RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(
    DIRECTORY include/mwrender
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY themes/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mwrender/themes
)

install(
    EXPORT MWRenderTargets
    FILE MWRenderTargets.cmake
    NAMESPACE MWRender::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MWRender
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MWRenderConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MWRenderConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MWRender
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MWRenderConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/MWRenderConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/MWRenderConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MWRender
)
