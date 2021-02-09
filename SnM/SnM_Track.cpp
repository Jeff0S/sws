/******************************************************************************
/ SnM_Track.cpp
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
#include "SnM_Routing.h"
#include "SnM_Track.h"
#include "SnM_Util.h"

#include <WDL/projectcontext.h>
#include <WDL/localize/localize.h>

///////////////////////////////////////////////////////////////////////////////
// Track helpers (with ReaProject* parameter)
// Note: primitive funcs, no undo points
///////////////////////////////////////////////////////////////////////////////

// get a track from a project by track count (1-based, 0 for master)
MediaTrack* SNM_GetTrack(ReaProject* _proj, int _idx) {
	if (!_idx) return GetMasterTrack(_proj);
	return GetTrack(_proj, _idx-1);
}

int SNM_GetTrackId(ReaProject* _proj, MediaTrack* _tr)
{
	int count = CountTracks(_proj);
	for (int i=0; i <= count; i++)
		if (SNM_GetTrack(_proj, i) == _tr)
			return i;
	return -1;
}

// take the master into account (contrary to CountSelectedTracks())
int SNM_CountSelectedTracks(ReaProject* _proj, bool _master)
{
	int selCnt = CountSelectedTracks(_proj);
	if (_master) {
		MediaTrack* mtr = GetMasterTrack(_proj);
		if (mtr && *(int*)GetSetMediaTrackInfo(mtr, "I_SELECTED", NULL))
			selCnt++;
	}
	return selCnt;
}

// get a track from a project by selected track count index
// rmk: to be used with SNM_CountSelectedTracks() and not the API's 
//      CountSelectedTracks() which does not take the master into account..
MediaTrack* SNM_GetSelectedTrack(ReaProject* _proj, int _idx, bool _withMaster)
{
	if (_withMaster)
	{
		MediaTrack* mtr = GetMasterTrack(_proj);
		if (mtr && *(int*)GetSetMediaTrackInfo(mtr, "I_SELECTED", NULL)) {
			if (!_idx) return mtr;
			else return GetSelectedTrack(_proj, _idx-1);
		}
	}
	return GetSelectedTrack(_proj, _idx);
}

// save, restore & clear track selection (primitives: no undo points)

void SNM_GetSelectedTracks(ReaProject* _proj, WDL_PtrList<MediaTrack>* _trs, bool _withMaster)
{
	if (_trs)
	{
		_trs->Empty();
		if (int count = CountTracks(_proj))
			for (int i=_withMaster?0:1; i <= count; i++)
				if (MediaTrack* tr = SNM_GetTrack(_proj, i))
					if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
						_trs->Add(tr);
	}
}

// note: no way to know if something has been updated with SetOnlyTrackSelected()
bool SNM_SetSelectedTracks(ReaProject* _proj, WDL_PtrList<MediaTrack>* _trs, bool _unselOthers, bool _withMaster)
{
	bool updated = false;
	const int count = CountTracks(_proj);

	PreventUIRefresh(1);
	for (int i=_withMaster?0:1; i <= count; i++)
	{
		if (MediaTrack* tr = SNM_GetTrack(_proj, i))
		{
			if (_trs && _trs->Find(tr) >= 0) {
				if (!*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL)) {
					GetSetMediaTrackInfo(tr, "I_SELECTED", &g_i1);
					updated = true;
				}
			}
			else if (_unselOthers) {
				if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL)) {
					GetSetMediaTrackInfo(tr, "I_SELECTED", &g_i0);
					updated = true;
				}
			}
		}
	}
	PreventUIRefresh(-1);

	return updated;
}

// wrapper func
// note: _tr=NULL + _unselOthers=true will unselect all tracks
bool SNM_SetSelectedTrack(ReaProject* _proj, MediaTrack* _tr, bool _unselOthers, bool _withMaster)
{
	WDL_PtrList<MediaTrack> trs; 
	if (_tr) trs.Add(_tr);
	return SNM_SetSelectedTracks(_proj, &trs, _unselOthers, _withMaster);
}


void SNM_ClearSelectedTracks(ReaProject* _proj, bool _withMaster)
{
	const int count = CountTracks(_proj);
	PreventUIRefresh(1);
	for (int i=_withMaster?0:1; i <= count; i++)
		if (MediaTrack* tr = SNM_GetTrack(_proj, i))
			if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
				GetSetMediaTrackInfo(tr, "I_SELECTED", &g_i0);
	PreventUIRefresh(-1);
}


///////////////////////////////////////////////////////////////////////////////
// Track grouping
///////////////////////////////////////////////////////////////////////////////

WDL_FastString g_trackGrpClipboard;
WDL_FastString g_trackGrpClipboard_high; // track groups 33 to 64, v5.70+

void CopyCutTrackGrouping(COMMAND_T* _ct)
{
	int updates = 0;
	bool copyDone = false;
	g_trackGrpClipboard.Set(""); // reset "clipboard"
	g_trackGrpClipboard_high.Set("");
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			SNM_ChunkParserPatcher p(tr);
			if (!copyDone) 
			{
				copyDone = (p.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "GROUP_FLAGS", 0, 0, &g_trackGrpClipboard, NULL, "MAINSEND") > 0);
				copyDone = (p.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "GROUP_FLAGS_HIGH", 0, 0, &g_trackGrpClipboard_high, NULL, "MAINSEND") > 0) || copyDone;
			}

			// cut (for all selected tracks)
			if ((int)_ct->user)
			{
				updates += p.RemoveLines("GROUP_FLAGS", true); // brutal removing ok: "GROUP_FLAGS" is not part of freeze data
				updates += p.RemoveLines("GROUP_FLAGS_HIGH", true); 
				if (updates) 
					p.IncUpdates();
			}
			// single copy: the 1st found track grouping
			else if (copyDone)
				break;
		}
	}
	if (updates)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

void PasteTrackGrouping(COMMAND_T* _ct)
{
	int updates = 0;
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			SNM_ChunkParserPatcher p(tr);
			updates += p.RemoveLines("GROUP_FLAGS", true); // brutal removing ok: "GROUP_FLAGS" is not part of freeze data
			updates += p.RemoveLines("GROUP_FLAGS_HIGH", true); 
			int patchPos = p.Parse(SNM_GET_CHUNK_CHAR, 1, "TRACK", "TRACKHEIGHT", 0, 0, NULL, NULL, "MAINSEND");
			if (patchPos > 0)
			{
				WDL_FastString* chunk = p.GetChunk();
				chunk->Insert(g_trackGrpClipboard_high.Get(), patchPos-1);
				chunk->Insert(g_trackGrpClipboard.Get(), patchPos-1);
				p.IncUpdates(); // as we're directly working on the cached chunk..
				updates++;
			}
		}
	}
	if (updates)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

void RemoveTrackGrouping(COMMAND_T* _ct)
{
	int updates = 0;
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL)) {
			SNM_ChunkParserPatcher p(tr);
			updates += p.RemoveLines("GROUP_FLAGS", true); // brutal removing ok: "GROUP_FLAGS" is not part of freeze data
			updates += p.RemoveLines("GROUP_FLAGS_HIGH", true);
			if (updates) 
				p.IncUpdates();
		}
	}
	if (updates)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

bool GetDefaultGroupFlags(WDL_FastString* _line, int _group)
{
	if (_line && _group >= 0 && _group < SNM_MAX_TRACK_GROUPS)
	{
		double grpMask = pow(2.0, _group < 32 ?_group*1.0 : (static_cast<double>(_group)-32)*1.0);
		char grpDefault[SNM_MAX_CHUNK_LINE_LENGTH] = "";
		GetPrivateProfileString("REAPER", "tgrpdef", "", grpDefault, sizeof(grpDefault), get_ini_file());
		WDL_FastString defFlags(grpDefault);
		while (defFlags.GetLength() < 64) { // complete the line (64 is more than needed: future-proof)
			if (defFlags.Get()[defFlags.GetLength()-1] != ' ') defFlags.Append(" ");
			defFlags.Append("0");
		}

		if (_group < 32)
			_line->Set("GROUP_FLAGS ");
		else
			_line->Set("GROUP_FLAGS_HIGH ");
		LineParser lp(false);
		if (!lp.parse(defFlags.Get())) {
			for (int i=0; i < lp.getnumtokens(); i++) {
				int n = lp.gettoken_int(i);
				_line->AppendFormatted(32, "%d", !n ? 0 : (int)grpMask);
				_line->Append(i == (lp.getnumtokens()-1) ? "\n" : " ");
			}
			return true;
		}
	}
	if (_line) _line->Set("");
	return false;
}

bool SetTrackGroup(int _group)
{
	int updates = 0;
	WDL_FastString defFlags;
	if (GetDefaultGroupFlags(&defFlags, _group))
	{
		for (int i=0; i <= GetNumTracks(); i++) // incl. master
		{
			MediaTrack* tr = CSurf_TrackFromID(i, false);
			if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			{
				SNM_ChunkParserPatcher p(tr);
				if (_group < 32)
					updates += p.RemoveLine("TRACK", "GROUP_FLAGS", 1, 0, "TRACKHEIGHT");
				else
					updates += p.RemoveLine("TRACK", "GROUP_FLAGS_HIGH", 1, 0, "TRACKHEIGHT");
				int pos = p.Parse(SNM_GET_CHUNK_CHAR, 1, "TRACK", "TRACKHEIGHT", 0, 0, NULL, NULL, "INQ");
				if (pos > 0) {
					pos--; // see SNM_ChunkParserPatcher..
					p.GetChunk()->Insert(defFlags.Get(), pos);
					p.SetUpdates(++updates); // as we're directly working on the cached chunk..
				}
			}
		}
	}
	return (updates > 0);
}

int FindFirstUnusedGroup()
{
	bool grp[SNM_MAX_TRACK_GROUPS];
	memset(grp, 0, sizeof(bool)*SNM_MAX_TRACK_GROUPS);

	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		//JFB TODO? exclude selected tracks?
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
		{
			SNM_ChunkParserPatcher p(tr);
			WDL_FastString grpLine;
			// groups 1 to 32
			if (p.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "GROUP_FLAGS", 0, 0, &grpLine, NULL, "TRACKHEIGHT"))
			{
				LineParser lp(false);
				if (!lp.parse(grpLine.Get())) {
					for (int j=1; j < lp.getnumtokens(); j++) { // skip 1st token GROUP_FLAGS
						int val = lp.gettoken_int(j);
						for (int k=0; k < 32; k++) {
							int grpMask = int(pow(2.0, k*1.0));
							grp[k] |= ((val & grpMask) == grpMask);
						}
					}
				}
			}
			// groups 33 to 64
			if (p.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "GROUP_FLAGS_HIGH", 0, 0, &grpLine, NULL, "TRACKHEIGHT"))
			{
				LineParser lp(false);
				if (!lp.parse(grpLine.Get())) {
					for (int j = 1; j < lp.getnumtokens(); j++) {
						int val = lp.gettoken_int(j);
						for (int k = 32; k < SNM_MAX_TRACK_GROUPS; k++) {
							int grpMask = int(pow(2.0, (static_cast<double>(k)-32)*1.0));
							grp[k] |= ((val & grpMask) == grpMask);
						}
					}
				}
			}
		}
	}

	for (int i=0; i < SNM_MAX_TRACK_GROUPS; i++) 
		if (!grp[i]) return i;

	return -1;
}

void SetTrackGroup(COMMAND_T* _ct) {
	if (SetTrackGroup((int)_ct->user))
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

void SetTrackToFirstUnusedGroup(COMMAND_T* _ct) {
	if (SetTrackGroup(FindFirstUnusedGroup()))
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}


///////////////////////////////////////////////////////////////////////////////
// Track folders
///////////////////////////////////////////////////////////////////////////////

int SNM_GetTrackDepth(MediaTrack* _tr)
{
	int depth = 0;
	for (int i=1; i <= GetNumTracks(); i++) // skip master
		if (MediaTrack* tr = CSurf_TrackFromID(i, false)) {
			depth += *(int*)GetSetMediaTrackInfo(tr, "I_FOLDERDEPTH", NULL);
			if (_tr==tr) return depth;
		}
	return depth;
}


WDL_PtrList_DOD<SNM_TrackInt> g_trackFolderStates;
WDL_PtrList_DOD<SNM_TrackInt> g_trackFolderCompactStates;

void SaveTracksFolderStates(COMMAND_T* _ct)
{
	const char* strState = !_ct->user ? "I_FOLDERDEPTH" : "I_FOLDERCOMPACT";
	WDL_PtrList_DOD<SNM_TrackInt>* saveList = !_ct->user ? &g_trackFolderStates : &g_trackFolderCompactStates;
	saveList->Empty(true);
	for (int i=1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			saveList->Add(new SNM_TrackInt(tr, *(int*)GetSetMediaTrackInfo(tr, strState, NULL)));
	}
}

void RestoreTracksFolderStates(COMMAND_T* _ct)
{
	bool updated = false;
	const char* strState = !_ct->user ? "I_FOLDERDEPTH" : "I_FOLDERCOMPACT";
	WDL_PtrList_DOD<SNM_TrackInt>* saveList = !_ct->user ? &g_trackFolderStates : &g_trackFolderCompactStates;
	for (int i=1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			for(int j=0; j < saveList->GetSize(); j++)
			{
				SNM_TrackInt* savedTF = saveList->Get(j);
				int current = *(int*)GetSetMediaTrackInfo(tr, strState, NULL);
				if (savedTF->m_tr == tr && 
					((!_ct->user && savedTF->m_int != current) ||
					(_ct->user && *(int*)GetSetMediaTrackInfo(tr, "I_FOLDERDEPTH", NULL) == 1 && savedTF->m_int != current)))
				{
					GetSetMediaTrackInfo(tr, strState, &(savedTF->m_int));
					updated = true;
					break;
				}
			}
		}
	}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

// I_FOLDERDEPTH :
//  0=normal, 1=track is a folder parent, 
// -1=track is the last in the innermost folder
// -2=track is the last in the innermost and next-innermost folders, etc
void SetTracksFolderState(COMMAND_T* _ct)
{
	bool updated = false;
	for (int i=1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			int newState = (int)_ct->user;
			int curState = *(int*)GetSetMediaTrackInfo(tr, "I_FOLDERDEPTH", NULL);
			if ((int)_ct->user == -1) // last in folder?
			{
				if (SNM_GetTrackDepth(tr)>0)
					newState = curState<0 ? curState-1 : -1;
				else
					newState = curState;
			}
			else if ((int)_ct->user == -2) // very last in folder?
			{
				int depth = SNM_GetTrackDepth(tr);
				if (depth>0)
					newState = depth * (-1);
				else
					newState = curState;
			}

			if (curState!=newState) {
				GetSetMediaTrackInfo(tr, "I_FOLDERDEPTH", &newState);
				updated = true;
			}
		}
	}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

// callers must unallocate the returned value, if any
WDL_PtrList<MediaTrack>* GetChildTracks(MediaTrack* _tr)
{
	int depth = (_tr ? *(int*)GetSetMediaTrackInfo(_tr, "I_FOLDERDEPTH", NULL) : 0);
	if (depth == 1)
	{
		WDL_PtrList<MediaTrack>* childTracks = new WDL_PtrList<MediaTrack>;
		int trIdx = CSurf_TrackToID(_tr, false) + 1;
		while (depth > 0 && trIdx <= GetNumTracks())
		{
			MediaTrack* tr = CSurf_TrackFromID(trIdx, false);
			depth += *(int*)GetSetMediaTrackInfo(tr, "I_FOLDERDEPTH", NULL);
			childTracks->Add(tr);
			trIdx++;
		}

		if (childTracks->GetSize())
			return childTracks;
		else
			delete childTracks;
	}
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// Track envelopes
///////////////////////////////////////////////////////////////////////////////

static const char s_trackEnvNames[][SNM_MAX_ENV_CHUNKNAME_LEN] =
{
	"PARMENV",
	"PROGRAMENV",
	// ^^ keep these as first items, see LookupTrackEnvName()

	"VOLENV2", // Volume
	"VOLENV3", // Trim volume
	"PANENV2",
	"WIDTHENV2",
	"VOLENV",
	"PANENV",
	"WIDTHENV",
	"MUTEENV",
	"AUXVOLENV",
	"AUXPANENV",
	"AUXMUTEENV",
	"HWVOLENV",
	"HWPANENV",
	"HWMUTEENV",
/*JFB useless: GetSetObjectState() removes them
	"TEMPOENVEX",
	"MASTERPLAYSPEEDENV",
*/
/*JFB useless: GetSetObjectState() wraps those into WIDTHENV, WIDTHENV2, etc..
	"MASTERWIDTHENV",
	"MASTERWIDTHENV2",
	"MASTERVOLENV",
	"MASTERPANENV",
	"MASTERVOLENV2",
	"MASTERPANENV2",
*/
	""
};

// native assumption: only 1 env selected at a time (v4.13)
MediaTrack* GetTrackWithSelEnv()
{
	if (TrackEnvelope* env = GetSelectedTrackEnvelope(NULL))
		for (int i=0; i <= GetNumTracks(); i++) // incl. master
			if (MediaTrack* tr = CSurf_TrackFromID(i, false))
				for (int j=0; j < CountTrackEnvelopes(tr); j++)
					if (env == GetTrackEnvelope(tr, j))
						return tr;
	return NULL;
}

void SelOnlyTrackWithSelEnv(COMMAND_T* _ct)
{
	bool updated = false;
	if (MediaTrack* envtr = GetTrackWithSelEnv())
		for (int i=0; i <= GetNumTracks(); i++) // incl. master
			if (envtr == CSurf_TrackFromID(i, false)) {
				updated = SNM_SetSelectedTrack(NULL, envtr, true);
				break;
			}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

// _allEnvs: true = track + fx param envs, false = track envs only
bool LookupTrackEnvName(const char* _str, bool _allEnvs)
{
	int i = _allEnvs ? 0:2; 
	while (*s_trackEnvNames[i]) {
		if (!strcmp(_str, s_trackEnvNames[i]))
			return true;
		i++;
	}
	return false;
}

void ToggleArmTrackEnv(COMMAND_T* _ct)
{
	bool updated = false;
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false); 
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			SNM_ArmEnvParserPatcher p(tr);
			switch(_ct->user)
			{
				// all envs
				case 0:
					p.SetNewValue(-1);//toggle
					updated = (p.ParsePatch(-1) > 0);
					break;
				case 1:
					p.SetNewValue(1);//arm
					updated = (p.ParsePatch(-1) > 0); 
					break;
				case 2:
					p.SetNewValue(0); //disarm
					updated = (p.ParsePatch(-1) > 0);
					break;
				// track vol/pan/mute envs
				case 3:
					updated = (p.ParsePatch(SNM_TOGGLE_CHUNK_INT, 2, "VOLENV2", "ARM", 0, 1) > 0);
					break;
				case 4:
					updated = (p.ParsePatch(SNM_TOGGLE_CHUNK_INT, 2, "PANENV2", "ARM", 0, 1) > 0);
					break;
				case 5:
					updated = (p.ParsePatch(SNM_TOGGLE_CHUNK_INT, 2, "MUTEENV", "ARM", 0, 1) > 0);
					break;
				// receive envs
				case 6:
					p.SetNewValue(-1); //toggle
					updated = (p.ParsePatch(-2) > 0);
					break;
				case 7:
					p.SetNewValue(-1); //toggle
					updated = (p.ParsePatch(-3) > 0);
					break;
				case 8:
					p.SetNewValue(-1); //toggle
					updated = (p.ParsePatch(-4) > 0);
					break;
				// plugin envs
				case 9:
					p.SetNewValue(-1); //toggle
					updated = (p.ParsePatch(-5) > 0);
					break;
			}
		}
	}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

// Veto: currently several issues (see #918):
// 1. Doesn't remove send envs of selected tracks
// 2. Removes send envs of all tracks which have sends to selected tracks
// 3. Automation items of removed envs remain in project orphaned (on "Track level" (depth 1), see "<POOLEDENV")
void RemoveAllEnvsSelTracks(COMMAND_T* _ct)
{
	bool updated = false;
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
			if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL)) {
				SNM_TrackEnvParserPatcher p(tr);
				updated |= p.RemoveEnvelopes();
			}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

void RemoveAllEnvsSelTracksNoChunk(COMMAND_T* _ct)
{
	const bool prompt = !!GetPrivateProfileInt("Misc", "RemoveAllEnvsSelTracksPrompt", 0, g_SNM_IniFn.Get());
	bool updated = false;
	TrackEnvelope *tempoEnv = nullptr;

	const int trackCount = GetNumTracks();
	for (int ti = 0; ti <= trackCount; ti++) { // incl. master
		MediaTrack *tr = CSurf_TrackFromID(ti, false);

		if (!tr || *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", nullptr) != 1)
			continue;

		if (ti == 0) // Master track
			tempoEnv = GetTrackEnvelopeByName(tr, "Tempo map");

		int envIndex = 0;
		while (CountTrackEnvelopes(tr) > envIndex) {
			TrackEnvelope* env = GetTrackEnvelope(tr, envIndex);

			if (!env || env == tempoEnv) {
				++envIndex;
				continue;
			}

			if (!updated) {
				if (prompt) {
					const int r = MessageBox(GetMainHwnd(),
						__LOCALIZE("All envelopes for selected tracks will be removed.\nDo you want to continue?", "sws_DLG_155"),
						__LOCALIZE("S&M - Question", "sws_DLG_155"), MB_OKCANCEL);
					if (r == IDCANCEL)
						return;
				}

				PreventUIRefresh(1);
				Undo_BeginBlock();
				Main_OnCommand(41148, 0); // required for env selection: Envelope: Show all envelopes for (selected) tracks.
				updated = true;
			}

			if(const int aiCount = CountAutomationItems(env)) {
				for(int ai = 0; ai < aiCount; ai++)
					GetSetAutomationItemInfo(env, ai, "D_UISEL", 1, true);
				Main_OnCommand(42086, 0);  // Envelope: Delete (selected) automation items
			}

			SetCursorContext(2, env);    // Select envelope
			DeleteEnvelopePointRange(env, -1000000000, 1000000000); //  hack to not spawn a dialog on action 40065
			Main_OnCommand(40065, 0);    // Envelope: Clear (selected) envelope
		}
	}

	if (updated) {
		PreventUIRefresh(-1);
		Undo_EndBlock(SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Toolbar track env. write mode toggle
///////////////////////////////////////////////////////////////////////////////

WDL_PtrList_DOD<SNM_TrackInt> g_toolbarAutoModeToggles;

void ToggleWriteEnvExists(COMMAND_T* _ct)
{
	bool updated = false;
	// restore write modes
	if (g_toolbarAutoModeToggles.GetSize())
	{
		for (int i=0; i < g_toolbarAutoModeToggles.GetSize(); i++)
		{
			SNM_TrackInt* tri = g_toolbarAutoModeToggles.Get(i);
			GetSetMediaTrackInfo(tri->m_tr, "I_AUTOMODE", &(tri->m_int));
			updated = true;
		}
		g_toolbarAutoModeToggles.Empty(true);
	}
	// set read mode and store previous write modes
	else
	{
		for (int i=0; i <= GetNumTracks(); i++) // incl. master
		{
			MediaTrack* tr = CSurf_TrackFromID(i, false); 
			int autoMode = tr ? *(int*)GetSetMediaTrackInfo(tr, "I_AUTOMODE", NULL) : -1;
			if (autoMode >= 2 /* touch */ && autoMode <= 5 /* latch preview */)
			{
				GetSetMediaTrackInfo(tr, "I_AUTOMODE", &g_i1); // set read mode
				g_toolbarAutoModeToggles.Add(new SNM_TrackInt(tr, autoMode));
				updated = true;
			}
		}
	}

	if (updated) {
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
		RefreshToolbar(SWSGetCommandID(ToggleWriteEnvExists)); // in case auto-refresh toolbar option is off..
	}
}

int WriteEnvExists(COMMAND_T* _ct)
{
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false); 
		int autoMode = tr ? *(int*)GetSetMediaTrackInfo(tr, "I_AUTOMODE", NULL) : -1;
		if (autoMode >= 2 /* touch */ && autoMode <= 5 /* latch preview */)
			return true;
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// Loading/applying track templates
///////////////////////////////////////////////////////////////////////////////

bool MakeSingleTrackTemplateChunk(WDL_FastString* _in, WDL_FastString* _out, bool _delItems, bool _delEnvs, int _tmpltIdx, bool _obeyOffset)
{
	if (_in && _in->GetLength() && _out && _in!=_out)
	{
		_out->Set("");

		// truncate to the track #_tmpltIdx found in the template
		SNM_ChunkParserPatcher pin(_in);
		if (pin.GetSubChunk("TRACK", 1, _tmpltIdx, _out) >= 0)
		{
			const int offsOpt = _obeyOffset ? ConfigVar<int>("templateditcursor").value_or(0) : 0; // >= REAPER v4.15

			// remove receives from the template as we deal with a single track
			// note: possible with multiple tracks in a same template file (w/ routings between those tracks)
			SNM_TrackEnvParserPatcher pout(_out);
			pout.RemoveLine("TRACK", "AUXRECV", 1, -1, "MIDIOUT");

			if (_delItems) // remove items from template (in one go)
				pout.RemoveSubChunk("ITEM", 2, -1);
			else if (offsOpt) { // or offset them if needed
				double add = GetCursorPositionEx(NULL);
				pout.ParsePatch(SNM_D_ADD, 2, "ITEM", "POSITION", -1, 1, &add);
			}

			if (_delEnvs) // remove all envs from template (in one go)
				pout.RemoveEnvelopes();
			else if (offsOpt) // or offset them if needed
				pout.OffsetEnvelopes(GetCursorPositionEx(NULL));
			return true;
		}
	}
	return false;
}

bool GetItemsSubChunk(WDL_FastString* _in, WDL_FastString* _out, int _tmpltIdx)
{
	if (_in && _in->GetLength() && _out && _in!=_out)
	{
		_out->Set("");

		// truncate to the track #_tmpltIdx found in the template
		SNM_ChunkParserPatcher p(_in);
		if (p.GetSubChunk("TRACK", 1, _tmpltIdx, _out) >= 0)
		{
//JFB!! to update with SNM_ChunkParserPatcher v2
//		ex: return (p->GetSubChunk("ITEM", 2, -1, _outSubChunk) >= 0); // -1 to get all items in one go
			SNM_ChunkParserPatcher pout(_out);
			int posItems = pout.GetSubChunk("ITEM", 2, 0); // no breakKeyword possible here: chunk ends with items
			if (posItems >= 0) {
				_out->Set((const char*)(pout.GetChunk()->Get()+posItems), pout.GetChunk()->GetLength()-posItems-2);  // -2: ">\n"
				return true;
			}
		}
	}
	return false;
}

// replace or paste items sub-chunk _tmpltItems
// _paste==false for replace, paste otherwise
// _p: optional (to factorize chunk updates)
bool ReplacePasteItemsFromTrackTemplate(MediaTrack* _tr, WDL_FastString* _tmpltItems, bool _paste, SNM_ChunkParserPatcher* _p)
{
	bool updated = false;
	if (_tr && _tr != GetMasterTrack(NULL) && _tmpltItems) // no items on master
	{
		SNM_ChunkParserPatcher* p = (_p ? _p : new SNM_ChunkParserPatcher(_tr));

		 // delete current items?
		if (!_paste)
			updated |= p->RemoveSubChunk("ITEM", 2, -1);

		// insert template items
		if (_tmpltItems->GetLength())
		{
			WDL_FastString tmpltItems(_tmpltItems); // do not alter _tmpltItems!

			// offset items if needed
			if (ConfigVar<int>("templateeditcursor").value_or(0)) // >= REAPER v4.15
			{
				double add = GetCursorPositionEx(NULL);
				SNM_ChunkParserPatcher pitems(&tmpltItems);
				pitems.ParsePatch(SNM_D_ADD, 1, "ITEM", "POSITION", -1, 1, &add);
			}

			p->GetChunk()->Insert(tmpltItems.Get(), p->GetChunk()->GetLength()-2); // -2: before ">\n"
			p->IncUpdates(); // as we directly work on the chunk
			updated = true;
		}

		if (!_p)
			DELETE_NULL(p); // + auto-commit if needed
	}
	return updated;
}

// apply a track template (primitive: no undo, folder states are lost, receives are removed)
// _tmplt: the track template
// _p: optional (to factorize chunk updates)
// note: assumes _tmplt contains a single track (with items/envs already removed when _itemsFromTmplt/_envsFromTmplt are false)
//       i.e. use MakeSingleTrackTemplateChunk() first!
bool ApplyTrackTemplatePrimitive(MediaTrack* _tr, WDL_FastString* _tmplt, bool _itemsFromTmplt, bool _envsFromTmplt, SNM_SendPatcher* _p)
{
	bool updated = false;
	if (_tr && _tmplt)
	{
		WDL_FastString tmplt(_tmplt); // not to mod the input template..
		SNM_ChunkParserPatcher* p = (_p ? _p : new SNM_ChunkParserPatcher(_tr));

		// add current track items, if any
		if (!_itemsFromTmplt) 
		{
/*JFB!! works with SNM_ChunkParserPatcher v2
			WDL_FastString curItems;
			if (p->GetSubChunk("ITEM", 2, -1, &curItems) >= 0) // -1 to get all items in one go
				newChunk.Insert(&curItems, newChunk.GetLength()-2); // -2: ">\n", 
*/
			// insert current items in template
			int posItems = p->GetSubChunk("ITEM", 2, 0);
			if (posItems >= 0)
				tmplt.Insert((char*)(p->GetChunk()->Get()+posItems), tmplt.GetLength()-2, p->GetChunk()->GetLength()-posItems-2); // -2: ">\n"
		}
		// safety: force items removal for master track
		//JFB REAPER BUG: test required, items would be added to the master (!)
		else if (_tr == GetMasterTrack(NULL))
		{
			SNM_ChunkParserPatcher ptmplt(&tmplt);
			ptmplt.RemoveSubChunk("ITEM", 2, -1);
		}

		// add current track envs in template, if any
		if (!_envsFromTmplt) 
		{
/*JFB simple casting ko: _mode mess, etc...
			if (const char* envs = ((SNM_TrackEnvParserPatcher*)p)->GetTrackEnvelopes())
*/
			SNM_TrackEnvParserPatcher penv(p->GetChunk(), false); // no auto-commit!
			if (const char* envs = penv.GetTrackEnvelopes()) // get all track envs in one go, exclude fx param envs
			{
				SNM_ChunkParserPatcher ptmplt(&tmplt);
				// ">ITEM": best effort for the break keyword (fx chain might not exist, etc..)
				ptmplt.InsertAfterBefore(1, envs, "TRACK", "MAINSEND", 1, 0, "<ITEM");
			}
		}

		// the meat, apply template!
		p->SetChunk(tmplt.Get());
		updated = true;

		if (!_p)
			DELETE_NULL(p); // + auto-commit if needed
	}
	return updated;
}

// apply a track template (preserves routings and folder states)
// _p: optional (to factorize chunk updates)
// note: assumes _tmplt contains a single track (with items/envs already removed when _itemsFromTmplt/_envsFromTmplt are false)
//       i.e. use MakeSingleTrackTemplateChunk() first!
bool ApplyTrackTemplate(MediaTrack* _tr, WDL_FastString* _tmplt, bool _itemsFromTmplt, bool _envsFromTmplt, void* _p)
{
	bool updated = false;
	if (_tr && _tmplt && _tmplt->GetLength())
	{
		SNM_SendPatcher* p = (_p ? (SNM_SendPatcher*)_p : new SNM_SendPatcher(_tr));

		// store receives, folder and compact states
		WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> > rcvs;
		WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> > snds;
		WDL_PtrList<MediaTrack> trs; trs.Add(_tr);
		WDL_FastString busLine, compbusLine;
		if (_tr != GetMasterTrack(NULL))
		{
			CopySendsReceives(true, &trs, &snds, &rcvs);
			// parse twice but it does not cost much (search at the very start of the chunk)
			p->Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "ISBUS", 0, -1, &busLine, NULL, "BUSCOMP");
			p->Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "BUSCOMP", 0, -1, &compbusLine, NULL, "SHOWINMIX");
		}

		// apply tr template
		if ((updated = ApplyTrackTemplatePrimitive(_tr, _tmplt, _itemsFromTmplt, _envsFromTmplt, p)))
		{
			// restore receives, folder and compact states
			WDL_PtrList<SNM_ChunkParserPatcher> ps; ps.Add(p);
			PasteSendsReceives(&trs, &snds, &rcvs, &ps);

			if (busLine.GetLength())
				p->ReplaceLine("TRACK", "ISBUS", 1, 0, busLine.Get(), "BUSCOMP");
			if (compbusLine.GetLength())
				p->ReplaceLine("TRACK", "BUSCOMP", 1, 0, compbusLine.Get(), "SHOWINMIX");
		}

/*JFB!! works with SNM_ChunkParserPatcher v2 + "SNM_SendPatcher" to remove

		// store current receives, folder and compact states
		// done through the chunk because the API is incomplete for folder states (tcp only)
		WDL_FastString rcvs, busLine, compbusLine;
		if (_tr != GetMasterTrack(NULL))
		{
			// parse twice but it does not cost much (search at the very start of the chunk)
			p->Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "AUXRECV", -1, -1, &rcvs, NULL, "MIDIOUT");
			p->Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "ISBUS", 0, -1, &busLine, NULL, "BUSCOMP");
			p->Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "BUSCOMP", 0, -1, &compbusLine, NULL, "SHOWINMIX");
		}

		// apply tr template
		if (updated = ApplyTrackTemplatePrimitive(_tr, _tmpltChunk, _itemsFromTmplt, _envsFromTmplt, p)) 
		{
			// restore receives, folder and compact states
			if (busLine.GetLength())
				p->ReplaceLine("TRACK", "ISBUS", 1, 0, busLine.Get(), "BUSCOMP");
			if (compbusLine.GetLength())
				p->ReplaceLine("TRACK", "BUSCOMP", 1, 0, compbusLine.Get(), "SHOWINMIX");
			if (rcvs.GetLength())
				p->InsertAfterBefore(0, rcvs.Get(), "TRACK", "MIDIOUT", 1, 0, "MAINSEND");
		}
*/
		if (!_p)
			DELETE_NULL(p); // + auto-commit if needed
	}
	return updated;
}


///////////////////////////////////////////////////////////////////////////////
// Save track templates
///////////////////////////////////////////////////////////////////////////////

// appends _tr's state to _chunkOut
//JFB TODO: save envs with beat position info (like it is done for items)
void SaveSingleTrackTemplateChunk(MediaTrack* _tr, WDL_FastString* _chunkOut, bool _delItems, bool _delEnvs)
{
	if (_tr && _chunkOut)
	{
		SNM_TrackEnvParserPatcher p(_tr, false); // no auto-commit!
		if (_delEnvs)
			p.RemoveEnvelopes();

		// systematically remove items whatever is _delItems, then
		// replace them with items + optional beat pos info, if !_delItems (i.e. mimic native templates)
		p.RemoveSubChunk("ITEM", 2, -1);

		_chunkOut->Append(p.GetChunk()->Get());

		if (!_delItems)
		{
			double d;
			WDL_FastString beatInfo;
			for (int i=0; i<GetTrackNumMediaItems(_tr); i++)
			{
				if (MediaItem* item = GetTrackMediaItem(_tr, i))
				{
					SNM_ChunkParserPatcher pitem(item, false); // no auto-commit!
					int posChunk = pitem.GetLinePos(1, "ITEM", "POSITION", 1, 0); // look for the *next* line
					if (--posChunk>=0) { // --posChunk to zap "\n"
						TimeMap2_timeToBeats(NULL, *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL), NULL, NULL, &d, NULL);
						beatInfo.SetFormatted(32, " %.14f", d);
						pitem.GetChunk()->Insert(beatInfo.Get(), posChunk);
					}
					posChunk = pitem.GetLinePos(1, "ITEM", "SNAPOFFS", 1, 0);
					if (--posChunk>=0) {
						TimeMap2_timeToBeats(NULL, *(double*)GetSetMediaItemInfo(item, "D_SNAPOFFSET", NULL), NULL, NULL, &d, NULL);
						beatInfo.SetFormatted(32, " %.14f", d);
						pitem.GetChunk()->Insert(beatInfo.Get(), posChunk);
					}
					posChunk = pitem.GetLinePos(1, "ITEM", "LENGTH", 1, 0);
					if (--posChunk>=0) {
						TimeMap2_timeToBeats(NULL, *(double*)GetSetMediaItemInfo(item, "D_LENGTH", NULL), NULL, NULL, &d, NULL);
						beatInfo.SetFormatted(32, " %.14f", d);
						pitem.GetChunk()->Insert(beatInfo.Get(), posChunk);
					}
					_chunkOut->Insert(pitem.GetChunk(), _chunkOut->GetLength()-2); // -2: before ">\n"
				}
			}
		}
	}
}


// supports folders, multi-selection and routings between tracks too
void SaveSelTrackTemplates(bool _delItems, bool _delEnvs, WDL_FastString* _chunkOut)
{
	if (!_chunkOut)
		return;

	WDL_PtrList<MediaTrack> tracks;

	// append selected track chunks (+ folders) -------------------------------
	for (int i=0; i <= GetNumTracks(); i++) // incl. master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			SaveSingleTrackTemplateChunk(tr, _chunkOut, _delItems, _delEnvs);
			tracks.Add(tr);

			// folder: save child templates
			WDL_PtrList<MediaTrack>* childTracks = GetChildTracks(tr);
			if (childTracks)
			{
				for (int j=0; j < childTracks->GetSize(); j++) {
					SaveSingleTrackTemplateChunk(childTracks->Get(j), _chunkOut, _delItems, _delEnvs);
					tracks.Add(childTracks->Get(j));
				}
				i += childTracks->GetSize(); // skip children
				delete childTracks;
			}
		}
	}

	// update receives ids ----------------------------------------------------
	// no break keyword used here: multiple tracks in the template
	SNM_ChunkParserPatcher p(_chunkOut);
	WDL_FastString line;
	int occurence = 0;
	int pos = p.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "AUXRECV", occurence, 1, &line); 
	while (pos > 0)
	{
		pos--; // see SNM_ChunkParserPatcher

		bool replaced = false;
		line.SetLen(line.GetLength()-1); // remove trailing '\n'
		LineParser lp(false);
		if (!lp.parse(line.Get()) && lp.getnumtokens() > 1)
		{
			int success, curId = lp.gettoken_int(1, &success);
			if (success)
			{
				MediaTrack* tr = CSurf_TrackFromID(curId+1, false);
				int newId = tracks.Find(tr);
				if (newId >= 0)
				{
					const char* p3rdTokenToEol = strchr(line.Get(), ' ');
					if (p3rdTokenToEol) p3rdTokenToEol = strchr((char*)(p3rdTokenToEol+1), ' ');
					if (p3rdTokenToEol)
					{
						WDL_FastString newRcv;
						newRcv.SetFormatted(SNM_MAX_CHUNK_LINE_LENGTH, "AUXRECV %d%s\n", newId, p3rdTokenToEol);
						replaced = p.ReplaceLine(pos, newRcv.Get());
						if (replaced)
							occurence++;
					}
				}
			}
		}

		if (!replaced)
			replaced = p.ReplaceLine(pos, "");
		if (!replaced) // skip, just in case..
			occurence++;

		line.Set("");
		pos = p.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "TRACK", "AUXRECV", occurence, 1, &line);
	}
}


///////////////////////////////////////////////////////////////////////////////
// Track input actions
///////////////////////////////////////////////////////////////////////////////

void SetMIDIInputChannel(COMMAND_T* _ct)
{
	bool updated = false;
	int ch = ((int)_ct->user)+1; // ch=0: all channels
	for (int i=1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			int in = *(int*)GetSetMediaTrackInfo(tr, "I_RECINPUT", NULL);
			if (((in & 0x1000) == 0x1000) && ((in & 0x1F) != ch))
			{
				in &= 0x1FE0;
				in |= ch;
				GetSetMediaTrackInfo(tr, "I_RECINPUT", &in);
				updated = true;
			}
		}
	}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

void RemapMIDIInputChannel(COMMAND_T* _ct)
{
	bool updated = false;
	int ch = ((int)_ct->user)+1; // ch=0: source channel

	char pLine[SNM_MAX_CHUNK_LINE_LENGTH] = "";
	if (ch && snprintfStrict(pLine, sizeof(pLine), "MIDI_INPUT_CHANMAP %d\n", ch-1) <= 0)
		return;

	for (int i=1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			int in = *(int*)GetSetMediaTrackInfo(tr, "I_RECINPUT", NULL);
			if ((in & 0x1000) == 0x1000) // midi in?
			{
				SNM_ChunkParserPatcher p(tr);
				char currentCh[3] = "";
				int chunkPos = p.Parse(SNM_GET_CHUNK_CHAR, 1, "TRACK", "MIDI_INPUT_CHANMAP", 0, 1, currentCh, NULL, "TRACKID");
				if (chunkPos > 0) {
					if (!ch || atoi(currentCh) != (ch-1))
						updated |= p.ReplaceLine(--chunkPos, pLine); // pLine can be "", i.e. remove line
				}
				else
					updated |= p.InsertAfterBefore(0, pLine, "TRACK", "TRACKHEIGHT", 1, 0, "TRACKID");
			}
		}
	}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}


///////////////////////////////////////////////////////////////////////////////
// Play track preview
///////////////////////////////////////////////////////////////////////////////

// helper funcs
void TrackPreviewInitDeleteMutex(preview_register_t* _prev, bool _init) {
	if (_init)
#ifdef _WIN32
		InitializeCriticalSection(&_prev->cs);
#else
		pthread_mutex_init(&_prev->mutex, NULL);
#endif
	else
#ifdef _WIN32
		DeleteCriticalSection(&_prev->cs);
#else
		pthread_mutex_destroy(&_prev->mutex);
#endif
}

void TrackPreviewLockUnlockMutex(preview_register_t* _prev, bool _lock) {
	if (_lock)
#ifdef _WIN32
		EnterCriticalSection(&_prev->cs);
#else
		pthread_mutex_lock(&_prev->mutex);
#endif
	else
#ifdef _WIN32
		LeaveCriticalSection(&_prev->cs);
#else
		pthread_mutex_unlock(&_prev->mutex);
#endif
}

void DeleteTrackPreview(void* _prev)
{
	if (_prev)
	{
		preview_register_t* prev = (preview_register_t*)_prev;
		StopTrackPreview2(NULL, prev);
		DELETE_NULL(prev->src);
		TrackPreviewInitDeleteMutex(prev, false);
		DELETE_NULL(prev);
	}
}

// can be used to start/pause all track previews in sync, such a lock should be avoided though
// => the related pref is intentionally burried in S&M.ini (for very specific use-cases)
void TrackPreviewLockUnlockTracks(bool _lock)
{
	if (g_SNM_MediaFlags&1)
	{
		if (_lock) MainThread_LockTracks();
		else MainThread_UnlockTracks();
	}
}


///////////////////////////////////////////////////////////////////////////////

class PausedPreview
{
public:
	PausedPreview(const char* _fn, MediaTrack* _tr, double _pos)
		: m_fn(_fn),m_tr(_tr),m_pos(_pos) {}
	bool IsMatching(preview_register_t* _prev)
	{
		return (_prev && _prev->preview_track == m_tr && 
			_prev->src && _prev->src->GetFileName() &&
			!_stricmp(_prev->src->GetFileName(), m_fn.Get()));
	}
	WDL_FastString m_fn; MediaTrack* m_tr; double m_pos;
};

// no WDL_PtrList_DOD here: need to stop the preview (and delete it, delete its src, etc)
WDL_PtrList_DeleteOnDestroy<preview_register_t> g_playPreviews(DeleteTrackPreview);
WDL_PtrList_DOD<PausedPreview> g_pausedPreviews;


// stop playing track previews if needed
// polled from the main thread via SNM_CSurfRun()
void StopTrackPreviewsRun()
{
	static WDL_PtrList<void> ano_trs;
	for (int i=g_playPreviews.GetSize()-1; i >=0; i--)
	{
		if (preview_register_t* prev = g_playPreviews.Get(i))
		{
			bool want_delete=false;

			TrackPreviewLockUnlockMutex(prev, true);
			if (!prev->loop && 
				(prev->volume < 0.5 || // stop has been requested
				prev->curpos >= prev->src->GetLength() || // preview has been entirely played
				fabs(prev->curpos - prev->src->GetLength()) < 0.000001))
			{
				// prepare all notes off, if needed (i.e. only for stopped MIDI files)
				if (prev->volume < 0.5 && // stopped files only, no cc123 is sent when files end normally
					!strncmp(prev->src->GetType(), "MIDI", 4) && 
					ano_trs.Find(prev->preview_track) < 0)
				{
					ano_trs.Add(prev->preview_track);
				}
				want_delete=true;
			}
			TrackPreviewLockUnlockMutex(prev, false);

			// stop and delete the preview, its src, etc
			if (want_delete) g_playPreviews.Delete(i, true, DeleteTrackPreview);
		}
	}

	// send all notes off, if needed
	SendAllNotesOff(&ano_trs, 1|2);
	ano_trs.Empty();
}

// from askjf.com:
// if 1 is set in bufflags, it will buffer-ahead (in case your source needs to do some cpu intensive processing, a good idea
// -- if it needs to be low latency, then don't set the 1 bit).
// MSI is the measure start interval, which if set to n greater than 0, means start synchronized to playback synchronized to a multiple of n measures
bool SNM_PlayTrackPreview(MediaTrack* _tr, PCM_source* _src, bool _pause, bool _loop, double _msi)
{
	bool ok = false;
	if (_src)
	{
		preview_register_t* prev = new preview_register_t;
		memset(prev, 0, sizeof(preview_register_t));
		TrackPreviewInitDeleteMutex(prev, true);
		prev->src = _src;
		prev->m_out_chan = -1;
		prev->curpos = 0.0;
		prev->loop = _loop;
		prev->volume = 1.0;
		prev->preview_track = _tr;

		// update start position for paused items
		// note: no need to mutex "prev" here as it is not started yet
		if (_pause)
		{
			for (int i=g_pausedPreviews.GetSize()-1; i>=0; i--)
			{
				if (g_pausedPreviews.Get(i)->IsMatching(prev))
				{
					prev->curpos = g_pausedPreviews.Get(i)->m_pos;
					g_pausedPreviews.Delete(i, true);
//					break;
				}
			}
		}

		// go!
		g_playPreviews.Add(prev);
		ok = (PlayTrackPreview2Ex(NULL, prev, !!_msi, _msi) != 0);
	}
	return ok;
}

// primitive func: _fn must be a valid/existing file
bool SNM_PlayTrackPreview(MediaTrack* _tr, const char* _fn, bool _pause, bool _loop, double _msi)
{
	if (PCM_source* src = PCM_Source_CreateFromFileEx(_fn, true)) // "true" so that the src is not imported as in-project data
		return SNM_PlayTrackPreview(_tr, src, _pause, _loop, _msi);
	return false;
}

void SNM_PlaySelTrackPreviews(const char* _fn, bool _pause, bool _loop, double _msi)
{
	TrackPreviewLockUnlockTracks(true);
	for (int i=1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			SNM_PlayTrackPreview(tr, _fn, _pause, _loop, _msi);
	}
	TrackPreviewLockUnlockTracks(false);
}

// returns true if something done
bool SNM_TogglePlaySelTrackPreviews(const char* _fn, bool _pause, bool _loop, double _msi)
{
	TrackPreviewLockUnlockTracks(true);

	bool done = false;
	WDL_PtrList<void> trsToStart;
	for (int j=1; j <= GetNumTracks(); j++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(j, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
			trsToStart.Add(tr);
	}

	{
		// stop play if needed, store if paused
		for (int i=g_playPreviews.GetSize()-1; i >=0; i--)
		{
			preview_register_t* prev = g_playPreviews.Get(i);
			for (int j=1; j <= GetNumTracks(); j++) // skip master
			{
				MediaTrack* tr = CSurf_TrackFromID(j, false);
				if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
				{
					TrackPreviewLockUnlockMutex(prev, true);
					if (prev->preview_track == tr && 
						prev->volume > 0.5 && // playing
						!_stricmp(prev->src->GetFileName(), _fn))
					{
						prev->loop = false;
						prev->volume = 0.0; // => will be stopped by next call to StopTrackPreviewsRun()
						if (_pause) g_pausedPreviews.Insert(0, new PausedPreview(_fn, tr, prev->curpos)); 
						trsToStart.Delete(trsToStart.Find(tr));
						done = true;
					}
					TrackPreviewLockUnlockMutex(prev, false);
				}
			}
		}
	}

	// start play if needed
	for (int i=0; i <= trsToStart.GetSize(); i++)
		if (MediaTrack* tr = (MediaTrack*)trsToStart.Get(i))
			done |= SNM_PlayTrackPreview(tr, _fn, _pause, _loop, _msi);

	TrackPreviewLockUnlockTracks(false);

	return done;
}

void StopTrackPreviews(bool _selTracksOnly)
{
	TrackPreviewLockUnlockTracks(true);
	for (int i=g_playPreviews.GetSize()-1; i >= 0; i--)
	{
		preview_register_t* prev = g_playPreviews.Get(i);
		for (int j=1; j <= GetNumTracks(); j++) // skip master
		{
			MediaTrack* tr = CSurf_TrackFromID(j, false);
			if (tr && (!_selTracksOnly ||*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL)))
			{
				TrackPreviewLockUnlockMutex(prev, true);

				if (prev->preview_track == tr) {
					prev->loop = false;
					prev->volume = 0.0; // => will be stopped by next call to StopTrackPreviewsRun()
				}

				for (int i=g_pausedPreviews.GetSize()-1; i>=0; i--)
					if (g_pausedPreviews.Get(i)->m_tr == tr)
						g_pausedPreviews.Delete(i, true);

				TrackPreviewLockUnlockMutex(prev, false);
			}
		}
	}
	TrackPreviewLockUnlockTracks(false);
}

void StopTrackPreviews(COMMAND_T* _ct) {
	StopTrackPreviews((int)_ct->user == 1);
}


///////////////////////////////////////////////////////////////////////////////
// Send CC123//CC64/CC120 on all channels
///////////////////////////////////////////////////////////////////////////////

class AllNoteOffSource : public PCM_source
{
public:
  AllNoteOffSource(int flags) { m_hasdone=0; m_flags=flags; }
  ~AllNoteOffSource() { }
  PCM_source *Duplicate() { return new AllNoteOffSource(m_flags); }
  bool SetFileName(const char *newfn) { return false; } 
  bool IsAvailable() { return true; }
  const char *GetType() { return "S&M_ALLNOTEOFFS"; };
  int GetNumChannels() { return 1; }
  double GetSampleRate() { return 0.0; }
  double GetLength() { return 10000000000000.0; }
  int PropertiesWindow(HWND hwndParent) { return -1; }
  void SaveState(ProjectStateContext *ctx) {}
  int LoadState(const char *firstline, ProjectStateContext *ctx) { return -1; }
  void Peaks_Clear(bool deleteFile) { }
  int PeaksBuild_Begin() { return 0; }
  int PeaksBuild_Run() { return 0; }
  void PeaksBuild_Finish() { }
  void GetPeakInfo(PCM_source_peaktransfer_t *block) { block->peaks_out=0; }
  
  void GetSamples(PCM_source_transfer_t *block)
  {
    block->samples_out=0;
    if (block->midi_events && !m_hasdone)
    {
      m_hasdone=1;
      block->samples_out=block->length;
      memset(block->samples,0,block->length*block->nch*sizeof(double));
      for (int x = 0; x < 16; x ++)
      {
        MIDI_event_t e={0, 3, {static_cast<unsigned char>(0xb0 + x)}};
        if (m_flags&1)
        {
          e.midi_message[1]=64;
          block->midi_events->AddItem(&e);
        }
        if (m_flags&2)
        {
          e.midi_message[1]=123;
          block->midi_events->AddItem(&e);
        }
        if (m_flags&4)
        {
          e.midi_message[1]=120;
          block->midi_events->AddItem(&e);
        }
      }
    }
  }
  
  int m_hasdone;
  int m_flags;
};

// _cc_flags&1=send cc#64 messages (reset sustain), &2=cc#123 (all notes off), &4=cc#120 (all sounds off)
bool SendAllNotesOff(WDL_PtrList<void>* _trs, int _cc_flags)
{
	const int trs_sz=_trs->GetSize();
	if (!_trs || !trs_sz) return false;

	int i, cnt = 0;
	WDL_PtrList_DeleteOnDestroy<preview_register_t> anos_prevs(DeleteTrackPreview);
	for (i=0; i<trs_sz; i++)
	{
		AllNoteOffSource *anos = new AllNoteOffSource(_cc_flags);

		preview_register_t *prev = anos_prevs.Add(new preview_register_t);
		memset(prev, 0, sizeof(preview_register_t));
		TrackPreviewInitDeleteMutex(prev, true);
		prev->src = anos;
		prev->m_out_chan = -1;
		prev->curpos = 0.0;
		prev->loop = false;
		prev->volume = 1.0;
		prev->preview_track = _trs->Get(i);

		if (PlayTrackPreview2Ex(NULL, prev, 0, 0.0)) cnt++;
		else anos->m_hasdone=true; // no locking needed here (not played)
  }

  // wait for all-notes-off to be sent before stopping
  int safety=0;
  int prev_cnt;
  while (safety<250 && (prev_cnt=anos_prevs.GetSize()))
  {
    for (i=prev_cnt-1; i>=0; i--)
    {
      preview_register_t *prev = anos_prevs.Get(i);
      AllNoteOffSource *anos = (AllNoteOffSource*)prev->src;

      // DeleteTrackPreview() will stop and delete the preview, its src, etc
      if (anos->m_hasdone) anos_prevs.Delete(i, true, DeleteTrackPreview);
    }
#ifdef _WIN32
    // keep the UI updating
    if (safety)
    {
      MSG msg;
      while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
      DispatchMessage(&msg);
    }
#endif
    Sleep(1);
    safety++;
  }
  return !!cnt;
}

// wrapping func
bool SendAllNotesOff(MediaTrack* _tr, int _cc_flags)
{
  if (_tr)
  {
    WDL_PtrList<void> trs; trs.Add(_tr);
    return SendAllNotesOff(&trs, _cc_flags);
  }
  return false;
}

void SendAllNotesOff(COMMAND_T* _ct)
{
	WDL_PtrList<void> trs;
	for (int j=1; j <= GetNumTracks(); j++) // skip master
		if (MediaTrack* tr = CSurf_TrackFromID(j, false))
			if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
				trs.Add(tr);
	SendAllNotesOff(&trs, (int)_ct->user);
}


///////////////////////////////////////////////////////////////////////////////
// ReaScript export
///////////////////////////////////////////////////////////////////////////////

bool SNM_AddTCPFXParm(MediaTrack* _tr, int _fxId, int _prmId)
{
	bool updated = false;
	if (_tr && _fxId>=0 && _prmId>=0 && _fxId<TrackFX_GetCount(_tr) && _prmId<TrackFX_GetNumParams(_tr, _fxId))
	{
		// already exists?
		int fxId, prmId;
		for (int i=0; i<CountTCPFXParms(NULL, _tr); i++)
			if (GetTCPFXParm(NULL, _tr, i, &fxId, &prmId))
				if (fxId==_fxId && prmId==_prmId)
					return false;

		SNM_ChunkParserPatcher p(_tr);

		// first get the fx chain: a straight search for the fx would fail (possible mismatch with frozen track, item fx, etc..)
		WDL_FastString chainChunk;
		if (p.GetSubChunk("FXCHAIN", 2, 0, &chainChunk, "<ITEM") > 0)
		{
			SNM_ChunkParserPatcher pfxc(&chainChunk, false);
			int pos = pfxc.Parse(SNM_GET_CHUNK_CHAR,1,"FXCHAIN","WAK",_fxId,0);
			if (pos>0)
			{
				char line[SNM_MAX_CHUNK_LINE_LENGTH] = "";
				if (snprintfStrict(line, sizeof(line), "PARM_TCP %d\n", _prmId) > 0)
				{
					pfxc.GetChunk()->Insert(line, --pos);
					if (p.ReplaceSubChunk("FXCHAIN", 2, 0, pfxc.GetChunk()->Get(), "<ITEM")) {
						updated = true;
						p.Commit(true);
					}
				}
			}
		}
	}
	return updated;
}


///////////////////////////////////////////////////////////////////////////////
// Misc. track helpers/actions
///////////////////////////////////////////////////////////////////////////////

void ScrollSelTrack(bool _tcp, bool _mcp)
{
	MediaTrack* tr = SNM_GetSelectedTrack(NULL, 0, true);
	if (tr && (_tcp || _mcp))
	{
		if (_tcp)
			Main_OnCommand(40913,0); // vertical scroll to selected tracks

		// REAPER BUG (last check v4.20):
		// SetMixerScroll() has no effect with most right tracks
		if (_mcp) 
			SetMixerScroll(tr);
	}
}

void ScrollSelTrack(COMMAND_T* _ct) {
	int flags = (int)_ct->user;
	ScrollSelTrack((flags&1)==1, (flags&2)==2); // == for warning C4800
}

//JFB change/restore sel programatically => not cool for control surfaces
void ScrollTrack(MediaTrack* _tr, bool _tcp, bool _mcp)
{
	if (_tr)
	{
		PreventUIRefresh(1);
		WDL_PtrList<MediaTrack> selTrs;
		SNM_GetSelectedTracks(NULL, &selTrs, true);
		SNM_SetSelectedTrack(NULL, _tr, true, true);
		ScrollSelTrack(true, false);
		SNM_SetSelectedTracks(NULL, &selTrs, true);
		PreventUIRefresh(-1);
		// restoration => no need to UpdateArrange();
	}
}

//JFB change/restore sel programatically => not cool for control surfaces
void ShowTrackRoutingWindow(MediaTrack* _tr)
{
	if (_tr)
	{
		PreventUIRefresh(1);
		WDL_PtrList<MediaTrack> selTrs;
		SNM_GetSelectedTracks(NULL, &selTrs, true);
		SNM_SetSelectedTrack(NULL, _tr, true, true);
		Main_OnCommand(40293, 0);
		SNM_SetSelectedTracks(NULL, &selTrs, true);
		PreventUIRefresh(-1);
	}
}

