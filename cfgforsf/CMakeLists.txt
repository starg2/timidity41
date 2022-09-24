
add_definitions(-DCFG_FOR_SF)

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

set_source_files_properties(../timidity/version.c PROPERTIES OBJECT_DEPENDS "${PROJECT_BINARY_DIR}/version.h")
add_dependencies(timidity_cfgforsf UpdateTim41Version)

add_executable(
    cfgforsf
    
    ../timidity/sndfont.c
)

target_link_libraries(cfgforsf timidity_cfgforsf libarc utils ws2_32)
install(TARGETS cfgforsf)
