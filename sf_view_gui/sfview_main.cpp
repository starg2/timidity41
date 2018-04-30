#define _WIN32_IE 0x500
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4530) // アンワインド セマンティクスが無効
#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comctl32.lib")
#include "OpenDlg.h"
extern "C" {
#include "myini.h"
}

#include <map>
#include <string>
struct sfvSFInst
{
	unsigned char bank;
	unsigned char preset;
	std::string str;
	std::string presetName;
};
std::map< int, std::map< int, sfvSFInst > > g_sfInst;
struct sfvSFDrum
{
	unsigned char bank;
	unsigned char preset;
	unsigned char note;
	std::string str;
	std::string presetName;
};
std::map< int, std::map< int, sfvSFDrum > > g_sfDrum;

extern "C" void InsertInst(int bank, int preset, char *str, const char *sfname)
{
	std::map< int, std::map< int, sfvSFInst > >::iterator it = g_sfInst.find(bank);
	if (it != g_sfInst.end()) {

		sfvSFInst newdata;
		newdata.bank		= bank;
		newdata.preset		= preset;
		newdata.str		= sfname;
		newdata.presetName	= str;

		std::map< int, sfvSFInst >::iterator itc = (*it).second.find(preset);
		if (itc != (*it).second.end()) {
			(*itc).second = newdata;
		} else {
			(*it).second.insert(std::make_pair(preset, newdata));
		}
	} else {
		g_sfInst.insert(std::make_pair(bank, std::map< int, sfvSFInst >()));
		InsertInst(bank, preset, str, sfname);
	}
}

extern "C" void InsertDrum(int bank, int preset, int note, const char *str, const char *sfname)
{
	std::map< int, std::map< int, sfvSFDrum > >::iterator it = g_sfDrum.find(preset);
	if (it != g_sfDrum.end()) {

		sfvSFDrum newdata;
		newdata.bank		= bank;
		newdata.preset		= preset;
		newdata.note		= note;
		newdata.str		= sfname;
		newdata.presetName	= str;

		std::map< int, sfvSFDrum >::iterator itc = (*it).second.find(note);
		if (itc != (*it).second.end()) {
			(*itc).second = newdata;
		} else {
			(*it).second.insert(std::make_pair(note, newdata));
		}
	} else {
		g_sfDrum.insert(std::make_pair(preset, std::map< int, sfvSFDrum >()));
		InsertDrum(bank, preset, note, str, sfname);
	}
}

void SFView_ExportConfigFile(char *outFileName, int outListEnable, int outComment, int outSpace, int keepFullPath)
{
	FILE *fp = fopen(outFileName, "w");
	for (std::map< int, std::map< int, sfvSFInst > >::iterator it =  g_sfInst.begin(); it != g_sfInst.end(); ++it) {
		if (outListEnable)
			fprintf(fp, "bank %d\n", (*it).first);
		else
			fprintf(fp, "bank %d\n", (*it).first);
		for (std::map< int, sfvSFInst >::iterator itc = (*it).second.begin(); itc != (*it).second.end(); ++itc) {
			const char *file = (*itc).second.str.c_str();
			const int program = (*itc).first;
			const int bank = (*itc).second.bank;
			const int preset = (*itc).second.preset;
			const char *comment = (*itc).second.presetName.c_str();
			if (!keepFullPath)
				file = MyIni_PathFindFileName(file);
			if (outSpace)
				fprintf(fp, "        ");
			if (outListEnable)
				fprintf(fp, "%03d:%03d %s (%s)\n", bank, preset, comment, file);
			else {
				if (strstr(file, " "))
					fprintf(fp, "%d %%font \"%s\" %d %d", program, file, bank, preset);
				else
					fprintf(fp, "%d %%font %s %d %d", program, file, bank, preset);
				if (outComment)
					fprintf(fp, " # %s ", comment);
				fprintf(fp, "\n");
			}
		}
	}

	for (std::map< int, std::map< int, sfvSFDrum > >::iterator it =  g_sfDrum.begin(); it != g_sfDrum.end(); ++it) {
		if (outListEnable)
			fprintf(fp, "drumset %d\n", (*it).first);
		else
			fprintf(fp, "drumset %d\n", (*it).first);
		for (std::map< int, sfvSFDrum >::iterator itc = (*it).second.begin(); itc != (*it).second.end(); ++itc) {
			const char *file = (*itc).second.str.c_str();
			const int program = (*itc).first;
			const int bank = (*itc).second.bank;
			const int preset = (*itc).second.preset;
			const int note = (*itc).second.note;
			const char *comment = (*itc).second.presetName.c_str();
			if (!keepFullPath)
				file = MyIni_PathFindFileName(file);
			if (outSpace)
				fprintf(fp, "        ");
			if (outListEnable)
				fprintf(fp, "%03d:%03d %s (%s)\n", preset, note, comment, file);
			else {
				if (strstr(file, " "))
					fprintf(fp, "%d %%font \"%s\" %d %d %d", program, file, bank, preset, note);
				else
					fprintf(fp, "%d %%font %s %d %d %d", program, file, bank, preset, note);
				if (outComment)
					fprintf(fp, " # %s ", comment);
				fprintf(fp, "\n");
			}
		}
	}
	fclose(fp);
}

extern "C" {
void CreateSoundFontTree(HWND hDlg, LPCSTR x_sf_filename_);
void ResetSoundFontTree(HWND hDlg);
}
void ExportFile(HWND hDlg, bool bExportList)
{
	MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
	HMENU hMenu = GetMenu(hDlg);
	CONST UINT menuIDs[] = { IDM_OPT_APPEND_COMMENT, IDM_OPT_APPEND_FSPACES, IDM_OPT_KEEP_FULLPATH };
	BOOL states[] = { FALSE, FALSE, FALSE };
	CMyFileDialog fd;
	fd.setSaveDlgDefaultSetting();
	fd.setTitle("Export filename ..");
	fd.setOwner(hDlg);

	for (int i = 0; i < sizeof(menuIDs) / sizeof(menuIDs[0]); ++i) {
		mii.fMask = MIIM_STATE | MIIM_ID;
		GetMenuItemInfo(hMenu, menuIDs[i], FALSE, &mii);
		states[i] = (mii.fState & MF_CHECKED) ? TRUE : FALSE;
	}

	if (bExportList) {
		fd.setDefaultExt("txt");
		fd.setFilter("Soundfont Preset List (*.txt)\0*.txt\0\0");
	} else {
		fd.setDefaultExt("cfg");
		fd.setFilter("TiMidity++ Config File (*.cfg)\0*.cfg\0\0");
	}
	if (fd.Execute()) {
		SFView_ExportConfigFile((char*)fd.getFile(0),
					(int)bExportList,
					(int)states[0],
					(int)states[1],
					(int)states[2]);
	}
}

HIMAGELIST g_hil = NULL;
HINSTANCE g_hInst = NULL;

LRESULT DlgMainProc_INITDIALOG(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
	HMENU hMenu = GetMenu(hDlg);
	CONST UINT menuIDs[] = { IDM_OPT_APPEND_COMMENT, IDM_OPT_APPEND_FSPACES, IDM_OPT_KEEP_FULLPATH };
	CONST UINT states[] = { MFS_UNCHECKED, MFS_UNCHECKED, MFS_CHECKED };

	if (__argc == 2) {
		ResetSoundFontTree(hDlg);
		CreateSoundFontTree(hDlg, __argv[1]);
	}

	SendMessage(hDlg, WM_SETICON, 0, (LPARAM)LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1)));

	for (int i = 0; i < sizeof(menuIDs) / sizeof(menuIDs[0]); ++i) {
		mii.fMask = MIIM_STATE | MIIM_ID;
		GetMenuItemInfo(hMenu, menuIDs[i], FALSE, &mii);
		mii.fState = states[i];
		SetMenuItemInfo(hMenu, menuIDs[i], FALSE, &mii);
	}

	//g_hil = ImageList_LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_TREEICON), 16, 0, RGB(255, 255, 255));
	g_hil = ImageList_LoadImage(g_hInst, MAKEINTRESOURCE(IDB_TREEICON), 16, 0, RGB(255, 255, 255), IMAGE_BITMAP, LR_CREATEDIBSECTION);
	TreeView_SetImageList(GetDlgItem(hDlg, IDC_TREE1), g_hil, TVSIL_NORMAL);

	return TRUE;
}

LRESULT DlgMainProc_DROPFILES(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	HDROP hDrop = (HDROP)wParam;
	int n = DragQueryFile((HDROP)wParam, 0xFFFFFFFF, NULL, 0);
	const int max_filename_size = 32768;
	char *FileName;

	FileName = new char[max_filename_size]();

	g_sfInst.clear();
	g_sfDrum.clear();
	ResetSoundFontTree(hDlg);

	for (int i = 0; i < n; i++) {
		DragQueryFile(hDrop, i, FileName, max_filename_size - 1);

		CreateSoundFontTree(hDlg, FileName);
	}

	delete [] FileName;
	DragFinish(hDrop);
	return 0;
}

LRESULT DlgMainProc_COMMAND(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
	HMENU hMenu;

	switch (LOWORD(wParam)) {
	case IDM_OPENSF2:
	{
		CMyFileDialog fd;
		fd.setOpenDlgDefaultSetting();
		fd.setTitle("open soundfont");
		fd.setFilter("soundfont (*.sf2;*.sf3)\0*.sf2;*.sf3\0All files (*.*)\0*.*\0\0");
		fd.setOwner(hDlg);
		if (fd.Execute()) {
			const int n = fd.getIndex();
			g_sfInst.clear();
			g_sfDrum.clear();
			ResetSoundFontTree(hDlg);
			for (int i = 0; i < n; i++) {
				CreateSoundFontTree(hDlg, (const char*)fd.getFile(i));
			}
		}
		break;
	}

	case IDM_QUIT:
		SendMessage(hDlg, WM_CLOSE, 0, 0);
		break;

	case IDM_SAVE_CFG:
		ExportFile(hDlg, false);
		break;

	case IDM_SAVE_PRESETLIST:
		ExportFile(hDlg, true);
		break;

	case IDM_OPT_APPEND_COMMENT:
	case IDM_OPT_APPEND_FSPACES:
	case IDM_OPT_KEEP_FULLPATH:
		hMenu = GetMenu(hDlg);
		mii.fMask = MIIM_STATE | MIIM_ID;
		GetMenuItemInfo(hMenu, LOWORD(wParam), FALSE, &mii);
		mii.fState ^= MF_CHECKED;
		SetMenuItemInfo(hMenu, LOWORD(wParam), FALSE, &mii);
		break;

	default:
		break;
	}

	return FALSE;
}

LRESULT DlgMainProc_CLOSE(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	ImageList_Destroy(g_hil);
	EndDialog(hDlg, TRUE);
	return TRUE;
}

LRESULT CALLBACK DlgMainProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
#define SET_MYWINMSG(VN) case WM_ ## VN: return DlgMainProc_## VN(hDlg, wParam, lParam);
	switch (msg) {
		SET_MYWINMSG(INITDIALOG);
		SET_MYWINMSG(DROPFILES);
		SET_MYWINMSG(COMMAND);
		SET_MYWINMSG(CLOSE);
	}
#undef  SET_MYWINMSG
	return FALSE;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int ShowCmd)
{
	g_hInst = hInstance;
	DialogBoxA(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)DlgMainProc);
	return 0;
}


