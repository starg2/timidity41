
#define _WIN32_IE 0x500
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4530) // アンワインド セマンティクスが無効
#include <windows.h>
#include <windowsx.h>
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
	BOOL dls;
	unsigned char bank;
	unsigned char preset;
	std::string str;
	std::string presetName;
};
std::map< int, std::map< int, sfvSFInst > > g_sfInst;
struct sfvSFDrum
{
	BOOL dls;
	unsigned char bank;
	unsigned char preset;
	unsigned char note;
	std::string str;
	std::string presetName;
};
std::map< int, std::map< int, sfvSFDrum > > g_sfDrum;

extern "C" void InsertInst(BOOL dls, int bank, int preset, char *str, const char *sfname)
{
	std::map< int, std::map< int, sfvSFInst > >::iterator it = g_sfInst.find(bank);
	if (it != g_sfInst.end()) {

		sfvSFInst newdata;
        newdata.dls = dls;
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
		InsertInst(dls, bank, preset, str, sfname);
	}
}

extern "C" void InsertDrum(BOOL dls, int bank, int preset, int note, const char *str, const char *sfname)
{
	std::map< int, std::map< int, sfvSFDrum > >::iterator it = g_sfDrum.find(preset);
	if (it != g_sfDrum.end()) {

		sfvSFDrum newdata;
        newdata.dls = dls;
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
		InsertDrum(dls, bank, preset, note, str, sfname);
	}
}

void SFView_ExportConfigFile(char *outFileName, int outListEnable, int outComment, int outSpace, int keepFullPath, int prependBaseDir)
{
	FILE *fp = fopen(outFileName, "w");
	if (!outListEnable && prependBaseDir)
		fprintf(fp, "\ndir \"${basedir}\"\n");
	for (std::map< int, std::map< int, sfvSFInst > >::iterator it =  g_sfInst.begin(); it != g_sfInst.end(); ++it) {
		if (!outListEnable)
			fprintf(fp, "\n");
		fprintf(fp, "bank %d\n", (*it).first);
		for (std::map< int, sfvSFInst >::iterator itc = (*it).second.begin(); itc != (*it).second.end(); ++itc) {
			BOOL dls = (*itc).second.dls;
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
					fprintf(fp, "%d %s \"%s\" %d %d", program, (dls ? "%dls" : "%font"), file, bank, preset);
				else
					fprintf(fp, "%d %s %s %d %d", program, (dls ? "%dls" : "%font"), file, bank, preset);
				if (outComment && comment && strlen(comment))
					fprintf(fp, " # %s", comment);
				fprintf(fp, "\n");
			}
		}
	}

	for (std::map< int, std::map< int, sfvSFDrum > >::iterator it =  g_sfDrum.begin(); it != g_sfDrum.end(); ++it) {
		if (!outListEnable)
			fprintf(fp, "\n");
		fprintf(fp, "drumset %d\n", (*it).first);
		for (std::map< int, sfvSFDrum >::iterator itc = (*it).second.begin(); itc != (*it).second.end(); ++itc) {
			BOOL dls = (*itc).second.dls;
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
					fprintf(fp, "%d %s \"%s\" %d %d %d", program, (dls ? "%dls" : "%font"), file, bank, preset, note);
				else
					fprintf(fp, "%d %s %s %d %d %d", program, (dls ? "%dls" : "%font"), file, bank, preset, note);
				if (outComment && comment && strlen(comment))
					fprintf(fp, " # %s", comment);
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
	CONST UINT menuIDs[] = { IDM_OPT_APPEND_COMMENT, IDM_OPT_APPEND_FSPACES, IDM_OPT_KEEP_FULLPATH, IDM_OPT_PREPEND_BASEDIR};
	BOOL states[] = { FALSE, FALSE, FALSE, FALSE };
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
            (int)states[2],
            (int)states[3]
        );
    }
}

HIMAGELIST g_hil = NULL;
HINSTANCE g_hInst = NULL;

LRESULT DlgMainProc_INITDIALOG(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
	HMENU hMenu = GetMenu(hDlg);
	CONST UINT menuIDs[] = { IDM_OPT_APPEND_COMMENT, IDM_OPT_APPEND_FSPACES, IDM_OPT_KEEP_FULLPATH, IDM_OPT_PREPEND_BASEDIR};
	CONST UINT states[] = { MFS_CHECKED, MFS_UNCHECKED, MFS_UNCHECKED, MFS_CHECKED };

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
		fd.setFilter(
#ifdef ENABLE_DLS
            "Supported files (*.dls;*.sf2;*.sf3)\0*.dls;*.sf2;*.sf3\0"
#else
            "Supported files (*.sf2;*.sf3)\0*.sf2;*.sf3\0"
#endif
            "Soundfont (*.sf2;*.sf3)\0*.sf2;*.sf3\0"
#ifdef ENABLE_DLS
            "DLS (*.dls)\0*.dls\0"
#endif
            "All files (*.*)\0*.*\0\0"
        );
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
    case IDM_OPT_PREPEND_BASEDIR:
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

LRESULT DlgMainProc_SIZE(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
    if (wParam != SC_MINIMIZE) {
        SetWindowPos(
            GetDlgItem(hDlg, IDC_TREE1),
            NULL,
            0,
            0,
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam) - 17,
            SWP_NOACTIVATE | SWP_NOZORDER
        );

        SetWindowPos(
            GetDlgItem(hDlg, IDC_EDSFLABEL),
            NULL,
            0,
            GET_Y_LPARAM(lParam) - 16,
            99,
            16,
            SWP_NOACTIVATE | SWP_NOZORDER
        );

        SetWindowPos(
            GetDlgItem(hDlg, IDC_EDSFNAME),
            NULL,
            100,
            GET_Y_LPARAM(lParam) - 16,
            GET_X_LPARAM(lParam) - 100,
            16,
            SWP_NOACTIVATE | SWP_NOZORDER
        );
    }

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
		SET_MYWINMSG(SIZE);
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


