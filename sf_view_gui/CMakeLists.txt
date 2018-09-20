
add_definitions(
    -DCFG_FOR_SF
    -DSF2VIEWER_GUI
)

remove_definitions(${CFGFORSF_UNNECESSARY_DEFINITIONS})

add_executable(
    sfviewer WIN32

    mysndfont.c
    OpenDlg.cpp
    sfview_main.cpp
    ../timidity/myini.c

    OpenDlg.h
    resource.h
    ../timidity/myini.h

    sfview_dlg.rc

    $<TARGET_OBJECTS:timidity_cfgforsf>
    $<TARGET_OBJECTS:libarc>
    $<TARGET_OBJECTS:utils>
)

add_dependencies(sfviewer UpdateTim41Version)
target_link_libraries(sfviewer comctl32 shlwapi ws2_32)
