
add_definitions(-DCFG_FOR_SF)

add_executable(
    cfgforsf

    ../interface/dumb_c.c
    ../timidity/common.c
    ../timidity/controls.c
    ../timidity/decode.c
    ../timidity/envelope.c
    ../timidity/filter.c
    ../timidity/freq.c
    ../timidity/instrum.c
    ../timidity/int_synth.c
    ../timidity/mt19937ar.c
    ../timidity/myini.c
    ../timidity/quantity.c
    ../timidity/resample.c
    ../timidity/sbkconv.c
    ../timidity/sffile.c
    ../timidity/sfitem.c
    ../timidity/sfz.cpp
    ../timidity/smplfile.c
    ../timidity/sndfont.c
    ../timidity/tables.c
    ../timidity/version.c
    ../timidity/w32g_vorbis_dll.c
    ../timidity/w32g_vorbisfile_dll.c

    ../timidity/common.h
    ../timidity/controls.h
    ../timidity/decode.h
    ../timidity/envelope.h
    ../timidity/filter.h
    ../timidity/freq.h
    ../timidity/instrum.h
    ../timidity/int_synth.h
    ../timidity/mt19937ar.h
    ../timidity/myini.h
    ../timidity/output.h
    ../timidity/playmidi.h
    ../timidity/quantity.h
    ../timidity/readmidi.h
    ../timidity/resample.h
    ../timidity/sffile.h
    ../timidity/sfitem.h
    ../timidity/sflayer.h
    ../timidity/sfz.h
    ../timidity/tables.h
    ../timidity/timidity.h
    ../timidity/wrd.h
    ../utils/mblock.h
    ../utils/nkflib.h
    ../utils/strtab.h
    ../utils/support.h

    $<TARGET_OBJECTS:libarc>
    $<TARGET_OBJECTS:utils>
)

add_dependencies(cfgforsf UpdateTim41Version)
target_link_libraries(cfgforsf ws2_32)
