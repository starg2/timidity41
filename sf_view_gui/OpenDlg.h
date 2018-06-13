#include <string>
#include <list>
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

class CMyFileDialog
{
	const std::string c_title;
	const std::string c_ext;
	const std::string c_filter;
	std::string m_title;
	std::string m_ext;
	char *m_filter;
	std::list<std::string> m_files;
	HWND m_hOwnerWnd;
	DWORD m_ofnFlags;

	BOOL SafeGetOpenFileName(LPOPENFILENAMEA lpofn);
	BOOL SafeGetSaveFileName(LPOPENFILENAMEA lpofn);
	typedef BOOL (CMyFileDialog::*GetOpenSaveFileName)(LPOPENFILENAMEA);
	GetOpenSaveFileName getOpenSaveFileNamePtr;

	bool runDialog(LPOPENFILENAMEA lpofn, std::list<std::string>&);

	int strlen_doublenull(const char*);

public:
	CMyFileDialog();
	~CMyFileDialog();

	// "開く" ダイアログの標準設定を読み込む
	void setOpenDlgDefaultSetting(void);

	// "名前を付けて保存" ダイアログの標準設定を読み込む
	void setSaveDlgDefaultSetting(void);

	// ダイアログのタイトル文字列を設定する
	void setTitle(const char*);

	// ダイアログの標準拡張子を設定する
	void setDefaultExt(const char*);

	// ダイアログのフィルター文字列を設定する
	void setFilter(const char*);

	//ダイアログの所有ウィンドウを設定する
	void setOwner(HWND);

	// ダイアログを展開する
	bool Execute(void);

	// バッファ内のファイルパス名を取り出す
	const char *getFile(int);

	// バッファ内のリスト総数を取り出す
	int getIndex(void);
};

