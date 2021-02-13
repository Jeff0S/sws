/******************************************************************************
/ SnM_Find.cpp
/
/ Copyright (c) 2010 and later Jeffos
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

// intentionally not localized
// (better find/replace version sleeping in a drawer..)

#include "stdafx.h"

#include "SnM.h"
#include "SnM_Dlg.h"
#include "SnM_Find.h"
#include "SnM_Item.h"
#include "SnM_Notes.h"
#include "SnM_Track.h"
#include "SnM_Util.h"
#include "../Zoom.h"

#include <WDL/localize/localize.h>

#define FIND_WND_ID				"SnMFind"
#define FIND_INI_SEC			"Find"
#define MAX_SEARCH_STR_LEN		128

enum {
  TXTID_SCOPE=0xF000,
  BTNID_FIND,
  BTNID_PREV,
  BTNID_NEXT,
  BTNID_ZOOM_SCROLL_EN,
  CMBID_TYPE,
  TXTID_RESULT
};

enum {
	TYPE_ITEM_NAME=0,
	TYPE_ITEM_NAME_ALL_TAKES,
	TYPE_ITEM_FILENAME,
	TYPE_ITEM_FILENAME_ALL_TAKES,
	TYPE_ITEM_NOTES,
	TYPE_TRACK_NAME,
	TYPE_TRACK_NOTES,
	TYPE_MARKER_REGION
};

SNM_WindowManager<FindWnd> g_findWndMgr(FIND_WND_ID);
char g_searchStr[MAX_SEARCH_STR_LEN] = "";
bool g_notFound=false;


///////////////////////////////////////////////////////////////////////////////

bool TakeNameMatch(MediaItem_Take* _tk, const char* _searchStr)
{
	char* takeName = _tk ? (char*)GetSetMediaItemTakeInfo(_tk, "P_NAME", NULL) : NULL;
	return (takeName && stristr(takeName, _searchStr));
}

bool TakeFilenameMatch(MediaItem_Take* _tk, const char* _searchStr)
{
	bool match = false;
	PCM_source* src = _tk ? (PCM_source*)GetSetMediaItemTakeInfo(_tk, "P_SOURCE", NULL) : NULL;
	if (src) 
	{
		const char* takeFilename = src->GetFileName();
		match = (takeFilename && strstr(takeFilename, _searchStr)); // no stristr: osx + utf-8
	}
	return match;
}

bool ItemNotesMatch(MediaItem* _item, const char* _searchStr)
{
	bool match = false;
	if (_item)
	{
		SNM_ChunkParserPatcher p(_item);
		WDL_FastString notes;
		if (p.GetSubChunk("NOTES", 2, 0, &notes, "VOLPAN") >= 0) // rmk: we use VOLPAN as it also exists for empty items
			//JFB TODO? we compare a formated string with a normal one here, oh well..
			match = (stristr(notes.Get(), _searchStr) != NULL);
	}
	return match;
}

bool TrackNameMatch(MediaTrack* _tr, const char* _searchStr) {
	char* name = _tr ? (char*)GetSetMediaTrackInfo(_tr, "P_NAME", NULL) : NULL;
	return (name && stristr(name, _searchStr));
}

bool TrackNotesMatch(MediaTrack* _tr, const char* _searchStr) 
{
	bool match = false;
	if (_tr)
	{
		for (int i=0; i < g_SNM_TrackNotes.Get()->GetSize(); i++)
		{
			if (g_SNM_TrackNotes.Get()->Get(i)->GetTrack() == _tr)
			{
				match = stristr(g_SNM_TrackNotes.Get()->Get(i)->GetNotes(), _searchStr) != NULL;
				break;
			}
		}
	}
	return match;
}

///////////////////////////////////////////////////////////////////////////////
// FindWnd
///////////////////////////////////////////////////////////////////////////////

// S&M windows lazy init: below's "" prevents registering the SWS' screenset callback
// (use the S&M one instead - already registered via SNM_WindowManager::Init())
FindWnd::FindWnd()
	: SWS_DockWnd(IDD_SNM_FIND, __LOCALIZE("Find","sws_DLG_154"), "")
{
	m_id.Set(FIND_WND_ID);
	m_type = 0;
	m_zoomSrollItems = false;

	// Must call SWS_DockWnd::Init() to restore parameters and open the window if necessary
	Init();
}

void FindWnd::OnInitDlg()
{
	m_resize.init_item(IDC_EDIT, 0.0, 0.0, 1.0, 0.0);
	SetWindowLongPtr(GetDlgItem(m_hwnd, IDC_EDIT), GWLP_USERDATA, 0xdeadf00b);

	// load prefs 
	m_type = GetPrivateProfileInt(FIND_INI_SEC, "Type", 0, g_SNM_IniFn.Get());
	m_zoomSrollItems = (GetPrivateProfileInt(FIND_INI_SEC, "ZoomScrollToFoundItems", 0, g_SNM_IniFn.Get()) == 1);


	LICE_CachedFont* font = SNM_GetThemeFont();

	m_vwnd_painter.SetGSC(WDL_STYLE_GetSysColor);
	m_parentVwnd.SetRealParent(m_hwnd);
	
	m_txtScope.SetID(TXTID_SCOPE);
	m_txtScope.SetFont(font);
	m_txtScope.SetText(__LOCALIZE("Find in:","sws_DLG_154"));
	m_parentVwnd.AddChild(&m_txtScope);

	m_btnEnableZommScroll.SetID(BTNID_ZOOM_SCROLL_EN);
	m_btnEnableZommScroll.SetTextLabel(__LOCALIZE("Zoom/Scroll","sws_DLG_154"), -1, font);
	m_btnEnableZommScroll.SetCheckState(m_zoomSrollItems);
	m_parentVwnd.AddChild(&m_btnEnableZommScroll);

	m_btnFind.SetID(BTNID_FIND);
	m_parentVwnd.AddChild(&m_btnFind);

	m_btnPrev.SetID(BTNID_PREV);
	m_parentVwnd.AddChild(&m_btnPrev);

	m_btnNext.SetID(BTNID_NEXT);
	m_parentVwnd.AddChild(&m_btnNext);

	m_cbType.SetID(CMBID_TYPE);
	m_cbType.SetFont(font);
	m_cbType.AddItem(__LOCALIZE("Item names","sws_DLG_154"));
	m_cbType.AddItem(__LOCALIZE("Item names (all takes)","sws_DLG_154"));
	m_cbType.AddItem(__LOCALIZE("Media filenames","sws_DLG_154"));
	m_cbType.AddItem(__LOCALIZE("Media filenames (all takes)","sws_DLG_154"));
	m_cbType.AddItem(__LOCALIZE("Item notes","sws_DLG_154"));
	m_cbType.AddItem(__LOCALIZE("Track names","sws_DLG_154"));
	m_cbType.AddItem(__LOCALIZE("Track notes","sws_DLG_154"));
	m_cbType.AddItem(__LOCALIZE("Marker/region names","sws_DLG_154"));
	m_cbType.SetCurSel(m_type);
	m_parentVwnd.AddChild(&m_cbType);

	m_txtResult.SetID(TXTID_RESULT);
	m_txtResult.SetFont(font);
	m_txtResult.SetColors(LICE_RGBA(170,0,0,255));
	m_parentVwnd.AddChild(&m_txtResult);


	g_notFound = false;
//	*g_searchStr = 0;
	SetDlgItemText(m_hwnd, IDC_EDIT, g_searchStr);

	m_parentVwnd.RequestRedraw(NULL);
}

void FindWnd::OnDestroy() 
{
	// save prefs
	char type[4] = "";
	if (snprintfStrict(type, sizeof(type), "%d", m_type) > 0)
		WritePrivateProfileString(FIND_INI_SEC, "Type", type, g_SNM_IniFn.Get());
	WritePrivateProfileString(FIND_INI_SEC, "ZoomScrollToFoundItems", m_zoomSrollItems ? "1" : "0", g_SNM_IniFn.Get());

	m_cbType.Empty();
	g_notFound = false;
//	*g_searchStr = 0;
}

void FindWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch(LOWORD(wParam))
	{
		case IDC_EDIT:
			if (HIWORD(wParam)==EN_CHANGE) {
				GetDlgItemText(m_hwnd, IDC_EDIT, g_searchStr, MAX_SEARCH_STR_LEN);
				UpdateNotFoundMsg(true);
			}
			break;
		case BTNID_ZOOM_SCROLL_EN:
			if (!HIWORD(wParam) ||  HIWORD(wParam)==600)
				m_zoomSrollItems = !m_zoomSrollItems;
			break;
		case BTNID_FIND:
			Find(0);
			break;
		case BTNID_PREV:
			Find(-1);
			break;
		case BTNID_NEXT:
			Find(1);
			break;
		case CMBID_TYPE:
			if (HIWORD(wParam)==CBN_SELCHANGE) {
				m_type = m_cbType.GetCurSel();
				UpdateNotFoundMsg(true); // + redraw
				SetFocus(GetDlgItem(m_hwnd, IDC_EDIT));
			}
			break;
		default:
			Main_OnCommand((int)wParam, (int)lParam);
			break;
	}
}

int FindWnd::OnKey(MSG* _msg, int _iKeyState) 
{
	HWND h = GetDlgItem(m_hwnd, IDC_EDIT);
/*JFB not needed: IDC_EDIT is the single control of this window..
#ifdef _WIN32
	if (_msg->hwnd == h)
#else
	if (GetFocus() == h)
#endif
*/
	{
		// ctrl+A => select all
		if ((_msg->message == WM_KEYDOWN || _msg->message == WM_CHAR) &&
			_msg->wParam == 'A' && _iKeyState == LVKF_CONTROL)
		{
			SetFocus(h);
			SendMessage(h, EM_SETSEL, 0, -1);
			return 1; // eat
		}
	}

	if (_msg->message == WM_KEYDOWN &&
		(_msg->wParam == VK_F3 || _msg->wParam == VK_RETURN))
	{
		// F3: find next
		if (!_iKeyState) {
				Find(1); 
				return 1;
		}
		// Shift-F3: find previous
		else if (_iKeyState == LVKF_SHIFT) {
				Find(-1);
				return 1;
		}
	}
	return 0; // pass-thru
}

void FindWnd::DrawControls(LICE_IBitmap* _bm, const RECT* _r, int* _tooltipHeight)
{
	// 1st row of controls
	int x0 = _r->left + SNM_GUI_X_MARGIN_OLD;
	int h = SNM_GUI_TOP_H;
	if (_tooltipHeight)
		*_tooltipHeight = h;
	bool drawLogo = false;

	if (!SNM_AutoVWndPosition(DT_LEFT, &m_txtScope, NULL, _r, &x0, _r->top, h, 5))
		return;

	if (SNM_AutoVWndPosition(DT_LEFT, &m_cbType, &m_txtScope, _r, &x0, _r->top, h))
	{
		switch (m_type)
		{
			case TYPE_ITEM_NAME:
			case TYPE_ITEM_NAME_ALL_TAKES:
			case TYPE_ITEM_FILENAME:
			case TYPE_ITEM_FILENAME_ALL_TAKES:
			case TYPE_ITEM_NOTES:
				m_btnEnableZommScroll.SetCheckState(m_zoomSrollItems);
				drawLogo = SNM_AutoVWndPosition(DT_LEFT, &m_btnEnableZommScroll, NULL, _r, &x0, _r->top, h);
				break;
			default:
				drawLogo = true;
				break;
		}
	}

	if (drawLogo)
		SNM_AddLogo(_bm, _r, x0, h);

	// 2nd row of controls
	h = 45;
	x0 = _r->left + SNM_GUI_X_MARGIN_OLD;
	int y0 = _r->top+56;

	SNM_SkinToolbarButton(&m_btnFind, __LOCALIZE("Find all","sws_DLG_154"));
	m_btnFind.SetGrayed(!*g_searchStr || m_type == TYPE_MARKER_REGION);
	if (SNM_AutoVWndPosition(DT_LEFT, &m_btnFind, NULL, _r, &x0, y0, h, 4))
	{
		SNM_SkinToolbarButton(&m_btnPrev, __LOCALIZE("Previous","sws_DLG_154"));
		m_btnPrev.SetGrayed(!*g_searchStr);
		if (SNM_AutoVWndPosition(DT_LEFT, &m_btnPrev, NULL, _r, &x0, y0, h, 4))
		{
			SNM_SkinToolbarButton(&m_btnNext, __LOCALIZE("Next","sws_DLG_154"));
			m_btnNext.SetGrayed(!*g_searchStr);
			SNM_AutoVWndPosition(DT_LEFT, &m_btnNext, NULL, _r, &x0, y0, h);
		}
	}

	m_txtResult.SetText(g_notFound ? __LOCALIZE("Not found!","sws_DLG_154") : "");
	SNM_AutoVWndPosition(DT_LEFT, &m_txtResult, NULL, _r, &x0, y0, h);
}


///////////////////////////////////////////////////////////////////////////////

bool FindWnd::Find(int _mode)
{
	bool update = false;
	switch(m_type)
	{
		case TYPE_ITEM_NAME:
			update = FindMediaItem(_mode, false, TakeNameMatch);
		break;
		case TYPE_ITEM_NAME_ALL_TAKES:
			update = FindMediaItem(_mode, true, TakeNameMatch);
		break;
		case TYPE_ITEM_FILENAME:
			update = FindMediaItem(_mode, false, TakeFilenameMatch);
		break;
		case TYPE_ITEM_FILENAME_ALL_TAKES:
			update = FindMediaItem(_mode, true, TakeFilenameMatch);
		break;
		case TYPE_ITEM_NOTES:
			update = FindMediaItem(_mode, false, NULL, ItemNotesMatch);
		break;
		case TYPE_TRACK_NAME:
			update = FindTrack(_mode, TrackNameMatch);
		break;
		case TYPE_TRACK_NOTES:
			update = FindTrack(_mode, TrackNotesMatch);
		break;
		case TYPE_MARKER_REGION:
			update = FindMarkerRegion(_mode);
	}
	return update;
}

MediaItem* FindWnd::FindPrevNextItem(int _dir, MediaItem* _item)
{
	if (!_dir)
		return NULL;

	MediaItem* previous = NULL;
	int startTrIdx = (_dir == -1 ? CountTracks(NULL) : 1);
	if (_item)
		if (MediaTrack* trItem = GetMediaItem_Track(_item))
			startTrIdx = CSurf_TrackToID(trItem, false);

	if (startTrIdx>=0)
	{
		bool found = (_item == NULL);
		for (int i = startTrIdx; !previous && i <= CountTracks(NULL) && i >= 1; i+=_dir)
		{
			MediaTrack* tr = CSurf_TrackFromID(i, false); 
			int nbItems = GetTrackNumMediaItems(tr);
			for (int j = (_dir > 0 ? 0 : (nbItems-1)); j < nbItems && j >= 0; j+=_dir)
			{
				MediaItem* item = GetTrackMediaItem(tr,j);
				if (found && item) {
					previous = item;
					break;
				}
				if (_item && item == _item)
					found = true;
			}
		}
	}
	return previous;
}

// param _allTakes only makes sense if jobTake() is used
bool FindWnd::FindMediaItem(int _dir, bool _allTakes, bool (*jobTake)(MediaItem_Take*,const char*), bool (*jobItem)(MediaItem*,const char*))
{
	bool update = false, found = false, sel = true;
	if (*g_searchStr)
	{
		PreventUIRefresh(1);

		MediaItem* startItem = NULL;
		bool clearCurrentSelection = false;
		if (_dir)
		{
			WDL_PtrList<MediaItem> items;
			SNM_GetSelectedItems(NULL, &items);
			if (items.GetSize())
			{
				startItem = FindPrevNextItem(_dir, items.Get(_dir > 0 ? 0 : items.GetSize()-1));
				clearCurrentSelection = (startItem != NULL); 
			}
			else
				startItem = FindPrevNextItem(_dir, NULL);
		}
		else
		{
			startItem = FindPrevNextItem(1, NULL);
			clearCurrentSelection = (startItem != NULL); 
		}

		if (clearCurrentSelection)
		{
			Undo_BeginBlock2(NULL);
			Main_OnCommand(40289,0); // unselect all items
			update = true;
		}

		MediaItem* item = NULL;
		MediaTrack* startTr = startItem ? GetMediaItem_Track(startItem) : NULL;
		int startTrIdx = startTr ? CSurf_TrackToID(startTr, false) : -1;
		if (startTr && startItem && startTrIdx>=0)
		{
			// find startItem idx
			int startItemIdx=-1;
			while (item != startItem) 
				item = GetTrackMediaItem(startTr,++startItemIdx);

			bool firstItem=true, breakSelection=false;
			for (int i=startTrIdx; !breakSelection && i <= CountTracks(NULL) && i>=1; i += (!_dir ? 1 : _dir))
			{
				MediaTrack* tr = CSurf_TrackFromID(i, false); 
				int nbItems = GetTrackNumMediaItems(tr);
				for (int j = (firstItem ? startItemIdx : (_dir >= 0 ? 0 : (nbItems-1))); 
					 tr && !breakSelection && j < nbItems && j >= 0; 
					 j += (!_dir ? 1 : _dir))
				{
					item = GetTrackMediaItem(tr,j);
					firstItem = false;

					// search at item level 
					if (jobItem)
					{
						if (jobItem(item, g_searchStr))
						{
							if (!update) Undo_BeginBlock2(NULL);
							update = found = true;
							GetSetMediaItemInfo(item, "B_UISEL", &sel);
							if (_dir) breakSelection = true;
						}
					}
					// search at take level 
					else if (jobTake)
					{
						int nbTakes = GetMediaItemNumTakes(item);
						for (int k=0; item && k < nbTakes; k++)
						{
							MediaItem_Take* tk = GetMediaItemTake(item, k);
							if (tk && (_allTakes || tk == GetActiveTake(item)))
							{
								if (jobTake(tk, g_searchStr))
								{
									if (!update) Undo_BeginBlock2(NULL);
									update = found = true;
									GetSetMediaItemInfo(item, "B_UISEL", &sel);
									if (_dir) {
										breakSelection = true;
										break;
									}
								}
							}
						}
					}
				}
			}
		}
		UpdateNotFoundMsg(found);
		if (found && m_zoomSrollItems) {
			if (!_dir) ZoomToSelItems();
			else if (item) ScrollToSelItem(item);
		}

		PreventUIRefresh(-1);
	}

	if (update)
	{
		UpdateTimeline();
		Undo_EndBlock2(NULL, __LOCALIZE("Find: change media item selection","sws_undo"), UNDO_STATE_ALL);
	}
	return update;
}

bool FindWnd::FindTrack(int _dir, bool (*job)(MediaTrack*,const char*))
{
	bool update = false, found = false;
	if (*g_searchStr)
	{
		int startTrIdx = -1;
		bool clearCurrentSelection = false;
		if (_dir)
		{
			if (const int selTracksCount = SNM_CountSelectedTracks(NULL, true))
			{
				if (MediaTrack* startTr = SNM_GetSelectedTrack(NULL, _dir > 0 ? 0 : selTracksCount-1, true))
				{
					int id = CSurf_TrackToID(startTr, false);
					if ((_dir > 0 && id < CountTracks(NULL)) || (_dir < 0 && id >0))
					{
						startTrIdx = id + _dir;
						clearCurrentSelection = true;
					}
				}
			}
			else
				startTrIdx = (_dir > 0 ? 0 : CountTracks(NULL));
		}
		else
		{
			startTrIdx = 0;
			clearCurrentSelection = true;
		}

		if (clearCurrentSelection)
		{
			Undo_BeginBlock2(NULL);
			Main_OnCommand(40297,0); // unselect all tracks
			update = true;
		}

		if (startTrIdx >= 0)
		{
			for (int i = startTrIdx; i <= CountTracks(NULL) && i>=0; i += (!_dir ? 1 : _dir))
			{
				MediaTrack* tr = CSurf_TrackFromID(i, false); 
				if (tr && job(tr, g_searchStr))
				{
					if (!update)
						Undo_BeginBlock2(NULL);

					update = found = true;
					GetSetMediaTrackInfo(tr, "I_SELECTED", &g_i1);
					if (_dir) 
						break;
				}
			}
		}

		UpdateNotFoundMsg(found);	
		if (found)
			ScrollSelTrack(true, true);
	}

	if (update)
		Undo_EndBlock2(NULL, __LOCALIZE("Find: change track selection","sws_undo"), UNDO_STATE_ALL);

	return update;
}

bool FindWnd::FindMarkerRegion(int _dir)
{
	if (!_dir)
		return false;

	bool update = false, found = false;
	if (*g_searchStr)
	{
		double startPos = GetCursorPositionEx(NULL);
		int id, x = 0;
		bool bR;
		double dPos, dRend, dMinMaxPos = _dir < 0 ? -DBL_MAX : DBL_MAX;
		const char *cName;
		while ((x=EnumProjectMarkers2(NULL, x, &bR, &dPos, &dRend, &cName, &id)))
		{
			if (_dir == 1 && dPos > startPos) {
				if (stristr(cName, g_searchStr)) {
					found = true;
					dMinMaxPos = min(dPos, dMinMaxPos);
				}
			}
			else if (_dir == -1 && dPos < startPos) {
				if (stristr(cName, g_searchStr)) {
					found = true;
					dMinMaxPos = max(dPos, dMinMaxPos);
				}
			}
		}
		UpdateNotFoundMsg(found);	
		if (found) {
			SetEditCurPos2(NULL, dMinMaxPos, true, false);
			update = true;
		}
	}
	if (update)
		Undo_OnStateChangeEx2(NULL, __LOCALIZE("Find: change edit cursor position","sws_undo"), UNDO_STATE_ALL, -1); // in case the pref "undo pt for edit cursor positions" is enabled..
	return update;
}

void FindWnd::UpdateNotFoundMsg(bool _found)
{
	g_notFound = !_found;
	m_parentVwnd.RequestRedraw(NULL);
}


///////////////////////////////////////////////////////////////////////////////

int FindInit()
{
	// instanciate the window if needed, can be NULL
	g_findWndMgr.Init();
	return 1;
}

void FindExit() {
	g_findWndMgr.Delete();
}

void OpenFind(COMMAND_T*)
{
	if (FindWnd* w = g_findWndMgr.Create()) {
		w->Show(true, true);
		SetFocus(GetDlgItem(w->GetHWND(), IDC_EDIT));
	}
}

int IsFindDisplayed(COMMAND_T*)
{
	if (FindWnd* w = g_findWndMgr.Get())
		return w->IsWndVisible();
	return 0;
}

void FindNextPrev(COMMAND_T* _ct) {
	if (FindWnd* w = g_findWndMgr.Get())
		w->Find((int)_ct->user); 
}
