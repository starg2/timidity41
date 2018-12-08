
add_definitions(
    -Dpopen=_popen
    -Dpclose=_pclose
)

add_library(
    libarc OBJECT

    arc.c
    arc_lzh.c
    arc_mime.c
    arc_tar.c
    arc_zip.c
    deflate.c
    explode.c
    inflate.c
    unlzh.c
    url.c
    url_b64decode.c
    url_buff.c
    url_cache.c
    url_constmem.c
    url_dir.c
    url_file.c
    url_ftp.c
    url_hqxdecode.c
    url_http.c
    url_inflate.c
    url_mem.c
    url_news.c
    url_newsgroup.c
    url_pipe.c
    url_qsdecode.c
    url_uudecode.c
    
    arc.h
    explode.h
    unlzh.h
    url.h
    zip.h
)
