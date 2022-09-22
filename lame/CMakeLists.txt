
add_definitions(
    -DHAVE_CONFIG_H
    -DLAME_LIBRARY_BUILD
    -DHAVE_XMMINTRIN_H
    -DSTDC_HEADERS
    -DHAVE_ERRNO_H
    -DHAVE_FCNTL_H
    -DHAVE_LIMITS_H
    -DPACKAGE="lame"
    -DPROTOTYPES
    -DUSE_FAST_LOG
    -DHAVE_STRCHR
    -DHAVE_MEMCPY
)

include_directories(.)

add_library(
    lame_enc SHARED

    config.h

    Dll/BladeMP3EncDLL.c
    Dll/BladeMP3EncDLL.h

    include/lame.h

    libmp3lame/bitstream.c
    libmp3lame/bitstream.h
    libmp3lame/encoder.c
    libmp3lame/encoder.h
    libmp3lame/fft.c
    libmp3lame/fft.h
    libmp3lame/gain_analysis.c
    libmp3lame/gain_analysis.h
    libmp3lame/id3tag.c
    libmp3lame/id3tag.h
    libmp3lame/l3side.h
    libmp3lame/lame-analysis.h
    libmp3lame/lame.c
    libmp3lame/lameerror.h
    libmp3lame/lame_global_flags.h
    libmp3lame/machine.h
    libmp3lame/mpglib_interface.c
    libmp3lame/newmdct.c
    libmp3lame/newmdct.h
    libmp3lame/presets.c
    libmp3lame/psymodel.c
    libmp3lame/psymodel.h
    libmp3lame/quantize.c
    libmp3lame/quantize.h
    libmp3lame/quantize_pvt.c
    libmp3lame/quantize_pvt.h
    libmp3lame/reservoir.c
    libmp3lame/reservoir.h
    libmp3lame/set_get.c
    libmp3lame/set_get.h
    libmp3lame/tables.c
    libmp3lame/tables.h
    libmp3lame/takehiro.c
    libmp3lame/util.c
    libmp3lame/util.h
    libmp3lame/vbrquantize.c
    libmp3lame/vbrquantize.h
    libmp3lame/VbrTag.c
    libmp3lame/VbrTag.h
    libmp3lame/version.c
    libmp3lame/version.h
    libmp3lame/vector/lame_intrin.h
    libmp3lame/vector/xmm_quantize_sub.c

    Dll/BladeMP3EncDLL.def
    libmp3lame/lame.rc
)

set_target_properties(lame_enc PROPERTIES PREFIX "")

install(TARGETS lame_enc)
