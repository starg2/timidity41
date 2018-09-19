
#pragma once

#ifdef ENABLE_DLS

void init_dls(void);
void free_dls(void);
Instrument *extract_dls_file(char *sample_file, uint8 font_bank, int8 font_preset, int8 font_keynote);
void free_dls_file(Instrument *ip);

#endif
