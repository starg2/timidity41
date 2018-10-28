
add_definitions(-D_WINDLL)

add_library(
    portmidi SHARED

    pm_common/pminternal.h
    pm_common/pmutil.c
    pm_common/pmutil.h
    pm_common/portmidi.c
    pm_common/portmidi.h

    pm_win/pmwin.c
    pm_win/pmwinmm.c
    pm_win/pmwinmm.h

    porttime/porttime.c
    porttime/porttime.h
    porttime/ptwinmm.c
)

target_link_libraries(portmidi winmm)
