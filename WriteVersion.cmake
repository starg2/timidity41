
execute_process(
    COMMAND "${GIT_EXECUTABLE}" describe
    OUTPUT_VARIABLE gitDescribeResult
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(REPLACE "tim" "" tim41VersionStr "${gitDescribeResult}")
string(REGEX MATCH "[0-9.]+\\.[0-9.]+\\.[0-9.]+" tim41Version3 "${gitDescribeResult}")
string(REPLACE "." "," tim41Version3Comma "${tim41Version3}")
configure_file(${INFILE} ${OUTFILE})
