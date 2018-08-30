
cmake_minimum_required(VERSION 3.9)

project(timidity41)

set(TIM41_USE_AVX2 FALSE CACHE BOOL "Enable use of Intel AVX2 enhanced instructions")
set(TIM41_USE_CLANG_CL FALSE CACHE BOOL "Compile with clang-cl")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel/bin")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel/lib")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel/lib")

find_package(Git)

if("${Git_FOUND}" AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    add_custom_target(
        UpdateTim41Version
        COMMAND "${CMAKE_COMMAND}"
            -DGIT_EXECUTABLE="${GIT_EXECUTABLE}"
            -DINFILE="${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
            -DOUTFILE="${CMAKE_CURRENT_BINARY_DIR}/version.h"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/WriteVersion.cmake"
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/version.h"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Updating version info..."
    )
else()
    set(tim41VersionStr "current")
    set(tim41Version4Comma "41,0,0,0")
    configure_file(version.h.in version.h)

    add_custom_target(
        UpdateTim41Version
        COMMAND "${CMAKE_COMMAND}" -E echo "automatic version detection is not available"
    )
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
	-DHAVE_SYS_STAT_H
    -DHAVE_SYS_TYPES_H
    -DHAVE_FCNTL_H
    -DHAVE_LIBVORBIS
    -DHAVE_LIBGOGO
    -DHAVE_LIBMPG123
    -DHAVE_MMSYSTEM_H
    -DENABLE_SHERRY
    -DENABLE_THREAD
    -DSUPPORT_SOUNDSPEC
    -DANOTHER_MAIN
    -D__W32__
    -D__W32READDIR__
	-DUSE_TWSYN_BRIDGE
    -DENABLE_SFZ
	-DTIMW32G_USE_NEW_CONSOLE
	-DENABLE_DLS

# TiMidity++ audio
    -DAU_W32
    -DAU_WASAPI
    -DAU_WDMKS
    -DAU_OPUS
    -DAU_OPUS_DLL
    -DAU_SPEEX
	-DAU_SPEEX_DLL
    -DAU_LAME
    -DAU_GOGO
    -DAU_GOGO_DLL
    -DAU_VORBIS
    -DAU_VORBIS_DLL
    -DVORBIS_DLL_INCLUDE_VORBISENC
    -DVORBIS_DLL_INCLUDE_VORBISFILE
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

# pdcurses
    -DPDC_DLL_BUILD

# Windows
    -DWIN32_LEAN_AND_MEAN
    -DWINVER=0x0A00
    -D_WIN32_WINNT=0x0A00

# MSVCRT
    -D_CRT_SECURE_NO_WARNINGS
)

if(TIM41_USE_AVX2)
    add_definitions(-DUSE_AVX2)
else()
    add_definitions(-DUSE_SSE42)
endif()

if(MSVC)
    add_compile_options(/W4 /Zi /GS /fp:fast)

    if(NOT "${TIM41_USE_CLANG_CL}")
        add_compile_options(/Qspectre)
    endif()

    add_compile_options(/we4013)    # '%s' undefined; assuming extern returning int
    
    if(TIM41_USE_AVX2)
        add_compile_options(/arch:AVX2)
    endif()
    
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /std:c++17")

    if(NOT "${TIM41_USE_CLANG_CL}")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /GL /guard:cf")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL /guard:cf")
    endif()

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")

    if("${TIM41_USE_CLANG_CL}")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /DEBUG:FULL")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /DEBUG:FULL")
    else()
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /LTCG:incremental /DEBUG:FULL /GUARD:CF")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /LTCG:incremental /DEBUG:FULL /GUARD:CF")
        set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG:incremental")
    endif()

    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /OPT:REF /DEBUG:FASTLINK")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /OPT:REF /DEBUG:FASTLINK")

    add_definitions(
        -DSTDOUT_FILENO=1
    )
else()
    add_compile_options(-Wall -Wextra -Wno-missing-braces -ffast-math)

    if(TIM41_USE_AVX2)
        add_compile_options(-mavx2 -mfma)
    else()
        add_compile_options(-msse4.2)
    endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
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

include_directories(
    "."
    FLAC/include
    interface
    lame/DLL
    lame/include
    libarc
    libmpg123/include
    libunimod
    ogg/include
    opus/include
    pdcurses
    portaudio/include
    portmidi/pm_common
    portmidi/pm_win
    portmidi/porttime
    timidity
	twsyn_bridge
    utils
    vorbis/include
)

add_subdirectory(FLAC)
add_subdirectory(lame)
add_subdirectory(libmpg123)
add_subdirectory(ogg)
add_subdirectory(opus)
add_subdirectory(pdcurses)
add_subdirectory(portaudio)
add_subdirectory(portmidi)
add_subdirectory(speex)
add_subdirectory(vorbis)

add_subdirectory(cfgforsf)
add_subdirectory(interface)
add_subdirectory(libarc)
add_subdirectory(libunimod)
add_subdirectory(sf_view_gui)
add_subdirectory(timidity)
add_subdirectory(twsyn_bridge)
add_subdirectory(utils)
