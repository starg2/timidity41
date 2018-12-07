#include <windows.h>
#include <stdio.h>

#pragma comment(linker, "/opt:nowin98")

void __fastcall kbExtractFilePath(char *szPath)
{
    char *p = szPath;
    char *last_path_delimiter = NULL;
    while (*p) {
        if (IsDBCSLeadByte((BYTE) *p)) {
            p+=2;
        }
        else if (*p == '\\') {
            last_path_delimiter = p;
            p++;
        }
        else {
            p++;
        }
    }
    if (last_path_delimiter) {
        last_path_delimiter[1] = 0;
    }
}
void __fastcall kbReplaceFileExt(char *szPath, char *szExt)
{//szExt は先頭の . を含む
    char *p = szPath;
    char *last_period = NULL;
    while (*p) {
        if (IsDBCSLeadByte((BYTE) *p)) {
            p+=2;
        }
        else if (*p == '.') {
            last_period = p;
            p++;
        }
        else {
            p++;
        }
    }
    if (last_period) {
        strcpy(last_period, szExt);
    }
}

int main(int argc, char *argv[])
{
    WIN32_FIND_DATA fd;
    HANDLE hFind;
    int count = 0;
    char szKbTim[MAX_PATH*2]; //[kbtim.kpi があるフォルダ]\kbtim.kpi
    char szSearch[MAX_PATH*2];//[kbtim.kpi があるフォルダ]\*.ini
    char szPath[MAX_PATH*2];  //[kbtim.kpi があるフォルダ]\
    GetModuleFileName(NULL, szKbTim, sizeof(szKbTim));
    kbExtractFilePath(szKbTim);
    strcpy(szPath, szKbTim);
    strcpy(szSearch, szKbTim);
    strcat(szSearch, "*.ini");   //[kbtim.kpi があるフォルダ]\*.ini
    strcat(szKbTim, "kbtim.kpi");//[kbtim.kpi があるフォルダ]\kbtim.kpi
    BOOL bAllYes = FALSE;
    if (argc > 1) {
        if (strcmpi(argv[1], "/i") == 0) {
            bAllYes = TRUE;
        }
    }
    hFind = FindFirstFile(szKbTim, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        if (!bAllYes) printf("\"kbtim.kpi\" が見つかりません\n");
        goto End;
    }
    FindClose(hFind);
    hFind = FindFirstFile(szSearch, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do{
            if (strcmp(fd.cFileName, ".") == 0 ||
               strcmp(fd.cFileName, "..") == 0 ||
               (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            }
            else {
                char szKpi[MAX_PATH*2+32];
                kbReplaceFileExt(fd.cFileName, ".kpi");
                strcpy(szKpi, szPath);
                strcat(szKpi, fd.cFileName);
                if (strcmpi(szKpi, szKbTim) == 0) {
                    continue;
                }
                BOOL bSuccess = ::CopyFile(szKbTim, szKpi, FALSE);
                if (!bAllYes && bSuccess) {
                    printf("Created \"%s\"\n", szKpi);
                }
                else if (!bSuccess) {
                    fprintf(stderr, "Error! failed to create \"%s\"\n", szKpi);
                }
            }
        }while (FindNextFile(hFind, &fd));
        FindClose(hFind);
    }
End:
    if (!bAllYes) {
        char sz[8192];
        printf("Enter を押すと終了します");
        gets(sz);
    }
    return 0;
}
