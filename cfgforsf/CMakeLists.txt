
add_definitions(-DCFG_FOR_SF)
remove_definitions(${CFGFORSF_UNNECESSARY_DEFINITIONS})

add_library(
    timidity_cfgforsf OBJECT

    ../timidity/common.c
    ../timidity/controls.c
    ../timidity/decode.c
    ../timidity/filter.c
    ../timidity/freq.c
    ../timidity/instrum.c
    ../timidity/mt19937ar.c
    ../timidity/quantity.c
    ../timidity/resample.c
    ../timidity/sbkconv.c
    ../timidity/sffile.c
    ../timidity/sfitem.c
    ../timidity/smplfile.c
    ../timidity/tables.c
    ../timidity/version.c

    ../timidity/common.h
    ../timidity/controls.h
    ../timidity/decode.h
    ../timidity/filter.h
    ../timidity/freq.h
    ../timidity/instrum.h
    ../timidity/mt19937ar.h
    ../timidity/quantity.h
    ../timidity/resample.h
    ../timidity/sffile.h
    ../timidity/sfitem.h
    ../timidity/sflayer.h
    ../timidity/tables.h
    ../timidity/timidity.h
)

add_executable(
    cfgforsf
    
    ../timidity/sndfont.c

    $<TARGET_OBJECTS:timidity_cfgforsf>
    $<TARGET_OBJECTS:libarc>
    $<TARGET_OBJECTS:utils>
)

add_dependencies(cfgforsf UpdateTim41Version)
target_link_libraries(cfgforsf ws2_32)
