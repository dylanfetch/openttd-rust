execute_process(COMMAND "${CARGO}" rustc --package openttd-kernels --release --locked
    --target "${TARGET}" --manifest-path "${SOURCE}/Cargo.toml" -- --print=native-static-libs
    WORKING_DIRECTORY "${SOURCE}" OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERROR RESULT_VARIABLE RESULT)
file(WRITE "${LOG}" "${OUTPUT}${ERROR}")
if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "Rust archive build failed: ${OUTPUT}${ERROR}")
endif()
message(STATUS "${OUTPUT}${ERROR}")
