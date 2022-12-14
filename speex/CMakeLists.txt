
add_definitions(
    -DHAVE_CONFIG_H
)

add_library(
    speex SHARED

    bits.c
    cb_search.c
    exc_10_16_table.c
    exc_10_32_table.c
    exc_20_32_table.c
    exc_5_256_table.c
    exc_5_64_table.c
    exc_8_128_table.c
    filters.c
    gain_table.c
    gain_table_lbr.c
    hexc_10_32_table.c
    hexc_table.c
    high_lsp_tables.c
    lpc.c
    lsp.c
    lsp_tables_nb.c
    ltp.c
    modes.c
    modes_wb.c
    nb_celp.c
    quant_lsp.c
    sb_celp.c
    speex.c
    speex_callbacks.c
    speex_header.c
    stereo.c
    vbr.c
    vq.c
    window.c
    
    config.h
    arch.h
    cb_search.h
    cb_search_sse.h
    filters.h
    filters_sse.h
    fixed_debug.h
    fixed_generic.h
    lpc.h
    lsp.h
    ltp.h
    ltp_sse.h
    math_approx.h
    modes.h
    nb_celp.h
    os_support.h
    quant_lsp.h
    sb_celp.h
    stack_alloc.h
    vbr.h
    vq.h
    vq_sse.h
    speex.h
    speex_bits.h
    speex_callbacks.h
    speex_header.h
    speex_stereo.h
    speex_types.h

    libspeex.def
)

install(TARGETS speex)
