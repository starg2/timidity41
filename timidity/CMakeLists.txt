
add_definitions(
    [[-DDEFAULT_PATH="./"]]
)

# calcnewton

add_executable(calcnewton calcnewt.c)

if(MSVC AND "${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    target_compile_options(calcnewton PRIVATE /Oi-)
endif()

if(MSVC)
    target_compile_options(calcnewton PRIVATE /Od)
else()
    target_compile_options(calcnewton PRIVATE -O0)
endif()

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/newton_table.c"
    COMMAND calcnewton "${CMAKE_CURRENT_BINARY_DIR}/newton_table.c"
)

set_source_files_properties(resample.c OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/newton_table.c")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# timidity common

set(
    TIMIDITY_INTERFACE_DEPENDENT_SOURCE

    controls.c
    flac_a.c
    gogo_a.c
    int_synth.c
    lame_a.c
    miditrace.c
    npipe_a.c
    opus_a.c
    output.c
    playmidi.c
    portaudio_a.c
    sndfontini.c
    speex_a.c
    thread.c
    timer2.c
    timidity.c
    vorbis_a.c
    wrdt.c
    
    controls.h
    flac_a.h
    gogo_a.h
    int_synth.h
    LameEnc.h
    miditrace.h
    output.h
    portaudio_a.h
    thread.h
    timer2.h
    timidity.h
    vstwrapper.h
    wrd.h
)

add_definitions(${TIMIDITY_FE_DEFINITIONS})

set(
    TIMIDITY_COMMON_SOURCES

    aiff_a.c
    ao_a.c
    aq.c
    au_a.c
    audio_cnv.c
    common.c
    decode.c
	dls.cpp
	ecw.cpp
    effect.c
    envelope.c
    filter.c
    freq.c
    instrum.c
    midi_a.c
    oscillator.c
    list_a.c
    loadtab.c
    m2m.c
    mfi.c
    mfnode.c
    mix.c
    mod.c
    mod2midi.c
    modmid_a.c
    mt19937ar.c
    myini.c
    optcode.c
    quantity.c
    raw_a.c
    rcp.c
    readmidi.c
    recache.c
    resample.c
    sbkconv.c
    sffile.c
    sfitem.c
    sfz.cpp
    benchmark_a.c
    # smfconv.c
    smplfile.c
    sndfont.c
    tables.c
    version.c
    voice_effect.c
    volumecalc_a.c
    w32_a.c
    w32_gogo.c
    wasapi_a.c
    wave_a.c
    w32g_ogg_dll.c
    w32_portaudio_dll.c
	w32_speex_dll.c
    w32g_vorbis_dll.c
    w32g_vorbisenc_dll.c
    w32g_vorbisfile_dll.c
    w32g_opus_dll.c
    w32_libFLAC_dll.c
    w32_libmpg123_dll.c
    w32_libOggFLAC_dll.c
    wdmks_a.c
    wrd_read.c

    aenc.h
    aq.h
    audio_cnv.h
    audriv.h
    common.h
    decode.h
	dls.h
    dlutils.h
	ecw.h
    filter.h
    freq.h
    instrum.h
    oscillator.h
    loadsndfontini.h
    mac_com.h
    mac_main.h
    mfnode.h
    mix.h
    mod.h
    mod2midi.h
    mt19937ar.h
    myini.h
    optcode.h
    playmidi.h
    pragmapack.h
    quantity.h
    readmidi.h
    recache.h
    resample.h
    effect.h
    sffile.h
    sfitem.h
    sflayer.h
    sfz.h
    # smfconv.h
    sndfontini.h
    sysdep.h
    tables.h
    voice_effect.h
    w32_a.h
    w32_gogo.h
    w32_libFLAC_dll.h
    w32_libFLAC_dll_g.h
    w32_libFLAC_dll_i.h
    w32_libOggFLAC_dll.h
    w32_libOggFLAC_dll_g.h
    w32_libOggFLAC_dll_i.h
	wasapi_a.h
    wdmks_a.h

    ../vorbis-tools/vorbiscomment/vcedit.c
    ../vorbis-tools/vorbiscomment/vcedit.h
)

add_library(timidity_common OBJECT ${TIMIDITY_COMMON_SOURCES})
add_library(timidity_common_u8 OBJECT ${TIMIDITY_COMMON_SOURCES})
target_compile_definitions(timidity_common_u8 PRIVATE -DUNICODE -D_UNICODE)

if("${CMAKE_C_COMPILER_ID}" MATCHES "Intel")
    # Workaround for xilink: error #10014: problem during multi-file optimization compilation (code 4)
    set_source_files_properties(ecw.cpp effect.c PROPERTIES COMPILE_OPTIONS "/Qipo-")
endif()

set(
    TIMIDITY_COMMON_LIBS

    avrt
    comctl32
    setupapi
    shlwapi
    winmm
    ws2_32
)

# timidity

add_executable(
    timidity

    ${TIMIDITY_INTERFACE_DEPENDENT_SOURCE}
    $<TARGET_OBJECTS:interface>
    $<TARGET_OBJECTS:libarc>
    $<TARGET_OBJECTS:libunimod>
    $<TARGET_OBJECTS:timidity_common>
    $<TARGET_OBJECTS:utils>
)

add_dependencies(timidity UpdateTim41Version)
target_compile_definitions(timidity PRIVATE -DIA_NCURSES -DIA_VT100)
target_link_libraries(timidity ${TIMIDITY_COMMON_LIBS} pdcurses)

# twsynsrv

add_executable(
    twsynsrv

    ${TIMIDITY_INTERFACE_DEPENDENT_SOURCE}
    $<TARGET_OBJECTS:interface_synsrv>
    $<TARGET_OBJECTS:libarc>
    $<TARGET_OBJECTS:libunimod>
    $<TARGET_OBJECTS:timidity_common>
    $<TARGET_OBJECTS:utils>
)

add_dependencies(twsynsrv UpdateTim41Version)
target_compile_definitions(twsynsrv PRIVATE -DIA_WINSYN -DTWSYNSRV -DTWSYNG32INI)
target_link_libraries(twsynsrv ${TIMIDITY_COMMON_LIBS})

if("${MSVC}" AND NOT ("${CMAKE_C_COMPILER_ID}" MATCHES "Clang"))
    # timw32g

    add_executable(
        timw32g WIN32

        ${TIMIDITY_INTERFACE_DEPENDENT_SOURCE}
        $<TARGET_OBJECTS:interface_w32g>
        $<TARGET_OBJECTS:libarc>
        $<TARGET_OBJECTS:libunimod>
        $<TARGET_OBJECTS:timidity_common>
        $<TARGET_OBJECTS:utils>
    )

    add_dependencies(timw32g UpdateTim41Version)
    target_compile_definitions(timw32g PRIVATE -DIA_W32GUI -D__W32G__)
    target_link_libraries(timw32g ${TIMIDITY_COMMON_LIBS})
    DisableMSManifest(timw32g)

    # twsyng

    add_executable(
        twsyng WIN32

        ${TIMIDITY_INTERFACE_DEPENDENT_SOURCE}
        $<TARGET_OBJECTS:interface_w32gsyn>
        $<TARGET_OBJECTS:libarc>
        $<TARGET_OBJECTS:libunimod>
        $<TARGET_OBJECTS:timidity_common>
        $<TARGET_OBJECTS:utils>
    )

    add_dependencies(twsyng UpdateTim41Version)
    target_compile_definitions(twsyng PRIVATE -DIA_WINSYN -DIA_W32G_SYN -D__W32G__ -DTWSYNG32 -DTWSYNG32INI)
    target_link_libraries(twsyng ${TIMIDITY_COMMON_LIBS})
    DisableMSManifest(twsyng)
endif()

# timw32g_u8

add_executable(
    timw32g_u8 WIN32

    ${TIMIDITY_INTERFACE_DEPENDENT_SOURCE}
    $<TARGET_OBJECTS:interface_w32g_u8>
    $<TARGET_OBJECTS:libarc_u8>
    $<TARGET_OBJECTS:libunimod>
    $<TARGET_OBJECTS:timidity_common_u8>
    $<TARGET_OBJECTS:utils_u8>
)

add_dependencies(timw32g_u8 UpdateTim41Version)
target_compile_definitions(timw32g_u8 PRIVATE -DUNICODE -D_UNICODE -DIA_W32GUI -D__W32G__)
target_link_libraries(timw32g_u8 ${TIMIDITY_COMMON_LIBS})

if(NOT "${MSVC}")
    target_link_libraries(timw32g_u8 -municode)
endif()

DisableMSManifest(timw32g_u8)

# twsyng_u8

add_executable(
    twsyng_u8 WIN32

    ${TIMIDITY_INTERFACE_DEPENDENT_SOURCE}
    $<TARGET_OBJECTS:interface_w32gsyn_u8>
    $<TARGET_OBJECTS:libarc_u8>
    $<TARGET_OBJECTS:libunimod>
    $<TARGET_OBJECTS:timidity_common_u8>
    $<TARGET_OBJECTS:utils_u8>
)

add_dependencies(twsyng_u8 UpdateTim41Version)
target_compile_definitions(twsyng_u8 PRIVATE -DUNICODE -D_UNICODE -DIA_WINSYN -DIA_W32G_SYN -D__W32G__ -DTWSYNG32 -DTWSYNG32INI)
target_link_libraries(twsyng_u8 ${TIMIDITY_COMMON_LIBS})

if(NOT "${MSVC}")
    target_link_libraries(twsyng_u8 -municode)
endif()

DisableMSManifest(twsyng_u8)
