#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "w32g_ut2.h"

char *timidity_window_inifile;

// ****************************************************************************
// DlgChooseFont
// hwnd: Owner Window of This Dialog
// hwndFontChange: Window to Change Font
// hFontPre: Previous Font of hwndFontChange (Call CloseHandle())
int DlgChooseFontAndApply(HWND hwnd, HWND hwndFontChange, HFONT hFontPre, char *fontname, int *fontheight, int *fontwidth)
{
	LOGFONT lf;
	CHOOSEFONT cf;
	HFONT hFont;
	memset(&lf,0,sizeof(LOGFONT));
	memset(&cf,0,sizeof(CHOOSEFONT));

//	lf.lfHeight = 16;
//	lf.lfWidth = 8;
	strcpy(lf.lfFaceName,"‚l‚r –¾’©");
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = hwnd;
//    cf.hDC = NULL;
    cf.lpLogFont = &lf;
//    cf.iPointSize = 16;
//    cf.Flags = CF_ANSIONLY | CF_FORCEFONTEXIST ;
    cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;;
//    cf.rgbColors = RGB(0,0,0);
//    cf.lCustData = NULL;
//    cf.lpfnHook = NULL;
//    cf.lpTemplateName = NULL;
//    cf.hInstance = 0;
//    cf.lpszStyle = NULL;
    cf.nFontType = SCREEN_FONTTYPE;
//    cf.nSizeMin = 4;
//    cf.nSizeMax = 72;
	ChooseFont(&cf);

//	if(ChooseFont(&cf)==TRUE)
//		return -1;
	if(hFontPre!=NULL)
		CloseHandle(hFontPre);
	hFont = CreateFontIndirect(&lf);
	SendMessage(hwndFontChange,WM_SETFONT,(WPARAM)hFont,(LPARAM)MAKELPARAM(TRUE,0));
	if(fontname!=NULL) strcpy(fontname,lf.lfFaceName);
	if(fontheight!=NULL) *fontheight = lf.lfHeight;
	if(fontwidth!=NULL) *fontwidth = lf.lfWidth;
	return 0;
}

int DlgChooseFont(HWND hwnd, char *fontName, int *fontHeight, int *fontWidth)
{
	LOGFONT lf;
	CHOOSEFONT cf;

	memset(&lf,0,sizeof(LOGFONT));
	if(fontHeight!=NULL) lf.lfHeight = *fontHeight;
	if(fontWidth!=NULL) lf.lfWidth = *fontWidth;
	if(fontName!=NULL) strcpy(lf.lfFaceName,fontName);

	memset(&cf,0,sizeof(CHOOSEFONT));
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = hwnd;
//    cf.hDC = NULL;
    cf.lpLogFont = &lf;
//    cf.iPointSize = 16;
//    cf.Flags = CF_ANSIONLY | CF_FORCEFONTEXIST ;
    cf.Flags = CF_ANSIONLY | CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
//    cf.rgbColors = RGB(0,0,0);
//    cf.lCustData = NULL;
//    cf.lpfnHook = NULL;
//    cf.lpTemplateName = NULL;
//    cf.hInstance = 0;
//    cf.lpszStyle = NULL;
    cf.nFontType = SCREEN_FONTTYPE;
//    cf.nSizeMin = 4;
//    cf.nSizeMax = 72;
	if(ChooseFont(&cf)!=TRUE)
		return -1;

	if(fontName!=NULL) strcpy(fontName,lf.lfFaceName);
	if(fontHeight!=NULL) *fontHeight = abs(lf.lfHeight);
	if(fontWidth!=NULL) *fontWidth = lf.lfWidth;
	return 0;
}

/**********************************************************************/
int INILoadAll(void)
{
	INILoadListWnd();
	return 0;
}
int INISaveAll(void)
{
	INISaveListWnd();
	return 0;
}


/**********************************************************************/
#define SEC_LISTWND "ListWnd"
int INISaveListWnd(void)
{
	char *section = SEC_LISTWND;
	char *inifile = TIMIDITY_WINDOW_INI_FILE;
	char buffer[256];
	sprintf(buffer,"%d",ListWndInfo.Width);
	WritePrivateProfileString(section,"Width",buffer,inifile);
	sprintf(buffer,"%d",ListWndInfo.Height);
	WritePrivateProfileString(section,"Height",buffer,inifile);
	WritePrivateProfileString(section,"fontName",ListWndInfo.fontName,inifile);
	sprintf(buffer,"%d",ListWndInfo.fontWidth);
	WritePrivateProfileString(section,"fontWidth",buffer,inifile);
	sprintf(buffer,"%d",ListWndInfo.fontHeight);
	WritePrivateProfileString(section,"fontHeight",buffer,inifile);
	sprintf(buffer,"%d",ListWndInfo.fontFlags);
	WritePrivateProfileString(section,"fontFlags",buffer,inifile);
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
	return 0;
}

int INILoadListWnd(void)
{
	char *section = SEC_LISTWND;
	char *inifile = TIMIDITY_WINDOW_INI_FILE;
	int num;
	char buffer[64];
	num = GetPrivateProfileInt(section,"Width",-1,inifile);
	if(num!=-1) ListWndInfo.Width = num;
	num = GetPrivateProfileInt(section,"Height",-1,inifile);
	if(num!=-1) ListWndInfo.Height = num;
	GetPrivateProfileString(section,"fontName","",buffer,32,inifile);
	if(buffer[0]!=0) strcpy(ListWndInfo.fontName,buffer);
	num = GetPrivateProfileInt(section,"fontWidth",-1,inifile);
	if(num!=-1) ListWndInfo.fontWidth = num;
	num = GetPrivateProfileInt(section,"fontHeight",-1,inifile);
	if(num!=-1) ListWndInfo.fontHeight = num;
	num = GetPrivateProfileInt(section,"fontFlags",-1,inifile);
	if(num!=-1) ListWndInfo.fontFlags = num;
	return 0;
}

#define SEC_DOCWND "DocWnd"
int INISaveDocWnd(void)
{
	char *section = SEC_DOCWND;
	char *inifile = TIMIDITY_WINDOW_INI_FILE;
	char buffer[256];
	sprintf(buffer,"%d",DocWndInfo.Width);
	WritePrivateProfileString(section,"Width",buffer,inifile);
	sprintf(buffer,"%d",DocWndInfo.Height);
	WritePrivateProfileString(section,"Height",buffer,inifile);
	WritePrivateProfileString(section,"fontName",DocWndInfo.fontName,inifile);
	sprintf(buffer,"%d",DocWndInfo.fontWidth);
	WritePrivateProfileString(section,"fontWidth",buffer,inifile);
	sprintf(buffer,"%d",DocWndInfo.fontHeight);
	WritePrivateProfileString(section,"fontHeight",buffer,inifile);
	sprintf(buffer,"%d",DocWndInfo.fontFlags);
	WritePrivateProfileString(section,"fontFlags",buffer,inifile);
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
	return 0;
}

int INILoadDocWnd(void)
{
	char *section = SEC_DOCWND;
	char *inifile = TIMIDITY_WINDOW_INI_FILE;
	int num;
	char buffer[64];
	num = GetPrivateProfileInt(section,"Width",-1,inifile);
	if(num!=-1) DocWndInfo.Width = num;
	num = GetPrivateProfileInt(section,"Height",-1,inifile);
	if(num!=-1) DocWndInfo.Height = num;
	GetPrivateProfileString(section,"fontName","",buffer,32,inifile);
	if(buffer[0]!=0) strcpy(DocWndInfo.fontName,buffer);
	num = GetPrivateProfileInt(section,"fontWidth",-1,inifile);
	if(num!=-1) DocWndInfo.fontWidth = num;
	num = GetPrivateProfileInt(section,"fontHeight",-1,inifile);
	if(num!=-1) DocWndInfo.fontHeight = num;
	num = GetPrivateProfileInt(section,"fontFlags",-1,inifile);
	if(num!=-1) DocWndInfo.fontFlags = num;
	return 0;
}
