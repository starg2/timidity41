#include "OpenDlg.h"
#include <cstring>

CMyFileDialog::CMyFileDialog()
	: c_title(""), c_ext(""),
	  c_filter("(*.*)\0*.*\0\0"),
	  m_filter(0),
	  m_hOwnerWnd(0),
	  getOpenSaveFileNamePtr(&CMyFileDialog::SafeGetOpenFileName)
{
	setTitle(0);
	setDefaultExt(0);
	setFilter(0);
	m_files.clear();
}

CMyFileDialog::~CMyFileDialog()
{
	delete [] m_filter;
}

BOOL CMyFileDialog::SafeGetOpenFileName(LPOPENFILENAMEA lpofn)
{
	BOOL result;
	char currentdir[MAX_PATH];

	GetCurrentDirectoryA(MAX_PATH, currentdir);
	result = (GetOpenFileNameA(lpofn) != FALSE);
	SetCurrentDirectoryA(currentdir);

	return result;
}

BOOL CMyFileDialog::SafeGetSaveFileName(LPOPENFILENAMEA lpofn)
{
	BOOL result;
	char currentdir[MAX_PATH];

	GetCurrentDirectoryA(MAX_PATH, currentdir);
	result = (GetSaveFileNameA(lpofn) != FALSE);
	SetCurrentDirectoryA(currentdir);

	return result;
}

bool CMyFileDialog::runDialog(LPOPENFILENAMEA lpofn, std::list<std::string>& files)
{
	bool result;

	files.clear();
	result = ((this->*getOpenSaveFileNamePtr)(lpofn) != FALSE);

	if (result && lpofn->nFileOffset > 0 && !lpofn->lpstrFile[lpofn->nFileOffset - 1]) {
		const char *pstrFile = lpofn->lpstrFile;
		std::string dir = pstrFile;
		dir += "\\";
		pstrFile += strlen(pstrFile) + 1;

		while (*pstrFile) {
			std::string path = dir;
			path += pstrFile;

			m_files.insert(m_files.begin(), path);
			pstrFile += strlen(pstrFile) + 1;
		}
	}
	else if (result) {
		std::string path = lpofn->lpstrFile;

		m_files.insert(m_files.begin(), path);
	}

	return result;
}

int CMyFileDialog::strlen_doublenull(const char *src)
{
	int result;
	int i = 0;

	while (src[i] || src[i + 1]) {
		++i;
	}

	result = i + 2;

	return result;
}

void CMyFileDialog::setOpenDlgDefaultSetting(void)
{
	getOpenSaveFileNamePtr = &CMyFileDialog::SafeGetOpenFileName;

	m_ofnFlags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER
		| OFN_READONLY | OFN_ALLOWMULTISELECT;
}

void CMyFileDialog::setSaveDlgDefaultSetting(void)
{
	getOpenSaveFileNamePtr = &CMyFileDialog::SafeGetSaveFileName;

	m_ofnFlags = OFN_PATHMUSTEXIST | OFN_EXPLORER
		| OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_CREATEPROMPT
		| OFN_NOTESTFILECREATE;
}

void CMyFileDialog::setTitle(const char *title)
{
	if (!title) {
		title = c_title.c_str();
	}

	m_title = title;
}

void CMyFileDialog::setDefaultExt(const char *ext)
{
	if (!ext) {
		ext = c_ext.c_str();
	}

	m_ext = ext;
}

void CMyFileDialog::setFilter(const char *filter)
{
	int length;

	if (!filter) {
		filter = c_filter.c_str();
	}

	length = strlen_doublenull(filter);

	delete [] m_filter;
	m_filter = new char[length + 1]();
	memcpy(m_filter, filter, length);
}

//ダイアログの所有ウィンドウを設定する
void CMyFileDialog::setOwner(HWND hWnd)
{
	m_hOwnerWnd = hWnd;
}

bool CMyFileDialog::Execute(void)
{
	bool result;
	OPENFILENAMEA ofn;
	char *filename;
	const int max_filename_size = 32768;

	filename = new char[max_filename_size]();

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = m_hOwnerWnd;
	ofn.hInstance = GetModuleHandle(NULL);
	ofn.lpstrFilter = m_filter;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = max_filename_size;
	ofn.lpstrInitialDir = 0;
	ofn.lpstrTitle	= m_title.c_str();
	ofn.Flags = m_ofnFlags;
	ofn.lpstrDefExt = m_ext.c_str();

	result = runDialog(&ofn, m_files);

	delete [] filename;

	return result;
}

const char *CMyFileDialog::getFile(int index)
{
	const int num = m_files.size();
	const char *result = 0;

	if (index < num) {
		std::list<std::string>::const_iterator it;

		it = m_files.begin();
		while (index-- > 0) {
			++it;
		}

		result = (*it).c_str();
	}

	return result;
}

int CMyFileDialog::getIndex(void)
{
	return m_files.size();
}


