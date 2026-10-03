# Native targets only: enabling Rust must never silently select the C++ fallback.
if(CMAKE_CROSSCOMPILING OR NOT CMAKE_HOST_SYSTEM_NAME STREQUAL CMAKE_SYSTEM_NAME)
    message(FATAL_ERROR "OPTION_RUST requires a supported native target")
endif()
find_program(CARGO_EXECUTABLE cargo REQUIRED)
find_program(RUSTC_EXECUTABLE rustc REQUIRED)
execute_process(COMMAND "${RUSTC_EXECUTABLE}" -vV WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE RUST_VERSION RESULT_VARIABLE RUST_RESULT)
file(READ "${CMAKE_SOURCE_DIR}/rust-toolchain.toml" RUST_TOOLCHAIN)
string(REGEX MATCH "channel = \"([^\"]+)\"" _ "${RUST_TOOLCHAIN}")
set(RUST_PIN "${CMAKE_MATCH_1}")
string(REGEX MATCH "release: ([^\r\n]+)" _ "${RUST_VERSION}")
if(NOT RUST_RESULT EQUAL 0 OR NOT CMAKE_MATCH_1 STREQUAL RUST_PIN)
    message(FATAL_ERROR "Rust compiler must match rust-toolchain.toml (${RUST_PIN})")
endif()
string(REGEX MATCH "host: ([^\r\n]+)" _ "${RUST_VERSION}")
set(RUST_HOST "${CMAKE_MATCH_1}")
set(RUST_BUILD_ENV "CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/cargo" "RUSTC=${RUSTC_EXECUTABLE}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(RUST_HOST STREQUAL "x86_64-unknown-linux-gnu")
        set(RUST_ARCH_CHECK "defined(__x86_64__)")
    elseif(RUST_HOST STREQUAL "aarch64-unknown-linux-gnu")
        set(RUST_ARCH_CHECK "defined(__aarch64__)")
    else()
        message(FATAL_ERROR "Unsupported native Linux Rust host: ${RUST_HOST}")
    endif()
    set(RUST_PLATFORM_CHECK "defined(__linux__) && defined(__GLIBC__)")
    set(RUST_CHECK_HEADERS "#include <features.h>")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT RUST_HOST STREQUAL "aarch64-apple-darwin" OR
       NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        message(FATAL_ERROR "OPTION_RUST requires native macOS with exactly CMAKE_OSX_ARCHITECTURES=arm64; universal and Intel targets are unsupported")
    endif()
    if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "11.0")
        message(FATAL_ERROR "Rust macOS arm64 requires a deployment target of at least 11.0")
    endif()
    if(IS_ABSOLUTE "${CMAKE_OSX_SYSROOT}")
        set(RUST_SDKROOT "${CMAKE_OSX_SYSROOT}")
    else()
        if(CMAKE_OSX_SYSROOT)
            set(RUST_SDK_NAME "${CMAKE_OSX_SYSROOT}")
        else()
            set(RUST_SDK_NAME macosx)
        endif()
        execute_process(COMMAND xcrun --sdk "${RUST_SDK_NAME}" --show-sdk-path
            OUTPUT_VARIABLE RUST_SDKROOT OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE RUST_RESULT)
        if(NOT RUST_RESULT EQUAL 0)
            message(FATAL_ERROR "Cannot resolve the configured macOS SDK")
        endif()
    endif()
    if(NOT IS_DIRECTORY "${RUST_SDKROOT}")
        message(FATAL_ERROR "Configured macOS SDK does not exist: ${RUST_SDKROOT}")
    endif()
    # Both compilers consume the same SDK and deployment minimum.
    set(CMAKE_OSX_SYSROOT "${RUST_SDKROOT}" CACHE PATH "macOS SDK" FORCE)
    list(APPEND RUST_BUILD_ENV "SDKROOT=${RUST_SDKROOT}" "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    string(REPLACE "." ";" RUST_MIN_COMPONENTS "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    list(LENGTH RUST_MIN_COMPONENTS RUST_MIN_COUNT)
    list(GET RUST_MIN_COMPONENTS 0 RUST_MIN_MAJOR)
    set(RUST_MIN_MINOR 0)
    set(RUST_MIN_PATCH 0)
    if(RUST_MIN_COUNT GREATER 1)
        list(GET RUST_MIN_COMPONENTS 1 RUST_MIN_MINOR)
    endif()
    if(RUST_MIN_COUNT GREATER 2)
        list(GET RUST_MIN_COMPONENTS 2 RUST_MIN_PATCH)
    endif()
    math(EXPR RUST_MIN_MACRO "${RUST_MIN_MAJOR} * 10000 + ${RUST_MIN_MINOR} * 100 + ${RUST_MIN_PATCH}")
    set(RUST_ARCH_CHECK "defined(__aarch64__)")
    set(RUST_PLATFORM_CHECK "defined(__APPLE__) && TARGET_OS_OSX && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ == ${RUST_MIN_MACRO}")
    set(RUST_CHECK_HEADERS "#include <TargetConditionals.h>")
else()
    message(FATAL_ERROR "OPTION_RUST supports native GNU/Linux and macOS arm64 only")
endif()
include(CheckCXXSourceCompiles)
unset(RUST_CXX_TARGET_MATCHES CACHE)
check_cxx_source_compiles("${RUST_CHECK_HEADERS}\n#if !(${RUST_ARCH_CHECK} && ${RUST_PLATFORM_CHECK})\n#error Rust/C++ target mismatch\n#endif\nstatic_assert(sizeof(void *) == 8);\nint main() { return 0; }" RUST_CXX_TARGET_MATCHES)
if(NOT RUST_CXX_TARGET_MATCHES OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Configured C++ architecture/platform/pointer width does not match Rust host ${RUST_HOST}")
endif()
set(RUST_TARGET "${RUST_HOST}" CACHE INTERNAL "Validated native Rust target" FORCE)
set(RUST_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo" CACHE INTERNAL "Cargo artifacts" FORCE)
set(RUST_ARCHIVE "${RUST_TARGET_DIR}/${RUST_TARGET}/release/libopenttd_kernels.a" CACHE INTERNAL "Shared Rust archive" FORCE)
set(RUST_PLATFORM "${CMAKE_SYSTEM_NAME}" CACHE INTERNAL "Validated C++ platform" FORCE)
set(RUST_POINTER_WIDTH "${CMAKE_SIZEOF_VOID_P}" CACHE INTERNAL "Validated C++ pointer bytes" FORCE)
file(WRITE "${CMAKE_BINARY_DIR}/rust-toolchain.txt" "${RUST_VERSION}\ntarget=${RUST_TARGET}\narchitecture=${CMAKE_OSX_ARCHITECTURES}\npointer_bytes=${RUST_POINTER_WIDTH}\nsdk=${RUST_SDKROOT}\ndeployment_target=${CMAKE_OSX_DEPLOYMENT_TARGET}\n")
# Query the pinned standard library, rather than borrowing Linux link flags on Darwin.
file(WRITE "${CMAKE_BINARY_DIR}/rust-native-libs.rs" "pub fn native_link_probe() {}\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${RUST_BUILD_ENV}
    "${RUSTC_EXECUTABLE}" --crate-type staticlib --edition=2024 --target "${RUST_TARGET}"
    -C panic=abort -C overflow-checks=on -C opt-level=3 --print=native-static-libs
    "${CMAKE_BINARY_DIR}/rust-native-libs.rs" -o "${CMAKE_BINARY_DIR}/rust-native-libs.a"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE RUST_LIB_STDOUT
    ERROR_VARIABLE RUST_LIB_STDERR RESULT_VARIABLE RUST_RESULT)
file(WRITE "${CMAKE_BINARY_DIR}/rust-native-libs.log" "${RUST_LIB_STDOUT}${RUST_LIB_STDERR}")
string(REGEX MATCH "native-static-libs: ([^\r\n]+)" _ "${RUST_LIB_STDOUT}${RUST_LIB_STDERR}")
if(NOT RUST_RESULT EQUAL 0 OR NOT CMAKE_MATCH_1)
    message(FATAL_ERROR "Cannot obtain native static libraries for ${RUST_TARGET}; see rust-native-libs.log")
endif()
separate_arguments(RUST_NATIVE_LIBS UNIX_COMMAND "${CMAKE_MATCH_1}")
set(RUST_NATIVE_LIBS "${RUST_NATIVE_LIBS}" CACHE INTERNAL "Pinned Rust native link flags" FORCE)
# Makefiles do not track command-line/environment changes as output dependencies.
# Generate a content-stable configuration file and invalidate the release crate
# only when its effective configuration changes, including SDK-only changes.
set(RUST_CONFIGURATION "compiler=${RUSTC_EXECUTABLE}\ncargo=${CARGO_EXECUTABLE}\n${RUST_VERSION}target=${RUST_TARGET}\nsdk=${RUST_SDKROOT}\ndeployment=${CMAKE_OSX_DEPLOYMENT_TARGET}\n")
string(TOUPPER "${RUST_TARGET}" RUST_TARGET_ENV)
string(REPLACE "-" "_" RUST_TARGET_ENV "${RUST_TARGET_ENV}")
set(RUST_CONFIGURATION_ENV RUSTFLAGS CARGO_ENCODED_RUSTFLAGS RUSTC_WRAPPER
    RUSTC_WORKSPACE_WRAPPER CARGO_BUILD_RUSTFLAGS
    "CARGO_TARGET_${RUST_TARGET_ENV}_RUSTFLAGS" "CARGO_TARGET_${RUST_TARGET_ENV}_LINKER"
    CARGO_PROFILE_RELEASE_OPT_LEVEL CARGO_PROFILE_RELEASE_DEBUG
    CARGO_PROFILE_RELEASE_STRIP CARGO_PROFILE_RELEASE_DEBUG_ASSERTIONS
    CARGO_PROFILE_RELEASE_OVERFLOW_CHECKS CARGO_PROFILE_RELEASE_LTO
    CARGO_PROFILE_RELEASE_PANIC CARGO_PROFILE_RELEASE_INCREMENTAL
    CARGO_PROFILE_RELEASE_CODEGEN_UNITS)
foreach(RUST_ENV IN LISTS RUST_CONFIGURATION_ENV)
    if(DEFINED ENV{${RUST_ENV}})
        string(APPEND RUST_CONFIGURATION "${RUST_ENV}=$ENV{${RUST_ENV}}\n")
        list(APPEND RUST_BUILD_ENV "${RUST_ENV}=$ENV{${RUST_ENV}}")
    else()
        string(APPEND RUST_CONFIGURATION "${RUST_ENV}=<unset>\n")
        list(APPEND RUST_BUILD_ENV "--unset=${RUST_ENV}")
    endif()
endforeach()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/rust-build-configuration.txt" CONTENT "${RUST_CONFIGURATION}")
file(GLOB_RECURSE RUST_INPUTS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/rust/*.rs" "${CMAKE_SOURCE_DIR}/rust/*/Cargo.toml")
add_custom_command(OUTPUT "${RUST_ARCHIVE}"
    COMMAND ${CMAKE_COMMAND} -E env ${RUST_BUILD_ENV} "${CMAKE_COMMAND}"
        "-DCARGO=${CARGO_EXECUTABLE}" "-DSOURCE=${CMAKE_SOURCE_DIR}" "-DTARGET=${RUST_TARGET}"
        "-DLOG=${CMAKE_BINARY_DIR}/rust-build.log"
        "-DCONFIGURATION=${CMAKE_BINARY_DIR}/rust-build-configuration.txt" -P "${CMAKE_SOURCE_DIR}/cmake/BuildRust.cmake"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS ${RUST_INPUTS} "${CMAKE_SOURCE_DIR}/Cargo.toml" "${CMAKE_SOURCE_DIR}/Cargo.lock"
        "${CMAKE_SOURCE_DIR}/rust-toolchain.toml" "${CMAKE_SOURCE_DIR}/cmake/BuildRust.cmake"
        "${CMAKE_SOURCE_DIR}/cmake/Rust.cmake"
        "${CMAKE_BINARY_DIR}/rust-build-configuration.txt"
    COMMENT "Building shared Rust algorithms for ${RUST_TARGET}" VERBATIM)
add_custom_target(openttd_rust_build DEPENDS "${RUST_ARCHIVE}")
add_library(openttd_rust STATIC IMPORTED)
set_target_properties(openttd_rust PROPERTIES IMPORTED_LOCATION "${RUST_ARCHIVE}")
add_dependencies(openttd_rust openttd_rust_build)
# Keep framework pairs intact if a future pinned standard library requires them.
set(RUST_FRAMEWORK_NEXT FALSE)
foreach(RUST_LIB IN LISTS RUST_NATIVE_LIBS)
    if(RUST_FRAMEWORK_NEXT)
        target_link_options(openttd_rust INTERFACE "SHELL:-framework ${RUST_LIB}")
        set(RUST_FRAMEWORK_NEXT FALSE)
    elseif(RUST_LIB STREQUAL "-framework")
        set(RUST_FRAMEWORK_NEXT TRUE)
    else()
        target_link_libraries(openttd_rust INTERFACE "${RUST_LIB}")
    endif()
endforeach()
if(RUST_FRAMEWORK_NEXT)
    message(FATAL_ERROR "Incomplete Rust framework link flag")
endif()
target_compile_definitions(openttd_rust INTERFACE WITH_RUST)
