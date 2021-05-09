
include_directories(src src/compat src/libmpg123)

add_definitions(-DOPT_GENERIC)

add_library(
    mpg123 SHARED

    src/compat/compat.c
    src/compat/compat_str.c

    src/libmpg123/dct64.c
    src/libmpg123/dither.c
    src/libmpg123/equalizer.c
    src/libmpg123/feature.c
    src/libmpg123/format.c
    src/libmpg123/frame.c
    src/libmpg123/icy.c
    src/libmpg123/icy2utf8.c
    src/libmpg123/id3.c
    src/libmpg123/index.c
    # src/libmpg123/lfs_alias.c
    src/libmpg123/layer1.c
    src/libmpg123/layer2.c
    src/libmpg123/layer3.c
    # src/libmpg123/lfs_wrap.c
    src/libmpg123/libmpg123.c
    src/libmpg123/ntom.c
    src/libmpg123/optimize.c
    src/libmpg123/parse.c
    src/libmpg123/readers.c
    src/libmpg123/stringbuf.c
    src/libmpg123/synth.c
    src/libmpg123/synth_8bit.c
    src/libmpg123/synth_real.c
    src/libmpg123/synth_s32.c
    src/libmpg123/tabinit.c

    src/libmpg123/msvc.c

    src/config.h
    src/intsym.h
    src/compat/compat.h
    src/compat/wpathconv.h
    src/libmpg123/abi_align.h
    src/libmpg123/debug.h
    src/libmpg123/decode.h
    src/libmpg123/dither.h
    src/libmpg123/dither_impl.h
    src/libmpg123/fmt123.h
    src/libmpg123/frame.h
    src/libmpg123/gapless.h
    src/libmpg123/getbits.h
    src/libmpg123/getcpuflags.h
    src/libmpg123/huffman.h
    src/libmpg123/icy.h
    src/libmpg123/icy2utf8.h
    src/libmpg123/id3.h
    src/libmpg123/index.h
    src/libmpg123/l2tables.h
    src/libmpg123/mangle.h
    src/libmpg123/mpeghead.h
    include/mpg123.h
    src/libmpg123/mpg123lib_intern.h
    src/libmpg123/newhuffman.h
    src/libmpg123/optimize.h
    src/libmpg123/parse.h
    src/libmpg123/reader.h
    src/libmpg123/sample.h
    src/libmpg123/swap_bytes_impl.h
    src/libmpg123/synth.h
    src/libmpg123/synth_8bit.h
    src/libmpg123/synth_mono.h
    src/libmpg123/synth_ntom.h
    src/libmpg123/synths.h
    src/libmpg123/true.h
)

target_compile_definitions(mpg123 PRIVATE -DBUILD_MPG123_DLL INTERFACE -DLINK_MPG123_DLL)
target_link_libraries(mpg123 shlwapi)
