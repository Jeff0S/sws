/******************************************************************************
/ SnM_RegionPlaylist.cpp
/
/ Copyright (c) 2012 and later Jeffos
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
#include "SnM_CSurf.h"
#include "SnM_Dlg.h"
#include "SnM_Item.h"
#include "SnM_Project.h"
#include "SnM_RegionPlaylist.h"
#include "SnM_Util.h"
#include "../Prompt.h"
#include "WDL/xsrand.h"

#include <WDL/localize/localize.h>
#include <WDL/projectcontext.h>

#define RGNPL_WND_ID			"SnMRgnPlaylist"
#define UNDO_PLAYLIST_STR		__LOCALIZE("Region Playlist edition", "sws_undo")

#define OSC_CURRENT_RGN			"/snm/rgnplaylist/current"
#define OSC_NEXT_RGN			"/snm/rgnplaylist/next"

enum {
	DELETE_MSG=0xF000,
	PERFORM_MSG,
	CROP_PRJ_MSG,
	NEW_PLAYLIST_MSG,
	COPY_PLAYLIST_MSG,
	DEL_PLAYLIST_MSG,
	REN_PLAYLIST_MSG,
	ADD_ALL_REGIONS_MSG,
	CROP_PRJTAB_MSG,
	APPEND_PRJ_MSG,
	PASTE_CURSOR_MSG,
	APPEND_SEL_RGN_MSG,
	PASTE_SEL_RGN_MSG,
	TGL_INFINITE_LOOP_MSG,
	TGL_SEEK_NOW_MSG,
	TGL_SEEK_CLICK_MSG,
	TGL_MOVE_CUR_MSG,
	TGL_SHUFFLE_MSG,
	OSC_START_MSG,                                        // 64 osc csurfs max -->
	OSC_END_MSG = OSC_START_MSG+64,                       // <--
	ADD_REGION_START_MSG,                                 // 1024 marker/region indexes supported (*) -->
	ADD_REGION_END_MSG = ADD_REGION_START_MSG+1024,       // <--
	INSERT_REGION_START_MSG,                              // 1024 marker/region indexes supported (*) -->
	INSERT_REGION_END_MSG = INSERT_REGION_START_MSG+1024, // <--
							      // (*) but no max nb of supported regions/markers, of course
	LAST_MSG // keep as last item!
};

enum {
	BTNID_LOCK = LAST_MSG,
	BTNID_PLAY,
	BTNID_STOP,
	BTNID_REPEAT,
	TXTID_PLAYLIST,
	CMBID_PLAYLIST,
	WNDID_ADD_DEL,
	BTNID_NEW_PLAYLIST,
	BTNID_DEL_PLAYLIST,
	BTNID_PASTE,
	TXTID_MONITOR_PL,
	WNDID_MONITORS,
	TXTID_MON0,
	TXTID_MON1,
	TXTID_MON2,
	TXTID_MON3,
	TXTID_MON4
};


SNM_WindowManager<RegionPlaylistWnd> g_rgnplWndMgr(RGNPL_WND_ID);
SWSProjConfig<RegionPlaylists> g_pls;
SNM_OscCSurf* g_osc = NULL;


// user prefs
char g_rgnplBigFontName[64] = SNM_DYN_FONT_NAME;
bool g_monitorMode = false;
bool g_repeatPlaylist = false;	// playlist repeat state
bool g_seekImmediate = false;
bool g_shufflePlaylist = false;  // Playlist shuffle state.
int g_optionFlags = 0;

// see PlaylistRun()
int g_playPlaylist = -1;		// -1: stopped, playlist id otherwise
bool g_unsync = false;			// true when switching to a position that is not part of the playlist
int g_playCur = -1;				// index of the item being played, -1 means "not playing yet"
int g_playNext = -1;			// index of the next item to be played, -1 means "the end"
int g_rgnLoop = 0;				// region loop count: 0 not looping, <0 infinite loop, n>0 looping n times
bool g_plLoop = false;			// other (corner case) loops in the playlist?
double g_lastRunPos = -1.0;
double g_nextRgnPos, g_nextRgnEnd;
double g_curRgnPos = 0.0, g_curRgnEnd = -1.0; // to detect unsync, end<pos means non relevant

int g_oldSeekPref = -1;
int g_oldStopprojlenPref = -1;
int g_oldRepeatState = -1;


// _plId: -1 for the displayed/edited playlist
RegionPlaylist* GetPlaylist(int _plId = -1) {
	if (_plId < 0) _plId = g_pls.Get()->m_editId;
	return g_pls.Get()->Get(_plId);
}


///////////////////////////////////////////////////////////////////////////////
// RegionPlaylist
///////////////////////////////////////////////////////////////////////////////

RegionPlaylist::RegionPlaylist(RegionPlaylist* _pl, const char* _name)
	: m_name(_name), WDL_PtrList<RgnPlaylistItem>()
{
	if (_pl)
	{
		for (int i=0; i<_pl->GetSize(); i++)
			Add(new RgnPlaylistItem(_pl->Get(i)->m_rgnId, _pl->Get(i)->m_cnt));
		if (!_name)
			m_name.Set(_pl->m_name.Get());
	}
}

bool RegionPlaylist::IsValidIem(int _i)
{
	if (RgnPlaylistItem* item = Get(_i))
		return item->IsValidIem();
	return false;
}

// return the first found playlist idx for _pos
int RegionPlaylist::IsInPlaylist(double _pos, bool _repeat, int _startWith)
{
	double rgnpos, rgnend;
	for (int i=_startWith; i<GetSize(); i++)
		if (RgnPlaylistItem* plItem = Get(i))
			if (plItem->m_rgnId>0 && plItem->m_cnt!=0 && EnumMarkerRegionById(NULL, plItem->m_rgnId, NULL, &rgnpos, &rgnend, NULL, NULL, NULL)>=0)
				if (_pos >= rgnpos && _pos <= rgnend)
					return i;
	// 2nd try
	if (_repeat)
		for (int i=0; i<_startWith; i++)
			if (RgnPlaylistItem* plItem = Get(i))
				if (plItem->m_rgnId>0 && plItem->m_cnt!=0 && EnumMarkerRegionById(NULL, plItem->m_rgnId, NULL, &rgnpos, &rgnend, NULL, NULL, NULL)>=0)
					if (_pos >= rgnpos && _pos <= rgnend)
						return i;
	return -1;
}

int RegionPlaylist::IsInfinite()
{
	int num;
	for (int i=0; i<GetSize(); i++)
		if (RgnPlaylistItem* plItem = Get(i))
			if (plItem->m_rgnId>0 && plItem->m_cnt<0 && EnumMarkerRegionById(NULL, plItem->m_rgnId, NULL, NULL, NULL, NULL, &num, NULL)>=0)
				return num;
	return -1;
}

// returns playlist length is seconds, with a negative value if it contains infinite loops
double RegionPlaylist::GetLength()
{
	bool infinite = false;
	double length=0.0;
	for (int i=0; i<GetSize(); i++)
	{
		double rgnpos, rgnend;
		if (RgnPlaylistItem* plItem = Get(i))
		{
			if (plItem->m_rgnId>0 && plItem->m_cnt!=0 && EnumMarkerRegionById(NULL, plItem->m_rgnId, NULL, &rgnpos, &rgnend, NULL, NULL, NULL)>=0) {
				infinite |= plItem->m_cnt<0;
				length += ((rgnend-rgnpos) * abs(plItem->m_cnt));
			}
		}
	}
	return infinite ? length*(-1) : length;
}

// get the 1st marker/region num which has nested marker/region
int RegionPlaylist::GetNestedMarkerRegion()
{
	for (int i=0; i<GetSize(); i++)
	{
		if (RgnPlaylistItem* plItem = Get(i))
		{
			double rgnpos, rgnend;
			int num, rgnidx = EnumMarkerRegionById(NULL, plItem->m_rgnId, NULL, &rgnpos, &rgnend, NULL, &num, NULL);
			if (rgnidx>=0)
			{
				int x=0, lastx=0; double dPos, dEnd; bool isRgn;
				while ((x = EnumProjectMarkers2(NULL, x, &isRgn, &dPos, &dEnd, NULL, NULL)))
				{
					if (rgnidx != lastx)
					{
						if (isRgn)
						{
							// issue 613 => use SNM_FUDGE_FACTOR to skip adjacent regions
							if ((dPos>=(rgnpos+SNM_FUDGE_FACTOR) && dPos<=(rgnend-SNM_FUDGE_FACTOR)) || 
								(dEnd>=(rgnpos+SNM_FUDGE_FACTOR) && dEnd<=(rgnend-SNM_FUDGE_FACTOR)))
								return num;
						}
						else if (dPos>=(rgnpos+SNM_FUDGE_FACTOR) && dPos<=(rgnend-SNM_FUDGE_FACTOR))
							return num;
					}
					lastx=x;
				}
			}
		}
	}
	return -1;
}

// get the 1st marker/region num which has a marker/region > _pos
int RegionPlaylist::GetGreaterMarkerRegion(double _pos)
{
	for (int i=0; i<GetSize(); i++)
		if (RgnPlaylistItem* plItem = Get(i))
		{
			double rgnpos; int num;
			if (EnumMarkerRegionById(NULL, plItem->m_rgnId, NULL, &rgnpos, NULL, NULL, &num, NULL)>=0 && rgnpos>_pos)
				return num;
		}
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
// RegionPlaylistView
///////////////////////////////////////////////////////////////////////////////

enum {
  COL_RGN=0,
  COL_RGN_NAME,
  COL_RGN_COUNT,
  COL_RGN_START,
  COL_RGN_END,
  COL_RGN_LEN,
  COL_COUNT
};

// !WANT_LOCALIZE_STRINGS_BEGIN:sws_DLG_165
static SWS_LVColumn s_playlistCols[] = { {50,2,"#"}, {150,1,"Name"}, {70,1,"Loop count"}, {50,2,"Start"}, {50,2,"End"}, {50,2,"Length"} };
// !WANT_LOCALIZE_STRINGS_END

RegionPlaylistView::RegionPlaylistView(HWND hwndList, HWND hwndEdit)
	: SWS_ListView(hwndList, hwndEdit, COL_COUNT, s_playlistCols, "RgnPlaylistViewState", false, "sws_DLG_165", false)
{
}

// "compact" the playlist 
// (e.g. 2 consecutive regions "7" are merged into one with loop counter = 2)
void RegionPlaylistView::UpdateCompact()
{
	if (RegionPlaylist* pl = GetPlaylist())
		for (int i=pl->GetSize()-1; i>=0 ; i--)
			if (RgnPlaylistItem* item = pl->Get(i))
				if ((i-1)>=0 && pl->Get(i-1) && item->m_rgnId == pl->Get(i-1)->m_rgnId)
				{
					bool infinite = (pl->Get(i-1)->m_cnt<0 || item->m_cnt<0);
					pl->Get(i-1)->m_cnt = abs(pl->Get(i-1)->m_cnt) + abs(item->m_cnt);
					if (infinite)
						pl->Get(i-1)->m_cnt *= (-1);
					pl->Delete(i, true);
				}
	Update();
}

void RegionPlaylistView::GetItemText(SWS_ListItem* item, int iCol, char* str, int iStrMax)
{
	if (str) *str = '\0';
	if (RgnPlaylistItem* pItem = (RgnPlaylistItem*)item)
	{
		switch (iCol)
		{
			case COL_RGN: {
				RegionPlaylist* curpl = GetPlaylist();
				snprintf(str, iStrMax, "%s %d", 
					curpl && g_playPlaylist>=0 && curpl==GetPlaylist(g_playPlaylist) ? // current playlist being played?
					(!g_unsync && curpl->Get(g_playCur)==pItem ? UTF8_BULLET : (curpl->Get(g_playNext)==pItem ? UTF8_CIRCLE : " ")) : " ", GetMarkerRegionNumFromId(pItem->m_rgnId));
				break;
			}
			case COL_RGN_NAME:
				if (EnumMarkerRegionDescById(NULL, pItem->m_rgnId, str, iStrMax, SNM_REGION_MASK, false, true, false)<0 /* && *str */)
					lstrcpyn(str, __LOCALIZE("Unknown region","sws_DLG_165"), iStrMax);
				break;
			case COL_RGN_COUNT:
				if (pItem->m_cnt < 0)
					snprintf(str, iStrMax, "%s", UTF8_INFINITY);
				else
					snprintf(str, iStrMax, "%d", pItem->m_cnt);
				break;
			case COL_RGN_START: {
				double pos;
				if (EnumMarkerRegionById(NULL, pItem->m_rgnId, NULL, &pos, NULL, NULL, NULL, NULL)>=0)
					format_timestr_pos(pos, str, iStrMax, -1);
				break;
			}
			case COL_RGN_END: {
				double end;
				if (EnumMarkerRegionById(NULL, pItem->m_rgnId, NULL, NULL, &end, NULL, NULL, NULL)>=0)
					format_timestr_pos(end, str, iStrMax, -1);
				break;
			}
			case COL_RGN_LEN: {
				double pos, end;
				if (EnumMarkerRegionById(NULL, pItem->m_rgnId, NULL, &pos, &end, NULL, NULL, NULL)>=0)
					format_timestr_len(end-pos, str, iStrMax, pos, -1);
				break;
			}
		}
	}
}

void RegionPlaylistView::GetItemList(SWS_ListItemList* pList)
{
	if (RegionPlaylist* pl = GetPlaylist())
		for (int i=0; i < pl->GetSize(); i++)
			if (SWS_ListItem* item = (SWS_ListItem*)pl->Get(i))
				pList->Add(item);
}

void RegionPlaylistView::SetItemText(SWS_ListItem* item, int iCol, const char* str)
{
	if (RgnPlaylistItem* pItem = (RgnPlaylistItem*)item)
	{
		switch (iCol)
		{
			case COL_RGN_COUNT:
				pItem->m_cnt = str && *str ? atoi(str) : 0;
				Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1); 
				PlaylistResync();
				break;
			case COL_RGN_NAME:
			{
				bool isrgn; double pos, end; int num, col;
				if (str && EnumMarkerRegionById(NULL, pItem->m_rgnId, &isrgn, &pos, &end, NULL, &num, &col)>=0)
				{
					SetProjectMarker4(NULL, num, isrgn, pos, end, str, col ? col | 0x1000000 : 0, !*str ? 1 : 0);
					Undo_OnStateChangeEx2(NULL, __LOCALIZE("Edit region name","sws_undo"), UNDO_STATE_MISCCFG, -1);
					// update notified back through PlaylistMarkerRegionListener
				}
				break;
			}
		}
	}
}

void RegionPlaylistView::OnItemClk(SWS_ListItem* item, int iCol, int iKeyState)
{
	if (RgnPlaylistItem* pItem = (RgnPlaylistItem*)item)
	{
		if (g_optionFlags&2)
			SetEditCurPos2(NULL, pItem->GetPos(), true, false); // move edit curdor, seek done below

		// do not use PERFORM_MSG here: depends on play state in this case
		if ((g_optionFlags&1) && GetPlaylist() && (GetPlayState()&1))
			PlaylistPlay(g_pls.Get()->m_editId, GetPlaylist()->Find(pItem)); // obeys g_seekImmediate
	}
}

void RegionPlaylistView::OnItemDblClk(SWS_ListItem* item, int iCol)
{
	if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get())
		w->OnCommand(PERFORM_MSG, 0);
}

// "disable" sort
int RegionPlaylistView::OnItemSort(SWS_ListItem* _item1, SWS_ListItem* _item2) 
{
	int i1=-1, i2=-1;
	if (RegionPlaylist* pl = GetPlaylist())
	{
		for (int i=0; (i1<0 && i2<0) || i<pl->GetSize(); i++)
		{
			SWS_ListItem* item = (SWS_ListItem*)pl->Get(i);
			if (item == _item1) i1=i;
			if (item == _item2) i2=i;
		}
		if (i1 >= 0 && i2 >= 0) {
			if (i1 > i2) return 1;
			else if (i1 < i2) return -1;
		}
	}
	return 0;
}

void RegionPlaylistView::OnBeginDrag(SWS_ListItem* item) {
	SetCapture(GetParent(m_hwndList));
}

void RegionPlaylistView::OnDrag()
{
	RegionPlaylist* pl  = GetPlaylist();
	if (!pl) return;

	POINT p; GetCursorPos(&p);
	if (RgnPlaylistItem* hitItem = (RgnPlaylistItem*)GetHitItem(p.x, p.y, NULL))
	{
		int iNewPriority = pl->Find(hitItem);
		int x=0, iSelPriority;
		while(RgnPlaylistItem* selItem = (RgnPlaylistItem*)EnumSelected(&x))
		{
			iSelPriority = pl->Find(selItem);
			if (iNewPriority == iSelPriority) return;
			m_draggedItems.Add(selItem);
		}

		bool bDir = iNewPriority > iSelPriority;
		for (int i = bDir ? 0 : m_draggedItems.GetSize()-1; bDir ? i < m_draggedItems.GetSize() : i >= 0; bDir ? i++ : i--) {
			pl->Delete(pl->Find(m_draggedItems.Get(i)), false);
			pl->Insert(iNewPriority, m_draggedItems.Get(i));
		}

		ListView_DeleteAllItems(m_hwndList); // because of the special sort criteria ("not sortable" somehow)
		Update(); // no UpdateCompact() here, it would crash! see OnEndDrag()

		for (int i=0; i < m_draggedItems.GetSize(); i++)
			SelectByItem((SWS_ListItem*)m_draggedItems.Get(i), i==0, i==0);
	}
}

void RegionPlaylistView::OnEndDrag()
{
	UpdateCompact();
	if (m_draggedItems.GetSize()) {
		Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1);
		m_draggedItems.Empty(false);
		PlaylistResync();
	}
}


///////////////////////////////////////////////////////////////////////////////
// Info strings for monitoring windows + OSC feedback
///////////////////////////////////////////////////////////////////////////////

void GetMonitoringInfo(WDL_FastString* _curNum, WDL_FastString* _cur, 
					   WDL_FastString* _nextNum, WDL_FastString* _next)
{
	if (g_playPlaylist>=0 && _curNum && _cur && _nextNum && _next)
	{
		if (RegionPlaylist* curpl = GetPlaylist(g_playPlaylist))
		{
			// current playlist item
			if (g_unsync)
			{
				_curNum->Set("!");
				_cur->Set(__LOCALIZE("<SYNC LOSS>)","sws_DLG_165"));
			}
			else if (RgnPlaylistItem* curItem = curpl->Get(g_playCur))
			{
				char buf[64]="";
				EnumMarkerRegionDescById(NULL, curItem->m_rgnId, buf, sizeof(buf), SNM_REGION_MASK, false, true, false);
				_curNum->SetFormatted(16, "%d", GetMarkerRegionNumFromId(curItem->m_rgnId));
				_cur->Set(buf);
			}
			// current item not foud (e.g. playlist switch, g_playCur = -1) 
			// => best effort: find region by play pos
			else
			{
				int id, idx = FindMarkerRegion(NULL, GetPlayPositionEx(NULL), SNM_REGION_MASK, &id);
				if (id > 0)
				{
					char buf[64]="";
					EnumMarkerRegionDesc(NULL, idx, buf, sizeof(buf), SNM_REGION_MASK, false, true, false);
					_curNum->SetFormatted(16, "%d", GetMarkerRegionNumFromId(id));
					_cur->Set(buf);
				}
			}

			// *** next playlist item ***
			if (g_playNext<0)
			{
				_nextNum->Set("-");
				_next->Set(__LOCALIZE("<END>","sws_DLG_165"));
			}
			else if (!g_unsync && g_rgnLoop && g_playCur>=0 && g_playCur==g_playNext)
			{
				if (g_rgnLoop>0)
				{
					_nextNum->Set(_curNum);
					_next->SetFormatted(32, __LOCALIZE_VERFMT("<LOOP: %d>","sws_DLG_165"), g_rgnLoop);
				}
				else if (g_rgnLoop<0)
				{
					_nextNum->Set(_curNum);
					_next->Set(UTF8_INFINITY);
				}
			}
			else if (RgnPlaylistItem* nextItem = curpl->Get(g_playNext))
			{
				char buf[64]="";
				EnumMarkerRegionDescById(NULL, nextItem->m_rgnId, buf, sizeof(buf), SNM_REGION_MASK, false, true, false);
				_nextNum->SetFormatted(16, "%d", GetMarkerRegionNumFromId(nextItem->m_rgnId));
				_next->Set(buf);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// RegionPlaylistWnd
///////////////////////////////////////////////////////////////////////////////

// S&M windows lazy init: below's "" prevents registering the SWS' screenset callback
// (use the S&M one instead - already registered via SNM_WindowManager::Init())
RegionPlaylistWnd::RegionPlaylistWnd()
	: SWS_DockWnd(IDD_SNM_RGNPLAYLIST, __LOCALIZE("Region Playlist","sws_DLG_165"), "")
{
	m_id.Set(RGNPL_WND_ID);
	// must call SWS_DockWnd::Init() to restore parameters and open the window if necessary
	Init();
}

RegionPlaylistWnd::~RegionPlaylistWnd()
{
	m_mons.RemoveAllChildren(false);
	m_mons.SetRealParent(NULL);
	m_btnsAddDel.RemoveAllChildren(false);
	m_btnsAddDel.SetRealParent(NULL);
}

void RegionPlaylistWnd::OnInitDlg()
{
	m_resize.init_item(IDC_LIST, 0.0, 0.0, 1.0, 1.0);
	m_pLists.Add(new RegionPlaylistView(GetDlgItem(m_hwnd, IDC_LIST), GetDlgItem(m_hwnd, IDC_EDIT)));

	LICE_CachedFont* font = SNM_GetThemeFont();

	m_vwnd_painter.SetGSC(WDL_STYLE_GetSysColor);
	m_parentVwnd.SetRealParent(m_hwnd);
	
	m_btnLock.SetID(BTNID_LOCK);
	m_parentVwnd.AddChild(&m_btnLock);

	m_btnPlay.SetID(BTNID_PLAY);
	m_parentVwnd.AddChild(&m_btnPlay);
	m_btnStop.SetID(BTNID_STOP);
	m_parentVwnd.AddChild(&m_btnStop);
	m_btnRepeat.SetID(BTNID_REPEAT);
	m_parentVwnd.AddChild(&m_btnRepeat);

	m_txtPlaylist.SetID(TXTID_PLAYLIST);
	m_txtPlaylist.SetFont(font);
	m_parentVwnd.AddChild(&m_txtPlaylist);

	m_cbPlaylist.SetID(CMBID_PLAYLIST);
	m_cbPlaylist.SetFont(font);
	FillPlaylistCombo();
	m_parentVwnd.AddChild(&m_cbPlaylist);

	m_btnAdd.SetID(BTNID_NEW_PLAYLIST);
	m_btnsAddDel.AddChild(&m_btnAdd);
	m_btnDel.SetID(BTNID_DEL_PLAYLIST);
	m_btnsAddDel.AddChild(&m_btnDel);
	m_btnsAddDel.SetID(WNDID_ADD_DEL);
	m_parentVwnd.AddChild(&m_btnsAddDel);

	m_btnCrop.SetID(BTNID_PASTE);
	m_parentVwnd.AddChild(&m_btnCrop);

	m_monPl.SetID(TXTID_MONITOR_PL);
	m_monPl.SetFontName(g_rgnplBigFontName);
	m_monPl.SetAlign(DT_LEFT, DT_CENTER);
	m_parentVwnd.AddChild(&m_monPl);

	for (int i=0; i<5; i++)
		m_txtMon[i].SetID(TXTID_MON0+i);
	m_mons.AddMonitors(&m_txtMon[0], &m_txtMon[1], &m_txtMon[2], &m_txtMon[3], &m_txtMon[4]);
	m_mons.SetID(WNDID_MONITORS);
	m_mons.SetFontName(g_rgnplBigFontName);
	m_mons.SetTitles(__LOCALIZE("CURRENT","sws_DLG_165"), " ", __LOCALIZE("NEXT","sws_DLG_165"), " ");  // " " trick to get a lane
	m_parentVwnd.AddChild(&m_mons);

	Update();

	RegisterToMarkerRegionUpdates(&m_mkrRgnListener);
}

void RegionPlaylistWnd::OnDestroy()
{
	UnregisterToMarkerRegionUpdates(&m_mkrRgnListener);
	m_cbPlaylist.Empty();
	m_mons.RemoveAllChildren(false);
	m_mons.SetRealParent(NULL);
	m_btnsAddDel.RemoveAllChildren(false);
	m_btnsAddDel.SetRealParent(NULL);
}

// _flags: &1=fast update, normal/full update otherwise
void RegionPlaylistWnd::Update(int _flags, WDL_FastString* _curNum, WDL_FastString* _cur, WDL_FastString* _nextNum, WDL_FastString* _next)
{
	static bool sRecurseCheck = false;
	if (sRecurseCheck)
		return;
	sRecurseCheck = true;

	ShowWindow(GetDlgItem(m_hwnd, IDC_LIST), !g_monitorMode && g_pls.Get()->GetSize() ? SW_SHOW : SW_HIDE);

	// normal/full update
	if (!_flags)
	{
		if (m_pLists.GetSize())
			((RegionPlaylistView*)GetListView())->UpdateCompact();

		UpdateMonitoring(_curNum, _cur, _nextNum, _next);
		m_parentVwnd.RequestRedraw(NULL);
	}
	// "fast" update (used while playing) - exclusive with the above!
	else if (_flags&1)
	{
		// monitoring mode
		if (g_monitorMode)
			UpdateMonitoring(_curNum, _cur, _nextNum, _next);
		// edition mode
		else if (g_pls.Get()->m_editId == g_playPlaylist && m_pLists.GetSize()) // is it the displayed playlist?
			((RegionPlaylistView*)GetListView())->Update(); // no playlist compacting
	}

	sRecurseCheck = false;
}

// just update monitoring VWnds
// params: optional, for optimization while playing: monitor wnds + osc in one go, see PlaylistRun()
//         they should be ALL valid or ALL null, although a mix is managed..
void RegionPlaylistWnd::UpdateMonitoring(WDL_FastString* _curNum, WDL_FastString* _cur, WDL_FastString* _nextNum, WDL_FastString* _next)
{
	WDL_FastString pl;
	if (g_playPlaylist>=0)
		if (RegionPlaylist* curpl = GetPlaylist(g_playPlaylist))
			pl.SetFormatted(128, "#%d \"%s\"", g_playPlaylist+1, curpl->m_name.Get());
	m_monPl.SetText(g_playPlaylist>=0 ? pl.Get() : __LOCALIZE("<STOPPED>","sws_DLG_165"));

#ifdef _SNM_MISC
	// big fonts with alpha doesn't work well ATM (on OS X at least), such overlapped texts look a bit clunky anyway...
	pl.Set("");
	if (g_playPlaylist>=0)
		pl.SetFormatted(16, "#%d", g_playPlaylist+1);
	m_mons.SetText(0, pl.Get(), 0, 16);
#endif

	WDL_FastString *cur=_cur, *curNum=_curNum, *next=_next, *nextNum=_nextNum;
	if (!_cur) cur = new WDL_FastString;
	if (!_curNum) curNum = new WDL_FastString;
	if (!_next) next = new WDL_FastString;
	if (!_nextNum) nextNum = new WDL_FastString;
	if (!_cur||!_curNum||!_next||!_nextNum)
		GetMonitoringInfo(curNum, cur, nextNum, next);

	m_mons.SetText(1, curNum->Get(), g_playPlaylist<0 ? 0 : g_unsync ? SNM_COL_RED_MONITOR : 0);
	m_mons.SetText(2, cur->Get(), g_playPlaylist<0 ? 0 : g_unsync ? SNM_COL_RED_MONITOR : 0);
	m_mons.SetText(3, nextNum->Get(), 0, 153);
	m_mons.SetText(4, next->Get(), 0, 153);

	if (!_cur) delete cur;
	if (!_curNum) delete curNum;
	if (!_next) delete next;
	if (!_nextNum) delete nextNum;
}

void RegionPlaylistWnd::FillPlaylistCombo()
{
	m_cbPlaylist.Empty();
	for (int i=0; i < g_pls.Get()->GetSize(); i++)
	{
		char name[128]="";
		snprintf(name, sizeof(name), "%d - %s", i+1, g_pls.Get()->Get(i)->m_name.Get());
		m_cbPlaylist.AddItem(name);
	}
	m_cbPlaylist.SetCurSel(g_pls.Get()->m_editId);
}

void RegionPlaylistWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch(LOWORD(wParam))
	{
		case BTNID_LOCK:
			if (!HIWORD(wParam))
				ToggleLock();
			break;
		case CMBID_PLAYLIST:
			if (HIWORD(wParam)==CBN_SELCHANGE)
			{
				// stop cell editing (changing the list content would be ignored otherwise,
				// leading to unsynchronized dropdown box vs list view)
				GetListView()->EditListItemEnd(false);
				g_pls.Get()->m_editId = m_cbPlaylist.GetCurSel();
				Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1);
				Update();
			}
			break;
		case NEW_PLAYLIST_MSG:
		case BTNID_NEW_PLAYLIST:
		case COPY_PLAYLIST_MSG:
		{
			char name[64]="";
			lstrcpyn(name, __LOCALIZE("Untitled","sws_DLG_165"), 64);
			if (PromptUserForString(m_hwnd, __LOCALIZE("S&M - Add playlist","sws_DLG_165"), name, 64, true) && *name)
			{
				if (g_pls.Get()->Add(new RegionPlaylist(LOWORD(wParam)==COPY_PLAYLIST_MSG ? GetPlaylist() : NULL, name)))
				{
					g_pls.Get()->m_editId = g_pls.Get()->GetSize()-1;
					FillPlaylistCombo();
					Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1); 
					Update();
				}
			}
			break;
		}
		case DEL_PLAYLIST_MSG:
		case BTNID_DEL_PLAYLIST:
			if (GetPlaylist() && g_pls.Get()->GetSize() > 0)
			{
				WDL_PtrList_DeleteOnDestroy<RegionPlaylist> delItems;
				int reply = IDYES;
				if (GetPlaylist()->GetSize()) // do not ask if empty
				{
					char msg[128] = "";
					snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("Are you sure you want to delete the playlist #%d \"%s\"?","sws_DLG_165"), g_pls.Get()->m_editId+1, GetPlaylist()->m_name.Get());
					reply = MessageBox(m_hwnd, msg, __LOCALIZE("S&M - Delete playlist","sws_DLG_165"), MB_YESNO);
				}
				if (reply != IDNO)
				{
					// updatte vars if playing
					if (g_playPlaylist>=0) {
						if (g_playPlaylist==g_pls.Get()->m_editId) PlaylistStop();
						else if (g_playPlaylist>g_pls.Get()->m_editId) g_playPlaylist--;
					}
					delItems.Add(g_pls.Get()->Get(g_pls.Get()->m_editId));
					g_pls.Get()->Delete(g_pls.Get()->m_editId, false); // no deletion yet (still used in GUI)
					g_pls.Get()->m_editId = BOUNDED(g_pls.Get()->m_editId-1, 0, g_pls.Get()->GetSize()-1);
					FillPlaylistCombo();
					Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1); 
					Update();
				}
			} // + delItems cleanup
			break;
		case REN_PLAYLIST_MSG:
			if (GetPlaylist() && g_pls.Get()->GetSize() > 0)
			{
				char newName[64]="";
				lstrcpyn(newName, GetPlaylist()->m_name.Get(), 64);
				if (PromptUserForString(m_hwnd, __LOCALIZE("S&M - Rename playlist","sws_DLG_165"), newName, 64, true) && *newName)
				{
					GetPlaylist()->m_name.Set(newName);
					FillPlaylistCombo();
					Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1); 
					Update();
				}
			}
			break;
		case BTNID_PLAY:
			PlaylistPlay(g_pls.Get()->m_editId, GetNextValidItem(g_pls.Get()->m_editId, 0, true, g_repeatPlaylist, g_shufflePlaylist));
			break;
		case BTNID_STOP:
			OnStopButton();
			break;
		case BTNID_REPEAT:
			SetPlaylistRepeat(NULL);
			break;
		case BTNID_PASTE:
			RECT r; m_btnCrop.GetPositionInTopVWnd(&r);
			ClientToScreen(m_hwnd, (LPPOINT)&r);
			ClientToScreen(m_hwnd, ((LPPOINT)&r)+1);
			SendMessage(m_hwnd, WM_CONTEXTMENU, 0, MAKELPARAM((UINT)(r.left), (UINT)(r.bottom+SNM_1PIXEL_Y)));
			break;
		case CROP_PRJ_MSG:
			AppendPasteCropPlaylist(GetPlaylist(), CROP_PROJECT);
			break;
		case CROP_PRJTAB_MSG:
			AppendPasteCropPlaylist(GetPlaylist(), CROP_PROJECT_TAB);
			break;
		case APPEND_PRJ_MSG:
			AppendPasteCropPlaylist(GetPlaylist(), PASTE_PROJECT);
			break;
		case PASTE_CURSOR_MSG:
			AppendPasteCropPlaylist(GetPlaylist(), PASTE_CURSOR);
			break;
		case DELETE_MSG:
		{
			int x=0, slot; bool updt = false;
			WDL_PtrList_DeleteOnDestroy<RgnPlaylistItem> delItems;
			while(RgnPlaylistItem* item = (RgnPlaylistItem*)GetListView()->EnumSelected(&x))
			{
				slot = GetPlaylist()->Find(item);
				if (slot>=0)
				{
					delItems.Add(item);
					GetPlaylist()->Delete(slot, false);  // no deletion yet (still used in GUI)
					updt=true;
				}
			}
			if (updt)
			{
				Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1);
				PlaylistResync();
				Update();
			} // + delItems cleanup
			break;
		}
		case TGL_INFINITE_LOOP_MSG:
		{
			int x=0; bool updt = false;
			while(RgnPlaylistItem* item = (RgnPlaylistItem*)GetListView()->EnumSelected(&x)) {
				item->m_cnt *= (-1);
				updt = true;
			}
			if (updt)
			{
				Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1); 
				PlaylistResync();
				Update();
			}
			break;
		}
		case TGL_SEEK_NOW_MSG:
			g_seekImmediate = !g_seekImmediate;
			break;
		case TGL_SHUFFLE_MSG:
			SetPlaylistOptionShuffle(nullptr);
			break;
		case TGL_SEEK_CLICK_MSG:
			if (g_optionFlags&1) g_optionFlags &= ~1;
			else g_optionFlags |= 1;
			break;
		case TGL_MOVE_CUR_MSG:
			if (g_optionFlags&2) g_optionFlags &= ~2;
			else g_optionFlags |= 2;
			break;
		case APPEND_SEL_RGN_MSG:
		case PASTE_SEL_RGN_MSG:
		{
			RegionPlaylist p("temp");
			int x=0;
			while(RgnPlaylistItem* item = (RgnPlaylistItem*)GetListView()->EnumSelected(&x))
				p.Add(new RgnPlaylistItem(item->m_rgnId, item->m_cnt));
			AppendPasteCropPlaylist(&p, LOWORD(wParam) == PASTE_SEL_RGN_MSG ? PASTE_CURSOR : PASTE_PROJECT);
			break;
		}
		case PERFORM_MSG:
			if (GetPlaylist())
			{
				int x=0;
				if (RgnPlaylistItem* pItem = (RgnPlaylistItem*)GetListView()->EnumSelected(&x))
					PlaylistPlay(g_pls.Get()->m_editId, GetPlaylist()->Find(pItem)); // obeys g_seekImmediate
			}
			break;
		case ADD_ALL_REGIONS_MSG:
		{
			int x=0, y, num; bool isRgn, updt=false;
			while ((y = EnumProjectMarkers2(NULL, x, &isRgn, NULL, NULL, NULL, &num)))
			{
				if (isRgn) {
					RgnPlaylistItem* newItem = new RgnPlaylistItem(MakeMarkerRegionId(num, isRgn));
					updt |= (GetPlaylist() && GetPlaylist()->Add(newItem) != NULL);
				}
				x=y;
			}
			if (updt)
			{
				Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1);
				PlaylistResync();
				Update();
			}
			else
				MessageBox(m_hwnd, __LOCALIZE("No region found in project!","sws_DLG_165"), __LOCALIZE("S&M - Error","sws_DLG_165"), MB_OK);
			break;
		}
		default:
			if (LOWORD(wParam) >= INSERT_REGION_START_MSG && LOWORD(wParam) <= INSERT_REGION_END_MSG)
			{
				RegionPlaylist* pl = GetPlaylist();
				if (!pl) break;
				RgnPlaylistItem* newItem = new RgnPlaylistItem(GetMarkerRegionIdFromIndex(NULL, LOWORD(wParam)-INSERT_REGION_START_MSG));
				if (pl->GetSize())
				{
					if (RgnPlaylistItem* item = (RgnPlaylistItem*)GetListView()->EnumSelected(NULL))
					{
						int slot = pl->Find(item);
						if (slot >= 0 && pl->Insert(slot, newItem))
						{
							Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1); 
							PlaylistResync();
							Update();
							GetListView()->SelectByItem((SWS_ListItem*)newItem);
							return;
						}
					}
				}
				// empty list, no selection, etc.. => add
				if (pl->Add(newItem))
				{
					Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1); 
					PlaylistResync();
					Update();
					GetListView()->SelectByItem((SWS_ListItem*)newItem);
				}
			}
			else if (LOWORD(wParam) >= ADD_REGION_START_MSG && LOWORD(wParam) <= ADD_REGION_END_MSG)
			{
				RgnPlaylistItem* newItem = new RgnPlaylistItem(GetMarkerRegionIdFromIndex(NULL, LOWORD(wParam)-ADD_REGION_START_MSG));
				if (GetPlaylist() && GetPlaylist()->Add(newItem))
				{
					Undo_OnStateChangeEx2(NULL, UNDO_PLAYLIST_STR, UNDO_STATE_MISCCFG, -1); 
					PlaylistResync();
					Update();
					GetListView()->SelectByItem((SWS_ListItem*)newItem);
				}
			}
			else if (LOWORD(wParam) >= OSC_START_MSG && LOWORD(wParam) <= OSC_END_MSG) 
			{
				DELETE_NULL(g_osc);
				WDL_PtrList_DeleteOnDestroy<SNM_OscCSurf> oscCSurfs;
				LoadOscCSurfs(&oscCSurfs);
				if (SNM_OscCSurf* osc = oscCSurfs.Get((int)LOWORD(wParam)-1 - OSC_START_MSG))
					g_osc = new SNM_OscCSurf(osc);
			}
			else
				Main_OnCommand((int)wParam, (int)lParam);
			break;
	}
}

int RegionPlaylistWnd::OnKey(MSG* _msg, int _iKeyState) 
{
	if (_msg->message == WM_KEYDOWN && !_iKeyState)
	{
		switch(_msg->wParam)
		{
			case VK_DELETE:
				OnCommand(DELETE_MSG, 0);
				return 1;
			case VK_RETURN:
				OnCommand(PERFORM_MSG, 0);
				return 1;
		}
	}
	return 0;
}

// play/stop/pause buttons are right aligned when _RGNPL_TRANSPORT_RIGHT is defined
#define _RGNPL_TRANSPORT_RIGHT

void RegionPlaylistWnd::DrawControls(LICE_IBitmap* _bm, const RECT* _r, int* _tooltipHeight)
{
	IconTheme* it = SNM_GetIconTheme();
	int x0=_r->left+SNM_GUI_X_MARGIN, h=SNM_GUI_TOP_H;
	if (_tooltipHeight)
		*_tooltipHeight = h;
	
	bool hasPlaylists = (g_pls.Get()->GetSize()>0);
	RegionPlaylist* pl = GetPlaylist();

	SNM_SkinButton(&m_btnLock, it ? &it->toolbar_lock[!g_monitorMode] : NULL, g_monitorMode ? __LOCALIZE("Edition mode","sws_DLG_165") : __LOCALIZE("Monitoring mode","sws_DLG_165"));
	if (SNM_AutoVWndPosition(DT_LEFT, &m_btnLock, NULL, _r, &x0, _r->top, h))
	{
#ifndef _RGNPL_TRANSPORT_RIGHT
		SNM_SkinButton(&m_btnPlay, it ? &it->gen_play[g_playPlaylist>=0?1:0] : NULL, __LOCALIZE("Play","sws_DLG_165"));
		if (SNM_AutoVWndPosition(DT_LEFT, &m_btnPlay, NULL, _r, &x0, _r->top, h, 0))
		{
			SNM_SkinButton(&m_btnStop, it ? &(it->gen_stop) : NULL, __LOCALIZE("Stop","sws_DLG_165"));
			if (SNM_AutoVWndPosition(DT_LEFT, &m_btnStop, NULL, _r, &x0, _r->top, h, 0))
			{
				SNM_SkinButton(&m_btnRepeat, it ? &it->gen_repeat[g_repeatPlaylist?1:0] : NULL, __LOCALIZE("Repeat","sws_DLG_165"));
				if (SNM_AutoVWndPosition(DT_LEFT, &m_btnRepeat, NULL, _r, &x0, _r->top, h))
#endif
				{
					// *** monitoring ***
					if (g_monitorMode)
					{
						RECT r = *_r;

						r.top += int(SNM_GUI_TOP_H/2+0.5);
						r.bottom -= int(SNM_GUI_TOP_H/2);
						m_mons.SetPosition(&r);
						m_mons.SetVisible(true);
						
						r = *_r;

						// playlist name
						r.left = x0;
						r.bottom = h;
#ifndef _RGNPL_TRANSPORT_RIGHT
						LICE_IBitmap* logo = SNM_GetThemeLogo();
//						r.right -= logo ? logo->getWidth()+5 : 5; // 5: margin
#else
						r.right -= (75+SNM_GUI_X_MARGIN); //JFB lazy here.. should be play+stop+repeat btn widths
#endif
						m_monPl.SetPosition(&r);
						m_monPl.SetVisible(true);
					}

					// *** edition ***
					else
					{
						m_txtPlaylist.SetText(hasPlaylists ? __LOCALIZE("Playlist #","sws_DLG_165") : __LOCALIZE("Playlist: None","sws_DLG_165"));
						if (SNM_AutoVWndPosition(DT_LEFT, &m_txtPlaylist, NULL, _r, &x0, _r->top, h, hasPlaylists?4:SNM_DEF_VWND_X_STEP))
						{
							if (!hasPlaylists || SNM_AutoVWndPosition(DT_LEFT, &m_cbPlaylist, &m_txtPlaylist, _r, &x0, _r->top, h, 4))
							{
								m_btnDel.SetEnabled(hasPlaylists);
								if (SNM_AutoVWndPosition(DT_LEFT, &m_btnsAddDel, NULL, _r, &x0, _r->top, h))
								{
									if (abs(hasPlaylists && pl ? pl->GetLength() : 0.0) > 0.0) // <0.0 means infinite
									{
										SNM_SkinToolbarButton(&m_btnCrop, __LOCALIZE("Edit project","sws_DLG_165"));
										if (SNM_AutoVWndPosition(DT_LEFT, &m_btnCrop, NULL, _r, &x0, _r->top, h))
										{
#ifndef _RGNPL_TRANSPORT_RIGHT
											SNM_AddLogo(_bm, _r, x0, h);
#endif
										}
									}
#ifndef _RGNPL_TRANSPORT_RIGHT
									else
										SNM_AddLogo(_bm, _r, x0, h);
#endif
								}
							}
						}
					}
				}
#ifndef _RGNPL_TRANSPORT_RIGHT
			}
		}
#endif
	}

#ifdef _RGNPL_TRANSPORT_RIGHT
	x0 = _r->right-SNM_GUI_X_MARGIN;
	SNM_SkinButton(&m_btnRepeat, it ? &it->gen_repeat[g_repeatPlaylist?1:0] : NULL, __LOCALIZE("Repeat","sws_DLG_165"));
	if (SNM_AutoVWndPosition(DT_RIGHT, &m_btnRepeat, NULL, _r, &x0, _r->top, h, 0))
	{
		SNM_SkinButton(&m_btnStop, it ? &(it->gen_stop) : NULL, __LOCALIZE("Stop","sws_DLG_165"));
		if (SNM_AutoVWndPosition(DT_RIGHT, &m_btnStop, NULL, _r, &x0, _r->top, h, 0))
		{
			SNM_SkinButton(&m_btnPlay, it ? &it->gen_play[g_playPlaylist>=0?1:0] : NULL, __LOCALIZE("Play","sws_DLG_165"));
			SNM_AutoVWndPosition(DT_RIGHT, &m_btnPlay, NULL, _r, &x0, _r->top, h, 0);
		}
	}
	if (g_monitorMode)
		SNM_AddLogo(_bm, _r);
#endif
}

void RegionPlaylistWnd::PlaylistMenu(HMENU _menu)
{
	if (GetMenuItemCount(_menu))
		AddToMenu(_menu, SWS_SEPARATOR, 0);
	AddToMenu(_menu, __LOCALIZE("New playlist...","sws_DLG_165"), NEW_PLAYLIST_MSG);
	AddToMenu(_menu, __LOCALIZE("Copy playlist...","sws_DLG_165"), COPY_PLAYLIST_MSG, -1, false, GetPlaylist() ? MF_ENABLED : MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Delete","sws_DLG_165"), DEL_PLAYLIST_MSG, -1, false, GetPlaylist() ? MF_ENABLED : MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Rename...","sws_DLG_165"), REN_PLAYLIST_MSG, -1, false, GetPlaylist() ? MF_ENABLED : MF_GRAYED);
}

void RegionPlaylistWnd::EditMenu(HMENU _menu)
{
	if (GetMenuItemCount(_menu))
		AddToMenu(_menu, SWS_SEPARATOR, 0);
	AddToMenu(_menu, __LOCALIZE("Crop project to playlist","sws_DLG_165"), CROP_PRJ_MSG, -1, false, GetPlaylist() && GetPlaylist()->GetSize() ? 0 : MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Crop project to playlist (new project tab)","sws_DLG_165"), CROP_PRJTAB_MSG, -1, false, GetPlaylist() && GetPlaylist()->GetSize() ? 0 : MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Append playlist to project","sws_DLG_165"), APPEND_PRJ_MSG, -1, false, GetPlaylist() && GetPlaylist()->GetSize() ? 0 : MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Paste playlist at edit cursor","sws_DLG_165"), PASTE_CURSOR_MSG, -1, false, GetPlaylist() && GetPlaylist()->GetSize() ? 0 : MF_GRAYED);

	int x=0; bool hasSel = GetListView()->EnumSelected(&x) != NULL;
	AddToMenu(_menu, SWS_SEPARATOR, 0);
	AddToMenu(_menu, __LOCALIZE("Append selected regions to project","sws_DLG_165"), APPEND_SEL_RGN_MSG, -1, false, hasSel ? 0 : MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Paste selected regions at edit cursor","sws_DLG_165"), PASTE_SEL_RGN_MSG, -1, false, hasSel ? 0 : MF_GRAYED);
}

void RegionPlaylistWnd::OptionsMenu(HMENU _menu)
{
	if (GetMenuItemCount(_menu))
		AddToMenu(_menu, SWS_SEPARATOR, 0);
	AddToMenu(_menu, __LOCALIZE("Move edit cursor when clicking regions","sws_DLG_165"), TGL_MOVE_CUR_MSG, -1, false, g_optionFlags&2 ? MF_CHECKED : MF_UNCHECKED);
	AddToMenu(_menu, SWS_SEPARATOR, 0);
	AddToMenu(_menu, __LOCALIZE("Seek playback when clicking regions","sws_DLG_165"), TGL_SEEK_CLICK_MSG, -1, false, g_optionFlags&1 ? MF_CHECKED : MF_UNCHECKED);
	AddToMenu(_menu, __LOCALIZE("Seek/start playback when double-clicking regions","sws_DLG_165"), -1, -1, false, MF_CHECKED|MF_GRAYED); // just a helper item..
	AddToMenu(_menu, __LOCALIZE("Smooth seek (seek immediately if disabled)","sws_DLG_165"), TGL_SEEK_NOW_MSG, -1, false, !g_seekImmediate ? MF_CHECKED : MF_UNCHECKED);
	AddToMenu(_menu, __LOCALIZE("Shuffle playlist items","sws_DLG_165"), TGL_SHUFFLE_MSG, -1, false, g_shufflePlaylist ? MF_CHECKED : MF_UNCHECKED);
//	AddToMenu(_menu, SWS_SEPARATOR, 0);
//	AddToMenu(_menu, __LOCALIZE("Repeat playlist","sws_DLG_165"), BTNID_REPEAT, -1, false, g_repeatPlaylist ? MF_CHECKED : MF_UNCHECKED);	
}

HMENU RegionPlaylistWnd::OnContextMenu(int _x, int _y, bool* _wantDefaultItems)
{
	HMENU hMenu = CreatePopupMenu();
	if (!g_monitorMode)
	{
		// specific context menu for the paste button
		POINT p; GetCursorPos(&p);
		ScreenToClient(m_hwnd,&p);
		if (WDL_VWnd* v = m_parentVwnd.VirtWndFromPoint(p.x,p.y,1))
			switch (v->GetID())
			{
				case BTNID_PASTE:
					*_wantDefaultItems = false;
					EditMenu(hMenu);
					return hMenu;
				case TXTID_PLAYLIST:
				case CMBID_PLAYLIST:
					*_wantDefaultItems = false;
					PlaylistMenu(hMenu);
					return hMenu;
			}

		int x=0; bool hasSel = (GetListView()->EnumSelected(&x) != NULL);
		*_wantDefaultItems = !(GetListView()->GetHitItem(_x, _y, &x) && x >= 0);

		if (*_wantDefaultItems)
		{
			HMENU hPlaylistSubMenu = CreatePopupMenu();
			AddSubMenu(hMenu, hPlaylistSubMenu, __LOCALIZE("Playlists","sws_DLG_165"));
			PlaylistMenu(hPlaylistSubMenu);
		}

		if (GetPlaylist())
		{
			if (GetMenuItemCount(hMenu))
				AddToMenu(hMenu, SWS_SEPARATOR, 0);

			AddToMenu(hMenu, __LOCALIZE("Add all regions","sws_DLG_165"), ADD_ALL_REGIONS_MSG);
			HMENU hAddSubMenu = CreatePopupMenu();
			AddSubMenu(hMenu, hAddSubMenu, __LOCALIZE("Add region","sws_DLG_165"));
			FillMarkerRegionMenu(NULL, hAddSubMenu, ADD_REGION_START_MSG, SNM_REGION_MASK);

			if (!*_wantDefaultItems) 
			{
				HMENU hInsertSubMenu = CreatePopupMenu();
				AddSubMenu(hMenu, hInsertSubMenu, __LOCALIZE("Insert region","sws_DLG_165"), -1, hasSel ? 0 : MF_GRAYED);
				FillMarkerRegionMenu(NULL, hInsertSubMenu, INSERT_REGION_START_MSG, SNM_REGION_MASK);

				AddToMenu(hMenu, __LOCALIZE("Remove selected regions","sws_DLG_165"), DELETE_MSG, -1, false, hasSel ? 0 : MF_GRAYED);

				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				AddToMenu(hMenu, __LOCALIZE("Toggle infinite loop for selected regions","sws_DLG_165"), TGL_INFINITE_LOOP_MSG, -1, false, hasSel ? 0 : MF_GRAYED);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				AddToMenu(hMenu, __LOCALIZE("Append selected regions to project","sws_DLG_165"), APPEND_SEL_RGN_MSG, -1, false, hasSel ? 0 : MF_GRAYED);
				AddToMenu(hMenu, __LOCALIZE("Paste selected regions at edit cursor","sws_DLG_165"), PASTE_SEL_RGN_MSG, -1, false, hasSel ? 0 : MF_GRAYED);

				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				HMENU hOptionSubMenu = CreatePopupMenu();
				AddSubMenu(hMenu, hOptionSubMenu, __LOCALIZE("Options","sws_DLG_165"));
				OptionsMenu(hOptionSubMenu);
			}
		}
	}

	if (*_wantDefaultItems)
	{
		if (GetMenuItemCount(hMenu))
			AddToMenu(hMenu, SWS_SEPARATOR, 0);
		HMENU hCropPasteSubMenu = CreatePopupMenu();
		AddSubMenu(hMenu, hCropPasteSubMenu, __LOCALIZE("Edit project","sws_DLG_165"));
		EditMenu(hCropPasteSubMenu);

		AddToMenu(hMenu, SWS_SEPARATOR, 0);
		HMENU hOptionSubMenu = CreatePopupMenu();
		AddSubMenu(hMenu, hOptionSubMenu, __LOCALIZE("Options","sws_DLG_165"));
		OptionsMenu(hOptionSubMenu);

		AddToMenu(hMenu, SWS_SEPARATOR, 0);
		AddOscCSurfMenu(hMenu, g_osc, OSC_START_MSG, OSC_END_MSG);
	}
	return hMenu;
}

bool RegionPlaylistWnd::GetToolTipString(int _xpos, int _ypos, char* _bufOut, int _bufOutSz)
{
	if (WDL_VWnd* v = m_parentVwnd.VirtWndFromPoint(_xpos,_ypos,1))
	{
		switch (v->GetID())
		{
			case BTNID_LOCK:
				lstrcpyn(_bufOut, __LOCALIZE("Toggle monitoring/edition mode","sws_DLG_165"), _bufOutSz);
				return true;
			case BTNID_PLAY:
				if (g_playPlaylist>=0) snprintf(_bufOut, _bufOutSz, __LOCALIZE_VERFMT("Playing playlist #%d","sws_DLG_165"), g_playPlaylist+1);
				else lstrcpyn(_bufOut, __LOCALIZE("Play","sws_DLG_165"), _bufOutSz);
				return true;
			case BTNID_STOP:
				lstrcpyn(_bufOut, __LOCALIZE("Stop","sws_DLG_165"), _bufOutSz);
				return true;
			case BTNID_REPEAT:
				lstrcpyn(_bufOut, __LOCALIZE("Repeat playlist","sws_DLG_165"), _bufOutSz);
				return true;
			case CMBID_PLAYLIST:
				if (RegionPlaylist* pl = GetPlaylist())
				{
					double len = pl->GetLength();
					char timeStr[64]="";
					if (len >= 0.0) format_timestr_pos(len, timeStr, sizeof(timeStr), -1);
					else lstrcpyn(timeStr, __LOCALIZE("infinite","sws_DLG_165"), sizeof(timeStr));
					snprintf(_bufOut, _bufOutSz, __LOCALIZE_VERFMT("Edited playlist: #%d \"%s\"\nLength: %s","sws_DLG_165"), g_pls.Get()->m_editId+1, pl->m_name.Get(), timeStr);
					return true;
				}
			case BTNID_NEW_PLAYLIST:
				lstrcpyn(_bufOut, __LOCALIZE("Add playlist","sws_DLG_165"), _bufOutSz);
				return true;
			case BTNID_DEL_PLAYLIST:
				lstrcpyn(_bufOut, __LOCALIZE("Delete playlist","sws_DLG_165"), _bufOutSz);
				return true;
			case BTNID_PASTE:
				lstrcpyn(_bufOut, __LOCALIZE("Crop, paste or append playlist","sws_DLG_165"), _bufOutSz);
				return true;
			case TXTID_MONITOR_PL:
				if (g_playPlaylist>=0) snprintf(_bufOut, _bufOutSz, __LOCALIZE_VERFMT("Playing playlist: #%d \"%s\"","sws_DLG_165"), g_playPlaylist+1, GetPlaylist(g_playPlaylist)->m_name.Get());
				return (g_playPlaylist>=0);
		}
	}
	return false;
}

void RegionPlaylistWnd::ToggleLock()
{
	g_monitorMode = !g_monitorMode;
	RefreshToolbar(SWSGetCommandID(ToggleRegionPlaylistLock));
	Update();
}


///////////////////////////////////////////////////////////////////////////////

int IsInPlaylists(double _pos)
{
	for (int i=0; i < g_pls.Get()->GetSize(); i++)
		if (RegionPlaylist* pl = g_pls.Get()->Get(i))
			if (pl->IsInPlaylist(_pos, false, 0) >= 0)
				return i;
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
// Polling on play: PlaylistRun() and related funcs
///////////////////////////////////////////////////////////////////////////////

namespace {

	int GetShuffledItem(RegionPlaylist* playlist) {
		// Returns -1 if no item is found.
		int numItems = playlist->GetSize();
		int attempts = 0;
		int timestamp = static_cast<int>(time_precise() * 1000);
		while (attempts < 10 && numItems > 1) {
			WDL_UINT64 randomIndex = XS64Rand(timestamp).rand64();
			int candidateItem = static_cast<int>(randomIndex % numItems);
			if (playlist->IsValidIem(candidateItem)) {
				return candidateItem;
			}
			attempts++;
		}
		return -1;
	}
}

// never use things like playlist->Get(i+1) but this func!
// _startWith == True can be used to get the very first region as opposed to the region after the currently playing
// region.
int GetNextValidItem(int _plId, int _itemId, bool _startWith, bool _repeat, bool _shuffle)
{
	if (_plId>=0 && _itemId>=0)
	{
		if (RegionPlaylist* pl = GetPlaylist(_plId))
		{
			if (_shuffle)
			{
				int candidateItem = GetShuffledItem(pl);
				if (candidateItem >= 0) {
					return candidateItem;
				}
				// Fall back on default behavior if shuffling fails...
			}
			for (int i=_itemId+(_startWith?0:1); i<pl->GetSize(); i++)
				if (pl->IsValidIem(i))
					return i;
			if (_repeat)
				for (int i=0; i<pl->GetSize() && i<(_itemId+(_startWith?1:0)); i++)
					if (pl->IsValidIem(i))
						return i;
			// not found if we are here..
			if (_repeat && pl->IsValidIem(_itemId))
				return _itemId;
		}
	}
	return -1;
}

// never use things like playlist->Get(i-1) but this func!
int GetPrevValidItem(int _plId, int _itemId, bool _startWith, bool _repeat, bool _shuffle)
{
	if (_plId>=0 && _itemId>=0)
	{
		if (RegionPlaylist* pl = GetPlaylist(_plId))
		{
			if (_shuffle)
			{
				int candidateItem = GetShuffledItem(pl);
				if (candidateItem >= 0) {
					return candidateItem;
				}
				// Fall back on default behavior if shuffling fails...
			}
			for (int i = _itemId - (_startWith ? 0 : 1); i >= 0; i--)
				if (pl->IsValidIem(i))
					return i;
			if (_repeat)
				for (int i = pl->GetSize() - 1; i >= 0 && i > (_itemId - (_startWith ? 1 : 0)); i--)
					if (pl->IsValidIem(i))
						return i;
			// not found if we are here..
			if (_repeat && pl->IsValidIem(_itemId))
				return _itemId;
		}
	}
	return -1;
}

bool SeekItem(int _plId, int _nextItemId, int _curItemId)
{
	if (RegionPlaylist* pl = g_pls.Get()->Get(_plId))
	{
		// trick to stop the playlist in sync: smooth seek to the end of the project (!)
		if (_nextItemId<0)
		{
			// temp override of the "stop play at project end" option
			if (ConfigVar<int> opt = "stopprojlen") {
				g_oldStopprojlenPref = *opt;
				*opt = 1;
			}
			g_playNext = -1;
			g_rgnLoop = 0;
			g_nextRgnPos = SNM_GetProjectLength()+1.0;
			g_nextRgnEnd = g_nextRgnPos+1.0;
			SeekPlay(g_nextRgnPos);
			return true;
		}
		else if (RgnPlaylistItem* next = pl->Get(_nextItemId))
		{
			double a, b;
			if (EnumMarkerRegionById(NULL, next->m_rgnId, NULL, &a, &b, NULL, NULL, NULL)>=0)
			{
				g_playNext = _nextItemId;
				g_playCur = _plId==g_playPlaylist ? g_playCur : _curItemId;
				g_rgnLoop = next->m_cnt<0 ? -1 : next->m_cnt>1 ? next->m_cnt : 0;
				g_nextRgnPos = a;
				g_nextRgnEnd = b;
				if (_curItemId<0) {
					g_curRgnPos = 0.0;
					g_curRgnEnd = -1.0;
				}
				SeekPlay(g_nextRgnPos);
				return true;
			}
		}
	}
	return false;
}

// the meat!
// polls the playing position and smooth seeks if needed
// remember we always lookup one region ahead!
// made as idle as possible, polled via SNM_CSurfRun()
void PlaylistRun()
{
	if (g_playPlaylist>=0)
	{
#if defined(_SNM_RGNPL_DEBUG1) || defined(_SNM_RGNPL_DEBUG2)
		char dbg[256] = "";
#endif
		bool updated = false;
		double pos = GetPlayPosition2Ex(NULL);

		// NF: potentially fix #886
		// it seems that if '+0.01' isn't added to 'pos' below, adjacent regions are no more occasionally skipped
		// https://forum.cockos.com/showpost.php?p=1935561&postcount=6 and my own tests so far seem to confirm also
		// but I'm unsure what potential side effects this might have
		if ((pos/*+0.01*/) >= g_nextRgnPos && pos <= g_nextRgnEnd)	//JFB!! +0.01 because 'pos' can be a bit ahead of time
																// +1 sample block would be better, but no API..
																// note: sync loss detection will deal with this in the worst case
		{
			// a bunch of calls end here when looping!!

			if (!g_plLoop || g_unsync || pos<g_lastRunPos)
			{
				g_plLoop = false;

				bool first = false;
				if (g_playCur != g_playNext 
/*JFB those vars are only altered here
					|| g_curRgnPos != g_nextRgnPos || g_curRgnEnd != g_nextRgnEnd
*/
					)
				{
#ifdef _SNM_RGNPL_DEBUG1
					 OutputDebugString("\n");
					snprintf(dbg, sizeof(dbg), "NEXT DETECTED - pos = %f\n", pos); OutputDebugString(dbg);
					snprintf(dbg, sizeof(dbg), "                g_curRgnPos = %f, g_curRgnEnd = %f\n", g_curRgnPos, g_curRgnEnd); OutputDebugString(dbg);
					snprintf(dbg, sizeof(dbg), "                g_nextRgnPos = %f, g_nextRgnEnd = %f\n", g_nextRgnPos, g_nextRgnEnd); OutputDebugString(dbg);
					snprintf(dbg, sizeof(dbg), "                g_playCur = %d, g_playNext = %d\n", g_playCur, g_playNext); OutputDebugString(dbg);
					snprintf(dbg, sizeof(dbg), "                g_unsync = %d, g_lastRunPos = %f\n", g_unsync, g_lastRunPos); OutputDebugString(dbg);
#endif
					first = updated = true;
					g_playCur = g_playNext;
					g_curRgnPos = g_nextRgnPos;
					g_curRgnEnd = g_nextRgnEnd;
				}

				// region loop?
				if (g_rgnLoop && (first || g_unsync || pos<g_lastRunPos))
				{
					updated = true;
					if (g_rgnLoop>0)
						g_rgnLoop--;
					if (g_rgnLoop)
						SeekPlay(g_nextRgnPos); // then exit
				}

				if (!g_rgnLoop) // if, not else if!
				{
					int nextId = GetNextValidItem(g_playPlaylist, g_playCur, false, g_repeatPlaylist, g_shufflePlaylist);

					// loop corner cases
					// ex: 1 item in the playlist + repeat on, or repeat on + last region == first region,
					//     or playlist = region3, then unknown region (e.g. deleted) and region3 again, or etc..
					if (nextId>=0)
						if (RegionPlaylist* pl = GetPlaylist(g_playPlaylist))
							if (RgnPlaylistItem* next = pl->Get(nextId)) 
								if (RgnPlaylistItem* cur = pl->Get(g_playCur)) 
									g_plLoop = (cur->m_rgnId==next->m_rgnId); // valid regions at this point

#ifdef _SNM_RGNPL_DEBUG1
					snprintf(dbg, sizeof(dbg), "SEEK - Current = %d, Next = %d\n", g_playCur, nextId); OutputDebugString(dbg);
#endif
					if (!SeekItem(g_playPlaylist, nextId, g_playCur))
						SeekItem(g_playPlaylist, -1, g_playCur); // end of playlist..
					updated = true;
				}
			}
			g_unsync = false;
		}
		else if (g_curRgnPos<g_curRgnEnd) // relevant vars?
		{
			// seek play requested, waiting for region switch..
			if ((pos+0.01) >= g_curRgnPos && pos <= g_curRgnEnd) //JFB!! +0.01 because 'pos' can be a bit ahead of time
																 // +1 sample block would be better, but no API..
			{
				// a bunch of calls end here!
				g_unsync = false;
			}
			// playlist no more in sync?
			else if (!g_unsync)
			{
#ifdef _SNM_RGNPL_DEBUG2
				snprintf(dbg, sizeof(dbg), ">>> SYNC LOSS - pos = %f\n", pos); OutputDebugString(dbg);
				snprintf(dbg, sizeof(dbg), "                g_curRgnPos = %f, g_curRgnEnd = %f\n", g_curRgnPos, g_curRgnEnd); OutputDebugString(dbg);
				snprintf(dbg, sizeof(dbg), "                g_nextRgnPos = %f, g_nextRgnEnd = %f\n", g_nextRgnPos, g_nextRgnEnd); OutputDebugString(dbg);
#endif
				updated = g_unsync = true;
				int spareItemId = -1;
				if (RegionPlaylist* pl = g_pls.Get()->Get(g_playPlaylist))
					spareItemId = pl->IsInPlaylist(pos, g_repeatPlaylist, g_playCur>=0?g_playCur:0);
				if (spareItemId<0 || !SeekItem(g_playPlaylist, spareItemId, -1))
				{
#ifdef _SNM_RGNPL_DEBUG2
					snprintf(dbg, sizeof(dbg), ">>> SYNC LOSS, SEEK expected region pos = %f\n", g_nextRgnPos); OutputDebugString(dbg);
#endif
					SeekPlay(g_nextRgnPos);	// try to resync the expected region, best effort
				}
#ifdef _SNM_RGNPL_DEBUG2
				else {
					snprintf(dbg, sizeof(dbg), ">>> SYNC LOSS, SEEK - Current = %d, Next = %d\n", -1, spareItemId); OutputDebugString(dbg);
				}
#endif
			}
		}

		g_lastRunPos = pos;
		if (updated && (g_osc || g_rgnplWndMgr.Get()))
		{
			// one call to GetMonitoringInfo() for both the wnd & osc
			WDL_FastString cur, curNum, next, nextNum;
			GetMonitoringInfo(&curNum, &cur, &nextNum, &next);

			if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get())
				w->Update(1, &curNum, &cur, &nextNum, &next); // 1: fast update flag

			if (g_osc)
			{
				static WDL_FastString sOSC_CURRENT_RGN(OSC_CURRENT_RGN);
				static WDL_FastString sOSC_NEXT_RGN(OSC_NEXT_RGN);

				if (curNum.GetLength() && cur.GetLength()) curNum.Append(" ");
				curNum.Append(&cur);

				if (nextNum.GetLength() && next.GetLength()) nextNum.Append(" ");
				nextNum.Append(&next);

				WDL_PtrList<WDL_FastString> strs;
				strs.Add(&sOSC_CURRENT_RGN);
				strs.Add(&curNum);
				strs.Add(&sOSC_NEXT_RGN);
				strs.Add(&nextNum);
				g_osc->SendStrBundle(&strs);
				strs.Empty(false);
			}
		}
	}
}

// _itemId: callers must not use no hard coded value but GetNextValidItem() or GetPrevValidItem()
void PlaylistPlay(int _plId, int _itemId)
{
	if (!g_pls.Get()->GetSize()) {
		PlaylistStop();
		MessageBox(g_rgnplWndMgr.GetMsgHWND(), __LOCALIZE("No playlist defined!\nUse the tiny button \"+\" to add one.","sws_DLG_165"), __LOCALIZE("S&M - Error","sws_DLG_165"),MB_OK);
		return;
	}

	RegionPlaylist* pl = GetPlaylist(_plId);
	if (!pl) // e.g. when called via actions
	{
		PlaylistStop();
		char msg[128] = "";
		snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("Unknown playlist #%d!","sws_DLG_165"), _plId+1);
		MessageBox(g_rgnplWndMgr.GetMsgHWND(), msg, __LOCALIZE("S&M - Error","sws_DLG_165"),MB_OK);
		return;
	}

	double prjlen = SNM_GetProjectLength();
	if (prjlen>0.1)
	{
		//JFB REAPER bug? workaround, the native pref "stop play at project end" does not work when the project is empty (should not play at all..)
		int num = pl->GetGreaterMarkerRegion(prjlen);
		if (num>0)
		{
			char msg[256] = "";
			snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("The playlist #%d might end unexpectedly!\nIt constains regions that start after the end of project (region %d at least).","sws_DLG_165"), _plId+1, num);
			if (IDCANCEL == MessageBox(g_rgnplWndMgr.GetMsgHWND(), msg, __LOCALIZE("S&M - Warning","sws_DLG_165"), MB_OKCANCEL)) {
				PlaylistStop();
				return;
			}
		}

		num = pl->GetNestedMarkerRegion();
		if (num>0)
		{
			char msg[256] = "";
			snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("The playlist #%d might not work as expected!\nIt contains nested markers/regions (inside region %d at least).","sws_DLG_165"), _plId+1, num);
			if (IDCANCEL == MessageBox(g_rgnplWndMgr.GetMsgHWND(), msg, __LOCALIZE("S&M - Warning","sws_DLG_165"), MB_OKCANCEL)) {
				PlaylistStop();
				return;
			}
		}

		// handle empty project corner case
		if (pl->IsValidIem(_itemId))
		{
			if (g_seekImmediate)
				PlaylistStop();

			// temp override of the "smooth seek" option
			if (ConfigVar<int> opt = "smoothseek") {
				g_oldSeekPref = *opt;
				*opt = 3;
			}
			// temp override of the repeat/loop state option
			if (GetSetRepeat(-1) == 1) {
				g_oldRepeatState = 1;
				GetSetRepeat(0);
			}

			g_plLoop = false;
			g_unsync = false;
			g_lastRunPos = SNM_GetProjectLength()+1.0;
			if (SeekItem(_plId, _itemId, g_playPlaylist==_plId ? g_playCur : -1))
			{
				g_playPlaylist = _plId; // enables PlaylistRun()
				if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get())
					w->Update(); // for the play button, next/previous region actions, etc....
			}
			else
				PlaylistStopped(); // reset vars & native prefs
		}
	}

	if (g_playPlaylist<0)
	{
		char msg[128] = "";
		snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("Playlist #%d: nothing to play!\n(empty playlist, empty project, removed regions, etc..)","sws_DLG_165"), _plId+1);
		MessageBox(g_rgnplWndMgr.GetMsgHWND(), msg, __LOCALIZE("S&M - Error","sws_DLG_165"),MB_OK);
	}
}

// _ct=NULL or _ct->user=-1 to play the edited playlist, use the provided playlist id otherwise
void PlaylistPlay(COMMAND_T* _ct)
{
	int plId = _ct ? (int)_ct->user : -1;
	plId = plId>=0 ? plId : g_pls.Get()->m_editId;
	PlaylistPlay(plId, GetNextValidItem(plId, 0, true, g_repeatPlaylist, g_shufflePlaylist));
}

// always cycle (whatever is g_repeatPlaylist)
void PlaylistSeekPrevNext(COMMAND_T* _ct)
{
	if (g_playPlaylist < 0)
	{
		PlaylistPlay(NULL);
	} else if (g_shufflePlaylist)
	{
		if ((int) _ct->user > 0)
		{
			PlaylistPlay(g_playPlaylist, g_playNext);
		} else
		{
			PlaylistPlay(g_playPlaylist, g_playCur);
		}
	} else
	{
		int itemId;
		if ((int) _ct->user > 0)
			itemId = GetNextValidItem(g_playPlaylist, g_playNext, false, true, g_shufflePlaylist);
		else
		{
			itemId = GetPrevValidItem(g_playPlaylist, g_playNext, false, true, g_shufflePlaylist);
			if (itemId == g_playCur)
				itemId = GetPrevValidItem(g_playPlaylist, g_playCur, false, true, g_shufflePlaylist);
		}
		PlaylistPlay(g_playPlaylist, itemId);
	}
}

// Seek prev/next region based on current playing region
void PlaylistSeekPrevNextCurBased(COMMAND_T* _ct)
{
	if (g_playPlaylist < 0)
	{
		PlaylistPlay(NULL);
	} else if (g_shufflePlaylist)
	{
		PlaylistPlay(g_playPlaylist, g_playNext);
	} else
	{
		int itemId;
		if ((int) _ct->user > 0)
			itemId = GetNextValidItem(g_playPlaylist, g_playCur, false, true, g_shufflePlaylist);
		else
			itemId = GetPrevValidItem(g_playPlaylist, g_playCur, false, true, g_shufflePlaylist);
		PlaylistPlay(g_playPlaylist, itemId);
	}
}

void PlaylistStop()
{
	if (g_playPlaylist>=0 || (GetPlayStateEx(NULL)&1) == 1)
	{
		OnStopButton();
/* commented: already done via SNM_CSurfSetPlayState() callback
		PlaylistStopped();
*/
	}
}

void PlaylistStopped(bool _pause)
{
	if (g_playPlaylist>=0 && !_pause)
	{
		g_playPlaylist = -1;

		// restore options
		if (g_oldSeekPref >= 0)
			if (ConfigVar<int> opt = "smoothseek") {
				*opt = g_oldSeekPref;
				g_oldSeekPref = -1;
			}
		if (g_oldStopprojlenPref >= 0)
			if (ConfigVar<int> opt = "stopprojlen") {
				*opt = g_oldStopprojlenPref;
				g_oldStopprojlenPref = -1;
			}
		if (g_oldRepeatState >=0) {
			GetSetRepeat(g_oldRepeatState);
			g_oldRepeatState = -1;
		}

		if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get())
			w->Update();
	}
}

void PlaylistUnpaused() {
	PlaylistResync();
}

// used when editing the playlist/regions while playing (required because we always look one region ahead)
void PlaylistResync()
{
	if (RegionPlaylist* pl = GetPlaylist(g_playPlaylist))
		if (RgnPlaylistItem* item = pl->Get(g_playCur))
			SeekItem(g_playPlaylist, GetNextValidItem(g_playPlaylist, g_playCur, item->m_cnt<0 || item->m_cnt>1, g_repeatPlaylist, g_shufflePlaylist), g_playCur);
}

void SetPlaylistRepeat(COMMAND_T* _ct)
{
	int mode = _ct ? (int)_ct->user : -1; // toggle if no COMMAND_T is specified
	switch(mode) {
		case -1: g_repeatPlaylist=!g_repeatPlaylist; break;
		case 0: g_repeatPlaylist=false; break;
		case 1: g_repeatPlaylist=true; break;
	}
	RefreshToolbar(SWSGetCommandID(SetPlaylistRepeat, -1));
	PlaylistResync();
	if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get())
		w->Update();
}

int IsPlaylistRepeat(COMMAND_T*) {
	return g_repeatPlaylist;
}

void SetPlaylistOptionShuffle(COMMAND_T* _ct)
{
	int mode = _ct ? (int)_ct->user : -1; // toggle if no COMMAND_T is specified
	switch(mode) {
		case -1: g_shufflePlaylist=!g_shufflePlaylist; break;
		case 0: g_shufflePlaylist=false; break;
		case 1: g_shufflePlaylist=true; break;
	}
	RefreshToolbar(SWSGetCommandID(SetPlaylistRepeat, -1));
	PlaylistResync();
	if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get())
		w->Update();
}

int IsPlaylistOptionShuffle(COMMAND_T*) {
	return g_shufflePlaylist;
}

// VT: get/set g_seekImmediate
void SetPlaylistOptionSmoothSeek(COMMAND_T* _ct)
{
	int mode = _ct ? (int)_ct->user : -1; // toggle if no COMMAND_T is specified
	switch(mode) {
		case -1: g_seekImmediate=!g_seekImmediate; break;
		case 0: g_seekImmediate=false; break;
		case 1: g_seekImmediate=true; break;
	}
	if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get())
		w->Update();
}

int IsPlaylistOptionSmoothSeek(COMMAND_T*) {
	return g_seekImmediate;
}


///////////////////////////////////////////////////////////////////////////////
// Editing features
///////////////////////////////////////////////////////////////////////////////

//JFB nothing to see here.. please move on :)
// (doing things that are not really possible with the API (v4.3) => macro-ish, etc..)
// note: moves/copies env points too, makes polled items, etc.. according to user prefs
//JFB TODO? crop => markers removed
void AppendPasteCropPlaylist(RegionPlaylist* _playlist, const AppendPasteCropPlaylist_Mode _mode)
{
	if (!_playlist || !_playlist->GetSize())
		return;

	int rgnNum = _playlist->IsInfinite();
	if (rgnNum >= 0)
	{
		char msg[256] = "";
		snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("Paste/crop aborted: infinite loop (for region %d at least)!","sws_DLG_165"), rgnNum);
		MessageBox(g_rgnplWndMgr.GetMsgHWND(), msg, __LOCALIZE("S&M - Error","sws_DLG_165"),MB_OK);
		return;
	}

	bool updated = false;
	double prjlen=SNM_GetProjectLength(), startPos=prjlen, endPos=prjlen;

	// insert empty space?
	if (_mode == PASTE_CURSOR)
	{
		startPos = endPos = GetCursorPositionEx(NULL);
		if (startPos < prjlen)
		{
			// not _playlist->IsInPlaylist()!
			if (GetPlaylist()->IsInPlaylist(startPos, false, 0) >= 0)
			{
				MessageBox(g_rgnplWndMgr.GetMsgHWND(), 
					__LOCALIZE("Aborted: pasting inside a region which is used in the playlist!","sws_DLG_165"),
					__LOCALIZE("S&M - Error","sws_DLG_165"), MB_OK);
				return;
			}
			if (int usedId = IsInPlaylists(startPos) >= 0)
			{
				char msg[256] = "";
				snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("Warning: pasting inside a region that belongs to playlist #%d!\nAre you sure you want to continue?","sws_DLG_165"), usedId+1);
				if (IDNO == MessageBox(g_rgnplWndMgr.GetMsgHWND(), msg, __LOCALIZE("S&M - Warning","sws_DLG_165"), MB_YESNO))
					return;
			}

			updated = true;
			Undo_BeginBlock2(NULL);
			PreventUIRefresh(1);
			InsertSilence(NULL, startPos, _playlist->GetLength());
		}
	}

//	OnStopButton();

	ConfigVarOverride<int> options[] {
		{"env_reduce",     2}, // add edge points
		{"envattach",      1}, // move with items
		{"projgroupover",  1}, // disable item grouping
		{"splitautoxfade", 0}, // disable overlapping items when splitting
	};
	(void)options;

	WDL_PtrList_DeleteOnDestroy<MarkerRegion> rgns;
	for (int i=0; i < _playlist->GetSize(); i++)
	{
		if (RgnPlaylistItem* plItem = _playlist->Get(i))
		{
			int rgnnum, rgncol=0; double rgnpos, rgnend; const char* rgnname;
			if (EnumMarkerRegionById(NULL, plItem->m_rgnId, NULL, &rgnpos, &rgnend, &rgnname, &rgnnum, &rgncol)>=0)
			{
				if (!updated)
				{
					updated = true;
					Undo_BeginBlock2(NULL);
					PreventUIRefresh(1);
				}

				// trick: generate dummy items to move envelope points, etc -- even with empty/folder tracks
				WDL_PtrList<void> itemsToRemove;
				GenerateItemsInInterval(&itemsToRemove, rgnpos+SNM_FUDGE_FACTOR, rgnend-SNM_FUDGE_FACTOR, "<S&M Region Playlist - TEMP>");

				WDL_PtrList<void> itemsToKeep;
				GetItemsInInterval(&itemsToKeep, rgnpos, rgnend, false);
				// store regions
				bool found = false;
				for (int k = 0; !found && k<rgns.GetSize(); k++)
					found |= (rgns.Get(k)->GetNum() == rgnnum);
				if (!found)
					rgns.Add(new MarkerRegion(true, endPos-startPos, endPos+rgnend-rgnpos-startPos, rgnname, rgnnum, rgncol));

				// store data needed to "unsplit"
				// note: not needed when croping as those items will get removed
				WDL_PtrList_DeleteOnDestroy<SNM_ItemChunk> itemSates;
				if (_mode == PASTE_PROJECT || _mode == PASTE_CURSOR)
				{
					for (int j = 0; j < itemsToKeep.GetSize(); j++)
						itemSates.Add(new SNM_ItemChunk((MediaItem*)itemsToKeep.Get(j)));
				}

				WDL_PtrList<void> splitItems;
				SplitSelectItemsInInterval(NULL, rgnpos, rgnend, false, _mode == PASTE_PROJECT || _mode == PASTE_CURSOR ? &splitItems : NULL);

				// REAPER "bug": the last param of ApplyNudge() is ignored although
				// it is used in duplicate mode => use a loop instead
				for (int k = 0; k < plItem->m_cnt; k++)
				{
					DupSelItems(NULL, endPos-rgnpos, &itemsToKeep); // overrides the native ApplyNudge()
					endPos += (rgnend-rgnpos);
				}

				// "unsplit" items
				for (int j = 0; j < itemSates.GetSize(); j++)
				{
					if (SNM_ItemChunk* ic = itemSates.Get(j)) {
						SNM_ChunkParserPatcher p(ic->m_item);
						p.SetChunk(ic->m_chunk.Get());
					}
				}

				DeleteMediaItemsByName("<S&M Region Playlist - TEMP>");

				for (int j=0; j < splitItems.GetSize(); j++)
				{
					if (MediaItem* item = (MediaItem*)splitItems.Get(j))
						if (itemsToKeep.Find(item) < 0)
							DeleteTrackMediaItem(GetMediaItem_Track(item), item); // might have been deleted above already (no-op)
				}
			}
		}
	}

	// nothing done..
	if (!updated)
	{
		PreventUIRefresh(-1);
		return;
	}

	///////////////////////////////////////////////////////////////////////////
	// append/paste to current project
	if (_mode == PASTE_PROJECT || _mode == PASTE_CURSOR)
	{
//		Main_OnCommand(40289, 0); // unselect all items
		SetEditCurPos2(NULL, endPos, true, false);

		PreventUIRefresh(-1);

		Undo_EndBlock2(NULL, _mode==2 ? __LOCALIZE("Append playlist to project","sws_undo") : __LOCALIZE("Paste playlist at edit cursor","sws_undo"), UNDO_STATE_ALL);
		return;
	}

	///////////////////////////////////////////////////////////////////////////
	// crop current project

	// dup the playlist (needed when cropping to new project tab)
	RegionPlaylist* dupPlaylist = _mode==1 ? new RegionPlaylist(_playlist) : NULL;

	// crop current project
	GetSet_LoopTimeRange(true, false, &startPos, &endPos, false);
	Main_OnCommand(40049, 0); // crop project to time sel
//	Main_OnCommand(40289, 0); // unselect all items

	// restore (updated) regions
	for (int i=0; i<rgns.GetSize(); i++)
		rgns.Get(i)->AddToProject();

	// cleanup playlists (some other regions may have been removed)
	for (int i=g_pls.Get()->GetSize()-1; i>=0; i--)
		if (RegionPlaylist* pl = g_pls.Get()->Get(i))
			if (pl != _playlist)
				for (int j=0; j<pl->GetSize(); j++)
					if (pl->Get(j) && GetMarkerRegionIndexFromId(NULL, pl->Get(j)->m_rgnId) < 0) {
						g_pls.Get()->Delete(i, true);
						break;
					}
	g_pls.Get()->m_editId = g_pls.Get()->Find(_playlist);
	if (g_pls.Get()->m_editId < 0)
		g_pls.Get()->m_editId = 0; // just in case..

	if (_mode == CROP_PROJECT)
	{
		// clear time sel + edit cursor position
		GetSet_LoopTimeRange(true, false, &g_d0, &g_d0, false);
		SetEditCurPos2(NULL, 0.0, true, false);

		PreventUIRefresh(-1);

		Undo_EndBlock2(NULL, __LOCALIZE("Crop project to playlist","sws_undo"), UNDO_STATE_ALL);

		if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get()) {
			w->FillPlaylistCombo();
			w->Update();
		}
		return;
	}

	///////////////////////////////////////////////////////////////////////////
	// crop to new project tab
	// macro-ish solution because copying the project the proper way (e.g. 
	// copying all track states) is totally unstable
	// note: so the project is "copied" w/o extension data, project bay, etc..

	// store master track
	WDL_FastString mstrStates;
	if (MediaTrack* tr = CSurf_TrackFromID(0, false)) {
		SNM_ChunkParserPatcher p(tr);
		mstrStates.Set(p.GetChunk());
	}

	Main_OnCommand(40296, 0); // select all tracks
	Main_OnCommand(40210, 0); // copy tracks

	PreventUIRefresh(-1);

	// trick!
	Undo_EndBlock2(NULL, __LOCALIZE("Crop project to playlist","sws_undo"), UNDO_STATE_ALL);
	if (!Undo_DoUndo2(NULL))
		return;
/*JFB no-op, unfortunately
	CSurf_FlushUndo(true);
*/

	///////////////////////////////////////////////////////////////////////////
	// New project tab (ignore default template)
	Main_OnCommand(41929, 0);

	Undo_BeginBlock2(NULL);

	PreventUIRefresh(1);

	Main_OnCommand(40058, 0); // paste item/tracks
	Main_OnCommand(40297, 0); // unselect all tracks
	if (OpenClipboard(GetMainHwnd())) { // clear clipboard
	    EmptyClipboard();
		CloseClipboard();
	}
	if (MediaTrack* tr = CSurf_TrackFromID(0, false)) {
		SNM_ChunkParserPatcher p(tr);
		p.SetChunk(mstrStates.Get());
	}
	// restore regions (in the new project this time)
	for (int i=0; i<rgns.GetSize(); i++)
		rgns.Get(i)->AddToProject();

	// new project: the playlist is empty at this point
	g_pls.Get()->Add(dupPlaylist);
	g_pls.Get()->m_editId = 0;

	PreventUIRefresh(-1);
	SNM_UIRefresh(NULL);

	Undo_EndBlock2(NULL, __LOCALIZE("Crop project to playlist","sws_undo"), UNDO_STATE_ALL);

	if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get()) {
		w->FillPlaylistCombo();
		w->Update();
	}
}

void AppendPasteCropPlaylist(COMMAND_T* _ct) {
	AppendPasteCropPlaylist(GetPlaylist(), static_cast<AppendPasteCropPlaylist_Mode>(_ct->user));
}


///////////////////////////////////////////////////////////////////////////////

void PlaylistUpdateJob::Perform() {
	if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get()) {
		w->FillPlaylistCombo();
		w->Update();
	}
}


///////////////////////////////////////////////////////////////////////////////

// ScheduledJob used because of multi-notifs
void PlaylistMarkerRegionListener::NotifyMarkerRegionUpdate(int _updateFlags) {
	PlaylistResync();
	ScheduledJob::Schedule(new PlaylistUpdateJob(SNM_SCHEDJOB_ASYNC_DELAY_OPT));
}


///////////////////////////////////////////////////////////////////////////////
// project_config_extension_t
///////////////////////////////////////////////////////////////////////////////

static bool ProcessExtensionLine(const char *line, ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	LineParser lp(false);
	if (lp.parse(line) || lp.getnumtokens() < 1)
		return false;

	if (!strcmp(lp.gettoken_str(0), "<S&M_RGN_PLAYLIST"))
	{
		if (RegionPlaylist* playlist = g_pls.Get()->Add(new RegionPlaylist(lp.gettoken_str(1))))
		{
			int success;
			if (lp.gettoken_int(2, &success) && success)
				g_pls.Get()->m_editId = g_pls.Get()->GetSize()-1;

			char linebuf[SNM_MAX_CHUNK_LINE_LENGTH]="";
			while(true)
			{
				if (!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
				{
					if (lp.getnumtokens() && lp.gettoken_str(0)[0] == '>')
						break;
					else if (lp.getnumtokens() == 2)
						playlist->Add(new RgnPlaylistItem(lp.gettoken_int(0), lp.gettoken_int(1)));
				}
				else
					break;
			}
			if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get()) {
				w->FillPlaylistCombo();
				w->Update();
			}
			return true;
		}
	}
	return false;
}

static void SaveExtensionConfig(ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	for (int j=0; j < g_pls.Get()->GetSize(); j++)
	{
		WDL_FastString confStr("<S&M_RGN_PLAYLIST "), escStr;
		makeEscapedConfigString(GetPlaylist(j)->m_name.Get(), &escStr);
		confStr.Append(escStr.Get());
		if (j == g_pls.Get()->m_editId)
			confStr.Append(" 1\n");
		else
			confStr.Append("\n");
		int iHeaderLen = confStr.GetLength();

		for (int i=0; i < GetPlaylist(j)->GetSize(); i++)
			if (RgnPlaylistItem* item = GetPlaylist(j)->Get(i))
				confStr.AppendFormatted(128,"%d %d\n", item->m_rgnId, item->m_cnt);

		// ignore empty playlists when saving but always take them into account for undo
		if (isUndo || confStr.GetLength() > iHeaderLen) {
			confStr.Append(">\n");
			StringToExtensionConfig(&confStr, ctx);
		}
	}
}

static void BeginLoadProjectState(bool isUndo, struct project_config_extension_t *reg)
{
	g_pls.Cleanup();
	g_pls.Get()->Empty(true);
	g_pls.Get()->m_editId=0;
}

static project_config_extension_t s_projectconfig = {
	ProcessExtensionLine, SaveExtensionConfig, BeginLoadProjectState, NULL
};


///////////////////////////////////////////////////////////////////////////////

// ScheduledJob used because of multi-notifs
void RegionPlaylistSetTrackListChange() {
	ScheduledJob::Schedule(new PlaylistUpdateJob(SNM_SCHEDJOB_ASYNC_DELAY_OPT));
}


///////////////////////////////////////////////////////////////////////////////

int RegionPlaylistInit()
{
	// load prefs
	char buf[64]="";
	g_monitorMode = GetPrivateProfileInt("RegionPlaylist", "MonitorMode", 0, g_SNM_IniFn.Get());
	g_repeatPlaylist = GetPrivateProfileInt("RegionPlaylist", "Repeat", 0, g_SNM_IniFn.Get());
	g_seekImmediate = GetPrivateProfileInt("RegionPlaylist", "SeekImmediate", 0, g_SNM_IniFn.Get());
	g_shufflePlaylist = GetPrivateProfileInt("RegionPlaylist", "ShufflePlaylist", 0, g_SNM_IniFn.Get());
	g_optionFlags = GetPrivateProfileInt("RegionPlaylist", "SeekPlay", 0, g_SNM_IniFn.Get());
	GetPrivateProfileString("RegionPlaylist", "BigFontName", SNM_DYN_FONT_NAME, g_rgnplBigFontName, sizeof(g_rgnplBigFontName), g_SNM_IniFn.Get());
	GetPrivateProfileString("RegionPlaylist", "OscFeedback", "", buf, sizeof(buf), g_SNM_IniFn.Get());
	g_osc = LoadOscCSurfs(NULL, buf); // NULL on err (e.g. "", token doesn't exist, etc.)

	// instanciate the window if needed, can be NULL
	g_rgnplWndMgr.Init();

	if (!plugin_register("projectconfig", &s_projectconfig))
		return 0;

	return 1;
}

void RegionPlaylistExit()
{
	plugin_register("-projectconfig", &s_projectconfig);

	char format[SNM_MAX_PATH];

	// save prefs
	const std::pair<const char *, int> intOptions[] = {
		{ "MonitorMode",     g_monitorMode     },
		{ "Repeat",          g_repeatPlaylist  },
		{ "SeekImmediate",   g_seekImmediate   },
		{ "ShufflePlaylist", g_shufflePlaylist },
		{ "SeekPlay",        g_optionFlags     },
	};
	for(const auto &pair : intOptions) {
		snprintf(format, sizeof(format), "%d", pair.second);
		WritePrivateProfileString("RegionPlaylist", pair.first, format, g_SNM_IniFn.Get());
	}

	WritePrivateProfileString("RegionPlaylist", "ScrollView", NULL, g_SNM_IniFn.Get()); // deprecated
	WritePrivateProfileString("RegionPlaylist", "BigFontName", g_rgnplBigFontName, g_SNM_IniFn.Get());
	if (g_osc)
	{
		snprintf(format, sizeof(format), R"("%s")", g_osc->m_name.Get());
		WritePrivateProfileString("RegionPlaylist", "OscFeedback", format, g_SNM_IniFn.Get());
	}
	else
		WritePrivateProfileString("RegionPlaylist", "OscFeedback", NULL, g_SNM_IniFn.Get());

	DELETE_NULL(g_osc);
	g_rgnplWndMgr.Delete();
}

void OpenRegionPlaylist(COMMAND_T*)
{
	if (RegionPlaylistWnd* w = g_rgnplWndMgr.Create())
		w->Show(true, true);
}

int IsRegionPlaylistDisplayed(COMMAND_T*){
	if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get())
		return w->IsWndVisible();
	return 0;
}

void ToggleRegionPlaylistLock(COMMAND_T*) {
	if (RegionPlaylistWnd* w = g_rgnplWndMgr.Get())
		w->ToggleLock();
}

int IsRegionPlaylistMonitoring(COMMAND_T*) {
	return g_monitorMode;
}
