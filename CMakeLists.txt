
cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0091 NEW)   # MSVC runtime library flags are selected by an abstraction.
cmake_policy(SET CMP0092 NEW)   # Don't add /W3 flag in CMAKE_C_FLAGS by default.

project(timidity41)

set(TIM41_X86_SIMD_LEVEL SSE42 CACHE STRING "x86 SIMD instruction set extension to use")
set_property(CACHE TIM41_X86_SIMD_LEVEL PROPERTY STRINGS SSE2 SSE42 AVX AVX2 AVX512)

set(TIM41_INTEL_ONLY FALSE CACHE BOOL "Optimize exclusively for Intel CPU; resulting binaries won't run on non-Intel CPUs")

set(TIM41_GCC_WITH_LIBICONV TRUE CACHE BOOL "GCC supports charset conversion")

set(TIM41_SPECTRE_MITIGATION FALSE CACHE BOOL "Enable Spectre mitigation")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/bin" CACHE PATH "Where to put all the RUNTIME target files when built.")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/lib" CACHE PATH "Where to put all the ARCHIVE target files when built.")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/lib" CACHE PATH "Where to put all the LIBRARY target files when built.")

find_package(Git)

if("${Git_FOUND}" AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    add_custom_target(
        UpdateTim41Version
        COMMAND "${CMAKE_COMMAND}"
            -DGIT_EXECUTABLE="${GIT_EXECUTABLE}"
            -DINFILE="${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
            -DOUTFILE="${PROJECT_BINARY_DIR}/version.h"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/WriteVersion.cmake"
        BYPRODUCTS "${PROJECT_BINARY_DIR}/version.h"
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

# base definitions
add_definitions(
# TiMidity++ common
    -DWIN32
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
    -DHAVE_MMSYSTEM_H
    -DENABLE_SHERRY
    -DENABLE_THREAD
    -DSUPPORT_SOUNDSPEC
    -DANOTHER_MAIN
    -D__W32__
    -D__W32READDIR__
	-DUSE_TWSYN_BRIDGE
	-DTIMW32G_USE_NEW_CONSOLE

# TiMidity++ audio
    -DAU_W32

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
    -D_CRT_NONSTDC_NO_DEPRECATE
)

set(
	TIMIDITY_FE_DEFINITIONS

# TiMidity++ common
    -DHAVE_LIBVORBIS
    -DHAVE_LIBGOGO
    -DHAVE_LIBMPG123
    -DENABLE_SFZ
	-DENABLE_DLS
	-DENABLE_ECW
    -DSNDFNT_INITFILE_NAME="soundfont.ini"

# TiMidity++ audio
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
    # -DVORBIS_DLL_INCLUDE_VORBISENC
    # -DVORBIS_DLL_INCLUDE_VORBISFILE
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

# FLAC
    -DFLAC__NO_DLL
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(-DVST_LIBRARY_NAME="timvstwrap_x64.dll")
else()
    add_definitions(-DVST_LIBRARY_NAME="timvstwrap.dll")
endif()

if("${TIM41_X86_SIMD_LEVEL}" STREQUAL "AVX512")
    add_definitions(-DUSE_AVX512)
elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "AVX2")
    add_definitions(-DUSE_AVX2)
elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "AVX")
    add_definitions(-DUSE_AVX)
elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "SSE42")
    add_definitions(-DUSE_SSE42)
elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "SSE2")
    add_definitions(-DUSE_SSE2)
endif()

if(MSVC)
    add_compile_options(/W4 /GS /fp:fast)

    if("${TIM41_SPECTRE_MITIGATION}")
        add_compile_options(/Qspectre)
    endif()

    add_compile_options(/we4013)    # '%s' undefined; assuming extern returning int
    add_compile_options(/we4028)    # formal parameter %d different from declaration
    add_compile_options(/we4133)    # 'function': incompatible types - from '%s' to '%s'

    if("${TIM41_X86_SIMD_LEVEL}" STREQUAL "AVX512")
        if("${CMAKE_C_COMPILER_ID}" MATCHES "Intel")
            if("${TIM41_INTEL_ONLY}")
                add_compile_options(/QxCORE-AVX512)
            else()
                add_compile_options(/arch:CORE-AVX512)
            endif()
        else()
            add_compile_options(/arch:AVX512)
        endif()

        if(("${CMAKE_C_COMPILER_ID}" MATCHES "IntelLLVM") OR ("${CMAKE_C_COMPILER_ID}" MATCHES "Clang"))
            add_definitions(-D__PRFCHWINTRIN_H)  # Workaround for conflicting _m_prefetchw() definitions
        endif()
    elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "AVX2")
        if("${CMAKE_C_COMPILER_ID}" MATCHES "Intel")
            if("${TIM41_INTEL_ONLY}")
                add_compile_options(/QxCORE-AVX2)
            else()
                add_compile_options(/arch:CORE-AVX2)
            endif()
        else()
            add_compile_options(/arch:AVX2)
        endif()
    elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "AVX")
        if("${CMAKE_C_COMPILER_ID}" MATCHES "Intel")
            if("${TIM41_INTEL_ONLY}")
                add_compile_options(/QxAVX)
            else()
                add_compile_options(/arch:AVX)
            endif()
        else()
            add_compile_options(/arch:AVX)
        endif()
    elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "SSE42")
        if("${CMAKE_C_COMPILER_ID}" MATCHES "Intel")
            if("${TIM41_INTEL_ONLY}")
                add_compile_options(/QxSSE4.2)
            else()
                add_compile_options(/arch:SSE4.2)
            endif()
        elseif("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
            add_compile_options(-msse4.2 -mpclmul -mpopcnt)
        endif()
    endif()

	string(APPEND CMAKE_CXX_FLAGS " /EHsc /std:c++17")
    string(APPEND CMAKE_C_FLAGS_RELEASE " /Zi /guard:cf")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Zi /guard:cf")

    if("${CMAKE_C_COMPILER_ID}" MATCHES "Intel")
        string(APPEND CMAKE_C_FLAGS_RELEASE " /Qipo")
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Qipo")
    elseif("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
        string(APPEND CMAKE_C_FLAGS_RELEASE " -flto=thin")
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " -flto=thin")
    elseif("${CMAKE_C_COMPILER_ID}" MATCHES "MSVC")
        string(APPEND CMAKE_C_FLAGS_RELEASE " /GL")
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " /GL")

        if ("${CMAKE_C_COMPILER_VERSION}" VERSION_GREATER_EQUAL "19.20")
            string(REGEX REPLACE "/Ob[0-2]" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
            string(REGEX REPLACE "/Ob[0-2]" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
            string(APPEND CMAKE_C_FLAGS_RELEASE " /Ob3")
            string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Ob3")
        endif()
    endif()

    string(APPEND CMAKE_EXE_LINKER_FLAGS " /STACK:16777216")

    if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
        string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG " /DEBUG")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_DEBUG " /DEBUG")

        string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /OPT:REF /OPT:ICF /DEBUG /GUARD:CF")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /OPT:REF /OPT:ICF /DEBUG /GUARD:CF")

        string(APPEND CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO " /OPT:REF /DEBUG")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO " /OPT:REF /DEBUG")
    else()
        string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG " /DEBUG:FASTLINK")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_DEBUG " /DEBUG:FASTLINK")

        string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /OPT:REF /OPT:ICF /LTCG:incremental /DEBUG:FULL /GUARD:CF")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /OPT:REF /OPT:ICF /LTCG:incremental /DEBUG:FULL /GUARD:CF")
        string(APPEND CMAKE_STATIC_LINKER_FLAGS_RELEASE " /LTCG:incremental")

        string(APPEND CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO " /OPT:REF /DEBUG:FASTLINK")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO " /OPT:REF /DEBUG:FASTLINK")
    endif()

    string(APPEND CMAKE_RC_FLAGS " /nologo")

    add_definitions(
        -DSTDOUT_FILENO=1
    )
else()
    add_compile_definitions(_FORTIFY_SOURCE=2)
    add_compile_options(-Wall -Wextra -Wno-missing-braces -ffast-math -fstack-protector-strong)

    if("${TIM41_GCC_WITH_LIBICONV}")
        add_compile_options(-finput-charset=cp932 -fexec-charset=cp932)
    endif()

    if("${TIM41_X86_SIMD_LEVEL}" STREQUAL "AVX512")
        add_compile_options(
            -mavx512f -mavx512cd -mavx512vl -mavx512dq -mavx512bw # -mavx512ifma -mavx512vbmi # -mavx512vpopcntdq -mavx512vnni -mavx512vbmi2 -mavx512bitalg -mvpclmulqdq -mgfni -mvaes
            -mpclmul -mpopcnt -maes -mavx2 -mfma -mfsgsbase -mf16c -mmovbe -mrdrnd -mlzcnt -mbmi -mbmi2 -mprfchw -mrdseed -madx
        )
    elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "AVX2")
        add_compile_options(-mavx2 -mpclmul -mpopcnt -maes -mavx2 -mfma -mfsgsbase -mf16c -mmovbe -mrdrnd -mlzcnt -mbmi -mbmi2)
    elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "AVX")
        add_compile_options(-mavx -mpclmul -mpopcnt -maes)
    elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "SSE42")
        add_compile_options(-msse4.2 -mpclmul -mpopcnt)
    elseif("${TIM41_X86_SIMD_LEVEL}" STREQUAL "SSE2")
        add_compile_options(-msse2)
    endif()

    string(APPEND CMAKE_CXX_FLAGS " -std=c++17")

    if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
        string(APPEND CMAKE_C_FLAGS_RELEASE " -flto=thin")
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " -flto=thin")
    else()
        string(APPEND CMAKE_C_FLAGS_RELEASE " -flto")
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " -flto")
    endif()

    string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--stack,16777216")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " -s")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " -s")
    string(PREPEND CMAKE_RC_FLAGS "-O coff ")
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
    vorbis-tools/include
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

install(FILES gpl-2.0.txt lgpl-2.0.txt lgpl-2.1.txt tim41-readme.txt DESTINATION .)
