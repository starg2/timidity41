
add_library(
    interface STATIC
    w32g.h
    w32g_c.c
    w32g_dib.c
    w32g_dib.h
    w32g_i.c
    w32g_ini.c
    w32g_mag.c
    w32g_mag.h
    w32g_playlist.c
    w32g_pref.c
    w32g_pref.h
    w32g_rec.h
    w32g_res.h
    w32g_subwin.c
    w32g_subwin.h
    w32g_subwin2.c
    w32g_subwin3.c
    w32g_syn.c
    w32g_tracer.h
    w32g_ut2.c
    w32g_ut2.h
    w32g_utl.c
    w32g_utl.h
    w32g_wrd.h
    wrdt_w32g.c
)

add_library(
    interface_res OBJECT
    w32g_res.rc
)
