
cmake_minimum_required(VERSION 3.7)

project(timidity_mod)

set(TIMIDITY_AUDIO_GOGO FALSE CACHE BOOL "GOGO")
set(TIMIDITY_AUDIO_VORBIS FALSE CACHE BOOL "vorbis")
set(TIMIDITY_AUDIO_FLAC FALSE CACHE BOOL "flac")
set(TIMIDITY_AUDIO_PORTAUDIO FALSE CACHE BOOL "portaudio")

set(PORTAUDIO_INCLUDE_DIR "." CACHE PATH "portaudio include dir")

set(TIMIDITY_INTERFACE_W32GUI TRUE CACHE BOOL "Windows GUI")
set(TIMIDITY_INTERFACE_WINSYN FALSE CACHE BOOL "Windows synthesizer")

add_definitions(
    -DHAVE_VPRINTF
    -Dinline=__inline
    -DRETSIGTYPE=int
    -DSTDC_HEADERS
    -DHAVE_GETCWD
    -DHAVE_POPEN
    -DHAVE_SIGNAL
    -DHAVE_SNPRINTF
    -DHAVE_STRDUP
    -DHAVE_STRERROR
    -DHAVE_STRNCASECMP
    -DHAVE_STRSTR
    -DHAVE_VSNPRINTF
    -DHAVE_ERRNO_H
    -DHAVE_LIMITS_H
    -DHAVE_MALLOC_H
    -DHAVE_SYS_TYPES_H
    -DHAVE_STDINT_H
    -DHAVE_FCNTL_H
    -DHAVE_CONFIG_H

# Windows only
    -Dpopen=_popen
    -Dpclose=_pclose
    -Dstrdup=_strdup
    -DHAVE_MMSYSTEM_H
    -D__W32__
    -D__W32READDIR__
    -DANOTHER_MAIN
    -D__W32G__

    -D_CRT_SECURE_NO_WARNINGS

# network
    -DWINSOCK
    -DHAVE_SOCKET
    -DSUPPORT_SOCKET
    -DAU_W32
)

if(TIMIDITY_AUDIO_GOGO)
    add_definitions(
        -DHAVE_LIBGOGO
        -DAU_GOGO
        -DAU_GOGO_DLL
    )
endif()

if(TIMIDITY_AUDIO_VORBIS)
    add_definitions(
        -DHAVE_LIBVORBIS
        -DAU_VORBIS
        -DAU_VORBIS_DLL
    )
endif()

if(TIMIDITY_AUDIO_FLAC)
    add_definitions(
        -DAU_FLAC
        -DAU_FLAC_DLL
    )
endif()

if(TIMIDITY_AUDIO_PORTAUDIO)
    add_definitions(
        -DAU_PORTAUDIO
        -DAU_PORTAUDIO_DLL
    )

    include_directories("${PORTAUDIO_INCLUDE_DIR}")
endif()

if(TIMIDITY_INTERFACE_W32GUI)
    add_definitions(
        -DIA_W32GUI
    )
endif()

if(TIMIDITY_INTERFACE_WINSYN)
    add_definitions(
        -DTWSYNG32
        -DIA_W32G_SYN
    )
endif()

if(MSVC)
    add_compile_options(-W4 -Zi -fp:fast)

    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /GL /GS /guard:cf")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL /GS /guard:cf")

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")
	set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE
        "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /LTCG:incremental /DEBUG:FULL /GUARD:CF"
    )
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE
        "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /LTCG:incremental /DEBUG:FULL /GUARD:CF"
    )
	set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG:incremental")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
        "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /OPT:REF /DEBUG:FASTLINK"
    )
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO
        "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /OPT:REF /DEBUG:FASTLINK"
    )
else()
    add_compile_options(-Wall -Wextra)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -s")
endif()

include_directories("." interface libarc libunimod timidity utils)

add_subdirectory(interface)
add_subdirectory(libarc)
add_subdirectory(libunimod)
add_subdirectory(timidity)
add_subdirectory(utils)
