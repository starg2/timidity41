
add_executable(
	twsyn_bridge WIN32

	twsyn_bridge_exe.c

	twsyn_bridge_common.h
	twsyn_bridge_exe_res.h

	twsyn_bridge_exe.rc
)

if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set_target_properties(twsyn_bridge PROPERTIES OUTPUT_NAME "twsyn_bridge_x64")
else()
    set_target_properties(twsyn_bridge PROPERTIES OUTPUT_NAME "twsyn_bridge_x86")
endif()

target_link_libraries(twsyn_bridge shlwapi winmm)

install(TARGETS twsyn_bridge)
