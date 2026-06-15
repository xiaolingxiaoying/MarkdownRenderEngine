execute_process(
    COMMAND "${MWRENDER_CLI}" - --fragment
    INPUT_FILE "${MWRENDER_INPUT}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "stdin render failed (${result}): ${error}")
endif()

if(NOT output MATCHES "id=\"heading\"[^>]*>Heading</h1>")
    message(FATAL_ERROR "stdin render output did not contain the expected heading")
endif()
