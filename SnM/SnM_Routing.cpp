/******************************************************************************
/ SnM_Routing.cpp
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

#include <WDL/localize/localize.h>

///////////////////////////////////////////////////////////////////////////////
// Cut/copy/paste routings + track with routings
// Note: these functions/actions ignore routing envelopes
///////////////////////////////////////////////////////////////////////////////

// lazy here: WDL_PtrList_DOD would have been better but it requires too much updates...
WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> > g_sndTrackClipboard;
WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> > g_rcvTrackClipboard; 
WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> > g_sndClipboard;
WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> > g_rcvClipboard;

void FlushClipboard(WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> >* _clipboard)
{
	if (_clipboard)
		_clipboard->Empty(true);
}

void CopySendsReceives(bool _noIntra, WDL_PtrList<MediaTrack>* _trs,
		WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> >* _snds, 
		WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> >* _rcvs)
{
	FlushClipboard(_snds);
	FlushClipboard(_rcvs);

	for (int i=0; i < _trs->GetSize(); i++)
	{
		if (MediaTrack* tr = _trs->Get(i))
		{
			// copy sends
			if (_snds)
			{
				_snds->Add(new WDL_PtrList_DeleteOnDestroy<SNM_SndRcv>());

				int idx=0;
				MediaTrack* dest = (MediaTrack*)GetSetTrackSendInfo(tr, 0, idx, "P_DESTTRACK", NULL);
				while (dest)
				{
					if (!_noIntra || _trs->Find(dest)<0)
					{
						SNM_SndRcv* send = new SNM_SndRcv();
						if (send->FillIOFromReaper(tr, dest, 0, idx))
							_snds->Get(i)->Add(send);
					}
					dest = (MediaTrack*)GetSetTrackSendInfo(tr, 0, ++idx, "P_DESTTRACK", NULL);
				}
			}

			// copy receives
			if (_rcvs)
			{
				_rcvs->Add(new WDL_PtrList_DeleteOnDestroy<SNM_SndRcv>());

				int idx=0;
				MediaTrack* src = (MediaTrack*)GetSetTrackSendInfo(tr, -1, idx, "P_SRCTRACK", NULL);
				while (src)
				{
					if (!_noIntra || _trs->Find(src)<0)
					{
						SNM_SndRcv* rcv = new SNM_SndRcv();
						if (rcv->FillIOFromReaper(src, tr, -1, idx))
							_rcvs->Get(i)->Add(rcv);
					}
					src = (MediaTrack*)GetSetTrackSendInfo(tr, -1, ++idx, "P_SRCTRACK", NULL);
				}
			}
		}
	}
}

bool PasteSendsReceives(bool _send, MediaTrack* _tr, 
		WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> >* _ios, 
		int _trIdx, WDL_PtrList<SNM_ChunkParserPatcher>* _ps)
{
	bool updated = false;
	if (_tr && _ps && _ios && _ios->Get(_trIdx))
	{
		for (int j=0; j < _ios->Get(_trIdx)->GetSize(); j++)
		{
			SNM_SndRcv* io = _ios->Get(_trIdx)->Get(j);
			if (MediaTrack* tr = GuidToTrack(_send ? &io->m_dest : &io->m_src))
			{
				SNM_SendPatcher* p = (SNM_SendPatcher*)SNM_FindCPPbyObject(_ps, _send ? tr : _tr);
				if (!p) {
					p = new SNM_SendPatcher(_send ? tr : _tr); 
					_ps->Add(p);
				}
				updated |= p->AddReceive(_send ? _tr : tr, io);
			}
		}
	}
	return updated;
}

bool PasteSendsReceives(WDL_PtrList<MediaTrack>* _trs,
		WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> >* _snds, 
		WDL_PtrList_DeleteOnDestroy<WDL_PtrList_DeleteOnDestroy<SNM_SndRcv> >* _rcvs, 
		WDL_PtrList<SNM_ChunkParserPatcher>* _ps)
{
	bool updated = false;

	// a same track can be multi-patched
	// => patch everything in one go thanks to this list
	WDL_PtrList<SNM_ChunkParserPatcher>* ps = (_ps ? _ps : new WDL_PtrList<SNM_ChunkParserPatcher>);

	for (int i=0; i<_trs->GetSize(); i++)
	{
		// when the nb of copied tracks' routings == nb of dest tracks, paste them respectively
		if ((!_snds || _trs->GetSize()==_snds->GetSize()) &&
		    (!_rcvs || _trs->GetSize()==_rcvs->GetSize()))
		{
			updated |= PasteSendsReceives(true, _trs->Get(i), _snds, i, ps);
			updated |= PasteSendsReceives(false, _trs->Get(i), _rcvs, i, ps);
		}
		// otherwise: merge all copied routings into dest tracks
		//JFB TODO? "intra pasted routings" to manage?
		else
		{
			for (int j=0; _snds && j<_snds->GetSize(); j++)
				updated |= PasteSendsReceives(true, _trs->Get(i), _snds, j, ps);
			for (int j=0; _rcvs && j<_rcvs->GetSize(); j++)
				updated |= PasteSendsReceives(false, _trs->Get(i), _rcvs, j, ps);
		}
	}
	if (!_ps) ps->Empty(true); // + auto commit if needed
	return updated;
}

void CopyWithIOs(COMMAND_T* _ct)
{
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize()) {
		CopySendsReceives(true, &trs, &g_sndTrackClipboard, &g_rcvTrackClipboard); // true: do no copy routings between sel. tracks
		Main_OnCommand(40210, 0); // copy sel tracks
	}
}

void CutWithIOs(COMMAND_T* _ct)
{
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize()) 
	{
		Undo_BeginBlock2(NULL);
		CopySendsReceives(true, &trs, &g_sndTrackClipboard, &g_rcvTrackClipboard); // true: do no copy routings between sel. tracks
		Main_OnCommand(40337, 0); // cut sel tracks
		Undo_EndBlock2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL);
	}
}

void PasteWithIOs(COMMAND_T* _ct)
{
	if (int iTracks = GetNumTracks())
	{
		Undo_BeginBlock2(NULL);
		Main_OnCommand(40058, 0); // native track paste - note: this only preserves routings between pasted tracks
		if (iTracks != GetNumTracks()) // new/pasted tracks?
		{
			WDL_PtrList<MediaTrack> trs;
			SNM_GetSelectedTracks(NULL, &trs, false);
			PasteSendsReceives(&trs, &g_sndTrackClipboard, &g_rcvTrackClipboard, NULL); // false: we keep intra routings between pasted tracks, see above
		}
		Undo_EndBlock2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL);
	}
}

// routing cut copy/paste
void CopyRoutings(COMMAND_T* _ct)
{
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize())
		CopySendsReceives(false, &trs, &g_sndClipboard, &g_rcvClipboard);
}

void CutRoutings(COMMAND_T* _ct)
{
	bool updated = false;
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize())
	{
		WDL_PtrList<SNM_ChunkParserPatcher> ps;
		CopySendsReceives(false, &trs, &g_sndClipboard, &g_rcvClipboard);
		updated |= RemoveSends(&trs, &ps);
		updated |= RemoveReceives(&trs, &ps);
		ps.Empty(true); // auto-commit, if needed
	}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

void PasteRoutings(COMMAND_T* _ct)
{
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize() && PasteSendsReceives(&trs, &g_sndClipboard, &g_rcvClipboard, NULL))
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

// sends cut copy/paste
void CopySends(COMMAND_T* _ct)
{
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize())
		CopySendsReceives(false, &trs, &g_sndClipboard, NULL);
}

void CutSends(COMMAND_T* _ct)
{
	bool updated = false;
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize()) {
		CopySendsReceives(false, &trs, &g_sndClipboard, NULL);
		updated |= RemoveSends(&trs, NULL);
	}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

void PasteSends(COMMAND_T* _ct)
{
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize() && PasteSendsReceives(&trs, &g_sndClipboard, NULL, NULL))
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

// receives cut copy/paste
void CopyReceives(COMMAND_T* _ct)
{
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize())
		CopySendsReceives(false, &trs, NULL, &g_rcvClipboard);
}

void CutReceives(COMMAND_T* _ct)
{
	bool updated = false;
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize()) {
		CopySendsReceives(false, &trs, NULL, &g_rcvClipboard);
		updated |= RemoveReceives(&trs, NULL);
	}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

void PasteReceives(COMMAND_T* _ct)
{
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize() && PasteSendsReceives(&trs, NULL, &g_rcvClipboard, NULL))
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}


///////////////////////////////////////////////////////////////////////////////
// Remove routing
///////////////////////////////////////////////////////////////////////////////

// primitive
bool RemoveSends(WDL_PtrList<MediaTrack>* _trs, WDL_PtrList<SNM_ChunkParserPatcher>* _ps)
{
	bool updated = false;
	WDL_PtrList<SNM_ChunkParserPatcher>* ps = (_ps ? _ps : new WDL_PtrList<SNM_ChunkParserPatcher>);
	for (int j=1; j <= GetNumTracks(); j++) // skip master
	{
		if (MediaTrack* trSrc = CSurf_TrackFromID(j, false))
		{
			SNM_SendPatcher* p = (SNM_SendPatcher*)SNM_FindCPPbyObject(ps, trSrc);
			if (!p) {
				p = new SNM_SendPatcher(trSrc); 
				ps->Add(p);
			}
			for (int i=0; i < _trs->GetSize(); i++)
//				if (_trs->Get(i) != trSrc)
					updated |= (p->RemoveReceivesFrom(_trs->Get(i)) > 0);
		}
	}
	if (!_ps) ps->Empty(true); // auto-commit if needed
	return updated;
}

void RemoveSends(COMMAND_T* _ct)
{
	bool updated = false;
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize())
		updated = RemoveSends(&trs, NULL);
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

// primitive
bool RemoveReceives(WDL_PtrList<MediaTrack>* _trs, WDL_PtrList<SNM_ChunkParserPatcher>* _ps)
{
	bool updated = false;
	WDL_PtrList<SNM_ChunkParserPatcher>* ps = (_ps ? _ps : new WDL_PtrList<SNM_ChunkParserPatcher>);
	for (int i=0; i < _trs->GetSize(); i++)
		if (MediaTrack* tr = _trs->Get(i))
		{
			SNM_SendPatcher* p = (SNM_SendPatcher*)SNM_FindCPPbyObject(ps, tr);
			if (!p) {
				p = new SNM_SendPatcher(tr); 
				ps->Add(p);
			}
			updated |= (p->RemoveReceives() > 0); 
		}
	if (!_ps) ps->Empty(true); // auto-commit if needed
	return updated;
}

void RemoveReceives(COMMAND_T* _ct)
{
	bool updated = false;
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize())
		updated = RemoveReceives(&trs, NULL);
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

void RemoveRoutings(COMMAND_T* _ct)
{
	bool updated = false;
	WDL_PtrList<MediaTrack> trs;
	SNM_GetSelectedTracks(NULL, &trs, false);
	if (trs.GetSize())
	{
		WDL_PtrList<SNM_ChunkParserPatcher> ps;
		updated |= RemoveSends(&trs, &ps);
		updated |= RemoveReceives(&trs, &ps);
		ps.Empty(true); // auto-commit, if needed
	}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1); 
}


///////////////////////////////////////////////////////////////////////////////
// ReaScript export
///////////////////////////////////////////////////////////////////////////////

// _type -1=Default type (user preferences), 0=Post-Fader (Post-Pan), 1=Pre-FX, 2=deprecated, 3=Pre-Fader (Post-FX)
bool SNM_AddReceive(MediaTrack* _srcTr, MediaTrack* _destTr, int _type)
{
	bool ok=false;
	PreventUIRefresh(1);
	if (_srcTr && _destTr && _srcTr!=_destTr && _type<=3)
	{
		int idx=CreateTrackSend(_srcTr, _destTr);
		if (idx>=0 && _type>=0) GetSetTrackSendInfo(_srcTr, 0, idx, "I_SENDMODE", (void*)&_type);
		ok = (idx>=0);
	}
	PreventUIRefresh(-1);
	return ok;
}

bool SNM_RemoveReceive(MediaTrack* _tr, int _rcvIdx)
{
	return RemoveTrackSend(_tr, -1, _rcvIdx);
}

bool SNM_RemoveReceivesFrom(MediaTrack* _tr, MediaTrack* _srcTr)
{
	bool updated = false;
	if (const int count = GetTrackNumSends(_tr, -1))
	{
		MediaTrack* src;
		int idx = count-1;
    
		PreventUIRefresh(1);
		while (idx>=0 && (src = (MediaTrack*)GetSetTrackSendInfo(_tr, -1, idx, "P_SRCTRACK", NULL)))
		{
			if (src == _srcTr) updated |= RemoveTrackSend(_tr, -1, idx);
			idx--;
		}
		PreventUIRefresh(-1);
	}
	return updated;
}


///////////////////////////////////////////////////////////////////////////////
// Other routing funcs/commands
// Note: the pref "defsendflag" also includes the mode (post/pre fader, etc..)
///////////////////////////////////////////////////////////////////////////////

int g_defSndFlags = -1;

void SaveDefaultTrackSendPrefs(COMMAND_T*) {
	if (const ConfigVar<int> flags = "defsendflag")
		g_defSndFlags = *flags;
}

void RecallDefaultTrackSendPrefs(COMMAND_T*) {
	if (g_defSndFlags>=0)
		ConfigVar<int>("defsendflag").try_set(g_defSndFlags);
}

// flags != _ct->user because of the weird state of midi+audio which is not 0x300 but 0 (!)
void SetDefaultTrackSendPrefs(COMMAND_T* _ct) {
	if (ConfigVar<int> flags = "defsendflag") {
		switch((int)_ct->user) {
			case 0: *flags&=0xFF; break; // both
			case 1: *flags&=0xF0FF; *flags|=0x100; break; // audio
			case 2: *flags&=0xF0FF; *flags|=0x200; break; // midi
		}
	}
}

// primitive (no undo)
bool MuteSends(MediaTrack* _src, MediaTrack* _dest, bool _mute)
{
	bool updated = false;
	if (_src && _dest && _src!=_dest)
	{
		int sndIdx=0;
		MediaTrack* destTr = (MediaTrack*)GetSetTrackSendInfo(_src, 0, sndIdx, "P_DESTTRACK", NULL);
		while(destTr)
		{
			if (destTr==_dest && _mute != *(bool*)GetSetTrackSendInfo(_src, 0, sndIdx, "B_MUTE", NULL)) {
				GetSetTrackSendInfo(_src, 0, sndIdx, "B_MUTE", &_mute);
				updated = true;
			}
			destTr = (MediaTrack*)GetSetTrackSendInfo(_src, 0, ++sndIdx, "P_DESTTRACK", NULL);
		}
	}
	return updated;
}

// primitive (no undo)
bool MuteReceives(MediaTrack* _src, MediaTrack* _dest, bool _mute)
{
	bool updated = false;
	if (_src && _dest && _src!=_dest)
	{
		int rcvIdx=0;
		MediaTrack* src = (MediaTrack*)GetSetTrackSendInfo(_dest, -1, rcvIdx, "P_SRCTRACK", NULL);
		while(src)
		{
			if (src == _src && _mute != *(bool*)GetSetTrackSendInfo(_dest, -1, rcvIdx, "B_MUTE", NULL)) {
				GetSetTrackSendInfo(_dest, -1, rcvIdx, "B_MUTE", &_mute);
				updated = true;
			}
			src = (MediaTrack*)GetSetTrackSendInfo(_dest, -1, ++rcvIdx, "P_SRCTRACK", NULL);
		}
	}
	return updated;
}

bool HasReceives(MediaTrack* _src, MediaTrack* _dest)
{
	if (_src && _dest && _src != _dest)
	{
		int rcvIdx=0;
		MediaTrack* src = (MediaTrack*)GetSetTrackSendInfo(_dest, -1, rcvIdx, "P_SRCTRACK", NULL);
		while(src)
		{
			if (src == _src)
				return true;
			src = (MediaTrack*)GetSetTrackSendInfo(_dest, -1, ++rcvIdx, "P_SRCTRACK", NULL);
		}
	}
	return false;
}
