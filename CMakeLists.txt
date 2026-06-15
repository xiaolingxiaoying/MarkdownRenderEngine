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

# Build or locate the host-side embed tool. Cross builds cannot normally run a
# target executable on the build host.
set(MWRENDER_EMBED_TOOL "" CACHE FILEPATH
    "Host executable used to embed resources when cross-compiling")
if(CMAKE_CROSSCOMPILING)
    if(NOT MWRENDER_EMBED_TOOL)
        message(FATAL_ERROR
            "Cross-compiling requires MWRENDER_EMBED_TOOL to point to a host executable")
    endif()
    set(MWRENDER_EMBED_COMMAND "${MWRENDER_EMBED_TOOL}")
    set(MWRENDER_EMBED_DEPENDS "${MWRENDER_EMBED_TOOL}")
else()
    add_executable(mwrender_embed tools/embed.cpp)
    set(MWRENDER_EMBED_COMMAND "$<TARGET_FILE:mwrender_embed>")
    set(MWRENDER_EMBED_DEPENDS mwrender_embed)
endif()

# Helper function to embed a file
function(embed_file input_file variable_name output_file)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/${output_file}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender"
        COMMAND "${MWRENDER_EMBED_COMMAND}" "${input_file}" "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/${output_file}" "${variable_name}"
        DEPENDS "${input_file}" ${MWRENDER_EMBED_DEPENDS}
        COMMENT "Embedding ${input_file} into ${output_file}"
        VERBATIM
    )
endfunction()

embed_file("${CMAKE_CURRENT_SOURCE_DIR}/themes/github-light/content.css" githubLightCss embed_github_light_css.hpp)
embed_file("${CMAKE_CURRENT_SOURCE_DIR}/third_party/source/github-markdown-css-main/github-markdown-dark.css" githubDarkCss embed_github_dark_css.hpp)
embed_file("${CMAKE_CURRENT_SOURCE_DIR}/third_party/source/github-markdown-css-main/github-markdown-dark-dimmed.css" githubDarkDimmedCss embed_github_dark_dimmed_css.hpp)
embed_file("${CMAKE_CURRENT_SOURCE_DIR}/third_party/source/github-markdown-css-main/github-markdown-dark-high-contrast.css" githubDarkHcCss embed_github_dark_hc_css.hpp)
embed_file("${CMAKE_CURRENT_SOURCE_DIR}/third_party/source/github-markdown-css-main/github-markdown-dark-colorblind.css" githubDarkCbCss embed_github_dark_cb_css.hpp)
embed_file("${CMAKE_CURRENT_SOURCE_DIR}/third_party/source/github-markdown-css-main/github-markdown-light-colorblind.css" githubLightCbCss embed_github_light_cb_css.hpp)
embed_file("${CMAKE_CURRENT_SOURCE_DIR}/third_party/source/github-markdown-css-main/github-markdown.css" githubAutoCss embed_github_auto_css.hpp)

embed_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/js/highlight.min.js" highlightJs embed_highlight_js.hpp)
embed_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/css/github.min.css" highlightCss embed_highlight_css.hpp)
embed_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/js/mathjax-tex-svg.js" mathjaxJs embed_mathjax_js.hpp)
embed_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/js/mermaid.min.js" mermaidJs embed_mermaid_js.hpp)

set(MWRENDER_EMBEDDED_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_github_light_css.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_github_dark_css.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_github_dark_dimmed_css.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_github_dark_hc_css.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_github_dark_cb_css.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_github_light_cb_css.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_github_auto_css.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_highlight_js.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_highlight_css.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_mathjax_js.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mwrender/embed_mermaid_js.hpp"
)

add_library(mwrender_core
    ${MWRENDER_EMBEDDED_HEADERS}
    src/analysis/document_analysis.cpp
    src/engine.cpp
    src/parser/parser.cpp
    src/render/html_renderer.cpp
    src/sanitizer.cpp
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

    add_executable(mwrender_snapshot_tests tests/snapshot_test.cpp)
    target_compile_features(mwrender_snapshot_tests PRIVATE cxx_std_20)
    target_compile_definitions(
        mwrender_snapshot_tests
        PRIVATE MWRENDER_SNAPSHOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/snapshots"
    )
    target_link_libraries(mwrender_snapshot_tests PRIVATE MWRender::Core)
    add_test(NAME mwrender.snapshot COMMAND mwrender_snapshot_tests)

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

        add_test(
            NAME mwrender.cli.stdin
            COMMAND
                "${CMAKE_COMMAND}"
                "-DMWRENDER_CLI=$<TARGET_FILE:mwrender>"
                "-DMWRENDER_INPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/snapshots/blocks.md"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_stdin_test.cmake"
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
