/******************************************************************************
/ ProjectSetList.cpp
/
/ Copyright (c) 2020 Ruben Sandoval (SWS)
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
#include "SnM/SnM_Util.h" 
#include "SnM/SnM_Dlg.h"
#include "../reaper/localize.h"
#include "ProjectSetListMgr.h"
#include "ProjectSetList.h"

#define SETLIST_WND_ID			"SetList"

// Globals

// List of songs in the set list.
static SWS_ProjectSetListWnd* g_pProjList = nullptr;

// This is the SongItem that is currently selected in the Set List.
static SWS_SongItem* pCurrSongItem = nullptr;

// SongItem that was loaded into Reaper by hitting the RETURN key or double click.
static SWS_SongItem* pSongLoadedFromSetList = nullptr;

// Is Reaper playing the song?
static bool isReaperPlayingTheSong = false;

// When isSetListEnabled = true, the next song will be loaded into reaper.
static bool isSetListEnabled = false;

// If isSetListEnabled == true && autoPlayNextSong == true, the next song will auto-start playing without user intervention.
static bool autoPlayNextSong = false;

// Flag will be set to true if user hit "N" - next song or loaded the song using the RETURN key or double click.
// If isSetListEnabled == true && autoPlayNextSong == true && songManuallyLoaded == true, the user will have to manually hit <space> to start the song.
static bool songManuallyLoaded = false;

// By default this will point to the next song in the list but user can hit enter on any song to alter the play order.
static SWS_SongItem* nextSongToPlay = nullptr;

// Drag variables.
static bool dragInProgress;
static int s_ignore_update;

enum { COL_ID = 0, COL_TYPE, COL_FILTER, COL_COLOR, COL_ICON, COL_TCP_LAYOUT, COL_MCP_LAYOUT, COL_COUNT };

// !WANT_LOCALIZE_STRINGS_BEGIN:sws_DLG_146
static SWS_LVColumn g_cols[] = { {25, 0, "#" }, { 400, 0, "Name" }, };
// !WANT_LOCALIZE_STRINGS_END

SWS_ProjectSetListView::SWS_ProjectSetListView(HWND hwndList, HWND hwndEdit)
	:SWS_ListView(hwndList, hwndEdit, 2, g_cols, "ProjSetListViewState", false, "sws_DLG_146", false)
{
}

void SWS_ProjectSetListView::GetItemText(SWS_ListItem* item, int iCol, char* str, int iStrMax)
{
	SWS_SongItem* songItem = (SWS_SongItem*)item;
	if (!songItem)
		return;

	bool itemLoadedFromSetList = false;
	bool nextSongToPlayFromSetList = false;

	if (pSongLoadedFromSetList) {
		if (pSongLoadedFromSetList == songItem)
			itemLoadedFromSetList = true;
	}
	if (nextSongToPlay) {
		if (nextSongToPlay == songItem)
			nextSongToPlayFromSetList = true;
	}

	switch (iCol)
	{
		case 0: // Slot
		{
			if (dragInProgress) {
				snprintf(str, iStrMax, "%s %d", itemLoadedFromSetList ? UTF8_BULLET : nextSongToPlayFromSetList ? isSetListEnabled ? UTF8_CIRCLE : " " : " ", g_SongItems.Find(songItem) + 1);
			}
			else {
				snprintf(str, iStrMax, "%s %d", itemLoadedFromSetList ? UTF8_BULLET : nextSongToPlayFromSetList ? isSetListEnabled ? UTF8_CIRCLE : " " : " ", songItem->m_iSlot);
			}
			break;
		}
		case 1: // Name
		{
			char* fileName = songItem->m_song_name.Get();
			lstrcpyn(str, fileName, iStrMax);
			break;
		}
	}
}

void SWS_ProjectSetListView::GetItemList(SWS_ListItemList* pList)
{
	if (g_SongItems.GetSize() == 0)
		return;

	for (int i = 0; i < g_SongItems.GetSize(); i++)
		pList->Add((SWS_ListItem*)g_SongItems.Get(i));
}

void SWS_ProjectSetListView::OnItemDblClk(SWS_ListItem* item, int iCol)
{
	SWS_SongItem* songItem = (SWS_SongItem*)item;
	songManuallyLoaded = true;
	LoadSong(songItem);
}

void SWS_ProjectSetListView::OnItemClk(SWS_ListItem* item, int iCol, int iKeyState)
{
	pCurrSongItem = (SWS_SongItem*)item;
}

// "disable" sort
int SWS_ProjectSetListView::OnItemSort(SWS_ListItem* _item1, SWS_ListItem* _item2)
{
	int i1 = -1, i2 = -1;
	for (int i = 0; (i1 < 0 && i2 < 0) || i < g_SongItems.GetSize(); i++)
	{
		SWS_ListItem* item = (SWS_ListItem*)g_SongItems.Get(i);
		if (item == _item1) i1 = i;
		if (item == _item2) i2 = i;
	}
	if (i1 >= 0 && i2 >= 0) {
		if (i1 > i2) return 1;
		else if (i1 < i2) return -1;
	}
	return 0;
}

void SWS_ProjectSetListView::OnBeginDrag(SWS_ListItem* item)
{
	if (abs(m_iSortCol) == (COL_ID + 1)) //1-based
		SetCapture(GetParent(m_hwndList));

	dragInProgress = true;
}

void SWS_ProjectSetListView::OnDrag()
{
	POINT p;
	GetCursorPos(&p);
	SWS_SongItem* hitItem = (SWS_SongItem*)GetHitItem(p.x, p.y, NULL);
	if (hitItem)
	{
		int iNewPriority = g_SongItems.Find(hitItem);
		int iSelPriority = 0;

		WDL_PtrList<SWS_SongItem> draggedItems;
		int x = 0;
		SWS_SongItem* selItem;
		while ((selItem = (SWS_SongItem*)EnumSelected(&x)))
		{
			iSelPriority = g_SongItems.Find(selItem);
			if (iNewPriority == iSelPriority)
				return;
			draggedItems.Add(selItem);
		}

		// Remove the dragged items and then readd them
		// Switch order of add based on direction of drag & sort order
		bool bDir = iNewPriority > iSelPriority;
		if (m_iSortCol < 0)
			bDir = !bDir;
		for (int i = bDir ? 0 : draggedItems.GetSize() - 1; bDir ? i < draggedItems.GetSize() : i >= 0; bDir ? i++ : i--)
		{
			int index = g_SongItems.Find(draggedItems.Get(i));
			g_SongItems.Delete(index);
			g_SongItems.Insert(iNewPriority, draggedItems.Get(i));
		}

		g_pProjList->Update();

		++s_ignore_update;
		for (int i = 0; i < draggedItems.GetSize(); i++)
			SelectByItem((SWS_ListItem*)draggedItems.Get(i), i == 0, i == 0);
		--s_ignore_update;
	}
}

void SWS_ProjectSetListView::OnEndDrag()
{
	g_pProjList->Update();
	dragInProgress = false;
}

SWS_ProjectSetListWnd::SWS_ProjectSetListWnd()
	:SWS_DockWnd(IDD_PROJSETLIST, __LOCALIZE("Project Set List", "sws_DLG_158"), "SWSProjectSetList", SWSGetCommandID(OpenProjectSetList))
{
	m_id.Set(SETLIST_WND_ID);

	// Must call SWS_DockWnd::Init() to restore parameters and open the window if necessary
	Init();
}

void SWS_ProjectSetListWnd::OnInitDlg()
{
	m_resize.init_item(IDC_LIST, 0.0, 0.0, 1.0, 1.0);
	m_resize.init_item(IDC_SETLIST_INSERT, 1.0, 0.0, 1.0, 0.0);
	m_resize.init_item(IDC_SETLIST_DELETE, 1.0, 0.0, 1.0, 0.0);
	m_resize.init_item(IDC_SETLIST_MOVE_UP, 1.0, 0.0, 1.0, 0.0);
	m_resize.init_item(IDC_SETLIST_MOVE_DOWN, 1.0, 0.0, 1.0, 0.0);
	m_resize.init_item(IDC_SETLIST_LOAD, 1.0, 0.0, 1.0, 0.0);
	m_resize.init_item(IDC_SETLIST_SAVE, 1.0, 0.0, 1.0, 0.0);
	m_resize.init_item(IDC_SETLIST_CLEAR, 1.0, 0.0, 1.0, 0.0);
	m_resize.init_item(IDC_SETLIST_NEXT, 1.0, 0.0, 1.0, 0.0);
	m_resize.init_item(IDC_SETLIST_AUTOPLAY, 1.0, 0.0, 1.0, 0.0);
	m_resize.init_item(IDC_SETLIST_ENABLED, 1.0, 0.0, 1.0, 0.0);

	CheckDlgButton(m_hwnd, IDC_SETLIST_ENABLED, isSetListEnabled ? BST_CHECKED : BST_UNCHECKED);
	if (isSetListEnabled == FALSE)
		EnableWindow(GetDlgItem(m_hwnd, IDC_SETLIST_AUTOPLAY), FALSE);

	m_pLists.Add(new SWS_ProjectSetListView(GetDlgItem(m_hwnd, IDC_LIST), GetDlgItem(m_hwnd, IDC_EDIT)));
	m_parentVwnd.SetRealParent(m_hwnd);

	Update();

	if (pCurrSongItem) {
		int lastSelectedSong = pCurrSongItem->m_iSlot - 1;
		g_pProjList->SelectItemInList(lastSelectedSong);
	}

	if (!g_setListNameLoadedFromFileSystem.empty())
		m_wndTitle.Set(g_setListNameLoadedFromFileSystem.c_str());
}

void SWS_ProjectSetListWnd::OnResize()
{
	m_resize.get_item(IDC_LIST)->orig = m_resize.get_item(IDC_LIST)->real_orig;
	InvalidateRect(m_hwnd, NULL, 0);
}

HMENU SWS_ProjectSetListWnd::OnContextMenu(int x, int y, bool* wantDefaultItems)
{
	HMENU hMenu = CreatePopupMenu();
	AddToMenu(hMenu, __localizeFunc(g_projSetListMgrCmdTable[0].menuText, "sws_menu", 0), g_projSetListMgrCmdTable[0].accel.accel.cmd);
	AddToMenu(hMenu, __localizeFunc(g_projSetListMgrCmdTable[1].menuText, "sws_menu", 0), g_projSetListMgrCmdTable[1].accel.accel.cmd);
	return hMenu;
}

int ProjectSetListInit()
{
	g_pProjList = new SWS_ProjectSetListWnd();
	SWSRegisterCommands(g_projSetListMgrCmdTable);
	return 1;
}

void ProjectSetListExit()
{
	DELETE_NULL(g_pProjList);
}

void SWS_ProjectSetListWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
		case IDC_SETLIST_INSERT:
		{
			InsertSong();
			break;
		}
		case IDC_SETLIST_DELETE:
		{
			DeleteSong();
			break;
		}
		case IDC_SETLIST_NEXT:
		{
			LoadNextSong();
			break;
		}
		case IDC_SETLIST_MOVE_UP:
		{
			MoveSelectedSongsUp();
			break;
		}
		case IDC_SETLIST_MOVE_DOWN:
		{
			MoveSelectedSongsDown();
			break;
		}
		case IDC_SETLIST_LOAD:
		{
			OpenProjectsFromSetList(NULL);
			break;
		}
		case IDC_SETLIST_SAVE:
		{
			SaveProjectSetList(NULL);
			break;
		}
		case IDC_SETLIST_CLEAR:
		{
			g_SongItems.Empty();
			pCurrSongItem = NULL;
			pSongLoadedFromSetList = NULL;
			Update();
			break;
		}
		case IDC_SETLIST_ENABLED:
		{
			isSetListEnabled = (IsDlgButtonChecked(m_hwnd, IDC_SETLIST_ENABLED) == BST_CHECKED);
			EnableWindow(GetDlgItem(m_hwnd, IDC_SETLIST_AUTOPLAY), isSetListEnabled);
			if (isSetListEnabled)
			{
				isSetListEnabled = true;
				if (pSongLoadedFromSetList)
					nextSongToPlay = g_SongItems.Get(pCurrSongItem->m_iSlot);
			}
			else {
				isSetListEnabled = false;
				nextSongToPlay = NULL;
			}
			ProjectSetListUpdate();
			break;
		}
		case IDC_SETLIST_AUTOPLAY:
		{
			autoPlayNextSong = (IsDlgButtonChecked(m_hwnd, IDC_SETLIST_AUTOPLAY) == BST_CHECKED);
			break;
		}
		default:
		{
			Main_OnCommand((int)wParam, (int)lParam);
			break;
		}
	}
}

int SWS_ProjectSetListWnd::OnKey(MSG* msg, int iKeyState)
{
	pCurrSongItem = (SWS_SongItem*)m_pLists.Get(0)->EnumSelected(NULL);

	if ((iKeyState & LVKF_ALT)) {
		switch (msg->wParam)
		{
			case 'C':
			{
				ClearSetList();
				break;
			}
			case 'L':
			{
				pCurrSongItem = NULL;
				OpenProjectsFromSetList(NULL);
				break;
			}
			case 'S':
			{
				SaveProjectSetList(NULL);
				break;
			}
		}
		return 0;
	}

	if (msg->message == WM_KEYDOWN && msg->wParam == 'M') {
		return MoveSelectedSongsDown();
	}

	if (msg->message == WM_KEYDOWN && msg->wParam == 'P') {
		return MoveSelectedSongsUp();
	}

	if (msg->message == WM_KEYDOWN && !iKeyState)
	{
		if (msg->wParam == 'N')
		{
			return LoadNextSong();
		}

		if (msg->wParam == 'L')
		{
			pCurrSongItem = NULL;
			OpenProjectsFromSetList(NULL);
			return 1;
		}

		if (msg->wParam == 'S')
		{
			SaveProjectSetList(NULL);
			return 1;
		}

		if (msg->wParam == VK_INSERT || msg->wParam == 'A')
		{
			return InsertSong();
		}

		if (msg->wParam == 'C')
		{
			return ClearSetList();
		}

		if (msg->wParam == 'O')
		{
			isSetListEnabled = (IsDlgButtonChecked(m_hwnd, IDC_SETLIST_ENABLED) == BST_CHECKED);
			EnableWindow(GetDlgItem(m_hwnd, IDC_SETLIST_AUTOPLAY), !isSetListEnabled);
			CheckDlgButton(m_hwnd, IDC_SETLIST_ENABLED, !isSetListEnabled ? BST_CHECKED : BST_UNCHECKED);
			if (isSetListEnabled)
			{
				isSetListEnabled = false;
				nextSongToPlay = NULL;
			}
			else {
				isSetListEnabled = true;
				if (pSongLoadedFromSetList)
					nextSongToPlay = g_SongItems.Get(pCurrSongItem->m_iSlot);
			}
			ProjectSetListUpdate();
			return 1;
		}
		if (msg->wParam == 'Y')
		{
			autoPlayNextSong = !(IsDlgButtonChecked(m_hwnd, IDC_SETLIST_AUTOPLAY) == BST_CHECKED);
			CheckDlgButton(m_hwnd, IDC_SETLIST_AUTOPLAY, autoPlayNextSong ? BST_CHECKED : BST_UNCHECKED);
			return 1;
		}

		if (msg->wParam == VK_DELETE || msg->wParam == 'D')
		{
			return DeleteSong();
		}
		else if (msg->wParam == VK_RETURN)
		{
			if (m_pLists.Get(0))
			{
				SWS_SongItem* item = (SWS_SongItem*)m_pLists.Get(0)->EnumSelected(NULL);
				songManuallyLoaded = true;
				LoadSong(item);
			}

			return 1;
		}
	}
	return 0;
}

int SWS_ProjectSetListWnd::ClearSetList()
{
	g_SongItems.Empty(true);
	pCurrSongItem = nullptr;
	pSongLoadedFromSetList = nullptr;
	g_setListNameLoadedFromFileSystem = __LOCALIZE("Project Set List", "sws_DLG_158");
	ProjectSetListUpdate();
	return 1;
}

int SWS_ProjectSetListWnd::LoadNextSong()
{
	if (g_SongItems.GetSize() == 0)
		return 1;

	songManuallyLoaded = true;
	if (nextSongToPlay) {
		pCurrSongItem = nextSongToPlay;
		LoadSong(nextSongToPlay);
	}
	else {
		SWS_SongItem* nextSong = nullptr;
		if (pSongLoadedFromSetList) {
			nextSong = g_SongItems.Get(pSongLoadedFromSetList->m_iSlot);
		}
		else {
			nextSong = g_SongItems.Get(pCurrSongItem->m_iSlot);
		}

		if (nextSong) {
			pCurrSongItem = nextSong;
			LoadSong(nextSong);
		}
	}
	return 1;
}

void LoadSong(SWS_SongItem* songItem)
{
	if (isReaperPlayingTheSong) {
		nextSongToPlay = songItem;
		ProjectSetListUpdate();
		return;
	}

	if (songItem == nullptr)
		return;

	char* fileName = songItem->m_song_name.Get();
	Main_openProject(fileName);
	pSongLoadedFromSetList = songItem;
	if (isSetListEnabled) {
		// Go to the begining of the song.
		Main_OnCommandEx(40042, 0, NULL);
		// Start playing the song.
		if (songManuallyLoaded == false && autoPlayNextSong == true)
			Main_OnCommandEx(40044, 0, NULL);
		nextSongToPlay = g_SongItems.Get(pCurrSongItem->m_iSlot);
	}
	ProjectSetListUpdate();
}

int SWS_ProjectSetListWnd::MoveSelectedSongsUp()
{
	SWS_SongItem* hitItem = (SWS_SongItem*)m_pLists.Get(0)->EnumSelected(NULL, -1);
	if (hitItem)
	{
		int iNewPriority = g_SongItems.Find(hitItem);
		int iSelPriority;

		WDL_PtrList<SWS_SongItem> draggedItems;
		int x = 0;
		SWS_SongItem* selItem;
		while ((selItem = (SWS_SongItem*)m_pLists.Get(0)->EnumSelected(&x)))
		{
			iSelPriority = g_SongItems.Find(selItem);
			draggedItems.Add(selItem);
		}

		for (int i = draggedItems.GetSize() - 1; i >= 0; i--)
		{
			int index = g_SongItems.Find(draggedItems.Get(i));
			g_SongItems.Delete(index);
			g_SongItems.Insert(iNewPriority, draggedItems.Get(i));
		}

		g_pProjList->Update();

		for (int i = 0; i < draggedItems.GetSize(); i++)
			GetListView()->SelectByItem((SWS_ListItem*)draggedItems.Get(i), i == 0, i == 0);
	}
	return 1;
}

int SWS_ProjectSetListWnd::MoveSelectedSongsDown()
{
	int iNewPriority = 0;
	WDL_PtrList<SWS_SongItem> draggedItems;
	int x = 0;
	SWS_SongItem* selItem;
	while ((selItem = (SWS_SongItem*)m_pLists.Get(0)->EnumSelected(&x)))
	{
		draggedItems.Add(selItem);
		iNewPriority = selItem->m_iSlot;
	}

	for (int i = 0; i < draggedItems.GetSize(); i++)
	{
		int index = g_SongItems.Find(draggedItems.Get(i));
		g_SongItems.Delete(index);
		g_SongItems.Insert(iNewPriority, draggedItems.Get(i));
	}

	g_pProjList->Update();

	for (int i = 0; i < draggedItems.GetSize(); i++)
		GetListView()->SelectByItem((SWS_ListItem*)draggedItems.Get(i), i == 0, i == 0);

	return 1;
}

void SWS_ProjectSetListWnd::SelectItemInList(int idx)
{
	if (m_pLists.Get(0)) {
		HWND hwnd = GetDlgItem(m_hwnd, IDC_LIST);
		ListView_SetItemState(hwnd, -1, LVIF_STATE, LVIS_SELECTED);
		SetFocus(hwnd);
		ListView_SetItemState(hwnd, idx, LVIS_SELECTED, LVIS_SELECTED);
		ListView_SetItemState(hwnd, idx, LVIS_FOCUSED, LVIS_FOCUSED);
	}
}

int SWS_ProjectSetListWnd::GetNumberOfSongsLoadedInSetList()
{
	int i = 0;
	if (m_pLists.Get(0))
		i = m_pLists.Get(0)->GetListItemCount();

	return i;
}

int SWS_ProjectSetListWnd::DeleteSong()
{
	int x = 0;
	int idx = 0;
	SWS_SongItem* item;
	while ((item = (SWS_SongItem*)m_pLists.Get(0)->EnumSelected(&x)))
	{
		idx = g_SongItems.Find(item);
		if (idx >= 0)
			g_SongItems.Delete(idx);
	}

	for (int i = 0; i < g_SongItems.GetSize(); i++)
		g_SongItems.Get(i)->m_iSlot = i + 1;

	if (idx < g_SongItems.GetSize())
		pCurrSongItem = g_SongItems.Get(idx);

	ProjectSetListUpdate();
	return 1;
}

int SWS_ProjectSetListWnd::InsertSong()
{
	if (!m_pLists.GetSize())
		return 1;

	char directory[MAX_PATH]{};
	GetProjectPath(directory, sizeof(directory));

	vector<char*> filenames;
#ifdef _WIN32
	// Windows can add multiple songs from open file dialog.
	filenames = BrowseForFilesMultiSelect(__LOCALIZE("Select song", "sws_mbox"), directory, nullptr, true, "Reaper Project Song (*.RPP)\0*.RPP\0All Files\0*.*\0");
#else
	char* filename = BrowseForFiles(__LOCALIZE("Select song", "sws_mbox"), directory, nullptr, false, "Reaper Project Song (*.RPP)\0*.RPP\0All Files\0*.*\0");
	filenames.push_back(filename);
#endif 

	if (filenames.empty()) {
		ProjectSetListUpdate();
		return 1;
	}

	int pos = 1;
	SWS_SongItem* selectedSong = (SWS_SongItem*)GetListView()->EnumSelected(NULL);
	if (selectedSong)
	{
		pos = selectedSong->m_iSlot - 1;
		for (int i = pos; i < g_SongItems.GetSize(); i++)
			g_SongItems.Get(i)->m_iSlot = g_SongItems.Get(i)->m_iSlot + filenames.size();
	}

	SWS_SongItem* newItem = nullptr;
	int firstInsertedItemPos = 0;
	bool firstItemRead = true;
	for (char* filename : filenames)
	{
		if (firstItemRead) {
			firstItemRead = false;
			firstInsertedItemPos = pos;
		}
		newItem = new SWS_SongItem(pos, filename);
		g_SongItems.Insert(pos, newItem);
		pos++;
	}

	if (!pCurrSongItem)
		pCurrSongItem = newItem;

	ProjectSetListUpdate();

	return 1;
}

void OpenProjectSetList(COMMAND_T*)
{
	g_pProjList->Show(true, true);
}

int ProjectSetListEnabled(COMMAND_T*)
{
	return g_pProjList->IsWndVisible();
}

void ProjectSetListPlayState(bool play, bool pause, bool rec) {
	if (play) {
		isReaperPlayingTheSong = true;
	}
	else {
		isReaperPlayingTheSong = false;

		char buf[512] = "";
		char fileName[512];
		char fileName2[512];

		if (isSetListEnabled)
		{
			if (nextSongToPlay == NULL) {
				return;
			}

			ReaProject* proj = EnumProjects(-1, buf, sizeof(buf));

			int mediaItems = CountMediaItems(proj);
			if (mediaItems == 0)
				return;

			lstrcpyn(fileName, GetFilenameWithExt(buf), sizeof(fileName));
			lstrcpyn(fileName2, GetFilenameWithExt(nextSongToPlay->m_song_name.Get()), sizeof(fileName2));

			if (!strcmp(fileName, fileName2))
				return;

			double pos = GetPlayPosition2Ex(proj);
			double projectLength = GetProjectLength(proj);

			if (pos > 0 && projectLength > 0 && abs(projectLength - pos) < 1) {
				if (g_pProjList) {
					if (isSetListEnabled) {
						pCurrSongItem = nextSongToPlay;
						songManuallyLoaded = false;
						LoadSong(nextSongToPlay);
					}
				}
			}
		}
	}
}

void SWS_ProjectSetListWnd::Update()
{
	SetWindowText(m_hwnd, g_setListNameLoadedFromFileSystem.c_str());
	for (int i = 0; i < g_SongItems.GetSize(); i++) {
		if (SWS_SongItem* item = (SWS_SongItem*)g_SongItems.Get(i)) {
			item->m_iSlot = i + 1;
		}
	}

	if (m_pLists.Get(0))
		m_pLists.Get(0)->Update();
}

void ProjectSetListUpdate()
{
	int idx = 0;
	if (g_pProjList)
	{
		if (pCurrSongItem)
			idx = pCurrSongItem->m_iSlot - 1;

		g_pProjList->Update();
		if (idx >= 0)
			g_pProjList->SelectItemInList(idx);
	}
	return;
}