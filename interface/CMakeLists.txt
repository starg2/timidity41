
add_library(
    interface STATIC

    dumb_c.c
    ncurs_c.c
    npsyn_c.c
#    portmidisyn_c.c
    rtsyn_common.c
#    rtsyn_npipe.c
#    rtsyn_winmm.c
    vt100.c
    vt100_c.c
    VTPrsTbl.c
    wrdt_dumb.c
    wrdt_tty.c
    wrdt_wcon.c
#    winsyn_c.c

    gtk_h.h
    mac_c.h
    mac_mag.h
    mac_wrdwindow.h
    motif.h
    rtsyn.h
    server_defs.h
    soundspec.h
    vt100.h
    VTparse.h
    w32g_utl.h
    xaw.h
    xskin.h
    x_mag.h
    x_sherry.h
    x_wrdwindow.h
)

set(
    INTERFACE_W32G_COMMON_SOURCES

    w32g_c.c
    w32g_dib.c
    w32g_i.c
    w32g_ini.c
    w32g_int_synth_editor.c
    w32g_mag.c
    w32g_playlist.c
    w32g_pref.c
    w32g_subwin.c
    w32g_subwin2.c
    w32g_subwin3.c
    w32g_ut2.c
    w32g_utl.c
    w32g_soundspec.c
    wrdt_w32g.c

    soundspec.h
    w32g.h
    w32g_dib.h
    w32g_int_synth_editor.h
    w32g_mag.h
    w32g_pref.h
    w32g_rec.h
    w32g_res.h
    w32g_subwin.h
    w32g_tracer.h
    w32g_ut2.h
    w32g_utl.h
    w32g_wrd.h
)

set_source_files_properties(
    w32g_res.rc

    PROPERTIES
        OBJECT_DEPENDS
            w32g_btn.bmp
            w32g_icon.ico
            w32g_icon_gray.ico
            w32g_icon_normal.ico
            w32g_icon_silver.ico
            w32g_playlist_play.ico
            w32g_sleep.bmp
            w32g_subbtn.bmp
            w32g_tracer.bmp
)

# for timw32g

add_library(
    interface_w32g STATIC

    ${INTERFACE_W32G_COMMON_SOURCES}
)

target_compile_definitions(interface_w32g PRIVATE -DIA_W32GUI)

add_library(
    interface_w32g_res OBJECT
    w32g_res.rc
)

target_compile_definitions(interface_w32g_res PRIVATE -DIA_W32GUI)

add_library(
    interface_w32gsyn STATIC

    ${INTERFACE_W32G_COMMON_SOURCES}
)

target_compile_definitions(interface_w32gsyn PRIVATE -DIA_W32G_SYN)

add_library(
    interface_w32gsyn_res OBJECT
    w32g_res.rc
)

target_compile_definitions(interface_w32gsyn_res PRIVATE -DIA_W32G_SYN)