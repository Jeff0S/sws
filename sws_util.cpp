/******************************************************************************
/ sws_util.cpp
/
/ Copyright (c) 2010 and later Tim Payne (SWS), Jeffos
/
/
/ Permission is hereby granted, free of charge, to any person obtaining a copy
/ of this software and associated documentation files (the "Software"), to deal
/ in the Software without restriction, including without limitation the rights to
/ use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
/ of the Software, and to permit persons to whom the Software is furnished to
/ do so, subject to the following conditions:
/ 
/ The above copyright notice and this permission notice shall be included in all
/ copies or substantial portions of the Software.
/ 
/ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
/ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
/ OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
/ NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
/ HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
/ WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
/ OTHER DEALINGS IN THE SOFTWARE.
/
******************************************************************************/


#include "stdafx.h"

#include "Breeder/BR_Util.h"

#include <WDL/sha.h>
#include <WDL/localize/localize.h>

// Globals
double g_d0 = 0.0;
int  g_i0 = 0;
int  g_i1 = 1;
int  g_i2 = 2;
bool g_bTrue  = true;
bool g_bFalse = false;
MTRand g_MTRand;
#ifndef _WIN32
const GUID GUID_NULL = { 0, 0, 0, "\0\0\0\0\0\0\0" };
#endif

void SaveWindowPos(HWND hwnd, const char* cKey)
{
	// Remember the dialog position
	char str[256];
	RECT r;
	GetWindowRect(hwnd, &r);
	sprintf(str, "%d %d %d %d", (int)r.left, (int)r.top, (int)(r.right-r.left), (int)(r.bottom-r.top));
	WritePrivateProfileString(SWS_INI, cKey, str, get_ini_file());
}

void RestoreWindowPos(HWND hwnd, const char* cKey, bool bRestoreSize)
{
	char str[256];
	GetPrivateProfileString(SWS_INI, cKey, "unknown", str, 256,get_ini_file());
	LineParser lp(false);
	if (!lp.parse(str) && lp.getnumtokens() == 4)
	{
		RECT r;
		r.left   = lp.gettoken_int(0);
		r.top    = lp.gettoken_int(1);
		r.right  = lp.gettoken_int(0) + lp.gettoken_int(2);
		r.bottom = lp.gettoken_int(1) + lp.gettoken_int(3);
		EnsureNotCompletelyOffscreen(&r);
		if (bRestoreSize)
			SetWindowPos(hwnd, NULL, r.left, r.top, r.right-r.left, r.bottom-r.top, SWP_NOZORDER);
		else
			SetWindowPos(hwnd, NULL, r.left, r.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	}
}

void SetWindowPosAtMouse(HWND hwnd)
{
	RECT r;
	GetWindowRect(hwnd, &r);
	POINT p;
	GetCursorPos(&p);
	r.right = p.x + abs(r.right-r.left);
	r.bottom = p.y + abs(r.bottom-r.top);
	r.left = p.x;
	r.top = p.y;
	EnsureNotCompletelyOffscreen(&r);
	SetWindowPos(hwnd, NULL, r.left, r.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

#ifndef _WIN32

int GetMenuString(HMENU hMenu, UINT uIDItem, char* lpString, int nMaxCount, UINT uFlag)
{
	if (hMenu && lpString)
	{
		MENUITEMINFO xInfo;
		xInfo.cbSize = sizeof(MENUITEMINFO);
/* On Win OS
		xInfo.fMask = MIIM_STRING;
*/
		xInfo.fMask = MIIM_TYPE; // ok on OSX/SWELL
		xInfo.fType = MFT_STRING;
		xInfo.fState = 0;
		xInfo.wID = 0;
		xInfo.hSubMenu = NULL;
		xInfo.hbmpChecked = NULL;
		xInfo.hbmpUnchecked = NULL;
		xInfo.dwItemData = 0;
		xInfo.dwTypeData = lpString;
		xInfo.cch = nMaxCount;
		if (GetMenuItemInfo(hMenu, uIDItem, (uFlag&MF_BYPOSITION) == MF_BYPOSITION, &xInfo))
			return strlen(lpString);
	}
	return 0;
}

// Modified LoadCursor from Cockos WDL
// Supports these cursors:
// IDC_GRID_WARP
// IDC_ENV_PEN_GRID, IDC_ENV_PT_ADJ_VERT
// IDC_MISC_SPEAKER
// IDC_ZOOM_DRAG, IDC_ZOOM_IN, IDC_ZOOM_OUT, IDC_ZOOM_UNDO
// IDC_ERASER
HCURSOR SWS_LoadCursor(int id)
{
  // bytemaps are (white<<4)|(alpha)
  const unsigned char B = 0x0F;
  const unsigned char W = 0xFF;

  static SWS_Cursor cursors[] {
    { IDC_ENV_PEN_GRID, 2, 14, {
      0,0,0,0,0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,B,0,0,B,0,0,B,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,B,W,W,B,B,0,0,B,0,0,B,0,0,B,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,B,W,W,B,B,0,0,0,B,0,0,B,0,0,B,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,B,W,W,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,B,W,W,B,B,0,0,0,0,0,B,0,0,B,0,0,B,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,B,W,W,B,B,0,0,0,0,0,0,B,0,0,B,0,0,B,0,0,0,0,0,0,0,0,
      0,0,0,0,0,B,W,W,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,B,W,W,B,B,0,0,0,0,0,0,0,0,B,0,0,B,0,0,B,0,0,0,0,0,0,0,0,
      0,0,0,B,W,W,B,B,0,0,0,0,0,0,0,0,0,B,0,0,B,0,0,B,0,0,0,0,0,0,0,0,
      0,0,B,B,W,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,B,B,B,0,0,0,0,0,0,0,0,0,0,0,B,0,0,B,0,0,B,0,0,0,0,0,0,0,0,
      0,B,B,B,B,0,0,0,0,0,B,B,0,0,0,0,0,B,0,0,B,0,0,B,0,0,0,0,0,0,0,0,
      0,B,B,0,0,0,0,0,B,B,0,0,B,B,0,0,0,B,0,0,B,0,0,B,0,0,0,0,0,0,0,0,
      B,0,0,B,B,0,0,B,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,B,B,0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    }},
    { IDC_ENV_PT_ADJ_VERT, 4, 12, {
      0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,W,W,W,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,B,B,B,W,B,B,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,0,B,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,B,W,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,B,W,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,0,B,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,B,B,B,W,B,B,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,W,W,W,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    }},
    { IDC_GRID_WARP, 8, 10, {
      0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,B,B,0,0,0,B,0,0,0,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,W,B,0,0,0,B,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,B,B,B,B,B,B,B,B,B,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,W,W,W,W,W,W,W,W,W,W,W,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,B,B,B,B,B,B,B,B,B,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,W,B,0,0,0,B,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,B,B,0,0,0,B,0,0,0,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    }},
    { IDC_MISC_SPEAKER, 10, 7, {
      0,0,0,0,0,0,0,B,B,B,0,0,0,0,0,0,0,0,W,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,B,B,W,B,0,0,0,0,0,0,0,W,B,W,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,B,B,W,W,B,0,0,0,0,0,W,0,0,W,B,W,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,B,B,W,W,W,B,0,0,0,0,W,B,W,0,0,W,B,W,0,0,0,0,0,0,0,0,0,0,
      B,B,B,B,B,W,W,W,W,B,0,0,W,0,0,W,B,W,0,W,B,W,0,0,0,0,0,0,0,0,0,0,
      B,B,W,B,W,W,W,W,W,B,0,W,B,W,0,W,B,W,0,0,W,B,W,0,0,0,0,0,0,0,0,0,
      B,B,W,B,W,W,W,W,W,B,0,0,W,B,W,0,W,B,W,0,W,B,W,0,0,0,0,0,0,0,0,0,
      B,B,W,B,W,W,W,W,W,B,0,0,W,B,W,0,W,B,W,0,W,B,W,0,0,0,0,0,0,0,0,0,
      B,B,W,B,W,W,W,W,W,B,0,0,W,B,W,0,W,B,W,0,W,B,W,0,0,0,0,0,0,0,0,0,
      B,B,W,B,W,W,W,W,W,B,0,W,B,W,0,W,B,W,0,0,W,B,W,0,0,0,0,0,0,0,0,0,
      B,B,B,B,B,W,W,W,W,B,0,0,W,0,0,W,B,W,0,W,B,W,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,B,B,W,W,W,B,0,0,0,0,W,B,W,0,0,W,B,W,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,B,B,W,W,B,0,0,0,0,0,W,0,0,W,B,W,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,B,B,W,B,0,0,0,0,0,0,0,W,B,W,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,B,B,B,0,0,0,0,0,0,0,0,W,0,0,0,0,0,0,0,0,0,0,0,0,0,
    }},
    { IDC_ZOOM_DRAG, 5, 5, {
      0,0,0,0,B,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,B,W,W,W,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,B,B,B,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,B,B,W,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,B,W,W,W,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,B,B,B,B,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,B,W,W,W,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,B,B,W,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,B,B,B,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,B,W,W,W,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,B,B,B,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    }},
    { IDC_ZOOM_IN, 5, 5, {
      0,0,0,0,B,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,B,W,W,W,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,B,B,B,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,B,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,B,B,B,B,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,W,W,W,W,W,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,B,B,B,B,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,B,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,B,B,B,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,B,W,W,W,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,B,B,B,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    }},
    { IDC_ZOOM_OUT, 5, 5, {
      0,0,0,0,B,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,B,W,W,W,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,B,B,B,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,B,W,W,W,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,B,B,B,B,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,W,W,W,W,W,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,B,B,B,B,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,B,W,W,W,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,B,B,B,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,B,W,W,W,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,B,B,B,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    }},
    { IDC_ZOOM_UNDO, 5, 5, {
      0,0,0,0,B,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,B,W,W,W,B,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,B,B,B,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,B,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,W,B,0,B,W,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,W,B,0,B,W,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      B,W,B,W,B,B,B,W,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,B,W,W,W,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,B,W,W,B,B,B,W,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,B,B,W,W,W,B,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,B,B,B,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    }},
    { IDC_ERASER, 15, 2, {
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,B,B,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,B,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,W,W,B,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,W,W,W,W,B,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,W,W,W,W,W,B,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,W,W,W,W,W,W,B,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,W,W,W,W,W,W,W,B,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,W,W,W,W,W,W,B,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,W,W,W,W,W,B,0,0,B,B,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,W,W,W,W,B,0,0,B,B,B,B,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,W,W,B,0,0,B,B,B,B,B,B,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,W,W,B,0,0,B,B,B,B,B,B,B,B,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,W,B,0,0,B,B,B,B,B,B,B,B,B,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,0,0,B,B,B,B,B,B,B,B,B,B,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,B,B,B,B,B,B,B,B,B,B,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,B,B,B,B,B,B,B,B,B,B,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,B,B,B,B,B,B,B,B,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,B,B,B,B,B,B,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,B,B,B,B,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,B,B,B,0,0,0,0,0,
    }},
  };

  // std::end is C++11
  const size_t n = sizeof(cursors) / sizeof(cursors[0]);
  const auto it = std::find_if(cursors, cursors + n,
    [id](const SWS_Cursor &cd) { return cd.id == id; });

  return it == cursors + n ? nullptr : it->makeFromData();
}

#endif

MediaTrack* GetFirstSelectedTrack()
{
	for (int j = 0; j <= GetNumTracks(); j++)
	{
		MediaTrack* tr = CSurf_TrackFromID(j, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			return tr;
	}
	return NULL;
}

// NumSelTracks takes the master track into account (contrary to CountSelectedTracks())
int NumSelTracks()
{
	int iCount = 0;
	for (int i = 0; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			iCount++;
	}
	return iCount;
}

static MediaTrack** g_pSelTracks = NULL;
static int g_iNumSel = 0;
void SaveSelected()
{
	if (g_pSelTracks)
	{
		delete [] g_pSelTracks;
		g_pSelTracks=NULL;
	}
	g_iNumSel = NumSelTracks();
	if (g_iNumSel)
	{
		g_pSelTracks = new MediaTrack*[g_iNumSel];
		int i = 0;
		for (int j = 0; j <= GetNumTracks(); j++)
			if (*(int*)GetSetMediaTrackInfo(CSurf_TrackFromID(j, false), "I_SELECTED", NULL))
				g_pSelTracks[i++] = CSurf_TrackFromID(j, false);
	}
}

void RestoreSelected()
{
	if (!g_pSelTracks) return;

	int iSel = 1;
	for (int i = 0; i < g_iNumSel; i++)
		if (CSurf_TrackToID(g_pSelTracks[i], false) >= 0)
			GetSetMediaTrackInfo(g_pSelTracks[i], "I_SELECTED", &iSel);
}

void ClearSelected()
{
	int iSel = 0;
	for (int i = 0; i <= GetNumTracks(); i++)
		GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "I_SELECTED", &iSel);
}

int GetFolderDepth(MediaTrack* tr, int* iType, MediaTrack** nextTr) // iType: 0=normal, 1=parent, < 0=last in folder
{
	//static MediaTrack* nextTr = NULL; Let calling fcn keep track so state is reset appropriately
	static int iLastFolder = 0;
	int iFolder;

	if (tr == *nextTr)
	{
		*nextTr = CSurf_TrackFromID(CSurf_TrackToID(tr, false) + 1, false);

		if (CSurf_TrackToID(tr, false) == 0) // Special case for master
		{
			if (iType)
				*iType = 1;
			return -1; // Master is at '-1' depth
		}

		iFolder = *(int*)GetSetMediaTrackInfo(tr, "I_FOLDERDEPTH", NULL);
			
		if (iType)
			*iType = iFolder;

		if (iFolder == 0)
			return iLastFolder;
		else if (iFolder == 1)
		{
			iLastFolder++;
			return iLastFolder - 1; // Count parent as at the "previous" level
		}
		else if (iFolder < 0)
		{
			int iCur = iLastFolder;
			iLastFolder += iFolder;
			return iCur;
		}
	}
	else
	{
		// Must iterate until reaching the track passed in
		iLastFolder = 0;
		*nextTr = CSurf_TrackFromID(0, false);
		while (tr != *nextTr)
			GetFolderDepth(*nextTr, NULL, nextTr);

		return GetFolderDepth(tr, iType, nextTr);
	}
	return -1;
}

int GetTrackVis(MediaTrack* tr) // &1 == mcp, &2 == tcp
{
	int iTrack = CSurf_TrackToID(tr, false);
	if (iTrack == 0)
		return *ConfigVar<int>("showmaintrack") ? 3 : 1; // For now, always return master vis in MCP
	else if (iTrack < 0)
		return 0;

	int iVis = *(bool*)GetSetMediaTrackInfo(tr, "B_SHOWINMIXER", NULL) ? 1 : 0;
	iVis    |= *(bool*)GetSetMediaTrackInfo(tr, "B_SHOWINTCP", NULL) ? 2 : 0;
	return iVis;
}

void SetTrackVis(MediaTrack* tr, int vis) // &1 == mcp, &2 == tcp
{
	int iTrack = CSurf_TrackToID(tr, false);
	if (iTrack == 0)
	{	// TODO - obey master in mcp
		if ((vis & 2) != (*ConfigVar<int>("showmaintrack") ? 2 : 0))
			Main_OnCommand(40075, 0);
	}
	else if (iTrack > 0)
	{
		if (vis != GetTrackVis(tr))
		{
			GetSetMediaTrackInfo(tr, "B_SHOWINTCP",   vis & 2 ? &g_bTrue : &g_bFalse);
			GetSetMediaTrackInfo(tr, "B_SHOWINMIXER", vis & 1 ? &g_bTrue : &g_bFalse);
		}
	}
}

HWND GetTrackWnd()
{
	return GetArrangeWnd(); // BR: will take care of any localization issues
}

HWND GetRulerWnd()
{
	return GetRulerWndAlt(); // BR: will take care of any localization issues
}

// overrides the native GetTrackGUID(): returns a special GUID for the master track
// (useful for persistence as no GUID is stored in RPP files for the master..)
// note: TrackToGuid(NULL) will return also NULL, contrary to GetTrackGUID(NULL)
// also adds support for retriving GUIDs from any project tabs
const GUID* TrackToGuid(ReaProject* proj, MediaTrack* tr)
{
	if (tr == GetMasterTrack(proj))
		return &GUID_NULL;
	else if(ValidatePtr2(proj, tr, "MediaTrack*"))
		return static_cast<GUID*>(GetSetMediaTrackInfo(tr, "GUID", nullptr));

	return nullptr;
}


MediaTrack* GuidToTrack(ReaProject* project, const GUID* guid)
{
	if (!guid)
		return nullptr;

	MediaTrack* master = GetMasterTrack(project);
	if (master && TrackMatchesGuid(project, master, guid))
		return master;

	const int trackCount = CountTracks(project);
	for (int i = 0; i < trackCount; ++i) {
		MediaTrack* tr = GetTrack(project, i);
		if (tr && TrackMatchesGuid(project, tr, guid))
			return tr;
	}

	return nullptr;
}

bool GuidsEqual(const GUID* g1, const GUID* g2)
{
	return g1 && g2 && !memcmp(g1, g2, sizeof(GUID));
}

bool TrackMatchesGuid(ReaProject* proj, MediaTrack* tr, const GUID* match)
{
	return tr && match &&
		GuidsEqual(TrackToGuid(proj, tr), match);
}

const char *stristr(const char* a, const char* b)
{
  int i;
  int len = strlen(b);
  int n = strlen(a)-len;
  for (i = 0; i <= n; ++i)
  {
#ifdef _WIN32
    if (!_strnicmp(a+i, b, len)) return a+i;
#else
    if (!strnicmp(a+i, b, len)) return a+i;
#endif
  }
  return NULL;
}

#ifdef _WIN32
void dprintf(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int len = _vscprintf(format, args) + 1;
	char* buffer = new char[len];
	vsprintf_s(buffer, len, format, args); // C4996
	OutputDebugString(buffer);
	delete[] buffer;
}
#endif

void SWS_GetAllTracks(WDL_TypedBuf<MediaTrack*>* buf, bool bMaster)
{
	buf->Resize(0);
	for (int i = (bMaster ? 0 : 1); i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		int pos = buf->GetSize();
		buf->Resize(pos + 1);
		buf->Get()[pos] = tr;
	}
}

void SWS_GetSelectedTracks(WDL_TypedBuf<MediaTrack*>* buf, bool bMaster)
{
	buf->Resize(0);
	for (int i = (bMaster ? 0 : 1); i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			int pos = buf->GetSize();
			buf->Resize(pos + 1);
			buf->Get()[pos] = tr;
		}
	}
}

void SWS_GetSelectedMediaItems(WDL_TypedBuf<MediaItem*>* buf)
{
	buf->Resize(0);
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		for (int j = 0; j < GetTrackNumMediaItems(tr); j++)
		{
			MediaItem* item = GetTrackMediaItem(tr, j);
			if (*(bool*)GetSetMediaItemInfo(item, "B_UISEL", NULL))
			{
				int pos = buf->GetSize();
				buf->Resize(pos + 1);
				buf->Get()[pos] = item;
			}
		}
	}
}

void SWS_GetSelectedMediaItemsOnTrack(WDL_TypedBuf<MediaItem*>* buf, MediaTrack* tr)
{
	buf->Resize(0);
	if (CSurf_TrackToID(tr, false) <= 0)
		return;
	for (int j = 0; j < GetTrackNumMediaItems(tr); j++)
	{
		MediaItem* item = GetTrackMediaItem(tr, j);
		if (*(bool*)GetSetMediaItemInfo(item, "B_UISEL", NULL))
		{
			int pos = buf->GetSize();
			buf->Resize(pos + 1);
			buf->Get()[pos] = item;
		}
	}
}

int SWS_GetModifiers()
{
	int iKeys = GetAsyncKeyState(VK_CONTROL) & 0x8000 ? SWS_CTRL  : 0;
	iKeys    |= GetAsyncKeyState(VK_MENU)    & 0x8000 ? SWS_ALT   : 0;
	iKeys    |= GetAsyncKeyState(VK_SHIFT)   & 0x8000 ? SWS_SHIFT : 0;
	return iKeys;
}

MODIFIER g_modifiers[NUM_MODIFIERS] =
{
	{ 0, "None" },
	{ 2, "Ctrl" },
	{ 1, "Alt" },
	{ 4, "Shift" },
	{ 3, "Ctrl + Alt" },
	{ 6, "Ctrl + Shift" },
	{ 5, "Alt + Shift" },
	{ 7, "Ctrl + Alt + Shift" }
};

bool SWS_IsWindow(HWND hwnd)
{
#ifdef _WIN32
	return IsWindow(hwnd) ? true : false;
#else
	// ISSUE 498: OSX IsWindow is broken for docked windows!
	//   Justin recommends not using IsWindow at all, but there are a lot of cases
	//   where we rely on it for error checking.  Instead of removing all calls and
	//   doing internal window state handling I replaced all IsWindow calls with
	//   this function that checks for docked windows on OSX.  It's a bit of a
	//   hack, but less risky IMO than rewriting tons of window handling code.
	//	
	//   Maybe could replace with return hwnd != NULL;
	return (bool)IsWindow(hwnd) ? true : (DockIsChildOfDock(hwnd, NULL) != -1);
#endif
}

// Localization
WDL_FastString* g_LangPack = NULL;

// Never returns NULL ("" means no langpack defined)
WDL_FastString* GetLangPack()
{
	// lazy init, works because swtching LangPacks requires REAPER restart
	if (!g_LangPack)
	{
		g_LangPack = new WDL_FastString;
#ifdef _SWS_LOCALIZATION
		char fn[BUFFER_SIZE] = ""; 
		GetPrivateProfileString("REAPER", "langpack", "", fn, sizeof(fn), get_ini_file());
		if (*fn && strcmp(fn, "<>"))
		{
			g_LangPack->Set(fn);
			if (!FileExists(fn)) // retry with fn as "short name"
			{
				g_LangPack->SetFormatted(BUFFER_SIZE, "%s%cLangPack%c%s", GetResourcePath(), PATH_SLASH_CHAR, PATH_SLASH_CHAR, fn);
				if (!FileExists(g_LangPack->Get())) // does not exist either..
					g_LangPack->Set("");
			}
		}
#endif
	}
	return g_LangPack;
}

bool IsLocalized()
{
#ifdef _SWS_LOCALIZATION
	return (GetLangPack()->GetLength() > 0);
#else
	return false;
#endif
}

// check that action tags ("SWS: ", "SWS/FNG: ", SWS/S&M: ", etc..) are preserved
const char* GetLocalizedActionName(const char* _defaultStr, int _flags, const char* _section)
{
#ifdef _SWS_LOCALIZATION
	const char* p = __localizeFunc(_defaultStr, _section, _flags);
	if (IsSwsAction(p))
		return p;
#endif
	return _defaultStr;
}

bool IsLocalizableAction(const char* _customId) {
	return (!strstr(_customId, "_CYCLACTION") && !strstr(_customId, "_SWSCONSOLE_CUST"));
}

// wrappers to ignore localized envelope names
TrackEnvelope* SWS_GetTakeEnvelopeByName(MediaItem_Take* take, const char* envname) {
	return GetTakeEnvelopeByName(take, __localizeFunc(envname, "item", 0));
}
TrackEnvelope* SWS_GetTrackEnvelopeByName(MediaTrack* track, const char* envname) {
	return GetTrackEnvelopeByName(track,  __localizeFunc(envname, "envname", 0));
}

void UpdateStretchMarkersAfterSetTakeStartOffset(MediaItem_Take* take, double takeStartOffset_multiplyPlayrate)
{
	for (int i = 0; i < GetTakeNumStretchMarkers(take); i++) {
		double posOut;
		GetTakeStretchMarker(take, i, &posOut, NULL);
		SetTakeStretchMarker(take, i, posOut + takeStartOffset_multiplyPlayrate, NULL);
	}
	UpdateItemInProject(GetMediaItemTake_Item(take));
}
