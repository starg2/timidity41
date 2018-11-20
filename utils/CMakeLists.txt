
set(
    UTILS_SOURCES

    bitset.c
    fft.c
    fft4g.c
    getaddrinfo.c
    getopt.c
    mblock.c
    memb.c
    net.c
    nkflib.c
    readdir_win.c
    strtab.c
    support.c
    timer.c

    bitset.h
    fft.h
    mac_readdir.h
    mac_util.h
    mblock.h
    memb.h
    net.h
    nkflib.h
    readdir.h
    strtab.h
    support.h
    timer.h
)

add_library(utils OBJECT ${UTILS_SOURCES})
add_library(utils_u8 OBJECT ${UTILS_SOURCES})
target_compile_definitions(utils_u8 PRIVATE -DUNICODE -D_UNICODE)
