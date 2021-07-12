/******************************************************************************
/ SnM_Cyclactions.cpp
/
/ Copyright (c) 2011 and later Jeffos
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

//JFB
// - TODO: use native macro file format?
//   (CAs have their own format for historical reasons..)

#include "stdafx.h"

#include "SnM.h"
#include "SnM_Cyclactions.h"
#include "SnM_Dlg.h"
#include "SnM_Util.h"
#include "SnM_Window.h"
#include "../url.h"
#include "../Console/Console.h"
#include "../IX/IX.h"

#include <WDL/localize/localize.h>
#include <WDL/projectcontext.h>

#define CA_WND_ID	"SnMCyclaction"


WDL_PtrList<Cyclaction> g_cas[SNM_MAX_CA_SECTIONS];
SNM_WindowManager<CyclactionWnd> g_caWndMgr(CA_WND_ID);
bool g_undos = true; // consolidate undo points
bool g_preventUIRefresh = true;

///////////////////////////////////////////////////////////////////////////////
// CA helpers
///////////////////////////////////////////////////////////////////////////////

const char* GetCACustomId(int _sectionIdx)
{
	if (_sectionIdx>=0 && _sectionIdx<SNM_MAX_CA_SECTIONS)
	{
		SECTION_INFO_T *info = SNM_GetActionSectionInfo(_sectionIdx);
		if (info) return info->ca_cust_id;
	}
	return "";
}

int GetCASectionFromCustId(const char* _custId)
{
	for (int idx=0; idx<SNM_MAX_CA_SECTIONS; idx++)
		if (strstr(_custId, GetCACustomId(idx)))
			return idx;
	return -1;
}

const char* GetCAIniSection(int _sectionIdx)
{
	if (_sectionIdx>=0 && _sectionIdx<SNM_MAX_CA_SECTIONS)
	{
		SECTION_INFO_T *info = SNM_GetActionSectionInfo(_sectionIdx);
		if (info) return info->ca_ini_sec;
	}
	return "";
}

// this func works for CAs that are not registered yet
// (i.e. does not use NamedCommandLookup())
Cyclaction* GetCAFromCustomId(int _section, const char* _cmdStr, int* _cycleId = NULL)
{
	 // atoi() ok because custom ids are 1-based
	if (_cmdStr && *_cmdStr)
		if (int id = atoi((const char*)(_cmdStr + (*_cmdStr=='_'?1:0) + strlen(GetCACustomId(_section))))) {
			if (_cycleId) *_cycleId = id;
			return g_cas[_section].Get(id-1);
		}
	return NULL;
}

bool CheckRecursiveCmd(const char* _cmdStr, WDL_PtrList<WDL_FastString>* _parentCmds)
{
	if (_parentCmds && _cmdStr)
		for (int i=0; i<_parentCmds->GetSize(); i++)
			if (WDL_FastString* cmd = _parentCmds->Get(i))
				if (!strcmp(cmd->Get(), _cmdStr))
					return false;
	return true;
}


///////////////////////////////////////////////////////////////////////////////
// Statements
///////////////////////////////////////////////////////////////////////////////

// gotcha: do not localize these strings!
#define STATEMENT_IF		"IF"
#define STATEMENT_IFNOT		"IF NOT"
#define STATEMENT_IFAND		"IF AND"
#define STATEMENT_IFNAND	"IF NAND"
#define STATEMENT_IFOR		"IF OR"
#define STATEMENT_IFNOR		"IF NOR"
#define STATEMENT_IFXOR		"IF XOR"
#define STATEMENT_IFXNOR	"IF XNOR"
#define STATEMENT_ELSE		"ELSE"
#define STATEMENT_ENDIF		"ENDIF"
#define STATEMENT_LOOP		"LOOP"
#define STATEMENT_ENDLOOP	"ENDLOOP"
#define STATEMENT_CONSOLE	"CONSOLE"
#define STATEMENT_LABEL		"LABEL"

enum {
  IDX_STATEMENT_IF=0,
  IDX_STATEMENT_IFNOT,
  IDX_STATEMENT_IFAND,
  IDX_STATEMENT_IFNAND,
  IDX_STATEMENT_IFOR,
  IDX_STATEMENT_IFNOR,
  IDX_STATEMENT_IFXOR,
  IDX_STATEMENT_IFXNOR,
  IDX_STATEMENT_ELSE,
  IDX_STATEMENT_ENDIF,
  IDX_STATEMENT_LOOP,
  IDX_STATEMENT_ENDLOOP,
  IDX_STATEMENT_CONSOLE,
  IDX_STATEMENT_LABEL,
  NB_STATEMENTS
};

const char g_statements[][16] = {
	STATEMENT_IF,
	STATEMENT_IFNOT,
	STATEMENT_IFAND,
	STATEMENT_IFNAND,
	STATEMENT_IFOR,
	STATEMENT_IFNOR,
	STATEMENT_IFXOR,
	STATEMENT_IFXNOR,
	STATEMENT_ELSE,
	STATEMENT_ENDIF,
	STATEMENT_LOOP,
	STATEMENT_ENDLOOP,
	STATEMENT_CONSOLE,
	STATEMENT_LABEL
};

//!WANT_LOCALIZE_STRINGS_BEGIN:sws_DLG_161
const char g_statementInfos[][64] = {
	"If the next action is ON",
	"If the next action is OFF",
	"If both next actions are ON",
	"If at least 1 of the next 2 actions is OFF",
	"If at least 1 of the next 2 actions is ON",
	"If both next actions are OFF",
	"If the next 2 actions' states are different",
	"If the next 2 actions' states are the same",
	"Else",
	"End of conditional statement", 
	"Loop", 
	"End of loop", 
	"ReaConsole command",
	"Label Processor command"
};
//!WANT_LOCALIZE_STRINGS_END


bool IsStatementWithParam(int _id) {
	return _id==IDX_STATEMENT_LOOP || _id==IDX_STATEMENT_CONSOLE || _id==IDX_STATEMENT_LABEL;
}

int IsStatement(const char* _cmd)
{
	if (_cmd && *_cmd)
		for (int i=0; i<NB_STATEMENTS; i++)
		{
			if (IsStatementWithParam(i)) {
				if (!_strnicmp(g_statements[i], _cmd, strlen(g_statements[i])))
					return i;
			}
			else if (!_stricmp(g_statements[i], _cmd))
				return i;
		}
	return -1;
}

bool IsTwoCondStatement(const char* _cmd)
{
	return (_cmd && (
		!_stricmp(STATEMENT_IFAND, _cmd) || !_stricmp(STATEMENT_IFNAND, _cmd) || 
		!_stricmp(STATEMENT_IFOR, _cmd) || !_stricmp(STATEMENT_IFNOR, _cmd) || 
		!_stricmp(STATEMENT_IFXOR, _cmd) || !_stricmp(STATEMENT_IFXNOR, _cmd)));
}

bool IsCondStatement(const char* _cmd) {
	return (_cmd && (!_stricmp(STATEMENT_IF, _cmd) || !_stricmp(STATEMENT_IFNOT, _cmd) || IsTwoCondStatement(_cmd)));
}


///////////////////////////////////////////////////////////////////////////////
// Explode _cmdStr into "atomic" actions
//
// IMPORTANT: THOSE FUNCS MUST ASSUME ACTIONS ARE NOT REGISTERED YET!
//
// _flags:    &1=perform
//               Rmk: no need to explode sub CAs, macros and console cmds 
//               recursively in this case: faster + handles scripts + 
//               issue 618 + special macros made of "Wait for 1 s before..."
//            &2=get 1st toggle state
//            &8=explode all CAs' steps (current step only otherwise)
// _cmdStr:   custom id to explode
// _cmds:     output list of exploded commands
//            it is up to the caller to unalloc items!
// _macros:   to optimize accesses to reaper-kb.ini, 
//            if NULL, macros won't be exploded, no file access
// _consoles: to optimize accesses to reaconsole_customcommands.txt
//
// return values:
// if _flags&2: 1st found/valid toggle state (0 or 1)
//              -1 otherwise (no valid toggle state found)
// otherwise:   -2=recursion err, -1=other err, 1/0=exploded or not
///////////////////////////////////////////////////////////////////////////////

int ExplodeCmd(int _section, const char* _cmdStr,
	WDL_PtrList<WDL_FastString>* _cmds, WDL_PtrList<WDL_FastString>* _macros, 
	WDL_PtrList<WDL_FastString>* _consoles, int _flags)
{
	if (_cmdStr && *_cmdStr)
	{
		if (*_cmdStr == '_') // CA, extension, macro, or script?
		{
			if (strstr(_cmdStr, "_CYCLACTION"))
				return ExplodeCyclaction(_section, _cmdStr, _cmds, _macros, _consoles, _flags);
			else if (strstr(_cmdStr, "_SWSCONSOLE_CUST"))
				return ExplodeConsoleAction(_section, _cmdStr, _cmds, _macros, _consoles, _flags);
			else if (IsMacroOrScript(_cmdStr, false))
				return ExplodeMacro(_section, _cmdStr, _cmds, _macros, _consoles, _flags);
		}

		if (_flags&2)
		{
			if (KbdSectionInfo* kbdSec = SNM_GetActionSection(_section)) {
				if (int i = SNM_NamedCommandLookup(_cmdStr, kbdSec)) {
					i = GetToggleCommandState2(kbdSec, i);
					if (i>=0) return i;
				}
			}
			return -1;
		}

		if (_cmds)
			_cmds->Add(new WDL_FastString(_cmdStr));
	}
	return 0;
}

int ExplodeMacro(int _section, const char* _cmdStr,
	WDL_PtrList<WDL_FastString>* _cmds, WDL_PtrList<WDL_FastString>* _macros, 
	WDL_PtrList<WDL_FastString>* _consoles, int _flags)
{
	if (_flags&2) return -1; // macros/scripts do not report toggle states

	 // want macro explosion?
	if (_macros)
	{
		bool loadOk = true;
		if (!_macros->GetSize()) {
			loadOk = LoadKbIni(_macros);
			_macros->Add(new WDL_FastString);
		}
		if (!loadOk) return -1;

		WDL_PtrList_DeleteOnDestroy<WDL_FastString> subCmds;
		int r = GetMacroOrScript(_cmdStr, SNM_GetActionSectionUniqueId(_section), _macros, &subCmds);
		if (r==0)
		{
			return -1;
		}
		// macro?
		else if (r==1) // macro only!
		{
			WDL_FastString* parentCmd = NULL;
			if (_cmds)
			{
				if (!CheckRecursiveCmd(_cmdStr, _cmds))
					return -2;
				parentCmd = _cmds->Add(new WDL_FastString(_cmdStr));
			}
			for (int i=0; i<subCmds.GetSize(); i++) {
				r = ExplodeCmd(_section, subCmds.Get(i)->Get(), _cmds, _macros, _consoles, _flags);
				if (r<0) return r;
			}
			// it's a recursion check, not a dup check => remove the parent cmd
			if (_cmds && parentCmd)
				_cmds->Delete(_cmds->Find(parentCmd), true);
			return 1;
		}
		// script?
		else if (r==2)
		{
			// fall through...
			// we could analyze action calls within scripts but we just rely on 
			// hookCommandProc() and toggleActionHook() recursivity checks ATM...
		}
	}

	if (_cmds)
		_cmds->Add(new WDL_FastString(_cmdStr));
	return 0;
}

// _action: optional (tiny optimiz)
int ExplodeCyclaction(int _section, const char* _cmdStr, 
	WDL_PtrList<WDL_FastString>* _cmds, WDL_PtrList<WDL_FastString>* _macros, 
	WDL_PtrList<WDL_FastString>* _consoles, int _flags, Cyclaction* _action)
{
	// check "cross-section CA"
	// note: it would be easy to authorize "cross-section CA" like:
	//	_section = GetCASectionFromCustId(_cmdStr);
	// but this would lead to a general design issue: sections would
	// need to be defined for each commands (e.g. in the right list view)
	if (_section != GetCASectionFromCustId(_cmdStr))
		return -3;

	Cyclaction* action = _action;
	if (!action) action = GetCAFromCustomId(_section, _cmdStr); 
	if (!action) return -1;

	if (_flags&2)
	{
		switch(action->IsToggle())
		{
			case 1: return action->m_fakeToggle ? 1 : 0; 
			case 2: break; // real toggle state: requires explosion, see below..
			default: return -1;
		}
	}

	WDL_FastString* parentCmd = NULL;
	if (_cmds)
	{
		if (!CheckRecursiveCmd(_cmdStr, _cmds))
			return -2;
		parentCmd = _cmds->Add(new WDL_FastString(_cmdStr));
	}

	// add actions of all cycle points or only actions of the current cycle point
	int startIdx = (_flags&8) ? 0 : action->GetStepIdx();
	if (startIdx<0) return -1;

	bool done=false;
	for (int i=startIdx; !done && i<action->GetCmdSize(); i++)
	{
		const char* cmd = action->GetCmd(i);

		// break on end of list
		if (i == (action->GetCmdSize()-1))
		{
			if (_flags&1) { action->m_performState = 0; action->m_fakeToggle = !action->m_fakeToggle; }
			done = (_flags&8)!=8;
		}
		// break on next step
		else if (*cmd == '!')
		{
			if (_flags&1) { action->m_performState++; action->m_fakeToggle = !action->m_fakeToggle; }
			done = (_flags&8)!=8;
		}

		// add/explode sub actions
		if (*cmd && *cmd != '!') 
		{
			int r = ExplodeCmd(_section, cmd, _cmds, _macros, _consoles, _flags); // recursive call
			if (_flags&2) { if (r>=0) return r; }
			else if (r<0) return r;
		}
	}

	// it's a recursion check, not a dup check => remove the parent cmd
	if (_cmds && parentCmd)
		_cmds->Delete(_cmds->Find(parentCmd), true);

#ifdef _SNM_DEBUG
	if (_cmds)
	{
		OutputDebugString("ExplodeCyclaction: ");
		OutputDebugString(_cmdStr);
		OutputDebugString(" -> ");
		for (int i=0; i<_cmds->GetSize(); i++) {
			OutputDebugString(_cmds->Get(i)->Get());
			OutputDebugString(" ");
		}
		OutputDebugString("\n");
	}
#endif

	// CA must always be exploded!
	// (they can't be recursive, see remarks on top of file)
	return _flags&2 ? -1 : 1;
}

int ExplodeConsoleAction(int _section, const char* _cmdStr,
	WDL_PtrList<WDL_FastString>* _cmds, WDL_PtrList<WDL_FastString>* _macros, 
	WDL_PtrList<WDL_FastString>* _consoles, int _flags)
{
	if (_flags&2) return -1; // console actions do not report toggle states

	// safe single lazy init of _consoleCmds
	if (_consoles) // console cmd explosion required?
	{
		bool loadOk = true;
		if (!_consoles->GetSize()) {
			loadOk = LoadConsoleCmds(_consoles);
			_consoles->Add(new WDL_FastString);
		}
		if (!loadOk) return -1;

		// little trick to avoid NamedCommandLookup() as this action might not be registered yet
		int id = atoi(_cmdStr + (*_cmdStr=='_' ? 1 : 0) + strlen("SWSCONSOLE_CUST")); // ok because 1-based
		if (!id || id>=_consoles->GetSize()) return -1; else id--; // check + back to 0-base
		if (_cmds)
		{
			WDL_FastString* explCmd = new WDL_FastString;
			explCmd->SetFormatted(256, "%s %s", STATEMENT_CONSOLE, _consoles->Get(id)->Get());
			_cmds->Add(explCmd);
		}
		return 1;
	}

	if (_cmds)
		_cmds->Add(new WDL_FastString(_cmdStr));
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
// Perform cycle actions
///////////////////////////////////////////////////////////////////////////////

// assumes _cmdStr is valid and has been "exploded", if needed
int PerformSingleCommand(int _section, const char* _cmdStr, int _val, int _valhw, int _relmode, HWND _hwnd)
{
#ifdef _SNM_DEBUG
	OutputDebugString(_cmdStr);
	OutputDebugString("\n");
#endif
	if (_cmdStr && *_cmdStr)
	{
		KbdSectionInfo* kbdSec = SNM_GetActionSection(_section);
		if (!kbdSec)
			return 0;

		// SNM_NamedCommandLookup hard check: the command MUST be registered
		if (int cmdId = SNM_NamedCommandLookup(_cmdStr, kbdSec, true))
	{
			// can't just rely on kbdSec->onAction() because some actions
			// depend on the current focused window, etc
			switch (_section)
			{
				case SNM_SEC_IDX_MAIN:
					return KBD_OnMainActionEx(cmdId, _val, _valhw, _relmode, _hwnd, NULL);
				case SNM_SEC_IDX_ME:
				case SNM_SEC_IDX_ME_EL:
					return MIDIEditor_LastFocused_OnCommand(cmdId, _section==SNM_SEC_IDX_ME_EL);
				case SNM_SEC_IDX_EPXLORER:
					if (HWND h = GetReaHwndByTitle(__localizeFunc("Media Explorer", "explorer", 0))) {
						SendMessage(h, WM_COMMAND, cmdId, 0);
						return 1;
					}
					return 0;
				default:
					return kbdSec->onAction(cmdId, _val, _valhw, _relmode, _hwnd);
			}
		}
		// custom console command?
		// note: authorized in any section
		else if (!_strnicmp(STATEMENT_CONSOLE, _cmdStr, strlen(STATEMENT_CONSOLE)))
		{
			RunConsoleCommand(_cmdStr+strlen(STATEMENT_CONSOLE)+1); // +1 for the space char in "CONSOLE cmd"
			if (!g_undos)
			{
				char undo[128];
				snprintf(undo, sizeof(undo), __LOCALIZE("ReaConsole command '%s'","sws_undo"), _cmdStr+strlen(STATEMENT_CONSOLE)+1);
				Undo_OnStateChangeEx2(NULL, undo, UNDO_STATE_ALL, -1);
			}
			return 1;
		}
		// label processor command
		else if (!_strnicmp(STATEMENT_LABEL, _cmdStr, strlen(STATEMENT_LABEL)))
		{
			WDL_FastString str(_cmdStr+strlen(STATEMENT_LABEL)+1); // +1 for the space char in "LABEL cmd"
			RunLabelCommand(&str);
			if (!g_undos)
			{
				char undo[128];
				snprintf(undo, sizeof(undo), __LOCALIZE("Label Processor command '%s'","sws_undo"), _cmdStr+strlen(STATEMENT_LABEL)+1);
				Undo_OnStateChangeEx2(NULL, undo, UNDO_STATE_ALL, -1); // do not use UNDO_STATE_ITEMS here
			}
			return 1;
		}
	}
	return 0;
}

// assumes the CA is valid (e.g. no recursion) + its statements are valid + etc..
// (faulty CAs must not be registered at this point, see CheckRegisterableCyclaction())
void RunCycleAction(COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd)
{
	int sec = _ct ? SNM_GetActionSectionIndex(_ct->uniqueSectionId) : -1;
	if (sec<0)
		return;

	Cyclaction* action = _ct ? g_cas[sec].Get((int)_ct->user-1) : NULL; // id is 1-based (for user display)
	if (!action || !action->m_cmdId) // registered actions only
		return;

	KbdSectionInfo* kbdSec = SNM_GetActionSection(sec);
	if (!kbdSec) 
		return;

	for (;;)
	{
		// store step or action name *before* m_performState update
		const char* undoStr = action->GetStepName();

		WDL_PtrList_DeleteOnDestroy<WDL_FastString> subCmds;
		if (ExplodeCyclaction(sec, _ct->id, &subCmds, NULL, NULL, 0x1, action) > 0) // 0x1!
		{
			int loopCnt = -1;
			WDL_PtrList<WDL_FastString> allCmds, loopCmds;
			for (int i=0; i<subCmds.GetSize(); i++)
			{
				const char* cmdStr = subCmds.Get(i) ? subCmds.Get(i)->Get() : "";
				if (IsCondStatement(cmdStr))
				{
					bool twoConds = IsTwoCondStatement(cmdStr);
					if ((i + (twoConds?2:1)) < subCmds.GetSize())
					{
						bool isON = (!_stricmp(STATEMENT_IF, cmdStr) ||
							!_stricmp(STATEMENT_IFAND, cmdStr) ||
							!_stricmp(STATEMENT_IFOR, cmdStr) ||
							!_stricmp(STATEMENT_IFXOR, cmdStr));

						const char* cmdStrIF = cmdStr;
						cmdStr = subCmds.Get(++i) ? subCmds.Get(i)->Get() : ""; //++i ! => zap next command
						int tgl = GetToggleCommandState2(kbdSec, SNM_NamedCommandLookup(cmdStr, kbdSec));
						
						if (twoConds)
						{
							cmdStr = subCmds.Get(++i) ? subCmds.Get(i)->Get() : ""; //++i ! => zap next command
							int tgl2 = GetToggleCommandState2(kbdSec, SNM_NamedCommandLookup(cmdStr, kbdSec));

							// tgl = overall toggle state value
							if (!_stricmp(STATEMENT_IFAND, cmdStrIF) || !_stricmp(STATEMENT_IFNAND, cmdStrIF))
								tgl = (tgl && tgl2) ? 1 : 0;
							else if (!_stricmp(STATEMENT_IFOR, cmdStrIF) || !_stricmp(STATEMENT_IFNOR, cmdStrIF))
								tgl = (tgl || tgl2) ? 1 : 0;
							else // if (!_stricmp(STATEMENT_IFXOR, cmdStrIF) || !_stricmp(STATEMENT_IFXNOR, cmdStrIF))
								tgl = (tgl ^ tgl2) ? 1 : 0;
						}

						if (tgl>=0)
						{
							if (isON ? tgl==0 : tgl==1)
							{
								// zap commands until next ELSE or ENDIF
								while (++i<subCmds.GetSize())
									if (subCmds.Get(i) && (!_stricmp(STATEMENT_ELSE, subCmds.Get(i)->Get()) || !_stricmp(STATEMENT_ENDIF, subCmds.Get(i)->Get())))
										break;
							}
						}
						// zap commands until next ENDIF
						else
						{
							while (++i<subCmds.GetSize())
								if (subCmds.Get(i) && !_stricmp(STATEMENT_ENDIF, subCmds.Get(i)->Get()))
									break;
						}
					}
					continue; // zap 
				}
				else if (!_stricmp(STATEMENT_ELSE, cmdStr))
				{
					// zap commands until next ELSE or ENDIF
					while (++i<subCmds.GetSize())
						if (subCmds.Get(i) && !_stricmp(STATEMENT_ENDIF, subCmds.Get(i)->Get()))
							break;
					continue;
				}
				else if (!_strnicmp(STATEMENT_LOOP, cmdStr, strlen(STATEMENT_LOOP)))
				{
					if (cmdStr[strlen(STATEMENT_LOOP)+1] == 'x' || cmdStr[strlen(STATEMENT_LOOP)+1] == 'X') {
						loopCnt = PromptForInteger(undoStr, __LOCALIZE("Number of times to repeat","sws_DLG_161"), 0, 4096, false);
						loopCnt++; // 0-based => 1-based + ignore the loop if user has cancelled
					}
					else
						loopCnt = atoi((char*)cmdStr+strlen(STATEMENT_LOOP)+1); // +1 for the space char in "LOOP n"
					continue;
				}
				else if (!_stricmp(STATEMENT_ENDLOOP, cmdStr))
				{
					if (loopCnt>=0)
					{
						for (int j=0; j<loopCnt; j++)
							for (int k=0; k<loopCmds.GetSize(); k++)
								if (loopCmds.Get(k))
									allCmds.Add(loopCmds.Get(k));

						loopCmds.Empty(false);
						loopCnt = -1;
					}
					continue;
				}
				else if (!_stricmp(STATEMENT_ENDIF, cmdStr))
					continue;

				if (*cmdStr)
				{
					if (loopCnt > 0)
						loopCmds.Add(subCmds.Get(i));
					else if (loopCnt == -1)
						allCmds.Add(subCmds.Get(i));
				}
			}

			if (allCmds.GetSize())
			{
#ifdef _SNM_DEBUG
				OutputDebugString("RunCycleAction: ");
				OutputDebugString(undoStr);
				OutputDebugString(" ---------->");
				OutputDebugString("\n");
#endif
				if (g_undos)
					Undo_BeginBlock2(NULL);

				if (g_preventUIRefresh)
					PreventUIRefresh(1);

				for (int i=0; i<allCmds.GetSize(); i++)
					PerformSingleCommand(sec, allCmds.Get(i)->Get(), _val, _valhw, _relmode, _hwnd);

				if (g_preventUIRefresh)
					PreventUIRefresh(-1);

				if (g_undos)
					Undo_EndBlock2(NULL, undoStr, UNDO_STATE_ALL);

				RefreshToolbar(0); // not strictly needed, except for toggle states of CAs calling other CAs
#ifdef _SNM_DEBUG
				OutputDebugString("RunCycleAction <-------------------------");
				OutputDebugString("\n");
#endif
				break;
			}
			// (try to) switch to the next action step if nothing has been
			// performed (avoids to run some CAs once before they sync properly)
			// note: m_performState is already updated via ExplodeCyclaction()
			else //JFB!! if (action->IsToggle()==2)
			{
				// cycled back to the 1st step?
				if (!action->m_performState)
					break;
			}
		} // if (ExplodeCyclaction())
	} // for(;;)
}

int IsCyclactionEnabled(COMMAND_T* _ct)
{
	int sec = _ct ? SNM_GetActionSectionIndex(_ct->uniqueSectionId) : -1;
	if (sec<0)
		return -1;

	Cyclaction* action = g_cas[sec].Get((int)_ct->user-1); // id is 1-based
	if (action && action->m_cmdId && action->IsToggle()) // registered toggle cycle actions only
	{
		if (action->IsToggle()==2) // real state?
		{
			// no recursion check, etc.. : such faulty cycle actions are not registered
			int tgl = ExplodeCyclaction(sec, _ct->id, NULL, NULL, NULL, 0x2, action);
			if (tgl>=0)
				return tgl;
		}

		// default case: fake toggle state
/*JFB commented: old code, it fails with odd numbers of cycle steps (e.g. a cycle action w/o any step, just cmds)
		return (action->m_performState % 2) != 0;
*/
		return action->m_fakeToggle;
	}
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
// Tests & error messages 
///////////////////////////////////////////////////////////////////////////////

bool CheckEditableCyclaction(const char* _actionStr, WDL_FastString* _errMsg, bool _allowEmpty = true)
{
	if (!(_actionStr && *_actionStr && *_actionStr!=CA_SEP && 
		!((_actionStr[0]==CA_TGL1 || _actionStr[0]==CA_TGL2) && _actionStr[1]==CA_SEP)))
	{
		if (_errMsg) {
			_errMsg->AppendFormatted(256, __LOCALIZE_VERFMT("Error: invalid cycle action '%s'","sws_DLG_161"), _actionStr ? _actionStr : "?");
			_errMsg->Append("\n\n");
		}
		return false;
	}
	if (!_allowEmpty && !strcmp(CA_EMPTY, _actionStr))
		return false; // no msg: empty cycle actions are internal stuff
	return true;
}

bool CheckToggle(int _section, Cyclaction* _a, int _cmdIdx)
{
	bool tglOk = false;
	const char* nextCmd = _a ? _a->GetCmd(_cmdIdx) : "";
	if (*nextCmd)
	{
		const auto actionType = GetActionType(nextCmd, false);

		if (strstr(nextCmd, "_CYCLACTION"))
		{
			if (Cyclaction* nextAction = GetCAFromCustomId(_section, nextCmd))
				tglOk = (nextAction->IsToggle() >= 0);
		}
		else if (actionType == ActionType::Custom || strstr(nextCmd, "_SWSCONSOLE_CUST"))
		{
			// macros & console cmds do not report any toggle state
		}
		else
		{
			if (!_section && actionType != ActionType::ReaScript)
			{
				KbdSectionInfo* kbdSec = SNM_GetActionSection(_section);
				tglOk = (GetToggleCommandState2(kbdSec, SNM_NamedCommandLookup(nextCmd, kbdSec)) >= 0);
			}
			// for other sections the toggle state can depend on focus, etc..
			// script set their toggle state at runtime...
			// => in doubt, we authorize it
			else
				tglOk = true;
		}
	}
	return tglOk;
}

bool AppendErrMsg(int _section, Cyclaction* _a, WDL_FastString* _outErrMsg, const char* _msg = NULL)
{
	if (_outErrMsg)
	{
		WDL_FastString errMsg;
		errMsg.AppendFormatted(256, 
			__LOCALIZE_VERFMT("ERROR: '%s' (section '%s') was not registered!","sws_DLG_161"), 
			_a ? _a->GetName() : __LOCALIZE("invalid cycle action","sws_DLG_161"), 
			SNM_GetActionSectionName(_section));
		if (_msg && *_msg)
		{
			errMsg.Append("\n");
			errMsg.Append(__LOCALIZE("Details:","sws_DLG_161"));
			errMsg.Append(" ");
			errMsg.Append(_msg);
		}
		errMsg.Append("\n\n");

		if (!strstr(_outErrMsg->Get(), errMsg.Get()))
			_outErrMsg->Insert(&errMsg, 0); // so that errors are on top, warnings at bottom
	}
	return false; // for facility
}

void AppendWarnMsg(int _section, Cyclaction* _a, WDL_FastString* _outWarnMsg, const char* _msg)
{
	if (_a && _outWarnMsg)
	{
		WDL_FastString warnMsg;
		warnMsg.AppendFormatted(256, 
			__LOCALIZE_VERFMT("Warning: '%s' (section '%s') has been registered but it could be improved!","sws_DLG_161"), 
			_a->GetName(), SNM_GetActionSectionName(_section));
		warnMsg.Append("\n");
		warnMsg.Append(__LOCALIZE("Details:","sws_DLG_161"));
		warnMsg.Append(" ");
		warnMsg.Append(_msg);
		warnMsg.Append("\n\n");

		if (!strstr(_outWarnMsg->Get(), warnMsg.Get()))
			_outWarnMsg->Append(&warnMsg);
	}
}

bool CheckRegisterableCyclaction(int _section, Cyclaction* _a, 
								 WDL_PtrList<WDL_FastString>* _macros, 
								 WDL_PtrList<WDL_FastString>* _consoles, 
								 WDL_FastString* _applyMsg)
{
	KbdSectionInfo* kbdSec = SNM_GetActionSection(_section);
	if (kbdSec && _a && !_a->IsEmpty())
	{
		WDL_FastString str;
		bool warned = false;
		int steps=0, statements=0, cmdSz=_a->GetCmdSize();
		for (int i=0; i<cmdSz; i++)
		{
			const char* cmd = _a->GetCmd(i);

			////////////////////////////////////////////////////////////////////////////
			// user errors?

			if (*cmd == '!')
			{
				steps++;

				if (i==0 || i==(cmdSz-1))
					return AppendErrMsg(_section, _a, _applyMsg, __LOCALIZE("cycle actions should not start/end with a step '!'","sws_DLG_161"));
				else if (_a->GetCmd(i+1)[0] == '!') // no bound issue here: checked above
					return AppendErrMsg(_section, _a, _applyMsg, __LOCALIZE("duplicated steps '!'","sws_DLG_161"));
			}
			else if (IsStatement(cmd)>=0)
			{
				if (!_strnicmp(STATEMENT_CONSOLE, cmd, strlen(STATEMENT_CONSOLE)))
				{
					// make a copy because ParseConsoleCommand modifies its input
					std::string cmdline{cmd + strlen(STATEMENT_CONSOLE)};
					char *trackid, *args;
					if (cmdline[0] != '\x20' || ParseConsoleCommand(&cmdline[1], &trackid, &args) == UNKNOWN_COMMAND) {
						str.SetFormatted(256, __LOCALIZE_VERFMT("%s must be followed by a valid ReaConsole command\nSee %s","sws_DLG_161"), STATEMENT_CONSOLE, SWS_URL_HELP_DIR"/reaconsole.php");
						return AppendErrMsg(_section, _a, _applyMsg, str.Get());
					}
				}
				else if (!_strnicmp(STATEMENT_LABEL, cmd, strlen(STATEMENT_LABEL)))
				{
					if (cmd[strlen(STATEMENT_LABEL)] != ' ' || strlen((char*)cmd+strlen(STATEMENT_LABEL)+1)<2) {
						str.SetFormatted(256, __LOCALIZE_VERFMT("%s must be followed by a valid Label Processor command\nSee Main menu > Extensions > Label Processor","sws_DLG_161"), STATEMENT_LABEL);
						return AppendErrMsg(_section, _a, _applyMsg, str.Get());
					}
				}
				else if (IsCondStatement(cmd))
				{
					statements++;

					bool twoConds = IsTwoCondStatement(cmd);
					bool tglOk = CheckToggle(_section, _a, i+1);
					bool tglOk2 = twoConds ? CheckToggle(_section, _a, i+2) : false;
					if (!twoConds && !tglOk) {
						str.SetFormatted(256, __LOCALIZE_VERFMT("%s and %s must be followed by an action that reports a toggle state\nTip: such actions report \"on\" or \"off\" in the column \"State\" of the Actions window","sws_DLG_161"), STATEMENT_IF, STATEMENT_IFNOT);
						return AppendErrMsg(_section, _a, _applyMsg, str.Get());
					}
					else if (twoConds && (!tglOk || !tglOk2)) {
						str.SetFormatted(256, __LOCALIZE_VERFMT("%s, %s, %s, %s, %s, and %s \nmust be followed by TWO actions that report a toggle state.\nTip: such actions report \"on\" or \"off\" in the column \"State\" of the Actions window","sws_DLG_161"), STATEMENT_IFAND, STATEMENT_IFNAND, STATEMENT_IFOR, STATEMENT_IFNOR, STATEMENT_IFXOR, STATEMENT_IFXNOR);
						return AppendErrMsg(_section, _a, _applyMsg, str.Get());
					}

					for (int j=i+1; j<cmdSz; j++)
					{
						if (!_stricmp(STATEMENT_ENDIF, _a->GetCmd(j)))
							break;
						else if (j==(cmdSz-1) || _a->GetCmd(j)[0]=='!') {
							str.SetFormatted(256, __LOCALIZE_VERFMT("missing %s","sws_DLG_161"), STATEMENT_ENDIF);
							return AppendErrMsg(_section, _a, _applyMsg, str.Get());
						}
						else if (IsCondStatement(_a->GetCmd(j))) {
							str.SetFormatted(256, __LOCALIZE_VERFMT("nested conditional statement (e.g. %s inside %s, etc)","sws_DLG_161"), STATEMENT_IF, STATEMENT_IFNOT);
							return AppendErrMsg(_section, _a, _applyMsg, str.Get());
						}
					}
				}
				else if (!_stricmp(STATEMENT_ELSE, cmd))
				{
					statements++;

					bool foundCond = false;
					for (int j=i-1; !foundCond && j>=0; j--)
					{
						if (IsCondStatement(_a->GetCmd(j)))
							foundCond = true;
						else if (!_stricmp(STATEMENT_ENDIF, _a->GetCmd(j)))
							break;
					}
					if (!foundCond) {
						str.SetFormatted(256, __LOCALIZE_VERFMT("unexpected %s","sws_DLG_161"), STATEMENT_ELSE);
						return AppendErrMsg(_section, _a, _applyMsg, str.Get());
					}

					for (int j=i; j<cmdSz; j++)
					{
						if (!_stricmp(STATEMENT_ENDIF, _a->GetCmd(j)))
							break;
						else if (j==(cmdSz-1) || _a->GetCmd(j)[0]=='!') {
							str.SetFormatted(256, __LOCALIZE_VERFMT("missing %s","sws_DLG_161"), STATEMENT_ENDIF);
							return AppendErrMsg(_section, _a, _applyMsg, str.Get());
						}
					}
				}
				else if (!_strnicmp(STATEMENT_LOOP, cmd, strlen(STATEMENT_LOOP)))
				{
					statements++;
					int loopCnt = (strlen(STATEMENT_LOOP)+1) < strlen(cmd) ? atoi((char*)cmd+strlen(STATEMENT_LOOP)+1) : 0;
					if (cmd[strlen(STATEMENT_LOOP)] != ' ' || 
						(loopCnt<2 && cmd[strlen(STATEMENT_LOOP)+1] != 'x' && cmd[strlen(STATEMENT_LOOP)+1] != 'X'))
					{
						str.SetFormatted(256, __LOCALIZE_VERFMT("%s n (with n>1), or %s x (prompt) is required","sws_DLG_161"), STATEMENT_LOOP, STATEMENT_LOOP);
						return AppendErrMsg(_section, _a, _applyMsg, str.Get());
					}
					
					if (i==(cmdSz-1)) {
						str.SetFormatted(256, __LOCALIZE_VERFMT("missing %s","sws_DLG_161"), STATEMENT_ENDLOOP);
						return AppendErrMsg(_section, _a, _applyMsg, str.Get());
					}

					for (int j=i+1; j<cmdSz; j++)
					{
						if (!_stricmp(STATEMENT_ENDLOOP, _a->GetCmd(j)))
							break;
						else if (j==(cmdSz-1) || _a->GetCmd(j)[0]=='!') {
							str.SetFormatted(256, __LOCALIZE_VERFMT("missing %s","sws_DLG_161"), STATEMENT_ENDLOOP);
							return AppendErrMsg(_section, _a, _applyMsg, str.Get());
						}
						else if (!_strnicmp(STATEMENT_LOOP, _a->GetCmd(j), strlen(STATEMENT_LOOP))) {
							str.SetFormatted(256, __LOCALIZE_VERFMT("nested %s","sws_DLG_161"), STATEMENT_LOOP);
							return AppendErrMsg(_section, _a, _applyMsg, str.Get());
						}
					}
				}
			}

			// only actions, macros & scripts at this point 
			else
			{
				// check numeric ids
				if (int tstNum = CheckSwsMacroScriptNumCustomId(cmd, _section))
				{
					str.SetFormatted(256, __LOCALIZE_VERFMT("unreliable command ID '%s'","sws_DLG_161"), cmd);
					str.Append("\n");

					// sws check
					if (tstNum==-1) {
						str.Append(__LOCALIZE("For SWS/S&M actions, you must use identifier strings (e.g. _SWS_ABOUT), not command IDs (e.g. 47145)\nTip: to copy such identifiers, right-click the action in the Actions window > Copy selected action cmdID/identifier string","sws_DLG_161"));
						return AppendErrMsg(_section, _a, _applyMsg, str.Get());
					}
					// macro/script check
					else if (tstNum==-2) {
						str.Append(__LOCALIZE("For macros/scripts, you must use identifier strings (e.g. _f506bc780a0ab34b8fdedb67ed5d3649), not command IDs (e.g. 47145)\nTip: to copy such identifiers, right-click the macro/script in the Actions window > Copy selected action cmdID/identifier string","sws_DLG_161"));
						return AppendErrMsg(_section, _a, _applyMsg, str.Get());
					}
				}

				// explode everything and do more checks: validity, non recursive, etc...
				// note: recursion via scripts is possible (not parsed) => we rely on hookCommandProc() and 
				//       toggleActionHook() checks to avoid any stack overflow, it is an user error anyway...
				WDL_PtrList_DeleteOnDestroy<WDL_FastString> parentCmds;
				switch (ExplodeCmd(_section, cmd, &parentCmds, _macros, _consoles, 0x8))
				{
					case -1:
						str.SetFormatted(256, __LOCALIZE_VERFMT("unknown command ID or identifier string '%s'","sws_DLG_161"), cmd);
						return AppendErrMsg(_section, _a, _applyMsg, str.Get());
					case -2:
						return AppendErrMsg(_section, _a, _applyMsg, __LOCALIZE("recursive action (i.e. uses itself directly or indirectly)","sws_DLG_161"));
					case -3:
						return AppendErrMsg(_section, _a, _applyMsg, __LOCALIZE("cross-section cycle action","sws_DLG_161"));
				}

				if (!SNM_NamedCommandLookup(cmd, kbdSec)) {
					str.SetFormatted(256, __LOCALIZE_VERFMT("unknown command ID or identifier string '%s'","sws_DLG_161"), cmd);
					return AppendErrMsg(_section, _a, _applyMsg, str.Get());
				}
			}

			///////////////////////////////////////////////////////////////////
			// warnings?

			if (_applyMsg)
			{
				if (!warned && // not already warned?
					(strstr(cmd, "_CYCLACTION") ||
					 strstr(cmd, "SWSCONSOLE_CUST") ||
					 GetMacroOrScript(cmd, kbdSec->uniqueID, _macros, NULL) == 1)) // macros only, brutal but works for all sections
				{
					str.SetFormatted(256, __LOCALIZE_VERFMT("the identifier string '%s' cannot be shared with other users","sws_DLG_161"), cmd);
					str.Append("\n");
					str.AppendFormatted(256, __LOCALIZE_VERFMT("Tip: right-click this command > '%s'","sws_DLG_161"), __LOCALIZE("Explode into individual actions","sws_DLG_161"));
					AppendWarnMsg(_section, _a, _applyMsg, str.Get());

					// don't return false here, just a warning
					warned = true;
				}
			}

		} // for()

		if ((steps+statements) == cmdSz)
			return AppendErrMsg(_section, _a, _applyMsg, __LOCALIZE("no valid command found","sws_DLG_161"));
	}
	else
		return AppendErrMsg(_section, NULL, _applyMsg);
	return true;
}


///////////////////////////////////////////////////////////////////////////////
// Load, save and registration
///////////////////////////////////////////////////////////////////////////////

// primitive, see other RegisterCyclation() below
// returns the cmd id, or 0 if failed
// _cmdId: id to re-use or 0 to register a new cmd id
// _cycleId: 1-based
// note: no Cyclaction* param because of dynamic action renaming (deprecated)
int RegisterCyclation(const char* _name, int _section, int _cycleId, int _cmdId)
{
	char custId[SNM_MAX_ACTION_CUSTID_LEN]="";
	if (snprintfStrict(custId, sizeof(custId), "%s%d", GetCACustomId(_section), _cycleId) > 0)
	{
		return SWSCreateRegisterDynamicCmd(
			SNM_GetActionSectionUniqueId(_section),
			_cmdId,
			NULL,
			RunCycleAction,
			IsCyclactionEnabled,
			custId,
			_name,
			NULL,
			_cycleId,
			__FILE__,
			false); // no localization for cycle actions (name defined by the user)
	}
	return 0;
}

// register a CA recursively, i.e. register sub-CAs and then the parent CA
// mandatory for CheckRegisterableCyclaction() that would fail otherwise
int RegisterCyclation(Cyclaction* _a, int _section, int _cycleId,
					  WDL_PtrList_DeleteOnDestroy<WDL_FastString>* _macros,
					  WDL_PtrList_DeleteOnDestroy<WDL_FastString>* _consoles,
					  WDL_FastString* _applyMsg)
{
	if (_a && !_a->IsEmpty())
	{
		if (_a->m_cmdId)
			return _a->m_cmdId;

		static WDL_PtrList<Cyclaction> sSubCAs;
		sSubCAs.Add(_a);
		for (int i=0; i<_a->GetCmdSize(); i++)
		{
			int cycleId;
			if (Cyclaction* a = GetCAFromCustomId(_section, _a->GetCmd(i), &cycleId)) // works even is CA is not registered yet
			{
				if (sSubCAs.Find(a) == -1) {
					a->m_cmdId = RegisterCyclation(a, _section, cycleId, _macros, _consoles, _applyMsg);
					if (!a->m_cmdId) break; // simple break for sSubCAs cleanup + _applyMsg update
				}
				else break; // recursice CA! simple break for sSubCAs cleanup + _applyMsg update
			}
		}
		sSubCAs.Delete(sSubCAs.Find(_a));

		if (CheckRegisterableCyclaction(_section, _a, _macros, _consoles, _applyMsg))
		{
			const int cmdid = RegisterCyclation(_a->GetName(), _section, _cycleId, 0);
			if (!cmdid && _applyMsg)
				AppendErrMsg(_section, _a, _applyMsg, __LOCALIZE("failed to be registered in the action list (you may have hit the global action limit)","sws_DLG_161"));
			return cmdid;
		}
	}
	return 0;
}

void FlushCyclactions(int _section)
{
	for (int i=0; i<g_cas[_section].GetSize(); i++)
		if (Cyclaction* a = g_cas[_section].Get(i)) {
			SWSFreeUnregisterDynamicCmd(a->m_cmdId);
			a->m_cmdId = 0;
		}
	g_cas[_section].EmptySafe(true);
}

// _cyclactions: NULL to add/register to the main model, imports into _cyclactions otherwise
// _section: section index or -1 for all sections
// _iniFn: NULL => S&M.ini
// remark: undo pref is ignored, only loads cycle actions so that the user keeps his own undo pref
void LoadCyclactions(bool _wantMsg, WDL_PtrList<Cyclaction>* _cyclactions = NULL, int _section = -1, const char* _iniFn = NULL)
{
	WDL_FastString msg;
	char buf[32] = "", actionBuf[CA_MAX_LEN] = "";
	WDL_PtrList_DeleteOnDestroy<WDL_FastString> macros, consoles;
	for (int sec=0; sec<SNM_MAX_CA_SECTIONS; sec++)
	{
		if (_section == sec || _section == -1)
		{
			if (!_cyclactions)
				FlushCyclactions(sec);

			int nb = GetPrivateProfileInt(GetCAIniSection(sec), "Nb_Actions", 0, _iniFn ? _iniFn : g_SNM_CyclIniFn.Get());
			int ver = GetPrivateProfileInt(GetCAIniSection(sec), "Version", 1, _iniFn ? _iniFn : g_SNM_CyclIniFn.Get());
			for (int j=0; j<nb; j++)
			{
				if (snprintfStrict(buf, sizeof(buf), "Action%d", j+1) > 0)
				{
					GetPrivateProfileString(GetCAIniSection(sec), buf, CA_EMPTY, actionBuf, sizeof(actionBuf), _iniFn ? _iniFn : g_SNM_CyclIniFn.Get());
					
					// upgrade?
					if (*actionBuf && ver<CA_VERSION)
					{
						int i=-1;
						while (actionBuf[++i])
						{
							if (actionBuf[i]==CA_SEP_V1
#ifdef _WIN32 // file would be eff'd up on OSX anyway, too late...
								|| actionBuf[i]==CA_SEP_V2
#endif
							)
							{
								actionBuf[i]=CA_SEP;
							}
						}
					}

					// import into _cyclactions
					if (_cyclactions)
					{
						if (CheckEditableCyclaction(actionBuf, _wantMsg ? &msg : NULL, false))
							_cyclactions[sec].Add(new Cyclaction(actionBuf, true));
					}
					// 1st pass: add CAs to main model
					// (registered later, in a 2nd pass, because of possible cross-references)
					else
					{
						if (CheckEditableCyclaction(actionBuf, _wantMsg ? &msg : NULL, false))
							g_cas[sec].Add(new Cyclaction(actionBuf));
						else
							g_cas[sec].Add(new Cyclaction(CA_EMPTY)); // failed => adds a no-op cycle action in order to preserve cycle action ids
					}
				}
			}

			// 2nd pass: now that all CAs have been added, (try to) register them recursively
			// as CAs can call other CAs in any order (i.e. not in g_cas[sec]'s order)
			if (!_cyclactions)
				for (int j=0; j<g_cas[sec].GetSize(); j++)
					if (Cyclaction* a = g_cas[sec].Get(j))
						a->m_cmdId = RegisterCyclation(a, sec, j+1, &macros, &consoles, _wantMsg ? &msg : NULL); // recursive
		}
	}

	if (_wantMsg && msg.GetLength())
		SNM_ShowMsg(msg.Get(), __LOCALIZE("S&M - Warning", "sws_DLG_161"), g_caWndMgr.GetMsgHWND());

	RefreshToolbar(0);
}

// _cyclactions: NULL to update main model
// _section: section index or -1 for all sections
// _iniFn: NULL means S&M.ini
// remark: undo pref ignored, only saves cycle actions
void SaveCyclactions(WDL_PtrList<Cyclaction>* _cyclactions = NULL, int _section = -1, const char* _iniFn = NULL)
{
	if (!_cyclactions)
		_cyclactions = g_cas;

	for (int sec=0; sec < SNM_MAX_CA_SECTIONS; sec++)
	{
		if (_section == sec || _section == -1)
		{
			WDL_PtrList_DeleteOnDestroy<int> freeCycleIds;
			WDL_FastString iniSection("; Do not tweak by hand! Use the Cycle Action editor instead\n"); // no localization for ini files..

			// prepare ids "compression" (i.e. will re-assign ids of new actions for the next load)
			for (int j=0; j < _cyclactions[sec].GetSize(); j++)
				if (_cyclactions[sec].Get(j)->IsEmpty())
					freeCycleIds.Add(new int(j));

			int maxId = 0;
			for (int j=0; j < _cyclactions[sec].GetSize(); j++)
			{
				Cyclaction* a = _cyclactions[sec].Get(j);
				if (!_cyclactions[sec].Get(j)->IsEmpty()) // skip empty cyclactions
				{
					if (!a->m_added)
					{
						iniSection.AppendFormatted(CA_MAX_LEN, "Action%d=\"%s\"\n", j+1, a->GetDefinition());
						maxId = max(j+1, maxId);
					}
					else
					{
						a->m_added = false;
						if (freeCycleIds.GetSize())
						{
							int id = *(freeCycleIds.Get(0));
							iniSection.AppendFormatted(CA_MAX_LEN, "Action%d=\"%s\"\n", id+1, a->GetDefinition());
							freeCycleIds.Delete(0, true);
							maxId = max(id+1, maxId);
						}
						else
						{
							iniSection.AppendFormatted(CA_MAX_LEN, "Action%d=\"%s\"\n", ++maxId, a->GetDefinition()); 
						}
					}
				}
			}
			// "Nb_Actions" is a bad name now: it is a max id (kept for ascendant comp.)
			iniSection.AppendFormatted(32, "Nb_Actions=%d\n", maxId);
			iniSection.AppendFormatted(32, "Version=%d\n", CA_VERSION);
			SaveIniSection(GetCAIniSection(sec), &iniSection, _iniFn ? _iniFn : g_SNM_CyclIniFn.Get());
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// Cyclaction
///////////////////////////////////////////////////////////////////////////////

int Cyclaction::GetStepCount()
{
	int steps=1;
	for (int i=0; i<GetCmdSize(); i++)
		if (*GetCmd(i) == '!')
			steps++;
	return steps;
}

// get the 1st valid command index of the current cycle point (where "valid" means index+1 to skip the current step command '!')
// or 0 for the initial state (or cycling back to it) or -1 if failed
int Cyclaction::GetStepIdx(int _performState)
{
	int performState = (_performState<0 ? m_performState : _performState);
	int idx=0, sz=GetCmdSize(), state=0;
	while (idx<sz && state<performState)
		if (*GetCmd(idx++) == '!')
			state++;
	return idx<sz ? idx : -1;
}

const char* Cyclaction::GetStepName(int _performState)
{
	int cmdIdx = GetStepIdx(_performState);
	if (cmdIdx>0) {
		const char* stepName = GetCmd(cmdIdx-1)+1; // -1: see GetStepIdx(), +1: to skip '!'
		if (*stepName) return stepName;
	}
	return GetName();
}

void Cyclaction::SetToggle(int _toggle)
{
	if (_toggle==0)
	{
		if (IsToggle())
			m_def.DeleteSub(0,1);
	}
	else if (_toggle==1 || _toggle==2)
	{
		if (IsToggle())
			m_def.DeleteSub(0,1);
		m_def.Insert(_toggle==1 ? s_CA_TGL1_STR : s_CA_TGL2_STR, 0, 1);
	}
}

void Cyclaction::SetCmd(WDL_FastString* _cmd, const char* _newCmd)
{
	int i = m_cmds.Find(_cmd);
	if (_cmd && _newCmd && i >= 0) {
		m_cmds.Get(i)->Set(_newCmd);
		UpdateFromCmd();
	}
}

WDL_FastString* Cyclaction::AddCmd(const char* _cmd, int _pos)
{
	if (WDL_FastString* cmd = new WDL_FastString(_cmd))
	{
		if (_pos>=0) m_cmds.Insert(_pos, cmd); 
		else m_cmds.Add(cmd); 
		UpdateFromCmd(); 
		return cmd;
	}
	return NULL;
}

void Cyclaction::RemoveCmd(WDL_FastString* _cmd, bool _wantDelete)
{
	m_cmds.Delete(m_cmds.Find(_cmd), _wantDelete); 
	UpdateFromCmd(); 
}

void Cyclaction::ReplaceCmd(WDL_FastString* _cmd, bool _wantDelete, WDL_PtrList<WDL_FastString>* _newCmds)
{
	int idx = m_cmds.Find(_cmd);
	if (idx>=0) 
	{
		m_cmds.Delete(idx, _wantDelete);
		for (int j=0; _newCmds && j<_newCmds->GetSize(); j++)
			m_cmds.Insert(idx++, new WDL_FastString(_newCmds->Get(j)->Get()));
		UpdateFromCmd();
	}
}

void Cyclaction::UpdateNameAndCmds()
{
	m_cmds.EmptySafe(false); // to be deleted by callers (might be used in a list view)

	char actionStr[CA_MAX_LEN] = "";
	lstrcpyn_safe(actionStr, m_def.Get(), sizeof(actionStr));
	char* tok = strtok(actionStr, s_CA_SEP_STR);
	if (tok)
	{
		// 1st token = cycle action name (#name = toggle action)
		m_name.Set(*tok==CA_TGL1 || *tok==CA_TGL2 ? (const char*)tok+1 : tok);
		while ((tok = strtok(NULL, s_CA_SEP_STR)))
			m_cmds.Add(new WDL_FastString(tok));
	}
}

void Cyclaction::UpdateFromCmd()
{
	WDL_FastString newDef;
	if (int tgl=IsToggle())
		newDef.SetFormatted(CA_MAX_LEN, "%c", tgl==1?CA_TGL1:CA_TGL2);
	newDef.Append(GetName());
	newDef.Append(s_CA_SEP_STR);
	for (int i=0; i < m_cmds.GetSize(); i++) {
		newDef.Append(m_cmds.Get(i)->Get());
		newDef.Append(s_CA_SEP_STR);
	}
	m_def.Set(&newDef);
}

int Cyclaction::GetIndent(WDL_FastString* _cmd)
{
	int indent=0;
	for(int i=0; i<m_cmds.GetSize(); i++)
	{
		if (WDL_FastString* cmd = m_cmds.Get(i))
		{
			if (!_stricmp(cmd->Get(), STATEMENT_ELSE) || 
				!_stricmp(cmd->Get(), STATEMENT_ENDIF) ||
				!_stricmp(cmd->Get(), STATEMENT_ENDLOOP))
			{
				indent-=4;
			}

			if (cmd == _cmd)
				break;

			if (IsCondStatement(cmd->Get()) ||
				!_stricmp(cmd->Get(), STATEMENT_ELSE) ||
				!_strnicmp(STATEMENT_LOOP, cmd->Get(), strlen(STATEMENT_LOOP)))
			{
				indent+=4;
			}
		}
	}
	return indent;
}

///////////////////////////////////////////////////////////////////////////////
// GUI
///////////////////////////////////////////////////////////////////////////////

enum {
  ADD_CYCLACTION_MSG = 0xF001,
  DEL_CYCLACTION_MSG,
  RUN_CYCLACTION_MSG,
  CUT_CMD_MSG,
  COPY_CMD_MSG,
  PASTE_CMD_MSG,
  LEARN_CMD_MSG,
  DEL_CMD_MSG,
  EXPLODE_CMD_MSG,
  ADD_CMD_MSG,
  ADD_STEP_CMD_MSG,
  ADD_STATEMENT_MSG,                                        // --> statement cmds
  IMPORT_CUR_SECTION_MSG = ADD_STATEMENT_MSG+NB_STATEMENTS, // <--
  IMPORT_ALL_SECTIONS_MSG,
  EXPORT_SEL_MSG,
  EXPORT_CUR_SECTION_MSG,
  EXPORT_ALL_SECTIONS_MSG,
  RESET_CUR_SECTION_MSG,
  RESET_ALL_SECTIONS_MSG,
  LEARN_CYCLACTION_MSG,
  LAST_MSG // keep as last item!
};

enum {
  CMBID_SECTION = LAST_MSG,
  TXTID_SECTION,
  BTNID_UNDO,
  BTNID_PREVENTUIREFRESH,
  BTNID_APPLY,
  BTNID_CANCEL,
  BTNID_IMPEXP,
  BTNID_ACTIONLIST,
  WNDID_LR,
  BTNID_L,
  BTNID_R
};


// no default filter text on OSX (cannot catch EN_SETFOCUS/EN_KILLFOCUS)
#ifndef _SNM_SWELL_ISSUES
#define FILTER_DEFAULT_STR		__LOCALIZE("Filter","sws_DLG_161")
#else
#define FILTER_DEFAULT_STR		""
#endif


// note the Id column is not "over the top":
// - esases edition when the list view is sorted by ids instead of names
// - helps to distinguish new actions (id = *)
// - dbl-click ids to perform cycle actions

// !WANT_LOCALIZE_STRINGS_BEGIN:sws_DLG_161
static SWS_LVColumn s_casCols[] = { { 50, 0, "Id" }, { 260, 1, "Cycle action name" }, { 50, 2, "Toggle" } };
static SWS_LVColumn s_commandsCols[] = { { 180, 1, "Command" }, { 180, 0, "Description" } };
// !WANT_LOCALIZE_STRINGS_END

enum {
  COL_L_ID=0,
  COL_L_NAME,
  COL_L_TOGGLE,
  COL_L_COUNT
};

enum {
  COL_R_CMD=0,
  COL_R_NAME,
  COL_R_COUNT
};

// for cross-calls between list views
CyclactionsView* g_lvL = NULL;
CommandsView* g_lvR = NULL;
RECT g_origRectL = {0,0,0,0};
RECT g_origRectR = {0,0,0,0};
int g_lvLastState = 0, g_lvState = 0; // 0=display both list views, 1=left only, -1=right only

// fake list views' items (help items), init + localization in CyclactionInit()
static Cyclaction s_DEFAULT_L;
static WDL_FastString s_EMPTY_R;
static WDL_FastString s_DEFAULT_R;

WDL_FastString g_caFilter; // init + localization in CyclactionInit()

WDL_PtrList<Cyclaction> g_editedActions[SNM_MAX_CA_SECTIONS];
WDL_PtrList<WDL_FastString> g_clipboardCmds;
Cyclaction* g_editedAction = NULL;
int g_editedSection = 0; // edited section index
bool g_edited = false;
char g_lastExportFn[SNM_MAX_PATH] = "";
char g_lastImportFn[SNM_MAX_PATH] = "";


bool IsCAFiltered() {
	return g_caFilter.GetLength() && strcmp(g_caFilter.Get(), FILTER_DEFAULT_STR);
}

int CountEditedActions() {
	int count = 0;
	for (int i=0; i < g_editedActions[g_editedSection].GetSize(); i++)
		if(!g_editedActions[g_editedSection].Get(i)->IsEmpty())
			count++;
	return count;
}

void UpdateEditedStatus(bool _edited) {
	g_edited = _edited;
	if (CyclactionWnd* w = g_caWndMgr.Get())
		w->Update(false);
}

void UpdateListViews() {
	if (g_lvL) g_lvL->Update();
	if (g_lvR) g_lvR->Update();
}

void AllEditListItemEnd(bool _save) {
	bool edited = g_edited;
	if (g_lvL && g_lvL->EditListItemEnd(_save))
		edited = true;
	if (g_lvR && g_lvR->EditListItemEnd(_save))
		edited = true;
	UpdateEditedStatus(edited);// just to make sure..
}

void EditModelInit()
{
	// keep pointers (may be used in a listview: delete after listview update)
	WDL_PtrList_DeleteOnDestroy<Cyclaction> actionsToDelete;
	for (int i=0; i < g_editedActions[g_editedSection].GetSize(); i++)
		actionsToDelete.Add(g_editedActions[g_editedSection].Get(i));

	// model init (we display/edit copies for apply/cancel)
	for (int sec=0; sec < SNM_MAX_CA_SECTIONS; sec++)
	{
		g_editedActions[sec].EmptySafe(g_editedSection != sec); // keep current (displayed!) pointers
		for (int i=0; i < g_cas[sec].GetSize(); i++)
			g_editedActions[sec].Add(new Cyclaction(g_cas[sec].Get(i)));
	}
	g_editedAction = NULL;
	UpdateListViews();
} // + actionsToDelete auto clean-up!

void Apply()
{
	SNM_CloseMsg(); // close last error message, just in case

	// save/register cycle actions
	int oldCycleId = g_editedActions[g_editedSection].Find(g_editedAction);
	AllEditListItemEnd(true);
	bool wasEdited = g_edited;
	UpdateEditedStatus(false); // ok, apply: eof edition, g_edited==false then
	SaveCyclactions(g_editedActions);
#ifdef _WIN32
	// force ini file cache refresh
	// see http://support.microsoft.com/kb/68827 & http://github.com/reaper-oss/sws/issues/397
	WritePrivateProfileString(NULL, NULL, NULL, g_SNM_CyclIniFn.Get());
#endif
	LoadCyclactions(wasEdited); // + flush, unregister, re-register
	EditModelInit();

	if (oldCycleId>=0)
		if (Cyclaction* a = g_editedActions[g_editedSection].Get(oldCycleId))
			g_lvL->SelectByItem((SWS_ListItem*)a, true, true);
}

void Cancel(bool _checkSave)
{
	int oldCycleId = g_editedActions[g_editedSection].Find(g_editedAction);

	AllEditListItemEnd(false);

	// not used ATM: _checkSave is always false
	if (_checkSave && g_edited && 
			IDYES == MessageBox(g_caWndMgr.GetMsgHWND(),
				__LOCALIZE("Save cycle actions before quitting editor ?","sws_DLG_161"),
				__LOCALIZE("S&M - Confirmation","sws_DLG_161"), MB_YESNO))
	{
		SaveCyclactions(g_editedActions);
#ifdef _WIN32
		// force ini file cache refresh
		// see http://support.microsoft.com/kb/68827 & http://github.com/reaper-oss/sws/issues/397
		WritePrivateProfileString(NULL, NULL, NULL, g_SNM_CyclIniFn.Get());
#endif
		LoadCyclactions(true); // + flush, unregister, re-register
	}
	UpdateEditedStatus(false); // cancel: eof edition
	EditModelInit();

	if (oldCycleId>=0)
		if (Cyclaction* a = g_editedActions[g_editedSection].Get(oldCycleId))
			g_lvL->SelectByItem((SWS_ListItem*)a, true, true);
}

void ResetSection(int _section)
{
	// keep pointers (may be used in a listview): deleted after listview update
	WDL_PtrList_DeleteOnDestroy<Cyclaction> actionsToDelete;
	if (_section == g_editedSection)
		for (int i=0; i < g_editedActions[_section].GetSize(); i++)
			actionsToDelete.Add(g_editedActions[_section].Get(i));

	g_editedActions[_section].EmptySafe(_section != g_editedSection);

	if (_section == g_editedSection)
	{
		g_editedAction = NULL;
		UpdateListViews();
	}
	UpdateEditedStatus(true);
} // + actionsToDelete auto clean-up!


////////////////////
// Left list view
////////////////////

CyclactionsView::CyclactionsView(HWND hwndList, HWND hwndEdit)
:SWS_ListView(hwndList, hwndEdit, COL_L_COUNT, s_casCols, "LCyclactionViewState", false, "sws_DLG_161") {}

void CyclactionsView::GetItemText(SWS_ListItem* item, int iCol, char* str, int iStrMax)
{
	if (str) *str = '\0';
	if (Cyclaction* a = (Cyclaction*)item)
	{
		switch (iCol)
		{
			case COL_L_ID:
				if (!a->m_added)
				{
					int cycleId = g_editedActions[g_editedSection].Find(a);
					if (cycleId >= 0)
					{
						if (a->m_cmdId)
							snprintf(str, iStrMax, "%5.d", cycleId+1);
						else
							snprintf(str, iStrMax, "(%d)", cycleId+1);
					}
				}
				else
					lstrcpyn(str, "*", iStrMax);
				break;
			case COL_L_NAME:
				lstrcpyn(str, a->GetName(), iStrMax);
				break;
			case COL_L_TOGGLE:
				if (a->IsToggle()==1)
					lstrcpyn(str, UTF8_CIRCLE, iStrMax);
				else if (a->IsToggle()==2)
					lstrcpyn(str, UTF8_BULLET, iStrMax);
				break;
		}
	}
}

//JFB cancel cell editing on error would be great.. SWS_ListView mod?
void CyclactionsView::SetItemText(SWS_ListItem* _item, int _iCol, const char* _str)
{
	if (_iCol == COL_L_NAME)
	{
		if (strchr(_str, CA_TGL1) || strchr(_str, CA_TGL2) || strchr(_str, CA_SEP) || 
			strchr(_str, '\"') || strchr(_str, '\'') || strchr(_str, '`'))
		{
			WDL_FastString msg(__LOCALIZE("Cycle action names cannot contain any of the following characters:","sws_DLG_161"));
			msg.AppendFormatted(256, "%c %c %c \" ' ` ", CA_TGL1, CA_TGL2, CA_SEP);
			MessageBox(g_caWndMgr.GetMsgHWND(), msg.Get(), __LOCALIZE("S&M - Error","sws_DLG_161"), MB_OK);
			return;
		}

		Cyclaction* a = (Cyclaction*)_item;
		if (a && a != &s_DEFAULT_L)
		{
			WDL_FastString errMsg;
			if (!CheckEditableCyclaction(_str, &errMsg) && strcmp(a->GetName(), _str)) {
				if (errMsg.GetLength())
					MessageBox(g_caWndMgr.GetMsgHWND(), errMsg.Get(), __LOCALIZE("S&M - Error","sws_DLG_161"), MB_OK);
			}
			else {
				a->SetName(_str);
				UpdateEditedStatus(true);
			}
			// no update on cell editing (disabled)
		}
	}
}

bool CyclactionsView::IsEditListItemAllowed(SWS_ListItem* _item, int _iCol) {
	Cyclaction* a = (Cyclaction*)_item;
	return (a != &s_DEFAULT_L);
}

void CyclactionsView::GetItemList(SWS_ListItemList* pList)
{
	if (CountEditedActions())
	{
		if (IsCAFiltered())
		{
			LineParser lp(false);
			if (!lp.parse(g_caFilter.Get()))
			{
				for (int i=0; i < g_editedActions[g_editedSection].GetSize(); i++)
					if (Cyclaction* a = (Cyclaction*)g_editedActions[g_editedSection].Get(i))
					{
						bool match = true;
						for (int j=0; match && j<lp.getnumtokens(); j++)
							if (!stristr(a->GetName(), lp.gettoken_str(j)))
								match = false;
						if (match)
							pList->Add((SWS_ListItem*)a);
					}
			}
		}
		else
		{
			for (int i=0; i < g_editedActions[g_editedSection].GetSize(); i++)
				if (Cyclaction* a = (Cyclaction*)g_editedActions[g_editedSection].Get(i))
					if (!a->IsEmpty())
						pList->Add((SWS_ListItem*)a);
		}
	}
	else
		pList->Add((SWS_ListItem*)&s_DEFAULT_L);
}

void CyclactionsView::OnItemSelChanged(SWS_ListItem* item, int iState)
{
	AllEditListItemEnd(true);

	Cyclaction* action = (Cyclaction*)item;
/*JFB fails on OSX due to notifs order
	g_editedAction = (item && iState ? action : NULL);
*/
	if (iState && action != g_editedAction)
		g_editedAction = action;
	else if (!iState && action == g_editedAction)
		g_editedAction = NULL;

	g_lvR->Update();
}

void CyclactionsView::OnItemBtnClk(SWS_ListItem* item, int iCol, int iKeyState)
{
	Cyclaction* pItem = (Cyclaction*)item;
	if (pItem && pItem != &s_DEFAULT_L && iCol == COL_L_TOGGLE)
	{
		int tgl = pItem->IsToggle();
		pItem->SetToggle(tgl<2?tgl+1:0);
		Update();
		UpdateEditedStatus(true);
	}
}

void CyclactionsView::OnItemDblClk(SWS_ListItem* item, int iCol)
{
	Cyclaction* pItem = (Cyclaction*)item;
	if (pItem && pItem != &s_DEFAULT_L && iCol == COL_L_ID)
		if (CyclactionWnd* w = g_caWndMgr.Get())
			w->OnCommand(RUN_CYCLACTION_MSG, 0);
}


////////////////////
// Right list view
////////////////////

CommandsView::CommandsView(HWND hwndList, HWND hwndEdit)
:SWS_ListView(hwndList, hwndEdit, COL_R_COUNT, s_commandsCols, "RCyclactionViewState", false, "sws_DLG_161", false)
{
//	SetWindowLongPtr(hwndList, GWL_STYLE, GetWindowLongPtr(hwndList, GWL_STYLE) | LVS_SINGLESEL);
}

void CommandsView::GetItemText(SWS_ListItem* item, int iCol, char* str, int iStrMax)
{
	if (str) *str = '\0';
	if (WDL_FastString* pItem = (WDL_FastString*)item)
	{
		switch (iCol)
		{
			case COL_R_CMD:
				lstrcpyn(str, pItem->Get(), iStrMax);
				break;
			case COL_R_NAME:
				if (pItem == &s_EMPTY_R || pItem == &s_DEFAULT_R)
					return;
				if (pItem->GetLength())
				{
					WDL_FastString txt;
					int indent = g_editedAction->GetIndent(pItem);
					for (int i=0; i<indent; i++)
						txt.Append(" ");

					if (pItem->Get()[0] == '!') {
						txt.Append(__LOCALIZE("----- Step -----","sws_DLG_161")); lstrcpyn(str, txt.Get(), iStrMax);
						return;
					}

					int instIdx = IsStatement(pItem->Get());
					if (instIdx>=0) {
						txt.Append(__localizeFunc(g_statementInfos[instIdx],"sws_DLG_161",LOCALIZE_FLAG_NOCACHE)); 
						lstrcpyn(str, txt.Get(), iStrMax);
						return;
					}

					if (g_editedAction)
					{
						// won't work for cross CAs that are not yet registered, that's probably better anyway..
						if (KbdSectionInfo* kbdSec = SNM_GetActionSection(g_editedSection))
						{
							if (int cmdId = SNM_NamedCommandLookup(pItem->Get(), kbdSec))
							{
//								if (!CheckSwsMacroScriptNumCustomId(pItem->Get(), g_editedSection))
								{
									const char* name = SNM_GetTextFromCmd(cmdId, kbdSec);
									if (name && *name) {
										txt.Append(name); lstrcpyn(str, txt.Get(), iStrMax);
										return;
									}
								}
							}
						}
						txt.Append(__LOCALIZE("Unknown","sws_DLG_161"));
						lstrcpyn(str, txt.Get(), iStrMax);
					}
				}
				break;
		}
	}
}

void CommandsView::SetItemText(SWS_ListItem* _item, int _iCol, const char* _str)
{
	if (_iCol == COL_R_CMD)
	{
		if (strchr(_str, CA_SEP)) 
		{
			WDL_FastString msg(__LOCALIZE("Commands cannot contain the character: ","sws_DLG_161"));
			msg.Append(s_CA_SEP_STR);
			MessageBox(GetMainHwnd(), msg.Get(), __LOCALIZE("S&M - Error","sws_DLG_161"), MB_OK);
			return;
		}

		WDL_FastString* cmd = (WDL_FastString*)_item;
		if (cmd && _str && cmd != &s_EMPTY_R && cmd != &s_DEFAULT_R && g_editedAction && strcmp(cmd->Get(), _str))
		{
			g_editedAction->SetCmd(cmd, _str);

			char buf[128] = "";
			GetItemText(_item, 1, buf, sizeof(buf));
			ListView_SetItemText(m_hwndList, GetEditingItem(), DisplayToDataCol(1), buf);
			// ^^ direct GUI update 'cause Update() disabled during cell editing

			UpdateEditedStatus(true);
		}
	}
}

bool CommandsView::IsEditListItemAllowed(SWS_ListItem* _item, int _iCol) {
	WDL_FastString* cmd = (WDL_FastString*)_item;
	return (cmd && _iCol == COL_R_CMD && cmd != &s_EMPTY_R && cmd != &s_DEFAULT_R);
}

void CommandsView::GetItemList(SWS_ListItemList* pList)
{
	if (g_editedAction)
	{
		if (int nb = g_editedAction->GetCmdSize())
			for (int i=0; i < nb; i++)
				pList->Add((SWS_ListItem*)g_editedAction->GetCmdString(i));
		else if (CountEditedActions())
			pList->Add((SWS_ListItem*)&s_DEFAULT_R);
	}
	else if (CountEditedActions())
		pList->Add((SWS_ListItem*)&s_EMPTY_R);
}

// "disable" sort
int CommandsView::OnItemSort(SWS_ListItem* _item1, SWS_ListItem* _item2) 
{
	if (g_editedAction)
	{
		// not optimized.. but macros are not THAT big anyway
		int i1 = g_editedAction->FindCmd((WDL_FastString*)_item1);
		int i2 = g_editedAction->FindCmd((WDL_FastString*)_item2);
		if (i1 >= 0 && i2 >= 0) {
			if (i1 > i2) return 1;
			else if (i1 < i2) return -1;
		}
	}
	return 0;
}

void CommandsView::OnBeginDrag(SWS_ListItem* item) {
	AllEditListItemEnd(true);
	SetCapture(GetParent(m_hwndList));
}

void CommandsView::OnDrag()
{
	if (g_editedAction)
	{
		POINT p; GetCursorPos(&p);
		if (WDL_FastString* hitItem = (WDL_FastString*)GetHitItem(p.x, p.y, NULL))
		{
			WDL_PtrList<WDL_FastString> draggedItems;
			int iNewPriority = g_editedAction->FindCmd(hitItem);
			int x=0, iSelPriority;
			while(WDL_FastString* selItem = (WDL_FastString*)EnumSelected(&x))
			{
				iSelPriority = g_editedAction->FindCmd(selItem);
				if (iNewPriority == iSelPriority) return;
				draggedItems.Add(selItem);
			}

			bool bDir = iNewPriority > iSelPriority;
			for (int i = bDir ? 0 : draggedItems.GetSize()-1; bDir ? i < draggedItems.GetSize() : i >= 0; bDir ? i++ : i--) {
				g_editedAction->RemoveCmd(draggedItems.Get(i));
				g_editedAction->InsertCmd(iNewPriority, draggedItems.Get(i));
			}

			ListView_DeleteAllItems(m_hwndList); // because of the special sort criteria ("not sortable" somehow)
			Update();

			for (int i=0; i < draggedItems.GetSize(); i++)
				SelectByItem((SWS_ListItem*)draggedItems.Get(i), i==0, i==0);
			UpdateEditedStatus(draggedItems.GetSize() > 0);
		}
	}
}

void CommandsView::OnItemSelChanged(SWS_ListItem* item, int iState) {
	AllEditListItemEnd(true);
}


///////////////////////////////////////////////////////////////////////////////
// CyclactionWnd
///////////////////////////////////////////////////////////////////////////////

// S&M windows lazy init: below's "" prevents registering the SWS' screenset callback
// (use the S&M one instead - already registered via SNM_WindowManager::Init())
CyclactionWnd::CyclactionWnd()
	: SWS_DockWnd(IDD_SNM_CYCLACTION, __LOCALIZE("Cycle Actions","sws_DLG_161"), "")
{
	m_id.Set(CA_WND_ID);

	// Must call SWS_DockWnd::Init() to restore parameters and open the window if necessary
	Init();
}

CyclactionWnd::~CyclactionWnd()
{
	m_tinyLRbtns.RemoveAllChildren(false);
	m_tinyLRbtns.SetRealParent(NULL);
}

void CyclactionWnd::Update(bool _updateListViews)
{
	if (_updateListViews)
		for (int i=0; i < m_pLists.GetSize(); i++)
			GetListView(i)->Update();
	m_parentVwnd.RequestRedraw(NULL);
}

void CyclactionWnd::OnInitDlg()
{
	g_lvL = new CyclactionsView(GetDlgItem(m_hwnd, IDC_LIST1), GetDlgItem(m_hwnd, IDC_EDIT));
	m_pLists.Add(g_lvL);

	g_lvR = new CommandsView(GetDlgItem(m_hwnd, IDC_LIST2), GetDlgItem(m_hwnd, IDC_EDIT));
	m_pLists.Add(g_lvR);

	SetWindowLongPtr(GetDlgItem(m_hwnd, IDC_FILTER), GWLP_USERDATA, 0xdeadf00b);
	SetWindowLongPtr(GetDlgItem(m_hwnd, IDC_EDIT), GWLP_USERDATA, 0xdeadf00b);

	m_resize.init_item(IDC_FILTER, 0.0, 0.0, 0.0, 0.0);
	m_resize.init_item(IDC_LIST1, 0.0, 0.0, 0.5, 1.0);
	m_resize.init_item(IDC_LIST2, 0.5, 0.0, 1.0, 1.0);
	g_origRectL = m_resize.get_item(IDC_LIST1)->real_orig;
	g_origRectR = m_resize.get_item(IDC_LIST2)->real_orig;
	g_lvState = g_lvLastState = 0; // resync those vars: both list views are displayed (g_lvState not persisted yet..)


	LICE_CachedFont* font = SNM_GetThemeFont();

	m_vwnd_painter.SetGSC(WDL_STYLE_GetSysColor);
	m_parentVwnd.SetRealParent(m_hwnd);

	m_txtSection.SetID(TXTID_SECTION);
	m_txtSection.SetFont(font);
	m_txtSection.SetText(__LOCALIZE("Section:","sws_DLG_161"));
	m_parentVwnd.AddChild(&m_txtSection);

	m_cbSection.SetID(CMBID_SECTION);
	for (int i=0; i < SNM_MAX_CA_SECTIONS; i++)
		m_cbSection.AddItem(SNM_GetActionSectionName(i));
	m_cbSection.SetCurSel(g_editedSection);
	m_cbSection.SetFont(font);
	m_parentVwnd.AddChild(&m_cbSection);

	m_btnUndo.SetID(BTNID_UNDO);
	m_btnUndo.SetTextLabel(__LOCALIZE("Consolidate undo points","sws_DLG_161"), -1, font);
	m_parentVwnd.AddChild(&m_btnUndo);

	m_btnPreventUIRefresh.SetID(BTNID_PREVENTUIREFRESH);
	m_btnPreventUIRefresh.SetTextLabel(__LOCALIZE("Prevent UI refresh","sws_DLG_161"), -1, font);
	m_parentVwnd.AddChild(&m_btnPreventUIRefresh);

	m_btnApply.SetID(BTNID_APPLY);
	m_parentVwnd.AddChild(&m_btnApply);

	m_btnCancel.SetID(BTNID_CANCEL);
	m_parentVwnd.AddChild(&m_btnCancel);

	m_btnImpExp.SetID(BTNID_IMPEXP);
	m_parentVwnd.AddChild(&m_btnImpExp);

	m_btnActionList.SetID(BTNID_ACTIONLIST);
	m_parentVwnd.AddChild(&m_btnActionList);

	m_btnLeft.SetID(BTNID_L);
	m_tinyLRbtns.AddChild(&m_btnLeft);
	m_btnRight.SetID(BTNID_R);
	m_tinyLRbtns.AddChild(&m_btnRight);
	m_tinyLRbtns.SetID(WNDID_LR);
	m_parentVwnd.AddChild(&m_tinyLRbtns);

	// restores the text filter when docking/undocking + indirect call to Update() !
	SetDlgItemText(m_hwnd, IDC_FILTER, g_caFilter.Get());

/* see above comment
	Update();
*/
}

void CyclactionWnd::OnDestroy() 
{
/*no! would be triggered on dock/undock..
	Cancel(true);
*/
	m_cbSection.Empty();
	m_tinyLRbtns.RemoveAllChildren(false);
	m_tinyLRbtns.SetRealParent(NULL);
}

void CyclactionWnd::OnResize() 
{
	if (g_lvState != g_lvLastState)
	{
		g_lvLastState = g_lvState;
		switch(g_lvState)
		{
			case -1: // right list view only
				m_resize.remove_item(IDC_LIST1);
				m_resize.remove_item(IDC_LIST2);
				m_resize.init_item(IDC_LIST1, 0.0, 0.0, 0.0, 0.0);
				m_resize.init_item(IDC_LIST2, 0.0, 0.0, 1.0, 1.0);
				m_resize.get_item(IDC_LIST2)->orig =  g_origRectR;
				m_resize.get_item(IDC_LIST2)->orig.left = g_origRectL.left;
				ShowWindow(GetDlgItem(m_hwnd, IDC_LIST1), SW_HIDE);
				break;
			case 0: // default: both list views
				m_resize.remove_item(IDC_LIST1);
				m_resize.remove_item(IDC_LIST2);
				m_resize.init_item(IDC_LIST1, 0.0, 0.0, 0.5, 1.0);
				m_resize.init_item(IDC_LIST2, 0.5, 0.0, 1.0, 1.0);
				m_resize.get_item(IDC_LIST1)->orig = g_origRectL;
				m_resize.get_item(IDC_LIST2)->orig = g_origRectR;
				ShowWindow(GetDlgItem(m_hwnd, IDC_LIST1), SW_SHOW);
				ShowWindow(GetDlgItem(m_hwnd, IDC_LIST2), SW_SHOW);
				break;
			case 1: // left list view only
				m_resize.remove_item(IDC_LIST1);
				m_resize.remove_item(IDC_LIST2);
				m_resize.init_item(IDC_LIST1, 0.0, 0.0, 1.0, 1.0);
				m_resize.init_item(IDC_LIST2, 0.0, 0.0, 0.0, 0.0);
				m_resize.get_item(IDC_LIST1)->orig = g_origRectL;
				m_resize.get_item(IDC_LIST1)->orig.right = g_origRectR.right;
				ShowWindow(GetDlgItem(m_hwnd, IDC_LIST2), SW_HIDE);
				break;
		}
		InvalidateRect(m_hwnd, NULL, 0);
	}
}

// _flags&1=select new item, _flags&2=edit cell
void AddOrInsertCommand(const char* _cmd, int _flags = 0)
{
	if (g_editedAction && _cmd)
	{
		int x=0, pos=-1;
		if (WDL_FastString* selcmd = (WDL_FastString*)g_lvR->EnumSelected(&x))
			pos = g_editedAction->FindCmd(selcmd);

		WDL_FastString* newCmd = g_editedAction->AddCmd(_cmd, pos);
		g_lvR->Update();
		UpdateEditedStatus(true);

		if (pos>=0)
			ListView_SetItemState(g_lvR->GetHWND(), -1, 0, LVIS_SELECTED);
		if (_flags&1)
			g_lvR->SelectByItem((SWS_ListItem*)newCmd);
		if (_flags&2)
		{
			g_lvR->EditListItem((SWS_ListItem*)newCmd, COL_R_CMD);
			if (CyclactionWnd* w = g_caWndMgr.Get())
			{
				HWND h = GetDlgItem(w->GetHWND(), IDC_EDIT);
				SetFocus(h);
				SendMessage(h, EM_SETSEL, strlen(_cmd), strlen(_cmd)+1); // move cursor to the end
			}
		}
	}
}

void CyclactionWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	KbdSectionInfo* kbdSec = SNM_GetActionSection(g_editedSection);
	if (!kbdSec) return;

	int x=0;
	Cyclaction* action = (Cyclaction*)g_lvL->EnumSelected(&x);
	switch (LOWORD(wParam))
	{
		case IDC_FILTER:
			if (HIWORD(wParam)==EN_CHANGE)
			{
				char filter[128]="";
				GetWindowText(GetDlgItem(m_hwnd, IDC_FILTER), filter, sizeof(filter));
				g_caFilter.Set(filter);
				Update();
			}
#ifndef _SNM_SWELL_ISSUES
			else if (HIWORD(wParam)==EN_SETFOCUS)
			{
				HWND hFilt = GetDlgItem(m_hwnd, IDC_FILTER);
				char filter[128]="";
				GetWindowText(hFilt, filter, sizeof(filter));
				if (!strcmp(filter, FILTER_DEFAULT_STR))
					SetWindowText(hFilt, "");
			}
			else if (HIWORD(wParam)==EN_KILLFOCUS)
			{
				HWND hFilt = GetDlgItem(m_hwnd, IDC_FILTER);
				char filter[128]="";
				GetWindowText(hFilt, filter, sizeof(filter));
				if (*filter == '\0') 
					SetWindowText(hFilt, FILTER_DEFAULT_STR);
			}
#endif
			break;
		case ADD_CYCLACTION_MSG: 
		{
			Cyclaction* a = new Cyclaction(__LOCALIZE("Untitled","sws_DLG_161"));
			a->m_added = true;
			g_editedActions[g_editedSection].Add(a);
			UpdateListViews();
			UpdateEditedStatus(true);
			g_lvL->SelectByItem((SWS_ListItem*)a);
			g_lvL->EditListItem((SWS_ListItem*)a, COL_L_NAME);
			break;
		}
		case DEL_CYCLACTION_MSG: // remove cyclaction (for the user.. in fact it clears)
		{
			// keep pointers (may be used in a listview, delete once updated)
			int x=0; WDL_PtrList_DeleteOnDestroy<WDL_FastString> cmdsToDelete;
			while(Cyclaction* a = (Cyclaction*)g_lvL->EnumSelected(&x)) {
				for (int i=0; i < a->GetCmdSize(); i++)
					cmdsToDelete.Add(a->GetCmdString(i));
				a->Update(CA_EMPTY);
			}
//no!			if (cmdsToDelete.GetSize()) 
			{
				g_editedAction = NULL;
				UpdateListViews();
				UpdateEditedStatus(true);
			} // + cmdsToDelete auto clean-up
			break;
		}
		case RUN_CYCLACTION_MSG:
		case LEARN_CYCLACTION_MSG:
			if (action)
			{
				if (action->m_cmdId)
				{
					if (LOWORD(wParam) == RUN_CYCLACTION_MSG)
					{
						int c = action->m_cmdId, val=63, valhw=-1, relmode=0; // actioncommandID may get modified below
						kbd_RunCommandThroughHooks(kbdSec,&c,&val,&valhw,&relmode,NULL); // NULL hwnd here, this is "managed" in PerformSingleCommand()
					}
					else if (LearnAction(kbdSec, action->m_cmdId) && g_lvL)
						g_lvL->Update();
				}
				else
				{
					MessageBox(m_hwnd,
						__LOCALIZE("This action is not registered yet!","sws_DLG_161"),
						__LOCALIZE("S&M - Error","sws_DLG_161"), MB_OK);
				}
			}
			break;
		case ADD_CMD_MSG:
			AddOrInsertCommand("", 3);
			break;
		case ADD_STEP_CMD_MSG:
			AddOrInsertCommand("!", 3);
			break;
		case LEARN_CMD_MSG:
		{
			char idstr[SNM_MAX_ACTION_CUSTID_LEN] = "";
			if (GetSelectedAction(idstr, SNM_MAX_ACTION_CUSTID_LEN, kbdSec))
				AddOrInsertCommand(idstr, 1);
			break;
		}
		case PASTE_CMD_MSG:
			for (int i=g_clipboardCmds.GetSize()-1; i>=0; i--) // reverse order due to AddOrInsertCommand
				AddOrInsertCommand(g_clipboardCmds.Get(i)->Get(), 1);
			break;
		case CUT_CMD_MSG:
		case COPY_CMD_MSG:
			if (g_editedAction)
			{
				g_clipboardCmds.Empty(true);

				int x=0;
				while(WDL_FastString* cmd = (WDL_FastString*)g_lvR->EnumSelected(&x))
					g_clipboardCmds.Add(new WDL_FastString(cmd->Get()));
				if (LOWORD(wParam) != CUT_CMD_MSG) // cut => fall through
					break;
			}
			else
				break;
		case DEL_CMD_MSG:
			if (g_editedAction)
			{
				// keep pointers (may be used in a listview, delete once updated)
				int x=0; WDL_PtrList_DeleteOnDestroy<WDL_FastString> cmdsToDelete;
				while(WDL_FastString* delcmd = (WDL_FastString*)g_lvR->EnumSelected(&x)) {
					cmdsToDelete.Add(delcmd);
					g_editedAction->RemoveCmd(delcmd, false);
				}
				if (cmdsToDelete.GetSize()) {
					g_lvR->Update();
					UpdateEditedStatus(true);
				}
			} // + cmdsToDelete auto clean-up
			break;
		case EXPLODE_CMD_MSG:
			if (g_editedAction)
			{
				int x=0;
				bool sel = false;

				// to keep pointers (may be used in a listview, delete once updated)
				WDL_PtrList_DeleteOnDestroy<WDL_FastString> cmdsToDelete;
				WDL_PtrList_DeleteOnDestroy<WDL_FastString> macros, consoles;
				while(WDL_FastString* selcmd = (WDL_FastString*)g_lvR->EnumSelected(&x))
				{
					sel = true;
					WDL_PtrList_DeleteOnDestroy<WDL_FastString> subCmds;

					// commands to explode must be registered => SNM_NamedCommandLookup() hard check here!
					if (SNM_NamedCommandLookup(selcmd->Get(), kbdSec, true) && 
						ExplodeCmd(g_editedSection, selcmd->Get(), &subCmds, &macros, &consoles, 0) > 0) // >0 means "something done"
					{
						cmdsToDelete.Add(selcmd);
						g_editedAction->ReplaceCmd(selcmd, false, &subCmds);
					}
				}
				if (cmdsToDelete.GetSize()) {
					g_lvR->Update();
					UpdateEditedStatus(true);
				}
				else if (sel)
					MessageBox(
						g_caWndMgr.GetMsgHWND(),
						__LOCALIZE("Selected commands cannot be exploded!\nProbable cause: not a macro, not a cycle action, unknown or recursive command, etc..","sws_DLG_161"),
						__LOCALIZE("S&M - Error","sws_DLG_161"),
						MB_OK);
			} // + cmdsToDelete auto clean-up
			break;
		case IMPORT_CUR_SECTION_MSG:
			if (char* fn = BrowseForFiles(__LOCALIZE("S&M - Import cycle actions","sws_DLG_161"), g_lastImportFn, NULL, false, SNM_INI_EXT_LIST))
			{
				LoadCyclactions(false, g_editedActions, g_editedSection, fn);
				lstrcpyn(g_lastImportFn, fn, sizeof(g_lastImportFn));
				free(fn);
				g_editedAction = NULL;
				UpdateListViews();
				UpdateEditedStatus(true);
			}
			break;
		case IMPORT_ALL_SECTIONS_MSG:
			if (char* fn = BrowseForFiles(__LOCALIZE("S&M - Import cycle actions","sws_DLG_161"), g_lastImportFn, NULL, false, SNM_INI_EXT_LIST))
			{
				LoadCyclactions(false, g_editedActions, -1, fn);
				lstrcpyn(g_lastImportFn, fn, sizeof(g_lastImportFn));
				free(fn);
				g_editedAction = NULL;
				UpdateListViews();
				UpdateEditedStatus(true);
			}
			break;
		case EXPORT_SEL_MSG:
		{
			int x=0; WDL_PtrList_DeleteOnDestroy<Cyclaction> actions[SNM_MAX_CA_SECTIONS];
			while(Cyclaction* a = (Cyclaction*)g_lvL->EnumSelected(&x))
				actions[g_editedSection].Add(new Cyclaction(a));
			if (actions[g_editedSection].GetSize()) {
				char fn[SNM_MAX_PATH] = "";
				if (BrowseForSaveFile(__LOCALIZE("S&M - Export cycle actions","sws_DLG_161"), g_lastExportFn, strrchr(g_lastExportFn, '.') ? g_lastExportFn : NULL, SNM_INI_EXT_LIST, fn, sizeof(fn))) {
					SaveCyclactions(actions, g_editedSection, fn);
					lstrcpyn(g_lastExportFn, fn, sizeof(g_lastExportFn));
				}
			}
			break;
		}
		case EXPORT_CUR_SECTION_MSG:
		{
			WDL_PtrList_DeleteOnDestroy<Cyclaction> actions[SNM_MAX_CA_SECTIONS];
			for (int i=0; i < g_editedActions[g_editedSection].GetSize(); i++)
				actions[g_editedSection].Add(new Cyclaction(g_editedActions[g_editedSection].Get(i)));
			if (actions[g_editedSection].GetSize()) {
				char fn[SNM_MAX_PATH] = "";
				if (BrowseForSaveFile(__LOCALIZE("S&M - Export cycle actions","sws_DLG_161"), g_lastExportFn, strrchr(g_lastExportFn, '.') ? g_lastExportFn : NULL, SNM_INI_EXT_LIST, fn, sizeof(fn))) {
					SaveCyclactions(actions, g_editedSection, fn);
					lstrcpyn(g_lastExportFn, fn, sizeof(g_lastExportFn));
				}
			}
			break;
		}
		case EXPORT_ALL_SECTIONS_MSG:
		{
			char fn[SNM_MAX_PATH] = "";
			if (BrowseForSaveFile(__LOCALIZE("S&M - Export cycle actions","sws_DLG_161"), g_lastExportFn, g_lastExportFn, SNM_INI_EXT_LIST, fn, sizeof(fn))) {
				SaveCyclactions(g_editedActions, -1, fn);
				lstrcpyn(g_lastExportFn, fn, sizeof(g_lastExportFn));
			}
			break;
		}
		case RESET_CUR_SECTION_MSG:
			ResetSection(g_editedSection);
			break;
		case RESET_ALL_SECTIONS_MSG:
			for (int sec=0; sec < SNM_MAX_CA_SECTIONS; sec++)
				ResetSection(sec);
			break;
/*JFB!! doc not up to date
		case IDC_HELPTEXT:
			ShellExecute(m_hwnd, "open", "http://wiki.cockos.com/wiki/index.php/ALR_Main_S%26M_CREATE_CYCLACTION" , NULL, NULL, SW_SHOWNORMAL);
			break;
*/
		case CMBID_SECTION:
			if (HIWORD(wParam)==CBN_SELCHANGE) {
				AllEditListItemEnd(false);
				UpdateSection(m_cbSection.GetCurSel());
			}
			break;
		case BTNID_UNDO:
			if (!HIWORD(wParam) || HIWORD(wParam)==600) {
				g_undos = !g_undos;
				Update(false);
			}
			break;
		case BTNID_PREVENTUIREFRESH:
			if (!HIWORD(wParam) || HIWORD(wParam)==600) {
				g_preventUIRefresh = !g_preventUIRefresh;
				Update(false);
			}
			break;
		case BTNID_APPLY:
			Apply();
			break;
		case BTNID_CANCEL:
			Cancel(false);
			break;
		case BTNID_IMPEXP: {
				RECT r; m_btnImpExp.GetPositionInTopVWnd(&r);
				ClientToScreen(m_hwnd, (LPPOINT)&r);
				ClientToScreen(m_hwnd, ((LPPOINT)&r)+1);
				SendMessage(m_hwnd, WM_CONTEXTMENU, 0, MAKELPARAM((UINT)(r.left), (UINT)(r.bottom+SNM_1PIXEL_Y)));
			}
			break;
		case BTNID_ACTIONLIST:
			AllEditListItemEnd(false);
			ShowActionList(kbdSec, NULL);
			break;
		case BTNID_R:
			if (g_lvState<1) {
				g_lvState++;
				SendMessage(m_hwnd, WM_SIZE, 0, 0);
			}
			break;
		case BTNID_L:
			if (g_lvState>-1) {
				g_lvState--;
				SendMessage(m_hwnd, WM_SIZE, 0, 0);
			}
			break;
		default:
			if (LOWORD(wParam) >= ADD_STATEMENT_MSG && LOWORD(wParam) < (ADD_STATEMENT_MSG+NB_STATEMENTS))
			{
				int id = LOWORD(wParam)-ADD_STATEMENT_MSG;
				WDL_FastString cmd(g_statements[id]);
				if (IsStatementWithParam(id))
					cmd.Append(" ");

				// auto-add ENLOOP or ENDIF if needed
				if (IsCondStatement(g_statements[id]))
					AddOrInsertCommand(STATEMENT_ENDIF, 1);
				else if (!_stricmp(STATEMENT_LOOP, g_statements[id]))
					AddOrInsertCommand(STATEMENT_ENDLOOP, 1);

				AddOrInsertCommand(cmd.Get(), IsStatementWithParam(id) ? 3 : 1); // cmds with params: edit cell
				break;
			}
			else
				Main_OnCommand((int)wParam, (int)lParam);
			break;
	}
}

void CyclactionWnd::DrawControls(LICE_IBitmap* _bm, const RECT* _r, int* _tooltipHeight)
{
	// 1st row of controls, left
	int x0 = _r->left + SNM_GUI_X_MARGIN_OLD;
	int h = SNM_GUI_TOP_H;
	if (_tooltipHeight)
		*_tooltipHeight = h;

	// defines a new rect r that takes the filter edit box into account
	RECT r;
	GetWindowRect(GetDlgItem(m_hwnd, IDC_FILTER), &r);
	ScreenToClient(m_hwnd, (LPPOINT)&r);
	ScreenToClient(m_hwnd, ((LPPOINT)&r)+1);
	x0 = r.right + SNM_GUI_X_MARGIN_OLD;

	m_btnUndo.SetCheckState(g_undos);
	SNM_AutoVWndPosition(DT_LEFT, &m_btnUndo, NULL, _r, &x0, _r->top, h, 4);

	m_btnPreventUIRefresh.SetCheckState(g_preventUIRefresh);
	SNM_AutoVWndPosition(DT_LEFT, &m_btnPreventUIRefresh, NULL, _r, &x0, _r->top, h, 4);

	// right
	RECT r2 = *_r; r2.left = x0; // tweak to auto-hide the logo when needed
	x0 = _r->right-10; //JFB!! 10 is tied to the current .rc!
	if (SNM_AutoVWndPosition(DT_RIGHT, &m_cbSection, NULL, &r2, &x0, _r->top, h, 4))
		SNM_AutoVWndPosition(DT_RIGHT, &m_txtSection, &m_cbSection, &r2, &x0, _r->top, h, 0);

	// 2nd row of controls
	x0 = _r->left + SNM_GUI_X_MARGIN_OLD;
	h = SNM_GUI_BOT_H;
	int y0 = _r->bottom-h;

	SNM_SkinToolbarButton(&m_btnApply, __LOCALIZE("Apply","sws_DLG_161"));
	m_btnApply.SetGrayed(!g_edited);
	if (SNM_AutoVWndPosition(DT_LEFT, &m_btnApply, NULL, _r, &x0, y0, h, 4))
	{
		SNM_SkinToolbarButton(&m_btnCancel, __LOCALIZE("Cancel","sws_DLG_161"));
		if (SNM_AutoVWndPosition(DT_LEFT, &m_btnCancel, NULL, _r, &x0, y0, h, 4))
		{
			SNM_SkinToolbarButton(&m_btnImpExp, __LOCALIZE("Import/export","sws_DLG_161"));
			if (SNM_AutoVWndPosition(DT_LEFT, &m_btnImpExp, NULL, _r, &x0, y0, h, 4)) {
				SNM_SkinToolbarButton(&m_btnActionList, __LOCALIZE("Action list...","sws_DLG_161"));
				SNM_AutoVWndPosition(DT_LEFT, &m_btnActionList, NULL, _r, &x0, y0, h, 4);
			}
		}
	}
	r2 = *_r; r2.left = x0; // tweak to auto-hide the logo when needed
	SNM_AddLogo(_bm, &r2);

	// tiny left/right buttons
	if (IsWindowVisible(GetDlgItem(m_hwnd, IDC_LIST1))) // left list view is displayed: add tiny buttons on its right
	{
		GetWindowRect(GetDlgItem(m_hwnd, IDC_LIST1), &r);
		ScreenToClient(m_hwnd, (LPPOINT)&r);
		ScreenToClient(m_hwnd, ((LPPOINT)&r)+1);
		r.top = _r->top + h; 
		r.bottom = _r->top + h + (9*2 +1);
		r.left = r.right + 1;
		r.right = r.left + 5;
	}
	else // add tiny buttons on the left
	{
		r.top = _r->top + h; 
		r.bottom = _r->top + h + (9*2 +1);
		r.left = 1;
		r.right = r.left + 5;
	}

	m_btnLeft.SetEnabled(g_lvState>-1);
	m_btnRight.SetEnabled(g_lvState<1);
	m_tinyLRbtns.SetPosition(&r);
	m_tinyLRbtns.SetVisible(true);
}

void CyclactionWnd::AddImportExportMenu(HMENU _menu, bool _wantReset)
{
	char buf[128] = "";
	snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("Import in section '%s'...","sws_DLG_161"), SNM_GetActionSectionName(g_editedSection));
	AddToMenu(_menu, buf, IMPORT_CUR_SECTION_MSG, -1, false, IsCAFiltered() ? MF_GRAYED : MF_ENABLED);
	AddToMenu(_menu, __LOCALIZE("Import all sections...","sws_DLG_161"), IMPORT_ALL_SECTIONS_MSG, -1, false, IsCAFiltered() ? MF_GRAYED : MF_ENABLED);
	AddToMenu(_menu, SWS_SEPARATOR, 0);
	AddToMenu(_menu, __LOCALIZE("Export selected cycle actions...","sws_DLG_161"), EXPORT_SEL_MSG);
	snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("Export section '%s'...","sws_DLG_161"), SNM_GetActionSectionName(g_editedSection));
	AddToMenu(_menu, buf, EXPORT_CUR_SECTION_MSG);
	AddToMenu(_menu, __LOCALIZE("Export all sections...","sws_DLG_161"), EXPORT_ALL_SECTIONS_MSG);
	if (_wantReset) {
		AddToMenu(_menu, SWS_SEPARATOR, 0);
		AddResetMenu(_menu);
	}
}

void CyclactionWnd::AddResetMenu(HMENU _menu)
{
	char buf[128] = "";
	snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("Reset section '%s'...","sws_DLG_161"), SNM_GetActionSectionName(g_editedSection));
	AddToMenu(_menu, buf, RESET_CUR_SECTION_MSG);
	AddToMenu(_menu, __LOCALIZE("Reset all sections","sws_DLG_161"), RESET_ALL_SECTIONS_MSG);
}

HMENU CyclactionWnd::OnContextMenu(int x, int y, bool* wantDefaultItems)
{
	HMENU hMenu = CreatePopupMenu();

	AllEditListItemEnd(true);
		
	// specific context menu for the import/export button
	POINT p; GetCursorPos(&p);
	ScreenToClient(m_hwnd, &p);
	if (WDL_VWnd* v = m_parentVwnd.VirtWndFromPoint(p.x,p.y,1))
		if (v->GetID() == BTNID_IMPEXP) {
			*wantDefaultItems = false;
			AddImportExportMenu(hMenu, true);
			return hMenu;
		}

	// list view context menu?
	bool left=false, right=false;
	{
		POINT pt = {x, y};
		RECT r;	GetWindowRect(g_lvL->GetHWND(), &r);
		left = IsWindowVisible(g_lvL->GetHWND()) && PtInRect(&r, pt) ? true : false;
		GetWindowRect(g_lvR->GetHWND(), &r);
		right = IsWindowVisible(g_lvR->GetHWND()) && PtInRect(&r,pt) ? true : false;
	}

	if (left || right)
	{
		*wantDefaultItems = false;
		Cyclaction* action = (Cyclaction*)g_lvL->GetHitItem(x, y, NULL);
		WDL_FastString* cmd = (WDL_FastString*)g_lvR->GetHitItem(x, y, NULL);
		if (left)
		{
			AddToMenu(hMenu, __LOCALIZE("Add cycle action","sws_DLG_161"), ADD_CYCLACTION_MSG, -1, false, IsCAFiltered() ? MF_GRAYED : MF_ENABLED);
			if (action && action != &s_DEFAULT_L)
			{
				AddToMenu(hMenu, __LOCALIZE("Remove cycle actions","sws_DLG_161"), DEL_CYCLACTION_MSG); 
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				AddToMenu(hMenu, __LOCALIZE("Run","sws_DLG_161"), RUN_CYCLACTION_MSG, -1, false, action->m_cmdId ? MF_ENABLED : MF_GRAYED); 
/* commented: this is a job for REAPER's action list
				AddToMenu(hMenu, __LOCALIZE("Add shortcut...","sws_DLG_161"), LEARN_CYCLACTION_MSG, -1, false, action->m_cmdId ? MF_ENABLED : MF_GRAYED); 
*/
			}
		}
		// right
		else if (g_editedAction && g_editedAction!=&s_DEFAULT_L) // => s_EMPTY_R cannot be displayed here
		{
			AddToMenu(hMenu, cmd && cmd!=&s_DEFAULT_R ? __LOCALIZE("Insert","sws_DLG_161") : __LOCALIZE("Add","sws_DLG_161"), ADD_CMD_MSG);
			AddToMenu(hMenu, cmd && cmd!=&s_DEFAULT_R ? __LOCALIZE("Insert step","sws_DLG_161") : __LOCALIZE("Add step","sws_DLG_161"), ADD_STEP_CMD_MSG);

			// statements sub-menu
			{
				HMENU hStatementSubMenu = CreatePopupMenu();
				AddSubMenu(hMenu, hStatementSubMenu, cmd && cmd!=&s_DEFAULT_R ? __LOCALIZE("Insert statement","sws_DLG_161") : __LOCALIZE("Add statement","sws_DLG_161"));
				for (int i=0; i<NB_STATEMENTS; i++)
				{
#ifdef _SNM_MISC
					WDL_FastString itemTxt;
					itemTxt.Set(g_statements[i]);
					// skip obvious statements (no help details needed)
					if (i!=IDX_STATEMENT_ELSE && i!=IDX_STATEMENT_ENDIF &&
						i!=IDX_STATEMENT_LOOP && i!=IDX_STATEMENT_ENDLOOP)
					{
						itemTxt.Append("  [");
						itemTxt.Append(__localizeFunc(g_statementInfos[i],"sws_DLG_161",LOCALIZE_FLAG_NOCACHE));
						itemTxt.Append("]");
					}
					AddToMenu(hStatementSubMenu, itemTxt.Get(), ADD_STATEMENT_MSG+i);
#else
					AddToMenu(hStatementSubMenu, g_statements[i], ADD_STATEMENT_MSG+i);
#endif
				}
			}

			AddToMenu(hMenu, cmd && cmd!=&s_DEFAULT_R ? __LOCALIZE("Insert selected action (in the Actions window)","sws_DLG_161") : __LOCALIZE("Add selected action (in the Actions window)","sws_DLG_161"), LEARN_CMD_MSG);
			AddToMenu(hMenu, SWS_SEPARATOR, 0);

			if (cmd && cmd!=&s_DEFAULT_R)
			{
				AddToMenu(hMenu, __LOCALIZE("Delete","sws_DLG_161"), DEL_CMD_MSG);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				AddToMenu(hMenu, __LOCALIZE("Copy","sws_DLG_155"), COPY_CMD_MSG);
				AddToMenu(hMenu, __LOCALIZE("Cut","sws_DLG_155"), CUT_CMD_MSG);
			}
			AddToMenu(hMenu, __LOCALIZE("Paste","sws_DLG_155"), PASTE_CMD_MSG, -1, false, g_clipboardCmds.GetSize() ? MF_ENABLED : MF_GRAYED);
			if (cmd && cmd!=&s_DEFAULT_R)
			{
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				AddToMenu(hMenu, __LOCALIZE("Explode into individual actions","sws_DLG_161"), EXPLODE_CMD_MSG);
			}
		}
	}
	else
	{
		if (GetMenuItemCount(hMenu))
			AddToMenu(hMenu, SWS_SEPARATOR, 0);

		HMENU hImpExpSubMenu = CreatePopupMenu();
		AddSubMenu(hMenu, hImpExpSubMenu, __LOCALIZE("Import/export","sws_DLG_161"));
		AddImportExportMenu(hImpExpSubMenu, false);
		AddToMenu(hMenu, SWS_SEPARATOR, 0);
		AddResetMenu(hMenu);
	}
	return hMenu;
}

int CyclactionWnd::OnKey(MSG* _msg, int _iKeyState) 
{
	if (_msg->message == WM_KEYDOWN)
	{
		if (HWND h = GetFocus())
		{
			int focusedList = -1;
			if (h == g_lvL->GetHWND() && IsWindowVisible(g_lvL->GetHWND()))
				focusedList = 0;
			else if (h == g_lvR->GetHWND() && IsWindowVisible(g_lvR->GetHWND()))
				focusedList = 1;
			else
				return 0;

			if (!_iKeyState)
			{
				switch(_msg->wParam)
				{
					case VK_F2:
					{
						int x=0;
						if (focusedList<m_pLists.GetSize())
						{
							if (SWS_ListItem* item = GetListView(focusedList)->EnumSelected(&x))
							{
								GetListView(focusedList)->EditListItem(item, !focusedList ? (int)COL_L_NAME : (int)COL_R_CMD);
								return 1;
							}
						}
						break;
					}
					case VK_DELETE:
						OnCommand(!focusedList ? DEL_CYCLACTION_MSG : DEL_CMD_MSG, 0);
						return 1;
					case VK_RETURN:
						if (!focusedList) {
							OnCommand(RUN_CYCLACTION_MSG, 0);
							return 1;
						}
						break;
				}
			}
			else if (_iKeyState==LVKF_CONTROL && focusedList)
			{
				switch(_msg->wParam) {
					case 'X': OnCommand(CUT_CMD_MSG, 0);	return 1;
					case 'C': OnCommand(COPY_CMD_MSG, 0);	return 1;
					case 'V': OnCommand(PASTE_CMD_MSG, 0);	return 1;
				}
			}
		}
	}
	return 0;
}

void CyclactionWnd::UpdateSection(int _newSection)
{
	if (_newSection != g_editedSection)
	{
		g_editedSection = _newSection;
		g_editedAction = NULL;
		UpdateListViews();
	}
}


///////////////////////////////////////////////////////////////////////////////
// project_config_extension_t
///////////////////////////////////////////////////////////////////////////////

static bool ProcessExtensionLine(const char *line, ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	if (!isUndo || !g_undos) // undo only (nothing saved in projects!)
		return false;

	LineParser lp(false);
	if (lp.parse(line) || lp.getnumtokens() < 1)
		return false;

	// load cycle actions' states
	if (!strcmp(lp.gettoken_str(0), "<S&M_CYCLACTIONS"))
	{
		char line[SNM_MAX_CHUNK_LINE_LENGTH] = "";
		while(true)
		{
			if (!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
			{
				if (lp.getnumtokens()>0 && lp.gettoken_str(0)[0] == '>')
					break;
				else if (lp.getnumtokens() == 4)
				{
					int success, sec, cycleId, state, fakeTgl;
					sec = lp.gettoken_int(0, &success);
					if (success) cycleId = lp.gettoken_int(1, &success);
					if (success) state = lp.gettoken_int(2, &success);
					if (success) fakeTgl = lp.gettoken_int(3, &success);
					if (success)
					{
						if (Cyclaction* a = g_cas[sec].Get(cycleId))
						{
							if (a->m_cmdId && 
								(a->m_performState != state || // not enough (e.g. cycle actions w/o steps)
								a->m_fakeToggle != (fakeTgl?true:false))) // C4805
							{
								a->m_performState = state;
								a->m_fakeToggle = !a->m_fakeToggle;
								RefreshToolbar(a->m_cmdId);
							}
						}
					}
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
	if (!isUndo || !g_undos) // undo only (nothing saved in projects)
		return;

	WDL_FastString confStr("<S&M_CYCLACTIONS\n");
	int iHeaderLen = confStr.GetLength();
	for (int i=0; i < SNM_MAX_CA_SECTIONS; i++)
		for (int j=0; j < g_cas[i].GetSize(); j++)
			if (Cyclaction* a = g_cas[i].Get(j))
				if (!a->IsEmpty())
					confStr.AppendFormatted(SNM_MAX_CHUNK_LINE_LENGTH,"%d %d %d %d\n", i, j, a->m_performState, a->m_fakeToggle);

	if (confStr.GetLength() > iHeaderLen) {
		confStr.Append(">\n");
		StringToExtensionConfig(&confStr, ctx);
	}
}

static project_config_extension_t s_projectconfig = {
	ProcessExtensionLine, SaveExtensionConfig, NULL, NULL
};


///////////////////////////////////////////////////////////////////////////////

// called once REAPER is fully initialized, see SnM.cpp
// i.e. when *all*  actions are registered (native or not)
void CAsInit()
{
	LoadCyclactions(false);

	g_editedSection = 0;
	g_edited = false;
	EditModelInit();
}

int CyclactionInit()
{
	// localization
	g_caFilter.Set(FILTER_DEFAULT_STR);
	s_DEFAULT_L.Update(__LOCALIZE("Right click here to add cycle actions","sws_DLG_161"));
	s_EMPTY_R.Set(__LOCALIZE("<- Select a cycle action","sws_DLG_161"));
	s_DEFAULT_R.Set(__LOCALIZE("Right click here to add commands","sws_DLG_161"));

	snprintf(g_lastExportFn, sizeof(g_lastExportFn), SNM_CYCLACTION_EXPORT_FILE, GetResourcePath());
	snprintf(g_lastImportFn, sizeof(g_lastImportFn), SNM_CYCLACTION_EXPORT_FILE, GetResourcePath());

	// consolidate undo pref, default==enabled for ascendant compatibility
	// local pref: comes from the S&M.ini file
	g_undos = GetPrivateProfileInt("Cyclactions", "Undos", 1, g_SNM_IniFn.Get()) == 1;
	g_preventUIRefresh = GetPrivateProfileInt("Cyclactions", "PreventUIRefresh", 1, g_SNM_IniFn.Get()) == 1;

	if (!plugin_register("projectconfig", &s_projectconfig))
		return 0;

	// instanciate the window if needed, can be NULL
	g_caWndMgr.Init();

	return 1;
}

void CyclactionExit()
{
	plugin_register("-projectconfig", &s_projectconfig);
	WritePrivateProfileString("Cyclactions", "Undos", g_undos ? "1" : "0", g_SNM_IniFn.Get());
	WritePrivateProfileString("Cyclactions", "PreventUIRefresh", g_preventUIRefresh ? "1" : "0", g_SNM_IniFn.Get());
	g_caWndMgr.Delete();
}

void OpenCyclaction(COMMAND_T* _ct)
{
	if (CyclactionWnd* w = g_caWndMgr.Create()) {
		w->Show((g_editedSection == (int)_ct->user) /* i.e toggle */, true);
		w->SetType((int)_ct->user);
	}
}

int IsCyclactionDisplayed(COMMAND_T*)
{
	if (CyclactionWnd* w = g_caWndMgr.Get())
		return w->IsWndVisible();
	return 0;
}

