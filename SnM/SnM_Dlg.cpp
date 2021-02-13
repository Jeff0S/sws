/******************************************************************************
/ SnM_Dlg.cpp
/
/ Copyright (c) 2009 and later Jeffos
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

#include "SnM.h"
#include "SnM_Dlg.h"
#include "SnM_Util.h"
#ifdef _WIN32
  #include "../Breeder/BR_Util.h" // for SetWndIcon()
#endif
#include "../Prompt.h"

#include <WDL/localize/localize.h>

///////////////////////////////////////////////////////////////////////////////
// General S&M UI helpers
///////////////////////////////////////////////////////////////////////////////

void SNM_UIInit() {}

void SNM_UIExit() {
	if (LICE_IBitmap* logo = SNM_GetThemeLogo())
		DELETE_NULL(logo);
}

void SNM_UIRefresh(COMMAND_T* _ct)
{
	UpdateTimeline(); // ruler+arrange
	TrackList_AdjustWindows(false);
	DockWindowRefresh();
}


///////////////////////////////////////////////////////////////////////////////
// Theming
///////////////////////////////////////////////////////////////////////////////

// native font rendering default value
// note: not configurable on osx, optional on win (s&m.ini)
bool g_SNM_ClearType = true;

ColorTheme* SNM_GetColorTheme(bool _checkForSize) {
	int sz; ColorTheme* ct = (ColorTheme*)GetColorThemeStruct(&sz);
	if (ct && (!_checkForSize || sz >= sizeof(ColorTheme))) return ct;
	return NULL;
}

IconTheme* SNM_GetIconTheme(bool _checkForSize) {
	int sz; IconTheme* it = (IconTheme*)GetIconThemeStruct(&sz);
	if (it && (!_checkForSize || sz >= sizeof(IconTheme))) return it;
	return NULL;
}

// _type: 
// 0=default font (non native rendering)
// 1=default font (native rendering, optional)
// 2=toolbar button font (native rendering, optional)
LICE_CachedFont* SNM_GetFont(int _type)
{
	static LICE_CachedFont sFonts[3];
	if (!sFonts[_type].GetHFont())
	{
		LOGFONT lf = {
			SNM_FONT_HEIGHT,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,SNM_FONT_NAME
		};
		sFonts[_type].SetFromHFont(
			CreateFontIndirect(&lf),
			LICE_FONT_FLAG_OWNS_HFONT | (!_type ? 0 : (g_SNM_ClearType?LICE_FONT_FLAG_FORCE_NATIVE:0)));
		sFonts[_type].SetBkMode(TRANSPARENT);
		// others props are set on demand (to support theme switches)
	}
	if (ColorTheme* ct = SNM_GetColorTheme()) 
		sFonts[_type].SetTextColor(LICE_RGBA_FROMNATIVE(_type==2?ct->toolbar_button_text:ct->main_text,255));
	else
		sFonts[_type].SetTextColor(LICE_RGBA(255,255,255,255));
	return &sFonts[_type];
}

LICE_CachedFont* SNM_GetThemeFont() {
	return SNM_GetFont(1);
}

LICE_CachedFont* SNM_GetToolbarFont() {
	return SNM_GetFont(2);
}

void SNM_GetThemeListColors(int* _bg, int* _txt, int* _grid)
{
	int bgcol=-1, txtcol=-1, gridcol=-1;
	ColorTheme* ct = SNM_GetColorTheme(true); // true: list view colors are recent (v4.11)
	if (ct) {
		bgcol = ct->genlist_bg;
		txtcol = ct->genlist_fg;
		gridcol = ct->genlist_gridlines;
		// note: selection colors are not managed here
	}
	if (bgcol == txtcol) { // safety
		bgcol = GSC_mainwnd(COLOR_WINDOW);
		txtcol = GSC_mainwnd(COLOR_BTNTEXT);
	}
	if (_bg) *_bg = bgcol;
	if (_txt) *_txt = txtcol;
	if (_grid) *_grid = gridcol;
}

void SNM_GetThemeEditColors(int* _bg, int* _txt)
{
	int bgcol=-1, txtcol=-1;
	ColorTheme* ct = SNM_GetColorTheme();
	if (ct) {
		bgcol =  ct->main_editbk;
		txtcol = GSC_mainwnd(COLOR_BTNTEXT);
	}
	if (bgcol == txtcol) { // safety
		bgcol = GSC_mainwnd(COLOR_WINDOW);
		txtcol = GSC_mainwnd(COLOR_BTNTEXT);
	}
	if (_bg) *_bg = bgcol;
	if (_txt) *_txt = txtcol;
}

void SNM_ThemeListView(HWND _h)
{
	if (_h)
	{
		int bgcol, txtcol, gridcol;
		SNM_GetThemeListColors(&bgcol, &txtcol, &gridcol);
		ListView_SetBkColor(_h, bgcol);
		ListView_SetTextColor(_h, txtcol);
		ListView_SetTextBkColor(_h, bgcol);
#ifndef _WIN32
		ListView_SetGridColor(_h, gridcol);
#endif
	}
}

void SNM_ThemeListView(SWS_ListView* _lv)
{
	if (_lv) return SNM_ThemeListView(_lv->GetHWND());
}

// note that LICE_LoadPNGFromResource() is KO on OSX (it looks for REAPER's resources..)
LICE_IBitmap* SNM_GetThemeLogo()
{
	static LICE_IBitmap* sLogo;
	if (!sLogo)
	{
		// logo is loaded from memory (for OSX support)
		if (WDL_HeapBuf* hb = TranscodeStr64ToHeapBuf(SNM_LOGO_PNG_FILE)) {
			sLogo = LICE_LoadPNGFromMemory(hb->Get(), hb->GetSize());
			delete hb;
		}
	}
	return sLogo;
}


#ifdef _WIN32

static BOOL CALLBACK EnumRemoveXPStyles(HWND _hwnd, LPARAM _unused)
{
	// do not deal with list views & list boxes
	char className[64] = "";
	if (GetClassName(_hwnd, className, sizeof(className)) && 
		strcmp(className, WC_LISTVIEW) && 
		strcmp(className, WC_LISTBOX))
	{
		LONG style = GetWindowLong(_hwnd, GWL_STYLE);
		if ((style & BS_AUTOCHECKBOX) == BS_AUTOCHECKBOX ||
			(style & BS_AUTORADIOBUTTON) == BS_AUTORADIOBUTTON ||
			(style & BS_GROUPBOX) == BS_GROUPBOX)
		{
			RemoveXPStyle(_hwnd, 1);
		}
	}
	return TRUE;
}

#endif

WDL_DLGRET SNM_HookThemeColorsMessage(HWND _hwnd, UINT _uMsg, WPARAM _wParam, LPARAM _lParam, bool _wantColorEdit)
{
	if (SWS_THEMING)
	{
		switch(_uMsg)
		{
#ifdef _WIN32
			case WM_INITDIALOG :
				// remove XP style on some child ctrls (cannot be themed otherwise)
				EnumChildWindows(_hwnd, EnumRemoveXPStyles, 0);
				SetWndIcon(_hwnd);
				return 0;
#endif
			case WM_CTLCOLOREDIT:
				if (!_wantColorEdit) return 0;
			case WM_CTLCOLORSCROLLBAR: // not managed yet, just in case..
			case WM_CTLCOLORLISTBOX:
			case WM_CTLCOLORBTN:
			case WM_CTLCOLORDLG:
			case WM_CTLCOLORSTATIC:
/* commented (custom implementations)
			case WM_DRAWITEM:
*/
				return SendMessage(GetMainHwnd(), _uMsg, _wParam, _lParam);
		}
	}
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
// Messages, prompt, etc..
///////////////////////////////////////////////////////////////////////////////

void SNM_ShowMsg(const char* _msg, const char* _title, HWND _hParent)
{
	char msg[1024*8] = "";
	GetStringWithRN(_msg, msg, sizeof(msg)); // truncates if needed
	DisplayInfoBox(_hParent?_hParent:GetMainHwnd(), _title, msg, false, false); // modeless
}

// for modeless box created with SNM_ShowMsg()
void SNM_CloseMsg(HWND _hParent) {
	DisplayInfoBox(_hParent?_hParent:GetMainHwnd(), "", "", false, false);
}

// _min and _max: 1-based (i.e. as displayed)
// returns -1 on cancel, 0-based number otherwise
int PromptForInteger(const char* _title, const char* _what, int _min, int _max, bool _showMinMax)
{
	WDL_String str;
	int nb = -1;
	while (nb == -1)
	{
		if (_showMinMax)
			str.SetFormatted(128, "%s (%d-%d):", _what, _min, _max);
		else
			str.SetFormatted(128, "%s:", _what);

		char reply[32]= ""; // no default
		if (GetUserInputs(_title, 1, str.Get(), reply, sizeof(reply)))
		{
			nb = atoi(reply); // 0 on error
			if (nb >= _min && nb <= _max)
			{
				return (nb-1);
			}
			else
			{
				nb = -1;
				str.SetFormatted(128, __LOCALIZE_VERFMT("Please enter a value in [%d; %d].","sws_mbox"), _min, _max);
				MessageBox(GetMainHwnd(), str.Get(), __LOCALIZE("S&M - Error","sws_mbox"), MB_OK);
			}
		}
		else
			return -1; // user has cancelled
	}
	return -1;
}
