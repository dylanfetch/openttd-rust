# Shared game/tool Rust algorithms support native Linux only. Unsupported platforms
# retain the explicit OPTION_RUST=OFF C++ path; enabling Rust must never fall back.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "OPTION_RUST requires native Linux; use OFF for the portable C++ build")
endif()

find_program(CARGO_EXECUTABLE cargo REQUIRED)
set(CMAKE_THREAD_PREFER_PTHREAD YES)
find_package(Threads REQUIRED)
set(RUST_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo")
set(RUST_ARCHIVE "${RUST_TARGET_DIR}/release/libopenttd_kernels.a")
file(GLOB_RECURSE RUST_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/rust/*.rs"
    "${CMAKE_SOURCE_DIR}/rust/*/Cargo.toml"
)

add_custom_command(
    OUTPUT "${RUST_ARCHIVE}"
    COMMAND ${CMAKE_COMMAND} -E env "CARGO_TARGET_DIR=${RUST_TARGET_DIR}"
        "${CARGO_EXECUTABLE}" build --release --locked
        --manifest-path "${CMAKE_SOURCE_DIR}/Cargo.toml"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS ${RUST_INPUTS}
        "${CMAKE_SOURCE_DIR}/Cargo.toml"
        "${CMAKE_SOURCE_DIR}/Cargo.lock"
        "${CMAKE_SOURCE_DIR}/rust-toolchain.toml"
    COMMENT "Building shared Rust algorithms"
    VERBATIM
)
add_custom_target(openttd_rust_build DEPENDS "${RUST_ARCHIVE}")
add_library(openttd_rust STATIC IMPORTED)
set_target_properties(openttd_rust PROPERTIES IMPORTED_LOCATION "${RUST_ARCHIVE}")
add_dependencies(openttd_rust openttd_rust_build)
target_link_libraries(openttd_rust INTERFACE Threads::Threads ${CMAKE_DL_LIBS} m)
target_compile_definitions(openttd_rust INTERFACE WITH_RUST)
