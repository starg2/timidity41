#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "w32g_dib.h"

extern HWND hMainWnd;

w32g_dib_t *dib_create (int width, int height )
{
        BITMAPINFO bi;
        w32g_dib_t *dib;

        dib = (w32g_dib_t*) malloc ( sizeof (w32g_dib_t) );
        if ( dib == NULL ) return NULL;
        ZeroMemory ( dib,  sizeof (w32g_dib_t) );

        ZeroMemory ( &bi,  sizeof (BITMAPINFO) );
        bi.bmiHeader.biSize          = sizeof (BITMAPINFOHEADER);
        bi.bmiHeader.biWidth         = width;
        bi.bmiHeader.biHeight        = -height;
        bi.bmiHeader.biPlanes        = 1;
        bi.bmiHeader.biBitCount      = 8;
        bi.bmiHeader.biCompression   = BI_RGB;
        bi.bmiHeader.biSizeImage     = 0;
        bi.bmiHeader.biXPelsPerMeter = 0;
        bi.bmiHeader.biYPelsPerMeter = 0;
        bi.bmiHeader.biClrImportant  = 0;
        bi.bmiHeader.biClrUsed       = 0;
        dib->hbmp = CreateDIBSection ( NULL, &bi, DIB_RGB_COLORS, (void**) &dib->bits, NULL, 0);
        dib->width = width;
        dib->height = height;
        dib->pals_max = 16;
        dib->modified_rect_max = 0;
        if ( dib->bits == NULL ) { /* retry */
                HDC hDesktopDC;
                DeleteObject ( dib->hbmp );
#ifndef NDEBUG
                MessageBox(hMainWnd,
                        TEXT("Sorry. Couldn't execute CreateDIBSection func."), TEXT("First Error"), MB_ICONEXCLAMATION);
#endif
                hDesktopDC = GetDC(NULL);
                dib->hbmp = CreateDIBSection ( hDesktopDC, &bi, DIB_RGB_COLORS, (void**) &dib->bits, NULL, 0);
                ReleaseDC(NULL, hDesktopDC);
                if ( dib->bits == NULL ) { /* fail */
                        DeleteObject ( dib->hbmp );
                        MessageBox(hMainWnd,
                                TEXT("Sorry. Couldn't execute CreateDIBSection func."), NULL, MB_ICONEXCLAMATION);
                        free(dib);
                        dib = NULL;
                }
        }

        return dib;
}

void dib_free ( w32g_dib_t *dib )
{
        if ( dib == NULL )
                return;
        GdiFlush ();
        DeleteObject ( dib->hbmp );
        free ( dib );
}

void dib_add_modified_rect ( w32g_dib_t *dib, RECT *lprc )
{
        RECT rc;
        int i;
        for ( i = 0; i < dib->modified_rect_max; i++ ) {
                if ( dib->modified_rect[i].left <= lprc->left && lprc->right <= dib->modified_rect[i].right
                        && dib->modified_rect[i].top <= lprc->top && lprc->bottom <= dib->modified_rect[i].bottom )
                                return;
        }
        if ( dib->modified_rect_max >= W32G_DIB_MODIFIED_RECT_MAX ) {
                if ( UnionRect ( &rc, &dib->modified_rect[dib->modified_rect_max - 1], lprc ) )
                        CopyMemory ( &dib->modified_rect[dib->modified_rect_max - 1], &rc, sizeof (RECT) );
        } else {
                        CopyMemory ( &dib->modified_rect[dib->modified_rect_max], lprc, sizeof (RECT) );
                        dib->modified_rect_max++;
        }
}

void dib_modified_rect_whole ( w32g_dib_t *dib )
{
        dib->modified_rect_max = 1;
        SetRect ( &dib->modified_rect[0], 0, 0, dib->width, dib->height );
}

void dib_set_pal1 ( w32g_dib_t *dib, int pal_index, RGBQUAD rq )
{
        HDC hmdc;

        if ( pal_index >= dib->pals_max )
                return;
        if ( memcmp ( &dib->pals[pal_index], &rq, sizeof (RGBQUAD) ) == 0 )
                return;
        CopyMemory ( &dib->pals[pal_index], &rq, sizeof (RGBQUAD) );

        GdiFlush ();
        hmdc = CreateCompatibleDC ( (HDC) NULL );
        SelectObject ( hmdc, dib->hbmp );
        SetDIBColorTable ( hmdc, pal_index, 1, &dib->pals[pal_index] );
        DeleteDC ( hmdc );

        dib_modified_rect_whole ( dib );
}

void dib_set_pals ( w32g_dib_t *dib, RGBQUAD *pals, int pals_max )
{
        int i;
        BOOL modified = FALSE;

        if ( pals_max <= 0 ) return;
        if ( pals_max >= dib->pals_max )
                pals_max = dib->pals_max;
        for ( i = 0; i < pals_max; i ++ ) {
                if ( memcmp ( &dib->pals[i], &pals[i], sizeof (RGBQUAD) ) != 0 ) {
                        CopyMemory ( &dib->pals[i], &pals[i], sizeof (RGBQUAD) );
                        modified = TRUE;
                }
        }
        if ( modified ) {
                HDC hmdc;
                GdiFlush ();
                hmdc = CreateCompatibleDC ( (HDC) NULL );
                SelectObject ( hmdc, dib->hbmp );
                SetDIBColorTable ( hmdc, 0, dib->pals_max, dib->pals );
                DeleteDC ( hmdc );
                dib_modified_rect_whole ( dib );
        }
}

void dib_apply ( w32g_dib_t *dib, HDC hdc )
{
        HDC hmdc;
        int i;
        if ( dib->modified_rect_max <= 0 ) return;
        GdiFlush ();
        hmdc = CreateCompatibleDC ( hdc );
        SelectObject ( hmdc, dib->hbmp );
        for ( i = 0; i < dib->modified_rect_max; i++ ) {
                RECT *lprc = &dib->modified_rect[i];
                BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
                        hmdc, lprc->left, lprc->top, SRCCOPY );
        }
        dib->modified_rect_max = 0;
        DeleteDC ( hmdc );
}

