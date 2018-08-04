
add_library(
    ogg SHARED

    include/ogg/ogg.h
    include/ogg/os_types.h

    src/bitwise.c
    src/framing.c

    win32/ogg.def
)
