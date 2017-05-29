
cmake_minimum_required(VERSION 3.7)

project(timidity_mod)

set(TIMIDITY_AUDIO_GOGO FALSE CACHE BOOL "GOGO")
set(TIMIDITY_AUDIO_VORBIS FALSE CACHE BOOL "vorbis")
set(TIMIDITY_AUDIO_FLAC FALSE CACHE BOOL "flac")

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
    add_compile_options(-W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

include_directories(interface libarc libunimod timidity utils)

add_subdirectory(interface)
add_subdirectory(libarc)
add_subdirectory(libunimod)
add_subdirectory(timidity)
add_subdirectory(utils)
