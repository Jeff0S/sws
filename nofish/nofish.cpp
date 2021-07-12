/******************************************************************************
/ nofish.cpp
/
/ Copyright (c) 2016 nofish
/ http://forum.cockos.com/member.php?u=6870
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
#include "nofish.h"

#include "../Breeder/BR_ContinuousActions.h"
#include "../Breeder/BR_Util.h"
#include "../Breeder/BR_ReaScript.h" // BR_GetMouseCursorContext(), BR_ItemAtMouseCursor()
#include "../Utility/configvar.h"
#include "../Misc/Adam.h"

/******************************************************************************
* Globals (file-scope)                                                        *
******************************************************************************/
namespace {
	int gf_NFObeyTrackHeightLock;
	int gf_NFTogglePlayStopPlayPause;
}

//////////////////////////////////////////////////////////////////
//                                                              //
// Bypass FX (except VSTi) for selected tracks                  //
//                                                              //
//////////////////////////////////////////////////////////////////

void BypassFXexceptVSTiForSelTracks(COMMAND_T* ct)
{
	Undo_BeginBlock();
	WDL_TypedBuf<MediaTrack*> selTracks;
	SWS_GetSelectedTracks(&selTracks, true); // incl. master
	for (int i = 0; i < selTracks.GetSize(); i++) {
		MediaTrack* track = selTracks.Get()[i];
		int vsti_index = TrackFX_GetInstrument(track); // Get position of first VSTi on current track
		for (int j = 0; j < TrackFX_GetCount(track); j++) { // loop through FX on current track
			if (j != vsti_index) { // if current FX is not a VSTi...
				TrackFX_SetEnabled(track, j, 0); // ...bypass current FX
			}
		}
	}
	Undo_EndBlock(SWS_CMD_SHORTNAME(ct), 0);
}


//////////////////////////////////////////////////////////////////
//                                                              //
// #514 Toggle triplet and dotted grid in MIDI editor           //
//                                                              //
//////////////////////////////////////////////////////////////////

int Main_IsMIDIGridTriplet(COMMAND_T* = NULL)
{
	return GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41004); // Grid: Set grid type to triplet
}


int Main_IsMIDIGridDotted(COMMAND_T* = NULL)
{
	return GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41005); // Grid: Set grid type to dotted
}

int Main_IsMIDIGridSwing(COMMAND_T* = NULL)
{
	// BR: It seems REAPER doesn't report toggle state for swing grid so instead check all other three grid types
	return GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41005) == 0 // Grid: Set grid type to dotted
	    && GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41004) == 0 // Grid: Set grid type to triplet
	    && GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41003) == 0 // Grid: Set grid type to straight
	;
}

void Main_NFToggleDottedMIDI(COMMAND_T* = NULL);

void Main_NFToggleTripletMIDI(COMMAND_T* = NULL)
{
	if (Main_IsMIDIGridTriplet()) {
		// set to straight
		HWND midiEditor;
		midiEditor = MIDIEditor_GetActive();
		MIDIEditor_OnCommand(midiEditor, 41003); // null ptr. check is handled by REAPER
	}

	else {
		// set to triplet
		HWND midiEditor;
		midiEditor = MIDIEditor_GetActive();
		MIDIEditor_OnCommand(midiEditor, 41004);
	}

	UpdateMIDIGridToolbar();
}

void Main_NFToggleDottedMIDI(COMMAND_T*)
{
	if (Main_IsMIDIGridDotted()) {
		// set to straight
		HWND midiEditor;
		midiEditor = MIDIEditor_GetActive();
		MIDIEditor_OnCommand(midiEditor, 41003);
	}
	else {
		// set to dotted
		HWND midiEditor;
		midiEditor = MIDIEditor_GetActive();
		MIDIEditor_OnCommand(midiEditor, 41005);
	}

	UpdateMIDIGridToolbar();
}

void Main_NFToggleSwingMIDI(COMMAND_T*)
{
	if (Main_IsMIDIGridSwing()) {
		// set to straight
		HWND midiEditor;
		midiEditor = MIDIEditor_GetActive();
		MIDIEditor_OnCommand(midiEditor, 41003);
	}
	else {
		// set to swing
		HWND midiEditor;
		midiEditor = MIDIEditor_GetActive();
		MIDIEditor_OnCommand(midiEditor, 41006);

		// BR: REAPER doesn't obey 'Grid: Use the same grid division in arrange view and MIDI editor' when setting swing grid via action so make sure to set it here manually
		if (GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41022)) // Grid: Use the same grid division in arrange view and MIDI editor
			GetSetProjectGrid(NULL, true, NULL, &g_i1, NULL);
	}

	UpdateMIDIGridToolbar();
}



void UpdateMIDIGridToolbar()
{
	UpdateGridToolbar(); // to prevent duplication and possible omissions in the future in case of updates
}

// pass through from ME to Main
void ME_NFToggleTripletMIDI(COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd) {
	Main_NFToggleTripletMIDI(_ct);
}

void ME_NFToggleDottedMIDI(COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd) {
	Main_NFToggleDottedMIDI(_ct);
}
void ME_NFToggleSwingMIDI(COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd) {
	Main_NFToggleSwingMIDI(_ct);
}
// /#514


//////////////////////////////////////////////////////////////////
//                                                              //
// Disable / Enable multichannel metering                       //
//                                                              //
//////////////////////////////////////////////////////////////////

// ct->user == 0: all tracks, == 1: sel. tracks
void EnableDisableMultichannelMetering(COMMAND_T* ct, bool enable)
{
	WDL_TypedBuf<MediaTrack*> selTracks;
	SWS_GetSelectedTracks(&selTracks, false);

	PreventUIRefresh(1);
	Undo_BeginBlock();

	if (ct->user == 0)
	{
		for (int i = 1; i <= GetNumTracks(); ++i) {
			MediaTrack* track = CSurf_TrackFromID(i, false);
			SNM_ChunkParserPatcher p(track);
			int VUlineFound = p.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "VU", 0, -1, NULL, NULL, "TRACKHEIGHT");
			if ((enable && VUlineFound == 0) || (!enable && VUlineFound > 0)) {
				SetOnlyTrackSelected(track);
				Main_OnCommand(41726, 0); // Track: Toggle full multichannel metering
			}
		}
	}
	else if (ct->user == 1)
	{
		for (int i = 0; i < selTracks.GetSize(); ++i) {
			MediaTrack* track = selTracks.Get()[i];
			SNM_ChunkParserPatcher p(track);
			int VUlineFound = p.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "VU", 0, -1, NULL, NULL, "TRACKHEIGHT");
			if ((enable && VUlineFound == 0) || (!enable && VUlineFound > 0)) {
				SetOnlyTrackSelected(track);
				Main_OnCommand(41726, 0); // Track: Toggle full multichannel metering
			}
		}
	}

	Undo_EndBlock(SWS_CMD_SHORTNAME(ct), 1);

	// restore initial track selection
	ClearSelected();
	int iSel = 1;
	for (int i = 0; i < selTracks.GetSize(); ++i) {
		GetSetMediaTrackInfo(selTracks.Get()[i], "I_SELECTED", &iSel);
	}

	PreventUIRefresh(-1);
}

void DisableMultichannelMetering(COMMAND_T* ct)
{
	EnableDisableMultichannelMetering(ct, false);
}

void EnableMultichannelMetering(COMMAND_T* ct)
{
	EnableDisableMultichannelMetering(ct, true);
}


//////////////////////////////////////////////////////////////////
//                                                              //
// Eraser tool (continuous action)                              //
//                                                              //
//////////////////////////////////////////////////////////////////

char g_EraserToolCurMouseMod[32];
MediaItem* g_EraserToolLastSelItem = NULL;
// int g_EraserToolCurRelEdgesMode;

// called on start with init = true and on shortcut release with init = false. Return false to abort init.
// ct->user = 0: no snap, = 1: obey snap
static bool EraserToolInit(COMMAND_T* ct, bool init)
{
	if (init) {
		// get the currently assigned mm for Media Item - left drag, store it and restore after this continious action is performed
		GetMouseModifier("MM_CTX_ITEM", 0, g_EraserToolCurMouseMod, sizeof(g_EraserToolCurMouseMod));


		if (ct->user == 1)
			SetMouseModifier("MM_CTX_ITEM", 0, "28"); // Marquee sel. items and time = 28
		else if (ct->user == 0)
			SetMouseModifier("MM_CTX_ITEM", 0, "29"); // Marquee sel. items and time ignoring snap = 29

		// temp. disable Prefs->"Editing Behaviour -> If no items are selected..."
		// only needed when doing immediate erase which is disabled currently
		// GetConfig("relativeedges", g_EraserToolCurRelEdgesMode);
		// SetConfig("relativeedges", SetBit(g_EraserToolCurRelEdgesMode, 8));

		Undo_BeginBlock();
		Main_OnCommand(40635, 0); // remove time sel.
		Main_OnCommand(40289, 0); // unsel. all items
	}

	if (!init) {

		Main_OnCommand(40307, 0); // cut sel area of sel items
		Main_OnCommand(40635, 0); // remove time sel.
		Main_OnCommand(40289, 0); // unsel. all items
		Undo_EndBlock(SWS_CMD_SHORTNAME(ct), 0);

		g_EraserToolLastSelItem = NULL;

		// set mm back to original when shortcut is released
		SetMouseModifier("MM_CTX_ITEM", 0, g_EraserToolCurMouseMod);

		// set Prefs -> Editing Behaviour -> If no items are selected... back to original value
		// SetConfig("relativeedges", g_EraserToolCurRelEdgesMode);
	}

	return true;
}

static void DoEraserTool(COMMAND_T* ct)
{
	// BR_GetMouseCursorContext(windowOut, sizeof(windowOut), segmentOut, sizeof(segmentOut), detailsOut, sizeof(detailsOut));
	// MediaItem* item = BR_GetMouseCursorContext_Item();

	SetCursor(GetSwsMouseCursor(CURSOR_ERASER)); // set custom cursor during action

	// select items on hover
	MediaItem* item = BR_ItemAtMouseCursor(NULL);
	if (item != NULL && item != g_EraserToolLastSelItem) {
		if (!IsMediaItemSelected(item)) {
			SetMediaItemSelected(item, true);
			g_EraserToolLastSelItem = item;
			UpdateArrange();
		}
	}

	// immediate erase doesn't work well with Ripple edit
	// https://forum.cockos.com/showthread.php?t=208712
	// Main_OnCommand(40307, 0); // cut sel area of sel items
}

/*
// get a custom mouse cursor for the action
static HCURSOR EraserToolCursor(COMMAND_T* ct, int window)
{
	// if MAIN_RULER isn't added the custom mouse cursor doesn't appear immediately when pressing the assigned shortcut
	// not sure why
	if (window == BR_ContinuousAction::MAIN_ARRANGE || window == BR_ContinuousAction::MAIN_RULER)
		return GetSwsMouseCursor(CURSOR_ERASER);
	else
		return NULL;
}
*/

// called when shortcut is released, return undo flag to create undo point (or 0 for no undo point). If NULL, no undo point will get created
static int EraserToolUndo(COMMAND_T* ct)
{
	return 0; // undo point is created in EraserToolInit() already
}

void CycleMIDIRecordingModes(COMMAND_T* ct)
{
	WDL_TypedBuf<MediaTrack*> selTracks;
	SWS_GetSelectedTracks(&selTracks, false);

	for (int i = 0; i < selTracks.GetSize(); i++) {
		MediaTrack* track = selTracks.Get()[i];
		int recMode = (int)GetMediaTrackInfo_Value(track, "I_RECMODE");
		int nextRecMode;

		switch (recMode) {
		case 0: // input (audio or MIDI)
			nextRecMode = 4;
			break;
		case 4: // MIDI output
			nextRecMode = 7;
			break;
		case 7: // MIDI overdub
			nextRecMode = 8;
			break;
		case 8: // MIDI replace
			nextRecMode = 9;
			break;
		case 9: // MIDI touch-replace
			nextRecMode = 16;
			break;
		case 16: // MIDI latch-replace
			nextRecMode = 0;
			break;
		default: // when not in one of the MIDI rec. modes set to 'input (audio or MIDI)'
			nextRecMode = 0;
		}
		SetMediaTrackInfo_Value(track, "I_RECMODE", nextRecMode);
	}
}

void ME_CycleMIDIRecordingModes(COMMAND_T* ct, int val, int valhw, int relmode, HWND hwnd)
{
	CycleMIDIRecordingModes(NULL);
}

void CycleTrackAutomationModes(COMMAND_T* ct)
{
	WDL_TypedBuf<MediaTrack*> selTracks;
	SWS_GetSelectedTracks(&selTracks, false);

	for (int i = 0; i < selTracks.GetSize(); i++) {
		MediaTrack* track = selTracks.Get()[i];
		int autoMode = GetTrackAutomationMode(track);
		if (++autoMode > 5) autoMode = 0;
		SetTrackAutomationMode(track, autoMode);
	}
}

// toggle Limit apply FX / render stems to realtime, #1065
int IsRenderSpeedRealtime(COMMAND_T* = nullptr)
{
	return GetBit(*ConfigVar<int>("workrender"), 3);
}

void ToggleRenderSpeedRealtimeNotLim(COMMAND_T* = nullptr)
{
	const char* configStr = "workrender";
	ConfigVar<int> option(configStr);
	if (!option) return;

	*option = ToggleBit(*option, 3);
	RefreshToolbar(0);

	char tmp[256];
	snprintf(tmp, sizeof(tmp), "%d", *option);
	WritePrivateProfileString("reaper", configStr, tmp, get_ini_file());
}

// toggle Xenakios track height actions and SWS vertical zoom actions obey track height lock, #966
static void ToggleObeyTrackHeightLock(COMMAND_T* = nullptr)
{
	gf_NFObeyTrackHeightLock = !gf_NFObeyTrackHeightLock;
	WritePrivateProfileString(SWS_INI, "NFObeyTrackHeightLock", gf_NFObeyTrackHeightLock ? "1" : "0", get_ini_file());
}

static int IsObeyTrackHeightLockEnabled(COMMAND_T* = nullptr)
{
	return gf_NFObeyTrackHeightLock;
}

bool NF_IsObeyTrackHeightLockEnabled()
{
	return IsObeyTrackHeightLockEnabled();
}

static void TogglePlayStopPlayPause(COMMAND_T* ct = nullptr)
{
	gf_NFTogglePlayStopPlayPause = !gf_NFTogglePlayStopPlayPause;
	WritePrivateProfileString(SWS_INI, "NFTogglePlayStopPlayPause", gf_NFTogglePlayStopPlayPause ? "1" : "0", get_ini_file());
}

int IsTogglePlayStopPlayPauseEnabled(COMMAND_T* _ct = nullptr) {
	return gf_NFTogglePlayStopPlayPause;
}

static void PlayStopPlayPause(COMMAND_T* ct = nullptr)
{
	if (gf_NFTogglePlayStopPlayPause)
		Main_OnCommand(40073, 0); // Transport: Play/pause
	else
		Main_OnCommand(40044, 0); // Transport: Play/stop
}


//////////////////////////////////////////////////////////////////
//                                                              //
// Register commands                                            //
//                                                              //
//////////////////////////////////////////////////////////////////

static COMMAND_T g_commandTable[] =
{
	//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions

	// Bypass FX(except VSTi) for selected tracks
	{ { DEFACCEL, "SWS/NF: Bypass FX (except VSTi) for selected tracks" }, "NF_BYPASS_FX_EXCEPT_VSTI_FOR_SEL_TRACKS", BypassFXexceptVSTiForSelTracks, NULL },

	// #514
	// don't register in Main
	// { { DEFACCEL, "SWS/NF: Toggle triplet grid" },	"NF_MAIN_TOGGLETRIPLET_MIDI",   Main_NFToggleTripletMIDI, NULL, 0, Main_IsMIDIGridTriplet },
	// { { DEFACCEL, "SWS/NF: Toggle dotted grid" },   "NF_MAIN_TOGGLEDOTTED_MIDI",    Main_NFToggleDottedMIDI, NULL, 0, Main_IsMIDIGridDotted },

	{ { DEFACCEL, "SWS/NF: Toggle triplet grid" }, "NF_ME_TOGGLETRIPLET" , NULL, NULL, 0, Main_IsMIDIGridTriplet, SECTION_MIDI_EDITOR, ME_NFToggleTripletMIDI },
	{ { DEFACCEL, "SWS/NF: Toggle dotted grid" }, "NF_ME_TOGGLEDOTTED" , NULL, NULL, 0, Main_IsMIDIGridDotted, SECTION_MIDI_EDITOR, ME_NFToggleDottedMIDI  },
	{ { DEFACCEL, "SWS/NF: Toggle swing grid" }, "NF_ME_TOGGLESWING" , NULL, NULL, 0, Main_IsMIDIGridSwing, SECTION_MIDI_EDITOR, ME_NFToggleSwingMIDI  },

	// Disable / Enable multichannel metering
	{ { DEFACCEL, "SWS/NF: Disable multichannel metering (all tracks)" }, "NF_DISABLE_MULTICHAN_MTR_ALL", DisableMultichannelMetering, NULL, 0 },
	{ { DEFACCEL, "SWS/NF: Disable multichannel metering (selected tracks)" }, "NF_DISABLE_MULTICHAN_MTR_SEL", DisableMultichannelMetering, NULL, 1 },
	{ { DEFACCEL, "SWS/NF: Enable multichannel metering (all tracks)" }, "NF_ENABLE_MULTICHAN_MTR_ALL", EnableMultichannelMetering, NULL, 0 },
	{ { DEFACCEL, "SWS/NF: Enable multichannel metering (selected tracks)" }, "NF_ENABLE_MULTICHAN_MTR_SEL", EnableMultichannelMetering, NULL, 1 },
	// #994, #995
	{ { DEFACCEL, "SWS/NF: Cycle through MIDI recording modes" }, "NF_CYCLE_MIDI_RECORD_MODES", CycleMIDIRecordingModes },
	// also register in ME section
	{ { DEFACCEL, "SWS/NF: Cycle through MIDI recording modes" }, "NF_ME_CYCLE_MIDI_RECORD_MODES", NULL, NULL, 0, NULL, SECTION_MIDI_EDITOR, ME_CycleMIDIRecordingModes },
	{ { DEFACCEL, "SWS/NF: Cycle through track automation modes" }, "NF_CYCLE_TRACK_AUTOMATION_MODES", CycleTrackAutomationModes },

	// toggle Limit apply FX/render stems to realtime
	{ { DEFACCEL, "SWS/NF: Toggle render speed (apply FX/render stems) realtime/not limited" }, "NF_TOGGLERENDERSPEED_RT_NL", ToggleRenderSpeedRealtimeNotLim, NULL, 0,  IsRenderSpeedRealtime},

	// toggle Xenakios track height actions and SWS vertical zoom actions obey track height lock
	{ { DEFACCEL, "SWS/NF: Toggle obey track height lock in vertical zoom and track height actions" }, "NF_TOGGLE_OBEY_TRACK_HEIGHT_LOCK", ToggleObeyTrackHeightLock, NULL, 0, IsObeyTrackHeightLockEnabled},

	{ { DEFACCEL, "SWS/NF: Toggle Play/stop (off) or Play/pause (on)" }, "NF_TOGGLE_PLAY_STOP_PLAY_PAUSE", TogglePlayStopPlayPause, NULL, 0, IsTogglePlayStopPlayPauseEnabled},
	{ { DEFACCEL, "SWS/NF: Play/stop or Play/pause (obey 'SWS/NF: Toggle Play/stop or Play/pause' toggle state)" }, "NF_PLAY_STOP_PLAY_PAUSE", PlayStopPlayPause, NULL, 0},

	//!WANT_LOCALIZE_1ST_STRING_END

	{ {}, LAST_COMMAND, },
};

int nofish_Init()
{
	SWSRegisterCommands(g_commandTable);
	gf_NFObeyTrackHeightLock     = GetPrivateProfileInt(SWS_INI, "NFObeyTrackHeightLock",     0, get_ini_file()); // disabled by default
	gf_NFTogglePlayStopPlayPause = GetPrivateProfileInt(SWS_INI, "NFTogglePlayStopPlayPause", 0, get_ini_file());
	return 1;
}


//////////////////////////////////////////////////////////////////
//                                                              //
// Register commands - continuous actions                       //
//                                                              //
//////////////////////////////////////////////////////////////////

void EraserToolInit()
{
	//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions
	static COMMAND_T s_commandTable[] =
	{
		{ { DEFACCEL, "SWS/NF: Eraser tool (marquee sel. items and time ignoring snap, cut on shortcut release)" }, "NF_ERASER_TOOL_NOSNAP", DoEraserTool, NULL, 0 },
		{ { DEFACCEL, "SWS/NF: Eraser tool (marquee sel. items and time, cut on shortcut release)" }, "NF_ERASER_TOOL", DoEraserTool, NULL, 1 },
		{ {}, LAST_COMMAND }
	};
	//!WANT_LOCALIZE_1ST_STRING_END

	int i = -1;
	while (s_commandTable[++i].id != LAST_COMMAND) {
		// ContinuousActionRegister(new BR_ContinuousAction(&s_commandTable[i], EraserToolInit, EraserToolUndo, EraserToolCursor));
		// set cursor in DoEraserTool(), otherwise it would get changed back by Reaper
		ContinuousActionRegister(new BR_ContinuousAction(&s_commandTable[i], EraserToolInit, EraserToolUndo, NULL));
	}

}

void NF_RegisterContinuousActions()
{
	EraserToolInit();
}
