
add_definitions(
    [[-DTIMID_VERSION="current"]]
    [[-DDEFAULT_PATH=".\\"]]
)

# calcnewton

add_executable(calcnewton calcnewt.c)
add_custom_command(
    OUTPUT newton_table.c
    COMMAND calcnewton newton_table.c
)

set_source_files_properties(resample.c OBJECT_DEPENDS newton_table.c)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# timidity common

set(
    TIMIDITY_COMMON_SOURCES

    aiff_a.c
    ao_a.c
    aq.c
    au_a.c
    audio_cnv.c
    common.c
    controls.c
    effect.c
    envelope.c
    filter.c
    flac_a.c
    freq.c
    gogo_a.c
    instrum.c
    int_synth.c
    lame_a.c
    midi_a.c
    oscillator.c
    list_a.c
    loadtab.c
    m2m.c
    mfi.c
    mfnode.c
    miditrace.c
    mix.c
    mod.c
    mod2midi.c
    modmid_a.c
    mt19937ar.c
    myini.c
    npipe_a.c
    optcode.c
    opus_a.c
    output.c
    playmidi.c
    portaudio_a.c
    quantity.c
    raw_a.c
    rcp.c
    readmidi.c
    recache.c
    resample.c
    sbkconv.c
    sffile.c
    sfitem.c
    benchmark_a.c
    smfconv.c
    smplfile.c
    sndfont.c
    sndfontini.c
    speex_a.c
    tables.c
    thread.c
    timer2.c
    timidity.c
    version.c
    voice_effect.c
    volumecalc_a.c
    vorbis_a.c
    w32_a.c
    w32_gogo.c
    wave_a.c
    w32g_ogg_dll.c
    w32_portaudio_dll.c
    w32g_vorbis_dll.c
    w32g_vorbisenc_dll.c
    w32g_opus_dll.c
    w32_libFLAC_dll.c
    w32_libOggFLAC_dll.c
    wrd_read.c
    wrdt.c
    
    aenc.h
    aq.h
    audio_cnv.h
    audriv.h
    common.h
    controls.h
    dlutils.h
    filter.h
    freq.h
    gogo_a.h
    instrum.h
    int_synth.h
    LameEnc.h
    oscillator.h
    loadsndfontini.h
    mac_com.h
    mac_main.h
    mfnode.h
    miditrace.h
    mix.h
    mod.h
    mod2midi.h
    mt19937ar.h
    myini.h
    optcode.h
    output.h
    playmidi.h
    portaudio_a.h
    pragmapack.h
    quantity.h
    readmidi.h
    recache.h
    resample.h
    effect.h
    sffile.h
    sfitem.h
    sflayer.h
    smfconv.h
    sndfontini.h
    sysdep.h
    tables.h
    thread.h
    timer2.h
    timidity.h
    voice_effect.h
    vstwrapper.h
    w32_a.h
    w32_gogo.h
    w32_libFLAC_dll.h
    w32_libFLAC_dll_g.h
    w32_libFLAC_dll_i.h
    w32_libOggFLAC_dll.h
    w32_libOggFLAC_dll_g.h
    w32_libOggFLAC_dll_i.h
    wrd.h
)

set(
    TIMIDITY_COMMON_LIBS

    libarc
    libunimod
    utils
)

# timidity

add_executable(
    timidity

    ${TIMIDITY_COMMON_SOURCES}
)

target_compile_definitions(timidity PRIVATE -DIA_NCURSES)
target_link_libraries(timidity interface ${TIMIDITY_COMMON_LIBS})

# twsynsrv

add_executable(
    twsynsrv

    ${TIMIDITY_COMMON_SOURCES}
)

target_compile_definitions(twsynsrv PRIVATE -DIA_WINSYN)
target_link_libraries(twsynsrv interface ${TIMIDITY_COMMON_LIBS})

# timw32g

add_executable(
    timw32g WIN32

    ${TIMIDITY_COMMON_SOURCES}
    $<TARGET_OBJECTS:interface_w32g_res>
)

target_compile_definitions(timw32g PRIVATE -DIA_W32GUI)
target_link_libraries(timw32g interface_w32g ${TIMIDITY_COMMON_LIBS})

# twsyng

add_executable(
    twsyng WIN32

    ${TIMIDITY_COMMON_SOURCES}
    $<TARGET_OBJECTS:interface_w32gsyn_res>
)

target_compile_definitions(twsyng PRIVATE -DIA_W32G_SYN -DTWSYNG32)
target_link_libraries(twsyng interface_w32gsyn ${TIMIDITY_COMMON_LIBS})
