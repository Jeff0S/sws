/******************************************************************************
/ sws_wnd.cpp
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


/* From askjf.com:
	* It's often the easiest to build your application out of plain dialog (DialogBoxParam() or CreateDialogParam())
	* Use Get/SetWindowLong(hwnd,GWL_USERDATA) (or GWLP_USERDATA for x64) for "this" pointers, if an object owns the dialog. Set it in WM_INITDIALOG, and get it for other messages.
	* On the same token, when the owner object has a dialog, it's good to do the following:
		  o The object's constructor sets m_hwnd to 0.
		  o The object's destructor does if (m_hwnd) DestroyWindow(m_hwnd);
		  o WM_INITDIALOG sets this->m_hwnd to hwndDlg
		  o WM_DESTROY clears this->m_hwnd to 0
*/

#include "stdafx.h"

#include "./SnM/SnM.h"
#include "./SnM/SnM_Dlg.h"
#include "./Breeder/BR_Util.h"

#include <WDL/localize/localize.h>
#ifndef _WIN32
#  include <WDL/swell/swell-dlggen.h>
#endif


#define CELL_EDIT_TIMER		0x1000
#define CELL_EDIT_TIMEOUT	50
#define TOOLTIP_TIMER		0x1001
#define TOOLTIP_TIMEOUT		350


SWS_DockWnd::SWS_DockWnd(int iResource, const char* cWndTitle, const char* cId)
		: m_hwnd(NULL), m_iResource(iResource), m_wndTitle(cWndTitle), m_id(cId), m_bUserClosed(false), m_bSaveStateOnDestroy(true)
{
	if (cId && *cId) // e.g. default constructor
	{
		screenset_unregister((char*)cId);
		screenset_registerNew((char*)cId, screensetCallback, this);
	}

	memset(&m_state, 0, sizeof(SWS_DockWnd_State));
	*m_tooltip = '\0';
	m_ar.translateAccel = keyHandler;
	m_ar.isLocal = true;
	m_ar.user = this;
	plugin_register("accelerator", &m_ar);
}

// Init() must be called from the constructor of *every* derived class.  Unfortunately,
// you can't just call Init() from the DockWnd/base class, because the vTable isn't
// setup yet.
void SWS_DockWnd::Init()
{
	int iLen = sizeof(SWS_DockWnd_State);
	char* cState = SWS_LoadDockWndStateBuf(m_id.Get(), iLen);
	LoadState(cState, iLen);
	delete [] cState;
}

SWS_DockWnd::~SWS_DockWnd()
{
	plugin_register("-accelerator", &m_ar);
	if (m_id.GetLength())
		screenset_unregister((char*)m_id.Get());

	if (m_hwnd)
	{
		m_bSaveStateOnDestroy = false;
		DestroyWindow(m_hwnd);
	}
}

void SWS_DockWnd::Show(bool bToggle, bool bActivate)
{
	if (!SWS_IsWindow(m_hwnd))
	{
		CreateDialogParam(g_hInst, MAKEINTRESOURCE(m_iResource), g_hwndParent, SWS_DockWnd::sWndProc, (LPARAM)this);
		if (IsDocked() && bActivate)
			DockWindowActivate(m_hwnd);
#ifndef _WIN32
		// TODO see if DockWindowRefresh works here
		InvalidateRect(m_hwnd, NULL, TRUE);
#endif
	}
	else if (!IsWindowVisible(m_hwnd) || (bActivate && !bToggle))
	{
		if ((m_state.state & 2))
			DockWindowActivate(m_hwnd);
		else
			ShowWindow(m_hwnd, SW_SHOW);
		SetFocus(m_hwnd);
	}
	else if (bToggle)// If already visible close the window
		SendMessage(m_hwnd, WM_COMMAND, IDCANCEL, 0);
}

bool SWS_DockWnd::IsActive(bool bWantEdit)
{
	if (!IsValidWindow())
		return false;

	for (int i = 0; i < m_pLists.GetSize(); i++)
		if (m_pLists.Get(i)->IsActive(bWantEdit))
			return true;

	HWND hfoc = GetFocus();
	return hfoc == m_hwnd || IsChild(m_hwnd, hfoc);
}

INT_PTR WINAPI SWS_DockWnd::sWndProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	SWS_DockWnd* pObj = (SWS_DockWnd*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	if (!pObj && uMsg == WM_INITDIALOG)
	{
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		pObj = (SWS_DockWnd*)lParam;
		pObj->m_hwnd = hwndDlg;
	}
	return pObj ? pObj->WndProc(uMsg, wParam, lParam) : 0;
}

INT_PTR SWS_DockWnd::WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (SWS_THEMING)
	{
		// theme list views
		for (int i=0; i < m_pLists.GetSize(); i++)
			if (SWS_ListView* lv = m_pLists.Get(i))
				if (ListView_HookThemeColorsMessage(m_hwnd, uMsg, lParam, lv->GetOldColors(), GetWindowLong(lv->GetHWND(),GWL_ID), 0, lv->HideGridLines() ? 0 :lv->GetColumnCount()))
					return 1;
		// theme other ctrls
		if (INT_PTR r = SNM_HookThemeColorsMessage(m_hwnd, uMsg, wParam, lParam, false))
			return r;
	}

	// Since there are no virtual functions for WM_SHOWWINDOW, let the message get passed on just in case
	if (uMsg == WM_SHOWWINDOW)
		RefreshToolbar(0);

	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			m_resize.init(m_hwnd);
			OnInitDlg(); // derived class initialization
			SetWindowText(m_hwnd, m_wndTitle.Get());

			if (SWS_THEMING)
			{
				for (int i=0; i<m_pLists.GetSize(); i++)
				{
#ifndef _WIN32
					// override list view props for grid line theming
					ListView_SetExtendedListViewStyleEx(m_pLists.Get(i)->GetHWND(),
						LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP, LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP);
#endif
					SNM_ThemeListView(m_pLists.Get(i)); // initial theming, then ListView_HookThemeColorsMessage() does the job
				}
			}

			if ((m_state.state & 2))
			{
				DockWindowAddEx(m_hwnd, m_wndTitle.Get(), m_id.Get(), true);
			}
			else
			{
				EnsureNotCompletelyOffscreen(&m_state.r);
				if (m_state.r.left || m_state.r.top || m_state.r.right || m_state.r.bottom)
					SetWindowPos(m_hwnd, NULL, m_state.r.left, m_state.r.top, m_state.r.right-m_state.r.left, m_state.r.bottom-m_state.r.top, SWP_NOZORDER);
				AttachWindowTopmostButton(m_hwnd);
				AttachWindowResizeGrip(m_hwnd);
				ShowWindow(m_hwnd, SW_SHOW);
			}
			break;
		}
		case WM_TIMER:
			if (wParam == CELL_EDIT_TIMER)
			{
				for (int i = 0; i < m_pLists.GetSize(); i++)
					if (m_pLists.Get(i)->GetEditingItem() != -1)
						return m_pLists.Get(i)->OnEditingTimer();
			}
			else if (wParam == TOOLTIP_TIMER)
			{
				KillTimer(m_hwnd, wParam);

				POINT p; GetCursorPos(&p);
				ScreenToClient(m_hwnd,&p);
				RECT r; GetClientRect(m_hwnd,&r);
				char buf[TOOLTIP_MAX_LEN] = "";
				if (PtInRect(&r,p))
					if (!m_parentVwnd.GetToolTipString(p.x,p.y,buf,sizeof(buf)) && !GetToolTipString(p.x,p.y,buf,sizeof(buf)))
						*buf='\0';

				if (strcmp(buf, m_tooltip))
				{
					m_tooltip_pt = p;
					lstrcpyn(m_tooltip,buf,sizeof(m_tooltip));
					InvalidateRect(m_hwnd,NULL,FALSE);
				}
			}
			else
				OnTimer(wParam);
			break;
		case WM_NOTIFY:
		{
			NMHDR* hdr = (NMHDR*)lParam;
			for (int i = 0; i < m_pLists.GetSize(); i++)
				if (hdr->hwndFrom == m_pLists.Get(i)->GetHWND())
					return m_pLists.Get(i)->OnNotify(wParam, lParam);

			return OnNotify(wParam, lParam);
		}
		case WM_CONTEXTMENU:
		{
			KillTooltip(true);
			int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
			for (int i = 0; i < m_pLists.GetSize(); i++)
			{
				m_pLists.Get(i)->EditListItemEnd(true);
				// Are we over the column header?
				if (m_pLists.Get(i)->DoColumnMenu(x, y))
					return 0;
			}

			// SWS issue 373 - removed code from here that removed all but one selection on right click on OSX

			bool wantDefaultItems = true;
			HMENU hMenu = OnContextMenu(x, y, &wantDefaultItems);
			int dockId   = 0; // when NotifyOnContextMenu() returns false user could have put anything in there, so we'll just find unused menu ids not to interfere
			int cancelId = 0;
			if (wantDefaultItems)
			{
				if (!hMenu)
					hMenu = CreatePopupMenu();
				else
					AddToMenu(hMenu, SWS_SEPARATOR, 0);

				// Add std menu items
				char str[128];
				if (snprintf(str, sizeof(str), __LOCALIZE_VERFMT("Dock %s in Docker","sws_menu"), m_wndTitle.Get()) > 0)
				{
					if (!NotifyOnContextMenu())
						dockId = GetUnusedMenuId(hMenu);
					else
						dockId = DOCK_MSG;
					AddToMenu(hMenu, str, dockId);
				}
				// Check dock state
				if ((m_state.state & 2))
					CheckMenuItem(hMenu, dockId, MF_BYCOMMAND | MF_CHECKED);

				if (!NotifyOnContextMenu())
					cancelId = GetUnusedMenuId(hMenu);
				else
					cancelId = IDCANCEL;
				AddToMenu(hMenu, __LOCALIZE("Close window","sws_menu"), cancelId);
			}

			if (hMenu)
			{
				if (x == -1 || y == -1)
				{
					RECT r;
					GetWindowRect(m_hwnd, &r);
					x = r.left;
					y = r.top;
				}
				if (ReprocessContextMenu())
					kbd_reprocessMenu(hMenu, NULL);

				int id = TrackPopupMenu(hMenu, NotifyOnContextMenu() ? 0 : (TPM_RETURNCMD | TPM_NONOTIFY), x, y, 0, m_hwnd, NULL);
				DestroyMenu(hMenu);

				if (!NotifyOnContextMenu())
				{
					if (wantDefaultItems && (id == dockId || id == cancelId))
						SendMessage(m_hwnd, WM_COMMAND, (id == dockId) ?  DOCK_MSG : IDCANCEL, 0);
					else
						ContextMenuReturnId(id);
				}
			}
			break;
		}
		case WM_COMMAND:
			OnCommand(wParam, lParam);
			switch (wParam)
			{
				case DOCK_MSG:
					ToggleDocking();
					break;
				case IDCANCEL:
				case IDOK:
					if (wParam == IDOK || (wParam == IDCANCEL && CloseOnCancel()))
					{
						m_bUserClosed = true;
						DestroyWindow(m_hwnd);
					}
					break;
			}
			break;
		case WM_SIZE:
			if (wParam != SIZE_MINIMIZED)
			{
				static bool bRecurseCheck = false;
				if (!bRecurseCheck)
				{
					bRecurseCheck = true;
					for (int i = 0; i < m_pLists.GetSize(); i++)
						m_pLists.Get(i)->EditListItemEnd(true);
					KillTooltip();
					OnResize();
					m_resize.onResize();
					bRecurseCheck = false;
				}
			}
			break;
		// define a min size (+ fix flickering when docked)
		case WM_GETMINMAXINFO:
			if (lParam)
			{
				int w, h;
				GetMinSize(&w, &h);
				if (!IsDocked())
				{
					RECT rClient, rWnd;
					GetClientRect(m_hwnd, &rClient);
					GetWindowRect(m_hwnd, &rWnd);
					w += (rWnd.right - rWnd.left) - rClient.right;
					h += (rWnd.bottom - rWnd.top) - rClient.bottom;
				}
				LPMINMAXINFO l = (LPMINMAXINFO)lParam;
				l->ptMinTrackSize.x = w;
				l->ptMinTrackSize.y = h;
			}
			break;
		case WM_DROPFILES:
			OnDroppedFiles((HDROP)wParam);
			break;
		case WM_DESTROY:
		{
			KillTimer(m_hwnd, CELL_EDIT_TIMER);
			KillTooltip();

			m_parentVwnd.RemoveAllChildren(false);
			m_parentVwnd.SetRealParent(NULL);

			OnDestroy();

			m_resize.init(NULL);

			for (int i=0; i < m_pLists.GetSize(); i++)
				m_pLists.Get(i)->OnDestroy();

			if (m_bSaveStateOnDestroy)
			{
				char cState[4096]=""; // SDK: "will usually be 4k or greater"
				int iLen = SaveState(cState, sizeof(cState));
				if (iLen>0)
					WritePrivateProfileStruct(SWS_INI, m_id.Get(), cState, iLen, get_ini_file());
			}
			m_bUserClosed = false;
			m_bSaveStateOnDestroy = true;

			DockWindowRemove(m_hwnd); // Always safe to call even if the window isn't docked
		}
#ifdef _WIN32
			break;
		case WM_NCDESTROY:
#endif
			m_pLists.Empty(true);
			m_hwnd = NULL;
			RefreshToolbar(0);
			break;
		case WM_PAINT:
			if (!OnPaint() && m_parentVwnd.GetNumChildren())
			{
				int xo, yo; RECT r;
				GetClientRect(m_hwnd,&r);
				m_parentVwnd.SetPosition(&r);
				m_vwnd_painter.PaintBegin(m_hwnd, WDL_STYLE_GetSysColor(COLOR_WINDOW));
				if (LICE_IBitmap* bm = m_vwnd_painter.GetBuffer(&xo, &yo))
				{
					bm->resize(r.right-r.left,r.bottom-r.top);

					int x=0;
					while (WDL_VWnd* w = m_parentVwnd.EnumChildren(x++))
						w->SetVisible(false); // just a setter, no redraw

					int h=0;
					DrawControls(bm, &r, &h);
					m_vwnd_painter.PaintVirtWnd(&m_parentVwnd);

					if (*m_tooltip)
					{
						if (!(*ConfigVar<int>("tooltips")&2)) // obeys the "Tooltip for UI elements" pref
						{
							POINT p = { m_tooltip_pt.x + xo, m_tooltip_pt.y + yo };
							RECT rr = { r.left+xo,r.top+yo,r.right+xo,r.bottom+yo };
							if (h>0) rr.bottom = h+yo; //JFB make sure some tooltips are not hidden (could be better but it's enough ATM..)
							DrawTooltipForPoint(bm,p,&rr,m_tooltip);
						}
						Help_Set(m_tooltip, true);
					}
				}
				m_vwnd_painter.PaintEnd();
			}
			break;
		case WM_LBUTTONDBLCLK:
			if (!OnMouseDblClick(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)) &&
				!m_parentVwnd.OnMouseDblClick(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)) &&
				m_parentVwnd.OnMouseDown(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)) > 0)
			{
				m_parentVwnd.OnMouseUp(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
			}
			break;
		case WM_LBUTTONDOWN:
			KillTooltip(true);
			SetFocus(m_hwnd);
			if (OnMouseDown(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)) > 0 ||
				m_parentVwnd.OnMouseDown(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)) > 0)
			{
				SetCapture(m_hwnd);
			}
			break;
		case WM_LBUTTONUP:
			if (GetCapture() == m_hwnd)
			{
				if (!OnMouseUp(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)))
				{
					m_parentVwnd.OnMouseUp(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
					for (int i=0; i < m_pLists.GetSize(); i++)
						m_pLists.Get(i)->OnEndDrag();
				}
				ReleaseCapture();
			}
			KillTooltip(true);
			break;
		case WM_MOUSEMOVE:
			m_parentVwnd.OnMouseMove(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)); // no capture test (hover, etc..)
			if (GetCapture() == m_hwnd)
			{
				if (!OnMouseMove(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)))
				{
					POINT pt = {GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};
					for (int i=0; i < m_pLists.GetSize(); i++)
					{
						RECT r;	GetWindowRect(m_pLists.Get(i)->GetHWND(), &r);
						ScreenToClient(m_hwnd, (LPPOINT)&r);
						ScreenToClient(m_hwnd, ((LPPOINT)&r)+1);
						if (PtInRect(&r,pt)) {
							m_pLists.Get(i)->OnDrag();
							break;
						}
					}
				}
			}
			else
			{
				KillTooltip(true);
				SetTimer(m_hwnd, TOOLTIP_TIMER, TOOLTIP_TIMEOUT, NULL);
#ifdef _WIN32
				// request WM_MOUSELEAVE message
				TRACKMOUSEEVENT e = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hwnd, HOVER_DEFAULT };
				TrackMouseEvent(&e);
#endif
			}
			break;
#ifdef _WIN32
		// fixes possible stuck tooltips and VWnds stuck on hover state
		case WM_MOUSELEAVE:
			KillTooltip(true);
			m_parentVwnd.OnMouseMove(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam));
			break;
#endif
		case WM_CTLCOLOREDIT:
			if (SWS_THEMING)
			{
				// color override for list views' cell edition
				HWND hwnd = (HWND)lParam;
				for (int i=0; i<m_pLists.GetSize(); i++)
					if (SWS_ListView* lv = m_pLists.Get(i))
						if (hwnd == lv->GetEditHWND()) {
							uMsg = WM_CTLCOLORLISTBOX;
							break;
						}
				return SendMessage(GetMainHwnd(),uMsg,wParam,lParam);
			}
			return 0;
		default:
			return OnUnhandledMsg(uMsg, wParam, lParam);
	}
	return 0;
}

void SWS_DockWnd::ToggleDocking()
{
	if (!IsDocked())
		GetWindowRect(m_hwnd, &m_state.r);

	m_bSaveStateOnDestroy = false;
	DestroyWindow(m_hwnd);

	m_state.state ^= 2;
	Show(false, true);
}

// screenset support
LRESULT SWS_DockWnd::screensetCallback(int action, const char *id, void *param, void *actionParm, int actionParmSize)
{
	if (SWS_DockWnd* pObj = (SWS_DockWnd*)param)
	{
		switch(action)
		{
			case SCREENSET_ACTION_GETHWND:
				return (LRESULT)pObj->m_hwnd;
			case SCREENSET_ACTION_IS_DOCKED:
				return (LRESULT)pObj->IsDocked();
			case SCREENSET_ACTION_SWITCH_DOCK:
				if (SWS_IsWindow(pObj->m_hwnd))
					pObj->ToggleDocking();
				break;
			case SCREENSET_ACTION_LOAD_STATE:
				pObj->LoadState((char*)actionParm, actionParmSize);
				break;
			case SCREENSET_ACTION_SAVE_STATE:
				return pObj->SaveState((char*)actionParm, actionParmSize);
		}
	}
	return 0;
}

int SWS_DockWnd::keyHandler(MSG* msg, accelerator_register_t* ctx)
{
	SWS_DockWnd* p = (SWS_DockWnd*)ctx->user;
	if (p && p->IsActive(true))
	{
		// First check for editing keys
		SWS_ListView* pLV = NULL;
		for (int i = 0; i < p->m_pLists.GetSize(); i++)
		{
			pLV = p->m_pLists.Get(i);
			if (pLV->IsActive(true))
			{
				int iRet = pLV->EditingKeyHandler(msg);
				if (iRet)
					return iRet;
				break;
			}
		}

		// Check the derived class key handler next in case they want to override anything
		int iKeys = SWS_GetModifiers();
		int iRet = p->OnKey(msg, iKeys);
		if (iRet)
			return iRet;

		// Key wasn't handled by the DockWnd, check for keys to send to LV
		if (pLV)
		{
			iRet = pLV->LVKeyHandler(msg, iKeys);
			if (iRet)
				return iRet;
		}
		// We don't want the key, but this window has the focus.
		// Therefore, force it to main reaper wnd (passthrough) so that main wnd actions work!
		return -666;
	}
	return 0;
}

// *Local* function to save the state of the view: window size, position & dock state
int SWS_DockWnd::SaveState(char* cStateBuf, int iMaxLen)
{
	if (SWS_IsWindow(m_hwnd))
	{
		int dockIdx = DockIsChildOfDock(m_hwnd, NULL);
		if (dockIdx>=0)
			m_state.whichdock = dockIdx;
		else
			GetWindowRect(m_hwnd, &m_state.r);
	}

	if (!m_bUserClosed && SWS_IsWindow(m_hwnd))
		m_state.state |= 1;
	else
		m_state.state &= ~1;

	int iLen = sizeof(SWS_DockWnd_State);
	if (cStateBuf)
	{
		memcpy(cStateBuf, &m_state, iLen);
		for (int i = 0; i < iLen / (int)sizeof(int); i++)
			REAPER_MAKELEINTMEM(&(((int*)cStateBuf)[i]));
	}
	return iLen;
}

// *Local* function to restore view state: window size, position & dock state.
// if both _stateBuf and _len are NULL, hide (see SDK)
void SWS_DockWnd::LoadState(const char* cStateBuf, int iLen)
{
	bool bDocked = IsDocked();

	if (cStateBuf && iLen>=sizeof(SWS_DockWnd_State))
		SWS_SetDockWndState(cStateBuf, iLen, &m_state);
	else
		m_state.state &= ~1;

	Dock_UpdateDockID((char*)m_id.Get(), m_state.whichdock);

	if (m_state.state & 1)
	{
		if (SWS_IsWindow(m_hwnd) &&
			((bDocked != ((m_state.state & 2) == 2)) ||
			(bDocked && DockIsChildOfDock(m_hwnd, NULL) != m_state.whichdock)))
		{
			// If the window's already open, but the dock state or docker # has changed,
			// destroy and reopen.
			m_bSaveStateOnDestroy = false;
			DestroyWindow(m_hwnd);
		}
		Show(false, false);

		RECT r;
		GetWindowRect(m_hwnd, &r);

		// TRP 4/29/13 - Also set wnd pos/size if it's changed.
		if (!bDocked && memcmp(&r, &m_state.r, sizeof(RECT)) != 0)
			SetWindowPos(m_hwnd, NULL, m_state.r.left, m_state.r.top, m_state.r.right-m_state.r.left, m_state.r.bottom-m_state.r.top, SWP_NOZORDER);
	}
	else if (SWS_IsWindow(m_hwnd))
	{
		m_bUserClosed = true;
		DestroyWindow(m_hwnd);
	}
}

void SWS_DockWnd::KillTooltip(bool doRefresh)
{
	KillTimer(m_hwnd, TOOLTIP_TIMER);
	bool had=!!m_tooltip[0];
	*m_tooltip='\0';
	if (had && doRefresh)
		InvalidateRect(m_hwnd,NULL,FALSE);
}

// it's up to the caller to unalloc the returned value
char* SWS_LoadDockWndStateBuf(const char* _id, int _len)
{
	if (_len<=0) _len = sizeof(SWS_DockWnd_State);
	char* state = new char[_len];
	memset(state, 0, _len);
	GetPrivateProfileStruct(SWS_INI, _id, state, _len, get_ini_file());
	return state;
}

// if _stateBuf==NULL, read state from ini file
int SWS_GetDockWndState(const char* _id, const char* _stateBuf)
{
	SWS_DockWnd_State state;
	memset(&state, 0, sizeof(SWS_DockWnd_State));
	if (_stateBuf)
	{
		SWS_SetDockWndState(_stateBuf, sizeof(SWS_DockWnd_State), &state);
	}
	else
	{
		char* stateBuf = SWS_LoadDockWndStateBuf(_id);
		SWS_SetDockWndState(stateBuf, sizeof(SWS_DockWnd_State), &state);
		delete [] stateBuf;
	}
	return state.state;
}

void SWS_SetDockWndState(const char* _stateBuf, int _len, SWS_DockWnd_State* _state)
{
	if (_state && _stateBuf && _len>=sizeof(SWS_DockWnd_State))
		for (int i=0; i < _len / (int)sizeof(int); i++)
			((int*)_state)[i] = REAPER_MAKELEINT(*((int*)_stateBuf+i));
}


///////////////////////////////////////////////////////////////////////////////
// SWS_ListView
///////////////////////////////////////////////////////////////////////////////

SWS_ListView::SWS_ListView(HWND hwndList, HWND hwndEdit, int iCols, SWS_LVColumn* pCols, const char* cINIKey, bool bTooltips, const char* cLocalizeSection, bool bDrawArrow)
:m_hwndList(hwndList), m_hwndEdit(hwndEdit), m_hwndTooltip(NULL), m_iSortCol(1), m_iEditingItem(-1), m_iEditingCol(-1),
  m_iCols(iCols), m_pCols(NULL), m_pDefaultCols(NULL), m_bDisableUpdates(false), m_cINIKey(cINIKey), m_cLocalizeSection(cLocalizeSection),m_bDrawArrow(bDrawArrow),
#ifndef _WIN32
  m_pClickedItem(NULL)
#else
  m_dwSavedSelTime(0),m_bShiftSel(false)
#endif
{
	memset(m_oldColors,0,sizeof(m_oldColors));
	SetWindowLongPtr(hwndList, GWLP_USERDATA, (LONG_PTR)this);
	if (m_hwndEdit)
		SetWindowLongPtr(m_hwndEdit, GWLP_USERDATA, 0xdeadf00b);

	// Load sort and column data
	m_pCols = new SWS_LVColumn[m_iCols];
	m_pDefaultCols = new SWS_LVColumn[m_iCols];
	if (m_cLocalizeSection)
	{
		for (int i=0; i<m_iCols; i++)
		{
			m_pCols[i].iWidth = m_pDefaultCols[i].iWidth = pCols[i].iWidth;
			m_pCols[i].iType = m_pDefaultCols[i].iType = pCols[i].iType;
			m_pCols[i].iPos = m_pDefaultCols[i].iPos = pCols[i].iPos;
			m_pCols[i].cLabel = m_pDefaultCols[i].cLabel = __localizeFunc(pCols[i].cLabel, m_cLocalizeSection, 0);
		}
	}
	else
	{
		memcpy(m_pDefaultCols, pCols, sizeof(SWS_LVColumn) * m_iCols);
		memcpy(m_pCols, m_pDefaultCols, sizeof(SWS_LVColumn) * m_iCols);
	}

	char cDefaults[256];
	sprintf(cDefaults, "%d", m_iSortCol);
	int iPos = 0;
	for (int i = 0; i < m_iCols; i++)
		snprintf(cDefaults + strlen(cDefaults), 64-strlen(cDefaults), " %d %d", m_pCols[i].iWidth, m_pCols[i].iPos != -1 ? iPos++ : -1);
	char str[256];
	GetPrivateProfileString(SWS_INI, m_cINIKey, cDefaults, str, 256, get_ini_file());
	LineParser lp(false);

	if (!lp.parse(str))
	{
		int storedColCount = (lp.getnumtokens() - 1) / 2;
		if (storedColCount == m_iCols || !lp.parse(cDefaults))
		{
			m_iSortCol = lp.gettoken_int(0);
			iPos = 0;
			for (int i = 0; i < m_iCols; i++)
			{
				int iWidth = lp.gettoken_int(i*2+1);
				if (iWidth)
				{
					m_pCols[i].iWidth = lp.gettoken_int(i*2+1);
					m_pCols[i].iPos = lp.gettoken_int(i*2+2);
					iPos = m_pCols[i].iPos;
				}
				else if (m_pCols[i].iPos != -1) // new cols are invisible?
				{
					m_pCols[i].iPos = iPos++;
				}
			}
		}
	}

	int lvstyle=LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP;
#ifdef _WIN32
	lvstyle |= LVS_EX_DOUBLEBUFFER;
#else
	// note for osx: style can be overrided in SWS_DockWnd::WndProc() for optional grid line theming
#endif
	ListView_SetExtendedListViewStyleEx(hwndList, lvstyle, lvstyle);

#ifdef _WIN32
	// Setup UTF-8 (see http://forum.cockos.com/showthread.php?t=101547)
	WDL_UTF8_HookListView(hwndList);
#if !defined(WDL_NO_SUPPORT_UTF8)
	#ifdef WDL_SUPPORT_WIN9X
	if (GetVersion()<0x80000000)
	#endif
		SendMessage(hwndList,LVM_SETUNICODEFORMAT,1,0);
#endif

	// Create the tooltip window (if it's necessary)
	if (bTooltips)
	{
		m_hwndTooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, m_hwndList, NULL, g_hInst, NULL );
		SetWindowPos(m_hwndTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}
#endif

	ShowColumns();

	// Need to call Update(); when ready elsewhere
}

SWS_ListView::~SWS_ListView()
{
	delete [] m_pCols;
}

SWS_ListItem* SWS_ListView::GetListItem(int index, int* iState)
{
	if (index < 0)
		return NULL;
	LVITEM li;
	li.mask = LVIF_PARAM | (iState ? LVIF_STATE : 0);
	li.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
	li.iItem = index;
	li.iSubItem = 0;
	ListView_GetItem(m_hwndList, &li);
	if (iState)
		*iState = li.state;
	return (SWS_ListItem*)li.lParam;
}

bool SWS_ListView::IsSelected(int index)
{
	if (index < 0)
		return false;
	return ListView_GetItemState(m_hwndList, index, LVIS_SELECTED) ? true : false;
}

SWS_ListItem* SWS_ListView::EnumSelected(int* i, int iOffset)
{
	if (!m_hwndList)
		return NULL;

	int temp = 0;
	if (!i)
		i = &temp;
	LVITEM li;
	li.mask = LVIF_PARAM | LVIF_STATE;
	li.stateMask = LVIS_SELECTED;
	li.iSubItem = 0;

	while (*i < ListView_GetItemCount(m_hwndList))
	{
		li.iItem = (*i)++;
		ListView_GetItem(m_hwndList, &li);
		if (li.state)
		{
			if ((iOffset != 0) && (((*i - 1) + iOffset) >= 0) && (((*i - 1) + iOffset) < ListView_GetItemCount(m_hwndList)))  //sanitizing
			{
				li.iItem += iOffset;  //this allows the selection of another item besides the one clicked.
				ListView_GetItem(m_hwndList, &li);
			}
			return (SWS_ListItem*)li.lParam;
		}
	}
	return NULL;
}

int SWS_ListView::CountSelected ()
{
	int selCount = 0;
	int x = 0;
	while (this->EnumSelected(&x))
		++selCount;

	return selCount;
}

bool SWS_ListView::SelectByItem(SWS_ListItem* _item, bool bSelectOnly, bool bEnsureVisible)
{
	if (_item)
	{
		for (int i = 0; i < GetListItemCount(); i++)
		{
			SWS_ListItem* item = GetListItem(i);
			if (item == _item)
			{
				if (bSelectOnly)
					ListView_SetItemState(m_hwndList, -1, 0, LVIS_SELECTED);
				ListView_SetItemState(m_hwndList, i, LVIS_SELECTED, LVIS_SELECTED);
				if (bEnsureVisible)
					ListView_EnsureVisible(m_hwndList, i, true);
				return true;
			}
		}
	}
	return false;
}

int SWS_ListView::OnNotify(WPARAM wParam, LPARAM lParam)
{
	NMLISTVIEW* s = (NMLISTVIEW*)lParam;

#ifdef _WIN32
	if (!m_bDisableUpdates && s->hdr.code == LVN_ITEMCHANGING && s->iItem >= 0 && (s->uNewState ^ s->uOldState) & LVIS_SELECTED)
	{
		// These calls are made in big groups, save the cur state on the first call
		// so we can restore it later if necessary
		if (GetTickCount() - m_dwSavedSelTime > 20)
		{
			m_pSavedSel.Resize(ListView_GetItemCount(m_hwndList), false);
			for (int i = 0; i < m_pSavedSel.GetSize(); i++)
				m_pSavedSel.Get()[i] = ListView_GetItemState(m_hwndList, i, LVIS_SELECTED | LVIS_FOCUSED);
			m_dwSavedSelTime = GetTickCount();
			m_bShiftSel = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? true : false;
		}

		int iRet = OnItemSelChanging(GetListItem(s->iItem), s->uNewState & LVIS_SELECTED ? true : false);
		SetWindowLongPtr(GetParent(m_hwndList), DWLP_MSGRESULT, iRet);
		return iRet;
	}

	if (!m_bDisableUpdates && s->hdr.code == LVN_ITEMCHANGED && s->iItem >= 0)
#else
	//JFB no test on s->iItem for OSX: needed to detect empty selections
	if (!m_bDisableUpdates && s->hdr.code == LVN_ITEMCHANGED)
#endif
	{
		if (s->uChanged & LVIF_STATE && (s->uNewState ^ s->uOldState) & LVIS_SELECTED)
			OnItemSelChanged(GetListItem(s->iItem), s->uNewState);

#ifndef _WIN32
		// See OSX comments in NM_CLICK below
		// Send the full compliment of OnItemSelChange messges, either from the saved array or the curent state
		if (m_pSavedSel.GetSize() && m_pSavedSel.GetSize() == ListView_GetItemCount(m_hwndList))
		{	// Restore the "correct" selection
			for (int i = 0; i < m_pSavedSel.GetSize(); i++)
				ListView_SetItemState(m_hwndList, i, m_pSavedSel.Get()[i], LVIS_SELECTED | LVIS_FOCUSED);
			m_pSavedSel.Resize(0, false);
		}
		else
		{
			// Send OnItemSelChange messages for everything on the list
			for (int i = 0; i < ListView_GetItemCount(m_hwndList); i++)
			{
				int iState;
				SWS_ListItem* item = GetListItem(i, &iState);
				OnItemSelChanged(item, iState);
			}
		}

		if (m_pClickedItem)
		{
			OnItemBtnClk(m_pClickedItem, m_iClickedCol, m_iClickedKeys);
			m_pClickedItem = NULL;
		}
#endif
		return 0;
	}
	else if (s->hdr.code == NM_CLICK)
	{
		// Ignore clicks if editing (SWELL sends an extra NM_CLICK after NM_DBLCLK)
		if (m_iEditingItem != -1)
			return 0;

		int iDataCol = DisplayToDataCol(s->iSubItem);
#ifdef _WIN32
		int iKeys = ((NMITEMACTIVATE*)lParam)->uKeyFlags;
#else
		int iKeys = SWS_GetModifiers();
#endif
		// Call the std click handler
		OnItemClk(GetListItem(s->iItem), iDataCol, iKeys);

		// Then do some extra work for the item button click handler
		if (s->iItem >= 0 && (m_pCols[iDataCol].iType & 2))
		{	// Clicked on an item "button"
#ifdef _WIN32
			if ((GetTickCount() - m_dwSavedSelTime < 20 || (iKeys & LVKF_SHIFT)) && m_pSavedSel.GetSize() == ListView_GetItemCount(m_hwndList) && m_pSavedSel.Get()[s->iItem] & LVIS_SELECTED)
			{
				bool bSaveDisableUpdates = m_bDisableUpdates;
				m_bDisableUpdates = true;

				// If there's a valid saved selection, and the user clicked on a selected track, the restore that selection
				for (int i = 0; i < m_pSavedSel.GetSize(); i++)
				{
					OnItemSelChanged(GetListItem(i), m_pSavedSel.Get()[i]);
					ListView_SetItemState(m_hwndList, i, m_pSavedSel.Get()[i], LVIS_SELECTED | LVIS_FOCUSED);
				}

				m_bDisableUpdates = bSaveDisableUpdates;
			}
			else if (m_bShiftSel)
			{
				// Ignore shift if the selection changed (because of a shift key hit)
				iKeys &= ~LVKF_SHIFT;
				m_bShiftSel = false;
			}
			OnItemBtnClk(GetListItem(s->iItem), iDataCol, iKeys);
#else
			// In OSX NM_CLICK comes *before* the changed notification.
			// Cases:
			// 1 - the user clicked on a non-selected item
			//     Call OnBtnClk later, in the LVN_CHANGE handler
			// 2 - one item is selected, and the user clicked on that.
			//     Call OnBtnClk now, because no change will be sent later
			// 3 - more than one item is selected, user clicked a selected one
			//     Call OnBtnClk now.  LVN_CHANGE is called later, and change the selection
			//     back to where it should be in that handler

			int iState;
			SWS_ListItem* item = GetListItem(s->iItem, &iState);
			m_iClickedKeys = iKeys;

			// Case 1:
			if (!(iState & LVIS_SELECTED))
			{
				m_pClickedItem = item;
				m_iClickedCol = iDataCol;
				m_iClickedKeys &= (LVKF_ALT | LVKF_CONTROL); // Ignore shift later
				return 0;
			}

			if (ListView_GetSelectedCount(m_hwndList) == 1)
			{	// Case 2:
				OnItemBtnClk(item, iDataCol, m_iClickedKeys);
				m_pSavedSel.Resize(0, false); // Saved sel size of zero means to not reset
			}
			else
			{	// Case 3:
				// Save the selections to be restored later
				m_pSavedSel.Resize(ListView_GetItemCount(m_hwndList), false);
				for (int i = 0; i < m_pSavedSel.GetSize(); i++)
					m_pSavedSel.Get()[i] = iState;
				OnItemBtnClk(item, iDataCol, m_iClickedKeys);
			}
#endif
		}
	}
	else if (s->hdr.code == NM_DBLCLK && s->iItem >= 0)
	{
		int iDataCol = DisplayToDataCol(s->iSubItem);
		if (iDataCol >= 0 && iDataCol < m_iCols && (m_pCols[iDataCol].iType & 1) &&
			IsEditListItemAllowed(GetListItem(s->iItem), iDataCol))
		{
			EditListItem(s->iItem, iDataCol);
		}
		else
			OnItemDblClk(GetListItem(s->iItem), iDataCol);
	}
	else if (s->hdr.code == LVN_COLUMNCLICK)
	{
		int iDataCol = DisplayToDataCol(s->iSubItem);
		if (iDataCol+1 == abs(m_iSortCol))
			m_iSortCol = -m_iSortCol;
		else
			m_iSortCol = iDataCol + 1;
		Sort();
	}
	else if (s->hdr.code == LVN_BEGINDRAG)
	{
		EditListItemEnd(true);
		OnBeginDrag(GetListItem(s->iItem));
	}
	return 0;
}

void SWS_ListView::OnDestroy()
{
	// For safety
	EditListItemEnd(false);

	// Save cols
	int cols[20];
	int iCols = 0;
	for (int i = 0; i < m_iCols; i++)
		if (m_pCols[i].iPos != -1)
			iCols++;

#ifdef _WIN32
	ListView_GetColumnOrderArray(m_hwndList, iCols, &cols);
#else
	ListView_GetColumnOrderArray(m_hwndList, iCols, cols);
#endif

	iCols = 0;

	for (int i = 0; i < m_iCols; i++)
		if (m_pCols[i].iPos != -1)
			m_pCols[i].iPos = cols[iCols++];

	char str[256];
	sprintf(str, "%d", m_iSortCol);

	iCols = 0;
	for (int i = 0; i < m_iCols; i++)
		if (m_pCols[i].iPos >= 0)
			snprintf(str + strlen(str), 256-strlen(str), " %d %d", ListView_GetColumnWidth(m_hwndList, iCols++), m_pCols[i].iPos);
		else
			snprintf(str + strlen(str), 256-strlen(str), " %d %d", m_pCols[i].iWidth, m_pCols[i].iPos);

	WritePrivateProfileString(SWS_INI, m_cINIKey, str, get_ini_file());

	if (m_hwndTooltip)
	{
		DestroyWindow(m_hwndTooltip);
		m_hwndTooltip = NULL;
	}
}

int SWS_ListView::EditingKeyHandler(MSG *msg)
{
	if (msg->message == WM_KEYDOWN && m_iEditingItem != -1)
	{
		bool bShift = GetAsyncKeyState(VK_SHIFT)   & 0x8000 ? true : false;

		if (msg->wParam == VK_ESCAPE)
		{
			EditListItemEnd(false);
			return 1;
		}
		else if (msg->wParam == VK_TAB)
		{
			int iItem = m_iEditingItem;
			DisableUpdates(true);
			EditListItemEnd(true, false);
			if (!bShift)
			{
				if (++iItem >= ListView_GetItemCount(m_hwndList))
					iItem = 0;
			}
			else
			{
				if (--iItem < 0)
					iItem = ListView_GetItemCount(m_hwndList) - 1;
			}
			EditListItem(GetListItem(iItem), m_iEditingCol);
			DisableUpdates(false);
			return 1;
		}
		else if (msg->wParam == VK_RETURN)
		{
			EditListItemEnd(true);
			return 1;
		}

		// All other keystrokes when editing should go to the control, not to SWS_DockWnd or Reaper
		// Fixes bug where the delete key when editing would delete the line, but it would still exist temporarily
		return -1;
	}
	return 0;
}

int SWS_ListView::LVKeyHandler(MSG* msg, int iKeyState)
{
	// Catch a few keys that are handled natively by the list view
	if (msg->message == WM_KEYDOWN)
	{
		if (msg->wParam == VK_UP || msg->wParam == VK_DOWN || msg->wParam == VK_TAB ||
			msg->wParam == VK_HOME || msg->wParam == VK_END || msg->wParam == VK_PRIOR || msg->wParam == VK_NEXT)
		{
			return -1;
		}
		else if (msg->wParam == 'A' && iKeyState == LVKF_CONTROL && !(GetWindowLongPtr(m_hwndList, GWL_STYLE) & LVS_SINGLESEL))
		{
			for (int i = 0; i < ListView_GetItemCount(m_hwndList); i++)
				ListView_SetItemState(m_hwndList, i, LVIS_SELECTED, LVIS_SELECTED);
			return 1;
		}
	}
	return 0;
}

void SWS_ListView::Update()
{
	// Fill in the data by pulling it from the derived class
	if (m_iEditingItem == -1 && !m_bDisableUpdates)
	{
		m_bDisableUpdates = true;
		SendMessage(m_hwndList, WM_SETREDRAW, 0, 0);

		char str[CELL_MAX_LEN]="";

		bool bResort = false;
		static int iLastSortCol = -999;
		if (m_iSortCol != iLastSortCol)
		{
			iLastSortCol = m_iSortCol;
			bResort = true;
		}

		SWS_ListItemList items;
		GetItemList(&items);

		if (!items.GetSize())
			ListView_DeleteAllItems(m_hwndList);

		// The list is sorted, use that to our advantage here:
		int lvItemCount = ListView_GetItemCount(m_hwndList);
		int newIndex = lvItemCount;
		for (int i = 0; items.GetSize() || i < lvItemCount; i++)
		{
			bool bFound = false;
			SWS_ListItem* pItem;
			if (i < lvItemCount)
			{	// First check items in the listview, match to item list
				pItem = GetListItem(i);
				int iIndex = items.Find(pItem);
				if (iIndex == -1)
				{
					// Delete items from listview that aren't in the item list
					ListView_DeleteItem(m_hwndList, i);
					i--;
					lvItemCount--;
					newIndex--;
					continue;
				}
				else
				{
					// Delete item from item list to indicate "used"
					items.Delete(iIndex);
					bFound = true;
				}
			}
			else
			{	// Items left in the item list are new
				pItem = items.Remove();
			}

			// We have an item pointer, and a listview index, add/edit the listview
			// Update the list, no matter what, because text may have changed
			LVITEM item;
			item.mask = 0;
			int iNewState = GetItemState(pItem);
			if (iNewState >= 0)
			{
				int iCurState = bFound ? ListView_GetItemState(m_hwndList, i, LVIS_SELECTED | LVIS_FOCUSED) : 0;
				if (iNewState && !(iCurState & LVIS_SELECTED))
				{
					item.mask |= LVIF_STATE;
					item.state = LVIS_SELECTED;
					item.stateMask = LVIS_SELECTED;
				}
				else if (!iNewState && (iCurState & LVIS_SELECTED))
				{
					item.mask |= LVIF_STATE;
					item.state = 0;
					item.stateMask = LVIS_SELECTED | ((iCurState & LVIS_FOCUSED) ? LVIS_FOCUSED : 0);
				}
			}

			item.iItem = bFound ? i : newIndex++;
			item.pszText = str;

			int iCol = 0;
			for (int k = 0; k < m_iCols; k++)
				if (m_pCols[k].iPos != -1)
				{
					item.iSubItem = iCol;
					GetItemText(pItem, k, str, sizeof(str));
					if (!bFound)
					{
						item.mask |= LVIF_TEXT;
						if(iCol == 0) {
							item.mask |= LVIF_PARAM | LVIF_TEXT;
							item.lParam = reinterpret_cast<LPARAM>(pItem);
							ListView_InsertItem(m_hwndList, &item);
						}
						else
							ListView_SetItem(m_hwndList, &item);
						bResort = true;
					}
					else
					{
						char curStr[CELL_MAX_LEN]="";
						ListView_GetItemText(m_hwndList, item.iItem, iCol, curStr, sizeof(curStr));
						if (strcmp(str, curStr))
							item.mask |= LVIF_TEXT;
						if (item.mask)
						{
							// Only set if there's changes
							// May be less efficient here, but less messages get sent for sure!
							ListView_SetItem(m_hwndList, &item);
							bResort = true;
						}
					}
					item.mask = 0;
					iCol++;
				}
		}

		if (bResort)
			Sort();

#ifdef _WIN32
		if (m_hwndTooltip)
		{
			TOOLINFO ti = { sizeof(TOOLINFO), };
			ti.lpszText = str;
			ti.hwnd = m_hwndList;
			ti.uFlags = TTF_SUBCLASS;
			ti.hinst  = g_hInst;

			// Delete all existing tools
			while (SendMessage(m_hwndTooltip, TTM_ENUMTOOLS, 0, (LPARAM)&ti))
				SendMessage(m_hwndTooltip, TTM_DELTOOL, 0, (LPARAM)&ti);

			RECT r;
			// Add tooltips after sort
			for (int i = 0; i < ListView_GetItemCount(m_hwndList); i++)
			{
				// Get the rect of the line
				ListView_GetItemRect(m_hwndList, i, &r, LVIR_BOUNDS);
				memcpy(&ti.rect, &r, sizeof(RECT));
				ti.uId = i;
				GetItemTooltip(GetListItem(i), str, sizeof(str));
				SendMessage(m_hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
			}
		}
#endif

		SendMessage(m_hwndList, WM_SETREDRAW, 1, 0);
#ifdef _WIN32
		RedrawWindow(m_hwndList, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
#endif
		m_bDisableUpdates = false;
	}
}

// Return TRUE if a the column header was clicked
bool SWS_ListView::DoColumnMenu(int x, int y)
{
	int iCol;
	SWS_ListItem* item = GetHitItem(x, y, &iCol);
	if (!item && iCol != -1 && IsWindowVisible(m_hwndList))
	{
		EditListItemEnd(true); // fix possible crash

		HMENU hMenu = CreatePopupMenu();
		AddToMenuOrdered(hMenu, __LOCALIZE("Visible columns","sws_menu"), 0);
		EnableMenuItem(hMenu, 0, MF_BYPOSITION | MF_GRAYED);

		for (int i = 0; i < m_iCols; i++)
		{
			AddToMenuOrdered(hMenu, m_pCols[i].cLabel, i + 1);
			if (m_pCols[i].iPos != -1)
				CheckMenuItem(hMenu, i+1, MF_BYPOSITION | MF_CHECKED);
		}
		AddToMenuOrdered(hMenu, SWS_SEPARATOR, 0);
		AddToMenuOrdered(hMenu, __LOCALIZE("Reset","sws_menu"), m_iCols + 1);

		iCol = TrackPopupMenu(hMenu, TPM_RETURNCMD, x, y, 0, m_hwndList, NULL);
		DestroyMenu(hMenu);

		if (iCol)
		{
			iCol--;
			if (iCol == m_iCols)
			{
				memcpy(m_pCols, m_pDefaultCols, sizeof(SWS_LVColumn) * m_iCols);
				int iPos = 0;
				for (int i = 0; i < m_iCols; i++)
					if (m_pCols[i].iPos != -1)
						m_pCols[i].iPos = iPos++;
			}
			else
			{
				// Save all visible column widths
				for (int i = 0; i < m_iCols; i++)
					if (m_pCols[i].iPos != -1)
						m_pCols[i].iWidth = ListView_GetColumnWidth(m_hwndList, DataToDisplayCol(i));

				if (m_pCols[iCol].iPos == -1)
				{	// Adding column
					for (int i = 0; i < m_iCols; i++)
						if (m_pCols[i].iPos >= iCol)
							m_pCols[i].iPos++;
					m_pCols[iCol].iPos = iCol;
				}
				else
				{	// Deleting column
					int iDelPos = m_pCols[iCol].iPos;
					m_pCols[iCol].iPos = -1;
					for (int i = 0; i < m_iCols; i++)
						if (m_pCols[i].iPos > iDelPos)
							m_pCols[i].iPos--;

					// Fix the sort column
					if (abs(m_iSortCol) - 1 == iCol)
						for (int i = 0; i < m_iCols; i++)
							if (m_pCols[i].iPos != -1)
							{
								m_iSortCol = i+1;
								break;
								// Possible to break out leaving the sort column pointing to
								// an invisible col if there's no columns shown!
							}
				}
			}

			ListView_DeleteAllItems(m_hwndList);
			while(ListView_DeleteColumn(m_hwndList, 0));
			ShowColumns();
			Update();
		}
		return true;
	}
	return false;
}

bool SWS_ListView::HeaderHitTest(const POINT &point) const
{
	// extracted from ReaPack
#ifdef _WIN32
	RECT rect;
	GetWindowRect(ListView_GetHeader(m_hwndList), &rect);

	const int headerHeight = rect.bottom - rect.top;
#elif !defined(__APPLE__)
	const int headerHeight = SWELL_GetListViewHeaderHeight(m_hwndList);
#endif

#ifdef __APPLE__
	// This was broken on Linux and used a hard-coded header height on Windows
	// Fixed in REAPER v6.03
	return ListView_HeaderHitTest(m_hwndList, point);
#else
	return point.y >= 0 && point.y <= headerHeight;
#endif
}

SWS_ListItem* SWS_ListView::GetHitItem(int x, int y, int* iCol)
{
	LVHITTESTINFO ht;
	ht.pt = { x, y };
	ht.flags = LVHT_ONITEM;
	ScreenToClient(m_hwndList, &ht.pt);
	int iItem = ListView_SubItemHitTest(m_hwndList, &ht);

	if (HeaderHitTest(ht.pt))
	{
		if (iCol)
			*iCol = ht.iSubItem != -1 ? DisplayToDataCol(ht.iSubItem) : 0; // iCol != -1 means "header", set 0 for "unknown column"
		return NULL;
	}
	else if (iItem >= 0
#ifdef _WIN32 //JFB added: other "no mans land" but ListView_IsItemVisible() is not part of SWELL!
		&& ListView_IsItemVisible(m_hwndList, iItem)
#endif
		)
	{
		if (iCol)
			*iCol = DisplayToDataCol(ht.iSubItem);
		return GetListItem(iItem);
	}
	if (iCol)
		*iCol = -1; // -1 + NULL ret means "no mans land"
	return NULL;
}

void SWS_ListView::EditListItem(SWS_ListItem* item, int iCol)
{
	// Convert to index and call edit
#ifdef _WIN32
	LVFINDINFO fi;
	fi.flags = LVFI_PARAM;
	fi.lParam = (LPARAM)item;
	int iItem = ListView_FindItem(m_hwndList, -1, &fi);
#else
	int iItem = -1;
	LVITEM li;
	li.mask = LVIF_PARAM;
	for (int i = 0; i < ListView_GetItemCount(m_hwndList); i++)
	{
		li.iItem = i;
		ListView_GetItem(m_hwndList, &li);
		if ((SWS_ListItem*)li.lParam == item)
		{
			iItem = i;
			break;
		}
	}
#endif
	if (iItem >= 0)
		EditListItem(iItem, iCol);
}

void SWS_ListView::EditListItem(int iIndex, int iCol)
{
	RECT r;
	m_iEditingItem = iIndex;
	m_iEditingCol = iCol;
	int iDispCol = DataToDisplayCol(iCol);
	ListView_GetSubItemRect(m_hwndList, iIndex, iDispCol, LVIR_LABEL, &r);

	RECT sr = r;
	ClientToScreen(m_hwndList, (LPPOINT)&sr);
	ClientToScreen(m_hwndList, ((LPPOINT)&sr)+1);

	// clamp to list view width
	GetWindowRect(m_hwndList, &r);
#ifdef _WIN32
	sr.left = max(r.left+(GetSystemMetrics(SM_CXEDGE)*2), sr.left);
	sr.right = min(r.right-(GetSystemMetrics(SM_CXEDGE)*2), sr.right);
	// make sure left/top grid lines are visible
	sr.left += 1;
	sr.top += 1;
#else
	sr.left = max(r.left, sr.left);
	sr.right = min(r.right, sr.right);
#endif

	HWND hDlg = GetParent(m_hwndEdit);
	ScreenToClient(hDlg, (LPPOINT)&sr);
	ScreenToClient(hDlg, ((LPPOINT)&sr)+1);

	// Create a new edit control to go over that rect
	int lOffset = -1;
	SetWindowPos(m_hwndEdit, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetWindowPos(m_hwndEdit, HWND_TOP, sr.left+lOffset, sr.top, sr.right-sr.left, sr.bottom-sr.top, SWP_SHOWWINDOW | SWP_NOZORDER);
	ShowWindow(m_hwndEdit, SW_SHOW);

	SWS_ListItem* item = GetListItem(iIndex);
	char str[CELL_MAX_LEN]="";
	GetItemText(item, iCol, str, sizeof(str));
	SetWindowText(m_hwndEdit, str);
	SetFocus(m_hwndEdit);
	SendMessage(m_hwndEdit, EM_SETSEL, 0, -1);
	SetTimer(GetParent(m_hwndList), CELL_EDIT_TIMER, CELL_EDIT_TIMEOUT, NULL);
}

bool SWS_ListView::EditListItemEnd(bool bSave, bool bResort)
{
	bool updated = false;
	if (m_iEditingItem != -1 && SWS_IsWindow(m_hwndList) && SWS_IsWindow(m_hwndEdit))
	{
		KillTimer(GetParent(m_hwndList), CELL_EDIT_TIMER);
		if (bSave)
		{
			// reset the edited column right now, reset the edited item after the update:
			// useful to distinguish cell edition before (m_iEditingCol<0) or after (m_iEditingItem<0) the actual update
			// use-case: display "X" when editing a cell, "Y" otherwise (i.e. render "X" as "Y")
			int editedCol = m_iEditingCol;
			m_iEditingCol = -1;

			char newStr[CELL_MAX_LEN];
			char curStr[CELL_MAX_LEN];
			GetWindowText(m_hwndEdit, newStr, sizeof(newStr));
			SWS_ListItem* item = GetListItem(m_iEditingItem);
			GetItemText(item, editedCol, curStr, sizeof(curStr));
			if (strcmp(curStr, newStr))
			{
				SetItemText(item, editedCol, newStr);
				GetItemText(item, editedCol, newStr, sizeof(newStr));
				ListView_SetItemText(m_hwndList, m_iEditingItem, DataToDisplayCol(editedCol), newStr);
				updated = true;
			}
			if (bResort)
				ListView_SortItems(m_hwndList, sListCompare, (LPARAM)this);
			// TODO resort? Just call update?
			// Update is likely called when SetItemText is called too...
		}
		m_iEditingItem = -1;
		ShowWindow(m_hwndEdit, SW_HIDE);
		SetFocus(m_hwndList);
	}
	return updated;
}

int SWS_ListView::OnEditingTimer()
{
	HWND hfoc=GetFocus();
	if (m_iEditingItem == -1 || (hfoc != m_hwndEdit && !IsChild(m_hwndEdit, hfoc)))
		EditListItemEnd(true);
	return 0;
}

int SWS_ListView::OnItemSort(SWS_ListItem* item1, SWS_ListItem* item2)
{
	char str1[CELL_MAX_LEN];
	char str2[CELL_MAX_LEN];
	GetItemText(item1, abs(m_iSortCol)-1, str1, sizeof(str1));
	GetItemText(item2, abs(m_iSortCol)-1, str2, sizeof(str2));
  int cmp=WDL_strcmp_logical(str1, str2, false);
  return (m_iSortCol<0 ? -cmp : cmp);
}

void SWS_ListView::ShowColumns()
{
	LVCOLUMN col;
	col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;

	int iCol = 0;
	for (int i = 0; i < m_iCols; i++)
	{
		if (m_pCols[i].iPos >= 0)
		{
			col.pszText = (char*)m_pCols[i].cLabel;
			col.cx = m_pCols[i].iWidth;
			col.fmt = LVCFMT_LEFT;
			ListView_InsertColumn(m_hwndList, iCol, &col);
			iCol++;
		}
	}

	int cols[20];
	int iCols = 0;
	for (int i = 0; i < m_iCols; i++)
		if (m_pCols[i].iPos != -1)
			cols[iCols++] = m_pCols[i].iPos;

#ifdef _WIN32
	ListView_SetColumnOrderArray(m_hwndList, iCols, &cols);
#else
	ListView_SetColumnOrderArray(m_hwndList, iCols, cols);
#endif
}

void SWS_ListView::Sort()
{
	ListView_SortItems(m_hwndList, sListCompare, (LPARAM)this);
	int iCol = abs(m_iSortCol) - 1;
	iCol = DataToDisplayCol(iCol) + 1;
	if (m_iSortCol < 0)
		iCol = -iCol;
	SetListviewColumnArrows(iCol);
	OnItemSortEnd();
}

void SWS_ListView::SetListviewColumnArrows(int iSortCol)
{
	if (!m_bDrawArrow) return;

	HWND hhdr = ListView_GetHeader(m_hwndList);
	if (!hhdr) return;

	for (int i=0; i < Header_GetItemCount(hhdr); i++)
	{
		HDITEM hi = { HDI_FORMAT, 0, };
		Header_GetItem(hhdr, i, &hi);
		hi.fmt &= ~(HDF_SORTUP|HDF_SORTDOWN);
		if (abs(iSortCol) == i+1) hi.fmt |= (iSortCol > 0 ? HDF_SORTUP : HDF_SORTDOWN);
		Header_SetItem(hhdr, i, &hi);
	}
}

int SWS_ListView::DisplayToDataCol(int iCol)
{	// The display column # can be potentially less than the data column if there are hidden columns
	if (iCol < 0)
		return iCol;
	int iVis = 0;
	int iDataCol = iCol;
	for (int i = 0; i < m_iCols && iVis <= iCol; i++)
		if (m_pCols[i].iPos == -1)
			iDataCol++;
		else
			iVis++;
	return iDataCol;
}

int SWS_ListView::DataToDisplayCol(int iCol)
{	// The data col # can be more than the display col if there are hidden columns
	for (int i = 0; i < iCol; i++)
		if (m_pCols[i].iPos == -1)
			iCol--;
	return iCol;
}

int SWS_ListView::sListCompare(LPARAM lParam1, LPARAM lParam2, LPARAM lSortParam)
{
	if (SWS_ListView* pLV = (SWS_ListView*)lSortParam)
		return pLV->OnItemSort((SWS_ListItem*)lParam1, (SWS_ListItem*)lParam2);
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
// Code bits courtesy of Cockos. Thank you Cockos!
// Mods are plainly marked as such.
///////////////////////////////////////////////////////////////////////////////

// From http://askjf.com/index.php?q=1609s
// mod: clamp grid lines to nb of displayed rows + GUI glitch fix for SWS_ListView
//      before mod: http://stash.reaper.fm/11297/fixed_glitch.gif
//      after mod:  http://stash.reaper.fm/11298/fixed_glitch2.gif
#ifdef _WIN32
void DrawListCustomGridLines(HWND hwnd, HDC hdc, RECT br, int color, int ncol)
{
// JFB added --->
  int cnt = ListView_GetItemCount(hwnd);
  if (!cnt) return;
// <---

  int i;
  HPEN pen = CreatePen(PS_SOLID,0,color);
  HGDIOBJ oldObj = SelectObject(hdc,pen);
  for(i=0;i<ncol;i++)
  {
	RECT r;
	if (!ListView_GetSubItemRect(hwnd,0,i,LVIR_BOUNDS,&r) && i) break;
	r.left--;
	r.right--;
	if (!i)
	{
	  int h =r.bottom-r.top;
/* JFB commented: cannot be 0 here, i.e. no grid lines when the list is empty --->
	  if (!ListView_GetItemCount(hwnd))
	  {
		r.top = 0;
		HWND head=ListView_GetHeader(hwnd);
		if (head)
		{
		  GetWindowRect(head,&r);
		  r.top=r.bottom-r.top;
		}
		h=17;// todo getsystemmetrics
	  }
<--- */
	  if (h>0)
	  {
//JFB mod --->
		int row=0;
		while (r.top < br.bottom && row++ <= cnt)
//        while (r.top < br.bottom)
// <---
		{
		  if (r.top >= br.top)
		  {
			MoveToEx(hdc,br.left,r.top,NULL);
			LineTo(hdc,br.right,r.top);
//JFB? use LineTo(hdc,r.right,r.top) instead? ^^
		  }
		  r.top +=h;
		}
	  }
	}
	else if (r.right >= br.left && r.left < br.right)
	{
/*JFB commented: i==0 is impossible here
	  if (i)
*/
	  {
/*JFB commented: fix for missing grid lines (i.e. paint both right & left vertical lines in case some columns have been moved/hidden)
//               could be improved using SWS_ListView..
		if (i==1)
*/
		{
		  MoveToEx(hdc,r.left,br.top,NULL);
//JFB mod --->
		  LineTo(hdc,r.left,min(br.bottom, r.top+(r.bottom-r.top)*cnt));
//          LineTo(hdc,r.left,br.bottom);
// <---
		}
		MoveToEx(hdc,r.right,br.top,NULL);
//JFB mod --->
		LineTo(hdc,r.right,min(br.bottom, r.top+(r.bottom-r.top)*cnt));
//        LineTo(hdc,r.right,br.bottom);
// <---
	  }
	}
  }
  SelectObject(hdc,oldObj);
  DeleteObject(pen);
}
#endif

bool ListView_HookThemeColorsMessage(HWND hwndDlg, int uMsg, LPARAM lParam, int cstate[LISTVIEW_COLORHOOK_STATESIZE], int listID, int whichTheme, int wantGridForColumns)
{
//JFB added --->
  int sz;
  ColorTheme* ctheme = (ColorTheme*)GetColorThemeStruct(&sz);
  if (!ctheme || sz < sizeof(ColorTheme))
	  return false;
// <---

  // if whichTheme&1, is tree view
  switch (uMsg)
  {
	case WM_PAINT:
	  {
		int c1=RGB(255,255,255);
		int c2=RGB(0,0,0);
		int c3=RGB(224,224,224);

#ifndef _WIN32
		int selcols[4];
		selcols[0]=ctheme->genlist_sel[0];
		selcols[1]=ctheme->genlist_sel[1];
		selcols[2]=ctheme->genlist_selinactive[0];
		selcols[3]=ctheme->genlist_selinactive[1];
#endif
		if ((whichTheme&~1) == 0)
		{
		  c1 = ctheme->genlist_bg;
		  c2 = ctheme->genlist_fg;
		  c3 = ctheme->genlist_gridlines;
		}
		if (cstate[0] != c1 || cstate[1] != c2 || cstate[2] != c3
#ifndef _WIN32
			|| memcmp(selcols,cstate+3,4*sizeof(int))
#endif
			)
		{
		  cstate[0]=c1;
		  cstate[1]=c2;
		  cstate[2]=c3;
		  HWND h = GetDlgItem(hwndDlg,listID);
#ifndef _WIN32
		  memcpy(cstate+3,selcols,4*sizeof(int));
		  if (h) ListView_SetSelColors(h,selcols,4);
#endif
		  if (h)
		  {
			if (whichTheme&1)
			{
			  TreeView_SetBkColor(h,c1);
			  TreeView_SetTextColor(h,c2);
			}
			else
			{
			  ListView_SetBkColor(h,c1);
			  ListView_SetTextBkColor(h,c1);
			  ListView_SetTextColor(h,c2);
#ifndef _WIN32
			  ListView_SetGridColor(h, wantGridForColumns ? c3 : c1);
#endif
			}
		  }
		}
	  }
	break;
	case WM_CREATE:
	case WM_INITDIALOG:
	  memset(cstate, 0, LISTVIEW_COLORHOOK_STATESIZE*sizeof(cstate[0]));
	break;
#ifdef _WIN32
	case WM_NOTIFY:
	  if (lParam)
	  {
		NMHDR *hdr = (NMHDR *)lParam;
		bool wantThemedSelState=true;
		if (hdr->idFrom == listID) switch (hdr->code)
		{
		  case NM_CUSTOMDRAW:
			if (whichTheme&1)
			{
			  LPNMTVCUSTOMDRAW lptvcd = (LPNMTVCUSTOMDRAW)lParam;
			  if (wantThemedSelState) switch(lptvcd->nmcd.dwDrawStage)
			  {
				case CDDS_PREPAINT:
				  SetWindowLongPtr(hwndDlg,DWLP_MSGRESULT,CDRF_NOTIFYITEMDRAW);
				return true;
				case CDDS_ITEMPREPAINT:
				  if (wantThemedSelState&&lptvcd->nmcd.dwItemSpec)
				  {
					TVITEM tvi={TVIF_HANDLE|TVIF_STATE ,(HTREEITEM)lptvcd->nmcd.dwItemSpec};
					TreeView_GetItem(hdr->hwndFrom,&tvi);
					if(tvi.state&(TVIS_SELECTED|TVIS_DROPHILITED))
					{
					  int bg1=ctheme->genlist_sel[0];
					  int bg2=ctheme->genlist_selinactive[0];
					  int fg1=ctheme->genlist_sel[1];
					  int fg2=ctheme->genlist_selinactive[1];

					  bool active = (tvi.state&TVIS_DROPHILITED) || GetFocus()==hdr->hwndFrom;
					  lptvcd->clrText = active ? fg1 : fg2;
					  lptvcd->clrTextBk = active ? bg1 : bg2;
					  lptvcd->nmcd.uItemState &= ~CDIS_SELECTED;
					}
					SetWindowLongPtr(hwndDlg,DWLP_MSGRESULT,0);
					return true;
				  }
				break;
			  }
			}
			else if (wantGridForColumns||wantThemedSelState)
			{
			  LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;
			  switch(lplvcd->nmcd.dwDrawStage)
			  {
				case CDDS_PREPAINT:
				  SetWindowLongPtr(hwndDlg,DWLP_MSGRESULT,(wantGridForColumns?CDRF_NOTIFYPOSTPAINT:0)|
														  (wantThemedSelState?CDRF_NOTIFYITEMDRAW:0));
				return true;
				case CDDS_ITEMPREPAINT:
				  if (wantThemedSelState)
				  {
					int s = ListView_GetItemState(hdr->hwndFrom, (int)lplvcd->nmcd.dwItemSpec, LVIS_SELECTED|LVIS_FOCUSED);
					if(s&LVIS_SELECTED)
					{
					  int bg1=ctheme->genlist_sel[0];
					  int bg2=ctheme->genlist_selinactive[0];
					  int fg1=ctheme->genlist_sel[1];
					  int fg2=ctheme->genlist_selinactive[1];

					  bool active = GetFocus()==hdr->hwndFrom;
					  lplvcd->clrText = active ? fg1 : fg2;
					  lplvcd->clrTextBk = active ? bg1 : bg2;
					  lplvcd->nmcd.uItemState &= ~CDIS_SELECTED;
					}
					if (s&LVIS_FOCUSED)
					{
/*JFB commented (does not compile)
					  // todo: theme option for colors for focus state as well?
					  if (0 && GetFocus()==hdr->hwndFrom)
					  {
						lplvcd->clrText = BrightenColorSlightly(lplvcd->clrText);
						lplvcd->clrTextBk = BrightenColorSlightly(lplvcd->clrTextBk);
					  }
*/
					  lplvcd->nmcd.uItemState &= ~CDIS_FOCUS;
					}
					SetWindowLongPtr(hwndDlg,DWLP_MSGRESULT,0);
					return true;
				  }
				break;
				case CDDS_POSTPAINT:
				  if (wantGridForColumns)
				  {
					int c1 = ctheme->genlist_gridlines;
					int c2 = ctheme->genlist_bg;
					if (c1 != c2)
					{
					  DrawListCustomGridLines(hdr->hwndFrom,lplvcd->nmcd.hdc,lplvcd->nmcd.rc,c1,wantGridForColumns);
					}
				  }
				  SetWindowLongPtr(hwndDlg,DWLP_MSGRESULT,0);
				return true;
			  }
			}
		  break;
		}
	  }
#endif
  }
  return false;
}

// From Justin: "this should likely go into WDL"
void DrawTooltipForPoint(LICE_IBitmap *bm, POINT mousePt, RECT *wndr, const char *text)
{
	if (!bm || !text || !text[0])
		return;

	static LICE_CachedFont tmpfont;
	if (!tmpfont.GetHFont())
	{
//JFB mod: font size/name + optional ClearType rendering --->
/*
	  bool doOutLine = true;
	  LOGFONT lf =
	  {
		  14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,
		#ifdef _WIN32
		  "MS Shell Dlg"
		#else
		"Arial"
		#endif
	  };
	  tmpfont.SetFromHFont(CreateFontIndirect(&lf),LICE_FONT_FLAG_OWNS_HFONT);
*/
	  LOGFONT lf = {SNM_FONT_HEIGHT,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,SNM_FONT_NAME
	  };
#ifndef _SNM_SWELL_ISSUES
	 tmpfont.SetFromHFont(CreateFontIndirect(&lf),LICE_FONT_FLAG_OWNS_HFONT|(g_SNM_ClearType?LICE_FONT_FLAG_FORCE_NATIVE:0));
#else
	 tmpfont.SetFromHFont(CreateFontIndirect(&lf),LICE_FONT_FLAG_OWNS_HFONT); // SWELL issue: native font rendering won't draw multiple lines
#endif
//JFB <---
	}
	tmpfont.SetBkMode(TRANSPARENT);
	LICE_pixel col1 = LICE_RGBA(0,0,0,255);
//JFB mod: same tooltip color than REAPER --->
//    LICE_pixel col2 = LICE_RGBA(255,255,192,255);
	LICE_pixel col2 = LICE_RGBA(255,255,225,255);
// <---

	tmpfont.SetTextColor(col1);
	RECT r={0,};
	tmpfont.DrawText(bm,text,-1,&r,DT_CALCRECT);

	int xo = min(max(mousePt.x,wndr->left),wndr->right);
	int yo = min(max(mousePt.y + 24,wndr->top),wndr->bottom);

	if (yo + r.bottom > wndr->bottom-4) // too close to bottom, move up if possible
	{
	  if (mousePt.y - r.bottom - 12 >= wndr->top)
		yo = mousePt.y - r.bottom - 12;
	  else
		yo = wndr->bottom - 4 - r.bottom;

//JFB added: (try to) prevent hidden tooltip behind the mouse pointer --->
	  xo += 15;
// <---
	}

	if (xo + r.right > wndr->right - 4)
	  xo = wndr->right - 4 - r.right;

	r.left += xo;
	r.top += yo;
	r.right += xo;
	r.bottom += yo;

	int border = 3;
	LICE_FillRect(bm,r.left-border,r.top-border,r.right-r.left+border*2,r.bottom-r.top+border*2,col2,1.0f,LICE_BLIT_MODE_COPY);
	LICE_DrawRect(bm,r.left-border,r.top-border,r.right-r.left+border*2,r.bottom-r.top+border*2,col1,1.0f,LICE_BLIT_MODE_COPY);

	tmpfont.DrawText(bm,text,-1,&r,0);
}
