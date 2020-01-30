/******************************************************************************
/ ProjectSetList.h
/
/ Copyright (c) 2020 RS
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

class SWS_SongItem
{
public:
	SWS_SongItem(int slot, const char* song_name)
		: m_iSlot(slot), m_song_name(song_name)
	{
	}
	int m_iSlot;
	WDL_String m_song_name;
};

extern WDL_PtrList<SWS_SongItem> g_SongItems;
extern string g_setListNameLoadedFromFileSystem;

#pragma once
class SWS_ProjectSetListView : public SWS_ListView
{
public:
	SWS_ProjectSetListView(HWND hwndList, HWND hwndEdit);

protected:
	void GetItemText(SWS_ListItem* item, int iCol, char* str, int iStrMax);
	void GetItemList(SWS_ListItemList* pList);
	void OnItemDblClk(SWS_ListItem* item, int iCol);
	void OnItemClk(SWS_ListItem* item, int iCol, int iKeyState);
	int OnItemSort(SWS_ListItem* _item1, SWS_ListItem* _item2);
	void OnDrag();
	void OnBeginDrag(SWS_ListItem* item);
	void OnEndDrag();
};

class SWS_ProjectSetListWnd : public SWS_DockWnd
{
public:
	SWS_ProjectSetListWnd();
	void Update();
	void SelectItemInList(int idx);
	int GetNumberOfSongsLoadedInSetList();

protected:
	void OnInitDlg();
	void OnCommand(WPARAM wParam, LPARAM lParam);
	HMENU OnContextMenu(int x, int y, bool* wantDefaultItems);
	int OnKey(MSG* msg, int iKeyState);
	int ClearSetList();
	int LoadNextSong();
	int MoveSelectedSongsUp();
	int MoveSelectedSongsDown();
	int DeleteSong();
	int InsertSong();
	void OnResize();
};

int ProjectSetListEnabled(COMMAND_T*);
int ProjectSetListInit();
void ProjectSetListExit();

void LoadSong(SWS_SongItem* item);
void OpenProjectSetList(COMMAND_T*);
void ProjectSetListUpdate();
void ProjectSetListPlayState(bool play, bool pause, bool rec);
