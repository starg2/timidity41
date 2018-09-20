#define _CRT_SECURE_NO_WARNINGS
#pragma warning(push)
#pragma warning(disable:4047)
#pragma warning(disable:4113)
#pragma warning(disable:4133)
#pragma warning(disable:4028)
#define main _dummy_main
#include <windows.h>
#include "sndfont.c"
#include "myini.h"
#pragma warning(pop)

void sfgui_str_free(void **p)
{
	if (!*p) return;
	free(*p);
	*p = NULL;
}

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include "resource.h"

void InsertInst(int bank, int preset, const char *str, const char *sfname);
void InsertDrum(int bank, int preset, int note, const char *str, const char *sfname);
void CreateSoundFontTree(HWND hDlg, LPCSTR x_sf_filename_)
{
	HWND hTree = GetDlgItem(hDlg, IDC_TREE1);
	HTREEITEM hSF2, hBank, hBankSub, hDrum, hDrumSub;
	TV_INSERTSTRUCT tv;
	SFInsts *sf = NULL;
	int i, x_bank, x_preset, x_keynote;
	int initial = 0;
	const char *program_name = NULL;
	char str_[1024] = "";
	int flag = 0;
	char *pname_ = NULL, *pname_b_ = NULL;

	//TreeView_DeleteAllItems(hTree);

	for (x_bank = 0; x_bank <= 127; x_bank++) {
		for (x_preset = 0; x_preset <= 127; x_preset++) {
			x_cfg_info.m_str[x_bank][x_preset] = NULL;
			x_cfg_info.d_str[x_bank][x_preset] = NULL;
		}
	}

	SetDlgItemTextA(hDlg, IDC_EDSFNAME, x_sf_filename_);

	x_comment = 1;

	strcpy(x_sf_file_name, x_sf_filename_);

	for (x_preset = 0; x_preset <= 127; x_preset++) {
		for (x_keynote = 0; x_keynote <= 127; x_keynote++) {
			x_cfg_info.d_preset[x_preset][x_keynote] = -1;
		}
	}
	for (x_bank = 0; x_bank <= 127; x_bank++) {
		for (x_preset = 0; x_preset <= 127; x_preset++) {
			x_cfg_info.m_bank[x_bank][x_preset]  = -1;
		}
	}

    tv.hInsertAfter = TVI_LAST;
    tv.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tv.hParent = TVI_ROOT;
    tv.item.pszText = MyIni_PathFindFileName(x_sf_filename_);
    tv.item.iImage = 0;
    tv.item.iSelectedImage = 1;
	hSF2 = TreeView_InsertItem(hTree, &tv);

    tv.hInsertAfter = TVI_LAST;
    tv.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tv.hParent = hSF2;
    tv.item.pszText = "Bank";
    tv.item.iImage = 0;
    tv.item.iSelectedImage = 1;
	hBank = TreeView_InsertItem(hTree, &tv);

    tv.hInsertAfter = TVI_LAST;
    tv.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tv.hParent = hSF2;
    tv.item.pszText = "Drumset";
    tv.item.iImage = 0;
    tv.item.iSelectedImage = 1;
    hDrum = TreeView_InsertItem(hTree, &tv);

	ctl->verbosity = -1;

	for (i = 0; url_module_list[i]; i++)
	    url_add_module(url_module_list[i]);
	init_freq_table();
	init_bend_fine();
	init_bend_coarse();
	initialize_resampler_coeffs();
	control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
    sf = new_soundfont(x_sf_file_name);
    sf->next = NULL;
    sf->def_order = 2;
    sfrecs = sf;
	x_cfg_info_init();
	init_sf(sf);
	if (x_sort) {
	for (x_bank = 0; x_bank <= 127; x_bank++) {
		flag = 0;

		for (x_preset = 0; x_preset <= 127; x_preset++) {
			if (x_cfg_info.m_bank[x_bank][x_preset] >= 0 && x_cfg_info.m_preset[x_bank][x_preset] >= 0) {
				flag = 1;
			}
		}
		if (!flag)
			continue;
		if (!initial) {
			initial = 1;
		}

		// insert bank
		tv.hInsertAfter = TVI_LAST;
		tv.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tv.hParent = hBank;
		sprintf(str_, "Bank %03d", x_bank);
		tv.item.pszText = str_;
		tv.item.iImage = 0;
		tv.item.iSelectedImage = 1;
		hBankSub = TreeView_InsertItem(hTree, &tv);

		for (x_preset = 0; x_preset <= 127; x_preset++) {
			if (x_cfg_info.m_bank[x_bank][x_preset] >= 0 && x_cfg_info.m_preset[x_bank][x_preset] >= 0) {

				// insert bank
				tv.hInsertAfter = TVI_LAST;
				tv.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
				tv.hParent = hBankSub;

				pname_ =sf_preset_name[x_bank][x_preset];
//				if (x_cfg_info.m_rom[x_bank][x_preset])
//					sprintf(str_, "%03d:%03d[ROM] %s\0\0", x_bank, x_preset, pname_);
//				else
				sprintf(str_, "%03d:%03d %s\0\0", x_bank, x_preset, pname_);

				tv.item.pszText = str_;
				tv.item.cchTextMax = strlen(str_);
				tv.item.iImage = 2;
				tv.item.iSelectedImage = 3;
				TreeView_InsertItem(hTree, &tv);

				InsertInst(x_bank, x_preset, pname_, x_sf_filename_);
			}

		}
	}
	for (x_preset = 0; x_preset <= 127; x_preset++) {
		flag = 0;
		for (x_keynote = 0; x_keynote <= 127; x_keynote++) {
			if (x_cfg_info.d_preset[x_preset][x_keynote] >= 0 && x_cfg_info.d_keynote[x_preset][x_keynote] >= 0) {
				flag = 1;
			}
		}
		if (!flag)
			continue;
		if (!initial) {
			initial = 1;
//			fprintf(x_out, "drumset %d\n", x_preset);
		} else {
//			fprintf(x_out, "\ndrumset %d\n", x_preset);
		}

		// insert bank
		tv.hInsertAfter = TVI_LAST;
		tv.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tv.hParent = hDrum;
		sprintf(str_, "Drumset %03d (%s)", x_preset, sf_preset_name[128][x_preset]);
		tv.item.pszText = str_;
		tv.item.iImage = 0;
		tv.item.iSelectedImage = 1;
		hDrumSub = TreeView_InsertItem(hTree, &tv);

		for (x_keynote = 0; x_keynote <= 127; x_keynote++) {
			if (x_cfg_info.d_preset[x_preset][x_keynote] >= 0 && x_cfg_info.d_keynote[x_preset][x_keynote] >= 0) {

				// insert bank
				tv.hInsertAfter = TVI_LAST;
				tv.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
				tv.hParent = hDrumSub;

				pname_b_ = strtok(x_cfg_info.d_str[x_preset][x_keynote], "#");
				if (pname_b_) {
					if ((pname_ = strstr(pname_b_, ":")) == NULL)
						pname_ = pname_b_;
					else pname_ = "";
				} else pname_ = "";

//				if (x_cfg_info.d_rom[x_preset][x_keynote])
//					sprintf(str_, "Bank%03d Note%03d[ROM] %s\0\0", x_preset, x_keynote, pname_);
//				else
				sprintf(str_, "%03d:%03d %s\0\0", x_preset, x_keynote, pname_);

				tv.item.pszText = str_;
				tv.item.cchTextMax = strlen(str_);
				tv.item.iImage = 2;
				tv.item.iSelectedImage = 3;
				TreeView_InsertItem(hTree, &tv);
				InsertDrum(128, x_preset, x_keynote, pname_, x_sf_filename_);
			}
		}
	}
	}

	for (x_bank = 0; x_bank <= 127; x_bank++) {
		for (x_preset = 0; x_preset <= 127; x_preset++) {
			sfgui_str_free(&x_cfg_info.m_str[x_bank][x_preset]);
			sfgui_str_free(&x_cfg_info.d_str[x_bank][x_preset]);
		}
	}
	end_soundfont(sf);

	TreeView_Expand(hTree, hSF2, TVE_EXPAND);
	TreeView_Expand(hTree, hBank, TVE_EXPAND);
	TreeView_Expand(hTree, hDrum, TVE_EXPAND);
	TreeView_Select(hTree, hSF2, TVGN_CARET);
}

void ResetSoundFontTree(HWND hDlg)
{
	HWND hTree = GetDlgItem(hDlg, IDC_TREE1);

	TreeView_DeleteAllItems(hTree);
}


