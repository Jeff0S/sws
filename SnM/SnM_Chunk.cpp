/******************************************************************************
/ SnM_Chunk.cpp
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


// Some "SAX-ish like" parser classes inheriting SNM_ChunkParserPatcher


#include "stdafx.h" 
#include "SnM.h"
#include "SnM_Chunk.h"
#include "SnM_FXChain.h"
#include "SnM_Routing.h"
#include "SnM_Track.h"


///////////////////////////////////////////////////////////////////////////////
// SNM_ChunkIndenter
///////////////////////////////////////////////////////////////////////////////

bool SNM_ChunkIndenter::NotifyChunkLine(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents,
		WDL_FastString* _newChunk, int _updates)
{
	bool update = false;
	if (_mode == -1)
	{
		int depth = _parsedParents->GetSize();
		if (depth)
		{
			if (*_parsedLine == '<')
				depth--;
			for (int i=0; i < depth; i++)
				_newChunk->Append("  ");
		}
		_newChunk->Append(_parsedLine);
		_newChunk->Append("\n");
		update = true;
	}
	return update;
}


///////////////////////////////////////////////////////////////////////////////
// SNM_SendPatcher
// no NotifySkippedSubChunk() needed: single lines (not sub-chunks)
///////////////////////////////////////////////////////////////////////////////

bool SNM_SendPatcher::NotifyChunkLine(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos, 
	int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents,  
	WDL_FastString* _newChunk, int _updates)
{
	bool update = false;
	switch(_mode)
	{
		// add rcv
		case -1:
		{
			int defSndFlags = *ConfigVar<int>("defsendflag");
			bool audioSnd = ((defSndFlags & 512) != 512);
			bool midiSnd =  ((defSndFlags & 256) != 256);
			_newChunk->AppendFormatted(
				SNM_MAX_CHUNK_LINE_LENGTH,
				"AUXRECV %d %d %s %s 0 0 0 %d 0 -1.00000000000000 %d -1\n%s\n", 
				m_srcId-1, m_sendType, m_vol, m_pan, audioSnd?0:-1, midiSnd?0:31, _parsedLine);
			update = true;
			m_breakParsePatch = true;
		}
		break;

		// remove rcv
		case -2:
			update = (m_srcId > 0 && _lp->gettoken_int(1) == (m_srcId - 1));
			// update unchanged, i.e. we do not re-copy this receive
		break;

		// add "detailed" receive
		case -3:
		{
			SNM_SndRcv* mSndRcv = (SNM_SndRcv*)m_sndRcv;
			_newChunk->AppendFormatted(
				SNM_MAX_CHUNK_LINE_LENGTH,
				"AUXRECV %d %d %.14f %.14f %d %d %d %d %d %.14f %d %d\n%s\n", 
				m_srcId-1, 
				m_sendType, 
				mSndRcv->m_vol, 
				mSndRcv->m_pan,
				mSndRcv->m_mute,
				mSndRcv->m_mono,
				mSndRcv->m_phase,
				mSndRcv->m_srcChan,
				mSndRcv->m_destChan,
				mSndRcv->m_panl,
				mSndRcv->m_midi,
				-1, // API LIMITATION: cannot get snd/rcv automation
				_parsedLine);
			update = true;
			m_breakParsePatch = true;
		}
		break;
	}
	return update; 
}

int SNM_SendPatcher::AddReceive(MediaTrack* _srcTr, int _sendType, const char* _vol, const char* _pan) 
{
	m_srcId = _srcTr ? CSurf_TrackToID(_srcTr, false) : -1;
	if (m_srcId <= 0)
		return 0; 
	m_sendType = _sendType;
	m_vol = _vol;
	m_pan = _pan;
	m_sndRcv = NULL;
	return ParsePatch(-1, 1, "TRACK", "MIDIOUT");
}

bool SNM_SendPatcher::AddReceive(MediaTrack* _srcTr, void* _io)
{
	m_srcId = _srcTr ? CSurf_TrackToID(_srcTr, false) : -1;
	if (!_io || m_srcId <= 0)
		return false; 
	m_sendType = ((SNM_SndRcv*)_io)->m_mode;
	m_vol = NULL;
	m_pan = NULL;
	m_sndRcv = _io;
	return (ParsePatch(-3, 1, "TRACK", "MIDIOUT") > 0);
}

int SNM_SendPatcher::RemoveReceives() {
/* can fail since v4.1: freeze
	return RemoveLines("AUXRECV");
*/
	return RemoveLine("TRACK", "AUXRECV", 1, -1, "MIDIOUT");
	// REAPER will remove related envelopes, if any
}

int SNM_SendPatcher::RemoveReceivesFrom(MediaTrack* _srcTr) 
{
	m_srcId = _srcTr ? CSurf_TrackToID(_srcTr, false) : -1;
	if (m_srcId <= 0)
		return 0;
/* can fail since v4.1: freeze
	char buf[32];
	return snprintfStrict(buf, sizeof(buf), "AUXRECV %d", srcId-1)>0 && RemoveLines(buf);
*/
	return ParsePatch(-2, 1, "TRACK", "AUXRECV", -1, -1, NULL, NULL, "MIDIOUT");
	// REAPER will remove related envelopes, if any
}


///////////////////////////////////////////////////////////////////////////////
// SNM_FXChainTakePatcher
///////////////////////////////////////////////////////////////////////////////

bool SNM_FXChainTakePatcher::NotifyStartElement(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos, 
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	// start to *not* recopy
	if ((_mode == -1 || (m_activeTake && _mode == -2)) && !strcmp(GetParent(_parsedParents), "TAKEFX"))
	{
		m_removingTakeFx = true;
	}
	else if (_mode == -3 && m_activeTake && !strcmp(GetParent(_parsedParents), "TAKEFX"))
	{
		m_copyingTakeFx = true;
	}
	return m_removingTakeFx; 
}


bool SNM_FXChainTakePatcher::NotifyEndElement(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos, 
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	bool update = m_removingTakeFx;
	if ((_mode == -1 || (m_activeTake && _mode == -2)) && !strcmp(GetParent(_parsedParents), "SOURCE")) 
	{
		// if there's a chain to add, add it!
		if (m_fxChain != NULL)
		{
			update=true;
			_newChunk->Append(">\n");// ie write eof "SOURCE"
			MakeChunkTakeFX(_newChunk, m_fxChain);
		}
	}
	else if (m_removingTakeFx && 
		(_mode == -1 || (m_activeTake && _mode == -2)) && !strcmp(GetParent(_parsedParents), "TAKEFX")) 
	{
		m_removingTakeFx = false; // re-enable copy
		// note: "update" stays true if m_removingFxChain was initially true, 
		// i.e. we don't copy the end of chunk ">"
	}
	else if (_mode == -3 && m_activeTake && !strcmp(GetParent(_parsedParents), "TAKEFX")) 
	{
		m_copyingTakeFx = false;
	}
	return update;
}

// _mode: -1 set all takes FX chain, -2 set active take's FX chain, -3 copy active take's FX chain
bool SNM_FXChainTakePatcher::NotifyChunkLine(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	bool update = m_removingTakeFx;

	// "tag" the active take (m_activeTake must be initialized to true if the 1st take is active)
	if((_mode == -2 || _mode == -3) && !strcmp(_lp->gettoken_str(0), "TAKE"))
		m_activeTake = (_lp->getnumtokens() > 1 && !strcmp(_lp->gettoken_str(1), "SEL")); //JFB v4: gettoken_str(1) indirectly excludes TAKE NULL SEL

	// copy active FX chain
	if (_mode == -3 && m_copyingTakeFx)
	{
		// "<TAKEFX" not part of .RfxChain files
		if (strcmp(_lp->gettoken_str(0), "<TAKEFX") != 0) 
		{
			m_copiedFXChain.Append(_parsedLine);
			m_copiedFXChain.Append("\n");
		}
	}
	return update; 
}

bool SNM_FXChainTakePatcher::NotifySkippedSubChunk(int _mode, 
	const char* _subChunk, int _subChunkLength, int _subChunkPos,
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	if (_mode == -3 && m_copyingTakeFx)
		m_copiedFXChain.Insert(_subChunk, m_copiedFXChain.GetLength(), _subChunkLength);
	return m_removingTakeFx; 
}

// _fxChain == NULL clears current FX chain(s)
bool SNM_FXChainTakePatcher::SetFXChain(WDL_FastString* _fxChain, bool _activeTakeOnly)
{
	m_fxChain = _fxChain;
	m_removingTakeFx = false;
	m_activeTake = (*(int*)GetSetMediaItemInfo((MediaItem*)m_reaObject, "I_CURTAKE", NULL) == 0);
	// ParsePatch(): cannot specify more parameters, the parser has to go in depth
	return (ParsePatch((_activeTakeOnly && GetMediaItemNumTakes((MediaItem*)m_reaObject) > 1) ? -2 : -1) > 0);
	return false;
}

WDL_FastString* SNM_FXChainTakePatcher::GetFXChain()
{
	m_activeTake = (*(int*)GetSetMediaItemInfo((MediaItem*)m_reaObject, "I_CURTAKE", NULL) == 0);
	m_copiedFXChain.Set("");
	m_copyingTakeFx = false;
	// Parse(): cannot specify more parameters, the parser has to go in depth
	if (Parse(-3) >= 0 && // cannot check > 0 as usual due to *READ-ONLY* custom negative mode
		m_copiedFXChain.GetLength()) // .. here's the check
	{
		return &m_copiedFXChain;
	}
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// SNM_FXChainTrackPatcher
///////////////////////////////////////////////////////////////////////////////

bool SNM_FXChainTrackPatcher::NotifyStartElement(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	const char* parent = GetParent(_parsedParents);
	// start to *not* recopy
	m_removingFxChain |= ((_mode == -1 && !strcmp(parent, "FXCHAIN")) || (_mode == -2 && !strcmp(parent, "FXCHAIN_REC")));
	return m_removingFxChain; 
}

bool SNM_FXChainTrackPatcher::NotifyEndElement(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	bool update = m_removingFxChain;
	const char* parent = GetParent(_parsedParents);
	if (m_removingFxChain && ((_mode == -1 && !strcmp(parent, "FXCHAIN")) || (_mode == -2 && !strcmp(parent, "FXCHAIN_REC"))))
	{
		m_removingFxChain = false; // re-enable copy
		m_breakParsePatch = true; // optmization
	}
	return update; 

	// note: "update" is true if m_removingFxChain was initially true, 
	//       i.e. do not recopy end of "FXCHAIN" chunk '>'
}

// _mode -1: set FX chain, -2: set input FX chain
bool SNM_FXChainTrackPatcher::NotifyChunkLine(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	bool update = m_removingFxChain;

	// we rely on REAPER tolerance here: we patch right after "MAINSEND" even if track volume
	// pan, etc.. envelopes are normally defined there first (before FX chains). also relies
	// on REAPER's tolerance about input and standard FX chains order.
	if((_mode == -1 || _mode == -2) && !strcmp(_lp->gettoken_str(0), "MAINSEND"))
	{
		update=true;
		_newChunk->Append(_parsedLine); // write the "MAINSEND" line
		_newChunk->Append("\n");
		_mode == -1 ? _newChunk->Append("<FXCHAIN\n") : _newChunk->Append("<FXCHAIN_REC\n");
//			_newChunk->Append("WNDRECT 0 0 0 0\n"); 
		_newChunk->Append("SHOW 0\n"); // un-float fx chain window
		_newChunk->Append("LASTSEL 1\n");
		_newChunk->Append("DOCKED 0\n");
		if (m_fxChain)
			_newChunk->Append(m_fxChain);
		_newChunk->Append(">\n");
	}
	return update; 
}

bool SNM_FXChainTrackPatcher::NotifySkippedSubChunk(int _mode, 
	const char* _subChunk, int _subChunkLength, int _subChunkPos,
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	return m_removingFxChain;
}

// if _fxChain == NULL then clears the current FX chain
bool SNM_FXChainTrackPatcher::SetFXChain(WDL_FastString* _fxChain, bool _inputFX)
{
	m_fxChain = _fxChain;
	m_removingFxChain = false;

	// Parse(): cannot specify more parameters, the parser has to go in depth
	return (ParsePatch(_inputFX ? -2 : -1) > 0); 
}

int SNM_FXChainTrackPatcher::GetInputFXCount() {
	return Parse(SNM_COUNT_KEYWORD, 2, "FXCHAIN_REC", "WAK", -1, -1, NULL, NULL, "<ITEM");
}


///////////////////////////////////////////////////////////////////////////////
// SNM_TakeParserPatcher
///////////////////////////////////////////////////////////////////////////////

// _pos: if valid, it'll be set to the start position of the _gettedChunk (if found, -1 otherwise)
// return false if _takeIdx not found (e.g. empty *item*)
bool SNM_TakeParserPatcher::GetTakeChunk(int _takeIdx, WDL_FastString* _gettedChunk, int* _pos, int* _len)
{
	int pos, len;
	bool found = GetTakeChunkPos(_takeIdx, &pos, &len); // indirect call to GetChunk() (force cache + add fake 1st take if needed)
	if (found && _gettedChunk)
	{
		_gettedChunk->Set((const char*)(m_chunk->Get()+pos), len);
		if (_pos) *_pos = pos;
		if (_len) *_len = len;
	}
	return found;
}

// different from the API's CountTakes(MediaItem*), this method deals with the chunk
int SNM_TakeParserPatcher::CountTakesInChunk()
{
	if (m_currentTakeCount < 0)
	{
		m_currentTakeCount = 0;
		const char* p = strstr(GetChunk()->Get(), "\nTAKE"); // force GetChunk() (force cache + add fake 1st take if needed)
		while (p) 
		{
			if (IsValidTakeChunkLine(p))
				m_currentTakeCount++;
			p = strstr((char*)(p+1), "\nTAKE");
		}
	}
	return m_currentTakeCount;
}

// different from checking a PCM source: this method deals with the chunk
bool SNM_TakeParserPatcher::IsEmpty(int _takeIdx)
{
	WDL_FastString tkChunk;
	if (GetTakeChunk(_takeIdx, &tkChunk))
		return (
			!strncmp(tkChunk.Get(), "TAKE NULL", 9) || // empty take
			strstr(tkChunk.Get(), "<SOURCE EMPTY")); // take with an empty source
	return false;
}

// assumes that _tkChunk begins with "TAKE" 
// returns the end position after insertion (or -1 if failed)
int SNM_TakeParserPatcher::AddLastTake(WDL_FastString* _tkChunk)
{
	int afterPos = -1;
	if (_tkChunk && _tkChunk->GetLength() && GetChunk()) // force GetChunk() (cache + add fake 1st take if needed)
	{
		m_chunk->Insert(_tkChunk->Get(), m_chunk->GetLength()-2, _tkChunk->GetLength()); //-2: before ">\n"
		afterPos = m_chunk->GetLength()-2;
		m_currentTakeCount++; // *always* reflect the nb of takes in the *chunk*
		m_updates++; // as we're directly working on the cached chunk..
	}
	return afterPos;
}

// assumes _chunk always begins with "TAKE"
// _pos: start pos of the take if known, for optimization (-1 if unknown)
// returns the end position after insertion (or -1 if failed)
int SNM_TakeParserPatcher::InsertTake(int _takeIdx, WDL_FastString* _chunk, int _pos)
{
	int afterPos = -1;
	int length = _chunk ? _chunk->GetLength() : 0;
	if (length && GetChunk()) // force GetChunk() (force cache + add fake 1st take if needed)
	{
		// last pos?
		if (_takeIdx >= CountTakesInChunk())
		{
			afterPos = AddLastTake(_chunk);
		}
		// other pos
		else 
		{
			int pos = _pos;
			if (pos < 0)
				GetTakeChunkPos(_takeIdx, &pos);

			if (pos >= 0)
			{
				m_chunk->Insert(_chunk->Get(), pos);
				afterPos = pos + length;
				m_currentTakeCount++; // *always* reflect the nb of takes in the *chunk*
				m_updates++; // as we're directly working on the cached chunk..
			}
		}
	}
	return afterPos;
}

// usually, when only 1 take remains, DeleteTrackMediaItem() should be called but
// this method will also work (=> empty item w/o takes)
bool SNM_TakeParserPatcher::RemoveTake(int _takeIdx, WDL_FastString* _removedChunk, int* _removedStartPos)
{
	bool updated = false;
	int pos, len;
	if (GetTakeChunkPos(_takeIdx, &pos, &len)) // indirect call to GetChunk() (force cache + add fake 1st take if needed)
	{
		if (_removedChunk)
			_removedChunk->Set((const char*)(m_chunk->Get()+pos), len);
		if (_removedStartPos)
			*_removedStartPos = pos;

		m_chunk->DeleteSub(pos, len);
		m_updates++; // as we're directly working on the cached chunk..
		updated = true;
		m_currentTakeCount--;
	}
	return updated;
}

// assumes _newTakeChunk always begins with "TAKE"
bool SNM_TakeParserPatcher::ReplaceTake(int _startTakePos, int _takeLength, WDL_FastString* _newTakeChunk)
{
	bool updated = false;
	if (GetChunk() && _newTakeChunk && _startTakePos >= 0) // force GetChunk() (force cache + add fake 1st take if needed)
	{
		int prevLgth = GetChunk()->GetLength();
		GetChunk()->DeleteSub(_startTakePos, _takeLength);
		m_updates++; // as we're directly working on the cached chunk..
		updated = true;
		m_currentTakeCount--;
		if (prevLgth > GetChunk()->GetLength()) // see WDL_FastString.DeleteSub()
		{
			GetChunk()->Insert(_newTakeChunk->Get(), _startTakePos, _newTakeChunk->GetLength());
			m_updates++; 
			m_currentTakeCount++;
		}
	}
	return updated;
}

// Overrides SNM_ChunkParserPatcher::GetChunk() in order to add a fake "TAKE" line.
// This simplifies all chunk processings: we'll then have as much takes as there are
// lines starting with "TAKE ..." in the item chunk (since v4 and its NULL takes, the 1st 
// take in a chunk can begin with a "TAKE" line and not only with a "NAME" line anymore)
WDL_FastString* SNM_TakeParserPatcher::GetChunk()
{
	WDL_FastString* chunk = SNM_ChunkParserPatcher::GetChunk();
	if (!m_fakeTake && chunk)
	{
		m_fakeTake = true;
		const char* p = strstr(m_chunk->Get(), "\nNAME ");
		// empty item (i.e. no take at all) or NULL takes only
		if (!p) 
		{
			p = strstr(m_chunk->Get(), "\nTAKE");
			if (p)
				m_chunk->Insert("\nTAKE NULL", (int)(p-m_chunk->Get()));
			else
				m_chunk->Insert("TAKE NULL\n", m_chunk->GetLength()-2); // -2 for ">\n"
		}
		// "normal" item
		else
		{
			m_chunk->Insert("\nTAKE", (int)(p-m_chunk->Get()));

			// if there was a "TAKE" before "NAME" it must be "TAKE NULL"
			p--; // we assume it's safe (serious other issues otherwise)
			while (*p && p > m_chunk->Get() && *p != '\n') p--;
			if (!strncmp(p, "\nTAKE", 5) && strncmp(p, "\nTAKE NULL", 10))
				m_chunk->Insert(" NULL", (int)(p+5-m_chunk->Get()));
		}
	}
	return chunk;
}

// Overrides SNM_ChunkParserPatcher::Commit() in order to remove the fake "TAKE" line, see
// GetChunk() comments. Also see important comments for SNM_ChunkParserPatcher::Commit()
bool SNM_TakeParserPatcher::Commit(bool _force)
{
	if (m_reaObject && (m_updates || _force) && m_chunk->GetLength() && !(GetPlayStateEx(NULL) & 4))
	{
// SNM_ChunkParserPatcher::Commit() mod ----->
		if (m_fakeTake)
		{
			m_fakeTake = false;
			const char* p = strstr(m_chunk->Get(), "\nNAME ");
			// empty item (i.e. no take at all) or NULL takes only
			if (!p) 
			{
				p = strstr(m_chunk->Get(), "\nTAKE");
				if (p)
				{
					const char* p2 = p+5;
					while (*p2 && p2 < (m_chunk->Get()+m_chunk->GetLength()) && *p2 != '\n') p2++;
					if (*p2 == '\n')
						m_chunk->DeleteSub((int)(p-m_chunk->Get()), (int)(p2-p));
				}
			}
			// "normal" item
			else
			{
				char* p2 = (char*)(p-1);
				while (*p2 && p2 > m_chunk->Get() && *p2 != '\n') p2--;
				if (*p2 == '\n' && !strncmp(p2, "\nTAKE", 5))
				{
					m_chunk->DeleteSub((int)(p2-m_chunk->Get()), (int)(p-p2));
					p -= (int)(p-p2);
				}

				// if there's a "TAKE" before "NAME" it must not be "TAKE NULL"
				p--; // we assume it's safe (serious other issues otherwise)
				while (*p && p > m_chunk->Get() && *p != '\n') p--;
				if (!strncmp(p, "\nTAKE NULL", 10))
					m_chunk->DeleteSub((int)(p+5-m_chunk->Get()), 5); // removes " NULL"
			}
		}
// SNM_ChunkParserPatcher::Commit() mod <-----

		if (!SNM_GetSetObjectState(m_reaObject, m_chunk)) {
			SetChunk("", 0);
			return true;
		}
	}
	return false;
}

// return false if _takeIdx not found (e.g. empty *item*)
bool SNM_TakeParserPatcher::GetTakeChunkPos(int _takeIdx, int* _pos, int* _len)
{
	int tkCount = 0;
	const char* p = strstr(GetChunk()->Get(), "\nTAKE"); // force GetChunk() (+ add fake 1st take if needed)
	while (p)
	{
		if (IsValidTakeChunkLine(p))
		{
			if (tkCount == _takeIdx)
			{
				// is there a next take ?
				const char* p2 = strstr(p+1, "\nTAKE");
				while (p2) 
				{
					if (IsValidTakeChunkLine(p2)) break;
					p2 = strstr(p2+1, "\nTAKE");
				}

				*_pos = (int)(p+1 - m_chunk->Get()); 
				if (_len)
				{
					if (p2 && !strncmp(p2, "\nTAKE", 5)) *_len = (int)(p2-p);
					// it's the last take
					else  *_len = strlen(p+1)-2; // -2 for final ">\n"
				}
				return true;
			}
			tkCount++;
		}
		p = strstr(p+1, "\nTAKE");
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// SNM_RecPassParser
// Inherits SNM_TakeParserPatcher in order to ease processing, see header
///////////////////////////////////////////////////////////////////////////////

bool SNM_RecPassParser::NotifyChunkLine(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	if (_mode == -1)
	{
		// Ok because SNM_RecPassParser inherits SNM_TakeParserPatcher
		// in order to insert a fake "TAKE"
		if (!strcmp(_lp->gettoken_str(0), "TAKE"))
		{
			m_takeCounter++;
		}
		else if (!strcmp(_lp->gettoken_str(0), "RECPASS"))
		{
			int success, currentRecPass = _lp->gettoken_int(1, &success);
			if (success)
			{
				m_maxRecPass = max(currentRecPass, m_maxRecPass);
				// check if that RECPASS isn't duplicated
				bool duplicated = false;
				for (int i=0; i < m_takeCounter; i++) {
					if (m_recPasses[i] == currentRecPass) {
						duplicated = true;
						break;
					}
				}

				if (!duplicated)
					m_recPasses[m_takeCounter-1] = currentRecPass;
			}
		}
		else if (!strcmp(_lp->gettoken_str(0), "TAKECOLOR"))
		{
			int success, currentColor = _lp->gettoken_int(1, &success);
			if (success)
				m_takeColors[m_takeCounter-1] = currentColor;
		}
	}
	return false;
}

int SNM_RecPassParser::GetMaxRecPass(int* _recPasses, int* _takeColors)
{
	m_maxRecPass = -1;
	m_takeCounter = 0;
	memset(m_recPasses, 0, SNM_RECPASSPARSER_MAX_TAKES * sizeof(int));
	memset(m_takeColors, 0, SNM_RECPASSPARSER_MAX_TAKES * sizeof(int));
	if (Parse(-1, 1, "ITEM") >= 0)
	{
		if (_recPasses)
			memcpy(_recPasses, m_recPasses, SNM_RECPASSPARSER_MAX_TAKES * sizeof(int));
		if (_takeColors)
			memcpy(_takeColors, m_takeColors, SNM_RECPASSPARSER_MAX_TAKES * sizeof(int));
		return m_maxRecPass;
	}
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
// SNM_TrackEnvParserPatcher
// _mode: -1=remove all envs, -2=add to point positions, -3: get envs
///////////////////////////////////////////////////////////////////////////////

bool SNM_TrackEnvParserPatcher::NotifyStartElement(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	bool update = (_mode == -1 ? m_parsingEnv : false);
	if (!m_parsingEnv && LookupTrackEnvName(GetParent(_parsedParents), _mode != -3)) // _mode==-3: ignores PARMENV, PROGRAMENV
	{
		const char* grandpa = GetParent(_parsedParents, 2);
		m_parsingEnv = (!strcmp(grandpa, "TRACK") || !strcmp(grandpa, "FXCHAIN")); // do not deal with take env for ex.
	}
	if (_mode == -1)
		update = m_parsingEnv;
	return update;
}

bool SNM_TrackEnvParserPatcher::NotifyEndElement(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	bool update = (_mode == -1 ? m_parsingEnv : false);
	if (m_parsingEnv && LookupTrackEnvName(GetParent(_parsedParents), _mode != -3))
	{
		const char* grandpa = GetParent(_parsedParents, 2);
		m_parsingEnv = !(!strcmp(grandpa, "TRACK") || !strcmp(grandpa, "FXCHAIN")); // do not deal with take env for ex.

		if (_mode == -3 && _parsedParents->GetSize()>=2) //>= more future-proof
			m_envs.Append(">\n");

		// note: with _mode==-1, "update" stays true if m_parsingEnv was initially true, i.e. do not copy the end of chunk ">"
	}
	return update;
}

bool SNM_TrackEnvParserPatcher::NotifyChunkLine(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	bool update = (_mode == -1 ? m_parsingEnv : false);
	if (_mode == -2)
	{
		if (m_parsingEnv && !strcmp(_lp->gettoken_str(0), "PT") && strcmp(_lp->gettoken_str(1), "0.000000")) // do not move very first points
		{
			int success; double d = _lp->gettoken_float(1, &success);
			if (success) {
				d += m_addDelta;
				char buf[318]{};
				int l = snprintf(buf, sizeof(buf), "%.6f", d);
				if (l<=0 || l>=64) return update;
				update |= WriteChunkLine(_newChunk, buf, 1, _lp); 
			}
		}
	}
	else if (_mode == -3)
	{
		if (m_parsingEnv) {
			m_envs.Append(_parsedLine);
			m_envs.Append("\n");
		}
	}
	return update;
}

bool SNM_TrackEnvParserPatcher::RemoveEnvelopes() {
	m_parsingEnv = false;
	return (ParsePatch(-1) > 0);
}

bool SNM_TrackEnvParserPatcher::OffsetEnvelopes(double _delta) {
	m_parsingEnv = false;
	m_addDelta = _delta;
	return (ParsePatch(-2) > 0);
}

// for track envs only (i.e. do not get fx param envs)
const char* SNM_TrackEnvParserPatcher::GetTrackEnvelopes() {
	m_parsingEnv = false;
	m_envs.Set("");
	// looking for depth 2 ensures PARMENV will be ignored
	Parse(-3); // cannot specify more parameters, the parser has to go in depth
	return (m_envs.GetLength() ? m_envs.Get() : NULL);
}



///////////////////////////////////////////////////////////////////////////////
// SNM_ArmEnvParserPatcher
///////////////////////////////////////////////////////////////////////////////

// _mode: -1 all envelopes, -2 receive vol envelopes, -3 receive pan envelopes, -4 receive mute envelopes
bool SNM_ArmEnvParserPatcher::NotifyChunkLine(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	bool updated = false;
	if (_mode < 0)
	{
		if (_lp->getnumtokens() == 2 && _parsedParents->GetSize() > 1 &&  !strcmp(_lp->gettoken_str(0), "ARM"))
		{
			const char* grandpa = GetParent(_parsedParents, 2);
			if (!strcmp(grandpa, "TRACK") || !strcmp(grandpa, "FXCHAIN")) // don't scratch item env for ex
			{
				const char* parent = GetParent(_parsedParents);
				if (m_skipReceiveEnvs && (!strcmp(parent, "AUXVOLENV") || !strcmp(parent, "AUXPANENV") || !strcmp(parent, "AUXMUTEENV")))
					return false;
				// most "frequent" first!
				if ((_mode == -1 && LookupTrackEnvName(parent, true)) || 
					(_mode == -2 && !strcmp(parent, "AUXVOLENV")) ||
					(_mode == -3 && !strcmp(parent, "AUXPANENV")) ||
					(_mode == -4 && !strcmp(parent, "AUXMUTEENV")) ||
					(_mode == -5 && !strcmp(parent, "PARMENV")))
				{
					if (m_newValue == -1)
						_newChunk->AppendFormatted(SNM_MAX_CHUNK_LINE_LENGTH, "ARM %d\n", !_lp->gettoken_int(1) ? 1 : 0);
					else
						_newChunk->AppendFormatted(SNM_MAX_CHUNK_LINE_LENGTH, "ARM %d\n", m_newValue);
					updated = true;
				}
			}
		}
	}
	return updated;
}

// _mode: -1 all envelopes, -2 receive vol envelopes, -3 receive pan envelopes, -4 receive mute envelopes
bool SNM_ArmEnvParserPatcher_Env::NotifyChunkLine(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, WDL_FastString* _newChunk, int _updates)
{
	bool updated = false;
	if (_mode < 0)
	{
		// check if envelope is active
		if (!strcmp(_lp->gettoken_str(0), "ACT") && _lp->gettoken_int(1) == 1)
			m_envIsActive = true;

		if (_lp->getnumtokens() == 2 && !strcmp(_lp->gettoken_str(0), "ARM") && m_envIsActive)
		{
			const char* parent = GetParent(_parsedParents);
			// most "frequent" first!
			if ((_mode == -1 && LookupTrackEnvName(parent, false)) ||
				(_mode == -2 && !strcmp(parent, "AUXVOLENV")) ||
				(_mode == -3 && !strcmp(parent, "AUXPANENV")) ||
				(_mode == -4 && !strcmp(parent, "AUXMUTEENV")))
			{
				if (m_newValue == -1)
					_newChunk->AppendFormatted(SNM_MAX_CHUNK_LINE_LENGTH, "ARM %d\n", !_lp->gettoken_int(1) ? 1 : 0);
				else
					_newChunk->AppendFormatted(SNM_MAX_CHUNK_LINE_LENGTH, "ARM %d\n", m_newValue);
				updated = true;
			}
		}
	}
	return updated;
}

///////////////////////////////////////////////////////////////////////////////
// SNM_LearnMIDIChPatcher
///////////////////////////////////////////////////////////////////////////////

bool SNM_LearnMIDIChPatcher::NotifyEndElement(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	if (_mode == -1 && !strcmp(GetParent(_parsedParents), "FXCHAIN"))
		m_breakParsePatch = true; // optmization
	return false; 
}

bool SNM_LearnMIDIChPatcher::NotifyChunkLine(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	bool updated = false;
	if (_mode == -1)
	{
		if (_lp->getnumtokens() == 3 && !strcmp(_lp->gettoken_str(0), "BYPASS"))
		{
			m_currentFx++;
		}
		else if ((m_fx == -1 || m_fx == m_currentFx) && 
			_lp->getnumtokens() == 4 && // indirectly exclude params learned with osc (5 tokens)
			!strcmp(_lp->gettoken_str(0), "PARMLEARN"))
		{
			int midiMsg = _lp->gettoken_int(2) & 0xFFF0;
			midiMsg |= m_newChannel;
			_newChunk->AppendFormatted(SNM_MAX_CHUNK_LINE_LENGTH, "PARMLEARN %d %d %d\n", _lp->gettoken_int(1), midiMsg, _lp->gettoken_int(3));
			updated = true;
/* no! may be there are several learned params for that FX..
			m_breakParsePatch = (m_fx != -1); // one fx to be patched
*/
		}
	}
	return updated;
}

bool SNM_LearnMIDIChPatcher::SetChannel(int _newValue, int _fx)
{
	m_newChannel = _newValue;
	m_fx = _fx;
	m_currentFx = -1;
	return (ParsePatch(-1,2,"FXCHAIN") > 0);
}


///////////////////////////////////////////////////////////////////////////////
// SNM_FXSummaryParser
// works with .rfxchain, input fx chain and std fx chain
///////////////////////////////////////////////////////////////////////////////

bool SNM_FXSummaryParser::NotifyStartElement(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	if (_mode == -1)
	{
		if (_lp->getnumtokens() >= 3)
		{
			// process all fx types (in case rpp files were exchanged osx <-> win)
			if (!strcmp(_lp->gettoken_str(0), "<VST") || !strcmp(_lp->gettoken_str(0), "<AU"))
				m_summaries.Add(new SNM_FXSummary(_lp->gettoken_str(0)+1, _lp->gettoken_str(1), _lp->gettoken_str(2)));
			else if (!strcmp(_lp->gettoken_str(0), "<JS") || !strcmp(_lp->gettoken_str(0), "<DX") || !strcmp(_lp->gettoken_str(0), "<VIDEO_EFFECT"))
				m_summaries.Add(new SNM_FXSummary(_lp->gettoken_str(0)+1, _lp->gettoken_str(1), _lp->gettoken_str(1)));
		}
	}
	return false;
}

WDL_PtrList<SNM_FXSummary>* SNM_FXSummaryParser::GetSummaries()
{
	m_summaries.Empty(true);
	if (Parse(-1) >= 0)
		return &m_summaries;
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// SNM_TakeEnvParserPatcher
///////////////////////////////////////////////////////////////////////////////

bool SNM_TakeEnvParserPatcher::NotifyChunkLine(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates)
{
	bool updated = false;
	if (_mode == -1)
	{
		bool arm = false;
		if (!m_patchVisibilityOnly)
			updated = (!strcmp(_lp->gettoken_str(0), "ACT") || !strcmp(_lp->gettoken_str(0), "VIS"));
		else
			updated = !strcmp(_lp->gettoken_str(0), "VIS");
		arm = (strcmp(_lp->gettoken_str(0), "ARM") == 0);
		updated |= arm;
		if (updated) {
			_newChunk->AppendFormatted(32, "%s %d\n", _lp->gettoken_str(0), m_val); // 32 for OSX
			m_breakParsePatch = arm;
		}
	}
	return updated;
}

bool SNM_TakeEnvParserPatcher::SetVal(const char* _envKeyWord, int _val) {
	m_val = _val;
	return (ParsePatch(-1, 1, _envKeyWord) > 0);
}

void SNM_TakeEnvParserPatcher::SetPatchVisibilityOnly(bool visibilityOnly)
{
	m_patchVisibilityOnly = visibilityOnly;;
}

int g_disable_chunk_guid_filtering;
