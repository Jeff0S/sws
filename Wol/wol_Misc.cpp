/******************************************************************************
/ wol_Misc.cpp
/
/ Copyright (c) 2014 wol
/ http://forum.cockos.com/member.php?u=70153
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
#include "wol_Misc.h"
#include "wol_Util.h"
#include "../SnM/SnM_Dlg.h"
#include "../SnM/SnM_Util.h"
#include "../reaper/localize.h"

static struct wol_Misc_Ini {
	static const char* Section;
	static const char* RandomizerSlotsMin[8];
	static const char* RandomizerSlotsMax[8];
	static const char* NoteVelocitiesSlotsMin[8];
	static const char* NoteVelocitiesSlotsMax[8];
	static const char* SelRandMidiNotesPercent[8];
	static const char* EnableRealtimeRandomize;
	static const char* EnableRealtimeNoteSelectionByVelInRange;
	static const char* AddSelectedNotesByVelInRangeToSel;
	static const char* EnableRealtimeRandomNoteSelectionPerc;
	static const char* SelectRandomMidiNotesPercType;
} wol_Misc_Ini;

const char* wol_Misc_Ini::Section = "Misc";



///////////////////////////////////////////////////////////////////////////////////////////////////
// Navigation
///////////////////////////////////////////////////////////////////////////////////////////////////
void MoveEditCursorToBeatN(COMMAND_T* ct)
{
	int beat = (int)ct->user - 1;
	int meas = 0, cml = 0;
	TimeMap2_timeToBeats(NULL, GetCursorPosition(), &meas, &cml, 0, 0);
	if (beat >= cml)
		beat = cml - 1;
	SetEditCurPos(TimeMap2_beatsToTime(0, (double)beat, &meas), 1, 0);
}

void MoveEditCursorTo(COMMAND_T* ct)
{
	if ((int)ct->user == 0)
	{
		int meas = 0;
		SetEditCurPos(TimeMap2_beatsToTime(NULL, (double)(int)(TimeMap2_timeToBeats(NULL, GetCursorPosition(), &meas, NULL, NULL, NULL) + 0.5), &meas), true, false);
	}
	else if ((int)ct->user > 0)
	{
		if ((int)ct->user == 3 && GetToggleCommandState(40370) == 1)
			ApplyNudge(NULL, 2, 6, 18, 1.0f, false, 0);
		else
		{
			int meas = 0, cml = 0;
			int beat = (int)TimeMap2_timeToBeats(NULL, GetCursorPosition(), &meas, &cml, NULL, NULL);
			if (beat == cml - 1)
			{
				if ((int)ct->user > 1)
					++meas;
				SetEditCurPos(TimeMap2_beatsToTime(NULL, 0.0f, &meas), true, false);
			}
			else
				SetEditCurPos(TimeMap2_beatsToTime(NULL, (double)(beat + 1), &meas), true, false);
		}
	}
	else
	{
		if ((int)ct->user == -3 && GetToggleCommandState(40370) == 1)
			ApplyNudge(NULL, 2, 6, 18, 1.0f, true, 0);
		else
		{
			int meas = 0, cml = 0;
			int beat = (int)TimeMap2_timeToBeats(NULL, GetCursorPosition(), &meas, &cml, NULL, NULL);
			if (beat == 0)
			{
				if ((int)ct->user < -1)
					--meas;
				SetEditCurPos(TimeMap2_beatsToTime(NULL, (double)(cml - 1), &meas), true, false);
			}
			else
				SetEditCurPos(TimeMap2_beatsToTime(NULL, (double)(beat - 1), &meas), true, false);
		}
	}
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// Track
///////////////////////////////////////////////////////////////////////////////////////////////////
void SelectAllTracksExceptFolderParents(COMMAND_T* ct)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		GetSetMediaTrackInfo(tr, "I_SELECTED", *(int*)GetSetMediaTrackInfo(tr, "I_FOLDERDEPTH", NULL) == 1 ? &g_i0 : &g_i1);
	}
	Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// Midi
///////////////////////////////////////////////////////////////////////////////////////////////////
void MoveEditCursorToNote(COMMAND_T* ct, int val, int valhw, int relmode, HWND hwnd)
{
	if (MediaItem_Take* take = MIDIEditor_GetTake(MIDIEditor_GetActive()))
	{
		int note_count = 0;
		MIDI_CountEvts(take, &note_count, NULL, NULL);
		int i = 0;
		for (i = 0; i < note_count; ++i)
		{
			double start_pos = 0.0f;
			if (MIDI_GetNote(take, i, NULL, NULL, &start_pos, NULL, NULL, NULL, NULL))
			{
				if (((int)ct->user < 0) ? MIDI_GetProjTimeFromPPQPos(take, start_pos) >= GetCursorPosition() : MIDI_GetProjTimeFromPPQPos(take, start_pos) > GetCursorPosition())
				{
					if ((int)ct->user < 0 && i > 0)
						MIDI_GetNote(take, i - 1, NULL, NULL, &start_pos, NULL, NULL, NULL, NULL);
					SetEditCurPos(MIDI_GetProjTimeFromPPQPos(take, start_pos), true, false);
					break;
				}
			}
		}
		if ((int)ct->user < 0 && i == note_count)
		{
			double start_pos = 0.0f;
			MIDI_GetNote(take, i - 1, NULL, NULL, &start_pos, NULL, NULL, NULL, NULL);
			SetEditCurPos(MIDI_GetProjTimeFromPPQPos(take, start_pos), true, false);
		}
	}
}

//---------//
#define CMD_RT_RANDMIDIVEL UserInputAndSlotsEditorWnd::CMD_OPTION_MIN

const char* wol_Misc_Ini::RandomizerSlotsMin[8] = {
	"RandomizerSlot1Min",
	"RandomizerSlot2Min",
	"RandomizerSlot3Min",
	"RandomizerSlot4Min",
	"RandomizerSlot5Min",
	"RandomizerSlot6Min",
	"RandomizerSlot7Min",
	"RandomizerSlot8Min",
};
const char* wol_Misc_Ini::RandomizerSlotsMax[8] = {
	"RandomizerSlot1Max",
	"RandomizerSlot2Max",
	"RandomizerSlot3Max",
	"RandomizerSlot4Max",
	"RandomizerSlot5Max",
	"RandomizerSlot6Max",
	"RandomizerSlot7Max",
	"RandomizerSlot8Max",
};
const char* wol_Misc_Ini::EnableRealtimeRandomize = "EnableRealtimeRandomize";

struct RandomizerSlots {
	int min;
	int max;
};

static RandomizerSlots g_RandomizerSlots[8];
static bool g_realtimeRandomize = false;

#define RANDMIDIVELWND_ID "WolRandMidiVelWnd"
static SNM_WindowManager<RandomMidiVelWnd> g_RandMidiVelWndMgr(RANDMIDIVELWND_ID);

bool RandomizeSelectedMidiVelocities(int min, int max)
{
	if (MediaItem_Take* take = MIDIEditor_GetTake(MIDIEditor_GetActive()))
	{
		if (min > 0 && max < 128)
		{
			if (min < max)
			{
				int i = -1;
				while ((i = MIDI_EnumSelNotes(take, i)) != -1)
				{
					int vel = min + (int)(g_MTRand.rand()*(max - min) + 0.5);
					MIDI_SetNote(take, i, NULL, NULL, NULL, NULL, NULL, NULL, &vel);
				}
			}
			else
			{
				int i = -1;
				while ((i = MIDI_EnumSelNotes(take, i)) != -1)
					MIDI_SetNote(take, i, NULL, NULL, NULL, NULL, NULL, NULL, &min);
			}
			return true;
		}
	}
	return false;
}

void OnCommandCallback_RandMidiVelWnd(int cmd, int* kn1, int* kn2)
{
	static string undoDesc = "";
	switch (cmd)
	{
	case (UserInputAndSlotsEditorWnd::CMD_USERANSWER + IDOK):
	{
		if (RandomizeSelectedMidiVelocities(*kn1, *kn2))
			Undo_OnStateChangeEx2(NULL, undoDesc.c_str(), UNDO_STATE_ALL, -1);
		break;
	}
	case UserInputAndSlotsEditorWnd::CMD_CLOSE:
	{
		undoDesc.clear();
		break;
	}
	case CMD_SETUNDOSTR:
	{
		undoDesc = (const char*)kn1;
		break;
	}
	case CMD_RT_RANDMIDIVEL:
	{
		g_realtimeRandomize = !g_realtimeRandomize;
		SaveIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.EnableRealtimeRandomize, g_realtimeRandomize);
		if (RandomMidiVelWnd* wnd = g_RandMidiVelWndMgr.Get())
		{
			wnd->EnableRealtimeNotify(g_realtimeRandomize);
			wnd->SetOptionState(CMD_RT_RANDMIDIVEL, NULL, &g_realtimeRandomize);
		}
		break;
	}
	case UserInputAndSlotsEditorWnd::CMD_RT_KNOB1:
	case UserInputAndSlotsEditorWnd::CMD_RT_KNOB2:
	{
		if (RandomizeSelectedMidiVelocities(*kn1, *kn2))
			Undo_OnStateChangeEx2(NULL, undoDesc.c_str(), UNDO_STATE_ALL, -1);
		break;
	}
	default:
	{
		if (cmd < 8)
		{
			*kn1 = g_RandomizerSlots[cmd/* - UserInputAndSlotsEditorWnd::CMD_LOAD*/].min;
			*kn2 = g_RandomizerSlots[cmd].max;
			if (g_realtimeRandomize && RandomizeSelectedMidiVelocities(*kn1, *kn2))
				Undo_OnStateChangeEx2(NULL, undoDesc.c_str(), UNDO_STATE_ALL, -1);
		}
		else if (cmd < 16 && cmd > 7)
		{
			SaveIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.RandomizerSlotsMin[cmd - UserInputAndSlotsEditorWnd::CMD_SAVE], *kn1);
			SaveIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.RandomizerSlotsMax[cmd - UserInputAndSlotsEditorWnd::CMD_SAVE], *kn2);
			g_RandomizerSlots[cmd - UserInputAndSlotsEditorWnd::CMD_SAVE].min = *kn1;
			g_RandomizerSlots[cmd - UserInputAndSlotsEditorWnd::CMD_SAVE].max = *kn2;
		}
		break;
	}
	}
}

RandomMidiVelWnd::RandomMidiVelWnd()
	: UserInputAndSlotsEditorWnd(__LOCALIZE("SWS/wol-spk77 - Randomize midi velocities tool", "sws_DLG_182a"), __LOCALIZE("Random midi velocities tool", "sws_DLG_182a"), RANDMIDIVELWND_ID, SWSGetCommandID(RandomizeSelectedMidiVelocitiesTool))
{
	SetupTwoKnobs();
	SetupKnob1(1, 127, 64, 1, 12.0f, __LOCALIZE("Min value:", "sws_DLG_182a"), __LOCALIZE("", "sws_DLG_182a"), __LOCALIZE("1", "sws_DLG_182a"));
	SetupKnob2(1, 127, 64, 127, 12.0f, __LOCALIZE("Max value:", "sws_DLG_182a"), __LOCALIZE("", "sws_DLG_182a"), __LOCALIZE("1", "sws_DLG_182a"));
	SetupOnCommandCallback(OnCommandCallback_RandMidiVelWnd);
	SetupOKText(__LOCALIZE("Randomize", "sws_DLG_182a"));
	
	SetupAddOption(CMD_RT_RANDMIDIVEL, true, g_realtimeRandomize, __LOCALIZE("Enable real time randomizing", "sws_DLG_182a"));

	EnableRealtimeNotify(g_realtimeRandomize);
}

void RandomizeSelectedMidiVelocitiesTool(COMMAND_T* ct)
{
	if ((int)ct->user == 0)
	{
		if (RandomMidiVelWnd* w = g_RandMidiVelWndMgr.Create())
		{
			OnCommandCallback_RandMidiVelWnd(CMD_SETUNDOSTR, (int*)SWS_CMD_SHORTNAME(ct), NULL);
			w->Show(true, true);
		}
	}
	else
	{
		if (RandomizeSelectedMidiVelocities(g_RandomizerSlots[(int)ct->user - 1].min, g_RandomizerSlots[(int)ct->user - 1].max))
			Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);
	}
}

int IsRandomizeSelectedMidiVelocitiesOpen(COMMAND_T* ct)
{
	if ((int)ct->user == 0)
		if (RandomMidiVelWnd* w = g_RandMidiVelWndMgr.Get())
			return w->IsValidWindow();
	return 0;
}

//---------//
#define CMD_RT_SELMIDINOTESBYVELINRANGE UserInputAndSlotsEditorWnd::CMD_OPTION_MIN
#define CMD_ADDNOTESTOSEL CMD_RT_SELMIDINOTESBYVELINRANGE + 1
#define CMD_ADDNOTESTONEWSEL CMD_ADDNOTESTOSEL + 1

const char* wol_Misc_Ini::NoteVelocitiesSlotsMin[8] = {
	"NoteVelocitiesSlot1Min",
	"NoteVelocitiesSlot2Min",
	"NoteVelocitiesSlot3Min",
	"NoteVelocitiesSlot4Min",
	"NoteVelocitiesSlot5Min",
	"NoteVelocitiesSlot6Min",
	"NoteVelocitiesSlot7Min",
	"NoteVelocitiesSlot8Min",
};
const char* wol_Misc_Ini::NoteVelocitiesSlotsMax[8] = {
	"NoteVelocitiesSlot1Max",
	"NoteVelocitiesSlot2Max",
	"NoteVelocitiesSlot3Max",
	"NoteVelocitiesSlot4Max",
	"NoteVelocitiesSlot5Max",
	"NoteVelocitiesSlot6Max",
	"NoteVelocitiesSlot7Max",
	"NoteVelocitiesSlot8Max",
};
const char* wol_Misc_Ini::EnableRealtimeNoteSelectionByVelInRange = "EnableRealtimeNoteSelectionByVelInRange";
const char* wol_Misc_Ini::AddSelectedNotesByVelInRangeToSel = "AddSelectedNotesByVelInRangeToSel";

struct MidiVelocitiesSlots {
	int min;
	int max;
};

static MidiVelocitiesSlots g_MidiVelocitiesSlots[8];
static bool g_realtimeNoteSelByVelInRange = false;
static bool g_addNotesToSel = false;

#define SELMIDINOTESBYVELINRANGEWND_ID "WolSelMidiNotesByVelInRangeWnd"
static SNM_WindowManager<SelMidiNotesByVelInRangeWnd> g_SelMidiNotesByVelInRangeWndMgr(SELMIDINOTESBYVELINRANGEWND_ID);

static bool SelectMidiNotesByVelocityInRange(int min, int max, bool addToSelection)
{
	if (MediaItem_Take* take = MIDIEditor_GetTake(MIDIEditor_GetActive()))
	{
		if (min > 0 && max < 128)
		{
			int cnt = 0;
			MIDI_CountEvts(take, &cnt, NULL, NULL);
			for (int i = 0; i < cnt; ++i)
			{
				bool sel = false;
				int vel = 0;
				MIDI_GetNote(take, i, &sel, NULL, NULL, NULL, NULL, NULL, &vel);
				if (addToSelection && sel)
					continue;

				sel = false;
				if (min < max)
				{
					if (vel >= min && vel <= max)
						sel = true;
				}
				else
				{
					if (vel == min)
						sel = true;
				}
				MIDI_SetNote(take, i, &sel, NULL, NULL, NULL, NULL, NULL, NULL);
			}
			return true;
		}
	}
	return false;
}

void OnCommandCallback_SelMidiNotesByVelInRangeWnd(int cmd, int* kn1, int* kn2)
{
	static string undoDesc = "";
	switch (cmd)
	{
	case (UserInputAndSlotsEditorWnd::CMD_USERANSWER + IDOK):
	{
		if (SelectMidiNotesByVelocityInRange(*kn1, *kn2, g_addNotesToSel))
			Undo_OnStateChangeEx2(NULL, undoDesc.c_str(), UNDO_STATE_ALL, -1);
		break;
	}
	case UserInputAndSlotsEditorWnd::CMD_CLOSE:
	{
		undoDesc.clear();
		break;
	}
	case CMD_SETUNDOSTR:
	{
		undoDesc = (const char*)kn1;
		break;
	}
	case CMD_RT_SELMIDINOTESBYVELINRANGE:
	{
		g_realtimeNoteSelByVelInRange = !g_realtimeNoteSelByVelInRange;
		SaveIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.EnableRealtimeNoteSelectionByVelInRange, g_realtimeNoteSelByVelInRange);
		if (SelMidiNotesByVelInRangeWnd* wnd = g_SelMidiNotesByVelInRangeWndMgr.Get())
		{
			wnd->EnableRealtimeNotify(g_realtimeNoteSelByVelInRange);
			wnd->SetOptionState(CMD_RT_SELMIDINOTESBYVELINRANGE, NULL, &g_realtimeNoteSelByVelInRange);
		}
		break;
	}
	case CMD_ADDNOTESTOSEL:
	case CMD_ADDNOTESTONEWSEL:
	{
		g_addNotesToSel = (cmd == CMD_ADDNOTESTOSEL);
		SaveIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.AddSelectedNotesByVelInRangeToSel, g_addNotesToSel);
		if (SelMidiNotesByVelInRangeWnd* wnd = g_SelMidiNotesByVelInRangeWndMgr.Get())
		{
			bool checked = g_addNotesToSel;
			wnd->SetOptionState(CMD_ADDNOTESTOSEL, NULL, &checked);
			checked = !checked;
			wnd->SetOptionState(CMD_ADDNOTESTONEWSEL, NULL, &checked);
		}
		break;
	}
	case UserInputAndSlotsEditorWnd::CMD_RT_KNOB1:
	case UserInputAndSlotsEditorWnd::CMD_RT_KNOB2:
	{
		if (SelectMidiNotesByVelocityInRange(*kn1, *kn2, g_addNotesToSel))
			Undo_OnStateChangeEx2(NULL, undoDesc.c_str(), UNDO_STATE_ALL, -1);
		break;
	}
	default:
	{
		if (cmd < 8)
		{
			*kn1 = g_MidiVelocitiesSlots[cmd/* - UserInputAndSlotsEditorWnd::CMD_LOAD*/].min;
			*kn2 = g_MidiVelocitiesSlots[cmd].max;
			if (g_realtimeNoteSelByVelInRange && SelectMidiNotesByVelocityInRange(*kn1, *kn2, g_addNotesToSel))
				Undo_OnStateChangeEx2(NULL, undoDesc.c_str(), UNDO_STATE_ALL, -1);
		}
		else if (cmd < 16 && cmd > 7)
		{
			SaveIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.NoteVelocitiesSlotsMin[cmd - UserInputAndSlotsEditorWnd::CMD_SAVE], *kn1);
			SaveIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.NoteVelocitiesSlotsMax[cmd - UserInputAndSlotsEditorWnd::CMD_SAVE], *kn2);
			g_MidiVelocitiesSlots[cmd - UserInputAndSlotsEditorWnd::CMD_SAVE].min = *kn1;
			g_MidiVelocitiesSlots[cmd - UserInputAndSlotsEditorWnd::CMD_SAVE].max = *kn2;
		}
		break;
	}
	}
}

SelMidiNotesByVelInRangeWnd::SelMidiNotesByVelInRangeWnd()
	: UserInputAndSlotsEditorWnd(__LOCALIZE("SWS/wol-spk77 - Select midi notes by velocities in range tool", "sws_DLG_182b"), __LOCALIZE("Select midi notes by velocities in range tool", "sws_DLG_182b"), SELMIDINOTESBYVELINRANGEWND_ID, SWSGetCommandID(SelectMidiNotesByVelocitiesInRangeTool))
{
	SetupTwoKnobs();
	SetupKnob1(1, 127, 64, 1, 12.0f, __LOCALIZE("Min velocity:", "sws_DLG_182b"), __LOCALIZE("", "sws_DLG_182b"), __LOCALIZE("1", "sws_DLG_182b"));
	SetupKnob2(1, 127, 64, 127, 12.0f, __LOCALIZE("Max velocity:", "sws_DLG_182b"), __LOCALIZE("", "sws_DLG_182b"), __LOCALIZE("1", "sws_DLG_182b"));
	SetupOnCommandCallback(OnCommandCallback_SelMidiNotesByVelInRangeWnd);
	SetupOKText(__LOCALIZE("Select", "sws_DLG_182b"));
	
	SetupAddOption(CMD_RT_SELMIDINOTESBYVELINRANGE, true, g_realtimeNoteSelByVelInRange, __LOCALIZE("Enable real time note selection", "sws_DLG_182b"));
	SetupAddOption(UserInputAndSlotsEditorWnd::CMD_OPTION_MAX, true, false, SWS_SEPARATOR);
	SetupAddOption(CMD_ADDNOTESTOSEL, true, g_addNotesToSel, __LOCALIZE("Add notes to selection", "sws_DLG_182b"));
	SetupAddOption(CMD_ADDNOTESTONEWSEL, true, !g_addNotesToSel, __LOCALIZE("Clear selection", "sws_DLG_182b"));

	EnableRealtimeNotify(g_realtimeNoteSelByVelInRange);
}

void SelectMidiNotesByVelocitiesInRangeTool(COMMAND_T* ct)
{
	if ((int)ct->user == 0)
	{
		if (SelMidiNotesByVelInRangeWnd* w = g_SelMidiNotesByVelInRangeWndMgr.Create())
		{
			OnCommandCallback_SelMidiNotesByVelInRangeWnd(CMD_SETUNDOSTR, (int*)SWS_CMD_SHORTNAME(ct), NULL);
			w->Show(true, true);
		}
	}
	else
	{
		if (SelectMidiNotesByVelocityInRange(g_MidiVelocitiesSlots[(int)ct->user - ((int)ct->user < 9 ? 1 : 9)].min, g_MidiVelocitiesSlots[(int)ct->user - ((int)ct->user < 9 ? 1 : 9)].max, (int)ct->user < 9 ? false : true))
			Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);
	}
}

int IsSelectMidiNotesByVelocitiesInRangeOpen(COMMAND_T* ct)
{
	if ((int)ct->user == 0)
		if (SelMidiNotesByVelInRangeWnd* w = g_SelMidiNotesByVelInRangeWndMgr.Get())
			return w->IsValidWindow();
	return 0;
}

//---------//
#define CMD_RT_SELRANDMIDINOTESPERC UserInputAndSlotsEditorWnd::CMD_OPTION_MIN
#define CMD_ADDTOSEL CMD_RT_SELRANDMIDINOTESPERC + 1
#define CMD_CLEARSEL CMD_RT_SELRANDMIDINOTESPERC + 2
#define CMD_AMONGSELNOTES CMD_RT_SELRANDMIDINOTESPERC + 3

const char* wol_Misc_Ini::SelRandMidiNotesPercent[8] = {
	"SelRandMidiNotesPercentSlot1",
	"SelRandMidiNotesPercentSlot2",
	"SelRandMidiNotesPercentSlot3",
	"SelRandMidiNotesPercentSlot4",
	"SelRandMidiNotesPercentSlot5",
	"SelRandMidiNotesPercentSlot6",
	"SelRandMidiNotesPercentSlot7",
	"SelRandMidiNotesPercentSlot8",
};
const char* wol_Misc_Ini::EnableRealtimeRandomNoteSelectionPerc = "EnableRealtimeRandomNoteSelectionPerc";
const char* wol_Misc_Ini::SelectRandomMidiNotesPercType = "SelectRandomMidiNotesPercType";

static UINT g_SelRandMidiNotesPercent[8];
static bool g_realtimeRandomNoteSelPerc = false;
static int g_selType = 0; //0 add to selection, 1 clear selection, 2 among selected notes

#define SELRANDMIDINOTESPERCWND_ID "WolSelRandMidiNotesPercWnd"
static SNM_WindowManager<SelRandMidiNotesPercWnd> g_SelRandMidiNotesPercWndMgr(SELRANDMIDINOTESPERCWND_ID);

static bool SelectRandomMidiNotesPercent(UINT perc, bool addToSelection, bool amongSelNotes)
{
	if (MediaItem_Take* take = MIDIEditor_GetTake(MIDIEditor_GetActive()))
	{
		if (perc >= 0 && perc < 101)
		{
			int cnt = 0;
			MIDI_CountEvts(take, &cnt, NULL, NULL);
			for (int i = 0; i < cnt; ++i)
			{
				bool sel = false;
				MIDI_GetNote(take, i, &sel, NULL, NULL, NULL, NULL, NULL, NULL);
				if ((addToSelection && sel && !amongSelNotes) || (amongSelNotes && !sel))
					continue;

				sel = (g_MTRand.randInt() % 100) < perc;
				MIDI_SetNote(take, i, &sel, NULL, NULL, NULL, NULL, NULL, NULL);
			}
			return true;
		}
	}
	return false;
}

void OnCommandCallback_SelRandMidiNotesPercWnd(int cmd, int* kn1, int* kn2)
{
	static string undoDesc;
	switch (cmd)
	{
	case (UserInputAndSlotsEditorWnd::CMD_USERANSWER + IDOK) :
	{
		if (SelectRandomMidiNotesPercent((UINT)*kn1, (g_selType == 0 ? true : false), (g_selType == 2 ? true : false)))
			Undo_OnStateChangeEx2(NULL, undoDesc.c_str(), UNDO_STATE_ALL, -1);
		break;
	}
	case UserInputAndSlotsEditorWnd::CMD_CLOSE:
	{
		undoDesc.clear();
		break;
	}
	case CMD_SETUNDOSTR:
	{
		undoDesc = (const char*)kn1;
		break;
	}
	case CMD_RT_SELRANDMIDINOTESPERC:
	{
		g_realtimeRandomNoteSelPerc = !g_realtimeRandomNoteSelPerc;
		SaveIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.EnableRealtimeRandomNoteSelectionPerc, g_realtimeRandomNoteSelPerc);
		if (SelRandMidiNotesPercWnd* wnd = g_SelRandMidiNotesPercWndMgr.Get())
		{
			wnd->EnableRealtimeNotify(g_realtimeRandomNoteSelPerc);
			wnd->SetOptionState(CMD_RT_SELRANDMIDINOTESPERC, NULL, &g_realtimeRandomNoteSelPerc);
		}
		break;
	}
	case CMD_ADDTOSEL:
	case CMD_CLEARSEL:
	case CMD_AMONGSELNOTES:
	{
		int _cmd = CMD_ADDTOSEL;
		int res = cmd - _cmd;
		g_selType = res/*cmd - CMD_ADDTOSEL*/;
		SaveIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.SelectRandomMidiNotesPercType, g_selType);
		if (SelRandMidiNotesPercWnd* wnd = g_SelRandMidiNotesPercWndMgr.Get())
		{
			bool checked = (g_selType == 0);
			wnd->SetOptionState(CMD_ADDTOSEL, NULL, &checked);
			checked = (g_selType == 1);
			wnd->SetOptionState(CMD_CLEARSEL, NULL, &checked);
			checked = (g_selType == 2);
			wnd->SetOptionState(CMD_AMONGSELNOTES, NULL, &checked);
		}
		break;
	}
	case UserInputAndSlotsEditorWnd::CMD_RT_KNOB1:
	case UserInputAndSlotsEditorWnd::CMD_RT_KNOB2:
	{
		if (SelectRandomMidiNotesPercent((UINT)*kn1, (g_selType == 0 ? true : false), (g_selType == 2 ? true : false)))
			Undo_OnStateChangeEx2(NULL, undoDesc.c_str(), UNDO_STATE_ALL, -1);
		break;
	}
	default:
	{
		if (cmd < 8)
		{
			*kn1 = g_SelRandMidiNotesPercent[cmd/* - UserInputAndSlotsEditorWnd::CMD_LOAD*/];
			if (g_realtimeRandomNoteSelPerc && SelectRandomMidiNotesPercent((UINT)*kn1, (g_selType == 0 ? true : false), (g_selType == 2 ? true : false)))
				Undo_OnStateChangeEx2(NULL, undoDesc.c_str(), UNDO_STATE_ALL, -1);
		}
		else if (cmd < 16 && cmd > 7)
		{
			SaveIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.SelRandMidiNotesPercent[cmd - UserInputAndSlotsEditorWnd::CMD_SAVE], *kn1);
			g_SelRandMidiNotesPercent[cmd - UserInputAndSlotsEditorWnd::CMD_SAVE] = (UINT)*kn1;
		}
		break;
	}
	}
}

SelRandMidiNotesPercWnd::SelRandMidiNotesPercWnd()
	: UserInputAndSlotsEditorWnd(__LOCALIZE("SWS/wol-spk77 - Select random midi notes tool", "sws_DLG_182c"), __LOCALIZE("Select random midi notes tool", "sws_DLG_182c"), SELRANDMIDINOTESPERCWND_ID, SWSGetCommandID(SelectMidiNotesByVelocitiesInRangeTool))
{
	SetupKnob1(1, 100, 50, 50, 12.0f, __LOCALIZE("Percent:", "sws_DLG_182c"), __LOCALIZE("%", "sws_DLG_182c"), __LOCALIZE("", "sws_DLG_182c"));
	SetupOnCommandCallback(OnCommandCallback_SelRandMidiNotesPercWnd);
	SetupOKText(__LOCALIZE("Select", "sws_DLG_182c"));
	//SetupQuestion(__LOCALIZE("Options:\n 'Yes' to add to selection\n 'No' for new selection\n 'Cancel' to select among selected notes.", "sws_DLG_182c"), __LOCALIZE("SWS/wol-spk77 - Info", "sws_DLG_182c"), MB_YESNOCANCEL);

	SetupAddOption(CMD_RT_SELRANDMIDINOTESPERC, true, g_realtimeRandomNoteSelPerc, __LOCALIZE("Enable real time selection", "sws_DLG_182c"));
	SetupAddOption(UserInputAndSlotsEditorWnd::CMD_OPTION_MAX, true, false, SWS_SEPARATOR);
	SetupAddOption(CMD_ADDTOSEL, true, g_selType == 0, __LOCALIZE("Add notes to selection", "sws_DLG_182c"));
	SetupAddOption(CMD_CLEARSEL, true, g_selType == 1, __LOCALIZE("Clear selection", "sws_DLG_182c"));
	SetupAddOption(CMD_AMONGSELNOTES, true, g_selType == 2, __LOCALIZE("Select notes among selected notes", "sws_DLG_182c"));

	EnableRealtimeNotify(g_realtimeRandomNoteSelPerc);
}

void SelectRandomMidiNotesTool(COMMAND_T* ct)
{
	if ((int)ct->user == 0)
	{
		if (SelRandMidiNotesPercWnd* w = g_SelRandMidiNotesPercWndMgr.Create())
		{
			OnCommandCallback_SelRandMidiNotesPercWnd(CMD_SETUNDOSTR, (int*)SWS_CMD_SHORTNAME(ct), NULL);
			w->Show(true, true);
		}
	}
	else
	{
		if (SelectRandomMidiNotesPercent(g_SelRandMidiNotesPercent[(int)ct->user - ((int)ct->user < 9 ? 1 : ((int)ct->user < 17 ? 9 : 17))], ((int)ct->user < 9 || (int)ct->user > 16) ? false : true, (int)ct->user > 16))
			Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(ct), UNDO_STATE_ALL, -1);
	}
}

int IsSelectRandomMidiNotesOpen(COMMAND_T* ct)
{
	if ((int)ct->user == 0)
		if (SelRandMidiNotesPercWnd* w = g_SelRandMidiNotesPercWndMgr.Get())
			return w->IsValidWindow();
	return 0;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// Mixer
///////////////////////////////////////////////////////////////////////////////////////////////////
void ScrollMixer(COMMAND_T* ct)
{
	MediaTrack* tr = GetMixerScroll();
	if (tr)
	{
		int count = CountTracks(NULL);
		int i = (int)GetMediaTrackInfo_Value(tr, "IP_TRACKNUMBER") - 1;
		MediaTrack* nTr = NULL;
		if ((int)ct->user == 0)
		{
			for (int k = 1; k <= i; ++k)
			{
				if (IsTrackVisible(GetTrack(NULL, i - k), true))
				{
					nTr = GetTrack(NULL, i - k);
					break;
				}
			}
		}
		else
		{
			for (int k = 1; k < count - i; ++k)
			{
				if (IsTrackVisible(GetTrack(NULL, i + k), true))
				{
					nTr = GetTrack(NULL, i + k);
					break;
				}
			}
		}
		if (nTr && nTr != tr)
			SetMixerScroll(nTr);
	}
}



///////////////////////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////////////////////
void wol_MiscInit()
{
	for (int i = 0; i < 8; ++i)
	{
		g_RandomizerSlots[i].min = GetIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.RandomizerSlotsMin[i], 1);
		g_RandomizerSlots[i].max = GetIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.RandomizerSlotsMax[i], 127);
		g_MidiVelocitiesSlots[i].min = GetIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.NoteVelocitiesSlotsMin[i], 1);
		g_MidiVelocitiesSlots[i].max = GetIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.NoteVelocitiesSlotsMax[i], 127);
		g_SelRandMidiNotesPercent[i] = GetIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.SelRandMidiNotesPercent[i], 100);
	}

	g_realtimeRandomize = GetIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.EnableRealtimeRandomize, false);
	g_realtimeNoteSelByVelInRange = GetIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.EnableRealtimeNoteSelectionByVelInRange, false);
	g_addNotesToSel = GetIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.AddSelectedNotesByVelInRangeToSel, true);
	g_realtimeRandomNoteSelPerc = GetIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.EnableRealtimeRandomNoteSelectionPerc, false);
	g_selType = GetIniSettings(wol_Misc_Ini.Section, wol_Misc_Ini.SelectRandomMidiNotesPercType, 0);

	g_RandMidiVelWndMgr.Init();
	g_SelMidiNotesByVelInRangeWndMgr.Init();
	g_SelRandMidiNotesPercWndMgr.Init();
}

void wol_MiscExit()
{
	g_RandMidiVelWndMgr.Delete();
	g_SelMidiNotesByVelInRangeWndMgr.Delete();
	g_SelRandMidiNotesPercWndMgr.Delete();
}