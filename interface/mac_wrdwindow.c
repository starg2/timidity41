/* 
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	Macintosh interface for TiMidity
	by T.Nogami	<t-nogami@happy.email.ne.jp>
		
    mac_wrdwindow.c
    Macintosh graphics driver for WRD
*/

#include <string.h>

#define ENTITY 1
#include "mac_wrdwindow.h"

void dev_change_1_palette(int code, RGBColor color)
{
	int index=GCODE2INDEX(code);
	(**(**dispWorld->portPixMap).pmTable).ctTable[index].rgb=color;	
	(**(**dispWorld->portPixMap).pmTable).ctSeed++;
	//CTabChanged( (**dispWorld->portPixMap).pmTable );
	dev_palette[0][code]=color;
	//if( wrd_palette ){
	//	SetEntryUsage(wrd_palette, code, pmAnimated, 0x0000);
	//	//SetEntryColor(wrd_palette, code, &color);
	//	AnimateEntry(wrd_palette, code, &color);
	//	SetEntryUsage(wrd_palette, code, pmTolerant, 0x0000);		
	//}
	pallette_exist |= color.red;
	pallette_exist |= color.green;
	pallette_exist |= color.blue;
}

void dev_change_palette(RGBColor pal[16])
{					// don't update graphics
	int i;
	//RGBColor color;
	pallette_exist=0;
	
	for( i=0; i<16; i++ ){
		pallette_exist |= pal[i].red;
		pallette_exist |= pal[i].green;
		pallette_exist |= pal[i].blue;
		dev_change_1_palette( i, pal[i]);
	}
	//dev_remake_disp(portRect);
	//dev_redisp(portRect);
}

void dev_init_text_color()
{
	int code;
	for( code=0; code<=7; code++){
		//(**(**graphicWorld[0]->portPixMap).pmTable).ctTable[TCODE2INDEX(code)].rgb=textcolor[code];
		//(**(**graphicWorld[1]->portPixMap).pmTable).ctTable[TCODE2INDEX(code)].rgb=textcolor[code];
		(**(**dispWorld->portPixMap).pmTable).ctTable[TCOLOR_INDEX_SHIFT+code].rgb=textcolor[code];
	
		//(**(**graphicWorld[0]->portPixMap).pmTable).ctSeed++;
		//(**(**graphicWorld[1]->portPixMap).pmTable).ctSeed++;
		(**(**dispWorld->portPixMap).pmTable).ctSeed++;
	}
}

static void BlockMoveData_transparent(const void*	srcPtr,	 void *	destPtr,
								 Size 	byteCount)
{
	int i;
	static char	index[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
	
	if( srcPtr>destPtr ){
		for( i=0; i<byteCount; i++){
			//if( ((char*)srcPtr)[i]==GCODE2INDEX(0) ) ((char*)destPtr)[i]=((char*)srcPtr)[i];
			index[0]=((char*)destPtr)[i];
			((char*)destPtr)[i]=index[((char*)srcPtr)[i]]; //more faster?
		}
	}else{
		for( i=byteCount-1; i>=0; i--){
			//if( ((char*)srcPtr)[i]==GCODE2INDEX(0) ) ((char*)destPtr)[i]=((char*)srcPtr)[i];
			index[0]=((char*)destPtr)[i];
			((char*)destPtr)[i]=index[((char*)srcPtr)[i]];
		}
	}
}

static pascal void BlockMoveData_gmode(const void*	srcPtr,	 void *	destPtr,
								 Size 	byteCount)
{
	int i, tmp;
	
	if( srcPtr>destPtr ){
		for( i=0; i<byteCount; i++){
			tmp=((char*)destPtr)[i];
			tmp &= ~gmode_mask;
			tmp |= ( ((char*)srcPtr)[i] & gmode_mask);
			((char*)destPtr)[i]= tmp;
		}
	}else{
		for( i=byteCount-1; i>=0; i--){
			tmp=((char*)destPtr)[i];
			tmp &= ~gmode_mask;
			tmp |= ( ((char*)srcPtr)[i] & gmode_mask);
			((char*)destPtr)[i]= tmp;
		}
	}
}

#if __MC68K__
static pascal void mymemmove(const void* srcPtr, void * destPtr,Size byteCount)
{
	memmove(destPtr, srcPtr, byteCount);
}
 #define BlockMoveData mymemmove
#endif

void MyCopyBits(PixMapHandle srcPixmap, PixMapHandle dstPixmap,
					 Rect srcRect, Rect dstRect, short mode, int gmode)
{														//I ignore destRect.right,bottom
	int srcRowBytes= (**srcPixmap).rowBytes & 0x1FFF,
		destRowBytes= (**dstPixmap).rowBytes & 0x1FFF,
		y1, y2, width,hight, cut;
	Ptr	srcAdr= GetPixBaseAddr(srcPixmap),
		dstAdr= GetPixBaseAddr(dstPixmap);	
	Rect	srcBounds=  (**srcPixmap).bounds,
			dstBounds=  (**dstPixmap).bounds;

	
	//check params
	//chech src top
	if( srcRect.top<srcBounds.top ){
		cut= srcBounds.top-srcRect.top;
		srcRect.top+=cut; dstRect.top+=cut;
	}
	if( srcRect.top>srcBounds.bottom ) return;
	//check left
	if( srcRect.left  <srcBounds.left ){
		cut= srcBounds.left-srcRect.left;
		srcRect.left+= cut; dstRect.top+=cut;
	}
	if( srcRect.left>srcBounds.right ) return;
	//chech src bottom
	if( srcRect.bottom>srcBounds.bottom ){
		cut= srcRect.bottom-srcBounds.bottom;
		srcRect.bottom-= cut; dstRect.bottom-=cut;
	}
	if( srcRect.bottom<srcBounds.top ) return;
	//check right
	if( srcRect.right >srcBounds.right ){
		cut= srcRect.right-srcBounds.right;
		srcRect.right-= cut; srcBounds.right-= cut;
	}
	if( srcRect.right<srcBounds.left ) return;
	
	width=srcRect.right-srcRect.left;
	hight=srcRect.bottom-srcRect.top;
	
	//check dest
	//check top
	if( dstRect.top  <dstBounds.top ){
		cut= dstBounds.top-dstRect.top;
		srcRect.top+=cut; dstRect.top+=cut;
	}
	if( dstRect.top>dstBounds.bottom ) return;
	//check hight
	if( dstRect.top+hight>dstBounds.bottom ){	
		hight=dstBounds.bottom-dstRect.top;
		srcRect.bottom=srcRect.top+hight;
	}
	//check left
	if( dstRect.left <dstBounds.left ){
		cut= dstBounds.left-dstRect.left;
		srcRect.left+= cut; dstRect.top+=cut;
	}
	if( dstRect.left>dstBounds.right ) return;
	//check width
	if( dstRect.left+width>dstBounds.right )
		width=dstBounds.right-dstRect.left;
	
	switch( mode ){
	case 0://srcCopy
		{
		pascal void (*func)(const void* srcPtr, void *	destPtr,Size byteCount);
		if( gmode==0xF ) func=BlockMoveData;
				else func= BlockMoveData_gmode;
			if( srcRect.top >= dstRect.top ){
				for( y1=srcRect.top, y2=dstRect.top; y1<srcRect.bottom; y1++,y2++ ){
					func( &(srcAdr[y1*srcRowBytes+srcRect.left]),
									&(dstAdr[y2*destRowBytes+dstRect.left]), width);
				}
			}else{
				for( y1=srcRect.bottom-1, y2=dstRect.top+hight-1; y1>=srcRect.top; y1--, y2-- ){
					func( &(srcAdr[y1*srcRowBytes+srcRect.left]),
									&(dstAdr[y2*destRowBytes+dstRect.left]), width);
				}
			}
		}
		break;
	case 3://transparent
			if( srcRect.top >= dstRect.top ){
			for( y1=srcRect.top, y2=dstRect.top; y1<srcRect.bottom; y1++,y2++ ){
				BlockMoveData_transparent( &(srcAdr[y1*srcRowBytes+srcRect.left]),
								&(dstAdr[y2*destRowBytes+dstRect.left]), width);
			}
		}else{
			for( y1=srcRect.bottom-1, y2=dstRect.top+hight-1; y1>=srcRect.top; y1--, y2-- ){
				BlockMoveData_transparent( &(srcAdr[y1*srcRowBytes+srcRect.left]),
								&(dstAdr[y2*destRowBytes+dstRect.left]), width);
			}
		}

	}
}

void dev_line(PixMapHandle pixmap, Rect rect, int color, int sw, int gmode)
{
														//I ignore destRect.right,bottom
	int		rowBytes= (**pixmap).rowBytes & 0x1FFF,
			x, y1, width,hight, tmp, index;
	Ptr		baseAdr= GetPixBaseAddr(pixmap);
	Rect	bounds=  (**pixmap).bounds;
	
	//check params
	//chech src top
	if( rect.top<bounds.top ){
		rect.top=bounds.top;
	}
	if( rect.top>bounds.bottom ) return;
	//check left
	if( rect.left  <bounds.left ){
		rect.left= bounds.left;
	}
	if( rect.left>bounds.right ) return;
	//chech src bottom
	if( rect.bottom>bounds.bottom ){
		rect.bottom= bounds.bottom;
	}
	if( rect.bottom<bounds.top ) return;
	//check right
	if( rect.right >bounds.right ){
		rect.right= bounds.right;
	}
	if( rect.right<bounds.left ) return;
	
	width=rect.right-rect.left;
	hight=rect.bottom-rect.top;
	color &= gmode;
	index= GCODE2INDEX(color);

	switch( sw ){
	case 2://simple filled rect
		for( y1=rect.top; y1<rect.bottom; y1++ ){
			for( x=rect.left; x<rect.right; x++){
				tmp=baseAdr[y1*rowBytes+x];
				tmp &= ~gmode;
				tmp |= index;
				baseAdr[y1*rowBytes+x]=tmp;
			}
		}
	}
}



