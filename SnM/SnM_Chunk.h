/******************************************************************************
/ SnM_Chunk.h
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


//#pragma once

#ifndef _SNM_CHUNK_H_
#define _SNM_CHUNK_H_


#define SNM_RECPASSPARSER_MAX_TAKES		1024


///////////////////////////////////////////////////////////////////////////////
// SNM_ChunkIndenter
///////////////////////////////////////////////////////////////////////////////

class SNM_ChunkIndenter : public SNM_ChunkParserPatcher
{
public:
	SNM_ChunkIndenter(WDL_FastString* _chunk, bool _autoCommit = true)
		: SNM_ChunkParserPatcher(_chunk, _autoCommit, true, true, true) {}
	~SNM_ChunkIndenter() {}
	bool Indent() {return (ParsePatch(-1) > 0);}
protected:
	bool NotifyChunkLine(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents,
		WDL_FastString* _newChunk, int _updates);
};


///////////////////////////////////////////////////////////////////////////////
// SNM_SendPatcher
///////////////////////////////////////////////////////////////////////////////

class SNM_SendPatcher : public SNM_ChunkParserPatcher
{
public:
	SNM_SendPatcher(MediaTrack* _destTrack)	: SNM_ChunkParserPatcher(_destTrack) 
	{
		m_srcId = -1;
		m_sendType = 2; // voluntary deprecated
		m_vol = NULL;
		m_pan = NULL;
		m_sndRcv = NULL;
	}
	~SNM_SendPatcher() {}
	int AddReceive(MediaTrack* _srcTr, int _sendType, const char* _vol, const char* _pan);
	bool AddReceive(MediaTrack* _srcTr, void* _io);
	int RemoveReceives();
	int RemoveReceivesFrom(MediaTrack* _srcTr);

protected:
	bool NotifyChunkLine(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos, 
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);

	int m_srcId;
	int m_sendType;
	const char* m_vol;
	const char* m_pan;
	void* m_sndRcv;
};


///////////////////////////////////////////////////////////////////////////////
// SNM_FXChainTakePatcher
///////////////////////////////////////////////////////////////////////////////

class SNM_FXChainTakePatcher : public SNM_ChunkParserPatcher
{
public:
	SNM_FXChainTakePatcher(MediaItem* _item) : SNM_ChunkParserPatcher(_item) 
	{
		m_fxChain = NULL;
		m_removingTakeFx = false;
		m_activeTake = false;
	}
	~SNM_FXChainTakePatcher() {}

	bool SetFXChain(WDL_FastString* _fxChain, bool _activeTakeOnly);
	WDL_FastString* GetFXChain();

protected:
	bool NotifyStartElement(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos, 
		WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);

	bool NotifyEndElement(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);

	bool NotifyChunkLine(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);

	bool NotifySkippedSubChunk(int _mode, 
		const char* _subChunk, int _subChunkLength, int _subChunkPos,
		WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);

	WDL_FastString* m_fxChain;
	WDL_FastString m_copiedFXChain;
	bool m_removingTakeFx;
	bool m_copyingTakeFx;
	bool m_activeTake;
};


///////////////////////////////////////////////////////////////////////////////
// SNM_FXChainTrackPatcher
///////////////////////////////////////////////////////////////////////////////

class SNM_FXChainTrackPatcher : public SNM_ChunkParserPatcher
{
public:
	SNM_FXChainTrackPatcher(MediaTrack* _tr) : SNM_ChunkParserPatcher(_tr) 
	{
		m_fxChain = NULL;
		m_removingFxChain = false;
	}
	~SNM_FXChainTrackPatcher() {}

	bool SetFXChain(WDL_FastString* _fxChain, bool _inputFX = false);
	int GetInputFXCount();

protected:
	bool NotifyStartElement(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos, 
		WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);

	bool NotifyEndElement(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);

	bool NotifyChunkLine(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos, 
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);

	bool NotifySkippedSubChunk(int _mode, 
		const char* _subChunk, int _subChunkLength, int _subChunkPos,
		WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);

	WDL_FastString* m_fxChain;
	bool m_removingFxChain;
	bool m_copyingFxChain;
};


///////////////////////////////////////////////////////////////////////////////
// SNM_TakeParserPatcher
// JFB TODO? maintain m_activeTakeIdx when updating takes and call 
// GetSetMediaItemInfo(m_object, "I_CURTAKE", &m_activeTakeIdx) on commit ? 
// Note: let REAPER manages the active take ATM, seems ok..
///////////////////////////////////////////////////////////////////////////////

class SNM_TakeParserPatcher : public SNM_ChunkParserPatcher
{
public:
	SNM_TakeParserPatcher(MediaItem* _item, int _countTakes = -1) : SNM_ChunkParserPatcher(_item) {
		m_currentTakeCount = _countTakes; // lazy init through CountTakesInChunk() if < 0
		m_fakeTake = false;
	}
	// call to Commit(): when a constructor or destructor calls a virtual 
	// function it calls the function defined for the type whose constructor
	// or destructor is currently being run
	~SNM_TakeParserPatcher() {
		if (m_autoCommit)
			Commit(); // no-op if chunk not updated (or no valid m_object)
	}
	WDL_FastString* GetChunk();
	bool Commit(bool _force = false);
	bool GetTakeChunkPos(int _takeIdx, int* _pos, int* _len = NULL);
	bool GetTakeChunk(int _takeIdx, WDL_FastString* _gettedChunk, int* _pos = NULL, int* _len = NULL);
	int CountTakesInChunk();
	bool IsEmpty(int _takeIdx);
	int AddLastTake(WDL_FastString* _tkChunk);
	int InsertTake(int _takeIdx, WDL_FastString* _chunk, int _pos = -1);
	bool RemoveTake(int _takeIdx, WDL_FastString* _removedChunk = NULL, int* _removedStartPos = NULL);
	bool ReplaceTake(int _startTakePos, int _takeLength, WDL_FastString* _newTakeChunk);
protected:
	int m_currentTakeCount; // nb of takes *in the chunk* (may be different than REAPER's ones)
//	int m_activeTakeIdx;    // active take *in the chunk* (may be different than REAPER's ones)
private:
	// check that _pLine is indeed the 1st line of a new take in an item chunk
	// remarks: _pLine must start with "\nTAKE", also we assume we're processing
	//          a valid chunk here (i.e. doesn't end with "\nTAKE")
	bool IsValidTakeChunkLine(const char* _pLine) {return (_pLine[5] && (_pLine[5] == '\n' || _pLine[5] == ' '));}
	bool m_fakeTake;
};


///////////////////////////////////////////////////////////////////////////////
// SNM_RecPassParser
// Inherits SNM_TakeParserPatcher in order to ease processing
///////////////////////////////////////////////////////////////////////////////

class SNM_RecPassParser : public SNM_TakeParserPatcher
{
public:
	SNM_RecPassParser(MediaItem* _item, int _countTakes) : SNM_TakeParserPatcher(_item, _countTakes) {
		m_maxRecPass = -1;
		m_takeCounter = 0;
	}
	~SNM_RecPassParser() {}
	int GetMaxRecPass(int* _recPasses=NULL, int* _takeColors=NULL);
protected:
	bool NotifyChunkLine(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents,
		WDL_FastString* _newChunk, int _updates);
	int m_maxRecPass;
	int m_recPasses[SNM_RECPASSPARSER_MAX_TAKES];
	int m_takeColors[SNM_RECPASSPARSER_MAX_TAKES];
private:
	int m_takeCounter;
};


///////////////////////////////////////////////////////////////////////////////
// SNM_TrackEnvParserPatcher
///////////////////////////////////////////////////////////////////////////////

class SNM_TrackEnvParserPatcher : public SNM_ChunkParserPatcher
{
public:
	SNM_TrackEnvParserPatcher(WDL_FastString* _str, bool _autoCommit = true)
		: SNM_ChunkParserPatcher(_str, _autoCommit) {m_parsingEnv = false; }
	SNM_TrackEnvParserPatcher(MediaTrack* _tr, bool _autoCommit = true)
		: SNM_ChunkParserPatcher(_tr, _autoCommit) {m_parsingEnv = false; }
	~SNM_TrackEnvParserPatcher() {}
	bool RemoveEnvelopes();
	bool OffsetEnvelopes(double _delta);
	const char* GetTrackEnvelopes();
protected:
	bool NotifyStartElement(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);
	bool NotifyEndElement(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);
	bool NotifyChunkLine(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents,
		WDL_FastString* _newChunk, int _updates);
private:
	bool m_parsingEnv;
	double m_addDelta;
	WDL_FastString m_envs;
};


///////////////////////////////////////////////////////////////////////////////
// SNM_ArmEnvParserPatcher
///////////////////////////////////////////////////////////////////////////////

class SNM_ArmEnvParserPatcher : public SNM_ChunkParserPatcher
{
public:
	SNM_ArmEnvParserPatcher(MediaTrack* _tr) : SNM_ChunkParserPatcher(_tr) {
		m_newValue = -1; // i.e. toggle
		m_skipReceiveEnvs = false;
	}
	~SNM_ArmEnvParserPatcher() {}
	void SetNewValue(int _newValue) {m_newValue = _newValue;}
	void SkipReceiveEns(bool _skipReceiveEns) {m_skipReceiveEnvs = _skipReceiveEns;}
protected:
	bool NotifyChunkLine(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents,
		WDL_FastString* _newChunk, int _updates);
private:
	int m_newValue;
	bool m_skipReceiveEnvs;
};

// similar to SNM_ArmEnvParserPatcher, but works on a TrackEnvelope* directly
class SNM_ArmEnvParserPatcher_Env : public SNM_ChunkParserPatcher
{
public:
	SNM_ArmEnvParserPatcher_Env(TrackEnvelope* _env) : SNM_ChunkParserPatcher(_env) {
		m_envIsActive = false;
		m_newValue = -1; // i.e. toggle
	}
	~SNM_ArmEnvParserPatcher_Env() {}
	void SetNewValue(int _newValue) { m_newValue = _newValue; }
protected:
	bool NotifyChunkLine(int _mode,
		LineParser* _lp, const char* _parsedLine, int _linePos,
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents,
		WDL_FastString* _newChunk, int _updates) override;
private:
	bool m_envIsActive;
	int m_newValue;
};


///////////////////////////////////////////////////////////////////////////////
// SNM_LearnMIDIChPatcher
///////////////////////////////////////////////////////////////////////////////

class SNM_LearnMIDIChPatcher : public SNM_ChunkParserPatcher
{
public:
	SNM_LearnMIDIChPatcher(MediaTrack* _tr) : SNM_ChunkParserPatcher(_tr) {
		m_newChannel = -1;
		m_fx = -1; // i.e. all FX
		m_currentFx = -1;
	}
	~SNM_LearnMIDIChPatcher() {}
	bool SetChannel(int _newValue, int _fx);
protected:
	bool NotifyEndElement(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);
	bool NotifyChunkLine(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents,
		WDL_FastString* _newChunk, int _updates);
private:
	int m_newChannel, m_fx, m_currentFx;
};


///////////////////////////////////////////////////////////////////////////////
// SNM_FXSummaryParser (+ SNM_FXSummary)
///////////////////////////////////////////////////////////////////////////////

class SNM_FXSummary {
public:
	SNM_FXSummary(const char* _type, const char* _name, const char* _realName)
		: m_type(_type),m_name(_name),m_realName(_realName){}
	WDL_FastString m_type, m_name, m_realName;
};

class SNM_FXSummaryParser : public SNM_ChunkParserPatcher
{
public:
	SNM_FXSummaryParser(MediaTrack* _tr) : SNM_ChunkParserPatcher(_tr) { SetWantsMinimalState(true); }
	SNM_FXSummaryParser(MediaItem* _item) : SNM_ChunkParserPatcher(_item) { SetWantsMinimalState(true); }
	SNM_FXSummaryParser(WDL_FastString* _str) : SNM_ChunkParserPatcher(_str) {}
	~SNM_FXSummaryParser() {}
	WDL_PtrList<SNM_FXSummary>* GetSummaries();
protected:
	bool NotifyStartElement(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		WDL_PtrList<WDL_FastString>* _parsedParents, 
		WDL_FastString* _newChunk, int _updates);
	WDL_PtrList_DeleteOnDestroy<SNM_FXSummary> m_summaries;
};


///////////////////////////////////////////////////////////////////////////////
// SNM_TakeEnvParserPatcher
///////////////////////////////////////////////////////////////////////////////

class SNM_TakeEnvParserPatcher : public SNM_ChunkParserPatcher
{
public:
	SNM_TakeEnvParserPatcher(WDL_FastString* _tkChunk, bool _autoCommit = true) 
		: SNM_ChunkParserPatcher(_tkChunk, _autoCommit) {m_val = -1; m_patchVisibilityOnly = false;}
	~SNM_TakeEnvParserPatcher() {}
	bool SetVal(const char* _envKeyWord, int _val);
	// NF: #1078, if true, only env. visibilty (show/hide) is patched
	// (as opposed to also patching env. unbypass/bypass, resp. active/not active)
	void SetPatchVisibilityOnly(bool patchVisibilityOnly); 
protected:
	bool NotifyChunkLine(int _mode, 
		LineParser* _lp, const char* _parsedLine, int _linePos,
		int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents,
		WDL_FastString* _newChunk, int _updates);
private:
	int m_val;
	bool m_patchVisibilityOnly;
};

#endif
