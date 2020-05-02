/******************************************************************************
/ Autocolor.cpp
/
/ Copyright (c) 2010-2016 Tim Payne (SWS), Jeffos
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
#include "Color.h"
#include "Autocolor.h"
#include "../SnM/SnM_ChunkParserPatcher.h"
#include "../SnM/SnM_Dlg.h"
#include "../SnM/SnM_Marker.h"
#include "../SnM/SnM_Util.h"
#include "../reaper/localize.h"
#include "WDL/projectcontext.h"
#include <regex>

#define PRI_UP_MSG         0x10000
#define PRI_DOWN_MSG       0x10001
#define TYPETYPE_MSG       0x100F0
#define FILTERTYPE_MSG     0x10100
#define COLORTYPE_MSG      0x10110
#define LOAD_ICON_MSG      0x110000
#define CLEAR_ICON_MSG     0x110001
#define LAYOUTTYPE_TCP_MSG 0x110002
#define LAYOUTTYPE_MCP_MSG 0x110003


// INI params
#define AC_ENABLE_KEY  "AutoColorEnable"
#define ACM_ENABLE_KEY "AutoColorMarkerEnable"
#define ACR_ENABLE_KEY "AutoColorRegionEnable"
#define AI_ENABLE_KEY  "AutoIconEnable"
#define AL_ENABLE_KEY  "AutoLayoutEnable"
#define AC_COUNT_KEY   "AutoColorCount"
#define AC_ITEM_KEY    "AutoColor %d"


enum { AC_ANY=0, AC_UNNAMED, AC_FOLDER, AC_CHILDREN, AC_RECEIVE, AC_MASTER, AC_REC_ARM, AC_VCA_MASTER, NUM_FILTERTYPES };
enum { AC_RGNANY=0, AC_RGNUNNAMED, NUM_RGNFILTERTYPES };
enum { AC_CUSTOM, AC_GRADIENT, AC_RANDOM, AC_NONE, AC_PARENT, AC_IGNORE, NUM_COLORTYPES };
enum { COL_ID=0, COL_TYPE, COL_FILTER, COL_COLOR, COL_ICON, COL_TCP_LAYOUT, COL_MCP_LAYOUT, COL_COUNT };
enum { AC_TRACK=0, AC_MARKER, AC_REGION, NUM_TYPETYPES }; // keep this order and 2^ values
                                                          // (values used as masks => adding a 4th type would require another solution)
enum { AC_HIDE_TCP=0, NUM_TCP_LAYOUTTYPES };
enum { AC_HIDE_MCP=0, NUM_MCP_LAYOUTTYPES };

// Larger allocs for localized strings..
// !WANT_LOCALIZE_STRINGS_BEGIN:sws_DLG_115
static SWS_LVColumn g_cols[] = { {25, 0, "#" }, {25, 0, "Rule type"}, { 185, 1, "Filter" }, { 70, 1, "Color" }, { 200, 2, "Icon" }, { 100, 1, "TCP Layout" }, { 100, 1, "MCP Layout" }};
static const char cTypes[][256] = {"Track", "Marker", "Region" }; // keep this order, see above
static const char cFilterTypes[][256] = { "(any)", "(unnamed)", "(folder)", "(children)", "(receive)", "(master)", "(record armed)", "(vca master)" };
static const char cColorTypes[][256] = { "Custom", "Gradient", "Random", "None", "Parent", "Ignore" };
static const char cLayoutTypes[][256] = { "(hide)" }; // #1008, additional '(hide)' layout, same for TCP and MCP
// !WANT_LOCALIZE_STRINGS_END


// Globals
SWS_AutoColorWnd* g_pACWnd = NULL;
static WDL_PtrList<SWS_RuleItem> g_pACItems;
static SWSProjConfig<WDL_PtrList<SWS_RuleTrack> > g_pACTracks;
static bool g_bACEnabled = false;
static bool g_bACREnabled = false;
static bool g_bACMEnabled = false;
static bool g_bAIEnabled = false;
static bool g_bALEnabled = false;
static WDL_String g_ACIni;
static int s_ignore_update;
static const char g_cRegexPrefix = '$'; // Filter should start with that prefix to be treated as regex
static const auto g_bmRegexOptions = std::regex_constants::extended | std::regex_constants::icase | std::regex_constants::collate;


// Register to marker/region updates
class AC_MarkerRegionListener : public SNM_MarkerRegionListener {
public:
	AC_MarkerRegionListener() : SNM_MarkerRegionListener() {}
	void NotifyMarkerRegionUpdate(int _updateFlags) { AutoColorMarkerRegion(false, _updateFlags); }
};

AC_MarkerRegionListener g_mkrRgnListener;

// Optimized/custom version of RegisterToMarkerRegionUpdates()
// (to avoid useless polling behind the scene)
bool ACRegisterUnregisterToMarkerRegionUpdates()
{
	if (g_bACMEnabled || g_bACREnabled)
		for (int i = 0; i < g_pACItems.GetSize(); i++)
			if (SWS_RuleItem* rule = (SWS_RuleItem*)g_pACItems.Get(i))
				if ((g_bACMEnabled && rule->m_type == AC_MARKER) || (g_bACREnabled && rule->m_type == AC_REGION)) {
					RegisterToMarkerRegionUpdates(&g_mkrRgnListener);
					return true; // do not use the above returned value
				}
	UnregisterToMarkerRegionUpdates(&g_mkrRgnListener);
	return false; // do not use the above returned value
}

// Prototypes
void AutoColorSaveState();


SWS_AutoColorView::SWS_AutoColorView(HWND hwndList, HWND hwndEdit)
:SWS_ListView(hwndList, hwndEdit, COL_COUNT, g_cols, "AutoColorViewState", false, "sws_DLG_115")
{
}

void SWS_AutoColorView::GetItemText(SWS_ListItem* item, int iCol, char* str, int iStrMax)
{
	if (str)
		*str = '\0';

	SWS_RuleItem* pItem = (SWS_RuleItem*)item;
	if (!pItem)
		return;

	switch (iCol)
	{
	case COL_ID:
		snprintf(str, iStrMax, "%d", g_pACItems.Find(pItem) + 1);
		break;
	case COL_TYPE:
		lstrcpyn(str, __localizeFunc(cTypes[pItem->m_type],"sws_DLG_115",LOCALIZE_FLAG_NOCACHE), iStrMax);
		break;
	case COL_FILTER:
		{
			// internal str? => localize for display
			for (int i=0; i<NUM_FILTERTYPES; i++)
				if (!strcmp(pItem->m_str_filter.Get(), cFilterTypes[i])) {
					lstrcpyn(str, __localizeFunc(pItem->m_str_filter.Get(),"sws_DLG_115",LOCALIZE_FLAG_NOCACHE), iStrMax);
					return;
				}
			// user defined?
			lstrcpyn(str, pItem->m_str_filter.Get(), iStrMax);
		}
		break;
	case COL_COLOR:
		if (pItem->m_color<0 && (-pItem->m_color-1) < __ARRAY_SIZE(cColorTypes))
			lstrcpyn(str, __localizeFunc(cColorTypes[-pItem->m_color-1],"sws_DLG_115",LOCALIZE_FLAG_NOCACHE), iStrMax);
		else
#ifdef _WIN32
			snprintf(str, iStrMax, "0x%02x%02x%02x", pItem->m_color & 0xFF, (pItem->m_color >> 8) & 0xFF, (pItem->m_color >> 16) & 0xFF); //Brado: I think this is in reverse. Ini file dec to hex conversion (correct value for color) shows opposite of what this outputs to screen. e.g. 0xFFFF80 instead of 0x80FFFF
#else
			snprintf(str, iStrMax, "0x%06x", pItem->m_color);
#endif
		break;
	case COL_ICON:
		if (pItem->m_type == AC_TRACK)
			lstrcpyn(str, pItem->m_icon.Get(), iStrMax);
		break;
		case COL_TCP_LAYOUT:
			if (pItem->m_type == AC_TRACK)
				lstrcpyn(str, pItem->m_layout[0].Get(), iStrMax);
			break;
		case COL_MCP_LAYOUT:
			if (pItem->m_type == AC_TRACK)
				lstrcpyn(str, pItem->m_layout[1].Get(), iStrMax);
			break;
	}
}

//JFB: COL_TYPE must be edited via context menu, not by hand (prevent internal/localized strings mismatch)
void SWS_AutoColorView::SetItemText(SWS_ListItem* item, int iCol, const char* str)
{
	SWS_RuleItem* pItem = (SWS_RuleItem*)item;
	if (!pItem)
		return;

	switch(iCol)
	{
			case COL_FILTER:
				for (int i = 0; i < g_pACItems.GetSize(); i++)
					if (g_pACItems.Get(i) != pItem)
						if (g_pACItems.Get(i)->m_type == pItem->m_type && !strcmp(g_pACItems.Get(i)->m_str_filter.Get(), str)){
							MessageBox(GetParent(m_hwndList), __LOCALIZE("Autocolor entry with that name already exists.","sws_DLG_115"), __LOCALIZE("SWS - Error","sws_DLG_115"), MB_OK);
							return;
						}
				if (strlen(str))
					pItem->m_str_filter.Set(str);
				break;
			case COL_COLOR:
			{
				int iNewCol = strtol(str, NULL, 0);
				pItem->m_color = RGB((iNewCol >> 16) & 0xFF, (iNewCol >> 8) & 0xFF, iNewCol & 0xFF);
			}
			break;
			case COL_TCP_LAYOUT:
				pItem->m_layout[0].Set(str);
				break;
			case COL_MCP_LAYOUT:
				pItem->m_layout[1].Set(str);
				break;
	}
	g_pACWnd->Update();
}

void SWS_AutoColorView::GetItemList(SWS_ListItemList* pList)
{
	for (int i = 0; i < g_pACItems.GetSize(); i++)
		pList->Add((SWS_ListItem*)g_pACItems.Get(i));
}

void SWS_AutoColorView::OnItemDblClk(SWS_ListItem* item, int iCol)
{
	SWS_RuleItem* pItem = (SWS_RuleItem*)item;
	if (pItem && iCol == COL_ICON)
		g_pACWnd->OnCommand(LOAD_ICON_MSG, (LPARAM)pItem);
}

void SWS_AutoColorView::OnItemSelChanged(SWS_ListItem* item, int iState)
{
	g_pACWnd->Update(false);
}

void SWS_AutoColorView::OnBeginDrag(SWS_ListItem* item)
{
	if (abs(m_iSortCol) == (COL_ID+1)) //1-based
		SetCapture(GetParent(m_hwndList));
}

void SWS_AutoColorView::OnDrag()
{
	POINT p;
	GetCursorPos(&p);
	SWS_RuleItem* hitItem = (SWS_RuleItem*)GetHitItem(p.x, p.y, NULL);
	if (hitItem)
	{
		int iNewPriority = g_pACItems.Find(hitItem);
		int iSelPriority;

		WDL_PtrList<SWS_RuleItem> draggedItems;
		int x = 0;
		SWS_RuleItem* selItem;
		while((selItem = (SWS_RuleItem*)EnumSelected(&x)))
		{
			iSelPriority = g_pACItems.Find(selItem);
			if (iNewPriority == iSelPriority)
				return;
			draggedItems.Add(selItem);
		}

		// Remove the dragged items and then readd them
		// Switch order of add based on direction of drag & sort order
		bool bDir = iNewPriority > iSelPriority;
		if (m_iSortCol < 0)
			bDir = !bDir;
		for (int i = bDir ? 0 : draggedItems.GetSize()-1; bDir ? i < draggedItems.GetSize() : i >= 0; bDir ? i++ : i--)
		{
			int index = g_pACItems.Find(draggedItems.Get(i));
			g_pACItems.Delete(index);
			g_pACItems.Insert(iNewPriority, draggedItems.Get(i));
		}

		g_pACWnd->Update(false);

		++s_ignore_update;
		for (int i=0; i < draggedItems.GetSize(); i++)
			SelectByItem((SWS_ListItem*)draggedItems.Get(i), i==0, i==0);
		--s_ignore_update;
	}
}

void SWS_AutoColorView::OnEndDrag()
{
	g_pACWnd->Update();
}

SWS_AutoColorWnd::SWS_AutoColorWnd()
:SWS_DockWnd(IDD_AUTOCOLOR, __LOCALIZE("Auto Color/Icon/Layout","sws_DLG_115"), "SWSAutoColor")
#ifdef __APPLE__
	,m_bSettingColor(false)
#endif
{
	// Must call SWS_DockWnd::Init() to restore parameters and open the window if necessary
	Init();
}

void SWS_AutoColorWnd::Update(bool applyrules)
{
	if (s_ignore_update) return;

	if (IsValidWindow())
	{
		SetDlgItemText(m_hwnd, IDC_APPLY, (g_bACEnabled||g_bACMEnabled||g_bACREnabled||g_bAIEnabled||g_bALEnabled) ? __LOCALIZE("Force","sws_DLG_115") : __LOCALIZE("Apply","sws_DLG_115"));

		// Redraw the owner drawn button
#ifdef _WIN32
		RedrawWindow(GetDlgItem(m_hwnd, IDC_COLOR), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
#else
		InvalidateRect(GetDlgItem(m_hwnd, IDC_COLOR), NULL, 0);
#endif

		if (m_pLists.GetSize())
			m_pLists.Get(0)->Update();
	}

	if (applyrules)
	{
		AutoColorSaveState();
		AutoColorTrack(false);
		if (ACRegisterUnregisterToMarkerRegionUpdates())
			AutoColorMarkerRegion(false);
  }
}

void SWS_AutoColorWnd::OnInitDlg()
{
	m_resize.init_item(IDC_LIST,      0.0, 0.0, 1.0, 1.0);
	m_resize.init_item(IDC_ADD,       0.0, 1.0, 0.0, 1.0);
	m_resize.init_item(IDC_REMOVE,    0.0, 1.0, 0.0, 1.0);
	m_resize.init_item(IDC_COLOR,     0.0, 1.0, 0.0, 1.0);
	m_resize.init_item(IDC_OPTIONS,   1.0, 1.0, 1.0, 1.0);
	m_resize.init_item(IDC_APPLY,     1.0, 1.0, 1.0, 1.0);
	m_pView = new SWS_AutoColorView(GetDlgItem(m_hwnd, IDC_LIST), GetDlgItem(m_hwnd, IDC_EDIT));
	m_pLists.Add(m_pView);
	Update();
}

void SWS_AutoColorWnd::GetMinSize(int* w, int* h)
{
  *w=350; *h=MIN_DOCKWND_HEIGHT;
}

//JFB TODO? handle multi-selection
void SWS_AutoColorWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
		case IDC_APPLY:
			AutoColorTrack(true);
			AutoColorMarkerRegion(true);
			break;
		case IDC_OPTIONS:
		{
			HWND hwnd = GetDlgItem(m_hwnd, IDC_OPTIONS);
			RECT r;  GetClientRect(hwnd, &r);
			ClientToScreen(hwnd, (LPPOINT)&r);
			ClientToScreen(hwnd, ((LPPOINT)&r)+1);
			SendMessage(m_hwnd, WM_CONTEXTMENU, 0, MAKELPARAM((UINT)(r.left), (UINT)(r.bottom+SNM_1PIXEL_Y)));
			break;
		}
		case IDC_ADD:
			// default options when we add a new row
			g_pACItems.Add(new SWS_RuleItem(AC_TRACK, __LOCALIZE("(name)","sws_DLG_115"), -AC_NONE-1, "", "", ""));
			Update();
			break;
		case IDC_REMOVE:
		{
			int x = 0;
			SWS_RuleItem* item;
			while((item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(&x)))
			{
				int idx = g_pACItems.Find(item);
				if (idx >= 0)
					g_pACItems.Delete(idx);
			}
			Update();
			break;
		}
		case IDC_COLOR:
		{
			SWS_RuleItem* item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(NULL);
			if (item)
			{
				// Display the color picker
#ifdef _WIN32
				CHOOSECOLOR cc;
				memset(&cc, 0, sizeof(CHOOSECOLOR));
				cc.lStructSize = sizeof(CHOOSECOLOR);
				cc.hwndOwner = m_hwnd;
				UpdateCustomColors();
				cc.lpCustColors = g_custColors;
				cc.Flags = CC_FULLOPEN | CC_RGBINIT;
				cc.rgbResult = item->m_color;
				if (ChooseColor(&cc))
				{
					int x = 0;
					while ((item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(&x)))
						item->m_color = cc.rgbResult & 0xFFFFFF; // fix for issue 600
				}
				Update();
#elif defined(__APPLE__)
				m_bSettingColor = true;
				ShowColorChooser(item->m_color);
				SetTimer(m_hwnd, 1, 50, NULL);
#else
				UpdateCustomColors();

				int cc = item->m_color;
				if (SWELL_ChooseColor(m_hwnd,&cc,16,(int *)g_custColors))
				{
					int x = 0;
					while ((item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(&x)))
						item->m_color = cc & 0xFFFFFF;
				}
				Update();
#endif
			}
			break;
		}
		case PRI_UP_MSG:
		{
			int x = 0;
			SWS_RuleItem* item;
			while ((item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(&x)))
			{
				int iPos = g_pACItems.Find(item);
				if (iPos <= 0)
					break;
				g_pACItems.Delete(iPos, false);
				g_pACItems.Insert(iPos-1, item);
			}
			Update();
			break;
		}
		case PRI_DOWN_MSG:
		{
			// Go in reverse order
			for (int i = m_pLists.Get(0)->GetListItemCount()-1; i >= 0; i--)
			{
				if (m_pLists.Get(0)->IsSelected(i))
				{
					SWS_RuleItem* item = (SWS_RuleItem*)m_pLists.Get(0)->GetListItem(i);
					int iPos = g_pACItems.Find(item);
					if (iPos < 0)
						break;
					g_pACItems.Delete(iPos, false);
					g_pACItems.Insert(iPos+1, item);
				}
			}
			Update();
			break;
		}
		case LOAD_ICON_MSG:
		{
			SWS_RuleItem* item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(NULL);
			if (item)
			{
				char filename[SNM_MAX_PATH];
				if (BrowseResourcePath(__LOCALIZE("Load icon","sws_DLG_115"), "Data" WDL_DIRCHAR_STR "track_icons" , "PNG files (*.PNG)\0*.PNG\0ICO files (*.ICO)\0*.ICO\0JPEG files (*.JPG)\0*.JPG\0BMP files (*.BMP)\0*.BMP\0PCX files (*.PCX)\0*.PCX\0",
					filename, sizeof(filename)))
				{
					item->m_icon.Set(filename);
				}
			}
			Update();
			break;
		}
		case CLEAR_ICON_MSG:
		{
			SWS_RuleItem* item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(NULL);
			if (item)
				item->m_icon.Set("");
			Update();
			break;
		}

		default:
			if (wParam >= TYPETYPE_MSG && wParam < TYPETYPE_MSG + NUM_TYPETYPES)
			{
				SWS_RuleItem* item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(NULL);
				if (item)
					item->m_type = (int)wParam-TYPETYPE_MSG;
				Update();
			}
			else if (wParam >= FILTERTYPE_MSG && wParam < FILTERTYPE_MSG + NUM_FILTERTYPES)
			{
				SWS_RuleItem* item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(NULL);
				if (item)
				{
					int iType = (int)wParam - FILTERTYPE_MSG;
					for (int i = 0; i < g_pACItems.GetSize(); i++)
						if (g_pACItems.Get(i) != item)
							if (g_pACItems.Get(i)->m_type == item->m_type && !strcmp(g_pACItems.Get(i)->m_str_filter.Get(), cFilterTypes[iType])) {
								MessageBox(m_hwnd, __LOCALIZE("Autocolor entry of that type already exists.","sws_DLG_115"), __LOCALIZE("SWS - Error","sws_DLG_115"), MB_OK);
								return;
							}
					item->m_str_filter.Set(cFilterTypes[wParam-FILTERTYPE_MSG]);
				}
				Update();
			}
			else if (wParam >= COLORTYPE_MSG && wParam < COLORTYPE_MSG + NUM_COLORTYPES)
			{
				int x = 0;
				SWS_RuleItem* item;
				while ((item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(&x)))
					item->m_color = -1 - ((int)wParam - COLORTYPE_MSG);
				Update();
			}
			else if (wParam >= LAYOUTTYPE_TCP_MSG && wParam < LAYOUTTYPE_TCP_MSG + NUM_TCP_LAYOUTTYPES)
			{
				SWS_RuleItem* item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(NULL);
				if (item)
					item->m_layout[0].Set(cLayoutTypes[wParam - LAYOUTTYPE_TCP_MSG]);
				Update();
			}
			else if (wParam >= LAYOUTTYPE_MCP_MSG && wParam < LAYOUTTYPE_MCP_MSG + NUM_MCP_LAYOUTTYPES)
			{
				SWS_RuleItem* item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(NULL);
				if (item)
					item->m_layout[1].Set(cLayoutTypes[wParam - LAYOUTTYPE_MCP_MSG]);
				Update();
			}
			else
				Main_OnCommand((int)wParam, (int)lParam);
	}
}

#ifdef __APPLE__
void SWS_AutoColorWnd::OnTimer(WPARAM wParam)
{
	COLORREF cr;
	if (m_bSettingColor && GetChosenColor(&cr))
	{
		int x = 0;
		SWS_RuleItem* item;
		while ((item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(&x)))
			item->m_color = cr;

		KillTimer(m_hwnd, 1);
		Update();
	}
}

void SWS_AutoColorWnd::OnDestroy()
{
	if (m_bSettingColor)
	{
		HideColorChooser();
		KillTimer(m_hwnd, 1);
	}
}
#endif

INT_PTR SWS_AutoColorWnd::OnUnhandledMsg(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_DRAWITEM)
	{
		LPDRAWITEMSTRUCT pDI = (LPDRAWITEMSTRUCT)lParam;
		if (pDI->CtlID == IDC_COLOR)
		{
			// Dialog-box background grey for no selection, or multiple colors in selection
			// Doesn't account for the "special colors" because they're less than zero and get trapped
			// out below anyway.
			int col = -1;
			int x = 0;
			SWS_RuleItem* item;
			while ((item = (SWS_RuleItem*)m_pLists.Get(0)->EnumSelected(&x)))
			{
				if (col < 0)
					col = item->m_color;
				else if (col != item->m_color)
				{
					col = -1;
					break;
				}
			}

			if (col < 0)
				col = GetSysColor(COLOR_3DHILIGHT);

			HBRUSH hb = CreateSolidBrush(col);
			FillRect(pDI->hDC, &pDI->rcItem, hb);
			DeleteObject(hb);
			return 1;
		}
	}
	return 0;
}

// must remain in sync with main menu > extensions > sws options
void SWS_AutoColorWnd::AddOptionsMenu(HMENU _menu)
{
	if (GetMenuItemCount(_menu))
		AddToMenu(_menu, SWS_SEPARATOR, 0);
	AddToMenu(_menu, __LOCALIZE("Enable auto track coloring", "sws_ext_menu"), NamedCommandLookup("_SWSAUTOCOLOR_ENABLE"), -1, false, g_bACEnabled ?  MF_CHECKED : MF_UNCHECKED);
	AddToMenu(_menu, __LOCALIZE("Enable auto marker coloring", "sws_ext_menu"), NamedCommandLookup("_S&MAUTOCOLOR_MKR_ENABLE"), -1, false, g_bACMEnabled ?  MF_CHECKED : MF_UNCHECKED);
	AddToMenu(_menu, __LOCALIZE("Enable auto region coloring", "sws_ext_menu"), NamedCommandLookup("_S&MAUTOCOLOR_RGN_ENABLE"), -1, false, g_bACREnabled ?  MF_CHECKED : MF_UNCHECKED);
	AddToMenu(_menu, __LOCALIZE("Enable auto track icon", "sws_ext_menu"), NamedCommandLookup("_S&MAUTOICON_ENABLE"), -1, false, g_bAIEnabled ?  MF_CHECKED : MF_UNCHECKED);
	AddToMenu(_menu, __LOCALIZE("Enable auto track layout", "sws_ext_menu"), NamedCommandLookup("_S&MAUTOLAYOUT_ENABLE"), -1, false, g_bALEnabled ?  MF_CHECKED : MF_UNCHECKED);
}

HMENU SWS_AutoColorWnd::OnContextMenu(int x, int y, bool* wantDefaultItems)
{
	HMENU hMenu = CreatePopupMenu();

	// specific context menu for the button "options"
	{
		POINT pt = {x, y + 3*(SNM_1PIXEL_Y*(-1))}; // +/- 3: tiny trick, see OnCommand()
		RECT r;	GetWindowRect(GetDlgItem(m_hwnd, IDC_OPTIONS), &r);
		if (PtInRect(&r, pt))
		{
			*wantDefaultItems = false;
			AddOptionsMenu(hMenu);
			return hMenu;
		}
	}

	// "standard" context menu
	int iCol;
	if (SWS_RuleItem* item = (SWS_RuleItem*)m_pLists.Get(0)->GetHitItem(x, y, &iCol))
	{
		*wantDefaultItems = false;

		switch (iCol)
		{
			case COL_TYPE:
			{
				for (int i = 0; i < NUM_TYPETYPES; i++)
					AddToMenu(hMenu, __localizeFunc(cTypes[i],"sws_DLG_115",LOCALIZE_FLAG_NOCACHE), TYPETYPE_MSG + i);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				break;
			}
			case COL_FILTER:
			{
				int numFilters = 0;
				if (item->m_type == AC_TRACK)
					numFilters= NUM_FILTERTYPES;
				else if (item->m_type == AC_MARKER || item->m_type == AC_REGION)
					numFilters = NUM_RGNFILTERTYPES;
				if (numFilters)
				{
					for (int i=0; i<numFilters; i++)
						AddToMenu(hMenu, __localizeFunc(cFilterTypes[i],"sws_DLG_115",LOCALIZE_FLAG_NOCACHE), FILTERTYPE_MSG + i);
					AddToMenu(hMenu, SWS_SEPARATOR, 0);
				}
				break;
			}
			case COL_COLOR:
			{
				AddToMenu(hMenu, __LOCALIZE("Set color...","sws_DLG_115"), IDC_COLOR);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);

				if (item->m_type == AC_TRACK)
				{
					for (int i = 0; i < NUM_COLORTYPES; i++)
						AddToMenu(hMenu, __localizeFunc(cColorTypes[i],"sws_DLG_115",LOCALIZE_FLAG_NOCACHE), COLORTYPE_MSG + i);
				}
				else if (item->m_type == AC_MARKER || item->m_type == AC_REGION)
					AddToMenu(hMenu, __localizeFunc(cColorTypes[AC_NONE],"sws_DLG_115",LOCALIZE_FLAG_NOCACHE), COLORTYPE_MSG + AC_NONE);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				break;
			}
			case COL_ICON:
			{
				AddToMenu(hMenu, __LOCALIZE("Load icon...","sws_DLG_115"), LOAD_ICON_MSG, -1, false, item->m_type == AC_TRACK ? MF_ENABLED : MF_GRAYED);
				AddToMenu(hMenu, __LOCALIZE("Clear icon","sws_DLG_115"), CLEAR_ICON_MSG, -1, false, item->m_type == AC_TRACK ? MF_ENABLED : MF_GRAYED);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				break;
			}
			case COL_TCP_LAYOUT:
				for (int i = 0; i < NUM_TCP_LAYOUTTYPES; i++)
					AddToMenu(hMenu, __localizeFunc(cLayoutTypes[i], "sws_DLG_115", LOCALIZE_FLAG_NOCACHE), LAYOUTTYPE_TCP_MSG + i);

				AddToMenu(hMenu, __LOCALIZE("(Double-click to edit layout name)", "sws_DLG_115"), 0, -1, false, MF_GRAYED);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				break;
			case COL_MCP_LAYOUT:
				for (int i = 0; i < NUM_MCP_LAYOUTTYPES; i++)
					AddToMenu(hMenu, __localizeFunc(cLayoutTypes[i], "sws_DLG_115", LOCALIZE_FLAG_NOCACHE), LAYOUTTYPE_MCP_MSG + i);
				
				AddToMenu(hMenu, __LOCALIZE("(Double-click to edit layout name)","sws_DLG_115"), 0, -1, false, MF_GRAYED);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				break;
		}
		AddToMenu(hMenu, __LOCALIZE("Up in priority","sws_DLG_115"), PRI_UP_MSG);
		AddToMenu(hMenu, __LOCALIZE("Down in priority","sws_DLG_115"), PRI_DOWN_MSG);
	}
	else
	{
		AddToMenu(hMenu, __LOCALIZE("Show color management window","sws_DLG_115"), SWSGetCommandID(ShowColorDialog));
		AddToMenu(hMenu, SWS_SEPARATOR, 0);

		HMENU hOptionsSubMenu = CreatePopupMenu();
		AddSubMenu(hMenu, hOptionsSubMenu, __LOCALIZE("Options", "sws_menu"));
		AddOptionsMenu(hOptionsSubMenu);
	}
	return hMenu;
}

int SWS_AutoColorWnd::OnKey(MSG* msg, int iKeyState)
{
	if (msg->message == WM_KEYDOWN && msg->wParam == VK_DELETE && !iKeyState)
	{
		OnCommand(IDC_REMOVE, 0);
		return 1;
	}
	return 0;
}

void OpenAutoColor(COMMAND_T*)
{
	g_pACWnd->Show(true, true);
}

void ApplyColorRuleToTrack(SWS_RuleItem* rule, bool bDoColors, bool bDoIcons, bool bDoLayout, bool bForce)
{
	if(rule->m_type == AC_TRACK)
	{
		if (!bDoColors && !bDoIcons && !bDoLayout) // NF: fix #936
			return;

		PreventUIRefresh(1);

		int iCount = 0;
		WDL_PtrList<void> gradientTracks;

		if (rule->m_color == -AC_CUSTOM-1)
			UpdateCustomColors();

		// Check all tracks for matching strings/properties
		MediaTrack* temp = NULL;
		for (int i = 0; i <= GetNumTracks(); i++)
		{
			MediaTrack* tr = CSurf_TrackFromID(i, false);
			bool bColor = bDoColors;
			bool bIcon  = bDoIcons;
			bool bLayout[2];
			bLayout[0]=bLayout[1]=bDoLayout;

			bool bFound = false;
			SWS_RuleTrack* pACTrack = NULL;
			for (int j = 0; j < g_pACTracks.Get()->GetSize(); j++)
			{
				pACTrack = g_pACTracks.Get()->Get(j);
				if (pACTrack->m_pTr == tr)
				{
					bFound = true;
					break;
				}
			}

			if (bFound)
			{
				// If already modified by a different rule, or ignoring the color/icon/layout ignore this track
				if (pACTrack->m_bColored || rule->m_color == -AC_IGNORE-1)
					bColor = false;

				if (pACTrack->m_bIconed || !rule->m_icon.Get()[0])
					bIcon = false;

				for (int k=0; k<2; k++)
					if (pACTrack->m_bLayouted[k] || !rule->m_layout[k].Get()[0])
						bLayout[k] = false;
			}
			else
				pACTrack = g_pACTracks.Get()->Add(new SWS_RuleTrack(tr));

			// Do the track rule matching
			if (bColor || bIcon || bLayout[0] || bLayout[1])
			{
				bool bMatch = false;

				if (i) // ignore master for most things
				{
					// Check "special" rules first:
					if (strcmp(rule->m_str_filter.Get(), cFilterTypes[AC_FOLDER]) == 0)
					{
						int iType;
						GetFolderDepth(tr, &iType, &temp);
						if (iType == 1)
							bMatch = true;
					}
					else if (strcmp(rule->m_str_filter.Get(), cFilterTypes[AC_CHILDREN]) == 0)
					{
						temp = CSurf_TrackFromID(0, false); // JFB fix: 'temp' could be out of sync
						if (GetFolderDepth(tr, NULL, &temp) >= 1)
							bMatch = true;
					}
					else if (strcmp(rule->m_str_filter.Get(), cFilterTypes[AC_RECEIVE]) == 0)
					{
						if (GetSetTrackSendInfo(tr, -1, 0, "P_SRCTRACK", NULL))
							bMatch = true;
					}
					else if (strcmp(rule->m_str_filter.Get(), cFilterTypes[AC_UNNAMED]) == 0)
					{
						char* cName = (char*)GetSetMediaTrackInfo(tr, "P_NAME", NULL);
						if (!cName || !cName[0])
							bMatch = true;
					}
					else if (strcmp(rule->m_str_filter.Get(), cFilterTypes[AC_REC_ARM]) == 0)
					{
						int* ra = (int*)GetSetMediaTrackInfo(tr, "I_RECARM", NULL);
						if (ra && *ra)
							bMatch = true;
					}
					else if (strcmp(rule->m_str_filter.Get(), cFilterTypes[AC_VCA_MASTER]) == 0)
					{
						int iVcaMaster = GetSetTrackGroupMembership(tr, "VOLUME_VCA_MASTER", 0, 0);

						// check newly added groups 33 - 64
						int iVcaMasterHigh = GetSetTrackGroupMembershipHigh(tr, "VOLUME_VCA_MASTER", 0, 0);

						if (iVcaMaster || iVcaMasterHigh)
							bMatch = true;
					}
					else if (strcmp(rule->m_str_filter.Get(), cFilterTypes[AC_ANY]) == 0)
					{
						bMatch = true;
					}
					else // Check for name match
					{
						char* cName = (char*)GetSetMediaTrackInfo(tr, "P_NAME", NULL);
						if (cName){
							if (stristr(cName, rule->m_str_filter.Get())){
								// Exact match
								bMatch = true;
							}
							else if (strlen(rule->m_str_filter.Get()) > 1 && (rule->m_str_filter.Get())[0] == g_cRegexPrefix){
								// Regex pattern match
								const char* sUserPattern = (char*) rule->m_str_filter.Get() + 1; // Chopping the prefix
								std::regex reUserPattern(sUserPattern, g_bmRegexOptions);

								if (std::regex_match(cName, reUserPattern))
									bMatch = true;
							}
						}
					}
				}
				else if (strcmp(rule->m_str_filter.Get(), cFilterTypes[AC_MASTER]) == 0)
				{	// Check master rule
					bMatch = true;
				}

				if (bMatch)
				{
					// Set the color
					if (bColor)
					{
						int iCurColor = *(int*)GetSetMediaTrackInfo(tr, "I_CUSTOMCOLOR", NULL);
						if (!(iCurColor & 0x1000000))
							iCurColor = 0;
						int newCol = iCurColor;

						if (rule->m_color == -AC_RANDOM-1)
						{
							// Only randomize once
							if (!(iCurColor & 0x1000000))
								newCol = RGB(rand() % 256, rand() % 256, rand() % 256) | 0x1000000;
						}
						else if (rule->m_color == -AC_CUSTOM-1)
						{
							if (!AllBlack())
								while(!(newCol = g_custColors[iCount++ % 16]));
							newCol |= 0x1000000;
						}
						else if (rule->m_color == -AC_GRADIENT-1)
							gradientTracks.Add(tr);
						else if (rule->m_color == -AC_NONE-1)
							newCol = 0;
						else if (rule->m_color == -AC_PARENT-1)
						{
							MediaTrack* parent = (MediaTrack*)GetSetMediaTrackInfo(tr, "P_PARTRACK", NULL);
							if (parent)
							{
								int pcol = *(int*)GetSetMediaTrackInfo(parent, "I_CUSTOMCOLOR", NULL);
								if (pcol & 0x1000000) // Only color like parent if the parent has color (maybe not?)
									newCol = pcol;
							}
						}
						else
							newCol = rule->m_color | 0x1000000;

						// Only set the color if the user hasn't changed the color manually (but record it as being changed)
						if ((bForce || iCurColor == pACTrack->m_col) && newCol != iCurColor)
						{
							GetSetMediaTrackInfo(tr, "I_CUSTOMCOLOR", &newCol);
						}

						pACTrack->m_col = newCol;
						pACTrack->m_bColored = true;
					}

					if (bIcon)
					{
						if (_stricmp(rule->m_icon.Get(), pACTrack->m_icon.Get()))
						{
							const char *cur = (const char*)GetSetMediaTrackInfo(tr, "P_ICON", NULL); // requires REAPER v5.15pre6+
							cur = GetShortResourcePath("Data" WDL_DIRCHAR_STR "track_icons", cur);
							if (cur && _stricmp(cur, rule->m_icon.Get()))
							{
								// Only overwrite the icon if there's no icon, or we're forcing, or we set it ourselves earlier
								if (bForce || !_stricmp(cur, pACTrack->m_icon.Get()))
								{
									GetSetMediaTrackInfo(tr, "P_ICON", (void*)rule->m_icon.Get());
								}
							}
							pACTrack->m_icon.Set(rule->m_icon.Get());
						}
						pACTrack->m_bIconed = true;
					}

					// Set the layout
					for (int k=0; k<2; k++) if (bLayout[k])
					{
						// 'normal' track layout
						if (_stricmp(rule->m_layout[k].Get(), pACTrack->m_layout[k].Get()) && _stricmp(rule->m_layout[k].Get(), "(hide)")) 
						{
							const char *curlayout = (const char*)GetSetMediaTrackInfo(tr, k ? "P_MCP_LAYOUT" : "P_TCP_LAYOUT", NULL);
							if (curlayout && _stricmp(curlayout, rule->m_layout[k].Get()))
							{
								// Only overwrite the layout if there's no layout, or we're forcing, or we set it ourselves earlier
								if (bForce || !_stricmp(curlayout, pACTrack->m_layout[k].Get()))
								{
									GetSetMediaTrackInfo(tr, k ? "P_MCP_LAYOUT" : "P_TCP_LAYOUT", (void*)rule->m_layout[k].Get());
								}
							}
							pACTrack->m_layout[k].Set(rule->m_layout[k].Get());
						}
						// '(hide)' layout 
						if (_stricmp(rule->m_layout[k].Get(), pACTrack->m_layout[k].Get()) && !_stricmp(rule->m_layout[k].Get(), "(hide)")) 
						{
							bool isTrackVisible = IsTrackVisible(tr, k ? true : false);

							if (isTrackVisible && !_stricmp(rule->m_layout[k].Get(), "(hide)"))
							{	
								// Only hide the track if visible, or we're forcing, or we hid it ourselves earlier
								if (bForce || isTrackVisible == IsTrackVisible(pACTrack->m_pTr, k ? true : false))
								{
									GetSetMediaTrackInfo(tr, k ? "B_SHOWINMIXER" : "B_SHOWINTCP", &g_i0); // hide the track
									TrackList_AdjustWindows(k ? false : true); // https://forum.cockos.com/showthread.php?t=208275
								}
							}
							pACTrack->m_layout[k].Set(rule->m_layout[k].Get());
						}
						pACTrack->m_bLayouted[k] = true;
					}
				} // /if (bMatch)
			} // /Do the track rule matching
		} // /iterate through all tracks

		// Handle gradients
		for (int i = 0; i < gradientTracks.GetSize(); i++)
		{
			int newCol = g_crGradStart | 0x1000000;
			if (i && gradientTracks.GetSize() > 1)
				newCol = CalcGradient(g_crGradStart, g_crGradEnd, (double)i / (gradientTracks.GetSize()-1)) | 0x1000000;
			for (int j = 0; j < g_pACTracks.Get()->GetSize(); j++)
				if (g_pACTracks.Get()->Get(j)->m_pTr == (MediaTrack*)gradientTracks.Get(i))
				{
					g_pACTracks.Get()->Get(j)->m_col = newCol;
					break;
				}
			GetSetMediaTrackInfo((MediaTrack*)gradientTracks.Get(i), "I_CUSTOMCOLOR", &newCol);
		}

		PreventUIRefresh(-1);
	}
}

// Here's the meat and potatoes, apply the colors/icons!
void AutoColorTrack(bool bForce)
{
	static bool bRecurse = false;
	if (bRecurse || (!g_bACEnabled && !g_bAIEnabled && !g_bALEnabled && !bForce))
		return;
	bRecurse = true;

	// If forcing, start over with the saved track list
	if (bForce)
		g_pACTracks.Get()->Empty(true);
	else
		// Remove non-existant tracks from the autocolortracklist
		for (int i = 0; i < g_pACTracks.Get()->GetSize(); i++)
			if (CSurf_TrackToID(g_pACTracks.Get()->Get(i)->m_pTr, false) < 0)
			{
				g_pACTracks.Get()->Delete(i, true);
				i--;
			}

	// Clear the "colored" bit and "iconed" bit
	for (int i = 0; i < g_pACTracks.Get()->GetSize(); i++)
	{
		SWS_RuleTrack* r=g_pACTracks.Get()->Get(i);
		r->m_bColored = false;
		r->m_bIconed = false;
		r->m_bLayouted[0] = false;
		r->m_bLayouted[1] = false;
	}

	// Apply the rules
	bool bDoColors = g_bACEnabled || bForce;
	bool bDoIcons  = g_bAIEnabled || bForce;
	bool bDoLayouts  = g_bALEnabled || bForce;

	PreventUIRefresh(1);

	for (int i = 0; i < g_pACItems.GetSize(); i++)
		ApplyColorRuleToTrack(g_pACItems.Get(i), bDoColors, bDoIcons, bDoLayouts, bForce);

	// Remove colors/icons if necessary
	for (int i = 0; i < g_pACTracks.Get()->GetSize(); i++)
	{
		SWS_RuleTrack* pACTrack = g_pACTracks.Get()->Get(i);
		if (bDoColors && !pACTrack->m_bColored && pACTrack->m_col)
		{
			int iCurColor = *(int*)GetSetMediaTrackInfo(pACTrack->m_pTr, "I_CUSTOMCOLOR", NULL);
			if (!(iCurColor & 0x1000000))
				iCurColor = 0;

			// Only remove color on tracks that we colored ourselves
			if (pACTrack->m_col == iCurColor)
			{
				GetSetMediaTrackInfo(pACTrack->m_pTr, "I_CUSTOMCOLOR", &g_i0);
			}
			pACTrack->m_col = 0;
		}

		// There's an icon set, but there shouldn't be!
		if (bDoIcons && !pACTrack->m_bIconed && pACTrack->m_icon.GetLength())
		{
			// Only remove the icon on the track if we set it ourselves
			const char *cur = (const char*)GetSetMediaTrackInfo(pACTrack->m_pTr, "P_ICON", NULL); // requires REAPER v5.15pre6+
			cur = GetShortResourcePath("Data" WDL_DIRCHAR_STR "track_icons", cur);
			if (cur && !_stricmp(pACTrack->m_icon.Get(), cur))
			{
				GetSetMediaTrackInfo(pACTrack->m_pTr, "P_ICON", (void*)"");
			}
			pACTrack->m_icon.Set("");
		}

		if (bDoLayouts) for (int k=0; k<2; k++)
		{
			// There's a layout set, but there shouldn't be!
			// 'normal' track layout
			if (!pACTrack->m_bLayouted[k] && pACTrack->m_layout[k].GetLength() && _stricmp(pACTrack->m_layout[k].Get(), "(hide)"))
			{
				// Only remove the layout if we set it ourselves
				const char *curlayout = (const char*)GetSetMediaTrackInfo(pACTrack->m_pTr, k ? "P_MCP_LAYOUT" : "P_TCP_LAYOUT", NULL);
				if (curlayout && !_stricmp(pACTrack->m_layout[k].Get(), curlayout))
				{
					GetSetMediaTrackInfo(pACTrack->m_pTr, k ? "P_MCP_LAYOUT" : "P_TCP_LAYOUT", (void*)"");        
				}
				pACTrack->m_layout[k].Set("");
      }
			// '(hide)' layout 
			if (!pACTrack->m_bLayouted[k] && pACTrack->m_layout[k].GetLength() && !_stricmp(pACTrack->m_layout[k].Get(), "(hide)"))
			{
				// Only unhide the track if we hid it ourselves
				bool isTrackVisible = IsTrackVisible(pACTrack->m_pTr, k ? true : false);
				if (!isTrackVisible && !_stricmp(pACTrack->m_layout[k].Get(), "(hide)"))
				{
					GetSetMediaTrackInfo(pACTrack->m_pTr, k ? "B_SHOWINMIXER" : "B_SHOWINTCP", &g_i1); // show the track
					TrackList_AdjustWindows((k ? false : true));
				}
				pACTrack->m_layout[k].Set("");
			}
    }
	}

	if (bForce)
		Undo_OnStateChangeEx(__LOCALIZE("Apply auto color/icon/layout","sws_undo"), UNDO_STATE_TRACKCFG | UNDO_STATE_MISCCFG, -1);
	PreventUIRefresh(-1);

	bRecurse = false;
}

void ApplyColorRuleToMarkerRegion(SWS_RuleItem* _rule, int _flags)
{
	ColorTheme* ct = SNM_GetColorTheme();
	if (!_rule || !_flags || !ct)
		return;

	double pos, end;
	int x=0, num, color;
	bool isRgn;
	const char* name;
	const bool isRegex = (strlen(_rule->m_str_filter.Get()) > 1) && ((_rule->m_str_filter.Get())[0] == g_cRegexPrefix);

	PreventUIRefresh(1);
	if(_rule->m_type & _flags)
	{
		while ((x = EnumProjectMarkers3(NULL, x, &isRgn, &pos, &end, &name, &num, &color)))
		{
			if ((!strcmp(cFilterTypes[AC_RGNANY], _rule->m_str_filter.Get()) ||
				(!strcmp(cFilterTypes[AC_RGNUNNAMED], _rule->m_str_filter.Get()) && (!name || !*name)) ||
				(name && isRegex && std::regex_match(name, std::regex((char*)_rule->m_str_filter.Get() + 1, g_bmRegexOptions))) ||
				(name && stristr(name, _rule->m_str_filter.Get())))
				&&
				((_flags&AC_REGION && isRgn && _rule->m_type==AC_REGION) ||
				(_flags&AC_MARKER && !isRgn && _rule->m_type==AC_MARKER)))
			{
				SetProjectMarkerByIndex(NULL, x-1, isRgn, pos, end, num, NULL, _rule->m_color==-AC_NONE-1 ? (isRgn?ct->marker:ct->region) : _rule->m_color | 0x1000000);
			}
		}
	}
	PreventUIRefresh(-1);
}

void AutoColorMarkerRegion(bool _force, int _flags)
{
	static bool bRecurse = false;
	if (bRecurse || (!g_bACREnabled && !g_bACMEnabled && !_force))
		return;
	bRecurse = true;

	int newFlags = 0;
	if (_flags&AC_MARKER && (g_bACMEnabled || _force))
		newFlags |= AC_MARKER;
	if (_flags&AC_REGION && (g_bACREnabled || _force))
		newFlags |= AC_REGION;

	if (newFlags)
	{
		PreventUIRefresh(1);

		for (int i=g_pACItems.GetSize()-1; i>=0; i--) // reverse to obey priority
			ApplyColorRuleToMarkerRegion(g_pACItems.Get(i), newFlags);

		PreventUIRefresh(-1);
	}

	if (_force)
		Undo_OnStateChangeEx(__LOCALIZE("Apply auto marker/region color","sws_undo"), UNDO_STATE_MISCCFG, -1);
	bRecurse = false;
}

void EnableAutoColor(COMMAND_T* ct)
{
	switch((int)ct->user)
	{
		case 0: g_bACEnabled = !g_bACEnabled; break;
		case 1: g_bACMEnabled = !g_bACMEnabled; break;
		case 2: g_bACREnabled = !g_bACREnabled; break;
	}
	g_pACWnd->Update();
}

void EnableAutoIcon(COMMAND_T*)
{
	g_bAIEnabled = !g_bAIEnabled;
	g_pACWnd->Update();
}

void EnableAutoLayout(COMMAND_T*)
{
	g_bALEnabled = !g_bALEnabled;
	g_pACWnd->Update();
}

void ApplyAutoColor(COMMAND_T*)
{
	AutoColorTrack(true);
	AutoColorMarkerRegion(true);
}

int IsAutoColorOpen(COMMAND_T*)		{ return g_pACWnd->IsWndVisible(); }
int IsAutoIconEnabled(COMMAND_T*)	{ return g_bAIEnabled; }
int IsAutoLayoutEnabled(COMMAND_T*)	{ return g_bALEnabled; }

int IsAutoColorEnabled(COMMAND_T* ct)
{
	switch((int)ct->user) {
		case 0: return g_bACEnabled;
		case 1: return g_bACMEnabled;
		case 2: return g_bACREnabled;
	}
	return false;
 }

static bool ProcessExtensionLine(const char *line, ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	LineParser lp(false);
	if (lp.parse(line) || lp.getnumtokens() < 1)
		return false;
	if (strcmp(lp.gettoken_str(0), "<SWSAUTOCOLOR") == 0)
	{
		char linebuf[4096];
		while(true)
		{
			if (!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
			{
				if (lp.gettoken_str(0)[0] == '>')
					break;

				GUID g;
				stringToGuid(lp.gettoken_str(0), &g);
				MediaTrack* tr = GuidToTrack(&g);
				if (tr)
				{
					SWS_RuleTrack* rt = g_pACTracks.Get()->Add(new SWS_RuleTrack(tr));
					rt->m_col = lp.gettoken_int(1);
					rt->m_icon.Set(lp.gettoken_str(2));
					rt->m_layout[0].Set(lp.gettoken_str(3));
					rt->m_layout[1].Set(lp.gettoken_str(4));
				}
			}
			else
				break;
		}
		return true;
	}
	return false;
}

static void SaveExtensionConfig(ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	// first delete unused tracks
	for (int i = 0; i < g_pACTracks.Get()->GetSize(); i++)
		if (CSurf_TrackToID(g_pACTracks.Get()->Get(i)->m_pTr, false) < 0)
		{
			g_pACTracks.Get()->Delete(i, true);
			i--;
		}

	if (g_pACTracks.Get()->GetSize())
	{
		ctx->AddLine("<SWSAUTOCOLOR");
		char str[256];
		for (int i = 0; i < g_pACTracks.Get()->GetSize(); i++)
		{
			if (SWS_RuleTrack* rt = g_pACTracks.Get()->Get(i))
			{
				GUID g = GUID_NULL;
				if (CSurf_TrackToID(rt->m_pTr, false))
					g = *(GUID*)GetSetMediaTrackInfo(rt->m_pTr, "GUID", NULL);
				guidToString(&g, str);
				ctx->AddLine("%s %d \"%s\" \"%s\" \"%s\"", str, rt->m_col, rt->m_icon.Get(), rt->m_layout[0].Get(), rt->m_layout[1].Get());
			}
		}
		ctx->AddLine(">");
	}
}

static void BeginLoadProjectState(bool isUndo, struct project_config_extension_t *reg)
{
	g_pACTracks.Cleanup();
	g_pACTracks.Get()->Empty(true);
}

static project_config_extension_t g_projectconfig = { ProcessExtensionLine, SaveExtensionConfig, BeginLoadProjectState, NULL };

//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions
static COMMAND_T g_commandTable[] =
{
	{ { DEFACCEL, "SWS: Open auto color/icon/layout window" },				"SWSAUTOCOLOR_OPEN",		OpenAutoColor,		"SWS Auto color/icon/layout",			0, IsAutoColorOpen },
	{ { DEFACCEL, "SWS: Toggle auto track coloring enable" },		"SWSAUTOCOLOR_ENABLE",		EnableAutoColor,	"Enable auto track coloring",	0, IsAutoColorEnabled },
	{ { DEFACCEL, "SWS/S&M: Toggle auto marker coloring enable" },	"S&MAUTOCOLOR_MKR_ENABLE",	EnableAutoColor,	"Enable auto marker coloring",	1, IsAutoColorEnabled },
	{ { DEFACCEL, "SWS/S&M: Toggle auto region coloring enable" },	"S&MAUTOCOLOR_RGN_ENABLE",	EnableAutoColor,	"Enable auto region coloring",	2, IsAutoColorEnabled },
	{ { DEFACCEL, "SWS/S&M: Toggle auto track icon enable" },		"S&MAUTOICON_ENABLE",		EnableAutoIcon,		"Enable auto icon",				0, IsAutoIconEnabled },
	{ { DEFACCEL, "SWS/S&M: Toggle auto track layout enable" },		"S&MAUTOLAYOUT_ENABLE",		EnableAutoLayout,		"Enable auto layout",				0, IsAutoLayoutEnabled },
	{ { DEFACCEL, "SWS: Apply auto coloring" },						"SWSAUTOCOLOR_APPLY",		ApplyAutoColor,	},
	{ {}, LAST_COMMAND, }, // Denote end of table
};
//!WANT_LOCALIZE_1ST_STRING_END


int AutoColorInit()
{
	if (!plugin_register("projectconfig",&g_projectconfig))
		return 0;

	SWSRegisterCommands(g_commandTable);

	g_ACIni.SetFormatted(MAX_PATH, "%s%csws-autocoloricon.ini", GetResourcePath(), PATH_SLASH_CHAR);

	// "Upgrade" to the new ini file for just the auto color stuff if necessary
	WDL_String ini(g_ACIni);
	bool bUpgrade = false;
	int iCount = GetPrivateProfileInt(SWS_INI, AC_COUNT_KEY, 0, get_ini_file());
	if (iCount)
	{
		bUpgrade = true;
		ini.Set(get_ini_file());
	}
	else
		iCount = GetPrivateProfileInt(SWS_INI, AC_COUNT_KEY, 0, ini.Get());

	// Restore state
	char str[BUFFER_SIZE];
	g_bACEnabled = GetPrivateProfileInt(SWS_INI, AC_ENABLE_KEY, 0, ini.Get()) ? true : false;
	g_bACMEnabled = GetPrivateProfileInt(SWS_INI, ACM_ENABLE_KEY, 0, ini.Get()) ? true : false;
	g_bACREnabled = GetPrivateProfileInt(SWS_INI, ACR_ENABLE_KEY, 0, ini.Get()) ? true : false;
	g_bAIEnabled = GetPrivateProfileInt(SWS_INI, AI_ENABLE_KEY, 0, ini.Get()) ? true : false;
	g_bALEnabled = GetPrivateProfileInt(SWS_INI, AL_ENABLE_KEY, 0, ini.Get()) ? true : false;

	char key[32];
	for (int i = 0; i < iCount; i++)
	{
		snprintf(key, 32, AC_ITEM_KEY, i+1);
		GetPrivateProfileString(SWS_INI, key, "", str, BUFFER_SIZE, ini.Get()); // Read in AutoColor line (stored in str)
		if (bUpgrade) // Remove old lines
			WritePrivateProfileString(SWS_INI, key, NULL, get_ini_file());
		LineParser lp(false);
		if (!lp.parse(str) && lp.getnumtokens() >= 4)
			g_pACItems.Add(new SWS_RuleItem(lp.gettoken_int(0), lp.gettoken_str(1), lp.gettoken_int(2), lp.gettoken_str(3), lp.gettoken_str(4), lp.gettoken_str(5)));
		else if(!lp.parse(str) && lp.getnumtokens() == 3) //Reformat old format Autocolor line to new format (i.e. region + marker)
			g_pACItems.Add(new SWS_RuleItem(AC_TRACK, lp.gettoken_str(0), lp.gettoken_int(1), lp.gettoken_str(2), "", ""));
	}

	if (bUpgrade)
	{	// Remove old stuff
		WritePrivateProfileString(SWS_INI, AC_ENABLE_KEY, NULL, get_ini_file());
		WritePrivateProfileString(SWS_INI, AI_ENABLE_KEY, NULL, get_ini_file());
		WritePrivateProfileString(SWS_INI, AC_COUNT_KEY, NULL, get_ini_file());
		AutoColorSaveState();
	}

	ACRegisterUnregisterToMarkerRegionUpdates();

	g_pACWnd = new SWS_AutoColorWnd();

	return 1;
}

void AutoColorSaveState()
{
	// Save state
	char str[BUFFER_SIZE];
	sprintf(str, "%d", g_bACEnabled ? 1 : 0);
	WritePrivateProfileString(SWS_INI, AC_ENABLE_KEY, str, g_ACIni.Get());
	sprintf(str, "%d", g_bACMEnabled ? 1 : 0);
	WritePrivateProfileString(SWS_INI, ACM_ENABLE_KEY, str, g_ACIni.Get());
	sprintf(str, "%d", g_bACREnabled ? 1 : 0);
	WritePrivateProfileString(SWS_INI, ACR_ENABLE_KEY, str, g_ACIni.Get());
	sprintf(str, "%d", g_bAIEnabled ? 1 : 0);
	WritePrivateProfileString(SWS_INI, AI_ENABLE_KEY, str, g_ACIni.Get());
	sprintf(str, "%d", g_bALEnabled ? 1 : 0);
	WritePrivateProfileString(SWS_INI, AL_ENABLE_KEY, str, g_ACIni.Get());
	sprintf(str, "%d", g_pACItems.GetSize());
	WritePrivateProfileString(SWS_INI, AC_COUNT_KEY, str, g_ACIni.Get());

	char key[32];
	WDL_FastString tmp[4];
	for (int i = 0; i < g_pACItems.GetSize(); i++)
	{
		makeEscapedConfigString(g_pACItems.Get(i)->m_str_filter.Get(), &tmp[0]);
		makeEscapedConfigString(g_pACItems.Get(i)->m_icon.Get(), &tmp[1]);
		makeEscapedConfigString(g_pACItems.Get(i)->m_layout[0].Get(), &tmp[2]);
		makeEscapedConfigString(g_pACItems.Get(i)->m_layout[1].Get(), &tmp[3]);
		snprintf(str, BUFFER_SIZE, "%d %s %d %s %s %s", g_pACItems.Get(i)->m_type, tmp[0].Get(), g_pACItems.Get(i)->m_color, tmp[1].Get(), tmp[2].Get(), tmp[3].Get());

		snprintf(key, 32, AC_ITEM_KEY, i+1);
		WritePrivateProfileString(SWS_INI, key, str, g_ACIni.Get());
	}
	// Erase the n+1 entry
	snprintf(key, 32, AC_ITEM_KEY, g_pACItems.GetSize() + 1);
	WritePrivateProfileString(SWS_INI, key, NULL, g_ACIni.Get());
}

void AutoColorExit()
{
	plugin_register("-projectconfig",&g_projectconfig);
	UnregisterToMarkerRegionUpdates(&g_mkrRgnListener);
	AutoColorSaveState();
	DELETE_NULL(g_pACWnd);
}
