/******************************************************************************
/ sws_extension.cpp
/
/ Copyright (c) 2013 and later Tim Payne (SWS), Jeffos
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
#include "version.h"
#include "Console/Console.h"
#include "Freeze/Freeze.h"
#include "MarkerActions/MarkerActions.h"
#include "Snapshots/SnapshotClass.h"
#include "Snapshots/Snapshots.h"
#include "Zoom.h"
#include "Misc/Misc.h"
#include "Misc/RecCheck.h"
#include "Misc/Adam.h"
#include "Color/Color.h"
#include "Color/Autocolor.h"
#include "MarkerList/MarkerListClass.h"
#include "MarkerList/MarkerList.h"
#include "TrackList/TracklistFilter.h"
#include "TrackList/Tracklist.h"
#include "Projects/ProjectMgr.h"
#include "Projects/ProjectList.h"
#include "SnM/SnM.h"
#include "SnM/SnM_CSurf.h"
#include "SnM/SnM_Dlg.h"
#include "SnM/SnM_Util.h"
#include "Padre/padreActions.h"
#include "Fingers/FNG_client.h"
#include "Autorender/Autorender.h"
#include "IX/IX.h"
#include "Breeder/BR.h"
#include "Wol/wol.h"
#include "nofish/nofish.h"
#include "snooks/snooks.h"

#define LOCALIZE_IMPORT_PREFIX "sws_"
#include <WDL/localize/localize-import.h>
#include <WDL/localize/localize.h>

REAPER_PLUGIN_HINSTANCE g_hInst = NULL;
HWND g_hwndParent = NULL;
bool g_bInitDone = false;

#ifdef ACTION_DEBUG
void freeCmdFilesValue(WDL_String* p) {delete p;}
static WDL_IntKeyedArray<WDL_String*> g_cmdFiles(freeCmdFilesValue);
#endif
static WDL_IntKeyedArray<COMMAND_T*> g_commands; // no valdispose (cmds can be allocated in different ways)

int g_iFirstCommand = 0;
int g_iLastCommand = 0;


bool hookCommandProc(int iCmd, int flag)
{
	static WDL_PtrList<const char> sReentrantCmds;

	// for Xen extensions
	g_KeyUpUndoHandler=0;

	// "Hack" to make actions will #s less than 1000 work with SendMessage (AHK)
	// no recursion check here: handled by REAPER
	if (iCmd < 1000)
		return KBD_OnMainActionEx(iCmd, 0, 0, 0, g_hwndParent, NULL) ? true : false; // C4800

	// Special case for checking recording
	if (iCmd == 1013 && !RecordInputCheck())
		return true;

	if (BR_GlobalActionHook(iCmd, 0, 0, 0, 0))
		return true;

	// Ignore commands that don't have anything to do with us from this point forward
	if (COMMAND_T* cmd = SWSGetCommandByID(iCmd))
	{
		// For continuous actions
		if (BR_SwsActionHook(cmd, flag, NULL))
			return true;

		if (!cmd->uniqueSectionId && cmd->accel.accel.cmd==iCmd && cmd->doCommand)
		{
			if (sReentrantCmds.Find(cmd->id)<0)
			{
				sReentrantCmds.Add(cmd->id);
				cmd->fakeToggle = !cmd->fakeToggle;
#ifndef BR_DEBUG_PERFORMANCE_ACTIONS
				cmd->doCommand(cmd);
#else
				CommandTimer(cmd);
#endif
				sReentrantCmds.Delete(sReentrantCmds.Find(cmd->id));
				return true;
			}
#ifdef _SWS_DEBUG
			else
			{
				OutputDebugString("hookCommandProc - recursive action: ");
				OutputDebugString(cmd->id);
				OutputDebugString("\n");
			}
#endif
		}
	}
	return false;
}

bool hookCommandProc2(KbdSectionInfo* sec, int cmdId, int val, int valhw, int relmode, HWND hwnd)
{
	static WDL_PtrList<const char> sReentrantCmds;

	if (osara_isShortcutHelpEnabled && osara_isShortcutHelpEnabled())
		return false; // let OSARA handle the command if it was loaded after SWS
	else if (BR_GlobalActionHook(cmdId, val, valhw, relmode, hwnd))
		return true;

	// Ignore commands that don't have anything to do with us from this point forward
	if (COMMAND_T* cmd = SWSGetCommandByID(cmdId))
	{
		if (cmd->uniqueSectionId==sec->uniqueID && cmd->accel.accel.cmd==cmdId)
		{
			// job for hookCommandProc?
			// note: we could perform cmd->doCommand() here, but we'd loose the "flag" param value
			if (cmd->doCommand)
				return false;

			if (cmd->onAction)
			{
				// For continuous actions
				if (BR_SwsActionHook(cmd, relmode, hwnd))
					return true;

				if (sReentrantCmds.Find(cmd->id)<0)
				{
					sReentrantCmds.Add(cmd->id);
					cmd->fakeToggle = !cmd->fakeToggle;

#ifndef BR_DEBUG_PERFORMANCE_ACTIONS
					cmd->onAction(cmd, val, valhw, relmode, hwnd);
#else
					CommandTimer(cmd, val, valhw, relmode, hwnd, true);
#endif
					sReentrantCmds.Delete(sReentrantCmds.Find(cmd->id));
					return true;
				}
#ifdef _SWS_DEBUG
				else
				{
					OutputDebugString("hookCommandProc2 - recursive action: ");
					OutputDebugString(cmd->id);
					OutputDebugString("\n");
				}
#endif
			}
		}
	}
	return false;
}

// just a dummy example, not used ATM (commented in the entry point)
void hookPostCommandProc(int iCmd, int flag)
{
	WDL_FastString str;
	str.SetFormatted(512, "hookPostCommandProc: %s, flag=%d\r\n", kbd_getTextFromCmd(iCmd, NULL), flag);
	ShowConsoleMsg(str.Get());
}

// Returns:
// -1 = action does not belong to this extension, or does not toggle
//  0 = action belongs to this extension and is currently set to "off"
//  1 = action belongs to this extension and is currently set to "on"
int toggleActionHook(int iCmd)
{
	static WDL_PtrList<const char> sReentrantCmds;
	if (COMMAND_T* cmd = SWSGetCommandByID(iCmd))
	{
		if (cmd->accel.accel.cmd==iCmd && cmd->getEnabled)
		{
			if (sReentrantCmds.Find(cmd->id) == -1)
			{
				sReentrantCmds.Add(cmd->id);
				int state = cmd->getEnabled(cmd);
				sReentrantCmds.Delete(sReentrantCmds.Find(cmd->id));
				return state;
			}
#ifdef _SWS_DEBUG
			else
			{
				OutputDebugString("toggleActionHook - recursive action: ");
				OutputDebugString(cmd->id);
				OutputDebugString("\n");
			}
#endif
		}
	}
	return -1;
}

// 1) Get command ID from Reaper
// 2) Add keyboard accelerator (with localized action name) and add to the "action" list
int SWSRegisterCmd(COMMAND_T* pCommand, const char* cFile, int cmdId, bool localize)
{
  if (!pCommand || !pCommand->id || !pCommand->accel.desc || (!pCommand->doCommand && !pCommand->onAction)) return 0;

	// localized action name, if needed
	const char* defaultName = pCommand->accel.desc;
	if (localize) pCommand->accel.desc = GetLocalizedActionName(pCommand->accel.desc); // no alloc + no-op when no LangPack file is defined

	if (!pCommand->uniqueSectionId && pCommand->doCommand)
	{
		if (!cmdId) cmdId = plugin_register("command_id", (void*)pCommand->id);
		if (cmdId)
		{
			pCommand->accel.accel.cmd = cmdId; // need to this before registering "gaccel"
			cmdId = (plugin_register("gaccel", &pCommand->accel) ? cmdId : 0);
		}
	}
	else if (pCommand->onAction)
	{
		static custom_action_register_t s;
		memset(&s, 0, sizeof(custom_action_register_t));
		s.idStr = pCommand->id;
		s.name = pCommand->accel.desc;
		s.uniqueSectionId = pCommand->uniqueSectionId;
		cmdId = plugin_register("custom_action", (void*)&s); // will re-use the known cmd ID, if any
	}
	else
		cmdId = 0;
	pCommand->accel.accel.cmd = cmdId;

	// now that it is registered, restore the default action name
	if (pCommand->accel.desc != defaultName) pCommand->accel.desc = defaultName;

	if (!cmdId) return 0;

	if (!g_iFirstCommand || g_iFirstCommand > cmdId) g_iFirstCommand = cmdId;
	if (cmdId > g_iLastCommand) g_iLastCommand = cmdId;

	g_commands.Insert(cmdId, pCommand);
#ifdef ACTION_DEBUG
	g_cmdFiles.Insert(cmdId, new WDL_String(cFile));
#endif

	return pCommand->accel.accel.cmd;
}

// For each item in table call SWSRegisterCommand
int SWSRegisterCmds(COMMAND_T* pCommands, const char* cFile, bool localize)
{
	int i = 0;
	while(pCommands[i].id != LAST_COMMAND)
		SWSRegisterCmd(&pCommands[i++], cFile, 0, localize);
	return 1;
}

// Make and register a dynamic action (created/removed at runtime)
// If cmdId==0, get command ID from Reaper (use the provided cmdId otherwise)
// Note: SWSFreeUnregisterDynamicCmd() can be used to free/unregister such an action
int SWSCreateRegisterDynamicCmd(int uniqueSectionId, int cmdId, void(*doCommand)(COMMAND_T*), void(*onAction)(COMMAND_T*, int, int, int, HWND), int(*getEnabled)(COMMAND_T*), const char* cID, const char* cDesc, const char* cMenu, INT_PTR user, const char* cFile, bool localize)
{
	COMMAND_T* ct = new COMMAND_T;
	memset(ct, 0, sizeof(COMMAND_T));
	ct->uniqueSectionId = uniqueSectionId;
	ct->accel.desc = _strdup(cDesc);
	ct->id = _strdup(cID);
	ct->doCommand = doCommand;
	ct->onAction = onAction;
	ct->getEnabled = getEnabled;
	ct->user = user;
	ct->menuText = cMenu;
	return SWSRegisterCmd(ct, cFile, cmdId, localize);
}

bool SWSFreeUnregisterDynamicCmd(int id)
{
	if (COMMAND_T* ct = SWSUnregisterCmd(id))
	{
		free((void*)ct->accel.desc);
		free((void*)ct->id);
		DELETE_NULL(ct);
		return true;
	}
	return false;
}

// Returns the COMMAND_T entry (so it can be freed if necessary)
COMMAND_T* SWSUnregisterCmd(int id)
{
	if (COMMAND_T* ct = g_commands.Get(id, NULL))
	{
		if (!ct->uniqueSectionId && ct->doCommand)
		{
			plugin_register("-gaccel", &ct->accel);
/* this is no-op ATM
			plugin_register("-command_id", &id);
*/
		}
		else if (ct->onAction)
		{
			static custom_action_register_t s;
			s.idStr = ct->id;
			s.uniqueSectionId = ct->uniqueSectionId;
			plugin_register("-custom_action", (void*)&s);
		}
		g_commands.Delete(id);
#ifdef ACTION_DEBUG
		g_cmdFiles.Delete(id);
#endif
		return ct;
	}
	return NULL;
}

void UnregisterAllCmds() {
	for (int i=g_iFirstCommand; i<=g_iLastCommand; i++)
		SWSUnregisterCmd(i);
}

#ifdef ACTION_DEBUG
void ActionsList(COMMAND_T*)
{
	// Output sws_actions.csv
	char cBuf[512];
	strncpy(cBuf, get_ini_file(), 256);
	char* pC = strrchr(cBuf, PATH_SLASH_CHAR);
	if (pC)
	{
		strcpy(pC+1, "sws_actions.csv");
		FILE* f = fopenUTF8(cBuf, "w");
		fputs("Action,File,CmdID,CmdStr\n", f);
		if (f)
		{
			for (int i = 0; i < g_commands.GetSize(); i++)
			{
				if (COMMAND_T* cmd = g_commands.Enumerate(i, NULL, NULL))
				{
					WDL_String* pFn = g_cmdFiles.Get(cmd->accel.accel.cmd, NULL);
					snprintf(cBuf, sizeof(cBuf), "\"%s\",%s,%d,_%s\n", cmd->accel.desc, pFn ? pFn->Get() : "", cmd->accel.accel.cmd, cmd->id);
					fputs(cBuf, f);
				}
			}
			fclose(f);
		}
	}
}
#endif

//JFB questionnable func: ok most of the time but, for ex.,
// 2 different cmds can share the same function pointer cmd->doCommand
int SWSGetCommandID(void (*cmdFunc)(COMMAND_T*), INT_PTR user, const char** pMenuText)
{
	for (int i=0; i<g_commands.GetSize(); i++)
	{
		if (COMMAND_T* cmd = g_commands.Enumerate(i, NULL, NULL))
		{
			if (cmd->doCommand == cmdFunc && cmd->user == user)
			{
				if (pMenuText)
					*pMenuText = cmd->menuText;
				return cmd->accel.accel.cmd;
			}
		}
	}
	return 0;
}

COMMAND_T* SWSGetCommandByID(int cmdId) {
	if (cmdId >= g_iFirstCommand && cmdId <= g_iLastCommand) // not enough to ensure it is a SWS action
		return g_commands.Get(cmdId, NULL);
	return NULL;
}

int IsSwsAction(const char* _actionName)
{
	if (_actionName)
		if (const char* p = strstr(_actionName, ": ")) // no strchr() here: make sure p[2] is not out of bounds
			if (const char* tag = strstr(_actionName, "SWS")) // make sure it is a SWS tag
				if (tag < p) // make really sure
					return ((int)(p+2-_actionName));
	return 0;
}

HMENU SWSCreateMenuFromCommandTable(COMMAND_T pCommands[], HMENU hMenu, int* iIndex)
{
	int i = 0;
	if (iIndex)
		i = *iIndex;

	while (pCommands[i].id != LAST_COMMAND && pCommands[i].id != SWS_ENDSUBMENU)
	{
	const char* name = pCommands[i].menuText;
		if (name && *name)
		{
	  if (!hMenu)
		hMenu = CreatePopupMenu();

			if (pCommands[i].id == SWS_STARTSUBMENU)
			{
				i++;
				HMENU hSubMenu = SWSCreateMenuFromCommandTable(pCommands, NULL, &i);
				AddSubMenu(hMenu, hSubMenu, __localizeFunc(name,"sws_menu",0));
			}
			else
				AddToMenu(hMenu, __localizeFunc(name,"sws_menu",0), pCommands[i].accel.accel.cmd);
		}
		i++;
	}

	if (iIndex)
		*iIndex = i;
	return hMenu;
}

// This function creates the extension menu (flag==0) and handles checking menu items (flag==1).
// Reaper automatically checks menu items of customized menus using toggleActionHook above,
// but since we can't tell if a menu is customized we always check either way.
static void swsMenuHook(const char* menustr, HMENU hMenu, int flag)
{
	if (flag == 1)
	{
		// Handle checked menu items
		// Go through every menu item - see if it exists in the table, then check if necessary
		MENUITEMINFO mi={sizeof(MENUITEMINFO),};
		mi.fMask = MIIM_ID | MIIM_SUBMENU;
		for (int i = 0; i < GetMenuItemCount(hMenu); i++)
		{
			GetMenuItemInfo(hMenu, i, true, &mi);
			if (mi.hSubMenu)
				swsMenuHook(menustr, mi.hSubMenu, flag);
			else if (mi.wID >= (UINT)g_iFirstCommand && mi.wID <= (UINT)g_iLastCommand) {
				if (COMMAND_T* t = g_commands.Get(mi.wID, NULL))
					CheckMenuItem(hMenu, i, MF_BYPOSITION | (t->getEnabled && t->getEnabled(t) ? MF_CHECKED : MF_UNCHECKED));
			}
		}
	}
	else if (!strcmp(menustr, "Main extensions"))
		SWSCreateExtensionsMenu(hMenu);
}

static void importExtensionAPI()
{
	plugin_register("-timer", (void*)importExtensionAPI);

	// import functions exposed by third-party extensions
	osara_isShortcutHelpEnabled = (decltype(osara_isShortcutHelpEnabled))plugin_getapi("osara_isShortcutHelpEnabled");
}

// Fake control surface to get a low priority periodic time slice from Reaper
// and callbacks for some "track params have changed"
class SWSTimeSlice : public IReaperControlSurface
{
public:
	const char *GetTypeString() { return ""; }
	const char *GetDescString() { return ""; }
	const char *GetConfigString() { return ""; }

	bool m_bChanged;
	int m_iACIgnore;
	SWSTimeSlice() : m_bChanged(false), m_iACIgnore(0) {}

	void Run() // BR: Removed some stuff from here and made it use plugin_register("timer"/"-timer") - it's the same thing as this but it enables us to remove unused stuff completely
	{          // I guess we could do the rest too (and add user options to enable where needed)...
		SNM_CSurfRun();
		ZoomSlice();
		MiscSlice();

		if (m_bChanged)
		{
			m_bChanged = false;
			ScheduleTracklistUpdate();
			g_pMarkerList->Update();
			UpdateSnapshotsDialog();
			ProjectListUpdate();
		}
	}

	void SetPlayState(bool play, bool pause, bool rec)
	{
		SNM_CSurfSetPlayState(play, pause, rec);
		AWDoAutoGroup(rec);
		ItemPreviewPlayState(play, rec);
		BR_CSurf_SetPlayState(play, pause, rec);
	}

	// This is our only notification of active project tab change, so update everything
	void SetTrackListChange()
	{
		m_bChanged = true;
		AutoColorTrack(false);
		AutoColorMarkerRegion(false);
		SNM_CSurfSetTrackListChange();
		m_iACIgnore = GetNumTracks() + 1;
	}
	// For every SetTrackListChange we get NumTracks+1 SetTrackTitle calls, but we only
	// want to call AutoColorRun once, so ignore those n+1.
	// However, we still need to trap track name changes with no track list change.
	void SetTrackTitle(MediaTrack *tr, const char *c)
	{
		ScheduleTracklistUpdate();
		if (!m_iACIgnore)
		{
			AutoColorTrack(false);
			SNM_CSurfSetTrackTitle();
		}
		else
			m_iACIgnore--;
	}

	void OnTrackSelection(MediaTrack *tr) // 3 problems with this (last check v5.0pre28): doesn't work if Mixer option "Scroll view when tracks activated" is disabled
	{                                     //                                              gets called before CSurf->Extended(CSURF_EXT_SETLASTTOUCHEDTRACK)
	                                      //                                              only gets called when track is selected by clicking it in TCP/MCP (and not when track is selected via action)...bug or feature?

		// while OnTrackSelection appears to be broken, putting this in SetSurfaceSelected() would mean it gets called multiple times (because SetSurfaceSelected() gets called once for each track whose
		// selection state changed, and then once more for every track in the project). We could theoretically try and deduct when SetSurfaceSelected() got called the last time (but it's arguable if we could
		// do this with 100% accuracy because when only master track is selected, SetSurfaceSelected() gets called only once, and in case new track is inserted, it gets called 3 times for the last track (once for unselecting
		// prior to inserting new track, once for new track's selection state, and then once more when it iterates through all tracks)
		//
		// Besides these complications, it would also mean we would have to check all of these things a lot of times, thus clogging the Csurf just to execute one simple thing. So just leave it here and hope the
		// OnTrackSelection() gets fixed at some point :)
		BR_CSurf_OnTrackSelection(tr);
	}

	void SetSurfaceSelected(MediaTrack *tr, bool bSel)	{ ScheduleTracklistUpdate(); UpdateSnapshotsDialog(true); }
	void SetSurfaceMute(MediaTrack *tr, bool mute)		{ ScheduleTracklistUpdate(); UpdateTrackMute(); }
	void SetSurfaceSolo(MediaTrack *tr, bool solo)		{ ScheduleTracklistUpdate(); UpdateTrackSolo(); }
	void SetSurfaceRecArm(MediaTrack *tr, bool arm)		{ ScheduleTracklistUpdate(); UpdateTrackArm(); }
	int Extended(int call, void *parm1, void *parm2, void *parm3)
	{
		BR_CSurf_Extended(call, parm1, parm2, parm3);
		SNM_CSurfExtended(call, parm1, parm2, parm3);
		return 0;
	}
};

// WDL Stuff
bool WDL_STYLE_GetBackgroundGradient(double *gradstart, double *gradslope) { return false; }
bool WDL_STYLE_AllowSliderMouseWheel() { return true; }
LICE_IBitmap *WDL_STYLE_GetSliderBitmap2(bool vert) { return NULL; }
int WDL_STYLE_GetSliderDynamicCenterPos() { return 500; }
int WDL_STYLE_GetSysColor(int i)
{
	int col = GSC_mainwnd(i);

	// check & "fix" 3D colors that aren't distinguished in many themes..
#ifdef _WIN32
	if (i == COLOR_3DSHADOW || i == COLOR_3DLIGHT || i == COLOR_3DHILIGHT)
#else
	if (i == COLOR_3DSHADOW || i == COLOR_3DHILIGHT)
#endif
	{
		int col3ds,col3dl,bgcol=GSC_mainwnd(COLOR_WINDOW);
		ColorTheme* ct = SNM_GetColorTheme();
		if (ct)
		{
			col3dl = ct->io_3d[0];
			col3ds = ct->io_3d[1];
			if (i == COLOR_3DSHADOW) col = col3ds;
			else col = col3dl;
		}
		else
		{
			col3dl = GSC_mainwnd(COLOR_3DHILIGHT);
			col3ds = GSC_mainwnd(COLOR_3DSHADOW);
		}

		if (col3ds == col3dl || col3ds == bgcol || col3dl == bgcol)
		{
			int colDelta = SNM_3D_COLORS_DELTA * (i == COLOR_3DSHADOW ? -1 : 1);
			col = RGB(
				BOUNDED(LICE_GETR(bgcol) + colDelta, 0, 0xFF),
				BOUNDED(LICE_GETG(bgcol) + colDelta, 0, 0xFF),
				BOUNDED(LICE_GETB(bgcol) + colDelta, 0, 0xFF));
		}
	}
	return col;
}
int WDL_STYLE_WantGlobalButtonBorders() { return 0; }
bool WDL_STYLE_WantGlobalButtonBackground(int *col) { return false; }
void WDL_STYLE_ScaleImageCoords(int *x, int *y) { }


// Main DLL entry point

SWSTimeSlice* g_ts=NULL;

void ErrMsg(const char* errmsg, bool wantblabla=true)
{
	if (errmsg && *errmsg && (!IsREAPER || IsREAPER())) // don't display any message if loaded from ReaMote
	{
		WDL_FastString msg(errmsg);
		if (wantblabla)
		{
			// reversed insertion
			msg.Insert(" ", 0);
			msg.Insert(__LOCALIZE("Hint:","sws_mbox"), 0);
			msg.Insert("\r\n", 0);
			msg.Insert(__LOCALIZE("An error occured during the SWS extension initialization.","sws_mbox"), 0);
		}
		MessageBox(Splash_GetWnd&&Splash_GetWnd()?Splash_GetWnd():NULL, msg.Get(), __LOCALIZE("SWS - Error","sws_mbox"), MB_OK);
	}
}

// IMPAPI: maps mandatory API functions, i.e. forces users to upgrade REAPER if these functions are not found
// IMPAP_OPT: maps optional API functions. Such function pointers will be NULL otherwise.
#define IMPAPI(x)       if (!errcnt && !((*(void **)&(x)) = (void *)rec->GetFunc(#x))) errcnt++;
#define IMPAP_OPT(x)    *((void **)&(x)) = (void *)rec->GetFunc(#x);
#define ERR_RETURN(a)   { ErrMsg(a); goto error; }

extern "C"
{
	REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t *rec)
	{
		if (!rec)
		{
error:
			if (plugin_register)
			{
				UnregisterAllCmds();
				plugin_register("-hookcommand2", (void*)hookCommandProc2);
				plugin_register("-hookcommand", (void*)hookCommandProc);
//				plugin_register("-hookpostcommand", (void*)hookPostCommandProc);
				plugin_register("-toggleaction", (void*)toggleActionHook);
				plugin_register("-hookcustommenu", (void*)swsMenuHook);
				if (g_ts) { plugin_register("-csurf_inst", g_ts); DELETE_NULL(g_ts); }
				UnregisterExportedFuncs();
			}

			if (g_bInitDone) // no granularity here, but it'd be an internal error anyway
			{
				ColorExit();
				AutorenderExit();
				SnapshotsExit();
				TrackListExit();
				MarkerListExit();
				MarkerActionsExit();
				AutoColorExit();
				ProjectListExit();
				ProjectMgrExit();
				XenakiosExit();
				ConsoleExit();
				FreezeExit();
				MiscExit();
				FNGExtensionExit();
				PadreExit();
				SNM_Exit();
				BR_Exit();
			}
			return 0; // makes REAPER unloading us
		}

		if (rec->caller_version != REAPER_PLUGIN_VERSION)
			ERR_RETURN("Wrong REAPER_PLUGIN_VERSION!")

		g_hInst = hInstance;
		g_hwndParent = rec->hwnd_main;

		if (!rec->GetFunc)
			ERR_RETURN("Null rec->GetFunc ptr.")

#ifdef _SWS_LOCALIZATION
		*(void **)&importedLocalizeFunc = rec->GetFunc("__localizeFunc");
		*(void **)&importedLocalizeMenu = rec->GetFunc("__localizeMenu");
		*(void **)&importedLocalizePrepareDialog = rec->GetFunc("__localizePrepareDialog");
#endif

		// Mandatory API functions
		int errcnt=0; // IMPAPI failed if >0

		IMPAPI(plugin_register); // keep those first
		IMPAPI(plugin_getapi);
		IMPAPI(IsREAPER);

		IMPAPI(AddExtensionsMainMenu);
		IMPAPI(AddMediaItemToTrack);
		IMPAPI(AddProjectMarker);
		IMPAPI(AddProjectMarker2);
		IMPAPI(AddTakeToMediaItem);
/* deprecated
		IMPAPI(AddTempoTimeSigMarker);
*/
		IMPAPI(adjustZoom);
		IMPAPI(ApplyNudge);
		IMPAPI(AttachWindowTopmostButton);
		IMPAPI(AttachWindowResizeGrip);
		IMPAPI(AudioAccessorValidateState);
		IMPAPI(Audio_Init); // v5.111+
		IMPAPI(Audio_Quit); // v5.111+
		IMPAPI(Audio_RegHardwareHook);
/* unused
		IMPAPI(Audio_RegHardwareHook);
*/
		IMPAPI(ColorToNative)
		IMPAPI(CoolSB_GetScrollInfo);
		IMPAPI(CoolSB_SetScrollInfo);
		IMPAPI(CountActionShortcuts);
		IMPAPI(CountAutomationItems);
		IMPAPI(CountEnvelopePoints); // v5pre4+
		IMPAPI(CountEnvelopePointsEx) // v5.40+
		IMPAPI(CountMediaItems); // O(N): should be banned from the extension, ideally -- don't use it in loops, at least
		IMPAPI(CountProjectMarkers);
		IMPAPI(CountSelectedMediaItems); // O(MN): should be banned from the extension, ideally -- don't use it in loops, at least
		IMPAPI(CountSelectedTracks); // exclude master + O(N): should be banned from the extension, ideally -- don't use it in loops, at least
		IMPAPI(CountTakeEnvelopes) // v5pre12+ -- O(N): don't use it in loops
		IMPAPI(CountTakes);
		IMPAPI(CountTCPFXParms);
		IMPAPI(CountTempoTimeSigMarkers);
		IMPAPI(CountTracks);
		IMPAPI(CountTrackMediaItems);
		IMPAPI(CountTrackEnvelopes);
		IMPAPI(CreateLocalOscHandler);
		IMPAPI(CreateNewMIDIItemInProj);
		IMPAPI(CreateTakeAudioAccessor);
		IMPAPI(CreateTrackAudioAccessor);
		IMPAPI(CreateTrackSend); // v5.15pre1+
		IMPAPI(CSurf_FlushUndo);
		IMPAPI(CSurf_GoEnd);
		IMPAPI(CSurf_OnMuteChange);
		IMPAPI(CSurf_OnPanChange);
		IMPAPI(CSurf_OnPlayRateChange);
		IMPAPI(CSurf_OnSelectedChange);
		IMPAPI(CSurf_OnTrackSelection);
		IMPAPI(CSurf_OnVolumeChange);
		IMPAPI(CSurf_OnWidthChange);
		IMPAPI(CSurf_TrackFromID);
		IMPAPI(CSurf_TrackToID);
		IMPAPI(DB2SLIDER);
		IMPAPI(DeleteEnvelopePointRange); // v5pre5+
		IMPAPI(DeleteEnvelopePointRangeEx); // v5.4pre3+
		IMPAPI(DeleteActionShortcut);
		IMPAPI(DeleteProjectMarker);
		IMPAPI(DeleteProjectMarkerByIndex);
		IMPAPI(DeleteTakeStretchMarkers);
		IMPAPI(DeleteTempoTimeSigMarker); // v5pre4+
		IMPAPI(DeleteTrack);
		IMPAPI(DeleteTrackMediaItem);
		IMPAPI(DestroyAudioAccessor);
		IMPAPI(DestroyLocalOscHandler);
		IMPAPI(DoActionShortcutDialog);
		IMPAPI(Dock_UpdateDockID);
		IMPAPI(DockIsChildOfDock);
		IMPAPI(DockWindowActivate);
		IMPAPI(DockWindowAdd);
		IMPAPI(DockWindowAddEx);
		IMPAPI(DockWindowRefresh);
		IMPAPI(DockWindowRemove);
		IMPAPI(EnsureNotCompletelyOffscreen);
		IMPAPI(EnumProjectMarkers);
		IMPAPI(EnumProjectMarkers2);
		IMPAPI(EnumProjectMarkers3);
		IMPAPI(EnumProjects);
		IMPAPI(Envelope_Evaluate); // v5pre4+
		IMPAPI(Envelope_SortPoints); // v5pre4+
		IMPAPI(Envelope_SortPointsEx) // v5.4pre3+
		IMPAPI(file_exists);
		IMPAPI(format_timestr);
		IMPAPI(format_timestr_pos);
		IMPAPI(format_timestr_len);
		IMPAPI(FreeHeapPtr);
		IMPAPI(GetActionShortcutDesc);
		IMPAPI(GetActiveTake);
		IMPAPI(GetAllProjectPlayStates); // v5.111+
		IMPAPI(GetAppVersion);
		IMPAPI(GetAudioAccessorEndTime);
		IMPAPI(GetAudioAccessorHash);
		IMPAPI(GetAudioAccessorSamples);
		IMPAPI(GetAudioAccessorStartTime);
		IMPAPI(GetColorThemeStruct);
		IMPAPI(GetContextMenu);
		IMPAPI(GetCurrentProjectInLoadSave);
		IMPAPI(GetCursorContext);
		IMPAPI(GetCursorContext2);
		IMPAPI(GetCursorPosition);
		IMPAPI(GetCursorPositionEx);
		IMPAPI(GetEnvelopeInfo_Value); // 5.982+
		IMPAPI(GetEnvelopeName);
		IMPAPI(GetEnvelopePoint); // v5pre4
		IMPAPI(GetEnvelopePointByTime); // v5pre4
		IMPAPI(GetEnvelopePointByTimeEx); // v5.40+
		IMPAPI(GetEnvelopePointEx); // v5.40+
		IMPAPI(GetEnvelopeScalingMode); // v5pre13+
		IMPAPI(GetEnvelopeStateChunk);
		IMPAPI(GetExePath);
		IMPAPI(GetFocusedFX);
		IMPAPI(GetFXEnvelope); // v5pre5+
		IMPAPI(GetGlobalAutomationOverride);
		IMPAPI(GetHZoomLevel);
		IMPAPI(GetIconThemePointer);
		IMPAPI(GetIconThemeStruct);
		IMPAPI(GetInputChannelName);
		IMPAPI(GetItemEditingTime2);
		IMPAPI(GetLastColorThemeFile); // v5.02+
		IMPAPI(GetLastMarkerAndCurRegion); // v4.60+
		IMPAPI(GetLastTouchedFX);
		IMPAPI(GetLastTouchedTrack);
		IMPAPI(GetMainHwnd);
		IMPAPI(GetMasterMuteSoloFlags);
		IMPAPI(GetMasterTrackVisibility);
		IMPAPI(GetMasterTrack);
		IMPAPI(GetMediaItem); // O(N): should be banned from the extension, ideally
		IMPAPI(GetMediaItem_Track);
		IMPAPI(GetMediaItemInfo_Value);
		IMPAPI(GetMediaItemNumTakes);
		IMPAPI(GetMediaItemTake);
		IMPAPI(GetMediaItemTakeByGUID);
		IMPAPI(GetMediaItemTake_Item);
		IMPAPI(GetMediaItemTake_Source);
		IMPAPI(GetMediaItemTake_Track);
		IMPAPI(GetMediaItemTakeInfo_Value);
		IMPAPI(GetMediaItemTrack);
		IMPAPI(GetMediaSourceFileName);
		IMPAPI(GetMediaSourceType);
		IMPAPI(GetMediaTrackInfo_Value);
/* deprecated, no-op
		IMPAPI(get_midi_config_var);
*/
		IMPAPI(GetMouseModifier);
		IMPAPI(GetNumTracks);
		IMPAPI(GetOutputChannelName);
		IMPAPI(GetPeakFileName);
		IMPAPI(GetPeakFileNameEx);
		IMPAPI(GetPeaksBitmap);
		IMPAPI(GetPlayPosition);
		IMPAPI(GetPlayPosition2);
		IMPAPI(GetPlayPositionEx);
		IMPAPI(GetPlayPosition2Ex);
		IMPAPI(GetPlayState);
		IMPAPI(GetPlayStateEx);
		IMPAPI(GetProjectLength);
		IMPAPI(GetProjectPath);
		IMPAPI(GetProjectStateChangeCount);
		IMPAPI(GetProjectTimeSignature2);
		IMPAPI(GetResourcePath);
		IMPAPI(GetSelectedEnvelope);
		IMPAPI(GetSelectedMediaItem); // O(MN): should be banned from the extension, ideally
		IMPAPI(GetSelectedTrack); // exclude master + O(N): should be banned from the extension, ideally
		IMPAPI(GetSelectedTrackEnvelope);
		IMPAPI(GetSet_ArrangeView2);
		IMPAPI(GetSetAutomationItemInfo);
		IMPAPI(GetSetEnvelopeState);
		IMPAPI(GetSetMediaItemInfo);
		IMPAPI(GetSetMediaItemTakeInfo);
		IMPAPI(GetSetMediaItemTakeInfo_String);
		IMPAPI(GetMediaSourceLength); // v5.0pre3+
		IMPAPI(GetSetMediaTrackInfo);
		IMPAPI(GetSetMediaTrackInfo_String);
		IMPAPI(GetSetProjectGrid);
		IMPAPI(GetSetObjectState);
		IMPAPI(GetSetObjectState2);
		IMPAPI(GetSetProjectNotes); // v5.15pre1+
		IMPAPI(GetSetRepeat);
		IMPAPI(GetSetTrackGroupMembership); // v5.21pre5+
		IMPAPI(GetSetTrackGroupMembershipHigh); // v5.70+
		IMPAPI(GetTempoTimeSigMarker);
		IMPAPI(GetTakeEnvelopeByName);
		IMPAPI(GetTakeName);
		IMPAPI(GetTakeStretchMarker);
		IMPAPI(GetSetTrackSendInfo);
		IMPAPI(GetSetTrackState);
		IMPAPI(GetSet_LoopTimeRange);
		IMPAPI(GetSet_LoopTimeRange2);
		IMPAPI(GetSubProjectFromSource);
		IMPAPI(GetTake);
		IMPAPI(GetTakeEnvelope); // v5pre12+
		IMPAPI(GetTakeNumStretchMarkers);
		IMPAPI(GetTCPFXParm);
		IMPAPI(GetToggleCommandState);
		IMPAPI(GetToggleCommandState2);
		IMPAPI(GetToggleCommandStateEx);
		IMPAPI(GetSelectedEnvelope);
		IMPAPI(GetToggleCommandStateThroughHooks);
		IMPAPI(GetTooltipWindow);
		IMPAPI(GetTrack);
		IMPAPI(GetTrackAutomationMode);
		IMPAPI(GetTrackGUID);
		IMPAPI(GetTrackEnvelope);
		IMPAPI(GetTrackEnvelopeByName);
		IMPAPI(GetTrackInfo);
		IMPAPI(GetTrackMediaItem);
		IMPAPI(GetTrackMIDINoteNameEx);
		IMPAPI(GetTrackNumMediaItems);
		IMPAPI(GetTrackNumSends);
		IMPAPI(GetTrackStateChunk);
		IMPAPI(GetTrackUIVolPan);
		IMPAPI(GetUserInputs);
		IMPAPI(get_config_var);
		IMPAPI(get_ini_file);
		IMPAPI(GR_SelectColor);
		IMPAPI(GSC_mainwnd);
		IMPAPI(guidToString);
		IMPAPI(Help_Set);
		IMPAPI(InsertAutomationItem);
		IMPAPI(InsertMedia);
		IMPAPI(InsertEnvelopePoint); // v5pre4+
		IMPAPI(InsertEnvelopePointEx); // v5.4pre3+
		IMPAPI(InsertTrackAtIndex);
		IMPAPI(IsMediaExtension);
		IMPAPI(IsMediaItemSelected);
		IMPAPI(IsProjectDirty);
		IMPAPI(IsTrackVisible);
		IMPAPI(kbd_enumerateActions);
		IMPAPI(kbd_formatKeyName);
		IMPAPI(kbd_getCommandName);
		IMPAPI(kbd_getTextFromCmd);
		IMPAPI(KBD_OnMainActionEx);
		IMPAPI(kbd_reprocessMenu);
		IMPAPI(kbd_RunCommandThroughHooks);
		IMPAPI(kbd_translateAccelerator);
#ifdef __APPLE__
		IMPAPI(ListView_HeaderHitTest); // undocumented
#endif
		IMPAPI(Main_OnCommand);
		IMPAPI(Main_OnCommandEx);
		IMPAPI(Main_openProject);
		IMPAPI(MainThread_LockTracks);
		IMPAPI(MainThread_UnlockTracks);
		IMPAPI(MarkProjectDirty);
		IMPAPI(Master_GetPlayRate);
		IMPAPI(Master_GetPlayRateAtTime);
		IMPAPI(MIDI_CountEvts);
		IMPAPI(MIDI_DeleteCC);
		IMPAPI(MIDI_DeleteEvt);
		IMPAPI(MIDI_DeleteNote);
		IMPAPI(MIDI_DeleteTextSysexEvt);
		IMPAPI(MIDI_EnumSelCC);
		IMPAPI(MIDI_EnumSelEvts);
		IMPAPI(MIDI_EnumSelNotes);
		IMPAPI(MIDI_EnumSelTextSysexEvts);
		IMPAPI(MIDI_eventlist_Create);
		IMPAPI(MIDI_eventlist_Destroy);
		IMPAPI(MIDI_GetCC);
		IMPAP_OPT(MIDI_GetCCShape); // v6.0
		IMPAPI(MIDI_GetEvt);
		IMPAPI(MIDI_GetGrid)
		IMPAPI(MIDI_GetNote);
		IMPAPI(MIDI_GetPPQPos_EndOfMeasure);
		IMPAPI(MIDI_GetPPQPos_StartOfMeasure);
		IMPAPI(MIDI_GetPPQPosFromProjTime);
		IMPAPI(MIDI_GetProjTimeFromPPQPos);
		IMPAPI(MIDI_GetTextSysexEvt);
		IMPAPI(MIDI_InsertCC);
		IMPAPI(MIDI_InsertEvt);
		IMPAPI(MIDI_InsertNote);
		IMPAPI(MIDI_InsertTextSysexEvt);
		IMPAPI(MIDI_SetCC);
		IMPAP_OPT(MIDI_SetCCShape); // v6.0
		IMPAPI(MIDI_SetEvt);
		IMPAPI(MIDI_SetItemExtents); // v5.0pre (no data on exact build in whatsnew, but I'm pretty sure I never saw this in v4)
		IMPAPI(MIDI_SetNote);
		IMPAPI(MIDI_SetTextSysexEvt);
		IMPAPI(MIDIEditor_GetActive);
		IMPAPI(MIDIEditor_GetMode);
		IMPAPI(MIDIEditor_GetSetting_int);
		IMPAPI(MIDIEditor_GetTake);
		IMPAPI(MIDIEditor_LastFocused_OnCommand);
		IMPAPI(MIDIEditor_OnCommand);
		IMPAPI(mkpanstr);
		IMPAPI(mkvolpanstr);
		IMPAPI(mkvolstr);
		IMPAPI(MoveEditCursor);
		IMPAPI(MoveMediaItemToTrack);
		IMPAPI(OnColorThemeOpenFile); // v5.0pre21+
		IMPAPI(OnPauseButton);
		IMPAPI(OnPauseButtonEx);
		IMPAPI(OnPlayButton);
		IMPAPI(OnPlayButtonEx);
		IMPAPI(OnStopButton);
		IMPAPI(OnStopButtonEx);
		IMPAPI(NamedCommandLookup);
		IMPAPI(parse_timestr_len);
		IMPAPI(parse_timestr_pos);
		IMPAPI(PCM_Sink_CreateEx);
		IMPAPI(PCM_Source_CreateFromFile);
		IMPAPI(PCM_Source_CreateFromFileEx);
		IMPAPI(PCM_Source_CreateFromSimple);
		IMPAPI(PCM_Source_CreateFromType);
		IMPAPI(PCM_Source_GetSectionInfo);
		IMPAPI(PlayPreview);
		IMPAPI(PlayPreviewEx);
		IMPAPI(PlayTrackPreview);
		IMPAPI(PlayTrackPreview2Ex);
		IMPAPI(plugin_getFilterList);
		IMPAPI(plugin_getImportableProjectFilterList);
		IMPAPI(PreventUIRefresh);
		IMPAPI(projectconfig_var_addr);
		IMPAPI(projectconfig_var_getoffs);
		IMPAPI(realloc_cmd_ptr); // v5.965+
		IMPAPI(RefreshToolbar);
		IMPAPI(RefreshToolbar2); // v5pre8+
#ifdef _WIN32
		IMPAPI(RemoveXPStyle);
#endif
		IMPAPI(RenderFileSection);
		IMPAPI(Resample_EnumModes);
		IMPAPI(Resampler_Create);
		IMPAPI(RemoveTrackSend); // v5.15pre1+
		IMPAPI(ReverseNamedCommandLookup);
		IMPAPI(ScaleFromEnvelopeMode); // v5pre13+
		IMPAPI(ScaleToEnvelopeMode); // v5pre13+
		IMPAPI(screenset_register);
		IMPAPI(screenset_registerNew);
		IMPAPI(screenset_unregister);
		IMPAPI(SectionFromUniqueID);
		IMPAPI(SelectProjectInstance);
		IMPAPI(SendLocalOscMessage);
		IMPAPI(SetActiveTake)
		IMPAPI(SetCurrentBPM);
		IMPAPI(SetCursorContext);
		IMPAPI(SetEditCurPos);
		IMPAPI(SetEditCurPos2);
		IMPAPI(SetEnvelopePoint); // v5pre4+
		IMPAPI(SetEnvelopePointEx) // v5.40pre3
		IMPAPI(SetEnvelopeStateChunk);
		IMPAPI(SetGlobalAutomationOverride);
		IMPAPI(SetMasterTrackVisibility);
		IMPAPI(SetMediaItemInfo_Value);
		IMPAPI(SetMediaItemLength);
		IMPAPI(SetMediaItemPosition);
		IMPAPI(SetMediaItemSelected);
		IMPAPI(SetMediaItemTakeInfo_Value);
		IMPAPI(SetMediaTrackInfo_Value);
		IMPAPI(SetMIDIEditorGrid);
		IMPAPI(SetMixerScroll);
		IMPAPI(SetMouseModifier);
		IMPAPI(SetOnlyTrackSelected);
		IMPAPI(SetProjectMarkerByIndex);
		IMPAPI(SetProjectMarkerByIndex2);
		IMPAPI(SetProjectMarker);
		IMPAPI(SetProjectMarker2);
		IMPAPI(SetProjectMarker3);
		IMPAPI(SetProjectMarker4);
		IMPAPI(SetTempoTimeSigMarker);
		IMPAPI(SetTakeStretchMarker);
		IMPAPI(SetTrackAutomationMode);
		IMPAPI(SetTrackSelected);
		IMPAPI(SetTrackSendUIPan);
		IMPAPI(SetTrackSendUIVol);
		IMPAPI(SetTrackStateChunk);
		IMPAPI(ShowActionList);
		IMPAPI(ShowConsoleMsg);
		IMPAPI(ShowMessageBox);
		IMPAPI(SLIDER2DB);
		IMPAPI(SnapToGrid);
		IMPAPI(Splash_GetWnd);
		IMPAPI(SplitMediaItem);
		IMPAPI(StopPreview);
		IMPAPI(StopTrackPreview);
		IMPAPI(StopTrackPreview2);
		IMPAPI(stringToGuid);
		IMPAPI(TakeFX_GetChainVisible);
		IMPAPI(TakeFX_GetCount);
		IMPAPI(TakeFX_GetFloatingWindow);
		IMPAPI(TakeFX_GetOffline); // v5.95+
		IMPAPI(TakeFX_SetOffline); // v5.95+
		IMPAPI(TakeFX_SetOpen);
		IMPAPI(TakeFX_Show);
		IMPAPI(TakeIsMIDI);
		IMPAPI(time_precise);
		IMPAPI(TimeMap_GetDividedBpmAtTime);
		IMPAPI(TimeMap_GetTimeSigAtTime);
		IMPAPI(TimeMap_QNToTime);
		IMPAPI(TimeMap_QNToTime_abs);
		IMPAPI(TimeMap_timeToQN);
		IMPAPI(TimeMap_timeToQN_abs);
		IMPAPI(TimeMap2_beatsToTime);
		IMPAPI(TimeMap2_GetDividedBpmAtTime);
		IMPAPI(TimeMap2_GetNextChangeTime);
		IMPAPI(TimeMap2_QNToTime);
		IMPAPI(TimeMap2_timeToBeats);
		IMPAPI(TimeMap2_timeToQN);
		IMPAPI(TimeMap_curFrameRate);
		IMPAPI(TrackFX_FormatParamValue);
		IMPAPI(TrackFX_GetByName);
		IMPAPI(TrackFX_GetChainVisible);
		IMPAPI(TrackFX_GetEnabled);
		IMPAPI(TrackFX_GetFloatingWindow);
		IMPAPI(TrackFX_GetCount);
		IMPAPI(TrackFX_GetFXName);
		IMPAPI(TrackFX_GetFXGUID);
		IMPAPI(TrackFX_GetInstrument); // NF: didn't find when this was added in changelog
		IMPAPI(TrackFX_GetNumParams);
		IMPAPI(TrackFX_GetOpen);
		IMPAPI(TrackFX_GetParam);
		IMPAPI(TrackFX_GetParamName);
		IMPAPI(TrackFX_GetPreset);
		IMPAPI(TrackFX_GetPresetIndex);
		IMPAPI(TrackFX_GetUserPresetFilename); // v5.15pre1+
		IMPAPI(TrackFX_NavigatePresets);
		IMPAPI(TrackFX_GetOffline); // v5.95+
		IMPAPI(TrackFX_SetOffline); // v5.95+
		IMPAPI(TrackFX_SetEnabled);
		IMPAPI(TrackFX_SetOpen);
		IMPAPI(TrackFX_SetParam);
		IMPAPI(TrackFX_SetPreset);
		IMPAPI(TrackFX_SetPresetByIndex);
		IMPAPI(TrackFX_Show);
		IMPAPI(TrackList_AdjustWindows);
		IMPAPI(Undo_BeginBlock);
		IMPAPI(Undo_BeginBlock2);
		IMPAPI(Undo_CanRedo2);
		IMPAPI(Undo_CanUndo2);
		IMPAPI(Undo_DoUndo2);
		IMPAPI(Undo_EndBlock);
		IMPAPI(Undo_EndBlock2);
		IMPAPI(Undo_OnStateChange);
		IMPAPI(Undo_OnStateChange_Item);
		IMPAPI(Undo_OnStateChange2);
		IMPAPI(Undo_OnStateChangeEx); // note: the last param "trackparm" is ignored ATM (v5.15pre6)
		IMPAPI(Undo_OnStateChangeEx2); // note: the last param "trackparm" is ignored ATM (v5.15pre6)
		IMPAPI(UpdateArrange);
		IMPAPI(UpdateItemInProject);
		IMPAPI(UpdateTimeline);
		IMPAPI(ValidatePtr);
		IMPAPI(ValidatePtr2); // v5.12

		if (errcnt)
		{
			char txt[2048]="";
			snprintf(txt, sizeof(txt),
					// keep the message on a single line (for the LangPack generator)
					__LOCALIZE_VERFMT("The version of SWS extension you have installed is incompatible with your version of REAPER. You probably have a REAPER version less than v%d.%d%.0d installed.\r\nPlease install the latest version of REAPER from www.reaper.fm.","sws_mbox"),
					REA_VERSION);

			ErrMsg(txt,false);
			goto error;
		}

		// Look for SWS dupe/clone
		if (rec->GetFunc("SNM_GetIntConfigVar"))
		{
			WDL_FastString dir1, dir2, mypath(__LOCALIZE("Unknown","sws_mbox")), conflict;
#ifdef _WIN32
			dir1.SetFormatted(2048, "%s\\%s", GetExePath(), "Plugins");
			dir2.SetFormatted(2048, "%s\\%s", GetResourcePath(), "UserPlugins");

			HMODULE hm = NULL;
			if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
								   (LPCWSTR)&hookCommandProc, &hm))
			{
				wchar_t wpath[2048];
				GetModuleFileNameW(hm, wpath, sizeof(wpath));

				char path[2048]="";
				WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, sizeof(path), NULL, NULL);
				mypath.Set(path);
			}
#else
			dir1.SetFormatted(2048, "%s/%s", GetResourcePath(), "UserPlugins");
			dir2.Set("/Library/Application Support/REAPER/UserPlugins");

			Dl_info info;
			if (dladdr((const void*)hookCommandProc, &info))
			{
				mypath.Set(info.dli_fname);
			}

			// older reaper_sws.dylib?
			// (renamed into reaper_sws_extension.dylib in newer SWS version so that REAPER scans both)
			if (!strnicmp(mypath.Get(), dir2.Get(), dir2.GetLength()))
			{
				WDL_FastString tmp(dir1.Get());
				tmp.Append("/reaper_sws.dylib");
				if (FileExists(tmp.Get()))
				{
					conflict.SetFormatted(2048, __LOCALIZE_VERFMT("(probably %s/reaper_sws.dylib)","sws_mbox"), dir1.Get());
				}
			}
#endif

			char txt[8192]="";
			snprintf(txt, sizeof(txt),
			// keep the message on a single line (for the LangPack generator)
			__LOCALIZE_VERFMT("Several versions of the SWS extension (or SWS clones) are installed!\n\nThis SWS extension instance will not be loaded:\n- Version: %d.%d.%d #%d\n- Location: %s\n\nPlease quit REAPER and remove the conflicting extension %s.\n\nNote: REAPER will look for extension plugins in the following order/folders:\n\t%s\n\t%s","sws_mbox"),
				SWS_VERSION,
				mypath.Get(),
				conflict.Get()[0] ? conflict.Get() : __LOCALIZE("(see version in Main menu > Extensions > About SWS Extension)","sws_mbox"),
				dir1.Get(), dir2.Get());

			ErrMsg(txt,false);
			return 0; // do not unregister stuff of the conflicting plugin!
		}

		// hookcommand2 must be registered before hookcommand
		if (!rec->Register("hookcommand2", (void*)hookCommandProc2))
			ERR_RETURN("hookcommand2 error.")

		if (!rec->Register("hookcommand", (void*)hookCommandProc))
			ERR_RETURN("hookcommand error.")

		//if (!rec->Register("hookpostcommand", (void*)hookPostCommandProc))
		//	ERR_RETURN("hookpostcommand error.")

		if (!rec->Register("toggleaction", (void*)toggleActionHook))
			ERR_RETURN("Toggle action hook error.")

		// Call plugin specific init
		if (!AutoColorInit())
			ERR_RETURN("Auto Color init error.")
		if (!ColorInit())
			ERR_RETURN("Color init error.")
		if (!MarkerListInit())
			ERR_RETURN("Marker list init error.")
		if (!MarkerActionsInit())
			ERR_RETURN("Marker action init error.")
		if (!ConsoleInit())
			ERR_RETURN("ReaConsole init error.")
		if (!FreezeInit())
			ERR_RETURN("Freeze init error.")
		if (!SnapshotsInit()) // must be called before SNM_Init registers dynamic actions
			ERR_RETURN("Snapshots init error.")
		if (!TrackListInit())
			ERR_RETURN("Tracklist init error.")
		if (!ProjectListInit())
			ERR_RETURN("Project List init error.")
		if (!ProjectMgrInit())
			ERR_RETURN("Project Mgr init error.")
		if (!XenakiosInit())
			ERR_RETURN("Xenakios init error.")
		if (!MiscInit())
			ERR_RETURN("Misc init error.")
		if (!ZoomInit(false))
			ERR_RETURN("Zoom init error.")
		if(!FNGExtensionInit())
			ERR_RETURN("Fingers init error.")
		if (!PadreInit())
			ERR_RETURN("Padre init error.")
		if (!AboutBoxInit())
			ERR_RETURN("About box init error.")
		if (!AutorenderInit())
			ERR_RETURN("Autorender init error.")
		if (!IXInit())
			ERR_RETURN("IX init error.")
		if (!BR_Init())
			ERR_RETURN("Breeder init error.")
		if (!WOL_Init())
			ERR_RETURN("Wol init error.")
		if (!nofish_Init())
			ERR_RETURN("nofish init error.")
		if (!snooks_Init())
			ERR_RETURN("snooks init error.")
		if (!SNM_Init(rec)) // keep it as the last init (for cycle actions)
			ERR_RETURN("S&M init error.")

		// above specific inits went well
		{
			g_bInitDone=true;

			g_ts = new SWSTimeSlice();
			if (!rec->Register("csurf_inst", g_ts))
			{
				delete g_ts;
				ERR_RETURN("TimeSlice init error.")
			}
		}

		if (!rec->Register("hookcustommenu", (void*)swsMenuHook))
			ERR_RETURN("Menu hook error.")

		if ((!RegisterExportedFuncs(rec) || !RegisterExportedAPI(rec)))
			ERR_RETURN("Reascript export failed.");

		rec->Register("timer", (void*)importExtensionAPI);

		AddExtensionsMainMenu();
		ZoomInit(true); // touchy! only hook REAPER window procs at the very end, i.e. only if everything else went well
		BR_InitPost();  // same reason as above (wnd hooks)
		return 1;
	}
};   // end extern C


#ifndef _WIN32 // MAC resources
#include "WDL/swell/swell-dlggen.h"
#include "sws_extension.rc_mac_dlg"
#undef BEGIN
#undef END
#include "WDL/swell/swell-menugen.h"
#include "sws_extension.rc_mac_menu"
#endif
