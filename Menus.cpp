/******************************************************************************
/ Menus.cpp
/
/ Copyright (c) 2011 Tim Payne (SWS), Jeffos
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

// Utility functions for manipulating menus, as well as the main menu creation function
// for the SWS extension.

#include "stdafx.h"
#include "./reaper/localize.h"


void AddToMenuOrdered(HMENU hMenu, const char* text, int id, int iInsertAfter, bool bPos, UINT uiSate)
{
	if (!text)
		return;

	int iPos = GetMenuItemCount(hMenu);
	if (bPos)
		iPos = iInsertAfter;
	else
	{
		if (iInsertAfter < 0)
			iPos += iInsertAfter + 1;
		else
		{
			HMENU h = FindMenuItem(hMenu, iInsertAfter, &iPos);
			if (h)
			{
				hMenu = h;
				iPos++;
			}
		}
	}

	MENUITEMINFO mi={sizeof(MENUITEMINFO),};
	if (strcmp(text, SWS_SEPARATOR) == 0)
	{
		mi.fType = MFT_SEPARATOR;
		mi.fMask = MIIM_TYPE;
		InsertMenuItem(hMenu, iPos, true, &mi);
	}
	else
	{
		mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
		mi.fState = uiSate;
		mi.fType = MFT_STRING;
		mi.dwTypeData = (char*)text;
		mi.wID = id;
		InsertMenuItem(hMenu, iPos, true, &mi);
	}
}

// This version "auto sort" menu items
// Note: could be used as default AddToMenu (when no insert position is
// requested) but this func is only used when the extension is localized ATM..
void AddToMenu(HMENU hMenu, const char* text, int id, int iInsertAfter, bool bPos, UINT uiSate)
{
	if (!text)
		return;

	if(!IsLocalized() || bPos || iInsertAfter != -1) {
		AddToMenuOrdered(hMenu, text, id, iInsertAfter, bPos, uiSate);
		return;
	}

	MENUITEMINFO mi={sizeof(MENUITEMINFO),};
	if (strcmp(text, SWS_SEPARATOR) == 0)
	{
		mi.fType = MFT_SEPARATOR;
		mi.fMask = MIIM_TYPE;
		InsertMenuItem(hMenu, GetMenuItemCount(hMenu), true, &mi);
	}
	else
	{
		mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
		mi.fState = uiSate;
		mi.fType = MFT_STRING;
		mi.dwTypeData = (char*)text;
		mi.wID = id;
		InsertMenuItem(hMenu, FindSortedPos(hMenu, text), true, &mi);
	}
}

void AddSubMenuOld(HMENU hMenu, HMENU subMenu, const char* text, int iInsertAfter, UINT uiSate)
{
	int iPos = GetMenuItemCount(hMenu);
	if (iInsertAfter < 0)
	{
		iPos += iInsertAfter + 1;
		if (iPos < 0)
			iPos = 0;
	}
	else
	{
		HMENU h = FindMenuItem(hMenu, iInsertAfter, &iPos);
		if (h)
		{
			hMenu = h;
			iPos++;
		}
	}

	MENUITEMINFO mi={sizeof(MENUITEMINFO),};
	mi.fMask = MIIM_SUBMENU | MIIM_TYPE | MIIM_STATE;
	mi.fState = uiSate;
	mi.fType = MFT_STRING;
	mi.hSubMenu = subMenu;
	mi.dwTypeData = (LPSTR)text;
	InsertMenuItem(hMenu, iPos, true, &mi);
}

// This version "auto sort" sub menu items
// Note: could be used as default AddSubMenu (when no insert position is
// requested) but this func is only used when the extension is localized ATM..
void AddSubMenu(HMENU hMenu, HMENU subMenu, const char* text, int iInsertAfter, UINT uiSate)
{
	if(!IsLocalized() || iInsertAfter != -1) {
		AddSubMenuOld(hMenu, subMenu, text, iInsertAfter, uiSate);
		return;
	}

	MENUITEMINFO mi={sizeof(MENUITEMINFO),};
	mi.fMask = MIIM_SUBMENU | MIIM_TYPE | MIIM_STATE;
	mi.fState = uiSate;
	mi.fType = MFT_STRING;
	mi.hSubMenu = subMenu;
	mi.dwTypeData = (LPSTR)text;
	InsertMenuItem(hMenu, FindSortedPos(hMenu, text), true, &mi);
}

// find sorted position after the last separator
int FindSortedPos(HMENU hMenu, const char* text)
{
	int pos = -1, nbItems = GetMenuItemCount(hMenu);
#ifdef _WIN32
	wchar_t widetext[4096], widebuf[4096];
	MultiByteToWideChar(CP_UTF8, 0, text, -1, widetext, 4096);
	_locale_t locale = _create_locale(LC_ALL, "");
#else
	char buf[4096] = "";
#endif
	MENUITEMINFO mi = {sizeof(MENUITEMINFO),};
	mi.fMask = MIIM_TYPE;

	for (int i=nbItems-1; i>=0 ; i--)
	{
		GetMenuItemInfo(hMenu, i, true, &mi);
		if (mi.fType == MFT_SEPARATOR)
			break;
#ifdef _WIN32
		GetMenuStringW(hMenu, i, widebuf, 4096, MF_BYPOSITION);
		if (_wcsnicoll_l(widetext, widebuf, 4096, locale) < 0) // setLocale() can break things (atof and comma as a decimal mark) so use temporary locale object
			pos = i;
#else
		GetMenuString(hMenu, i, buf, sizeof(buf), MF_BYPOSITION);
		if (strcasecmp(text, buf) < 0) // not as good as on Win OS, e.g. French "Sélectionner" vs "Supprimer"
			pos = i;
#endif
	}
#ifdef _WIN32
	_free_locale(locale);
#endif
	return pos<0 ? nbItems : pos;
}

HMENU FindMenuItem(HMENU hMenu, int iCmd, int* iPos)
{
	MENUITEMINFO mi={sizeof(MENUITEMINFO),};
	mi.fMask = MIIM_ID | MIIM_SUBMENU;
	for (int i = 0; i < GetMenuItemCount(hMenu); i++)
	{
		GetMenuItemInfo(hMenu, i, true, &mi);
		if (mi.hSubMenu)
		{
			HMENU hSubMenu = FindMenuItem(mi.hSubMenu, iCmd, iPos);
			if (hSubMenu)
				return hSubMenu;
		}
		if (mi.wID == iCmd)
		{
			*iPos = i;
			return hMenu;
		}
	}
	return NULL;
}

void SWSSetMenuText(HMENU hMenu, int iCmd, const char* cText)
{
	int iPos;
	hMenu = FindMenuItem(hMenu, iCmd, &iPos);
	if (hMenu)
	{
		MENUITEMINFO mi={sizeof(MENUITEMINFO),};
		mi.fMask = MIIM_TYPE;
		mi.fType = MFT_STRING;
		mi.dwTypeData = (char*)cText;
		SetMenuItemInfo(hMenu, iPos, true, &mi);
	}
}

int SWSGetMenuPosFromID(HMENU hMenu, UINT id)
{	// Replacement for deprecated windows func GetMenuPosFromID
	MENUITEMINFO mi={sizeof(MENUITEMINFO),};
	mi.fMask = MIIM_ID;
	for (int i = 0; i < GetMenuItemCount(hMenu); i++)
	{
		GetMenuItemInfo(hMenu, i, true, &mi);
		if (mi.wID == id)
			return i;
	}
	return -1;
}


// *************************** MENU CREATION ***************************

// Important: both __LOCALIZE() parameters MUST remain literal strings (see sws_build_sample_langpack tool)
void SWSCreateExtensionsMenu(HMENU hMenu)
{
	if (GetMenuItemCount(hMenu))
		AddToMenu(hMenu, SWS_SEPARATOR, 0);

	AddToMenu(hMenu, __LOCALIZE("About SWS Extension", "sws_ext_menu"), NamedCommandLookup("_SWS_ABOUT"));
	AddToMenu(hMenu, __LOCALIZE("Auto Color/Icon/Layout", "sws_ext_menu"), NamedCommandLookup("_SWSAUTOCOLOR_OPEN"));

	HMENU hAutoRenderSubMenu = CreatePopupMenu();
	AddSubMenu(hMenu, hAutoRenderSubMenu, __LOCALIZE("Autorender", "sws_ext_menu"));
	AddToMenu(hAutoRenderSubMenu, __LOCALIZE("Batch render regions...", "sws_ext_menu"), NamedCommandLookup("_AUTORENDER"));
	AddToMenu(hAutoRenderSubMenu, __LOCALIZE("Edit project metadata...", "sws_ext_menu"), NamedCommandLookup("_AUTORENDER_METADATA"));
	AddToMenu(hAutoRenderSubMenu, __LOCALIZE("Global preferences...", "sws_ext_menu"), NamedCommandLookup("_AUTORENDER_PREFERENCES"));
	AddToMenu(hAutoRenderSubMenu, __LOCALIZE("Open render path", "sws_ext_menu"), NamedCommandLookup("_AUTORENDER_OPEN_RENDER_PATH"));
	AddToMenu(hAutoRenderSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hAutoRenderSubMenu, __LOCALIZE("Show help...", "sws_ext_menu"), NamedCommandLookup("_AUTORENDER_HELP"));

	AddToMenu(hMenu, __LOCALIZE("Command parameters...", "sws_ext_menu"), NamedCommandLookup("_XENAKIOS_SHOW_COMMANDPARAMS"));
	AddToMenu(hMenu, __LOCALIZE("Contextual toolbars...", "sws_ext_menu"), NamedCommandLookup("_BR_CONTEXTUAL_TOOLBARS_PREF"));
	AddToMenu(hMenu, __LOCALIZE("Cue Buss generator", "sws_ext_menu"), NamedCommandLookup("_S&M_SENDS4"));
	AddToMenu(hMenu, __LOCALIZE("Cycle Action editor...", "sws_ext_menu"), NamedCommandLookup("_S&M_CYCLEDITOR"));
	AddToMenu(hMenu, __LOCALIZE("Envelope processor...", "sws_ext_menu"), NamedCommandLookup("_PADRE_ENVPROC"));
//	AddToMenu(hMenu, __LOCALIZE("Fill gaps...", "sws_ext_menu"), NamedCommandLookup("_SWS_AWFILLGAPSADV"));
	AddToMenu(hMenu, __LOCALIZE("Find", "sws_ext_menu"), NamedCommandLookup("_S&M_SHOWFIND"));
	AddToMenu(hMenu, __LOCALIZE("Groove tool...", "sws_ext_menu"), NamedCommandLookup("_FNG_GROOVE_TOOL"));
	AddToMenu(hMenu, __LOCALIZE("Label processor...", "sws_ext_menu"), NamedCommandLookup("_IX_LABEL_PROC"));
	AddToMenu(hMenu, __LOCALIZE("Loudness...", "sws_ext_menu"), NamedCommandLookup("_BR_ANALAYZE_LOUDNESS_DLG"));
	AddToMenu(hMenu, __LOCALIZE("LFO generator...", "sws_ext_menu"), NamedCommandLookup("_PADRE_ENVLFO"));
	AddToMenu(hMenu, __LOCALIZE("Live Configs", "sws_ext_menu"), NamedCommandLookup("_S&M_SHOWMIDILIVE"));

	HMENU hMarkerSubMenu = CreatePopupMenu();
	AddSubMenu(hMenu, hMarkerSubMenu, __LOCALIZE("Marker utilites", "sws_ext_menu"));
	AddToMenu(hMarkerSubMenu, __LOCALIZE("MarkerList", "sws_ext_menu"), NamedCommandLookup("_SWSMARKERLIST1"));
	AddToMenu(hMarkerSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Load marker set...", "sws_ext_menu"), NamedCommandLookup("_SWSMARKERLIST2"));
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Save marker set...", "sws_ext_menu"), NamedCommandLookup("_SWSMARKERLIST3"));
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Delete marker set...", "sws_ext_menu"), NamedCommandLookup("_SWSMARKERLIST4"));
	AddToMenu(hMarkerSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Copy marker set to clipboard", "sws_ext_menu"), NamedCommandLookup("_SWSMARKERLIST5"));
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Paste marker set from clipboard", "sws_ext_menu"), NamedCommandLookup("_SWSMARKERLIST6"));
	AddToMenu(hMarkerSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Reorder marker IDs", "sws_ext_menu"), NamedCommandLookup("_SWSMARKERLIST7"));
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Reorder region IDs", "sws_ext_menu"), NamedCommandLookup("_SWSMARKERLIST8"));
	AddToMenu(hMarkerSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Select next region", "sws_ext_menu"), NamedCommandLookup("_SWS_SELNEXTREG"));
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Select prev region", "sws_ext_menu"), NamedCommandLookup("_SWS_SELPREVREG"));
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Delete all markers", "sws_ext_menu"), NamedCommandLookup("_SWSMARKERLIST9"));
	AddToMenu(hMarkerSubMenu, __LOCALIZE("Delete all regions", "sws_ext_menu"), NamedCommandLookup("_SWSMARKERLIST10"));
	AddToMenu(hMenu, __LOCALIZE("Notes", "sws_ext_menu"), NamedCommandLookup("_S&M_SHOW_NOTES_VIEW"));

	HMENU hPrjMgmtSubMenu = CreatePopupMenu();
	AddSubMenu(hMenu, hPrjMgmtSubMenu, __LOCALIZE("Project Management", "sws_ext_menu"));
	AddToMenu(hPrjMgmtSubMenu, __LOCALIZE("ProjectList", "sws_ext_menu"), NamedCommandLookup("_SWS_PROJLIST_OPEN"));
	AddToMenu(hPrjMgmtSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hPrjMgmtSubMenu, __LOCALIZE("Open projects from list...", "sws_ext_menu"), NamedCommandLookup("_SWS_PROJLISTSOPEN"));
	AddToMenu(hPrjMgmtSubMenu, __LOCALIZE("Save list of open projects...", "sws_ext_menu"), NamedCommandLookup("_SWS_PROJLISTSAVE"));
	AddToMenu(hPrjMgmtSubMenu, __LOCALIZE("Add related project(s)...", "sws_ext_menu"), NamedCommandLookup("_SWS_ADDRELATEDPROJ"));
	AddToMenu(hPrjMgmtSubMenu, __LOCALIZE("Delete related project...", "sws_ext_menu"), NamedCommandLookup("_SWS_DELRELATEDPROJ"));
	AddToMenu(hPrjMgmtSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hPrjMgmtSubMenu, __LOCALIZE("(related projects list)", "sws_ext_menu"), NamedCommandLookup("_SWS_OPENRELATED1"));

	HMENU hPrjSetListMgmtSubMenu = CreatePopupMenu();
	AddSubMenu(hMenu, hPrjSetListMgmtSubMenu, __LOCALIZE("Project Set List", "sws_ext_menu"));
	AddToMenu(hPrjSetListMgmtSubMenu, "Project Set List", NamedCommandLookup("_SWS_PROJSETLIST_OPEN"));
	AddToMenu(hPrjSetListMgmtSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hPrjSetListMgmtSubMenu, __LOCALIZE("Open set list...", "sws_ext_menu"), NamedCommandLookup("_SWS_PROJSETLISTSOPEN"));
	AddToMenu(hPrjSetListMgmtSubMenu, __LOCALIZE("Save set list...", "sws_ext_menu"), NamedCommandLookup("_SWS_PROJSETLISTSAVE"));

	HMENU hStartupSubMenu = CreatePopupMenu();
	AddSubMenu(hMenu, hStartupSubMenu, __LOCALIZE("Startup actions", "sws_ext_menu"));
	AddToMenu(hStartupSubMenu, __LOCALIZE("Set project startup action...", "sws_ext_menu"), NamedCommandLookup("_S&M_SET_PRJ_ACTION"));
	AddToMenu(hStartupSubMenu, __LOCALIZE("Clear project startup action", "sws_ext_menu"), NamedCommandLookup("_S&M_CLR_PRJ_ACTION"));
	AddToMenu(hStartupSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hStartupSubMenu, __LOCALIZE("Set global startup action...", "sws_ext_menu"), NamedCommandLookup("_S&M_SET_STARTUP_ACTION"));
	AddToMenu(hStartupSubMenu, __LOCALIZE("Clear global startup action", "sws_ext_menu"), NamedCommandLookup("_S&M_CLR_STARTUP_ACTION"));
	AddToMenu(hStartupSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hStartupSubMenu, __LOCALIZE("Show project/global startup actions...", "sws_ext_menu"), NamedCommandLookup("_S&M_SHOW_PRJ_ACTION"));

	AddToMenu(hMenu, __LOCALIZE("ReaConsole...", "sws_ext_menu"), NamedCommandLookup("_SWSCONSOLE"));
	AddToMenu(hMenu, __LOCALIZE("Region Playlist", "sws_ext_menu"), NamedCommandLookup("_S&M_SHOW_RGN_PLAYLIST"));
	AddToMenu(hMenu, __LOCALIZE("Resources", "sws_ext_menu"), NamedCommandLookup("_S&M_SHOW_RESOURCES_VIEW"));
	AddToMenu(hMenu, __LOCALIZE("Snapshots", "sws_ext_menu"), NamedCommandLookup("_SWSSNAPSHOT_OPEN"));

	HMENU hTempoSubMenu = CreatePopupMenu();
	AddSubMenu(hMenu, hTempoSubMenu, __LOCALIZE("Tempo", "sws_ext_menu"));
	AddToMenu(hTempoSubMenu, __LOCALIZE("Convert project markers to tempo markers...", "sws_ext_menu"), NamedCommandLookup("_SWS_BRCONVERTMARKERSTOTEMPO"));
	AddToMenu(hTempoSubMenu, __LOCALIZE("Select and adjust tempo markers...", "sws_ext_menu"), NamedCommandLookup("_SWS_BRADJUSTSELTEMPO"));
	AddToMenu(hTempoSubMenu, __LOCALIZE("Randomize tempo markers...", "sws_ext_menu"), NamedCommandLookup("_BR_RANDOMIZE_TEMPO"));
	AddToMenu(hTempoSubMenu, __LOCALIZE("Options for setting tempo marker shape...", "sws_ext_menu"), NamedCommandLookup("_BR_TEMPO_SHAPE_OPTIONS"));
	AddToMenu(hTempoSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hTempoSubMenu, __LOCALIZE("Help...", "sws_ext_menu"), NamedCommandLookup("_BR_TEMPO_HELP_WIKI"));
/* over exceeded by the native track mgr
	AddToMenu(hMenu, __LOCALIZE("Tracklist", "sws_ext_menu"), NamedCommandLookup("_SWSTL_OPEN"));
*/
	AddToMenu(hMenu, __LOCALIZE("Zoom preferences...", "sws_ext_menu"), NamedCommandLookup("_SWS_ZOOMPREFS"));

	AddToMenu(hMenu, SWS_SEPARATOR, 0);
	HMENU hOptionsSubMenu = CreatePopupMenu();
	AddSubMenu(hMenu, hOptionsSubMenu, __LOCALIZE("SWS Options", "sws_ext_menu"));

	AddToMenu(hOptionsSubMenu, __LOCALIZE("Enable auto track coloring", "sws_ext_menu"), NamedCommandLookup("_SWSAUTOCOLOR_ENABLE"));
	AddToMenu(hOptionsSubMenu, __LOCALIZE("Enable auto marker coloring", "sws_ext_menu"), NamedCommandLookup("_S&MAUTOCOLOR_MKR_ENABLE"));
	AddToMenu(hOptionsSubMenu, __LOCALIZE("Enable auto region coloring", "sws_ext_menu"), NamedCommandLookup("_S&MAUTOCOLOR_RGN_ENABLE"));
	AddToMenu(hOptionsSubMenu, __LOCALIZE("Enable auto track icon", "sws_ext_menu"), NamedCommandLookup("_S&MAUTOICON_ENABLE"));
	AddToMenu(hOptionsSubMenu, __LOCALIZE("Enable auto track layout", "sws_ext_menu"), NamedCommandLookup("_S&MAUTOLAYOUT_ENABLE"));
	AddToMenu(hOptionsSubMenu, SWS_SEPARATOR, 0);
	AddToMenu(hOptionsSubMenu, __LOCALIZE("Enable marker actions", "sws_ext_menu"), NamedCommandLookup("_SWSMA_TOGGLE"));
	AddToMenu(hOptionsSubMenu, __LOCALIZE("Enable record input check", "sws_ext_menu"), NamedCommandLookup("_SWS_TOGRECINCHECK"));
	AddToMenu(hOptionsSubMenu, __LOCALIZE("Enable red ruler while recording", "sws_ext_menu"), NamedCommandLookup("_SWS_RECREDRULER"));
	AddToMenu(hOptionsSubMenu, __LOCALIZE("Enable toolbars auto refresh", "sws_ext_menu"), NamedCommandLookup("_S&M_TOOLBAR_REFRESH_ENABLE"));
}

