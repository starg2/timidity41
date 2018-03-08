
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "timidity.h"
#include "common.h"

#ifdef AU_FLAC

extern void flac_set_option_verify(int);
extern void flac_set_option_padding(int);
extern void flac_set_compression_level(int);
extern void flac_set_compression_level(int compression_level);

#ifdef AU_OGGFLAC
extern void flac_set_option_oggflac(int isogg);
#endif

#endif /* AU_FLAC */
