
add_executable(calcnewton calcnewt.c)
add_custom_command(
    OUTPUT newton_table.c
    COMMAND calcnewton newton_table.c
)

set(
    TiMiditySources
    aiff_a.c
    aq.c
    audio_cnv.c
    au_a.c
    common.c
    controls.c
    effect.c
    filter.c
    freq.c
    gogo_a.c
    instrum.c
    list_a.c
    loadtab.c
    m2m.c
    mfi.c
    mfnode.c
    miditrace.c
    midi_a.c
    mix.c
    mod.c
    mod2midi.c
    modmid_a.c
    mt19937ar.c
    optcode.c
    output.c
    playmidi.c
    quantity.c
    raw_a.c
    rcp.c
    readmidi.c
    recache.c
    resample.c
    reverb.c
    sbkconv.c
    sffile.c
    sfitem.c
    smfconv.c
    smplfile.c
    sndfont.c
    tables.c
    timidity.c
    version.c
    vorbis_a.c
    w32_a.c
    wave_a.c
    wrdt.c
    wrd_read.c
    aenc.h
    aq.h
    audio_cnv.h
    audriv.h
    common.h
    controls.h
    dlutils.h
    filter.h
    gogo_a.h
    instrum.h
    mac_com.h
    mac_main.h
    mfnode.h
    miditrace.h
    mix.h
    mod.h
    mod2midi.h
    mt19937ar.h
    optcode.h
    output.h
    playmidi.h
    readmidi.h
    recache.h
    resample.h
    reverb.h
    sffile.h
    sfitem.h
    sflayer.h
    smfconv.h
    sysdep.h
    tables.h
    timidity.h
    wrd.h

    $<TARGET_OBJECTS:interface_res>
)

set_source_files_properties(resample.c OBJECT_DEPENDS newton_table.c)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(
    TiMidityLibs

    # TiMidity++ libs
    interface
    libarc
    libunimod
    utils

    # WinAPI
    kernel32
    user32
    gdi32
    winspool
    comdlg32
    advapi32
    shell32
    ole32
    oleaut32
    uuid
    comctl32
    winmm
    wsock32
)

if(TIMIDITY_AUDIO_GOGO)
    list(
        APPEND TiMiditySources
        w32_gogo.c
        w32_gogo.h
        w32g_ogg_dll.c
    )
endif()

if(TIMIDITY_AUDIO_VORBIS)
    list(
        APPEND TiMiditySources
        w32g_vorbis_dll.c
        w32g_vorbis_dll.c
    )
endif()

if(TIMIDITY_AUDIO_FLAC)
    list(
        APPEND TiMiditySources
        w32_libFLAC_dll.c
        w32_libFLAC_dll.h
        w32_libFLAC_dll_g.h
        w32_libFLAC_dll_i.h
    )
endif()

if(TIMIDITY_AUDIO_PORTAUDIO)
    list(
        APPEND TiMiditySources
        portaudio_a.c
        w32_portaudio.h
        w32_portaudio_dll.c
    )
endif()

if(TIMIDITY_INTERFACE_W32GUI)
    add_executable(timw32g WIN32 ${TiMiditySources})
    target_link_libraries(timw32g ${TiMidityLibs})
elseif(TIMIDITY_INTERFACE_WINSYN)
    add_executable(twsyng WIN32 ${TiMiditySources})
    target_link_libraries(timidity ${TiMidityLibs})
endif()
