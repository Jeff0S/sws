/******************************************************************************
/ SnM_RegionPlaylist.h
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

//#pragma once

#ifndef _SNM_REGIONPLAYLIST_H_
#define _SNM_REGIONPLAYLIST_H_

#include "SnM_Marker.h"
#include "SnM_VWnd.h"


class PlaylistMarkerRegionListener : public SNM_MarkerRegionListener {
public:
	PlaylistMarkerRegionListener() : SNM_MarkerRegionListener() {}
	void NotifyMarkerRegionUpdate(int _updateFlags);
};

// no other attributes (like a comment) because of the "auto-compacting" feature..
class RgnPlaylistItem {
public:
	RgnPlaylistItem(int _rgnId=-1, int _cnt=1) : m_rgnId(_rgnId),m_cnt(_cnt) {}
	bool IsValidIem() { return (m_rgnId>0 && m_cnt!=0 && GetMarkerRegionIndexFromId(NULL, m_rgnId)>=0); }
	double GetPos() { if (m_rgnId>0) { double pos; if (EnumMarkerRegionById(NULL, m_rgnId, NULL, &pos, NULL, NULL, NULL, NULL)>=0) return pos; } return 0.0; }
	int m_rgnId, m_cnt;
};

class RegionPlaylist : public WDL_PtrList<RgnPlaylistItem> {
public:
	RegionPlaylist(RegionPlaylist* _pl = NULL, const char* _name = NULL);
	RegionPlaylist(const char* _name) : m_name(_name), WDL_PtrList<RgnPlaylistItem>() {}
	~RegionPlaylist() {}
	bool IsValidIem(int _i);
	int IsInPlaylist(double _pos, bool _repeat, int _startWith);
	int IsInfinite();
	double GetLength();
	int GetNestedMarkerRegion();
	int GetGreaterMarkerRegion(double _pos);
	WDL_FastString m_name;
};

class RegionPlaylists : public WDL_PtrList<RegionPlaylist>
{
public:
	RegionPlaylists() : m_editId(0), WDL_PtrList<RegionPlaylist>() {}
	~RegionPlaylists() {}
	int m_editId; // edited playlist id
};

class RegionPlaylistView : public SWS_ListView {
public:
	RegionPlaylistView(HWND hwndList, HWND hwndEdit);
	void UpdateCompact();
	void OnDrag();
	void OnEndDrag();
protected:
	void GetItemText(SWS_ListItem* item, int iCol, char* str, int iStrMax);
	void GetItemList(SWS_ListItemList* pList);
	void SetItemText(SWS_ListItem* item, int iCol, const char* str);
	void OnItemClk(SWS_ListItem* item, int iCol, int iKeyState);
	void OnItemDblClk(SWS_ListItem* item, int iCol);
	int OnItemSort(SWS_ListItem* _item1, SWS_ListItem* _item2);
	void OnBeginDrag(SWS_ListItem* item);
	WDL_PtrList<RgnPlaylistItem> m_draggedItems;
};

class RegionPlaylistWnd : public SWS_DockWnd
{
public:
	RegionPlaylistWnd();
	virtual ~RegionPlaylistWnd();
	void GetMinSize(int* _w, int* _h) { *_w=202; *_h=100; }
	void OnCommand(WPARAM wParam, LPARAM lParam);
	void Update(int _flags = 0, WDL_FastString* _curNum=NULL, WDL_FastString* _cur=NULL, WDL_FastString* _nextNum=NULL, WDL_FastString* _next=NULL);
	void UpdateMonitoring(WDL_FastString* _curNum=NULL, WDL_FastString* _cur=NULL, WDL_FastString* _nextNum=NULL, WDL_FastString* _next=NULL);
	void FillPlaylistCombo();
	void ToggleLock();
protected:
	void OnInitDlg();
	void OnDestroy();
	void PlaylistMenu(HMENU _menu);
	void EditMenu(HMENU _menu);
	void OptionsMenu(HMENU _menu);
	HMENU OnContextMenu(int x, int y, bool* wantDefaultItems);
	int OnKey(MSG* msg, int iKeyState) ;
	void DrawControls(LICE_IBitmap* _bm, const RECT* _r, int* _tooltipHeight = NULL);
	bool GetToolTipString(int _xpos, int _ypos, char* _bufOut, int _bufOutSz);

	PlaylistMarkerRegionListener m_mkrRgnListener;

	WDL_VirtualStaticText m_txtPlaylist;
	WDL_VirtualComboBox m_cbPlaylist;
	SNM_TwoTinyButtons m_btnsAddDel;
	SNM_TinyPlusButton m_btnAdd;
	SNM_TinyMinusButton m_btnDel;
	SNM_ToolbarButton m_btnCrop;
	WDL_VirtualIconButton m_btnLock, m_btnPlay, m_btnStop, m_btnRepeat;
	SNM_DynSizedText m_monPl;
	SNM_FiveMonitors m_mons;
	SNM_DynSizedText m_txtMon[5];
};

class PlaylistUpdateJob : public ScheduledJob {
public:
	PlaylistUpdateJob(int _approxMs) : ScheduledJob(SNM_SCHEDJOB_PLAYLIST_UPDATE, _approxMs) {}
protected:
	void Perform();
};

int GetNextValidItem(int _playlistId, int _itemId, bool _startWith, bool _repeat, bool _shuffle);
int GetPrevValidItem(int _playlistId, int _itemId, bool _startWith, bool _repeat, bool _shuffle);
bool SeekItem(int _plId, int _nextItemId, int _curItemId);
void PlaylistRun();
void PlaylistPlay(int _playlistId, int _itemId);
void PlaylistPlay(COMMAND_T*);
void PlaylistSeekPrevNext(COMMAND_T*);
void PlaylistSeekPrevNextCurBased(COMMAND_T*);
void PlaylistStop();
void PlaylistStopped(bool _pause = false);
void PlaylistUnpaused();
void PlaylistResync();
void SetPlaylistRepeat(COMMAND_T*);
int IsPlaylistRepeat(COMMAND_T*);

void SetPlaylistOptionShuffle(COMMAND_T* _ct);
int IsPlaylistOptionShuffle(COMMAND_T* _ct);
void SetPlaylistOptionSmoothSeek(COMMAND_T*);
int IsPlaylistOptionSmoothSeek(COMMAND_T*);


enum AppendPasteCropPlaylist_Mode {
  CROP_PROJECT,     // crop current project
  CROP_PROJECT_TAB, // crop to new project tab
  PASTE_PROJECT,    // append to current project
  PASTE_CURSOR,     // paste at cursor position
};
void AppendPasteCropPlaylist(RegionPlaylist* _playlist, const AppendPasteCropPlaylist_Mode _mode);
void AppendPasteCropPlaylist(COMMAND_T*);

void RegionPlaylistSetTrackListChange();

int RegionPlaylistInit();
void RegionPlaylistExit();
void OpenRegionPlaylist(COMMAND_T*);
int IsRegionPlaylistDisplayed(COMMAND_T*);
void ToggleRegionPlaylistLock(COMMAND_T*);
int IsRegionPlaylistMonitoring(COMMAND_T*);
void AddAllRegionsToPlaylist(COMMAND_T* = nullptr);

#endif
