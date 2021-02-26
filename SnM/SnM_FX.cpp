/******************************************************************************
/ SnM_FX.cpp
/
/ Copyright (c) 2009 and later Jeffos
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
#include "SnM_Chunk.h"
#include "SnM_FX.h"
#include "SnM_Track.h"
#include "SnM_Util.h"
#include "cfillion/cfillion.hpp"


///////////////////////////////////////////////////////////////////////////////
// Track fx bypass/unbypass
///////////////////////////////////////////////////////////////////////////////

// returns 0-based fx id
// _fxCmdId: 0-based fx id or -1=selected fx, -2=last fx, -3=second to last, etc..
// note: unexisting but valid fx ids must pass through, i.e. no test on TrackFX_GetCount(_tr)
int GetTrackFXIdFromCmd(MediaTrack* _tr, int _fxCmdId)
{
	int fxId = -1;
	if (_tr)
	{
		fxId = _fxCmdId;
		if (fxId == -1) { // selected fx
			fxId = GetSelectedTrackFX(_tr); // -1 on error
			if (fxId < 0) return -1;
		}
		if (fxId < 0)
			fxId = TrackFX_GetCount(_tr) + _fxCmdId + 1;
	}
	return fxId;
}

int IsFXBypassedSelTracks(COMMAND_T* _ct)
{
	const int selTrCount = SNM_CountSelectedTracks(NULL, true);

	// single track selection: return a real toggle state
	if (selTrCount == 1)
	{
		MediaTrack* tr = SNM_GetSelectedTrack(NULL, 0, true);
		int fxId = GetTrackFXIdFromCmd(tr, (int)_ct->user);
		if (tr && fxId >= 0)
			return !TrackFX_GetEnabled(tr, fxId);
	}
	// several selected tracks: possible mix of different states 
	// => return a fake toggle state (best effort)
	else if (selTrCount)
		return GetFakeToggleState(_ct);
	return false;
}

// _mode: 1= toggle all, toggle all except, 2=toggle fx, 3=set fx, 4=set all, set all except
bool SetOrToggleFXBypassSelTracks(const char* _undoMsg, int _mode, int _fxCmdId, bool _val = false)
{
	bool updated = false;
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			int fxId = GetTrackFXIdFromCmd(tr, _fxCmdId);
			int fxcnt = TrackFX_GetCount(tr);
			if (fxId >= 0)
				switch(_mode)
				{
					case 1: // toggle all, toggle all except
						for (int j=0; j<fxcnt; j++)
						{
							if (j != fxId)
							{
								if (_undoMsg && !updated)
									Undo_BeginBlock2(NULL);
								TrackFX_SetEnabled(tr, j, !TrackFX_GetEnabled(tr, j));
								updated = true;
							}
						}
						break;
					case 2: // toggle selected fx
					{
						// Get handle to FX window for this track
						HWND fxChain = CF_GetTrackFXChain(tr);
						if(fxChain)
						{
							int index=-1;
							// enumerate selected FX
							while( (index = CF_EnumSelectedFX(fxChain, index)) >= 0)
							{
								if (_undoMsg && !updated)
									Undo_BeginBlock2(NULL);
								// toggle enabled state
								TrackFX_SetEnabled(tr, index, !TrackFX_GetEnabled(tr, index));
								updated = true;
							}
						}
						else if(fxId < fxcnt)
						{
							  if (_undoMsg && !updated)
								  Undo_BeginBlock2(NULL);
							  TrackFX_SetEnabled(tr, fxId, !TrackFX_GetEnabled(tr, fxId));
							  updated = true;
						}
					}
						break;
					case 3: // set
						if (fxId < fxcnt && _val != TrackFX_GetEnabled(tr, fxId))
						{
							if (_undoMsg && !updated)
								Undo_BeginBlock2(NULL);
							TrackFX_SetEnabled(tr, fxId, _val);
							updated = true;
						}
						break;
					case 4: // set all, set all except
						for (int j=0; j<fxcnt; j++)
						{
							if (j != fxId)
							{
								if (_val != TrackFX_GetEnabled(tr, j))
								{
									if (_undoMsg && !updated)
										Undo_BeginBlock2(NULL);
									TrackFX_SetEnabled(tr, j, _val);
									updated = true;
								}
							}
							else if (_val == TrackFX_GetEnabled(tr, j))
							{
								if (_undoMsg && !updated)
									Undo_BeginBlock2(NULL);
								TrackFX_SetEnabled(tr, j, !_val);
								updated = true;
							}
						}
						break;
				}
		}
	}
	if (_undoMsg && updated)
		Undo_EndBlock2(NULL, _undoMsg, UNDO_STATE_ALL); // using UNDO_STATE_FX crashes 
	return updated;
}

void ToggleExceptFXBypassSelTracks(COMMAND_T* _ct) { 
	SetOrToggleFXBypassSelTracks(SWS_CMD_SHORTNAME(_ct), 1, (int)_ct->user);
}

void ToggleAllFXsBypassSelTracks(COMMAND_T* _ct) { 
	SetOrToggleFXBypassSelTracks(SWS_CMD_SHORTNAME(_ct), 1, 0xFFFF); // trick: unreachable fx number 
}

void ToggleFXBypassSelTracks(COMMAND_T* _ct) { 
	SetOrToggleFXBypassSelTracks(SWS_CMD_SHORTNAME(_ct), 2, (int)_ct->user);
} 

void BypassFXSelTracks(COMMAND_T* _ct) { 
	SetOrToggleFXBypassSelTracks(SWS_CMD_SHORTNAME(_ct), 3, (int)_ct->user, false); 
}

void UnbypassFXSelTracks(COMMAND_T* _ct) { 
	SetOrToggleFXBypassSelTracks(SWS_CMD_SHORTNAME(_ct), 3, (int)_ct->user, true); 
}

void UpdateAllFXsBypassSelTracks(COMMAND_T* _ct) {
	SetOrToggleFXBypassSelTracks(SWS_CMD_SHORTNAME(_ct), 4, 0xFFFF, ((int)_ct->user) ? true : false); // trick: unreachable fx number
}

void BypassAllFXsExceptSelTracks(COMMAND_T* _ct) {
	SetOrToggleFXBypassSelTracks(SWS_CMD_SHORTNAME(_ct), 4, (int)_ct->user, false);
}

void UnypassAllFXsExceptSelTracks(COMMAND_T* _ct) {
	SetOrToggleFXBypassSelTracks(SWS_CMD_SHORTNAME(_ct), 4, (int)_ct->user, true);
}


///////////////////////////////////////////////////////////////////////////////
// Track fx online/offline
///////////////////////////////////////////////////////////////////////////////

int g_SNM_SupportBuggyPlug = 0; // set by the user in S&M.ini

int IsFXOfflineSelTracks(COMMAND_T * _ct)
{
	const int selTrCount = SNM_CountSelectedTracks(NULL, true);

	// single track selection: return a real toggle state
	if (selTrCount == 1)
	{
		MediaTrack* tr = SNM_GetSelectedTrack(NULL, 0, true);
		int fxId = GetTrackFXIdFromCmd(tr, (int)_ct->user);
		if (tr && fxId >= 0)
		{
			char state[2] = "0";
			SNM_ChunkParserPatcher p(tr);
			p.SetWantsMinimalState(true);
			if (p.Parse(SNM_GET_CHUNK_CHAR, 2, "FXCHAIN", "BYPASS", fxId, 2, state) > 0)
				return !strcmp(state,"1");
		}
	}
	// several selected tracks: possible mix of different states 
	// => return a fake toggle state (best effort)
	else if (selTrCount)
		return GetFakeToggleState(_ct);
	return false;
}

// core func (no dedicated API yet => state chunk update)
bool PatchSelTracksFXOnline(const char * _undoMsg, int _mode, int _fxCmdId, const char* _val = NULL, const char* _valExcept = NULL)
{
	bool updated = false;
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			int fxId = GetTrackFXIdFromCmd(tr, _fxCmdId);
			if (fxId >= 0)
			{
				SNM_ChunkParserPatcher p(tr);
				bool updt = (p.ParsePatch(_mode, 2, "FXCHAIN", "BYPASS", fxId, 2, (void*)_val, (void*)_valExcept) > 0);
				updated |= updt;

				// close the GUI for buggy plugins (before chunk update)
				// http://github.com/reaper-oss/sws/issues/317
				if (updt && g_SNM_SupportBuggyPlug)
					TrackFX_SetOpen(tr, fxId, false);

			} // => auto commit
		}
	}
	if (updated)
	{
		Main_OnCommand(41204, 0); // fully unload unloaded VSTs
		if (_undoMsg)
			Undo_OnStateChangeEx2(NULL, _undoMsg, UNDO_STATE_ALL, -1); // using UNDO_STATE_FX crashes 
	}
	return updated;
}

void ToggleFXOfflineSelTracks(COMMAND_T* _ct) { 
	PatchSelTracksFXOnline(SWS_CMD_SHORTNAME(_ct), SNM_TOGGLE_CHUNK_INT, (int)_ct->user);
}

void ToggleExceptFXOfflineSelTracks(COMMAND_T* _ct) { 
	PatchSelTracksFXOnline(SWS_CMD_SHORTNAME(_ct), SNM_TOGGLE_CHUNK_INT_EXCEPT, (int)_ct->user);
}
  
void ToggleAllFXsOfflineSelTracks(COMMAND_T* _ct) { 
	PatchSelTracksFXOnline(SWS_CMD_SHORTNAME(_ct), SNM_TOGGLE_CHUNK_INT_EXCEPT, 0xFFFF); // trick: unreachable fx number
}
 
void SetFXOfflineSelTracks(COMMAND_T* _ct) { 
	PatchSelTracksFXOnline(SWS_CMD_SHORTNAME(_ct), SNM_SET_CHUNK_CHAR, (int)_ct->user, "1"); 
}

void SetFXOnlineSelTracks(COMMAND_T* _ct) { 
	PatchSelTracksFXOnline(SWS_CMD_SHORTNAME(_ct), SNM_SET_CHUNK_CHAR, (int)_ct->user, "0"); 
}

void SetAllFXsOfflineExceptSelTracks(COMMAND_T* _ct) {
	PatchSelTracksFXOnline(SWS_CMD_SHORTNAME(_ct), SNM_SETALL_CHUNK_CHAR_EXCEPT, (int)_ct->user, "1", "0"); 
}

void SetAllFXsOnlineExceptSelTracks(COMMAND_T* _ct) {
	PatchSelTracksFXOnline(SWS_CMD_SHORTNAME(_ct), SNM_SETALL_CHUNK_CHAR_EXCEPT, (int)_ct->user, "0", "1"); 
}


///////////////////////////////////////////////////////////////////////////////
// Take fx online/offline, bypass/unbypass
///////////////////////////////////////////////////////////////////////////////

// core func
// note: all takes are patched atm (i.e. nothing specific to active takes yet)
bool PatchSelItemsFXState(const char * _undoMsg, int _mode, int _token, int _fxId, const char* _value)
{
	bool updated = false;
	for (int i = 1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		for (int j = 0; tr && j < GetTrackNumMediaItems(tr); j++)
		{
			MediaItem* item = GetTrackMediaItem(tr,j);
			if (item && *(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
			{
				SNM_ChunkParserPatcher p(item);
				bool updt = (p.ParsePatch(_mode, 2, "TAKEFX", "BYPASS", _fxId, _token, (void*)_value) > 0);
				updated |= updt;

/*JFB not used: doesn't seem to occur with take FX
				// close the GUI for buggy plugins
				// http://github.com/reaper-oss/sws/issues/317
				// API LIMITATION: cannot restore shown FX here (contrary to track FX)
				if (updt && g_SNM_SupportBuggyPlug && _token == 2)
				{
					p.ParsePatch(SNM_SETALL_CHUNK_CHAR_EXCEPT,2,"TAKEFX","FLOAT",255,0,(void*)"FLOATPOS"); // unfloat all
					p.ParsePatch(SNM_SET_CHUNK_CHAR, 2, "TAKEFX", "SHOW",0,1,(void*)"0"); // no FX shown..
				}
*/
			}
		}
	}

	if (updated)
	{
		Main_OnCommand(41204, 0); // fully unload unloaded VSTs
		if (_undoMsg)
			Undo_OnStateChangeEx2(NULL, _undoMsg, UNDO_STATE_ALL, -1);
	}
	return updated;
}

void ToggleAllFXsOfflineSelItems(COMMAND_T* _ct) { 
	PatchSelItemsFXState(SWS_CMD_SHORTNAME(_ct), SNM_TOGGLE_CHUNK_INT_EXCEPT, 2, 0xFFFF, NULL);
} 
  
void ToggleAllFXsBypassSelItems(COMMAND_T* _ct) { 
	PatchSelItemsFXState(SWS_CMD_SHORTNAME(_ct), SNM_TOGGLE_CHUNK_INT_EXCEPT, 1, 0xFFFF, NULL); // trick: unreachable fx number
} 

void UpdateAllFXsOfflineSelItems(COMMAND_T* _ct) {
	char pInt[4] = "";
	if (snprintfStrict(pInt, sizeof(pInt), "%d", (int)_ct->user) > 0)
		PatchSelItemsFXState(SWS_CMD_SHORTNAME(_ct), SNM_SETALL_CHUNK_CHAR_EXCEPT, 2, 0xFFFF, pInt); // trick: unreachable fx number
}

void UpdateAllFXsBypassSelItems(COMMAND_T* _ct) {
	char pInt[4] = "";
	if (snprintfStrict(pInt, sizeof(pInt), "%d", (int)_ct->user) > 0)
		PatchSelItemsFXState(SWS_CMD_SHORTNAME(_ct), SNM_SETALL_CHUNK_CHAR_EXCEPT, 1, 0xFFFF, pInt); // trick: unreachable fx number
}


///////////////////////////////////////////////////////////////////////////////
// Track FX selection
///////////////////////////////////////////////////////////////////////////////

bool SelectTrackFX(MediaTrack* _tr, int _fx)
{
	if (_tr && _fx >= 0 && _fx < TrackFX_GetCount(_tr))
	{
		if (TrackFX_GetChainVisible(_tr) != -1) // FX chain is open, can use API
		{
			TrackFX_Show(_tr, _fx, 1);
			return true;
		}
		else // FX chain is closed, can't use API because it would open the FX chain, use chunking
		{
			char pLastSel[4] = "";
			if (snprintfStrict(pLastSel, sizeof(pLastSel), "%d", _fx) > 0)
			{
				SNM_ChunkParserPatcher p(_tr);
				if (p.ParsePatch(SNM_SET_CHUNK_CHAR, 2, "FXCHAIN", "LASTSEL", 0, 1, pLastSel) > 0)
					return true;
			}
		}
	}
	return false;
}

void SelectTrackFX(COMMAND_T* _ct) 
{
	bool updated = false;
	int fx = (int)_ct->user;
	for (int i=0; i <= GetNumTracks(); i++)  // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			int sel = -1;
			switch(fx)
			{
				case -3: // sel. last
					sel = TrackFX_GetCount(tr) - 1; // -1 on error
					break;
				case -2: // sel. previous
					sel = GetSelectedTrackFX(tr); // -1 on error
					if (sel > 0) sel--;
					else if (sel == 0) sel = TrackFX_GetCount(tr)-1;
					break;
				case -1: // sel. next
					sel = GetSelectedTrackFX(tr); // -1 on error
					if (sel >= 0 && sel < (TrackFX_GetCount(tr)-1)) sel++;
					else if (sel == (TrackFX_GetCount(tr)-1)) sel = 0;
					break;
				default:
					sel=fx;
					break;
			}
			if (sel >=0)
				updated |= SelectTrackFX(tr, sel);
		}
	}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

int GetSelectedTrackFX(MediaTrack* _tr)
{
	if (_tr)
	{
		// avoids useless parsing (1st try)
		if (TrackFX_GetCount(_tr) == 1)
			return 0;

		// avoids useless parsing (2nd try)
		int currentFX = TrackFX_GetChainVisible(_tr);
		if (currentFX >= 0)
			return currentFX;

		// the 2 attempts above failed => no choice: parse to get the selected FX
		SNM_ChunkParserPatcher p(_tr);
		p.SetWantsMinimalState(true);
		char pLastSel[4] = ""; // 4: if there are many, many FXs
		p.Parse(SNM_GET_CHUNK_CHAR,2,"FXCHAIN","LASTSEL",0,1,&pLastSel);
		return atoi(pLastSel); // return 0 (first FX) if failed
	}
	return -1;
}

int GetSelectedTakeFX(MediaItem_Take* _take)
{
	if (_take)
	{
		// avoids useless parsing (1st try)
		if (TakeFX_GetCount(_take) == 1)
			return 0;

		// avoids useless parsing (2nd try)
		int currentFX = TakeFX_GetChainVisible(_take);
		if (currentFX >= 0)
			return currentFX;

		// the 2 attempts above failed => no choice: parse to get the selected FX
		SNM_ChunkParserPatcher p(_take);
		p.SetWantsMinimalState(true);
		char pLastSel[4] = ""; // 4: if there are many, many FXs
		p.Parse(SNM_GET_CHUNK_CHAR, 2, "TAKEFX", "LASTSEL", 0, 1, &pLastSel);
		return atoi(pLastSel); // return 0 (first FX) if failed
	}
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
// FX presets helpers/actions
///////////////////////////////////////////////////////////////////////////////

// it is up to the caller to unalloc _presetNames
int GetUserPresetNames(MediaTrack* _tr, int _fx, WDL_PtrList<WDL_FastString>* _presetNames)
{
	int cnt=0;
	if (_tr && _fx>=0)
	{
		char fn[SNM_MAX_PATH];
		*fn = '\0';
		TrackFX_GetUserPresetFilename(_tr, _fx, fn, sizeof(fn));
		if (*fn && FileOrDirExists(fn))
		{
			char sec[64];
			char buf[SNM_MAX_PRESET_NAME_LEN];
			const int nbPresets = GetPrivateProfileInt("General", "NbPresets", 0, fn);
			for (int i=0; i < nbPresets; i++)
			{
				snprintf(sec, sizeof(sec), "Preset%d", i);
				GetPrivateProfileString(sec, "Name", "", buf, sizeof(buf), fn);
				if (*buf)
				{
					if (_presetNames) _presetNames->Add(new WDL_FastString(buf));
					cnt++;
				}
			}
		}
	}
	return cnt;
}

// _presetId: only taken into account when _dir == 0, see below
// _fxId: fx index
// _dir: +1 or -1 switches to next or previous preset (_presetId is ignored)
// returns true if preset changed
bool TriggerFXPreset(MediaTrack* _tr, int _fxId, int _presetId, int _dir)
{
#ifdef _SNM_DEBUG
	char dbg[256]="";
	snprintf(dbg, sizeof(dbg), "TriggerFXPreset() - tr: %p, fx: %d, preset: %d, dir: %d\n", _tr, _fxId, _presetId, _dir);
	OutputDebugString(dbg);
#endif
	int nbFx = _tr ? TrackFX_GetCount(_tr) : 0;
	if (nbFx>0 && _fxId<nbFx && _fxId>=0)
	{
		if (_dir)
			return TrackFX_NavigatePresets(_tr, _fxId, _dir);
		else if (_presetId)
			return TrackFX_SetPresetByIndex(_tr, _fxId, _presetId);
	}
	return false;
}

// _fxId: -1 for the selected fx, fx index otherwise
void TriggerFXPresetSelTracks(int _fxId, int _presetId, int _dir)
{
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			int fxId = _fxId;
			if (fxId == -1)
				fxId = GetSelectedTrackFX(tr);
			if (fxId >= 0)
				TriggerFXPreset(tr, fxId, _presetId, _dir);
		}
	}
}

void NextPresetSelTracks(COMMAND_T* _ct) {
	TriggerFXPresetSelTracks((int)_ct->user, 0, 1);
}

void PrevPresetSelTracks(COMMAND_T* _ct) {
	TriggerFXPresetSelTracks((int)_ct->user, 0, -1);
}

void NextPrevPresetLastTouchedFX(COMMAND_T* _ct) {
	int trId, fxId;
	if (GetLastTouchedFX(&trId, &fxId, NULL))
		if (MediaTrack* tr = CSurf_TrackFromID(trId, false))
			TriggerFXPreset(tr, fxId, 0, (int)_ct->user);
}

// only deals with the 1st selected track
// _fxId: -1 for the selected fx, fx index otherwise
// _presetIdx: NULL to get, pointer to set
// _numberOfPresetsOut: optional, only changed when getting
// returns: -1 on error (e.g. no selected track, no fx on 1st selected track, etc..) 
//          or the preset index when getting
//          or 1 when setting successfully
int GetSetFXPresetSelTrack(int _fxId, int* _presetIdx, int* _numberOfPresetsOut)
{
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			int fxId = _fxId;
			if (fxId == -1)
				fxId = GetSelectedTrackFX(tr);
			if (fxId>=0 && fxId<TrackFX_GetCount(tr))
			{
				if (!_presetIdx)
					return TrackFX_GetPresetIndex(tr, fxId, _numberOfPresetsOut);
				else
					return TrackFX_SetPresetByIndex(tr, fxId, *_presetIdx) ? 1 : -1;
			}
			return -1;
		}
	}
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
// Switch FX presets through MIDI/OSC actions
// only deals with the 1st selected track
///////////////////////////////////////////////////////////////////////////////

// each action as its own scheduled job id (e.g. allow tweaking 2 knobs simultaneously), 
// note: "SWS/S&M: Trigger preset for selected FX of selected track (MIDI/OSC only)" (with _fxId==-1) uses the id SNM_PRESETS_NB_FX+1
TriggerPresetJob::TriggerPresetJob(int _approxMs, int _val, int _valhw, int _relmode, int _fxId) 
	: MidiOscActionJob(SNM_SCHEDJOB_TRIG_PRESET + _fxId<0 ? SNM_PRESETS_NB_FX+1 : _fxId, _approxMs, _val, _valhw, _relmode), m_fxId(_fxId) 
{
}

void TriggerPresetJob::Perform() {
	int val = GetIntValue(); // just for &l-value
	GetSetFXPresetSelTrack(m_fxId, &val);
}

double TriggerPresetJob::GetCurrentValue() {
	return GetSetFXPresetSelTrack(m_fxId, NULL, NULL);
}

double TriggerPresetJob::GetMaxValue()
{ 	
	int presetCnt=0;
	if (GetSetFXPresetSelTrack(m_fxId, NULL, &presetCnt)>=0)
		return presetCnt-1;
	return 0.0;
}

void TriggerFXPresetSelTrack(COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd) {
	ScheduledJob::Schedule(new TriggerPresetJob(SNM_SCHEDJOB_DEFAULT_DELAY, _val, _valhw, _relmode, (int)_ct->user));
}


///////////////////////////////////////////////////////////////////////////////
// Other
///////////////////////////////////////////////////////////////////////////////

// primitive func (no undo point)
// _fxId: fx index in chain or -1 for the selected fx
// _what: 0 to remove, -1 to move fx up in chain, 1 to move fx down in chain
// note: brutal code (we'd need a dedicated parser/patcher here..)
// initially comes from http://github.com/reaper-oss/sws/issues/258
bool SNM_MoveOrRemoveTrackFX(MediaTrack* _tr, int _fxId, int _what)
{
	bool updated = false;
	if (_tr && _what >= -1 && _what <= 1)
	{
		int nbFx = TrackFX_GetCount(_tr);
		if (nbFx > 0)
		{
			int fxId = _fxId<0 ? GetSelectedTrackFX(_tr) : _fxId;
			if (fxId >= 0 && 
				((_what == 0  && fxId < nbFx) ||
				 (_what == 1  && fxId < (nbFx-1)) || 
				 (_what == -1 && fxId > 0)))
			{
				g_disable_chunk_guid_filtering++;
				SNM_ChunkParserPatcher p(_tr);
				WDL_FastString chainChunk;
				if (p.GetSubChunk("FXCHAIN", 2, 0, &chainChunk, "<ITEM") > 0)
				{
					SNM_ChunkParserPatcher pfxc(&chainChunk, false);
					int p1 = pfxc.Parse(SNM_GET_CHUNK_CHAR,1,"FXCHAIN","BYPASS",fxId,0);
					if (p1>0)
					{
						p1--;

						// locate end of fx
						int p2 = pfxc.Parse(SNM_GET_CHUNK_CHAR,1,"FXCHAIN","BYPASS",fxId+1,0);
						if (p2 > 0)
							p2--;
						else
							p2 = pfxc.GetChunk()->GetLength()-2; // -2 for ">\n"

						// store fx state (if needed)
						WDL_FastString fxChunk;
						if (_what)
							fxChunk.Set((const char*)(pfxc.GetChunk()->Get()+p1), p2-p1);

						// cut fx
						pfxc.GetChunk()->DeleteSub(p1, p2-p1);
						
						// move fx (if needed)
						if (_what)
						{
							int p3 = pfxc.Parse(SNM_GET_CHUNK_CHAR, 1, "FXCHAIN", "BYPASS", fxId + _what, 0);
							if (p3>0)
								pfxc.GetChunk()->Insert(fxChunk.Get(), --p3);
							else if (_what == 1)
								pfxc.GetChunk()->Insert(fxChunk.Get(), pfxc.GetChunk()->GetLength()-2);
						}

						// patch updated chunk
						if (p.ReplaceSubChunk("FXCHAIN", 2, 0, pfxc.GetChunk()->Get(), "<ITEM"))
						{
							updated = true;
							p.Commit(true);
							if (_what)
								SelectTrackFX(_tr, fxId + _what);
						}
					}
				}
				g_disable_chunk_guid_filtering--;
			}
		}
	}
	return updated;
}

void MoveOrRemoveTrackFX(COMMAND_T* _ct)
{
	bool updated = false;
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
			if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
				updated |= SNM_MoveOrRemoveTrackFX(tr, -1, (int)_ct->user);
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

