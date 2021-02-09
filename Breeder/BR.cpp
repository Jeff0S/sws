/*****************************************************************************
/ BR.cpp
/
/ Copyright (c) 2012-2015 Dominik Martin Drzic
/ http://forum.cockos.com/member.php?u=27094
/ http://github.com/reaper-oss/sws
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

#include "BR.h"
#include "BR_ContextualToolbars.h"
#include "BR_ContinuousActions.h"
#include "BR_Envelope.h"
#include "BR_Loudness.h"
#include "BR_MidiEditor.h"
#include "BR_Misc.h"
#include "BR_ProjState.h"
#include "BR_Tempo.h"
#include "BR_Update.h"
#include "BR_Util.h"
#include "../SnM/SnM.h"
#include "../SnM/SnM_Util.h"

#include <WDL/localize/localize.h>

/******************************************************************************
* Globals                                                                     *
******************************************************************************/
static bool       g_deferRefreshToolbar = false;
static COMMAND_T* g_nextActionApplyer   = NULL;
static int        g_nextActionToApply   = 0;
static int        g_nextActionLoCmd     = 0;
static int        g_nextActionHiCmd     = 0;
static set<int>   g_nextActionApplyers;

int SCROLLBAR_W = 0; // legacy

/******************************************************************************
* Command hook                                                                *
******************************************************************************/
static void DeferRefreshToolbar()
{
	if (!g_deferRefreshToolbar)
	{
		RefreshToolbar2(SECTION_MIDI_EDITOR, 0);
		plugin_register("-timer", (void*)DeferRefreshToolbar);
	}
	else
		g_deferRefreshToolbar = false;
}

bool BR_GlobalActionHook (int cmd, int val, int valhw, int relmode, HWND hwnd)
{
	if (cmd == 40153) // Item: Open in built-in MIDI editor (set default behavior in preferences)
	{
		g_deferRefreshToolbar = true;
		plugin_register("timer", (void*)DeferRefreshToolbar); // Make sure to refresh various SWS/NF grid actions in MIDI editor
	}

	if (g_nextActionApplyer)
	{
		COMMAND_T* nextActionApplyer = g_nextActionApplyer;
		g_nextActionApplyer = NULL; // due to reentrancy, set as NULL before running the command

		g_nextActionToApply = cmd;
		if      (nextActionApplyer->doCommand) nextActionApplyer->doCommand(nextActionApplyer);
		else if (nextActionApplyer->onAction)  nextActionApplyer->onAction(nextActionApplyer, val, valhw, relmode, hwnd);

		g_nextActionToApply = 0;
		g_nextActionApplyer = NULL; // because BR_SwsActionHook might set it to something else during the call to this function

		return true;
	}
	return false;
}

bool BR_SwsActionHook (COMMAND_T* ct, int flagOrRelmode, HWND hwnd)
{
	int cmd = ct->accel.accel.cmd;

	// Action applies next action
	if (cmd >= g_nextActionLoCmd && cmd <= g_nextActionHiCmd && g_nextActionApplyers.find(cmd) != g_nextActionApplyers.end())
	{
		g_nextActionApplyer = SWSGetCommandByID(cmd);
		return true;
	}

	// Action is continuous action
	if (ContinuousActionHook(ct, cmd, flagOrRelmode, hwnd))
		return true;

	return false;
}

void BR_MenuHook (COMMAND_T* ct, HMENU menu, int id)
{
	if (ct->doCommand == SetOptionsFX)
	{
		// Disable the action "Flush FX on stop" if the option "Run FX when stopped" is turned off
		if ((int)ct->user == 1)
			EnableMenuItem(menu, id,
				(GetBit(*ConfigVar<int>("runallonstop"), 0) ? MF_ENABLED : MF_GRAYED) | MF_BYPOSITION);

		// Disable the action to set "Run FX after stopping for" if the option "Run FX when stopped" is turned on
		else if ((int)ct->user <= 0)
			EnableMenuItem(menu, id, (!GetBit(*ConfigVar<int>("runallonstop"), 0) ? MF_ENABLED : MF_GRAYED) | MF_BYPOSITION);
	}
}

int BR_GetNextActionToApply ()
{
	return g_nextActionToApply;
}

/******************************************************************************
* CSurf                                                                       *
******************************************************************************/
void BR_CSurf_SetPlayState (bool play, bool pause, bool rec)
{
	static const vector<void(*)(bool,bool,bool)>* s_functions = NULL;
	if (!s_functions) RegisterCsurfPlayState(false, NULL, &s_functions);

	if (s_functions->size() > 0)
	{
		for (size_t i = 0; i < s_functions->size(); ++i)
		{
			if (void(*func)(bool,bool,bool) = s_functions->at(i))
				func(play, pause, rec);
		}
		RegisterCsurfPlayState(false, NULL, NULL, true);
	}
}

void BR_CSurf_OnTrackSelection (MediaTrack* track)
{
	ExecuteTrackSelAction();
}

int BR_CSurf_Extended(int call, void* parm1, void* parm2, void* parm3)
{
	if (call == CSURF_EXT_RESET)
	{
		LoudnessUpdate();
	}
	else if (call == CSURF_EXT_SETSENDVOLUME || call == CSURF_EXT_SETSENDPAN)
	{
		if (parm1 && parm2)
		{
			BR_EnvType type = (call == CSURF_EXT_SETSENDPAN) ? PAN : VOLUME;
			GetSetLastAdjustedSend(true, (MediaTrack**)&parm1, (int*)parm2, &type);
		}
	}
	return 0;
}

/******************************************************************************
* Continuous actions                                                          *
******************************************************************************/
void BR_RegisterContinuousActions ()
{
	SetEnvPointMouseValueInit();
	PlaybackAtMouseCursorInit ();
	MoveGridToMouseInit();
}

/******************************************************************************
* Commands                                                                    *
******************************************************************************/
//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions
static COMMAND_T g_commandTable[] =
{
	/******************************************************************************
	* Contextual toolbars                                                         *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Contextual toolbars..." }, "BR_CONTEXTUAL_TOOLBARS_PREF", ContextualToolbarsOptions, NULL, 0, IsContextualToolbarsOptionsVisible},

	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 1" }, "BR_CONTEXTUAL_TOOLBAR_01",              ToggleContextualToolbar, NULL, 1, IsContextualToolbarVisible},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 1" }, "BR_ME_CONTEXTUAL_TOOLBAR_01",           NULL, NULL, 1, IsContextualToolbarVisible, SECTION_MIDI_EDITOR,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 1" }, "BR_ML_CONTEXTUAL_TOOLBAR_01",           NULL, NULL, 1, IsContextualToolbarVisible, SECTION_MIDI_LIST,       ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 1" }, "BR_MI_CONTEXTUAL_TOOLBAR_01",           NULL, NULL, 1, IsContextualToolbarVisible, SECTION_MIDI_INLINE,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 1" }, "BR_MX_CONTEXTUAL_TOOLBAR_01",           NULL, NULL, 1, IsContextualToolbarVisible, SECTION_MEDIA_EXPLORER,  ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 2" }, "BR_CONTEXTUAL_TOOLBAR_02",              ToggleContextualToolbar, NULL, 2, IsContextualToolbarVisible},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 2" }, "BR_ME_CONTEXTUAL_TOOLBAR_02",           NULL, NULL, 2, IsContextualToolbarVisible, SECTION_MIDI_EDITOR,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 2" }, "BR_ML_CONTEXTUAL_TOOLBAR_02",           NULL, NULL, 2, IsContextualToolbarVisible, SECTION_MIDI_LIST,       ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 2" }, "BR_MI_CONTEXTUAL_TOOLBAR_02",           NULL, NULL, 2, IsContextualToolbarVisible, SECTION_MIDI_INLINE,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 2" }, "BR_MX_CONTEXTUAL_TOOLBAR_02",           NULL, NULL, 2, IsContextualToolbarVisible, SECTION_MEDIA_EXPLORER,  ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 3" }, "BR_CONTEXTUAL_TOOLBAR_03",              ToggleContextualToolbar, NULL, 3, IsContextualToolbarVisible},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 3" }, "BR_ME_CONTEXTUAL_TOOLBAR_03",           NULL, NULL, 3, IsContextualToolbarVisible, SECTION_MIDI_EDITOR,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 3" }, "BR_ML_CONTEXTUAL_TOOLBAR_03",           NULL, NULL, 3, IsContextualToolbarVisible, SECTION_MIDI_LIST,       ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 3" }, "BR_MI_CONTEXTUAL_TOOLBAR_03",           NULL, NULL, 3, IsContextualToolbarVisible, SECTION_MIDI_INLINE,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 3" }, "BR_MX_CONTEXTUAL_TOOLBAR_03",           NULL, NULL, 3, IsContextualToolbarVisible, SECTION_MEDIA_EXPLORER,  ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 4" }, "BR_CONTEXTUAL_TOOLBAR_04",              ToggleContextualToolbar, NULL, 4, IsContextualToolbarVisible},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 4" }, "BR_ME_CONTEXTUAL_TOOLBAR_04",           NULL, NULL, 4, IsContextualToolbarVisible, SECTION_MIDI_EDITOR,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 4" }, "BR_ML_CONTEXTUAL_TOOLBAR_04",           NULL, NULL, 4, IsContextualToolbarVisible, SECTION_MIDI_LIST,       ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 4" }, "BR_MI_CONTEXTUAL_TOOLBAR_04",           NULL, NULL, 4, IsContextualToolbarVisible, SECTION_MIDI_INLINE,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 4" }, "BR_MX_CONTEXTUAL_TOOLBAR_04",           NULL, NULL, 4, IsContextualToolbarVisible, SECTION_MEDIA_EXPLORER,  ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 5" }, "BR_CONTEXTUAL_TOOLBAR_05",              ToggleContextualToolbar, NULL, 5, IsContextualToolbarVisible},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 5" }, "BR_ME_CONTEXTUAL_TOOLBAR_05",           NULL, NULL, 5, IsContextualToolbarVisible, SECTION_MIDI_EDITOR,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 5" }, "BR_ML_CONTEXTUAL_TOOLBAR_05",           NULL, NULL, 5, IsContextualToolbarVisible, SECTION_MIDI_LIST,       ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 5" }, "BR_MI_CONTEXTUAL_TOOLBAR_05",           NULL, NULL, 5, IsContextualToolbarVisible, SECTION_MIDI_INLINE,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 5" }, "BR_MX_CONTEXTUAL_TOOLBAR_05",           NULL, NULL, 5, IsContextualToolbarVisible, SECTION_MEDIA_EXPLORER,  ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 6" }, "BR_CONTEXTUAL_TOOLBAR_06",              ToggleContextualToolbar, NULL, 6, IsContextualToolbarVisible},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 6" }, "BR_ME_CONTEXTUAL_TOOLBAR_06",           NULL, NULL, 6, IsContextualToolbarVisible, SECTION_MIDI_EDITOR,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 6" }, "BR_ML_CONTEXTUAL_TOOLBAR_06",           NULL, NULL, 6, IsContextualToolbarVisible, SECTION_MIDI_LIST,       ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 6" }, "BR_MI_CONTEXTUAL_TOOLBAR_06",           NULL, NULL, 6, IsContextualToolbarVisible, SECTION_MIDI_INLINE,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 6" }, "BR_MX_CONTEXTUAL_TOOLBAR_06",           NULL, NULL, 6, IsContextualToolbarVisible, SECTION_MEDIA_EXPLORER,  ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 7" }, "BR_CONTEXTUAL_TOOLBAR_07",              ToggleContextualToolbar, NULL, 7, IsContextualToolbarVisible},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 7" }, "BR_ME_CONTEXTUAL_TOOLBAR_07",           NULL, NULL, 7, IsContextualToolbarVisible, SECTION_MIDI_EDITOR,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 7" }, "BR_ML_CONTEXTUAL_TOOLBAR_07",           NULL, NULL, 7, IsContextualToolbarVisible, SECTION_MIDI_LIST,       ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 7" }, "BR_MI_CONTEXTUAL_TOOLBAR_07",           NULL, NULL, 7, IsContextualToolbarVisible, SECTION_MIDI_INLINE,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 7" }, "BR_MX_CONTEXTUAL_TOOLBAR_07",           NULL, NULL, 7, IsContextualToolbarVisible, SECTION_MEDIA_EXPLORER,  ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 8" }, "BR_CONTEXTUAL_TOOLBAR_08",              ToggleContextualToolbar, NULL, 8, IsContextualToolbarVisible},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 8" }, "BR_ME_CONTEXTUAL_TOOLBAR_08",           NULL, NULL, 8, IsContextualToolbarVisible, SECTION_MIDI_EDITOR,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 8" }, "BR_ML_CONTEXTUAL_TOOLBAR_08",           NULL, NULL, 8, IsContextualToolbarVisible, SECTION_MIDI_LIST,       ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 8" }, "BR_MI_CONTEXTUAL_TOOLBAR_08",           NULL, NULL, 8, IsContextualToolbarVisible, SECTION_MIDI_INLINE,     ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Open/close contextual toolbar under mouse cursor, preset 8" }, "BR_MX_CONTEXTUAL_TOOLBAR_08",           NULL, NULL, 8, IsContextualToolbarVisible, SECTION_MEDIA_EXPLORER,  ToggleContextualToolbar},

	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 1" }, "BR_EXC_CONTEXTUAL_TOOLBAR_01",    ToggleContextualToolbar, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 1" }, "BR_ME_EXC_CONTEXTUAL_TOOLBAR_01", NULL, NULL, -1, NULL, SECTION_MIDI_EDITOR,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 1" }, "BR_ML_EXC_CONTEXTUAL_TOOLBAR_01", NULL, NULL, -1, NULL, SECTION_MIDI_LIST,      ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 1" }, "BR_MI_EXC_CONTEXTUAL_TOOLBAR_01", NULL, NULL, -1, NULL, SECTION_MIDI_INLINE,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 1" }, "BR_MX_EXC_CONTEXTUAL_TOOLBAR_01", NULL, NULL, -1, NULL, SECTION_MEDIA_EXPLORER, ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 2" }, "BR_EXC_CONTEXTUAL_TOOLBAR_02",    ToggleContextualToolbar, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 2" }, "BR_ME_EXC_CONTEXTUAL_TOOLBAR_02", NULL, NULL, -2, NULL, SECTION_MIDI_EDITOR,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 2" }, "BR_ML_EXC_CONTEXTUAL_TOOLBAR_02", NULL, NULL, -2, NULL, SECTION_MIDI_LIST,      ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 2" }, "BR_MI_EXC_CONTEXTUAL_TOOLBAR_02", NULL, NULL, -2, NULL, SECTION_MIDI_INLINE,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 2" }, "BR_MX_EXC_CONTEXTUAL_TOOLBAR_02", NULL, NULL, -2, NULL, SECTION_MEDIA_EXPLORER, ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 3" }, "BR_EXC_CONTEXTUAL_TOOLBAR_03",    ToggleContextualToolbar, NULL, -3},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 3" }, "BR_ME_EXC_CONTEXTUAL_TOOLBAR_03", NULL, NULL, -3, NULL, SECTION_MIDI_EDITOR,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 3" }, "BR_ML_EXC_CONTEXTUAL_TOOLBAR_03", NULL, NULL, -3, NULL, SECTION_MIDI_LIST,      ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 3" }, "BR_MI_EXC_CONTEXTUAL_TOOLBAR_03", NULL, NULL, -3, NULL, SECTION_MIDI_INLINE,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 3" }, "BR_MX_EXC_CONTEXTUAL_TOOLBAR_03", NULL, NULL, -3, NULL, SECTION_MEDIA_EXPLORER, ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 4" }, "BR_EXC_CONTEXTUAL_TOOLBAR_04",    ToggleContextualToolbar, NULL, -4},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 4" }, "BR_ME_EXC_CONTEXTUAL_TOOLBAR_04", NULL, NULL, -4, NULL, SECTION_MIDI_EDITOR,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 4" }, "BR_ML_EXC_CONTEXTUAL_TOOLBAR_04", NULL, NULL, -4, NULL, SECTION_MIDI_LIST,      ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 4" }, "BR_MI_EXC_CONTEXTUAL_TOOLBAR_04", NULL, NULL, -4, NULL, SECTION_MIDI_INLINE,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 4" }, "BR_MX_EXC_CONTEXTUAL_TOOLBAR_04", NULL, NULL, -4, NULL, SECTION_MEDIA_EXPLORER, ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 5" }, "BR_EXC_CONTEXTUAL_TOOLBAR_05",    ToggleContextualToolbar, NULL, -5},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 5" }, "BR_ME_EXC_CONTEXTUAL_TOOLBAR_05", NULL, NULL, -5, NULL, SECTION_MIDI_EDITOR,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 5" }, "BR_ML_EXC_CONTEXTUAL_TOOLBAR_05", NULL, NULL, -5, NULL, SECTION_MIDI_LIST,      ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 5" }, "BR_MI_EXC_CONTEXTUAL_TOOLBAR_05", NULL, NULL, -5, NULL, SECTION_MIDI_INLINE,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 5" }, "BR_MX_EXC_CONTEXTUAL_TOOLBAR_05", NULL, NULL, -5, NULL, SECTION_MEDIA_EXPLORER, ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 6" }, "BR_EXC_CONTEXTUAL_TOOLBAR_06",    ToggleContextualToolbar, NULL, -6},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 6" }, "BR_ME_EXC_CONTEXTUAL_TOOLBAR_06", NULL, NULL, -6, NULL, SECTION_MIDI_EDITOR,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 6" }, "BR_ML_EXC_CONTEXTUAL_TOOLBAR_06", NULL, NULL, -6, NULL, SECTION_MIDI_LIST,      ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 6" }, "BR_MI_EXC_CONTEXTUAL_TOOLBAR_06", NULL, NULL, -6, NULL, SECTION_MIDI_INLINE,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 6" }, "BR_MX_EXC_CONTEXTUAL_TOOLBAR_06", NULL, NULL, -6, NULL, SECTION_MEDIA_EXPLORER, ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 7" }, "BR_EXC_CONTEXTUAL_TOOLBAR_07",    ToggleContextualToolbar, NULL, -7},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 7" }, "BR_ME_EXC_CONTEXTUAL_TOOLBAR_07", NULL, NULL, -7, NULL, SECTION_MIDI_EDITOR,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 7" }, "BR_ML_EXC_CONTEXTUAL_TOOLBAR_07", NULL, NULL, -7, NULL, SECTION_MIDI_LIST,      ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 7" }, "BR_MI_EXC_CONTEXTUAL_TOOLBAR_07", NULL, NULL, -7, NULL, SECTION_MIDI_INLINE,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 7" }, "BR_MX_EXC_CONTEXTUAL_TOOLBAR_07", NULL, NULL, -7, NULL, SECTION_MEDIA_EXPLORER, ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 8" }, "BR_EXC_CONTEXTUAL_TOOLBAR_08",    ToggleContextualToolbar, NULL, -8},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 8" }, "BR_ME_EXC_CONTEXTUAL_TOOLBAR_08", NULL, NULL, -8, NULL, SECTION_MIDI_EDITOR,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 8" }, "BR_ML_EXC_CONTEXTUAL_TOOLBAR_08", NULL, NULL, -8, NULL, SECTION_MIDI_LIST,      ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 8" }, "BR_MI_EXC_CONTEXTUAL_TOOLBAR_08", NULL, NULL, -8, NULL, SECTION_MIDI_INLINE,    ToggleContextualToolbar},
	{ { DEFACCEL, "SWS/BR: Exclusive toggle contextual toolbar under mouse cursor, preset 8" }, "BR_MX_EXC_CONTEXTUAL_TOOLBAR_08", NULL, NULL, -8, NULL, SECTION_MEDIA_EXPLORER, ToggleContextualToolbar},

	/******************************************************************************
	* Envelopes - Misc                                                            *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Move edit cursor to next envelope point" },                                                                                                     "SWS_BRMOVEEDITTONEXTENV",            CursorToEnv1, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Move edit cursor to next envelope point and select it" },                                                                                       "SWS_BRMOVEEDITSELNEXTENV",           CursorToEnv1, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Move edit cursor to next envelope point and add to selection" },                                                                                "SWS_BRMOVEEDITTONEXTENVADDSELL",     CursorToEnv2, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Move edit cursor to previous envelope point" },                                                                                                 "SWS_BRMOVEEDITTOPREVENV",            CursorToEnv1, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Move edit cursor to previous envelope point and select it" },                                                                                   "SWS_BRMOVEEDITSELPREVENV",           CursorToEnv1, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Move edit cursor to previous envelope point and add to selection" },                                                                            "SWS_BRMOVEEDITTOPREVENVADDSELL",     CursorToEnv2, NULL, -1},

	{ { DEFACCEL, "SWS/BR: Select next envelope point" },                                                                                                                  "BR_ENV_SEL_NEXT_POINT",              SelNextPrevEnvPoint, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Select previous envelope point" },                                                                                                              "BR_ENV_SEL_PREV_POINT",              SelNextPrevEnvPoint, NULL, -1},

	{ { DEFACCEL, "SWS/BR: Expand envelope point selection to the right" },                                                                                                "BR_ENV_SEL_EXPAND_RIGHT",            ExpandEnvSel,    NULL, 1},
	{ { DEFACCEL, "SWS/BR: Expand envelope point selection to the left" },                                                                                                 "BR_ENV_SEL_EXPAND_LEFT",             ExpandEnvSel,    NULL, -1},
	{ { DEFACCEL, "SWS/BR: Expand envelope point selection to the right (end point only)" },                                                                               "BR_ENV_SEL_EXPAND_RIGHT_END",        ExpandEnvSelEnd, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Expand envelope point selection to the left (end point only)" },                                                                                "BR_ENV_SEL_EXPAND_L_END",            ExpandEnvSelEnd, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Shrink envelope point selection from the right" },                                                                                              "BR_ENV_SEL_SHRINK_RIGHT",            ShrinkEnvSel,    NULL, 1},
	{ { DEFACCEL, "SWS/BR: Shrink envelope point selection from the left" },                                                                                               "BR_ENV_SEL_SHRINK_LEFT",             ShrinkEnvSel,    NULL, -1},
	{ { DEFACCEL, "SWS/BR: Shrink envelope point selection from the right (end point only)" },                                                                             "BR_ENV_SEL_SHRINK_RIGHT_END",        ShrinkEnvSelEnd, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Shrink envelope point selection from the left (end point only)" },                                                                              "BR_ENV_SEL_SHRINK_LEFT_END",         ShrinkEnvSelEnd, NULL, -1},

	{ { DEFACCEL, "SWS/BR: Select envelope points on grid" },                                                                                                              "BR_ENV_SEL_PT_GRID",                 EnvPointsGrid, NULL, BINARY(1100)},
	{ { DEFACCEL, "SWS/BR: Select envelope points on grid (obey time selection, if any)" },                                                                                "BR_ENV_SEL_PT_GRID_TS",              EnvPointsGrid, NULL, BINARY(1101)},
	{ { DEFACCEL, "SWS/BR: Select envelope points between grid" },                                                                                                         "BR_ENV_SEL_PT_NO_GRID",              EnvPointsGrid, NULL, BINARY(0100)},
	{ { DEFACCEL, "SWS/BR: Select envelope points between grid (obey time selection, if any)" },                                                                           "BR_ENV_SEL_PT_NO_GRID_TS",           EnvPointsGrid, NULL, BINARY(0101)},
	{ { DEFACCEL, "SWS/BR: Add envelope points located on grid to existing selection" },                                                                                   "BR_ENV_ADD_SEL_PT_GRID",             EnvPointsGrid, NULL, BINARY(1000)},
	{ { DEFACCEL, "SWS/BR: Add envelope points located on grid to existing selection (obey time selection, if any)" },                                                     "BR_ENV_ADD_SEL_PT_GRID_TS",          EnvPointsGrid, NULL, BINARY(1001)},
	{ { DEFACCEL, "SWS/BR: Add envelope points located between grid to existing selection" },                                                                              "BR_ENV_ADD_SEL_PT_NO_GRID",          EnvPointsGrid, NULL, BINARY(0000)},
	{ { DEFACCEL, "SWS/BR: Add envelope points located between grid to existing selection (obey time selection, if any)" },                                                "BR_ENV_ADD_SEL_PT_NO_GRID_TS",       EnvPointsGrid, NULL, BINARY(0001)},
	{ { DEFACCEL, "SWS/BR: Delete envelope points on grid" },                                                                                                              "BR_ENV_DEL_PT_GRID",                 EnvPointsGrid, NULL, BINARY(1010)},
	{ { DEFACCEL, "SWS/BR: Delete envelope points on grid (obey time selection, if any)" },                                                                                "BR_ENV_DEL_PT_GRID_TS",              EnvPointsGrid, NULL, BINARY(1011)},
	{ { DEFACCEL, "SWS/BR: Delete envelope points between grid" },                                                                                                         "BR_ENV_DEL_PT_NO_GRID",              EnvPointsGrid, NULL, BINARY(0010)},
	{ { DEFACCEL, "SWS/BR: Delete envelope points between grid (obey time selection, if any)" },                                                                           "BR_ENV_DEL_PT_NO_GRID_TS",           EnvPointsGrid, NULL, BINARY(0011)},
	{ { DEFACCEL, "SWS/BR: Insert envelope points on grid using shape of the previous point" },                                                                            "BR_ENV_INS_PT_GRID",                 CreateEnvPointsGrid, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Insert envelope points on grid using shape of the previous point (obey time selection, if any)" },                                              "BR_ENV_INS_PT_GRID_TS",              CreateEnvPointsGrid, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Convert selected points in selected envelope to CC events in last clicked CC lane in last active MIDI editor" },                                "BR_ENV_TO_CC",                       EnvPointsToCC, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Convert selected points in selected envelope to CC events in last clicked CC lane in last active MIDI editor (clear existing events)" },        "BR_ENV_TO_CC_CLEAR",                 EnvPointsToCC, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Convert selected envelope's curve in time selection to CC events in last clicked CC lane in last active MIDI editor" },                         "BR_ENV_CURVE_TO_CC",                 EnvPointsToCC, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Convert selected envelope's curve in time selection to CC events in last clicked CC lane in last active MIDI editor (clear existing events)" }, "BR_ENV_CURVE_TO_CC_CLEAR",           EnvPointsToCC, NULL, -3},

	{ { DEFACCEL, "SWS/BR: Shift envelope point selection left" },                                                                                                         "BR_ENV_SHIFT_SEL_LEFT",              ShiftEnvSelection, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Shift envelope point selection right" },                                                                                                        "BR_ENV_SHIFT_SEL_RIGHT",             ShiftEnvSelection, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Select peaks in envelope (add to selection)" },                                                                                                 "BR_ENV_SEL_PEAKS_ADD",               PeaksDipsEnv, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Select peaks in envelope" },                                                                                                                    "BR_ENV_SEL_PEAKS",                   PeaksDipsEnv, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Select dips in envelope (add to selection)" },                                                                                                  "BR_ENV_SEL_DIPS_ADD",                PeaksDipsEnv, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Select dips in envelope" },                                                                                                                     "BR_ENV_SEL_DIPS",                    PeaksDipsEnv, NULL, -2},

	{ { DEFACCEL, "SWS/BR: Unselect envelope points outside time selection" },                                                                                             "BR_ENV_UNSEL_OUT_TIME_SEL",          SelEnvTimeSel, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Unselect envelope points in time selection" },                                                                                                  "BR_ENV_UNSEL_IN_TIME_SEL",           SelEnvTimeSel, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Set selected envelope points to next point's value" },                                                                                          "BR_SET_ENV_TO_NEXT_VAL",             SetEnvValToNextPrev, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Set selected envelope points to previous point's value" },                                                                                      "BR_SET_ENV_TO_PREV_VAL",             SetEnvValToNextPrev, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Set selected envelope points to last selected point's value" },                                                                                 "BR_SET_ENV_TO_LAST_SEL_VAL",         SetEnvValToNextPrev, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Set selected envelope points to first selected point's value" },                                                                                "BR_SET_ENV_TO_FIRST_SEL_VAL",        SetEnvValToNextPrev, NULL, -2},

	{ { DEFACCEL, "SWS/BR: Move closest envelope point to edit cursor" },                                                                                                  "BR_MOVE_CLOSEST_ENV_ECURSOR",        MoveEnvPointToEditCursor, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Move closest selected envelope point to edit cursor" },                                                                                         "BR_MOVE_CLOSEST_SEL_ENV_ECURSOR",    MoveEnvPointToEditCursor, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Insert 2 envelope points at time selection" },                                                                                                  "BR_INSERT_2_ENV_POINT_TIME_SEL",     Insert2EnvPointsTimeSelection, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Insert 2 envelope points at time selection to all visible track envelopes" },                                                                   "BR_INS_2_ENV_POINT_TIME_SEL_ALL",    Insert2EnvPointsTimeSelection, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Insert 2 envelope points at time selection to all visible track envelopes in selected tracks" },                                                "BR_INS_2_ENV_POINT_TIME_SEL_TRACKS", Insert2EnvPointsTimeSelection, NULL, 2},

	{ { DEFACCEL, "SWS/BR: Copy selected points in selected envelope to all visible envelopes in selected tracks" },                                                       "BR_COPY_ENV_PT_SEL_TR_ENVS_VIS",       CopyEnvPoints, NULL, BINARY(0000)},
	{ { DEFACCEL, "SWS/BR: Copy selected points in selected envelope to all visible record-armed envelopes in selected tracks" },                                          "BR_COPY_ENV_PT_SEL_TR_ENVS_REC",       CopyEnvPoints, NULL, BINARY(0001)},
	{ { DEFACCEL, "SWS/BR: Copy points in time selection in selected envelope to all visible envelopes in selected tracks" },                                              "BR_COPY_ENV_TS_SEL_TR_ENVS_VIS",       CopyEnvPoints, NULL, BINARY(0010)},
	{ { DEFACCEL, "SWS/BR: Copy points in time selection in selected envelope to all visible record-armed in envelopes of selected tracks" },                              "BR_COPY_ENV_TS_SEL_TR_ENVS_REC",       CopyEnvPoints, NULL, BINARY(0011)},

	{ { DEFACCEL, "SWS/BR: Copy selected points in selected envelope to all visible envelopes in selected tracks (paste at edit cursor)" },                                "BR_COPY_ENV_PT_SEL_TR_ENVS_VIS_CUR",   CopyEnvPoints, NULL, BINARY(0100)},
	{ { DEFACCEL, "SWS/BR: Copy selected points in selected envelope to all visible record-armed envelopes in selected tracks (paste at edit cursor)" },                   "BR_COPY_ENV_PT_SEL_TR_ENVS_REC_CUR",   CopyEnvPoints, NULL, BINARY(0101)},
	{ { DEFACCEL, "SWS/BR: Copy points in time selection in selected envelope to all visible envelopes in selected tracks (paste at edit cursor)" },                       "BR_COPY_ENV_TS_SEL_TR_ENVS_VIS_CUR",   CopyEnvPoints, NULL, BINARY(0110)},
	{ { DEFACCEL, "SWS/BR: Copy points in time selection in selected envelope to all visible record-armed in envelopes of selected tracks (paste at edit cursor)" },       "BR_COPY_ENV_TS_SEL_TR_ENVS_REC_CUR",   CopyEnvPoints, NULL, BINARY(0111)},

	{ { DEFACCEL, "SWS/BR: Copy selected points in selected envelope to envelope at mouse cursor" },                                                                       "BR_COPY_ENV_PT_SEL_TR_ENVS_MOUSE",     CopyEnvPoints, NULL, BINARY(1000)},
	{ { DEFACCEL, "SWS/BR: Copy points in time selection in selected envelope to envelope at mouse cursor" },                                                              "BR_COPY_ENV_TS_SEL_TR_ENVS_MOUSE",     CopyEnvPoints, NULL, BINARY(1010)},
	{ { DEFACCEL, "SWS/BR: Copy selected points in selected envelope to to envelope at mouse cursor (paste at edit cursor)" },                                             "BR_COPY_ENV_PT_SEL_TR_ENVS_MOUSE_CUR", CopyEnvPoints, NULL, BINARY(1100) },
	{ { DEFACCEL, "SWS/BR: Copy points in time selection in selected envelope to envelope at mouse cursor (paste at edit cursor)" },                                       "BR_COPY_ENV_TS_SEL_TR_ENVS_MOUSE_CUR", CopyEnvPoints, NULL, BINARY(1110)},

	{ { DEFACCEL, "SWS/BR: Fit selected envelope points to time selection" },                                                                                              "BR_FIT_ENV_POINTS_TO_TIMESEL",       FitEnvPointsToTimeSel, NULL},
	{ { DEFACCEL, "SWS/BR: Insert new envelope point at mouse cursor using value at current position (obey snapping)" },                                                   "BR_ENV_POINT_MOUSE_CURSOR",          CreateEnvPointMouse, NULL},

	{ { DEFACCEL, "SWS/BR: Increase selected envelope points by 0.1 db (volume envelope only)" },                                                                          "BR_INC_VOL_ENV_PT_01.db",            IncreaseDecreaseVolEnvPoints, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Increase selected envelope points by 0.5 db (volume envelope only)" },                                                                          "BR_INC_VOL_ENV_PT_05.db",            IncreaseDecreaseVolEnvPoints, NULL, 5},
	{ { DEFACCEL, "SWS/BR: Increase selected envelope points by 1 db (volume envelope only)" },                                                                            "BR_INC_VOL_ENV_PT_1db",              IncreaseDecreaseVolEnvPoints, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Increase selected envelope points by 5 db (volume envelope only)" },                                                                            "BR_INC_VOL_ENV_PT_5db",              IncreaseDecreaseVolEnvPoints, NULL, 50},
	{ { DEFACCEL, "SWS/BR: Increase selected envelope points by 10 db (volume envelope only)" },                                                                           "BR_INC_VOL_ENV_PT_10db",             IncreaseDecreaseVolEnvPoints, NULL, 100},
	{ { DEFACCEL, "SWS/BR: Decrease selected envelope points by 0.1 db (volume envelope only)" },                                                                          "BR_DEC_VOL_ENV_PT_01.db",            IncreaseDecreaseVolEnvPoints, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Decrease selected envelope points by 0.5 db (volume envelope only)" },                                                                          "BR_DEC_VOL_ENV_PT_05.db",            IncreaseDecreaseVolEnvPoints, NULL, -5},
	{ { DEFACCEL, "SWS/BR: Decrease selected envelope points by 1 db (volume envelope only)" },                                                                            "BR_DEC_VOL_ENV_PT_1db",              IncreaseDecreaseVolEnvPoints, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Decrease selected envelope points by 5 db (volume envelope only)" },                                                                            "BR_DEC_VOL_ENV_PT_5db",              IncreaseDecreaseVolEnvPoints, NULL, -50},
	{ { DEFACCEL, "SWS/BR: Decrease selected envelope points by 10 db (volume envelope only)" },                                                                           "BR_DEC_VOL_ENV_PT_10db",             IncreaseDecreaseVolEnvPoints, NULL, -100},

	{ { DEFACCEL, "SWS/BR: Select envelope at mouse cursor" },                                                                                                             "BR_SEL_ENV_MOUSE",                   SelectEnvelopeUnderMouse, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Select envelope point at mouse cursor (selected envelope only)" },                                                                              "BR_SEL_ENV_PT_MOUSE_ACT_ENV_ONLY",   SelectDeleteEnvPointUnderMouse, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Select envelope point at mouse cursor" },                                                                                                       "BR_SEL_ENV_PT_MOUSE",                SelectDeleteEnvPointUnderMouse, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Delete envelope point at mouse cursor (selected envelope only)" },                                                                              "BR_DEL_ENV_PT_MOUSE_ACT_ENV_ONLY",   SelectDeleteEnvPointUnderMouse, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Delete envelope point at mouse cursor" },                                                                                                       "BR_DEL_ENV_PT_MOUSE",                SelectDeleteEnvPointUnderMouse, NULL, -2},

	{ { DEFACCEL, "SWS/BR: Unselect envelope" },                                                                                                                           "BR_UNSEL_ENV",                       UnselectEnvelope, NULL, 0},

	{ { DEFACCEL, "SWS/BR: Apply next action to all visible envelopes in selected tracks" },                                                                               "BR_NEXT_CMD_SEL_TK_VIS_ENVS",        ApplyNextCmdToMultiEnvelopes, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Apply next action to all visible record-armed envelopes in selected tracks" },                                                                  "BR_NEXT_CMD_SEL_TK_REC_ENVS",        ApplyNextCmdToMultiEnvelopes, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Apply next action to all visible envelopes in selected tracks if there is no track envelope selected" },                                        "BR_NEXT_CMD_SEL_TK_VIS_ENVS_NOSEL",  ApplyNextCmdToMultiEnvelopes, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Apply next action to all visible record-armed envelopes in selected tracks if there is no track envelope selected" },                           "BR_NEXT_CMD_SEL_TK_REC_ENVS_NOSEL",  ApplyNextCmdToMultiEnvelopes, NULL, -2},

	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 01" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_1",             SaveEnvSelSlot, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 02" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_2",             SaveEnvSelSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 03" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_3",             SaveEnvSelSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 04" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_4",             SaveEnvSelSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 05" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_5",             SaveEnvSelSlot, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 06" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_6",             SaveEnvSelSlot, NULL, 5},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 07" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_7",             SaveEnvSelSlot, NULL, 6},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 08" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_8",             SaveEnvSelSlot, NULL, 7},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 09" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_9",             SaveEnvSelSlot, NULL, 8},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 10" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_10",            SaveEnvSelSlot, NULL, 9},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 11" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_11",            SaveEnvSelSlot, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 12" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_12",            SaveEnvSelSlot, NULL, 11},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 13" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_13",            SaveEnvSelSlot, NULL, 12},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 14" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_14",            SaveEnvSelSlot, NULL, 13},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 15" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_15",            SaveEnvSelSlot, NULL, 14},
	{ { DEFACCEL, "SWS/BR: Save envelope point selection, slot 16" },                                                                                                      "BR_SAVE_ENV_SEL_SLOT_16",            SaveEnvSelSlot, NULL, 15},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 01" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_1",          RestoreEnvSelSlot, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 02" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_2",          RestoreEnvSelSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 03" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_3",          RestoreEnvSelSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 04" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_4",          RestoreEnvSelSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 05" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_5",          RestoreEnvSelSlot, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 06" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_6",          RestoreEnvSelSlot, NULL, 5},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 07" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_7",          RestoreEnvSelSlot, NULL, 6},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 08" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_8",          RestoreEnvSelSlot, NULL, 7},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 09" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_9",          RestoreEnvSelSlot, NULL, 8},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 10" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_10",         RestoreEnvSelSlot, NULL, 9},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 11" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_11",         RestoreEnvSelSlot, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 12" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_12",         RestoreEnvSelSlot, NULL, 11},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 13" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_13",         RestoreEnvSelSlot, NULL, 12},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 14" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_14",         RestoreEnvSelSlot, NULL, 13},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 15" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_15",         RestoreEnvSelSlot, NULL, 14},
	{ { DEFACCEL, "SWS/BR: Restore envelope point selection, slot 16" },                                                                                                   "BR_RESTORE_ENV_SEL_SLOT_16",         RestoreEnvSelSlot, NULL, 15},

	/******************************************************************************
	* Envelopes - Visibility                                                      *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Hide all but selected track envelope for all tracks" },                                           "BR_ENV_HIDE_ALL_BUT_ACTIVE",              ShowActiveTrackEnvOnly, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Hide all but selected track envelope for selected tracks" },                                      "BR_ENV_HIDE_ALL_BUT_ACTIVE_SEL",          ShowActiveTrackEnvOnly, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Hide all but selected track envelope for all tracks (except envelopes in separate lanes)" },      "BR_ENV_HIDE_ALL_BUT_ACTIVE_NO_ENV_L",     ShowActiveTrackEnvOnly, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Hide all but selected track envelope for selected tracks (except envelopes in separate lanes)" }, "BR_ENV_HIDE_ALL_BUT_ACTIVE_SEL_NO_ENV_L", ShowActiveTrackEnvOnly, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Hide all but selected track envelope for all tracks (except envelopes in track lanes)" },         "BR_ENV_HIDE_ALL_BUT_ACTIVE_NO_TR_L",      ShowActiveTrackEnvOnly, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Hide all but selected track envelope for selected tracks (except envelopes in track lanes)" },    "BR_ENV_HIDE_ALL_BUT_ACTIVE_SEL_NO_TR_L",  ShowActiveTrackEnvOnly, NULL, -3},

	{ { DEFACCEL, "SWS/BR: Show/hide track envelope for last adjusted send (volume/pan only)" },                             "BR_LAST_ADJ_SEND_ENVELOPE",               ShowLastAdjustedSendEnv, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Show/hide volume track envelope for last adjusted send" },                                        "BR_LAST_ADJ_SEND_ENVELOPE_VOL",           ShowLastAdjustedSendEnv, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Show/hide pan track envelope for last adjusted send" },                                           "BR_LAST_ADJ_SEND_ENVELOPE_PAN",           ShowLastAdjustedSendEnv, NULL, 2},

	{ { DEFACCEL, "SWS/BR: Toggle show all active FX envelopes for selected tracks" },                                       "BR_T_SHOW_ACT_FX_ENV_SEL_TRACK",          ShowHideFxEnv, NULL, BINARY(110)},
	{ { DEFACCEL, "SWS/BR: Toggle show all FX envelopes for selected tracks" },                                              "BR_T_SHOW_FX_ENV_SEL_TRACK",              ShowHideFxEnv, NULL, BINARY(100)},
	{ { DEFACCEL, "SWS/BR: Show all active FX envelopes for selected tracks" },                                              "BR_SHOW_ACT_FX_ENV_SEL_TRACK",            ShowHideFxEnv, NULL, BINARY(010)},
	{ { DEFACCEL, "SWS/BR: Show all FX envelopes for selected tracks" },                                                     "BR_SHOW_FX_ENV_SEL_TRACK",                ShowHideFxEnv, NULL, BINARY(000)},
	{ { DEFACCEL, "SWS/BR: Hide all FX envelopes for selected tracks" },                                                     "BR_HIDE_FX_ENV_SEL_TRACK",                ShowHideFxEnv, NULL, BINARY(001)},

	{ { DEFACCEL, "SWS/BR: Toggle show all active send envelopes for selected tracks" },                                     "BR_T_SHOW_ACT_SEND_ENV_ALL_SEL_TRACK",    ShowHideSendEnv, NULL, BINARY(111110)},
	{ { DEFACCEL, "SWS/BR: Toggle show active volume send envelopes for selected tracks" },                                  "BR_T_SHOW_ACT_SEND_ENV_VOL_SEL_TRACK",    ShowHideSendEnv, NULL, BINARY(001110)},
	{ { DEFACCEL, "SWS/BR: Toggle show active pan send envelopes for selected tracks" },                                     "BR_T_SHOW_ACT_SEND_ENV_PAN_SEL_TRACK",    ShowHideSendEnv, NULL, BINARY(010110)},
	{ { DEFACCEL, "SWS/BR: Toggle show active mute send envelopes for selected tracks" },                                    "BR_T_SHOW_ACT_SEND_ENV_MUTE_SEL_TRACK",   ShowHideSendEnv, NULL, BINARY(100110)},
	{ { DEFACCEL, "SWS/BR: Show all active send envelopes for selected tracks" },                                            "BR_SHOW_ACT_SEND_ENV_ALL_SEL_TRACK",      ShowHideSendEnv, NULL, BINARY(111010)},
	{ { DEFACCEL, "SWS/BR: Show active volume send envelopes for selected tracks" },                                         "BR_SHOW_ACT_SEND_ENV_VOL_SEL_TRACK",      ShowHideSendEnv, NULL, BINARY(001010)},
	{ { DEFACCEL, "SWS/BR: Show active pan send envelopes for selected tracks" },                                            "BR_SHOW_ACT_SEND_ENV_PAN_SEL_TRACK",      ShowHideSendEnv, NULL, BINARY(010010)},
	{ { DEFACCEL, "SWS/BR: Show active mute send envelopes for selected tracks" },                                           "BR_SHOW_ACT_SEND_ENV_MUTE_SEL_TRACK",     ShowHideSendEnv, NULL, BINARY(100010)},

	{ { DEFACCEL, "SWS/BR: Toggle show all send envelopes for selected tracks" },                                            "BR_T_SHOW_SEND_ENV_ALL_SEL_TRACK",        ShowHideSendEnv, NULL, BINARY(111100)},
	{ { DEFACCEL, "SWS/BR: Toggle show volume send envelopes for selected tracks" },                                         "BR_T_SHOW_SEND_ENV_VOL_SEL_TRACK",        ShowHideSendEnv, NULL, BINARY(001100)},
	{ { DEFACCEL, "SWS/BR: Toggle show pan send envelopes for selected tracks" },                                            "BR_T_SHOW_SEND_ENV_PAN_SEL_TRACK",        ShowHideSendEnv, NULL, BINARY(010100)},
	{ { DEFACCEL, "SWS/BR: Toggle show mute send envelopes for selected tracks" },                                           "BR_T_SHOW_SEND_ENV_MUTE_SEL_TRACK",       ShowHideSendEnv, NULL, BINARY(100100)},
	{ { DEFACCEL, "SWS/BR: Show all send envelopes for selected tracks" },                                                   "BR_SHOW_SEND_ENV_ALL_SEL_TRACK",          ShowHideSendEnv, NULL, BINARY(111000)},
	{ { DEFACCEL, "SWS/BR: Show volume send envelopes for selected tracks" },                                                "BR_SHOW_SEND_ENV_VOL_SEL_TRACK",          ShowHideSendEnv, NULL, BINARY(001000)},
	{ { DEFACCEL, "SWS/BR: Show pan send envelopes for selected tracks" },                                                   "BR_SHOW_SEND_ENV_PAN_SEL_TRACK",          ShowHideSendEnv, NULL, BINARY(010000)},
	{ { DEFACCEL, "SWS/BR: Show mute send envelopes for selected tracks" },                                                  "BR_SHOW_SEND_ENV_MUTE_SEL_TRACK",         ShowHideSendEnv, NULL, BINARY(100000)},

	{ { DEFACCEL, "SWS/BR: Hide all send envelopes for selected tracks" },                                                   "BR_HIDE_SEND_ENV_SEL_ALL_TRACK",        ShowHideSendEnv, NULL, BINARY(111001)},
	{ { DEFACCEL, "SWS/BR: Hide volume send envelopes for selected tracks" },                                                "BR_HIDE_SEND_ENV_SEL_VOL_TRACK",        ShowHideSendEnv, NULL, BINARY(001001)},
	{ { DEFACCEL, "SWS/BR: Hide pan send envelopes for selected tracks" },                                                   "BR_HIDE_SEND_ENV_SEL_PAN_TRACK",        ShowHideSendEnv, NULL, BINARY(010001)},
	{ { DEFACCEL, "SWS/BR: Hide mute send envelopes for selected tracks" },                                                  "BR_HIDE_SEND_ENV_SEL_NUTE_TRACK",       ShowHideSendEnv, NULL, BINARY(100001)},

	/******************************************************************************
	* Loudness                                                                    *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Global loudness preferences..." },                           "BR_LOUDNESS_PREF",                ToggleLoudnessPref,         NULL, 0, IsLoudnessPrefVisible},
	{ { DEFACCEL, "SWS/BR: Analyze loudness..." },                                      "BR_ANALAYZE_LOUDNESS_DLG",        AnalyzeLoudness,            NULL, 0, IsAnalyzeLoudnessVisible},

	{ { DEFACCEL, "SWS/BR: Normalize loudness of selected items/tracks..." },           "BR_NORMALIZE_LOUDNESS_ITEMS",     NormalizeLoudness,          NULL, 0, IsNormalizeLoudnessVisible},
	{ { DEFACCEL, "SWS/BR: Normalize loudness of selected items to -23 LUFS" },         "BR_NORMALIZE_LOUDNESS_ITEMS23",   NormalizeLoudness,          NULL, 1, },
	{ { DEFACCEL, "SWS/BR: Normalize loudness of selected items to 0 LU" },             "BR_NORMALIZE_LOUDNESS_ITEMS_LU",  NormalizeLoudness,          NULL, -1, },
	{ { DEFACCEL, "SWS/BR: Normalize loudness of selected tracks to -23 LUFS" },        "BR_NORMALIZE_LOUDNESS_TRACKS23",  NormalizeLoudness,          NULL, 2, },
	{ { DEFACCEL, "SWS/BR: Normalize loudness of selected tracks to 0 LU" },            "BR_NORMALIZE_LOUDNESS_TRACKS_LU", NormalizeLoudness,          NULL, -2, },

	{ { DEFACCEL, "SWS/BR/NF: Toggle use high precision mode for loudness analyzing" }, "BR_NF_TOGGLE_LOUDNESS_HIGH_PREC", ToggleHighPrecisionOption,  NULL, 0, IsHighPrecisionOptionEnabled},
	{ { DEFACCEL, "SWS/BR/NF: Toggle use dual mono mode (for mono takes/channel modes) for loudness analyzing" }, "BR_NF_TOGGLE_LOUDNESS_DUAL_MONO", ToggleDualMonoOption,  NULL, 0, IsDualMonoOptionEnabled },

	/******************************************************************************
	* MIDI editor - Item preview                                                  *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Stop media item preview" },                                                                                                   "BR_ME_STOP_PREV_ACT_ITEM",             NULL, NULL, 0,    NULL, SECTION_MIDI_EDITOR, ME_StopMidiTakePreview},

	{ { DEFACCEL, "SWS/BR: Preview active media item through track" },                                                                                   "BR_ME_PREV_ACT_ITEM",                  NULL, NULL, 1111, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item through track (start from mouse position)" },                                                       "BR_ME_PREV_ACT_ITEM_POS",              NULL, NULL, 1211, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item through track (sync with next measure)" },                                                          "BR_ME_PREV_ACT_ITEM_SYNC",             NULL, NULL, 1311, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item through track and pause during preview" },                                                          "BR_ME_PREV_ACT_ITEM_PAUSE",            NULL, NULL, 1112, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item through track and pause during preview (start from mouse position)" },                              "BR_ME_PREV_ACT_ITEM_PAUSE_POS",        NULL, NULL, 1212, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item (selected notes only) through track" },                                                             "BR_ME_PREV_ACT_ITEM_NOTES",            NULL, NULL, 1121, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item (selected notes only) through track (start from mouse position)" },                                 "BR_ME_PREV_ACT_ITEM_NOTES_POS",        NULL, NULL, 1221, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item (selected notes only) through track (sync with next measure)" },                                    "BR_ME_PREV_ACT_ITEM_NOTES_SYNC",       NULL, NULL, 1321, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item (selected notes only) through track and pause during preview" },                                    "BR_ME_PREV_ACT_ITEM_NOTES_PAUSE",      NULL, NULL, 1122, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Preview active media item (selected notes only) through track and pause during preview (start from mouse position)" },        "BR_ME_PREV_ACT_ITEM_NOTES_PAUSE_POS",  NULL, NULL, 1222, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},

	{ { DEFACCEL, "SWS/BR: Toggle preview active media item through track" },                                                                            "BR_ME_TPREV_ACT_ITEM",                 NULL, NULL, 2111, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item through track (start from mouse position)" },                                                "BR_ME_TPREV_ACT_ITEM_POS",             NULL, NULL, 2211, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item through track (sync with next measure)" },                                                   "BR_ME_TPREV_ACT_ITEM_SYNC",            NULL, NULL, 2311, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item through track and pause during preview" },                                                   "BR_ME_TPREV_ACT_ITEM_PAUSE",           NULL, NULL, 2112, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item through track and pause during preview (start from mouse position)" },                       "BR_ME_TPREV_ACT_ITEM_PAUSE_POS",       NULL, NULL, 2212, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item (selected notes only) through track" },                                                      "BR_ME_TPREV_ACT_ITEM_NOTES",           NULL, NULL, 2121, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item (selected notes only) through track (start from mouse position)" },                          "BR_ME_TPREV_ACT_ITEM_NOTES_POS",       NULL, NULL, 2221, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item (selected notes only) through track (sync with next measure)" },                             "BR_ME_TPREV_ACT_ITEM_NOTES_SYNC",      NULL, NULL, 2321, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item (selected notes only) through track and pause during preview" },                             "BR_ME_TPREV_ACT_ITEM_NOTES_PAUSE",     NULL, NULL, 2122, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},
	{ { DEFACCEL, "SWS/BR: Toggle preview active media item (selected notes only) through track and pause during preview (start from mouse position)" }, "BR_ME_TPREV_ACT_ITEM_NOTES_PAUSE_POS", NULL, NULL, 2222, NULL, SECTION_MIDI_EDITOR, ME_PreviewActiveTake},

	/******************************************************************************
	* MIDI editor - Misc                                                          *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Toggle play from mouse cursor position" },                                                                                    "BR_ME_TOGGLE_PLAY_MOUSE",                      NULL, NULL, 1, NULL, SECTION_MIDI_EDITOR, ME_ToggleMousePlayback},
	{ { DEFACCEL, "SWS/BR: Toggle play from mouse cursor position and solo active item's track for the duration" },                                      "BR_ME_TOGGLE_PLAY_MOUSE_SOLO_TRACK",           NULL, NULL, 2, NULL, SECTION_MIDI_EDITOR, ME_ToggleMousePlayback},
	{ { DEFACCEL, "SWS/BR: Toggle play from mouse cursor position and solo active item for the duration" },                                              "BR_ME_TOGGLE_PLAY_MOUSE_SOLO_ITEM",            NULL, NULL, 3, NULL, SECTION_MIDI_EDITOR, ME_ToggleMousePlayback},
	{ { DEFACCEL, "SWS/BR: Toggle play from edit cursor position and solo active item's track for the duration" },                                       "BR_ME_TOGGLE_PLAY_EDIT_SOLO_TRACK",            NULL, NULL, -2, NULL, SECTION_MIDI_EDITOR, ME_ToggleMousePlayback},
	{ { DEFACCEL, "SWS/BR: Toggle play from edit cursor position and solo active item for the duration" },                                               "BR_ME_TOGGLE_PLAY_EDIT_SOLO_ITEM",             NULL, NULL, -3, NULL, SECTION_MIDI_EDITOR, ME_ToggleMousePlayback},

	{ { DEFACCEL, "SWS/BR: Play from mouse cursor position" },                                                                                           "BR_ME_PLAY_MOUSECURSOR",                       NULL, NULL, 0, NULL, SECTION_MIDI_EDITOR, ME_PlaybackAtMouseCursor},
	{ { DEFACCEL, "SWS/BR: Play/pause from mouse cursor position" },                                                                                     "BR_ME_PLAY_PAUSE_MOUSECURSOR",                 NULL, NULL, 1, NULL, SECTION_MIDI_EDITOR, ME_PlaybackAtMouseCursor},
	{ { DEFACCEL, "SWS/BR: Play/stop from mouse cursor position" },                                                                                      "BR_ME_PLAY_STOP_MOUSECURSOR",                  NULL, NULL, 2, NULL, SECTION_MIDI_EDITOR, ME_PlaybackAtMouseCursor},

	{ { DEFACCEL, "SWS/BR: Insert CC event at edit cursor in lane under mouse cursor" },                                                                 "BR_ME_INSERT_CC_EDIT_CURSOR_MOUSE_LANE",       NULL, NULL, 0, NULL, SECTION_MIDI_EDITOR, ME_CCEventAtEditCursor},
	{ { DEFACCEL, "SWS/BR: Show only used CC lanes (detect 14-bit)" },                                                                                   "BR_ME_SHOW_USED_CC_14_BIT",                    NULL, NULL, 0, NULL, SECTION_MIDI_EDITOR, ME_ShowUsedCCLanesDetect14Bit},
	{ { DEFACCEL, "SWS/BR: Show only used CC lanes with selected events (detect 14-bit)" },                                                              "BR_ME_SHOW_USED_AND_SELECTED_CC_14_BIT",       NULL, NULL, 1, NULL, SECTION_MIDI_EDITOR, ME_ShowUsedCCLanesDetect14Bit},
	{ { DEFACCEL, "SWS/BR: Create CC lane and make it last clicked" },                                                                                   "BR_ME_CREATE_CC_LAST_CLICKED",                 NULL, NULL, 0, NULL, SECTION_MIDI_EDITOR, ME_CreateCCLaneLastClicked},

	{ { DEFACCEL, "SWS/BR: Move last clicked CC lane up" },                                                                                              "BR_ME_MOVE_CC_LAST_CLICKED_UP",                NULL, NULL, 1,  NULL, SECTION_MIDI_EDITOR, ME_MoveCCLaneUpDown},
	{ { DEFACCEL, "SWS/BR: Move last clicked CC lane up (keep lane height constant)" },                                                                  "BR_ME_MOVE_CC_LAST_CLICKED_UP_CONST",          NULL, NULL, 2,  NULL, SECTION_MIDI_EDITOR, ME_MoveCCLaneUpDown},
	{ { DEFACCEL, "SWS/BR: Move last clicked CC lane down" },                                                                                            "BR_ME_MOVE_CC_LAST_CLICKED_DOWN",              NULL, NULL, -1, NULL, SECTION_MIDI_EDITOR, ME_MoveCCLaneUpDown},
	{ { DEFACCEL, "SWS/BR: Move last clicked CC lane down (keep lane height constant)" },                                                                "BR_ME_MOVE_CC_LAST_CLICKED_DOWN_CONST",        NULL, NULL, -2, NULL, SECTION_MIDI_EDITOR, ME_MoveCCLaneUpDown},

	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: left, vertical: bottom)" },                                          "BR_ME_MOVE_WINDOW_TO_MOUSE_H_L_V_B",           NULL, NULL, BINARY(00101), NULL, SECTION_MIDI_EDITOR, ME_MoveActiveWndToMouse},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: left, vertical: middle)" },                                          "BR_ME_MOVE_WINDOW_TO_MOUSE_H_L_V_M",           NULL, NULL, BINARY(00100), NULL, SECTION_MIDI_EDITOR, ME_MoveActiveWndToMouse},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: left, vertical: top)" },                                             "BR_ME_MOVE_WINDOW_TO_MOUSE_H_L_V_T",           NULL, NULL, BINARY(00111), NULL, SECTION_MIDI_EDITOR, ME_MoveActiveWndToMouse},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: middle, vertical: bottom)" },                                        "BR_ME_MOVE_WINDOW_TO_MOUSE_H_M_V_B",           NULL, NULL, BINARY(00001), NULL, SECTION_MIDI_EDITOR, ME_MoveActiveWndToMouse},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: middle, vertical: middle)" },                                        "BR_ME_MOVE_WINDOW_TO_MOUSE_H_M_V_M",           NULL, NULL, BINARY(00000), NULL, SECTION_MIDI_EDITOR, ME_MoveActiveWndToMouse},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: middle, vertical: top)" },                                           "BR_ME_MOVE_WINDOW_TO_MOUSE_H_M_V_T",           NULL, NULL, BINARY(00011), NULL, SECTION_MIDI_EDITOR, ME_MoveActiveWndToMouse},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: right, vertical: bottom)" },                                         "BR_ME_MOVE_WINDOW_TO_MOUSE_H_R_V_B",           NULL, NULL, BINARY(01101), NULL, SECTION_MIDI_EDITOR, ME_MoveActiveWndToMouse},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: right, vertical: middle)" },                                         "BR_ME_MOVE_WINDOW_TO_MOUSE_H_R_V_M",           NULL, NULL, BINARY(01100), NULL, SECTION_MIDI_EDITOR, ME_MoveActiveWndToMouse},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: right, vertical: top)" },                                            "BR_ME_MOVE_WINDOW_TO_MOUSE_H_R_V_T",           NULL, NULL, BINARY(01111), NULL, SECTION_MIDI_EDITOR, ME_MoveActiveWndToMouse},

	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 0 pixel" },                                                                                       "BR_ME_SET_CC_LANES_HEIGHT_0",                  NULL, NULL, 0,   NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 010 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_10",                 NULL, NULL, 10,  NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 020 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_20",                 NULL, NULL, 20,  NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 030 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_30",                 NULL, NULL, 30,  NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 040 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_40",                 NULL, NULL, 40,  NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 050 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_50",                 NULL, NULL, 50,  NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 060 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_60",                 NULL, NULL, 60,  NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 070 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_70",                 NULL, NULL, 70,  NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 080 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_80",                 NULL, NULL, 80,  NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 090 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_90",                 NULL, NULL, 90,  NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 100 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_100",                NULL, NULL, 100, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 110 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_110",                NULL, NULL, 110, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 120 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_120",                NULL, NULL, 120, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 130 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_130",                NULL, NULL, 130, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 140 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_140",                NULL, NULL, 140, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 150 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_150",                NULL, NULL, 150, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 160 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_160",                NULL, NULL, 160, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 170 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_170",                NULL, NULL, 170, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 180 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_180",                NULL, NULL, 180, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 190 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_190",                NULL, NULL, 190, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Set all CC lanes' height to 200 pixel" },                                                                                     "BR_ME_SET_CC_LANES_HEIGHT_200",                NULL, NULL, 200, NULL, SECTION_MIDI_EDITOR, ME_SetAllCCLanesHeight},

	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 001 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_1",                  NULL, NULL, 1,   NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 002 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_2",                  NULL, NULL, 2,   NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 003 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_3",                  NULL, NULL, 3,   NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 004 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_4",                  NULL, NULL, 4,   NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 005 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_5",                  NULL, NULL, 5,   NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 010 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_10",                 NULL, NULL, 10,  NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 020 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_20",                 NULL, NULL, 20,  NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 030 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_30",                 NULL, NULL, 30,  NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 040 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_40",                 NULL, NULL, 40,  NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 050 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_50",                 NULL, NULL, 50,  NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Increase all CC lanes' height by 100 pixel" },                                                                                "BR_ME_INC_CC_LANES_HEIGHT_100",                NULL, NULL, 100, NULL,  SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 001 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_1",                  NULL, NULL, -1,   NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 002 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_2",                  NULL, NULL, -2,   NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 003 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_3",                  NULL, NULL, -3,   NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 004 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_4",                  NULL, NULL, -4,   NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 005 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_5",                  NULL, NULL, -5,   NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 010 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_10",                 NULL, NULL, -10,  NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 020 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_20",                 NULL, NULL, -20,  NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 030 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_30",                 NULL, NULL, -30,  NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 040 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_40",                 NULL, NULL, -40,  NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 050 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_50",                 NULL, NULL, -50,  NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},
	{ { DEFACCEL, "SWS/BR: Decrease all CC lanes' height by 100 pixel" },                                                                                "BR_ME_DEC_CC_LANES_HEIGHT_100",                NULL, NULL, -100, NULL, SECTION_MIDI_EDITOR, ME_IncDecAllCCLanesHeight},

	{ { DEFACCEL, "SWS/BR: Hide last clicked CC lane" },                                                                                                 "BR_ME_HIDE_LAST_CLICKED_LANE",                 NULL, NULL, 1,   NULL,                     SECTION_MIDI_EDITOR, ME_HideCCLanes},
	{ { DEFACCEL, "SWS/BR: Hide all CC lanes except last clicked lane" },                                                                                "BR_ME_HIDE_ALL_NO_LAST_CLICKED",               NULL, NULL, 2,   NULL,                     SECTION_MIDI_EDITOR, ME_HideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane" },                                                                         "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED",        NULL, NULL, 1,   ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane (set lane height to 50 pixel)" },                                           "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED_50_PX",  NULL, NULL, 50,  ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane (set lane height to 100 pixel)" },                                          "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED_100_PX", NULL, NULL, 100, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane (set lane height to 150 pixel)" },                                          "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED_150_PX", NULL, NULL, 150, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane (set lane height to 200 pixel)" },                                          "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED_200_PX", NULL, NULL, 200, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane (set lane height to 250 pixel)" },                                          "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED_250_PX", NULL, NULL, 250, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane (set lane height to 300 pixel)" },                                          "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED_300_PX", NULL, NULL, 300, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane (set lane height to 350 pixel)" },                                          "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED_350_PX", NULL, NULL, 350, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane (set lane height to 400 pixel)" },                                          "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED_400_PX", NULL, NULL, 400, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane (set lane height to 450 pixel)" },                                          "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED_450_PX", NULL, NULL, 450, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except last clicked lane (set lane height to 500 pixel)" },                                          "BR_ME_TOGGLE_HIDE_ALL_NO_LAST_CLICKED_500_PX", NULL, NULL, 500, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},

	{ { DEFACCEL, "SWS/BR: Hide CC lane under mouse cursor" },                                                                                           "BR_ME_HIDE_MOUSE_LANE",                        NULL, NULL, -1,   NULL,                     SECTION_MIDI_EDITOR, ME_HideCCLanes},
	{ { DEFACCEL, "SWS/BR: Hide all CC lanes except lane under mouse cursor" },                                                                          "BR_ME_HIDE_ALL_NO_MOUSE_LANE",                 NULL, NULL, -2,   NULL,                     SECTION_MIDI_EDITOR, ME_HideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor" },                                                                   "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE",          NULL, NULL, -1,   ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor (set lane height to 50 pixel)" },                                     "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE_50_PX",    NULL, NULL, -50,  ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor (set lane height to 100 pixel)" },                                    "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE_100_PX",   NULL, NULL, -100, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor (set lane height to 150 pixel)" },                                    "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE_150_PX",   NULL, NULL, -150, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor (set lane height to 200 pixel)" },                                    "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE_200_PX",   NULL, NULL, -200, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor (set lane height to 250 pixel)" },                                    "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE_250_PX",   NULL, NULL, -250, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor (set lane height to 300 pixel)" },                                    "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE_300_PX",   NULL, NULL, -300, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor (set lane height to 350 pixel)" },                                    "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE_350_PX",   NULL, NULL, -350, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor (set lane height to 400 pixel)" },                                    "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE_400_PX",   NULL, NULL, -400, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor (set lane height to 450 pixel)" },                                    "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE_450_PX",   NULL, NULL, -450, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},
	{ { DEFACCEL, "SWS/BR: Toggle hide all CC lanes except lane under mouse cursor (set lane height to 500 pixel)" },                                    "BR_ME_TOGGLE_HIDE_ALL_NO_MOUSE_LANE_500_PX",   NULL, NULL, -500, ME_IsToggleHideCCLanesOn, SECTION_MIDI_EDITOR, ME_ToggleHideCCLanes},

	{ { DEFACCEL, "SWS/BR: Convert selected CC events in active item to square envelope points in selected envelope" },                                  "BR_ME_CC_TO_ENV_SQUARE",                       NULL, NULL, 1,  NULL, SECTION_MIDI_EDITOR, ME_CCToEnvPoints},
	{ { DEFACCEL, "SWS/BR: Convert selected CC events in active item to linear envelope points in selected envelope" },                                  "BR_ME_CC_TO_ENV_LINEAR",                       NULL, NULL, 2,  NULL, SECTION_MIDI_EDITOR, ME_CCToEnvPoints},
	{ { DEFACCEL, "SWS/BR: Convert selected CC events in active item to square envelope points in selected envelope (clear existing envelope points)" }, "BR_ME_CC_TO_ENV_SQUARE_CLEAR",                 NULL, NULL, -1, NULL, SECTION_MIDI_EDITOR, ME_CCToEnvPoints},
	{ { DEFACCEL, "SWS/BR: Convert selected CC events in active item to linear envelope points in selected envelope (clear existing envelope points)" }, "BR_ME_CC_TO_ENV_LINEAR_CLEAR",                 NULL, NULL, -2, NULL, SECTION_MIDI_EDITOR, ME_CCToEnvPoints},

	{ { DEFACCEL, "SWS/BR: Convert selected points in selected envelope to CC events in last clicked CC lane" },                                         "BR_ME_ENV_TO_CC",                              NULL, NULL, 1,  NULL, SECTION_MIDI_EDITOR, ME_EnvPointsToCC},
	{ { DEFACCEL, "SWS/BR: Convert selected points in selected envelope to CC events in last clicked CC lane (clear existing events)" },                 "BR_ME_ENV_TO_CC_CLEAR",                        NULL, NULL, -1, NULL, SECTION_MIDI_EDITOR, ME_EnvPointsToCC},
	{ { DEFACCEL, "SWS/BR: Convert selected points in selected envelope to CC events in CC lane at mouse cursor" },                                      "BR_ME_ENV_TO_CC_MOUSE",                        NULL, NULL, 2,  NULL, SECTION_MIDI_EDITOR, ME_EnvPointsToCC},
	{ { DEFACCEL, "SWS/BR: Convert selected points in selected envelope to CC events in CC lane at mouse cursor (clear existing events)" },              "BR_ME_ENV_TO_CC_MOUSE_CLEAR",                  NULL, NULL, -2, NULL, SECTION_MIDI_EDITOR, ME_EnvPointsToCC},
	{ { DEFACCEL, "SWS/BR: Convert selected envelope's curve in time selection to CC events in last clicked CC lane" },                                  "BR_ME_ENV_CURVE_TO_CC",                        NULL, NULL, 3,  NULL, SECTION_MIDI_EDITOR, ME_EnvPointsToCC},
	{ { DEFACCEL, "SWS/BR: Convert selected envelope's curve in time selection to CC events in last clicked CC lane (clear existing events)" },          "BR_ME_ENV_CURVE_TO_CC_CLEAR",                  NULL, NULL, -3, NULL, SECTION_MIDI_EDITOR, ME_EnvPointsToCC},
	{ { DEFACCEL, "SWS/BR: Convert selected envelope's curve in time selection to CC events in CC lane at mouse cursor" },                               "BR_ME_ENV_CURVE_TO_CC_MOUSE",                  NULL, NULL, 4,  NULL, SECTION_MIDI_EDITOR, ME_EnvPointsToCC},
	{ { DEFACCEL, "SWS/BR: Convert selected envelope's curve in time selection to CC events in CC lane at mouse cursor (clear existing events)" },       "BR_ME_ENV_CURVE_TO_CC_MOUSE_CLEAR",            NULL, NULL, -4, NULL, SECTION_MIDI_EDITOR, ME_EnvPointsToCC},

	{ { DEFACCEL, "SWS/BR: Copy selected CC events in active item to last clicked lane" },                                                               "BR_ME_SEL_CC_TO_LAST_CLICKED_LANE",            NULL, NULL, 1,  NULL, SECTION_MIDI_EDITOR, ME_CopySelCCToLane},
	{ { DEFACCEL, "SWS/BR: Copy selected CC events in active item to lane under mouse cursor" },                                                         "BR_ME_SEL_CC_TO_LANE_UNDER_MOUSE",             NULL, NULL, -1, NULL, SECTION_MIDI_EDITOR, ME_CopySelCCToLane},

	{ { DEFACCEL, "SWS/BR: Delete all events in last clicked lane" },                                                                                    "BR_ME_DEL_EVENTS_LAST_LANE",                   NULL, NULL, 0, NULL, SECTION_MIDI_EDITOR, ME_DeleteEventsLastClickedLane},
	{ { DEFACCEL, "SWS/BR: Delete selected events in last clicked lane" },                                                                               "BR_ME_DEL_SE_EVENTS_LAST_LANE",                NULL, NULL, 1, NULL, SECTION_MIDI_EDITOR, ME_DeleteEventsLastClickedLane},

	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 01" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_1",                 NULL, NULL, 0,  NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 02" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_2",                 NULL, NULL, 1,  NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 03" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_3",                 NULL, NULL, 2,  NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 04" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_4",                 NULL, NULL, 3,  NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 05" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_5",                 NULL, NULL, 4,  NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 06" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_6",                 NULL, NULL, 5,  NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 07" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_7",                 NULL, NULL, 6,  NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 08" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_8",                 NULL, NULL, 7,  NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 09" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_9",                 NULL, NULL, 8,  NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 10" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_10",                NULL, NULL, 9,  NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 11" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_11",                NULL, NULL, 10, NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 12" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_12",                NULL, NULL, 11, NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 13" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_13",                NULL, NULL, 12, NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 14" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_14",                NULL, NULL, 13, NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 15" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_15",                NULL, NULL, 14, NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 16" },                                                                                        "BR_ME_SAVE_CURSOR_POS_SLOT_16",                NULL, NULL, 15, NULL, SECTION_MIDI_EDITOR, ME_SaveCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 01" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_1",              NULL, NULL, 0,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 02" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_2",              NULL, NULL, 1,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 03" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_3",              NULL, NULL, 2,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 04" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_4",              NULL, NULL, 3,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 05" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_5",              NULL, NULL, 4,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 06" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_6",              NULL, NULL, 5,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 07" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_7",              NULL, NULL, 6,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 08" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_8",              NULL, NULL, 7,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 09" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_9",              NULL, NULL, 8,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 10" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_10",             NULL, NULL, 9,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 11" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_11",             NULL, NULL, 10, NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 12" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_12",             NULL, NULL, 11, NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 13" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_13",             NULL, NULL, 12, NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 14" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_14",             NULL, NULL, 13, NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 15" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_15",             NULL, NULL, 14, NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 16" },                                                                                     "BR_ME_RESTORE_CURSOR_POS_SLOT_16",             NULL, NULL, 15, NULL, SECTION_MIDI_EDITOR, ME_RestoreCursorPosSlot},

	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 01" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_1",                   NULL, NULL, 0,  NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 02" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_2",                   NULL, NULL, 1,  NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 03" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_3",                   NULL, NULL, 2,  NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 04" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_4",                   NULL, NULL, 3,  NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 05" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_5",                   NULL, NULL, 4,  NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 06" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_6",                   NULL, NULL, 5,  NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 07" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_7",                   NULL, NULL, 6,  NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 08" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_8",                   NULL, NULL, 7,  NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 09" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_9",                   NULL, NULL, 8,  NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 10" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_10",                  NULL, NULL, 9,  NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 11" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_11",                  NULL, NULL, 10, NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 12" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_12",                  NULL, NULL, 11, NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 13" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_13",                  NULL, NULL, 12, NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 14" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_14",                  NULL, NULL, 13, NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 15" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_15",                  NULL, NULL, 14, NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Save note selection from active item, slot 16" },                                                                             "BR_ME_SAVE_NOTE_SEL_SLOT_16",                  NULL, NULL, 15, NULL, SECTION_MIDI_EDITOR, ME_SaveNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 01" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_1",                NULL, NULL, 0,  NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 02" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_2",                NULL, NULL, 1,  NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 03" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_3",                NULL, NULL, 2,  NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 04" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_4",                NULL, NULL, 3,  NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 05" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_5",                NULL, NULL, 4,  NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 06" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_6",                NULL, NULL, 5,  NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 07" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_7",                NULL, NULL, 6,  NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 08" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_8",                NULL, NULL, 7,  NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 09" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_9",                NULL, NULL, 8,  NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 10" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_10",               NULL, NULL, 9,  NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 11" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_11",               NULL, NULL, 10, NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 12" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_12",               NULL, NULL, 11, NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 13" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_13",               NULL, NULL, 12, NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 14" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_14",               NULL, NULL, 13, NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 15" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_15",               NULL, NULL, 14, NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},
	{ { DEFACCEL, "SWS/BR: Restore note selection to active item, slot 16" },                                                                            "BR_ME_RESTORE_NOTE_SEL_SLOT_16",               NULL, NULL, 15, NULL, SECTION_MIDI_EDITOR, ME_RestoreNoteSelSlot},

	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 01" },                                                                     "BR_ME_SAVE_CC_SLOT_1",                         NULL, NULL, 1,   NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 02" },                                                                     "BR_ME_SAVE_CC_SLOT_2",                         NULL, NULL, 2,   NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 03" },                                                                     "BR_ME_SAVE_CC_SLOT_3",                         NULL, NULL, 3,   NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 04" },                                                                     "BR_ME_SAVE_CC_SLOT_4",                         NULL, NULL, 4,   NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 05" },                                                                     "BR_ME_SAVE_CC_SLOT_5",                         NULL, NULL, 5,   NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 06" },                                                                     "BR_ME_SAVE_CC_SLOT_6",                         NULL, NULL, 6,   NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 07" },                                                                     "BR_ME_SAVE_CC_SLOT_7",                         NULL, NULL, 7,   NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 08" },                                                                     "BR_ME_SAVE_CC_SLOT_8",                         NULL, NULL, 8,   NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 09" },                                                                     "BR_ME_SAVE_CC_SLOT_9",                         NULL, NULL, 9,   NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 10" },                                                                     "BR_ME_SAVE_CC_SLOT_10",                        NULL, NULL, 10,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 11" },                                                                     "BR_ME_SAVE_CC_SLOT_11",                        NULL, NULL, 11,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 12" },                                                                     "BR_ME_SAVE_CC_SLOT_12",                        NULL, NULL, 12,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 13" },                                                                     "BR_ME_SAVE_CC_SLOT_13",                        NULL, NULL, 13,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 14" },                                                                     "BR_ME_SAVE_CC_SLOT_14",                        NULL, NULL, 14,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 15" },                                                                     "BR_ME_SAVE_CC_SLOT_15",                        NULL, NULL, 15,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in last clicked CC lane, slot 16" },                                                                     "BR_ME_SAVE_CC_SLOT_16",                        NULL, NULL, 16,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 01" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_1",                   NULL, NULL, -1,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 02" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_2",                   NULL, NULL, -2,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 03" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_3",                   NULL, NULL, -3,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 04" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_4",                   NULL, NULL, -4,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 05" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_5",                   NULL, NULL, -5,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 06" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_6",                   NULL, NULL, -6,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 07" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_7",                   NULL, NULL, -7,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 08" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_8",                   NULL, NULL, -8,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 09" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_9",                   NULL, NULL, -9,  NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 10" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_10",                  NULL, NULL, -10, NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 11" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_11",                  NULL, NULL, -11, NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 12" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_12",                  NULL, NULL, -12, NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 13" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_13",                  NULL, NULL, -13, NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 14" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_14",                  NULL, NULL, -14, NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 15" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_15",                  NULL, NULL, -15, NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Save selected events in CC lane under mouse cursor, slot 16" },                                                               "BR_ME_SAVE_MOUSE_CC_SLOT_16",                  NULL, NULL, -16, NULL, SECTION_MIDI_EDITOR, ME_SaveCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 01" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_1",              NULL, NULL, 1,   NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 02" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_2",              NULL, NULL, 2,   NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 03" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_3",              NULL, NULL, 3,   NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 04" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_4",              NULL, NULL, 4,   NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 05" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_5",              NULL, NULL, 5,   NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 06" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_6",              NULL, NULL, 6,   NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 07" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_7",              NULL, NULL, 7,   NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 08" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_8",              NULL, NULL, 8,   NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 09" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_9",              NULL, NULL, 9,   NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 10" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_10",             NULL, NULL, 10,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 11" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_11",             NULL, NULL, 11,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 12" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_12",             NULL, NULL, 12,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 13" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_13",             NULL, NULL, 13,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 14" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_14",             NULL, NULL, 14,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 15" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_15",             NULL, NULL, 15,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to last clicked CC lane, slot 16" },                                                                           "BR_ME_RESTORE_CC_LAST_CL_SLOT_16",             NULL, NULL, 16,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 01" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_1",                NULL, NULL, -1,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 02" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_2",                NULL, NULL, -2,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 03" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_3",                NULL, NULL, -3,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 04" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_4",                NULL, NULL, -4,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 05" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_5",                NULL, NULL, -5,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 06" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_6",                NULL, NULL, -6,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 07" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_7",                NULL, NULL, -7,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 08" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_8",                NULL, NULL, -8,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 09" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_9",                NULL, NULL, -9,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 10" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_10",               NULL, NULL, -10, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 11" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_11",               NULL, NULL, -11, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 12" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_12",               NULL, NULL, -12, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 13" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_13",               NULL, NULL, -13, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 14" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_14",               NULL, NULL, -14, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 15" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_15",               NULL, NULL, -15, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to CC lane under mouse cursor, slot 16" },                                                                     "BR_ME_RESTORE_CC_MOUSE_SLOT_16",               NULL, NULL, -16, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEventsSlot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 01" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_1",              NULL, NULL,  0,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 02" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_2",              NULL, NULL,  1,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 03" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_3",              NULL, NULL,  2,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 04" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_4",              NULL, NULL,  3,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 05" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_5",              NULL, NULL,  4,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 06" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_6",              NULL, NULL,  5,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 07" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_7",              NULL, NULL,  6,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 08" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_8",              NULL, NULL,  7,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 09" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_9",              NULL, NULL,  7,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 10" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_10",             NULL, NULL,  9,  NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 11" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_11",             NULL, NULL,  10, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 12" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_12",             NULL, NULL,  11, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 13" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_13",             NULL, NULL,  12, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 14" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_14",             NULL, NULL,  13, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 15" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_15",             NULL, NULL,  14, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},
	{ { DEFACCEL, "SWS/BR: Restore events to all visible CC lanes, slot 16" },                                                                           "BR_ME_RESTORE_CC_ALL_VIS_SLOT_16",             NULL, NULL,  15, NULL, SECTION_MIDI_EDITOR, ME_RestoreCCEvents2Slot},

	/******************************************************************************
	* Misc                                                                        *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Toggle play from mouse cursor position" },                                                                                      "BR_TOGGLE_PLAY_MOUSE",                    ToggleMousePlayback, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Toggle play from mouse cursor position and solo track under mouse for the duration" },                                          "BR_TOGGLE_PLAY_MOUSE_SOLO_TRACK",         ToggleMousePlayback, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Toggle play from mouse cursor position and solo item and track under mouse for the duration" },                                 "BR_TOGGLE_PLAY_MOUSE_SOLO_ITEM",          ToggleMousePlayback, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Toggle play from edit cursor position and solo track under mouse for the duration" },                                           "BR_TOGGLE_PLAY_EDIT_SOLO_TRACK",          ToggleMousePlayback, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Toggle play from edit cursor position and solo item and track under mouse for the duration" },                                  "BR_TOGGLE_PLAY_EDIT_SOLO_ITEM",           ToggleMousePlayback, NULL, -3},

	{ { DEFACCEL, "SWS/BR: Play from mouse cursor position" },                                                                                             "BR_PLAY_MOUSECURSOR",                     PlaybackAtMouseCursor, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Play/pause from mouse cursor position" },                                                                                       "BR_PLAY_PAUSE_MOUSECURSOR",               PlaybackAtMouseCursor, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Play/stop from mouse cursor position" },                                                                                        "BR_PLAY_STOP_MOUSECURSOR",                PlaybackAtMouseCursor, NULL, 2},

	{ { DEFACCEL, "SWS/BR: Split selected items at tempo markers" },                                                                                       "SWS_BRSPLITSELECTEDTEMPO",                SplitItemAtTempo},
	{ { DEFACCEL, "SWS/BR: Split selected items at stretch markers" },                                                                                     "BR_SPLIT_SEL_ITEM_STRETCH_MARKERS",       SplitItemAtStretchMarkers},
	{ { DEFACCEL, "SWS/BR: Create project markers from selected tempo markers" },                                                                          "BR_TEMPO_TO_MARKERS",                     MarkersAtTempo},
	{ { DEFACCEL, "SWS/BR: Disable \"Ignore project tempo\" for selected MIDI items" },                                                                    "BR_MIDI_PROJ_TEMPO_DIS",                  MidiItemTempo, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Disable \"Ignore project tempo\" for selected MIDI items preserving time position of MIDI events" },                            "BR_MIDI_PROJ_TEMPO_DIS_TIME",             MidiItemTempo, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Enable \"Ignore project tempo\" for selected MIDI items (use tempo at item's start)" },                                         "BR_MIDI_PROJ_TEMPO_ENB",                  MidiItemTempo, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Enable \"Ignore project tempo\" for selected MIDI items preserving time position of MIDI events (use tempo at item's start)" }, "BR_MIDI_PROJ_TEMPO_ENB_TIME",             MidiItemTempo, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Trim MIDI item to active content" },                                                                                            "BR_TRIM_MIDI_ITEM_ACT_CONTENT",           MidiItemTrim, NULL},

	{ { DEFACCEL, "SWS/BR: Create project markers from notes in selected MIDI items" },                                                                    "BR_MIDI_NOTES_TO_MARKERS",                MarkersAtNotes},
	{ { DEFACCEL, "SWS/BR: Create project markers from stretch markers in selected items" },                                                               "BR_STRETCH_MARKERS_TO_MARKERS",           MarkersAtStretchMarkers},
	{ { DEFACCEL, "SWS/BR: Create project marker at mouse cursor" },                                                                                       "BR_MARKER_AT_MOUSE",                      MarkerAtMouse, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Create project marker at mouse cursor (obey snapping)" },                                                                       "BR_MARKER_AT_MOUSE_SNAP",                 MarkerAtMouse, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Create project markers from selected items (name by item's notes)" },                                                           "BR_ITEMS_TO_MARKERS_NOTES",               MarkersRegionsAtItems, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Create regions from selected items (name by item's notes)" },                                                                   "BR_ITEMS_TO_REGIONS_NOTES",               MarkersRegionsAtItems, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Move closest project marker to play cursor" },                                                                                  "BR_CLOSEST_PROJ_MARKER_PLAY",             MoveClosestMarker, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Move closest project marker to edit cursor" },                                                                                  "BR_CLOSEST_PROJ_MARKER_EDIT",             MoveClosestMarker, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Move closest project marker to mouse cursor" },                                                                                 "BR_CLOSEST_PROJ_MARKER_MOUSE",            MoveClosestMarker, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Move closest project marker to play cursor (obey snapping)" },                                                                  "BR_CLOSEST_PROJ_MARKER_PLAY_SNAP",        MoveClosestMarker, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Move closest project marker to edit cursor (obey snapping)" },                                                                  "BR_CLOSEST_PROJ_MARKER_EDIT_SNAP",        MoveClosestMarker, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Move closest project marker to mouse cursor (obey snapping)" },                                                                 "BR_CLOSEST_PROJ_MARKER_MOUSE_SNAP",       MoveClosestMarker, NULL, -3},

	{ { DEFACCEL, "SWS/BR: Focus arrange" },                                                                                                               "BR_FOCUS_ARRANGE_WND",                    FocusArrangeTracks, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Focus tracks" },                                                                                                                "BR_FOCUS_TRACKS",                         FocusArrangeTracks, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: left, vertical: bottom)" },                                            "BR_MOVE_WINDOW_TO_MOUSE_H_L_V_B",         MoveActiveWndToMouse, NULL, BINARY(00101)},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: left, vertical: middle)" },                                            "BR_MOVE_WINDOW_TO_MOUSE_H_L_V_M",         MoveActiveWndToMouse, NULL, BINARY(00100)},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: left, vertical: top)" },                                               "BR_MOVE_WINDOW_TO_MOUSE_H_L_V_T",         MoveActiveWndToMouse, NULL, BINARY(00111)},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: middle, vertical: bottom)" },                                          "BR_MOVE_WINDOW_TO_MOUSE_H_M_V_B",         MoveActiveWndToMouse, NULL, BINARY(00001)},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: middle, vertical: middle)" },                                          "BR_MOVE_WINDOW_TO_MOUSE_H_M_V_M",         MoveActiveWndToMouse, NULL, BINARY(00000)},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: middle, vertical: top)" },                                             "BR_MOVE_WINDOW_TO_MOUSE_H_M_V_T",         MoveActiveWndToMouse, NULL, BINARY(00011)},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: right, vertical: bottom)" },                                           "BR_MOVE_WINDOW_TO_MOUSE_H_R_V_B",         MoveActiveWndToMouse, NULL, BINARY(01101)},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: right, vertical: middle)" },                                           "BR_MOVE_WINDOW_TO_MOUSE_H_R_V_M",         MoveActiveWndToMouse, NULL, BINARY(01100)},
	{ { DEFACCEL, "SWS/BR: Move active floating window to mouse cursor (horizontal: right, vertical: top)" },                                              "BR_MOVE_WINDOW_TO_MOUSE_H_R_V_T",         MoveActiveWndToMouse, NULL, BINARY(01111)},
	{ { DEFACCEL, "SWS/BR: Move active floating track FX window to mouse cursor (horizontal: left, vertical: bottom)" },                                   "BR_MOVE_FX_WINDOW_TO_MOUSE_H_L_V_B",      MoveActiveWndToMouse, NULL, BINARY(10101)},
	{ { DEFACCEL, "SWS/BR: Move active floating track FX window to mouse cursor (horizontal: left, vertical: middle)" },                                   "BR_MOVE_FX_WINDOW_TO_MOUSE_H_L_V_M",      MoveActiveWndToMouse, NULL, BINARY(10100)},
	{ { DEFACCEL, "SWS/BR: Move active floating track FX window to mouse cursor (horizontal: left, vertical: top)" },                                      "BR_MOVE_FX_WINDOW_TO_MOUSE_H_L_V_T",      MoveActiveWndToMouse, NULL, BINARY(10111)},
	{ { DEFACCEL, "SWS/BR: Move active floating track FX window to mouse cursor (horizontal: middle, vertical: bottom)" },                                 "BR_MOVE_FX_WINDOW_TO_MOUSE_H_M_V_B",      MoveActiveWndToMouse, NULL, BINARY(10001)},
	{ { DEFACCEL, "SWS/BR: Move active floating track FX window to mouse cursor (horizontal: middle, vertical: middle)" },                                 "BR_MOVE_FX_WINDOW_TO_MOUSE_H_M_V_M",      MoveActiveWndToMouse, NULL, BINARY(10000)},
	{ { DEFACCEL, "SWS/BR: Move active floating track FX window to mouse cursor (horizontal: middle, vertical: top)" },                                    "BR_MOVE_FX_WINDOW_TO_MOUSE_H_M_V_T",      MoveActiveWndToMouse, NULL, BINARY(10011)},
	{ { DEFACCEL, "SWS/BR: Move active floating track FX window to mouse cursor (horizontal: right, vertical: bottom)" },                                  "BR_MOVE_FX_WINDOW_TO_MOUSE_H_R_V_B",      MoveActiveWndToMouse, NULL, BINARY(11101)},
	{ { DEFACCEL, "SWS/BR: Move active floating track FX window to mouse cursor (horizontal: right, vertical: middle)" },                                  "BR_MOVE_FX_WINDOW_TO_MOUSE_H_R_V_M",      MoveActiveWndToMouse, NULL, BINARY(11100)},
	{ { DEFACCEL, "SWS/BR: Move active floating track FX window to mouse cursor (horizontal: right, vertical: top)" },                                     "BR_MOVE_FX_WINDOW_TO_MOUSE_H_R_V_T",      MoveActiveWndToMouse, NULL, BINARY(11111)},

	{ { DEFACCEL, "SWS/BR: Toggle media item online/offline" },                                                                                            "BR_TOGGLE_ITEM_ONLINE",                   ToggleItemOnline},
	{ { DEFACCEL, "SWS/BR: Copy take media source file path of selected items to clipboard" },                                                             "BR_TSOURCE_PATH_TO_CLIPBOARD",            ItemSourcePathToClipBoard},

	{ { DEFACCEL, "SWS/BR: Delete take under mouse cursor" },                                                                                              "BR_DELETE_TAKE_MOUSE",                    DeleteTakeUnderMouse, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Select TCP track under mouse cursor" },                                                                                         "BR_SEL_TCP_TRACK_MOUSE",                  SelectTrackUnderMouse, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Select MCP track under mouse cursor" },                                                                                         "BR_SEL_MCP_TRACK_MOUSE",                  SelectTrackUnderMouse, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Select all empty items" },                                                                                                      "BR_SEL_ALL_ITEMS_EMPTY",                  SelectItemsByType, NULL, static_cast<int>(SourceType::Unknown)},
	{ { DEFACCEL, "SWS/BR: Select all audio items" },                                                                                                      "BR_SEL_ALL_ITEMS_AUDIO",                  SelectItemsByType, NULL, static_cast<int>(SourceType::Audio)},
	{ { DEFACCEL, "SWS/BR: Select all MIDI items" },                                                                                                       "BR_SEL_ALL_ITEMS_MIDI",                   SelectItemsByType, NULL, static_cast<int>(SourceType::MIDI)},
	{ { DEFACCEL, "SWS/BR: Select all video items" },                                                                                                      "BR_SEL_ALL_ITEMS_VIDEO",                  SelectItemsByType, NULL, static_cast<int>(SourceType::Video)},
	{ { DEFACCEL, "SWS/BR: Select all click source items" },                                                                                               "BR_SEL_ALL_ITEMS_CLICK",                  SelectItemsByType, NULL, static_cast<int>(SourceType::Click)},
	{ { DEFACCEL, "SWS/BR: Select all timecode generator items" },                                                                                         "BR_SEL_ALL_ITEMS_TIMECODE",               SelectItemsByType, NULL, static_cast<int>(SourceType::Timecode)},
	{ { DEFACCEL, "SWS/BR: Select all subproject (PiP) items" },                                                                                           "BR_SEL_ALL_ITEMS_PIP",                    SelectItemsByType, NULL, static_cast<int>(SourceType::Project)},
	{ { DEFACCEL, "SWS/BR: Select all video processor items" },                                                                                            "BR_SEL_ALL_ITEMS_VIDEOFX",                SelectItemsByType, NULL, static_cast<int>(SourceType::VideoEffect)},

	{ { DEFACCEL, "SWS/BR: Select all empty items (obey time selection, if any)" },                                                                        "BR_SEL_ALL_ITEMS_TIME_SEL_EMPTY",         SelectItemsByType, NULL, ObeyTimeSelection | static_cast<int>(SourceType::Unknown)},
	{ { DEFACCEL, "SWS/BR: Select all audio items (obey time selection, if any)" },                                                                        "BR_SEL_ALL_ITEMS_TIME_SEL_AUDIO",         SelectItemsByType, NULL, ObeyTimeSelection | static_cast<int>(SourceType::Audio)},
	{ { DEFACCEL, "SWS/BR: Select all MIDI items (obey time selection, if any)" },                                                                         "BR_SEL_ALL_ITEMS_TIME_SEL_MIDI",          SelectItemsByType, NULL, ObeyTimeSelection | static_cast<int>(SourceType::MIDI)},
	{ { DEFACCEL, "SWS/BR: Select all video items (obey time selection, if any)" },                                                                        "BR_SEL_ALL_ITEMS_TIME_SEL_VIDEO",         SelectItemsByType, NULL, ObeyTimeSelection | static_cast<int>(SourceType::Video)},
	{ { DEFACCEL, "SWS/BR: Select all click source items (obey time selection, if any)" },                                                                 "BR_SEL_ALL_ITEMS_TIME_SEL_CLICK",         SelectItemsByType, NULL, ObeyTimeSelection | static_cast<int>(SourceType::Click)},
	{ { DEFACCEL, "SWS/BR: Select all timecode items (obey time selection, if any)" },                                                                     "BR_SEL_ALL_ITEMS_TIME_SEL_TIMECODE",      SelectItemsByType, NULL, ObeyTimeSelection | static_cast<int>(SourceType::Timecode)},
	{ { DEFACCEL, "SWS/BR: Select all subproject (PiP) items (obey time selection, if any)" },                                                             "BR_SEL_ALL_ITEMS_TIME_SEL_PIP",           SelectItemsByType, NULL, ObeyTimeSelection | static_cast<int>(SourceType::Project)},
	{ { DEFACCEL, "SWS/BR: Select all video processor items (obey time selection, if any)" },                                                              "BR_SEL_ALL_ITEMS_TIME_SEL_VIDEOFX",       SelectItemsByType, NULL, ObeyTimeSelection | static_cast<int>(SourceType::VideoEffect)},

	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 01" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_1",               SaveCursorPosSlot, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 02" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_2",               SaveCursorPosSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 03" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_3",               SaveCursorPosSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 04" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_4",               SaveCursorPosSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 05" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_5",               SaveCursorPosSlot, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 06" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_6",               SaveCursorPosSlot, NULL, 5},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 07" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_7",               SaveCursorPosSlot, NULL, 6},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 08" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_8",               SaveCursorPosSlot, NULL, 7},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 09" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_9",               SaveCursorPosSlot, NULL, 8},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 10" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_10",              SaveCursorPosSlot, NULL, 9},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 11" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_11",              SaveCursorPosSlot, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 12" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_12",              SaveCursorPosSlot, NULL, 11},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 13" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_13",              SaveCursorPosSlot, NULL, 12},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 14" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_14",              SaveCursorPosSlot, NULL, 13},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 15" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_15",              SaveCursorPosSlot, NULL, 14},
	{ { DEFACCEL, "SWS/BR: Save edit cursor position, slot 16" },                                                                                          "BR_SAVE_CURSOR_POS_SLOT_16",              SaveCursorPosSlot, NULL, 15},

	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 01" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_1",            RestoreCursorPosSlot, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 02" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_2",            RestoreCursorPosSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 03" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_3",            RestoreCursorPosSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 04" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_4",            RestoreCursorPosSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 05" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_5",            RestoreCursorPosSlot, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 06" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_6",            RestoreCursorPosSlot, NULL, 5},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 07" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_7",            RestoreCursorPosSlot, NULL, 6},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 08" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_8",            RestoreCursorPosSlot, NULL, 7},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 09" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_9",            RestoreCursorPosSlot, NULL, 8},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 10" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_10",           RestoreCursorPosSlot, NULL, 9},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 11" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_11",           RestoreCursorPosSlot, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 12" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_12",           RestoreCursorPosSlot, NULL, 11},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 13" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_13",           RestoreCursorPosSlot, NULL, 12},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 14" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_14",           RestoreCursorPosSlot, NULL, 13},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 15" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_15",           RestoreCursorPosSlot, NULL, 14},
	{ { DEFACCEL, "SWS/BR: Restore edit cursor position, slot 16" },                                                                                       "BR_RESTORE_CURSOR_POS_SLOT_16",           RestoreCursorPosSlot, NULL, 15},

	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 01" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_1",      SaveItemMuteStateSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 02" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_2",      SaveItemMuteStateSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 03" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_3",      SaveItemMuteStateSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 04" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_4",      SaveItemMuteStateSlot, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 05" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_5",      SaveItemMuteStateSlot, NULL, 5},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 06" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_6",      SaveItemMuteStateSlot, NULL, 6},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 07" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_7",      SaveItemMuteStateSlot, NULL, 7},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 08" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_8",      SaveItemMuteStateSlot, NULL, 8},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 09" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_9",      SaveItemMuteStateSlot, NULL, 9},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 10" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_10",     SaveItemMuteStateSlot, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 11" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_11",     SaveItemMuteStateSlot, NULL, 11},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 12" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_12",     SaveItemMuteStateSlot, NULL, 12},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 13" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_13",     SaveItemMuteStateSlot, NULL, 13},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 14" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_14",     SaveItemMuteStateSlot, NULL, 14},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 15" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_15",     SaveItemMuteStateSlot, NULL, 15},
	{ { DEFACCEL, "SWS/BR: Save selected items' mute state, slot 16" },                                                                                    "BR_SAVE_SOLO_MUTE_SEL_ITEMS_SLOT_16",     SaveItemMuteStateSlot, NULL, 16},

	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 01" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_1",      SaveItemMuteStateSlot, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 02" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_2",      SaveItemMuteStateSlot, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 03" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_3",      SaveItemMuteStateSlot, NULL, -3},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 04" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_4",      SaveItemMuteStateSlot, NULL, -4},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 05" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_5",      SaveItemMuteStateSlot, NULL, -5},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 06" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_6",      SaveItemMuteStateSlot, NULL, -6},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 07" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_7",      SaveItemMuteStateSlot, NULL, -7},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 08" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_8",      SaveItemMuteStateSlot, NULL, -8},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 09" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_9",      SaveItemMuteStateSlot, NULL, -9},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 10" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_10",     SaveItemMuteStateSlot, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 11" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_11",     SaveItemMuteStateSlot, NULL, -11},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 12" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_12",     SaveItemMuteStateSlot, NULL, -12},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 13" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_13",     SaveItemMuteStateSlot, NULL, -13},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 14" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_14",     SaveItemMuteStateSlot, NULL, -14},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 15" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_15",     SaveItemMuteStateSlot, NULL, -15},
	{ { DEFACCEL, "SWS/BR: Save all items' mute state, slot 16" },                                                                                         "BR_SAVE_SOLO_MUTE_ALL_ITEMS_SLOT_16",     SaveItemMuteStateSlot, NULL, -16},

	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 01" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_1",   RestoreItemMuteStateSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 02" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_2",   RestoreItemMuteStateSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 03" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_3",   RestoreItemMuteStateSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 04" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_4",   RestoreItemMuteStateSlot, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 05" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_5",   RestoreItemMuteStateSlot, NULL, 5},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 06" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_6",   RestoreItemMuteStateSlot, NULL, 6},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 07" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_7",   RestoreItemMuteStateSlot, NULL, 7},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 08" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_8",   RestoreItemMuteStateSlot, NULL, 8},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 09" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_9",   RestoreItemMuteStateSlot, NULL, 9},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 10" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_10",  RestoreItemMuteStateSlot, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 11" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_11",  RestoreItemMuteStateSlot, NULL, 11},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 12" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_12",  RestoreItemMuteStateSlot, NULL, 12},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 13" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_13",  RestoreItemMuteStateSlot, NULL, 13},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 14" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_14",  RestoreItemMuteStateSlot, NULL, 14},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 15" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_15",  RestoreItemMuteStateSlot, NULL, 15},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to selected items, slot 16" },                                                                        "BR_RESTORE_SOLO_MUTE_SEL_ITEMS_SLOT_16",  RestoreItemMuteStateSlot, NULL, 16},

	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 01" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_1",   RestoreItemMuteStateSlot, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 02" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_2",   RestoreItemMuteStateSlot, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 03" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_3",   RestoreItemMuteStateSlot, NULL, -3},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 04" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_4",   RestoreItemMuteStateSlot, NULL, -4},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 05" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_5",   RestoreItemMuteStateSlot, NULL, -5},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 06" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_6",   RestoreItemMuteStateSlot, NULL, -6},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 07" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_7",   RestoreItemMuteStateSlot, NULL, -7},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 08" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_8",   RestoreItemMuteStateSlot, NULL, -8},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 09" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_9",   RestoreItemMuteStateSlot, NULL, -9},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 10" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_10",  RestoreItemMuteStateSlot, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 11" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_11",  RestoreItemMuteStateSlot, NULL, -11},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 12" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_12",  RestoreItemMuteStateSlot, NULL, -12},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 13" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_13",  RestoreItemMuteStateSlot, NULL, -13},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 14" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_14",  RestoreItemMuteStateSlot, NULL, -14},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 15" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_15",  RestoreItemMuteStateSlot, NULL, -15},
	{ { DEFACCEL, "SWS/BR: Restore items' mute state to all items, slot 16" },                                                                             "BR_RESTORE_SOLO_MUTE_ALL_ITEMS_SLOT_16",  RestoreItemMuteStateSlot, NULL, -16},

	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 01" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_1",     SaveTrackSoloMuteStateSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 02" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_2",     SaveTrackSoloMuteStateSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 03" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_3",     SaveTrackSoloMuteStateSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 04" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_4",     SaveTrackSoloMuteStateSlot, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 05" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_5",     SaveTrackSoloMuteStateSlot, NULL, 5},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 06" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_6",     SaveTrackSoloMuteStateSlot, NULL, 6},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 07" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_7",     SaveTrackSoloMuteStateSlot, NULL, 7},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 08" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_8",     SaveTrackSoloMuteStateSlot, NULL, 8},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 09" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_9",     SaveTrackSoloMuteStateSlot, NULL, 9},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 10" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_10",    SaveTrackSoloMuteStateSlot, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 11" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_11",    SaveTrackSoloMuteStateSlot, NULL, 11},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 12" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_12",    SaveTrackSoloMuteStateSlot, NULL, 12},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 13" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_13",    SaveTrackSoloMuteStateSlot, NULL, 13},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 14" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_14",    SaveTrackSoloMuteStateSlot, NULL, 14},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 15" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_15",    SaveTrackSoloMuteStateSlot, NULL, 15},
	{ { DEFACCEL, "SWS/BR: Save selected tracks' solo and mute state, slot 16" },                                                                          "BR_SAVE_SOLO_MUTE_SEL_TRACKS_SLOT_16",    SaveTrackSoloMuteStateSlot, NULL, 16},

	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 01" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_1",     SaveTrackSoloMuteStateSlot, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 02" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_2",     SaveTrackSoloMuteStateSlot, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 03" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_3",     SaveTrackSoloMuteStateSlot, NULL, -3},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 04" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_4",     SaveTrackSoloMuteStateSlot, NULL, -4},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 05" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_5",     SaveTrackSoloMuteStateSlot, NULL, -5},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 06" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_6",     SaveTrackSoloMuteStateSlot, NULL, -6},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 07" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_7",     SaveTrackSoloMuteStateSlot, NULL, -7},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 08" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_8",     SaveTrackSoloMuteStateSlot, NULL, -8},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 09" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_9",     SaveTrackSoloMuteStateSlot, NULL, -9},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 10" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_10",    SaveTrackSoloMuteStateSlot, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 11" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_11",    SaveTrackSoloMuteStateSlot, NULL, -11},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 12" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_12",    SaveTrackSoloMuteStateSlot, NULL, -12},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 13" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_13",    SaveTrackSoloMuteStateSlot, NULL, -13},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 14" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_14",    SaveTrackSoloMuteStateSlot, NULL, -14},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 15" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_15",    SaveTrackSoloMuteStateSlot, NULL, -15},
	{ { DEFACCEL, "SWS/BR: Save all tracks' solo and mute state, slot 16" },                                                                               "BR_SAVE_SOLO_MUTE_ALL_TRACKS_SLOT_16",    SaveTrackSoloMuteStateSlot, NULL, -16},

	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 01" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_1",  RestoreTrackSoloMuteStateSlot, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 02" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_2",  RestoreTrackSoloMuteStateSlot, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 03" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_3",  RestoreTrackSoloMuteStateSlot, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 04" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_4",  RestoreTrackSoloMuteStateSlot, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 05" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_5",  RestoreTrackSoloMuteStateSlot, NULL, 5},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 06" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_6",  RestoreTrackSoloMuteStateSlot, NULL, 6},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 07" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_7",  RestoreTrackSoloMuteStateSlot, NULL, 7},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 08" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_8",  RestoreTrackSoloMuteStateSlot, NULL, 8},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 09" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_9",  RestoreTrackSoloMuteStateSlot, NULL, 9},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 10" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_10", RestoreTrackSoloMuteStateSlot, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 11" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_11", RestoreTrackSoloMuteStateSlot, NULL, 11},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 12" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_12", RestoreTrackSoloMuteStateSlot, NULL, 12},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 13" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_13", RestoreTrackSoloMuteStateSlot, NULL, 13},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 14" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_14", RestoreTrackSoloMuteStateSlot, NULL, 14},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 15" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_15", RestoreTrackSoloMuteStateSlot, NULL, 15},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to selected tracks, slot 16" },                                                             "BR_RESTORE_SOLO_MUTE_SEL_TRACKS_SLOT_16", RestoreTrackSoloMuteStateSlot, NULL, 16},

	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 01" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_1",  RestoreTrackSoloMuteStateSlot, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 02" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_2",  RestoreTrackSoloMuteStateSlot, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 03" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_3",  RestoreTrackSoloMuteStateSlot, NULL, -3},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 04" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_4",  RestoreTrackSoloMuteStateSlot, NULL, -4},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 05" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_5",  RestoreTrackSoloMuteStateSlot, NULL, -5},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 06" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_6",  RestoreTrackSoloMuteStateSlot, NULL, -6},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 07" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_7",  RestoreTrackSoloMuteStateSlot, NULL, -7},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 08" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_8",  RestoreTrackSoloMuteStateSlot, NULL, -8},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 09" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_9",  RestoreTrackSoloMuteStateSlot, NULL, -9},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 10" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_10", RestoreTrackSoloMuteStateSlot, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 11" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_11", RestoreTrackSoloMuteStateSlot, NULL, -11},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 12" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_12", RestoreTrackSoloMuteStateSlot, NULL, -12},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 13" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_13", RestoreTrackSoloMuteStateSlot, NULL, -13},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 14" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_14", RestoreTrackSoloMuteStateSlot, NULL, -14},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 15" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_15", RestoreTrackSoloMuteStateSlot, NULL, -15},
	{ { DEFACCEL, "SWS/BR: Restore tracks' solo and mute state to all tracks, slot 16" },                                                                  "BR_RESTORE_SOLO_MUTE_ALL_TRACKS_SLOT_16", RestoreTrackSoloMuteStateSlot, NULL, -16},

	/******************************************************************************
	* Misc - REAPER preferences                                                   *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Grid snap settings follow grid visibility\"" },                               "BR_OPTIONS_SNAP_FOLLOW_GRID_VIS",    SnapFollowsGridVis, NULL, 0, IsSnapFollowsGridVisOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Playback position follows project timebase when changing tempo\"" },          "BR_OPTIONS_PLAYBACK_TEMPO_CHANGE",   PlaybackFollowsTempoChange, NULL, 0, IsPlaybackFollowsTempoChangeOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Apply trim when adding volume/pan envelopes\" to \"Always\"" },                  "BR_OPTIONS_ENV_TRIM_ALWAYS",         TrimNewVolPanEnvs, NULL, 0, IsTrimNewVolPanEnvsOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Apply trim when adding volume/pan envelopes\" to \"In read/write\"" },           "BR_OPTIONS_ENV_TRIM_READWRITE",      TrimNewVolPanEnvs, NULL, 1, IsTrimNewVolPanEnvsOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Apply trim when adding volume/pan envelopes\" to \"Never\"" },                   "BR_OPTIONS_ENV_TRIM_NEVER",          TrimNewVolPanEnvs, NULL, 2, IsTrimNewVolPanEnvsOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Display media item take name\"" },                                            "BR_OPTIONS_DISPLAY_ITEM_TAKE_NAME",  ToggleDisplayItemLabels, NULL, 0, IsToggleDisplayItemLabelsOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Display media item pitch/playrate if set\"" },                                "BR_OPTIONS_DISPLAY_ITEM_PITCH_RATE", ToggleDisplayItemLabels, NULL, 2, IsToggleDisplayItemLabelsOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Display media item gain if set\"" },                                          "BR_OPTIONS_DISPLAY_ITEM_GAIN",       ToggleDisplayItemLabels, NULL, 4, IsToggleDisplayItemLabelsOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Send all-notes-off on stop/play\"" },                                         "BR_OPTIONS_STOP_PLAY_NOTES_OFF",     SetMidiResetOnPlayStop, NULL, 0, IsSetMidiResetOnPlayStopOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Reset pitch on stop/play\"" },                                                "BR_OPTIONS_STOP_PLAY_RESET_PITCH",   SetMidiResetOnPlayStop, NULL, 1, IsSetMidiResetOnPlayStopOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Reset CC on stop/play\"" },                                                   "BR_OPTIONS_STOP_PLAY_RESET_CC",      SetMidiResetOnPlayStop, NULL, 2, IsSetMidiResetOnPlayStopOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Flush FX on stop\"" },                                                        "BR_OPTIONS_FLUSH_FX_ON_STOP",        SetOptionsFX, NULL, 1,      IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Flush FX when looping\"" },                                                   "BR_OPTIONS_FLUSH_FX_WHEN_LOOPING",   SetOptionsFX, NULL, 2,      IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 0 ms" },                                          "BR_OPTIONS_RUN_FX_AFTER_STOP_0",     SetOptionsFX, NULL, 0,      IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 100 ms" },                                        "BR_OPTIONS_RUN_FX_AFTER_STOP_100",   SetOptionsFX, NULL, -100,   IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 500 ms" },                                        "BR_OPTIONS_RUN_FX_AFTER_STOP_500",   SetOptionsFX, NULL, -500,   IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 1000 ms" },                                       "BR_OPTIONS_RUN_FX_AFTER_STOP_1000",  SetOptionsFX, NULL, -1000,  IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 2000 ms" },                                       "BR_OPTIONS_RUN_FX_AFTER_STOP_2000",  SetOptionsFX, NULL, -2000,  IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 3000 ms" },                                       "BR_OPTIONS_RUN_FX_AFTER_STOP_3000",  SetOptionsFX, NULL, -3000,  IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 4000 ms" },                                       "BR_OPTIONS_RUN_FX_AFTER_STOP_4000",  SetOptionsFX, NULL, -4000,  IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 5000 ms" },                                       "BR_OPTIONS_RUN_FX_AFTER_STOP_5000",  SetOptionsFX, NULL, -5000,  IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 6000 ms" },                                       "BR_OPTIONS_RUN_FX_AFTER_STOP_6000",  SetOptionsFX, NULL, -6000,  IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 7000 ms" },                                       "BR_OPTIONS_RUN_FX_AFTER_STOP_7000",  SetOptionsFX, NULL, -7000,  IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 8000 ms" },                                       "BR_OPTIONS_RUN_FX_AFTER_STOP_8000",  SetOptionsFX, NULL, -8000,  IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 9000 ms" },                                       "BR_OPTIONS_RUN_FX_AFTER_STOP_9000",  SetOptionsFX, NULL, -9000,  IsSetOptionsFXOn},
	{ { DEFACCEL, "SWS/BR: Options - Set \"Run FX after stopping for\" to 10000 ms" },                                      "BR_OPTIONS_RUN_FX_AFTER_STOP_10000", SetOptionsFX, NULL, -10000, IsSetOptionsFXOn},

	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Move edit cursor to start of time selection on time selection change\"" },    "BR_OPTIONS_MOVE_CUR_ON_TIME_SEL",    SetMoveCursorOnPaste, NULL, 2, IsSetMoveCursorOnPasteOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Move edit cursor when pasting/inserting media\"" },                           "BR_OPTIONS_MOVE_CUR_ON_PASTE",       SetMoveCursorOnPaste, NULL, -3, IsSetMoveCursorOnPasteOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Move edit cursor to end of recorded items on record stop\"" },                "BR_OPTIONS_MOVE_CUR_ON_RECORD_STOP", SetMoveCursorOnPaste, NULL, 4, IsSetMoveCursorOnPasteOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Stop/repeat playback at end of project\"" },                                  "BR_OPTIONS_STOP_PLAYBACK_PROJ_END",  SetPlaybackStopOptions, NULL, 0, IsSetPlaybackStopOptionsOn},
	{ { DEFACCEL, "SWS/BR: Options - Toggle \"Scroll view to edit cursor on stop\"" },                                      "BR_OPTIONS_SCROLL_TO_CURS_ON_STOP",  SetPlaybackStopOptions, NULL, 3, IsSetPlaybackStopOptionsOn},

	{ { DEFACCEL, "SWS/BR: Options - Set grid line Z order to \"Over items\"" },                                            "BR_OPTIONS_GRID_Z_OVER_ITEMS",       SetGridMarkerZOrder, NULL, 1,  IsSetGridMarkerZOrderOn},
	{ { DEFACCEL, "SWS/BR: Options - Set grid line Z order to \"Through items\"" },                                         "BR_OPTIONS_GRID_Z_THROUGH_ITEMS",    SetGridMarkerZOrder, NULL, 2,  IsSetGridMarkerZOrderOn},
	{ { DEFACCEL, "SWS/BR: Options - Set grid line Z order to \"Under items\"" },                                           "BR_OPTIONS_GRID_Z_UNDER_ITEMS",      SetGridMarkerZOrder, NULL, 3,  IsSetGridMarkerZOrderOn},
	{ { DEFACCEL, "SWS/BR: Options - Set marker line Z order to \"Over items\"" },                                          "BR_OPTIONS_MARKER_Z_OVER_ITEMS",     SetGridMarkerZOrder, NULL, -1, IsSetGridMarkerZOrderOn},
	{ { DEFACCEL, "SWS/BR: Options - Set marker line Z order to \"Through items\"" },                                       "BR_OPTIONS_MARKER_Z_THROUGH_ITEMS",  SetGridMarkerZOrder, NULL, -2, IsSetGridMarkerZOrderOn},
	{ { DEFACCEL, "SWS/BR: Options - Set marker line Z order to \"Under items\"" },                                         "BR_OPTIONS_MARKER_Z_UNDER_ITEMS",    SetGridMarkerZOrder, NULL, -3, IsSetGridMarkerZOrderOn},

	{ { DEFACCEL, "SWS/BR: Options - Automatically insert stretch markers when inserting tempo markers with SWS actions" }, "BR_OPTIONS_TEMPO_AUTO_STRETCH_M",    SetAutoStretchMarkers, NULL, -1, IsSetAutoStretchMarkersOn },

	{ { DEFACCEL, "SWS/BR: Options - Cycle through record modes" },                                                         "BR_CYCLE_RECORD_MODES",              CycleRecordModes},

	/******************************************************************************
	* Misc - Media item preview                                                   *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse" },                                                                                          "BR_PREV_ITEM_CURSOR",                 PreviewItemAtMouse, NULL, 11111},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse (start from mouse cursor position)" },                                                       "BR_PREV_ITEM_CURSOR_POS",             PreviewItemAtMouse, NULL, 11211},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse (sync with next measure)" },                                                                 "BR_PREV_ITEM_CURSOR_SYNC",            PreviewItemAtMouse, NULL, 11311},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse and pause during preview" },                                                                 "BR_PREV_ITEM_PAUSE_CURSOR",           PreviewItemAtMouse, NULL, 11121},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse and pause during preview (start from mouse cursor position)" },                              "BR_PREV_ITEM_PAUSE_CURSOR_POS",       PreviewItemAtMouse, NULL, 11221},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse at track fader volume" },                                                                    "BR_PREV_ITEM_CURSOR_FADER",           PreviewItemAtMouse, NULL, 12111},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse at track fader volume (start from mouse position)" },                                        "BR_PREV_ITEM_CURSOR_FADER_POS",       PreviewItemAtMouse, NULL, 12211},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse at track fader volume (sync with next measure)" },                                           "BR_PREV_ITEM_CURSOR_FADER_SYNC",      PreviewItemAtMouse, NULL, 12311},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse at track fader volume and pause during preview" },                                           "BR_PREV_ITEM_PAUSE_CURSOR_FADER",     PreviewItemAtMouse, NULL, 12121},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse at track fader volume and pause during preview (start from mouse cursor position)" },        "BR_PREV_ITEM_PAUSE_CURSOR_FADER_POS", PreviewItemAtMouse, NULL, 12221},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse through track" },                                                                            "BR_PREV_ITEM_CURSOR_TRACK",           PreviewItemAtMouse, NULL, 13111},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse through track (start from mouse position)" },                                                "BR_PREV_ITEM_CURSOR_TRACK_POS",       PreviewItemAtMouse, NULL, 13211},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse through track (sync with next measure)" },                                                   "BR_PREV_ITEM_CURSOR_TRACK_SYNC",      PreviewItemAtMouse, NULL, 13311},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse through track and pause during preview" },                                                   "BR_PREV_ITEM_PAUSE_CURSOR_TRACK",     PreviewItemAtMouse, NULL, 13121},
	{ { DEFACCEL, "SWS/BR: Preview media item under mouse through track and pause during preview (start from mouse cursor position)" },                "BR_PREV_ITEM_PAUSE_CURSOR_TRACK_POS", PreviewItemAtMouse, NULL, 13221},

	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse" },                                                                                   "BR_TPREV_ITEM_CURSOR",                 PreviewItemAtMouse, NULL, 21111},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse (start from mouse position)" },                                                       "BR_TPREV_ITEM_CURSOR_POS",             PreviewItemAtMouse, NULL, 21211},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse (sync with next measure)" },                                                          "BR_TPREV_ITEM_CURSOR_SYNC",            PreviewItemAtMouse, NULL, 21311},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse and pause during preview" },                                                          "BR_TPREV_ITEM_PAUSE_CURSOR",           PreviewItemAtMouse, NULL, 21121},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse and pause during preview (start from mouse cursor position)" },                       "BR_TPREV_ITEM_PAUSE_CURSOR_POS",       PreviewItemAtMouse, NULL, 21221},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse at track fader volume" },                                                             "BR_TPREV_ITEM_CURSOR_FADER",           PreviewItemAtMouse, NULL, 22111},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse at track fader volume (start from mouse position)" },                                 "BR_TPREV_ITEM_CURSOR_FADER_POS",       PreviewItemAtMouse, NULL, 22211},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse at track fader volume (sync with next measure)" },                                    "BR_TPREV_ITEM_CURSOR_FADER_SYNC",      PreviewItemAtMouse, NULL, 22311},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse at track fader volume and pause during preview" },                                    "BR_TPREV_ITEM_PAUSE_CURSOR_FADER",     PreviewItemAtMouse, NULL, 22121},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse at track fader volume and pause during preview (start from mouse cursor position)" }, "BR_TPREV_ITEM_PAUSE_CURSOR_FADER_POS", PreviewItemAtMouse, NULL, 22221},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse through track" },                                                                     "BR_TPREV_ITEM_CURSOR_TRACK",           PreviewItemAtMouse, NULL, 23111},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse through track (start from mouse position)" },                                         "BR_TPREV_ITEM_CURSOR_TRACK_POS",       PreviewItemAtMouse, NULL, 23211},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse through track (sync with next measure)" },                                            "BR_TPREV_ITEM_CURSOR_TRACK_SYNC",      PreviewItemAtMouse, NULL, 23311},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse through track and pause during preview" },                                            "BR_TPREV_ITEM_PAUSE_CURSOR_TRACK",     PreviewItemAtMouse, NULL, 23121},
	{ { DEFACCEL, "SWS/BR: Toggle preview media item under mouse through track and pause during preview (start from mouse cursor position)" },         "BR_TPREV_ITEM_PAUSE_CURSOR_TRACK_POS", PreviewItemAtMouse, NULL, 23221},

	{ { DEFACCEL, "SWS/BR: Preview take under mouse" },                                                                                                "BR_PREV_TAKE_CURSOR",                 PreviewItemAtMouse, NULL, 11112},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse (start from mouse cursor position)" },                                                             "BR_PREV_TAKE_CURSOR_POS",             PreviewItemAtMouse, NULL, 11212},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse (sync with next measure)" },                                                                       "BR_PREV_TAKE_CURSOR_SYNC",            PreviewItemAtMouse, NULL, 11312},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse and pause during preview" },                                                                       "BR_PREV_TAKE_PAUSE_CURSOR",           PreviewItemAtMouse, NULL, 11122},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse and pause during preview (start from mouse cursor position)" },                                    "BR_PREV_TAKE_PAUSE_CURSOR_POS",       PreviewItemAtMouse, NULL, 11222},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse at track fader volume" },                                                                          "BR_PREV_TAKE_CURSOR_FADER",           PreviewItemAtMouse, NULL, 12112},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse at track fader volume (start from mouse position)" },                                              "BR_PREV_TAKE_CURSOR_FADER_POS",       PreviewItemAtMouse, NULL, 12212},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse at track fader volume (sync with next measure)" },                                                 "BR_PREV_TAKE_CURSOR_FADER_SYNC",      PreviewItemAtMouse, NULL, 12312},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse at track fader volume and pause during preview" },                                                 "BR_PREV_TAKE_PAUSE_CURSOR_FADER",     PreviewItemAtMouse, NULL, 12122},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse at track fader volume and pause during preview (start from mouse cursor position)" },              "BR_PREV_TAKE_PAUSE_CURSOR_FADER_POS", PreviewItemAtMouse, NULL, 12222},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse through track" },                                                                                  "BR_PREV_TAKE_CURSOR_TRACK",           PreviewItemAtMouse, NULL, 13112},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse through track (start from mouse position)" },                                                      "BR_PREV_TAKE_CURSOR_TRACK_POS",       PreviewItemAtMouse, NULL, 13212},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse through track (sync with next measure)" },                                                         "BR_PREV_TAKE_CURSOR_TRACK_SYNC",      PreviewItemAtMouse, NULL, 13312},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse through track and pause during preview" },                                                         "BR_PREV_TAKE_PAUSE_CURSOR_TRACK",     PreviewItemAtMouse, NULL, 13122},
	{ { DEFACCEL, "SWS/BR: Preview take under mouse through track and pause during preview (start from mouse cursor position)" },                      "BR_PREV_TAKE_PAUSE_CURSOR_TRACK_POS", PreviewItemAtMouse, NULL, 13222},

	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse" },                                                                                         "BR_TPREV_TAKE_CURSOR",                 PreviewItemAtMouse, NULL, 21112},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse (start from mouse position)" },                                                             "BR_TPREV_TAKE_CURSOR_POS",             PreviewItemAtMouse, NULL, 21212},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse (sync with next measure)" },                                                                "BR_TPREV_TAKE_CURSOR_SYNC",            PreviewItemAtMouse, NULL, 21312},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse and pause during preview" },                                                                "BR_TPREV_TAKE_PAUSE_CURSOR",           PreviewItemAtMouse, NULL, 21122},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse and pause during preview (start from mouse cursor position)" },                             "BR_TPREV_TAKE_PAUSE_CURSOR_POS",       PreviewItemAtMouse, NULL, 21222},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse at track fader volume" },                                                                   "BR_TPREV_TAKE_CURSOR_FADER",           PreviewItemAtMouse, NULL, 22112},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse at track fader volume (start from mouse position)" },                                       "BR_TPREV_TAKE_CURSOR_FADER_POS",       PreviewItemAtMouse, NULL, 22212},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse at track fader volume (sync with next measure)" },                                          "BR_TPREV_TAKE_CURSOR_FADER_SYNC",      PreviewItemAtMouse, NULL, 22312},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse at track fader volume and pause during preview" },                                          "BR_TPREV_TAKE_PAUSE_CURSOR_FADER",     PreviewItemAtMouse, NULL, 22122},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse at track fader volume and pause during preview (start from mouse cursor position)" },       "BR_TPREV_TAKE_PAUSE_CURSOR_FADER_POS", PreviewItemAtMouse, NULL, 22222},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse through track" },                                                                           "BR_TPREV_TAKE_CURSOR_TRACK",           PreviewItemAtMouse, NULL, 23112},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse through track (start from mouse position)" },                                               "BR_TPREV_TAKE_CURSOR_TRACK_POS",       PreviewItemAtMouse, NULL, 23212},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse through track (sync with next measure)" },                                                  "BR_TPREV_TAKE_CURSOR_TRACK_SYNC",      PreviewItemAtMouse, NULL, 23312},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse through track and pause during preview" },                                                  "BR_TPREV_TAKE_PAUSE_CURSOR_TRACK",     PreviewItemAtMouse, NULL, 23122},
	{ { DEFACCEL, "SWS/BR: Toggle preview take under mouse through track and pause during preview (start from mouse cursor position)" },               "BR_TPREV_TAKE_PAUSE_CURSOR_TRACK_POS", PreviewItemAtMouse, NULL, 23222},

	/******************************************************************************
	* Misc - Adjust playrate                                                      *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Adjust playrate (MIDI CC only)" }, "BR_ADJUST_PLAYRATE_MIDI",         NULL, NULL, 0, NULL,                           0, AdjustPlayrate},
	{ { DEFACCEL, "SWS/BR: Adjust playrate options..." },     "BR_ADJUST_PLAYRATE_MIDI_OPTIONS", NULL, NULL, 1, IsAdjustPlayrateOptionsVisible, 0, AdjustPlayrate},

	/******************************************************************************
	* Misc - Project track selection action                                       *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Project track selection action - Set..." },  "BR_PROJ_TRACK_SEL_ACTION_SET",   SetProjectTrackSelAction, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Project track selection action - Show..." }, "BR_PROJ_TRACK_SEL_ACTION_SHOW",  ShowProjectTrackSelAction, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Project track selection action - Clear..." },"BR_PROJ_TRACK_SEL_ACTION_CLEAR", ClearProjectTrackSelAction, NULL, 0},

	/******************************************************************************
	* Tempo                                                                       *
	******************************************************************************/
	{ { DEFACCEL, "SWS/BR: Move closest grid line to edit cursor" },            "BR_MOVE_GRID_TO_EDIT_CUR",   MoveGridToEditPlayCursor, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Move closest grid line to play cursor" },            "BR_MOVE_GRID_TO_PLAY_CUR",   MoveGridToEditPlayCursor, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Move closest measure grid line to edit cursor" },    "BR_MOVE_M_GRID_TO_EDIT_CUR", MoveGridToEditPlayCursor, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Move closest measure grid line to play cursor" },    "BR_MOVE_M_GRID_TO_PLAY_CUR", MoveGridToEditPlayCursor, NULL, 3},
	{ { DEFACCEL, "SWS/BR: Move closest left side grid line to edit cursor" },  "BR_MOVE_L_GRID_TO_EDIT_CUR", MoveGridToEditPlayCursor, NULL, 4},
	{ { DEFACCEL, "SWS/BR: Move closest right side grid line to edit cursor" }, "BR_MOVE_R_GRID_TO_EDIT_CUR", MoveGridToEditPlayCursor, NULL, 5},

	{ { DEFACCEL, "SWS/BR: Move tempo marker forward 0.1 ms" },                                                               "SWS_BRMOVETEMPOFORWARD01",    MoveTempo, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward 1 ms" },                                                                 "SWS_BRMOVETEMPOFORWARD1",     MoveTempo, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward 10 ms" },                                                                "SWS_BRMOVETEMPOFORWARD10",    MoveTempo, NULL, 100},
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward 100 ms" },                                                               "SWS_BRMOVETEMPOFORWARD100",   MoveTempo, NULL, 1000},
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward 1000 ms" },                                                              "SWS_BRMOVETEMPOFORWARD1000",  MoveTempo, NULL, 10000},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back 0.1 ms" },                                                                  "SWS_BRMOVETEMPOBACK01",       MoveTempo, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back 1 ms" },                                                                    "SWS_BRMOVETEMPOBACK1",        MoveTempo, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back 10 ms" },                                                                   "SWS_BRMOVETEMPOBACK10",       MoveTempo, NULL, -100},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back 100 ms" },                                                                  "SWS_BRMOVETEMPOBACK100",      MoveTempo, NULL, -1000},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back 1000 ms" },                                                                 "SWS_BRMOVETEMPOBACK1000",     MoveTempo, NULL, -10000},
	{ { DEFACCEL, "SWS/BR: Move tempo marker forward" },                                                                      "SWS_BRMOVETEMPOFORWARD",      MoveTempo, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Move tempo marker back" },                                                                         "SWS_BRMOVETEMPOBACK",         MoveTempo, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Move closest tempo marker to edit cursor" },                                                       "BR_MOVE_CLOSEST_TEMPO",       MoveTempo, NULL, 3},

	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.001 BPM (preserve overall tempo)" },                                       "BR_INC_TEMPO_0.001_BPM",      EditTempo, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.01 BPM (preserve overall tempo)" },                                        "BR_INC_TEMPO_0.01_BPM",       EditTempo, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.1 BPM (preserve overall tempo)" },                                         "BR_INC_TEMPO_0.1_BPM",        EditTempo, NULL, 100},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 01 BPM (preserve overall tempo)" },                                          "BR_INC_TEMPO_1_BPM",          EditTempo, NULL, 1000},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.001 BPM (preserve overall tempo)" },                                       "BR_DEC_TEMPO_0.001_BPM",      EditTempo, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.01 BPM (preserve overall tempo)" },                                        "BR_DEC_TEMPO_0.01_BPM",       EditTempo, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.1 BPM (preserve overall tempo)" },                                         "BR_DEC_TEMPO_0.1_BPM",        EditTempo, NULL, -100},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 01 BPM (preserve overall tempo)" },                                          "BR_DEC_TEMPO_1_BPM",          EditTempo, NULL, -1000},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.001% (preserve overall tempo)" },                                          "BR_INC_TEMPO_0.001_PERC",     EditTempo, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.01% (preserve overall tempo)" },                                           "BR_INC_TEMPO_0.01_PERC",      EditTempo, NULL, 20},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 0.1% (preserve overall tempo)" },                                            "BR_INC_TEMPO_0.1_PERC",       EditTempo, NULL, 200},
	{ { DEFACCEL, "SWS/BR: Increase tempo marker 01% (preserve overall tempo)" },                                             "BR_INC_TEMPO_1_PERC",         EditTempo, NULL, 2000},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.001% (preserve overall tempo)" },                                          "BR_DEC_TEMPO_0.001_PERC",     EditTempo, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.01% (preserve overall tempo)" },                                           "BR_DEC_TEMPO_0.01_PERC",      EditTempo, NULL, -20},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 0.1% (preserve overall tempo)" },                                            "BR_DEC_TEMPO_0.1_PERC",       EditTempo, NULL, -200},
	{ { DEFACCEL, "SWS/BR: Decrease tempo marker 01% (preserve overall tempo)" },                                             "BR_DEC_TEMPO_1_PERC",         EditTempo, NULL, -2000},

	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.001 BPM)" },                                       "BR_INC_GR_TEMPO_0.001_BPM",   EditTempoGradual, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.01 BPM)" },                                        "BR_INC_GR_TEMPO_0.01_BPM",    EditTempoGradual, NULL, 10},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.1 BPM)" },                                         "BR_INC_GR_TEMPO_0.1_BPM",     EditTempoGradual, NULL, 100},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 01 BPM)" },                                          "BR_INC_GR_TEMPO_1_BPM",       EditTempoGradual, NULL, 1000},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.001 BPM)" },                                       "BR_DEC_GR_TEMPO_0.001_BPM",   EditTempoGradual, NULL, -1},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.01 BPM)" },                                        "BR_DEC_GR_TEMPO_0.01_BPM",    EditTempoGradual, NULL, -10},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.1 BPM)" },                                         "BR_DEC_GR_TEMPO_0.1_BPM",     EditTempoGradual, NULL, -100},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 01 BPM)" },                                          "BR_DEC_GR_TEMPO_1_BPM",       EditTempoGradual, NULL, -1000},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.001%)" },                                          "BR_INC_GR_TEMPO_0.001_PERC",  EditTempoGradual, NULL, 2},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.01%)" },                                           "BR_INC_GR_TEMPO_0.01_PERC",   EditTempoGradual, NULL, 20},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 0.1%)" },                                            "BR_INC_GR_TEMPO_0.1_PERC",    EditTempoGradual, NULL, 200},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (increase 01%)" },                                             "BR_INC_GR_TEMPO_1_PERC",      EditTempoGradual, NULL, 2000},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.001%)" },                                          "BR_DEC_GR_TEMPO_0.001_PERC",  EditTempoGradual, NULL, -2},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.01%)" },                                           "BR_DEC_GR_TEMPO_0.01_PERC",   EditTempoGradual, NULL, -20},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 0.1%)" },                                            "BR_DEC_GR_TEMPO_0.1_PERC",    EditTempoGradual, NULL, -200},
	{ { DEFACCEL, "SWS/BR: Alter slope of gradual tempo marker (decrease 01%)" },                                             "BR_DEC_GR_TEMPO_1_PERC",      EditTempoGradual, NULL, -2000},

	{ { DEFACCEL, "SWS/BR: Delete tempo marker (preserve overall tempo and positions if possible)" },                         "BR_DELETE_TEMPO",             DeleteTempo},
	{ { DEFACCEL, "SWS/BR: Delete tempo marker and preserve position and length of items (including MIDI events)" },          "BR_DELETE_TEMPO_ITEMS",       DeleteTempoPreserveItems, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Delete tempo marker and preserve position and length of selected items (including MIDI events)" }, "BR_DELETE_TEMPO_ITEMS_SEL",   DeleteTempoPreserveItems, NULL, 1},

	{ { DEFACCEL, "SWS/BR: Create tempo markers at grid after every selected tempo marker" },                                 "BR_TEMPO_GRID",               TempoAtGrid},

	{ { DEFACCEL, "SWS/BR: Select all partial time signature markers" },                                                      "BR_PART_TIME_SIG_SEL",        SelectMovePartialTimeSig, NULL, 0},
	{ { DEFACCEL, "SWS/BR: Reset position of selected partial time signature markers" },                                      "BR_PART_TIME_SIG_RESET",      SelectMovePartialTimeSig, NULL, 1},
	{ { DEFACCEL, "SWS/BR: Snap position of selected partial time signature markers to closest grid line" },                  "BR_PART_TIME_SIG_SNAP_GRID",  SelectMovePartialTimeSig, NULL, 2},

	{ { DEFACCEL, "SWS/BR: Tempo help..." },                                                                                  "BR_TEMPO_HELP_WIKI",          OpenTempoWiki},

	{ { DEFACCEL, "SWS/BR: Convert project markers to tempo markers..." },                                                    "SWS_BRCONVERTMARKERSTOTEMPO", ConvertMarkersToTempoDialog, NULL, 0, IsConvertMarkersToTempoVisible},
	{ { DEFACCEL, "SWS/BR: Select and adjust tempo markers..." },                                                             "SWS_BRADJUSTSELTEMPO",        SelectAdjustTempoDialog, NULL, 0, IsSelectAdjustTempoVisible},
	{ { DEFACCEL, "SWS/BR: Randomize tempo markers..." },                                                                     "BR_RANDOMIZE_TEMPO",          RandomizeTempoDialog},

	{ { DEFACCEL, "SWS/BR: Set tempo marker shape (options)..." },                                                            "BR_TEMPO_SHAPE_OPTIONS",      TempoShapeOptionsDialog, NULL, 0, IsTempoShapeOptionsVisible},
	{ { DEFACCEL, "SWS/BR: Set tempo marker shape to linear (preserve positions)" },                                          "BR_TEMPO_SHAPE_LINEAR",       TempoShapeLinear},
	{ { DEFACCEL, "SWS/BR: Set tempo marker shape to square (preserve positions)" },                                          "BR_TEMPO_SHAPE_SQUARE",       TempoShapeSquare},

	{ {}, LAST_COMMAND}
};
//!WANT_LOCALIZE_1ST_STRING_END

/******************************************************************************
* BR init/exit                                                                *
******************************************************************************/
int BR_Init ()
{
	// Register actions
	SWSRegisterCommands(g_commandTable);

	// Run various init functions
	ContextualToolbarsInitExit(true);
	ContinuousActionsInitExit(true);
	LoudnessInitExit(true);
	ProjectTrackSelInitExit(true);
	ProjStateInitExit(true);
	VersionCheckInitExit(true);

	// Load various global variables
	COMMAND_T ct = {};
	ct.user = (INT_PTR)GetPrivateProfileInt("common", "autoStretchMarkersTempo", 1, GetIniFileBR());
	SetAutoStretchMarkers(&ct);

	// Keep "apply next action" registration mechanism here (no need for separate module until more actions are added)
	g_nextActionApplyers.insert(NamedCommandLookup("_BR_NEXT_CMD_SEL_TK_VIS_ENVS"));        // Make sure these actions are registered consequentially
	g_nextActionApplyers.insert(NamedCommandLookup("_BR_NEXT_CMD_SEL_TK_REC_ENVS"));        // in g_commandTable, so their cmds end up consequential
	g_nextActionApplyers.insert(NamedCommandLookup("_BR_NEXT_CMD_SEL_TK_VIS_ENVS_NOSEL"));  // to optimize BR_SwsActionHook
	g_nextActionApplyers.insert(NamedCommandLookup("_BR_NEXT_CMD_SEL_TK_REC_ENVS_NOSEL"));
	g_nextActionLoCmd  = (g_nextActionApplyers.size() > 0) ? *g_nextActionApplyers.begin()  : 0;
	g_nextActionHiCmd  = (g_nextActionApplyers.size() > 0) ? *g_nextActionApplyers.rbegin() : 0;

	if (atof(GetAppVersion()) < 6) {
		// The scrollbar size is now included in the scrollinfo page size
		// since REAPER v6. This restores the previous scrollbar compensation for
		// compatibility with REAPER v5. See issue #1279.
		SCROLLBAR_W = 17; // legacy
	}

	return 1;
}

int BR_InitPost ()
{
	return 1;
}

void BR_Exit ()
{
	// Load various global variables
	char tmp[512];
	snprintf(tmp, sizeof(tmp), "%d", IsSetAutoStretchMarkersOn(NULL));
	WritePrivateProfileString("common", "autoStretchMarkersTempo", tmp, GetIniFileBR());

	// Run various exit functions
	ContextualToolbarsInitExit(false);
	ContinuousActionsInitExit(false);
	LoudnessInitExit(false);
	ProjectTrackSelInitExit(false);
	ProjStateInitExit(false);
	VersionCheckInitExit(false);

	ME_StopMidiTakePreview(NULL, 0, 0, 0, NULL); // in case any kind of preview is happening right now, make sure it's stopped
}
