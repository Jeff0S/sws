/******************************************************************************
/ sws_wnd.h
/
/ Copyright (c) 2012 and later Tim Payne (SWS), Jeffos
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

#pragma once

#define TOOLTIP_MAX_LEN					512
#define CELL_MAX_LEN					256
#define MIN_DOCKWND_WIDTH				147
#define MIN_DOCKWND_HEIGHT				100

#ifdef _WIN32
#define SWSDLG_TYPEFACE					"MS Shell Dlg"
#define LISTVIEW_COLORHOOK_STATESIZE	3
#else
#define SWSDLG_TYPEFACE					"Arial"
#define LISTVIEW_COLORHOOK_STATESIZE	(3+4)
#endif

typedef struct SWS_LVColumn
{
	int iWidth;
	int iType; // & 1 = editable, & 2 = clickable button
	const char* cLabel;
	int iPos;
} SWS_LVColumn;

class SWS_ListItem; // abstract.  At some point it might make sense to make this a real class?

class SWS_ListItemList
{
public:
	SWS_ListItemList() {}
	~SWS_ListItemList() {}
	int GetSize() { return m_list.GetSize(); }
	void Add(SWS_ListItem* item, bool sort = true) { if (sort) m_list.InsertSorted(item, ILIComp); else m_list.Add(item); }
	SWS_ListItem* Get(int iIndex) { return (SWS_ListItem*)m_list.Get(iIndex); }
	int Find(SWS_ListItem* item) { return m_list.FindSorted(item, ILIComp); }
	void Delete(int iIndex) { m_list.Delete(iIndex); }
	// Remove returns and also removes the last item.  It's the last because it's more efficient to remove at the end.
	SWS_ListItem* Remove() { if (!m_list.GetSize()) return NULL; int last = m_list.GetSize()-1; SWS_ListItem* item = (SWS_ListItem*)m_list.Get(last); m_list.Delete(last); return item; }
	void Empty() { m_list.Empty(); }
private:
	static int ILIComp(const SWS_ListItem** a, const SWS_ListItem** b) { return (*a > *b ? 1 : *a < *b ? -1 : 0); };
	WDL_PtrList<SWS_ListItem> m_list;
};

class SWS_ListView
{
public:
	SWS_ListView(HWND hwndList, HWND hwndEdit, int iCols, SWS_LVColumn* pCols, const char* cINIKey, bool bTooltips, const char* cLocalizeSection, bool bDrawArrow=true);
	virtual ~SWS_ListView();
	int GetColumnCount() { return m_iCols; }
	int GetListItemCount() { return ListView_GetItemCount(m_hwndList); }
	SWS_ListItem* GetListItem(int iIndex, int* iState = NULL);
	bool IsSelected(int index);
	SWS_ListItem* EnumSelected(int* i, int iOffset = 0);
	int CountSelected ();
	bool SelectByItem(SWS_ListItem* item, bool bSelectOnly = true, bool bEnsureVisible = true);
	int OnNotify(WPARAM wParam, LPARAM lParam);
	void OnDestroy();
	virtual void OnDrag() {}
	virtual void OnEndDrag() {}
	int EditingKeyHandler(MSG *msg);
	int LVKeyHandler(MSG *msg, int iKeyState);
	void Update(bool reassign = false);
	bool DoColumnMenu(int x, int y);
	bool HeaderHitTest(const POINT &) const;
	SWS_ListItem* GetHitItem(int x, int y, int* iCol);
	void EditListItem(SWS_ListItem* item, int iCol);
	int GetEditingItem() { return m_iEditingItem; }
	bool EditListItemEnd(bool bSave, bool bResort = true);
	int OnEditingTimer();
	const char* GetINIKey() { return m_cINIKey; }
	int DisplayToDataCol(int iCol);
	int DataToDisplayCol(int iCol);
	int* GetOldColors() { return m_oldColors; }
	int GetSortColumn() { return m_iSortCol; }
	void SetSortColumn(int iCol) { m_iSortCol = iCol; }
	
	bool IsActive(bool bWantEdit) { return GetFocus() == m_hwndList || (bWantEdit && m_iEditingItem != -1); }
	void DisableUpdates(bool bDisable) { m_bDisableUpdates = bDisable; }
	bool UpdatesDisabled() { return m_bDisableUpdates; }
	HWND GetHWND() { return m_hwndList; }
	HWND GetEditHWND() { return m_hwndEdit; }
	virtual bool HideGridLines() {return false;}

protected:
	void EditListItem(int iIndex, int iCol);

	// These methods are used to "pull" data for updating the listview
	virtual void SetItemText(SWS_ListItem* item, int iCol, const char* str) {}
	virtual void GetItemText(SWS_ListItem* item, int iCol, char* str, int iStrMax) { str[0] = 0; }
	virtual void GetItemTooltip(SWS_ListItem* item, char* str, int iStrMax) {}
	virtual void GetItemList(SWS_ListItemList* pList) { pList->Empty(); }
	virtual int  GetItemState(SWS_ListItem* item) { return -1; } // Selection state: -1 == unchanged, 0 == false, 1 == selected
	// These inform the derived class of user interaction
	virtual bool OnItemSelChanging(SWS_ListItem* item, bool bSel) { return false; } // Returns TRUE to prevent the change, or FALSE to allow the change
	virtual void OnItemSelChanged(SWS_ListItem* item, int iState) { }
	virtual void OnItemClk(SWS_ListItem* item, int iCol, int iKeyState) {}
	virtual void OnItemBtnClk(SWS_ListItem* item, int iCol, int iKeyState) {}
	virtual void OnItemDblClk(SWS_ListItem* item, int iCol) {}
	virtual int  OnItemSort(SWS_ListItem* item1, SWS_ListItem* item2);
	virtual void OnItemSortEnd() {};
	virtual void OnBeginDrag(SWS_ListItem* item) {}
	virtual bool IsEditListItemAllowed(SWS_ListItem* item, int iCol) { return true; }

	void SetListviewColumnArrows(int iSortCol);
	static int CALLBACK sListCompare(LPARAM lParam1, LPARAM lParam2, LPARAM lSortParam);

	HWND m_hwndList;
	HWND m_hwndTooltip;
	bool m_bDisableUpdates;
	int m_iSortCol; // 1 based col index, negative for desc sort
	int m_iEditingItem;
	int m_iEditingCol;
	const int m_iCols;
	SWS_LVColumn* m_pCols;
	bool m_bDrawArrow;
	const char* m_cLocalizeSection;
	int m_oldColors[LISTVIEW_COLORHOOK_STATESIZE];

#ifndef _WIN32
	int m_iClickedKeys;
#endif

private:
	void ShowColumns();
	void Sort();

#ifndef _WIN32
	int m_iClickedCol;
	SWS_ListItem* m_pClickedItem;
#else
	DWORD m_dwSavedSelTime;
	bool m_bShiftSel;
#endif
	WDL_TypedBuf<int> m_pSavedSel;
	HWND m_hwndEdit;
	SWS_LVColumn* m_pDefaultCols;
	const char* m_cINIKey;
};

#pragma pack(push, 4)
typedef struct SWS_DockWnd_State // Converted to little endian on store
{
	RECT r;
	int state;
	int whichdock;
} SWS_DockWnd_State;
#pragma pack(pop)

class SWS_DockWnd
{
public:
	// Unless you need the default constructor (new SWS_DockWnd()), you must provide all parameters
	explicit SWS_DockWnd(int iResource=0, const char* cWndTitle="", const char* cId="");
	virtual ~SWS_DockWnd();

	virtual bool IsActive(bool bWantEdit = false);
	bool IsDocked() { return (m_state.state & 2) == 2; }
	bool IsValidWindow() { return SWS_IsWindow(m_hwnd) ? true : false; }
	bool IsWndVisible() {return IsValidWindow() && IsWindowVisible(m_hwnd);}	
	HWND GetHWND() { return m_hwnd; }
	WDL_VWnd* GetParentVWnd() { return &m_parentVwnd; }
	WDL_VWnd_Painter* GetVWndPainter() { return &m_vwnd_painter; }
	SWS_ListView* GetListView(int i = 0) { return m_pLists.Get(i); }

	void Show(bool bToggle, bool bActivate);
	void ToggleDocking();
	int SaveState(char* cStateBuf, int iMaxLen);
	void LoadState(const char* cStateBuf, int iLen);
	virtual void OnCommand(WPARAM wParam, LPARAM lParam) {}
	virtual bool CloseOnCancel () {return true;}

	static LRESULT screensetCallback(int action, const char *id, void *param, void *actionParm, int actionParmSize);

	static const int DOCK_MSG = 0xFF0000;

protected:

	virtual INT_PTR WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
	void Init(); // call from derived constructor!!
	virtual void GetMinSize(int* w, int* h) { *w=MIN_DOCKWND_WIDTH; *h=MIN_DOCKWND_HEIGHT; }

	virtual void OnInitDlg() {}
	virtual int OnNotify(WPARAM wParam, LPARAM lParam) { return 0; }
	virtual HMENU OnContextMenu(int x, int y, bool* wantDefaultItems) { return NULL; }
	virtual bool ReprocessContextMenu() {return true;} // return false to prevent setting key assignments for ids that correspond to mapped action ids
	virtual bool NotifyOnContextMenu() {return true;}  // return false to prevent sending notification to OnCommand() and send it to ContextMenuReturnId() instead
	virtual void ContextMenuReturnId (int id) {} // see NotifyOnContextMenu()
	virtual bool OnPaint() { return false; } // return true if implemented
	virtual void OnResize() {}
	virtual void OnDestroy() {}
	virtual void OnTimer(WPARAM wParam=0) {}
	virtual void OnDroppedFiles(HDROP h) {}
	virtual int OnKey(MSG* msg, int iKeyState) { return 0; } // return 1 for "processed key"
	virtual int OnMouseDown(int xpos, int ypos) { return 0; } // return -1 to eat, >0 to capture
	virtual bool OnMouseDblClick(int xpos, int ypos) { return false; }
	virtual bool OnMouseMove(int xpos, int ypos) { return false; }
	virtual bool OnMouseUp(int xpos, int ypos) { return false; }
	virtual INT_PTR OnUnhandledMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) { return 0; }

	// Functions for WDL_VWnd-based GUIs
	virtual void DrawControls(LICE_IBitmap* bm, const RECT* r, int* tooltipHeight = NULL) {}
	virtual bool GetToolTipString(int xpos, int ypos, char* bufOut, int bufOutSz) { return false; }
	virtual void KillTooltip(bool doRefresh=false);

	HWND m_hwnd;
	int m_iResource;
	WDL_FastString m_wndTitle;
	WDL_FastString m_id;
	accelerator_register_t m_ar;
	SWS_DockWnd_State m_state;
	bool m_bUserClosed;
	bool m_bSaveStateOnDestroy;
	WDL_WndSizer m_resize;
	WDL_PtrList<SWS_ListView> m_pLists;
	WDL_VWnd_Painter m_vwnd_painter;
	WDL_VWnd m_parentVwnd; // owns all children VWnds
	char m_tooltip[TOOLTIP_MAX_LEN];
	POINT m_tooltip_pt;

private:
	static INT_PTR WINAPI sWndProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static int keyHandler(MSG *msg, accelerator_register_t *ctx);
};


char* SWS_LoadDockWndStateBuf(const char* _id, int _len = -1);
int SWS_GetDockWndState(const char* _id, const char* _stateBuf = NULL);
void SWS_SetDockWndState(const char* _stateBuf, int _len, SWS_DockWnd_State* _stateOut);

bool ListView_HookThemeColorsMessage(HWND hwndDlg, int uMsg, LPARAM lParam, int cstate[LISTVIEW_COLORHOOK_STATESIZE], int listID, int whichTheme, int wantGridForColumns);
void DrawTooltipForPoint(LICE_IBitmap *bm, POINT mousePt, RECT *wndr, const char *text);
