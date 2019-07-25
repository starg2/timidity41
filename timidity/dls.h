
#pragma once

#ifdef ENABLE_DLS

void init_dls(void);
void free_dls(void);
Instrument *extract_dls_file(char *sample_file, uint8 font_bank, int8 font_preset, int8 font_keynote);
void free_dls_file(Instrument *ip);

typedef struct {
	char *name;
	uint8 program;
} DLSProgramInfo;

typedef struct {
	DLSProgramInfo *programs;
	int program_count;
	uint8 bank;
} DLSBankInfo;

typedef struct {
	char *name;
	uint8 *notes;
	int note_count;
	uint8 program;
} DLSDrumsetInfo;

typedef struct {
	DLSBankInfo *banks;
	DLSDrumsetInfo *drumsets;
	int bank_count;
	int drumset_count;
} DLSCollectionInfo;

DLSCollectionInfo *get_dls_instrument_list(const char *sample_file);
void free_dls_instrument_list(DLSCollectionInfo *list);

#endif
