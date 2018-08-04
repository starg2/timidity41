
add_definitions(
    -DFLAC_API_EXPORTS
    -DFLAC__HAS_X86INTRIN
    -DFLAC__HAS_OGG

    -DPACKAGE_VERSION="1.3.2"
)

if(MINGW)
    add_definitions(
        -DHAVE_FSEEKO
        -DHAVE_LROUND
    )
endif()

include_directories(src)

add_library(
    FLAC SHARED

    include/FLAC/all.h
    include/FLAC/assert.h
    include/FLAC/callback.h
    include/FLAC/export.h
    include/FLAC/format.h
    include/FLAC/metadata.h
    include/FLAC/ordinals.h
    include/FLAC/stream_decoder.h
    include/FLAC/stream_encoder.h

    src/private/all.h
    src/private/bitmath.h
    src/private/bitreader.h
    src/private/bitwriter.h
    src/private/cpu.h
    src/private/crc.h
    src/private/fixed.h
    src/private/float.h
    src/private/format.h
    src/private/lpc.h
    src/private/macros.h
    src/private/md5.h
    src/private/memory.h
    src/private/metadata.h
    src/private/ogg_decoder_aspect.h
    src/private/ogg_encoder_aspect.h
    src/private/ogg_helper.h
    src/private/ogg_mapping.h
    src/private/stream_encoder.h
    src/private/stream_encoder_framing.h
    src/private/window.h

    src/share/alloc.h
    src/share/compat.h
    src/share/endswap.h
    src/share/getopt.h
    src/share/grabbag.h
    src/share/macros.h
    src/share/private.h
    src/share/replaygain_analysis.h
    src/share/replaygain_synthesis.h
    src/share/safe_str.h
    src/share/utf8.h
    src/share/windows_unicode_filenames.h
    src/share/win_utf8_io.h

    src/bitmath.c
    src/bitreader.c
    src/bitwriter.c
    src/cpu.c
    src/crc.c
    src/fixed.c
    src/fixed_intrin_sse2.c
    src/fixed_intrin_ssse3.c
    src/float.c
    src/format.c
    src/lpc.c
    src/lpc_intrin_avx2.c
    src/lpc_intrin_sse.c
    src/lpc_intrin_sse2.c
    src/lpc_intrin_sse41.c
    src/md5.c
    src/memory.c
    src/metadata_iterators.c
    src/metadata_object.c
    src/ogg_decoder_aspect.c
    src/ogg_encoder_aspect.c
    src/ogg_helper.c
    src/ogg_mapping.c
    src/stream_decoder.c
    src/stream_encoder.c
    src/stream_encoder_framing.c
    src/stream_encoder_intrin_avx2.c
    src/stream_encoder_intrin_sse2.c
    src/stream_encoder_intrin_ssse3.c
    src/window.c
    src/windows_unicode_filenames.c
)

target_link_libraries(FLAC ogg)
