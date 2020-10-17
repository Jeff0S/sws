/******************************************************************************
/ SnM_Project.cpp
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

#include "stdafx.h"

#include "SnM.h"
#include "SnM_Project.h"
#include "SnM_Util.h"
#include "../Prompt.h"

#include <WDL/localize/localize.h>

///////////////////////////////////////////////////////////////////////////////
// Project helpers
///////////////////////////////////////////////////////////////////////////////

bool IsActiveProjectInLoadSave(char* _projfn, int _projfnSz, bool _ensureRPP)
{
	bool ok = (GetCurrentProjectInLoadSave() == EnumProjects(-1, _projfn, _projfnSz));
	if (_ensureRPP)
	{
		if (!_projfn || !_projfnSz)
		{
			char buf[SNM_MAX_PATH] = "";
			EnumProjects(-1, buf, sizeof(buf));
			ok = HasFileExtension(buf, "RPP");
		}
		else
			ok = HasFileExtension(_projfn, "RPP");
	}
	return ok;
}

void TieFileToProject(const char* _fn, ReaProject* _prj, bool _tie)
{
	if (_fn && *_fn)
	{
		if (ReaProject* prj = _prj ? _prj : EnumProjects(-1, NULL, 0))
		{
			void *p[2] = { (void*)_fn, (void*)prj };
			plugin_register(_tie ? "file_in_project_ex" : "-file_in_project_ex", p);
#ifdef _SNM_DEBUG
			char dbg[256]="";
			snprintf(dbg, sizeof(dbg), "TieFileToProject() - ReaProject: %p, %s %s\n", prj, _tie ? "Added" : "Removed", _fn);
			OutputDebugString(dbg);
#endif
		}
	}
}

void UntieFileFromProject(const char* _fn, ReaProject* _prj) {
	TieFileToProject(_fn, _prj, false);
}

// flags&1=incl. items, &2=incl. markers/regions, &4=incl. envelopes
double SNM_GetProjectLength(int _flags)
{
  // all bits set/default flags (see the .h): make use of the new API
	if (_flags == 0xFFFF) return GetProjectLength(NULL);

	double prjlen = 0.0, pos, end;
	if (_flags&2)
	{
		int x=0; bool isRgn;
		while ((x = EnumProjectMarkers2(NULL, x, &isRgn, &pos, &end, NULL, NULL))) {
			if (isRgn) { if (end > prjlen) prjlen = end; }
			else { if (pos > prjlen) prjlen = pos; }
		}
	}
	if ((_flags&1) || (_flags&4))
	{
		double len;
		for (int i=0; i <= GetNumTracks(); i++)
		{
			if (MediaTrack* tr = CSurf_TrackFromID(i, false))
			{
				int cnt= i>0 ? GetTrackNumMediaItems(tr) : 0; // skip master
				if (_flags&1) for (int j=0; j<cnt; j++)
				{
					if (MediaItem* item = GetTrackMediaItem(tr,j))
					{
						pos = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
						len = *(double*)GetSetMediaItemInfo(item, "D_LENGTH", NULL);
						if ((pos+len)>prjlen) prjlen = pos+len;
					}
				}

				cnt=CountTrackEnvelopes(tr);
				if (_flags&4) for (int j=0; j<cnt; j++)
				{
					if (TrackEnvelope* env = GetTrackEnvelope(tr,j))
					{
						if (int ptcnt=CountEnvelopePoints(env))
						{
							pos=prjlen;
							GetEnvelopePoint(env, ptcnt-1, &pos, NULL, NULL, NULL, NULL);
							if (pos>prjlen) prjlen = pos;
						}
					}
				}
			}
		}
	}
	return prjlen;
}

bool InsertSilence(const char* _undoTitle, double _pos, double _len)
{
	if (_pos>=0.0 && _len>0.0 && _pos<SNM_GetProjectLength())
	{
		if (_undoTitle)
			Undo_BeginBlock2(NULL);

		PreventUIRefresh(1);

		double timeSel1, timeSel2, d=_pos+_len;
		GetSet_LoopTimeRange2(NULL, false, false, &timeSel1, &timeSel2, false);
		GetSet_LoopTimeRange2(NULL, true, false, &_pos, &d, false);
		Main_OnCommand(40200, 0); // insert space at time sel

		// restore time sel, enlarge if needed (mimic native behavior)
		if (timeSel1>_pos) timeSel1+=_len;
		if (_pos<timeSel2) timeSel2+=_len;
		GetSet_LoopTimeRange2(NULL, true, false, &timeSel1, &timeSel2, false);

		PreventUIRefresh(-1);

		if (_undoTitle)
			Undo_EndBlock2(NULL, _undoTitle, UNDO_STATE_ALL);
		return true;
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// Select/open project
///////////////////////////////////////////////////////////////////////////////

void LoadOrSelectProject(const char* _fn, bool _newTab)
{
	if (!_fn)
		return;

	int i=0;
	bool found = false;
	ReaProject* prj = NULL;
	char fn[SNM_MAX_PATH] = "";
	while (!found && (prj = EnumProjects(i++, fn, sizeof(fn))))
		if (!_stricmp(_fn, fn))
			found = true;

	if (found)
	{
		SelectProjectInstance(prj);
	}
	else
	{
		if (_newTab) Main_OnCommand(41929, 0); // New project tab (ignore default template)
		Main_openProject((char*)_fn); // includes an undo point
	}
}

void SelectProjectJob::Perform() {
	if (ReaProject* proj = EnumProjects(GetIntValue(), NULL, 0)) // project number is 0-based
		SelectProjectInstance(proj);
}

double SelectProjectJob::GetCurrentValue()
{
	int i=-1;
	ReaProject *prj, *curprj=EnumProjects(-1, NULL, 0);
	while ((prj = EnumProjects(++i, NULL, 0)))
		if (prj == curprj)
			return i;
	return 0.0;
}

double SelectProjectJob::GetMaxValue()
{
	int i=-1;
	ReaProject *prj;
	while ((prj = EnumProjects(++i, NULL, 0)));
	return i-1;
}

void SelectProject(COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd) {
	ScheduledJob::Schedule(new SelectProjectJob(SNM_SCHEDJOB_DEFAULT_DELAY, _val, _valhw, _relmode));
}


///////////////////////////////////////////////////////////////////////////////
// Global and per-project startup actions
// Based on a registered timer so that everything has been initialized
///////////////////////////////////////////////////////////////////////////////

SWSProjConfig<WDL_FastString> g_prjActions;
WDL_FastString g_globalAction;

// per-project action timer armed via ProcessExtensionLine()
void ProjectStartupActionTimer()
{
	plugin_register("-timer",(void*)ProjectStartupActionTimer);
	if (int cmdId = NamedCommandLookup(g_prjActions.Get()->Get()))
	{
		Main_OnCommand(cmdId, 0);
#ifdef _SNM_DEBUG
		OutputDebugString("ProjectStartupActionTimer() - Performed project startup action '");
		OutputDebugString(g_prjActions.Get()->Get());
		OutputDebugString("'\n");
#endif
	}
}

// global action timer armed via SNM_Init()/OnInitTimer()
void GlobalStartupActionTimer()
{
	if (int cmdId = NamedCommandLookup(g_globalAction.Get()))
	{
		Main_OnCommand(cmdId, 0);
#ifdef _SNM_DEBUG
		OutputDebugString("GlobalStartupActionTimer() - Performed global startup action '");
		OutputDebugString(g_globalAction.Get());
		OutputDebugString("'\n");
#endif
	}
}

int PromptClearStartupAction(int _type, bool _clear)
{
	int r=0, cmdId=SNM_NamedCommandLookup(_type ? g_globalAction.Get() : g_prjActions.Get()->Get());
	if (cmdId)
	{
		WDL_FastString msg;
		if (!_type)
		{
			msg.AppendFormatted(512, _clear ?
                          __LOCALIZE_VERFMT("Are you sure you want to clear the project startup action: '%s'?","sws_startup_action") :
                          __LOCALIZE_VERFMT("Are you sure you want to replace the project startup action: '%s'?","sws_startup_action"),
                          kbd_getTextFromCmd(cmdId, NULL));
		}
		else
		{
			msg.AppendFormatted(512, _clear ?
                          __LOCALIZE_VERFMT("Are you sure you want to clear the global startup action: '%s'?","sws_startup_action") :
                          __LOCALIZE_VERFMT("Are you sure you want to replace the global startup action: '%s'?","sws_startup_action"),
                          kbd_getTextFromCmd(cmdId, NULL));
		}
		r = MessageBox(GetMainHwnd(), msg.Get(), __LOCALIZE("S&M - Confirmation","sws_mbox"), MB_YESNO);
	}
	return r;
}

void SetStartupAction(COMMAND_T* _ct)
{
	int type=(int)_ct->user;

	if (PromptClearStartupAction(type, false) == IDNO)
		return;

	char idstr[SNM_MAX_ACTION_CUSTID_LEN];
	lstrcpyn(idstr, __LOCALIZE("Paste command ID or identifier string here","sws_startup_action"), sizeof(idstr));
	if (PromptUserForString(GetMainHwnd(), SWS_CMD_SHORTNAME(_ct), idstr, sizeof(idstr), true))
	{
		WDL_FastString msg;
		if (int cmdId = SNM_NamedCommandLookup(idstr))
		{
			// more checks: http://forum.cockos.com/showpost.php?p=1252206&postcount=1618
			if (int tstNum = CheckSwsMacroScriptNumCustomId(idstr))
			{
				// localization note: msgs shared with the CA editor
				msg.SetFormatted(512, __LOCALIZE_VERFMT("%s failed: unreliable command ID '%s'!","sws_startup_action"), SWS_CMD_SHORTNAME(_ct), idstr);
				msg.Append("\r\n");

				if (tstNum==-1)
					msg.Append(__LOCALIZE("For SWS/S&M actions, you must use identifier strings (e.g. _SWS_ABOUT), not command IDs (e.g. 47145).\nTip: to copy such identifiers, right-click the action in the Actions window > Copy selected action cmdID/identifier string.","sws_startup_action"));
				else if (tstNum==-2)
					msg.Append(__LOCALIZE("For macros/scripts, you must use identifier strings (e.g. _f506bc780a0ab34b8fdedb67ed5d3649), not command IDs (e.g. 47145).\nTip: to copy such identifiers, right-click the macro/script in the Actions window > Copy selected action cmdID/identifier string.","sws_startup_action"));
				MessageBox(GetMainHwnd(), msg.Get(), __LOCALIZE("S&M - Error","sws_mbox"), MB_OK);
			}
			else
			{
				if (!type)
				{
					g_prjActions.Get()->Set(idstr);
					Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_MISCCFG, -1);

					msg.SetFormatted(512, __LOCALIZE_VERFMT("'%s' is defined as project startup action","sws_startup_action"), kbd_getTextFromCmd(cmdId, NULL));          
					char prjFn[SNM_MAX_PATH] = "";
					EnumProjects(-1, prjFn, sizeof(prjFn));
					if (*prjFn)
					{
						msg.Append("\r\n");
						msg.AppendFormatted(SNM_MAX_PATH, __LOCALIZE_VERFMT("for %s","sws_startup_action"), prjFn);
						msg.Append(".\r\n\r\n");
						msg.Append(__LOCALIZE("Note: do not forget to save this project","sws_startup_action"));
					}
				}
				else
				{
					g_globalAction.Set(idstr);
					WritePrivateProfileString("Misc", "GlobalStartupAction", idstr, g_SNM_IniFn.Get()); 

					msg.SetFormatted(512, __LOCALIZE_VERFMT("'%s' is defined as global startup action","sws_startup_action"), kbd_getTextFromCmd(cmdId, NULL));
				}
				msg.Append(".");
				MessageBox(GetMainHwnd(), msg.Get(), SWS_CMD_SHORTNAME(_ct), MB_OK);
			}
		}
		else
		{
			msg.SetFormatted(512, __LOCALIZE_VERFMT("%s failed: command ID or identifier string '%s' not found in the 'Main' section of the action list!","sws_startup_action"), SWS_CMD_SHORTNAME(_ct), idstr);
			MessageBox(GetMainHwnd(), msg.Get(), __LOCALIZE("S&M - Error","sws_mbox"), MB_OK);
		}
	}
}

void ClearStartupAction(COMMAND_T* _ct)
{
	int type=(int)_ct->user;
  
	if (PromptClearStartupAction(type, true)==IDYES)
	{
		if (!type)
		{
			g_prjActions.Get()->Set("");
			Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_MISCCFG, -1);
		}
		else
		{
			g_globalAction.Set("");
			WritePrivateProfileString("Misc", "GlobalStartupAction", NULL, g_SNM_IniFn.Get()); 
		}
	}
}

void ShowStartupActions(COMMAND_T* _ct)
{
	WDL_FastString msg(__LOCALIZE("No project startup action is defined","sws_startup_action"));
	if (int cmdId = SNM_NamedCommandLookup(g_prjActions.Get()->Get()))
		msg.SetFormatted(512, __LOCALIZE_VERFMT("'%s' is defined as project startup action", "sws_startup_action"), kbd_getTextFromCmd(cmdId, NULL));

	char prjFn[SNM_MAX_PATH] = "";
	EnumProjects(-1, prjFn, sizeof(prjFn));
	if (*prjFn)
	{
		msg.Append("\r\n");
		msg.AppendFormatted(SNM_MAX_PATH, __LOCALIZE_VERFMT("for %s", "sws_startup_action"), prjFn);
	}
	msg.Append(".");
	msg.Append("\r\n\r\n");

	if (int cmdId = SNM_NamedCommandLookup(g_globalAction.Get()))
	{
		msg.AppendFormatted(512, __LOCALIZE_VERFMT("'%s' is defined as global startup action", "sws_startup_action"), kbd_getTextFromCmd(cmdId, NULL));
	}
	else
	{
		msg.Append(__LOCALIZE("No global startup action is defined","sws_startup_action"));
	}
	msg.Append(".");

	MessageBox(GetMainHwnd(), msg.Get(), SWS_CMD_SHORTNAME(_ct), MB_OK);
}

WDL_FastString* GetGlobalStartupAction()
{
	return &g_globalAction;
}

SWSProjConfig<WDL_FastString>* GetProjectLoadAction()
{
	return &g_prjActions;
}

static bool ProcessExtensionLine(const char *line, ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	LineParser lp(false);
	if (lp.parse(line) || lp.getnumtokens() < 1)
		return false;

	if (!strcmp(lp.gettoken_str(0), "S&M_PROJACTION"))
	{
		g_prjActions.Get()->Set(lp.gettoken_str(1));

		if (!isUndo && g_prjActions.Get()->GetLength() && IsActiveProjectInLoadSave())
			plugin_register("timer",(void*)ProjectStartupActionTimer);
		return true;
	}
	return false;
}

static void SaveExtensionConfig(ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	char line[SNM_MAX_CHUNK_LINE_LENGTH] = "";
	if (ctx && g_prjActions.Get()->GetLength())
		if (snprintfStrict(line, sizeof(line), "S&M_PROJACTION %s", g_prjActions.Get()->Get()) > 0)
			ctx->AddLine("%s", line);
}

static void BeginLoadProjectState(bool isUndo, struct project_config_extension_t *reg) {
	g_prjActions.Cleanup();
	g_prjActions.Get()->Set("");
}

static project_config_extension_t s_projectconfig = {
	ProcessExtensionLine, SaveExtensionConfig, BeginLoadProjectState, NULL
};

int SNM_ProjectInit()
{
  char buf[SNM_MAX_ACTION_CUSTID_LEN]="";
  GetPrivateProfileString("Misc", "GlobalStartupAction", "", buf, sizeof(buf), g_SNM_IniFn.Get());
  g_globalAction.Set(buf);
  return plugin_register("projectconfig", &s_projectconfig);
}

void SNM_ProjectExit()
{
	plugin_register("-projectconfig", &s_projectconfig);
	plugin_register("-timer",(void*)ProjectStartupActionTimer);
}


///////////////////////////////////////////////////////////////////////////////
// Misc project actions
///////////////////////////////////////////////////////////////////////////////

char g_lastSilenceVal[3][64] = {"10.0", "1.0", "512"};

void InsertSilence(COMMAND_T* _ct)
{
	char val[64]="";
	lstrcpyn(val, g_lastSilenceVal[(int)_ct->user], sizeof(val));

	if (PromptUserForString(GetMainHwnd(), SWS_CMD_SHORTNAME(_ct), val, sizeof(val)) && *val)
	{
		double pos = GetCursorPositionEx(NULL), len = 0.0;
		switch ((int)_ct->user)
		{
			case 0: // s
				len = parse_timestr_len(val, pos, 3);
				break;
			case 1: // meas.beat
			{
/* no! would not take get tempo markers into account since 'val' is not inserted yet
				len = parse_timestr_len(val, pos, 2);
				break;
*/
				int in_meas = atoi(val);
				double in_beats = 0.0;
				char* p = strchr(val, '.');
				if (p && p[1]) in_beats = atof(p+1);

				double bpm; int num, den;
				TimeMap_GetTimeSigAtTime(NULL, pos, &num, &den, &bpm);
				len = in_beats*(60.0/bpm) + in_meas*((240.0*num/den)/bpm);
				break;
			}
			case 2: // smp
				len = parse_timestr_len(val, pos, 4);
				break;
		}

		if (len>0.0) {
			lstrcpyn(g_lastSilenceVal[(int)_ct->user], val, sizeof(val));
			InsertSilence(SWS_CMD_SHORTNAME(_ct), pos, len); // includes an undo point
		}
		else
			MessageBox(GetMainHwnd(), __LOCALIZE("Invalid input!","sws_mbox"), __LOCALIZE("S&M - Error","sws_mbox"), MB_OK);
	}
}

void OpenProjectPathInExplorerFinder(COMMAND_T*)
{
	char path[SNM_MAX_PATH] = "";
	GetProjectPath(path, sizeof(path));
	if (*path)
		ShellExecute(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
}

