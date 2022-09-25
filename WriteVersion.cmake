
execute_process(
    COMMAND "${GIT_EXECUTABLE}" describe --first-parent --tags
    OUTPUT_VARIABLE gitDescribeResult
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(REPLACE "tim" "" tim41VersionStr "${gitDescribeResult}")

if("${gitDescribeResult}" MATCHES "[0-9]+\\.[0-9]+\\.[0-9]+-[0-9]+")
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+-[0-9]+" tim41Version4 "${gitDescribeResult}")
else()
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" tim41Version4 "${gitDescribeResult}")
    string(APPEND tim41Version4 ".0")
endif()

string(REGEX REPLACE "[\\.\\-]" "," tim41Version4Comma "${tim41Version4}")
configure_file(${INFILE} ${OUTFILE})
