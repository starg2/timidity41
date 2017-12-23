
add_library(
    utils STATIC

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
