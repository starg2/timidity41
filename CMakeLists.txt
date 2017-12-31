
cmake_minimum_required(VERSION 3.9)

project(timidity41)

find_package(Git)

if("${Git_FOUND}" AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    add_custom_target(
        UpdateTim41Version
        COMMAND "${CMAKE_COMMAND}"
            -DGIT_EXECUTABLE="${GIT_EXECUTABLE}"
            -DINFILE="${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
            -DOUTFILE="${CMAKE_CURRENT_BINARY_DIR}/version.h"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/WriteVersion.cmake"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Updating version info..."
    )
else()
    set(tim41VersionStr "current")
    set(tim41Version4Comma "41,0,0,0")
    configure_file(version.h.in version.h)
endif()

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_definitions(
# TiMidity++ common
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
    -DHAVE_VPRINTF
    -DHAVE_VSNPRINTF
    -DHAVE_ERRNO_H
    -DHAVE_LIMITS_H
    -DHAVE_MALLOC_H
    -DHAVE_STDLIB_H
    -DHAVE_SYS_TYPES_H
    -DHAVE_FCNTL_H
    -DHAVE_LIBVORBIS
    -DHAVE_LIBGOGO
    -DHAVE_MMSYSTEM_H
    -DENABLE_SHERRY
    -DENABLE_THREAD
    -DSUPPORT_SOUNDSPEC
    -DANOTHER_MAIN
    -D__W32__
    -D__W32READDIR__

# TiMidity++ audio
    -DAU_W32
    -DAU_WASAPI
    -DAU_OPUS
    -DAU_OPUS_DLL
    -DAU_SPEEX
    -DAU_LAME
    -DAU_GOGO
    -DAU_GOGO_DLL
    -DAU_VORBIS
    -DAU_VORBIS_DLL
    -DVORBIS_DLL_INCLUDE_VORBISENC
    -DAU_FLAC
    -DAU_FLAC_DLL
    -DAU_OGGFLAC
    -DAU_PORTAUDIO
    -DAU_PORTAUDIO_DLL
    -DPORTAUDIO_V19
    -DPORTAUDIO_V19_6
    -DVST_LOADER_ENABLE
    -DVSTWRAP_EXT
    -DAU_NPIPE
    -DAU_LIST
    -DAU_WRITE_MIDI
    -DAU_MODMIDI
    -DAU_VOLUME_CALC
    -DAU_BENCHMARK

# network
    -DWINSOCK
    -DHAVE_SOCKET
    -DSUPPORT_SOCKET

# Windows
    -DWIN32_LEAN_AND_MEAN
    -DWINVER=0x0601
    -D_WIN32_WINNT=0x0601

# MSVCRT
    -D_CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    add_compile_options(/W4 /Zi /fp:fast /permissive-)

    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /GL /GS /guard:cf")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL /GS /guard:cf")

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /LTCG:incremental /DEBUG:FULL /GUARD:CF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /LTCG:incremental /DEBUG:FULL /GUARD:CF")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG:incremental")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /OPT:REF /DEBUG:FASTLINK")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /OPT:REF /DEBUG:FASTLINK")

    add_definitions(
        -DSTDOUT_FILENO=1
    )
else()
    add_compile_options(-Wall -Wextra -ffast-math)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -s")
    set(CMAKE_RC_FLAGS "-O coff")
endif()

function(DisableMSManifest targetName)
    if(MSVC)
        get_property(linkFlags TARGET ${targetName} PROPERTY LINK_FLAGS)
        set_target_properties(${targetName} PROPERTIES LINK_FLAGS "${linkFlags} /MANIFEST:NO")
    endif()
endfunction()

include_directories("." interface libarc libunimod pdcurses portaudio/pa_common timidity utils)

add_subdirectory(pdcurses)
add_subdirectory(speex)

add_subdirectory(interface)
add_subdirectory(libarc)
add_subdirectory(libunimod)
add_subdirectory(timidity)
add_subdirectory(utils)
