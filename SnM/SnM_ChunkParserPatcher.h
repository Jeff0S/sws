/******************************************************************************
/ SnM_ChunkParserPatcher.h - v1.34
/
/ Copyright (c) 2008 and later Jeffos
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


// Some fast tools to parse and alter RPP state chunks.
//
// SNM_ChunkParserPatcher is a class that can be used either as a SAX-ish 
// parser (via inheritance) -OR- as a direct getter/altering tool (via Parse() 
// and ParsePatch(), respectively).
// In both cases, it is either attached to a WDL_FastString* (simple text 
// processor) -OR- to a reaThing* (MediaTrack*, MediaItem*, ..). 
// A SNM_ChunkParserPatcher instance only gets and sets the chunk once. In
// between, it works on a cache. IF ANY, updates are automatically committed 
// when destroying the instance (can also be avoided/forced, see m_autoCommit
// and Commit()).
//
// Important: 
// - Chunks can be HUGE! e.g. 4Mb+ is an usual case
// - The code assumes RPP chunks are consistent, left trimmed, with Unix EOL
// - A v2.0 with major refactoring is on the way


#ifndef _SNM_CHUNKPARSERPATCHER_H_
#define _SNM_CHUNKPARSERPATCHER_H_

#ifndef __GNUC__
#pragma warning(disable : 4267) // size_t to int warnings in x64
#endif

#define _SWS_EXTENSION
#ifdef _SWS_EXTENSION
#define SNM_FreeHeapPtr			SWS_FreeHeapPtr
#else
#define SNM_FreeHeapPtr			FreeHeapPtr
#endif


// The different modes for ParsePatch() and Parse()
#define SNM_PARSE_AND_PATCH				0
#define SNM_PARSE_AND_PATCH_EXCEPT		1
#define SNM_PARSE						2
#define SNM_PARSE_EXCEPT				3
#define SNM_GET_CHUNK_CHAR				6
#define SNM_SET_CHUNK_CHAR				7
#define SNM_SETALL_CHUNK_CHAR_EXCEPT	8
#define SNM_GETALL_CHUNK_CHAR_EXCEPT	9
#define SNM_TOGGLE_CHUNK_INT			10
#define SNM_TOGGLE_CHUNK_INT_EXCEPT		11
#define SNM_REPLACE_SUBCHUNK_OR_LINE	12
#define SNM_GET_SUBCHUNK_OR_LINE		13
#define SNM_GET_SUBCHUNK_OR_LINE_EOL	14
#define SNM_COUNT_KEYWORD				15
#define SNM_D_ADD						16
#define SNM_D_MUL						17


// Misc
#define SNM_MAX_CHUNK_LINE_LENGTH		8192 // inspired by WDL's projectcontext
#define SNM_MAX_CHUNK_KEYWORD_LENGTH	64
#define SNM_HEAPBUF_GRANUL				256*1024


///////////////////////////////////////////////////////////////////////////////
// Helpers (see EOF)
///////////////////////////////////////////////////////////////////////////////

static int RemoveChunkLines(char* _chunk, const char* _searchStr,
							bool _checkBOL = false, int _checkEOLChar = 0);
static int RemoveChunkLines(char* _chunk, WDL_PtrList<const char>* _searchStrs, 
							bool _checkBOL = false, int _checkEOLChar = 0);
static int RemoveChunkLines(WDL_FastString* _chunk, const char* _searchStr,
							bool _checkBOL = false, int _checkEOLChar = 0);
static int RemoveChunkLines(WDL_FastString* _chunk, WDL_PtrList<const char>* _searchStrs,
							bool _checkBOL = false, int _checkEOLChar = 0);
static int FindEndOfSubChunk(const char* _chunk, int _startPos);

static int SNM_PreObjectState(WDL_FastString* _str = NULL, bool _wantsMinState = false);
static void SNM_PostObjectState(int _oldfxstate);

// remove all ids, e.g. GUIDs, FXIDs, etc..
// fast but it does not check depth, parent, etc.. 
// note: preserve both POOLEDEVTS & FXID_NEXT (frozen FX ids)
static int RemoveAllIds(char* _chunk) {
	return RemoveChunkLines(_chunk, "ID {", false, '}');
}
static int RemoveAllIds(WDL_FastString* _chunk) {
	return RemoveChunkLines(_chunk, "ID {", false, '}');
}


///////////////////////////////////////////////////////////////////////////////
// SNM_ChunkParserPatcher
///////////////////////////////////////////////////////////////////////////////

class SNM_ChunkParserPatcher
{
public:

// when attached to a reaThing* (MediaTrack*, MediaItem*, ..)
// _autoCommit: if false, a 'manual' call to Commit() is needed to apply the updates
//              if true, Commit() is automatically called when destroying the object
//              note: Commit() is no-op if no updates
// _processXXX : optimization flags
SNM_ChunkParserPatcher(void* _reaObject, bool _autoCommit=true,
					   bool _processBase64=false, bool _processInProjectMIDI=false, bool _processFreeze=false)
{
	m_chunk = new WDL_FastString(SNM_HEAPBUF_GRANUL);
	m_reaObject = _reaObject;
	m_originalChunk = NULL;
	m_updates = 0;
	m_autoCommit = _autoCommit;
	m_breakParsePatch = false;
	m_processBase64 = _processBase64;
	m_processInProjectMIDI = _processInProjectMIDI;
	m_processFreeze = _processFreeze;
	m_minimalState = false;
}

// when attached to a WDL_FastString* (simple text chunk parser/patcher)
// _autoCommit: if false, a 'manual' call to Commit() is needed to apply the updates
//              if true, Commit() is automatically called when destroying the object (i.e. _chunk is updated)
//              note: Commit() is no-op if no updates
// _processXXX : optimization flags
SNM_ChunkParserPatcher(WDL_FastString* _chunk, bool _autoCommit=true,
					   bool _processBase64=false, bool _processInProjectMIDI=false, bool _processFreeze=false)
{
	m_chunk = new WDL_FastString(SNM_HEAPBUF_GRANUL);
	m_reaObject = NULL;
	m_originalChunk = _chunk;
	m_updates = 0;
	m_autoCommit = _autoCommit;
	m_breakParsePatch = false;
	m_processBase64 = _processBase64;
	m_processInProjectMIDI = _processInProjectMIDI;
	m_processFreeze = _processFreeze;
	m_minimalState = false;
}

virtual ~SNM_ChunkParserPatcher() 
{
	if (m_autoCommit)
		Commit(); // no-op if no updates

	if (m_chunk) {
		delete m_chunk;
		m_chunk = NULL;
	}
}

// See ParsePatchCore() comments
int ParsePatch(
	int _mode = SNM_PARSE_AND_PATCH, 
	int _depth = -1,
	const char* _expectedParent = NULL, 
	const char* _keyWord = NULL, 
	int _occurence = -1, 
	int _tokenPos = -1, 
	void* _value = NULL,
	void* _valueExcept = NULL,
	const char* _breakKeyword = NULL)
{
	return ParsePatchCore(true, _mode, _depth, _expectedParent, _keyWord, _occurence, _tokenPos, _value,_valueExcept, _breakKeyword);
}

// See ParsePatchCore() comments
int Parse(
	int _mode = SNM_PARSE, 
	int _depth = -1, 
	const char* _expectedParent = NULL, 
	const char* _keyWord = NULL, 
	int _occurence = -1, 
	int _tokenPos = -1, 
	void* _value = NULL,
	void* _valueExcept = NULL,
	const char* _breakKeyword = NULL)
{
	return ParsePatchCore(false, _mode, _depth, _expectedParent, _keyWord, _occurence, _tokenPos, _value,_valueExcept, _breakKeyword);
}

void* GetObject() {
	return m_reaObject;
}

// get and cache the RPP chunk
// note: this method *always* returns a valid value (non NULL)
virtual WDL_FastString* GetChunk() 
{
	if (!m_chunk->GetLength())
	{
		if (m_reaObject) {			
			if (const char* cData = (m_reaObject ? SNM_GetSetObjectState(m_reaObject, NULL) : NULL)) {
				m_chunk->Set(cData);
				SNM_FreeHeapPtr((void*)cData);
			}
		}
		else if (m_originalChunk)
			m_chunk->Set(m_originalChunk);
	}
	return m_chunk;
}


// IMPORTANT: 
// m_updates has to be kept up-to-date (nothing will be committed otherwise)
// => you must use UpdateChunk() or SetChunk() methods when altering the
//    the cached chunk directly (or 'manually' alter m_updates)

// clearing the cache is allowed
void SetChunk(const char* _newChunk, int _updates=1) {
	m_updates = _updates;
	GetChunk()->Set(_newChunk ? _newChunk : "");
}

int GetUpdates() {
	return m_updates;
}

int IncUpdates() {
	m_updates++;
	return m_updates; // for facility
}

int SetUpdates(int _updates) {
	m_updates = _updates;
	return m_updates; // for facility
}

// no-op if no updates: commit only if needed.
// when attached to a reaThing*, global protections apply:
// - no patch while recording 
// - remove all ids before patching, see SNM_GetSetObjectState()
virtual bool Commit(bool _force = false)
{
	if ((m_updates || _force) && GetChunk()->GetLength())
	{
		if (m_reaObject) {
			if (!(GetPlayStateEx(NULL) & 4) && !SNM_GetSetObjectState(m_reaObject, m_chunk)) {
				SetChunk("", 0);
				return true;
			}
		}
		else if (m_originalChunk) {
			m_originalChunk->Set(m_chunk);
			SetChunk("", 0);
			return true;
		}
	}
	return false;
}

const char* GetInfo() {
	return "SNM_ChunkParserPatcher - v1.34";
}

void SetProcessBase64(bool _enable) {
	m_processBase64 = _enable;
}

void SetProcessInProjectMIDI(bool _enable) {
	m_processInProjectMIDI = _enable;
}

void SetProcessFreeze(bool _enable) {
	m_processFreeze = _enable;
}

void SetWantsMinimalState(bool _enable) {
	m_minimalState = _enable;
}


///////////////////////////////////////////////////////////////////////////////
// Helpers

void CancelUpdates() {
	SetChunk("", 0);
}

// returns the start position of the sub-chunk or -1 if not found
// _depth: _keyword's depth
// _chunk: optional output prm, the searched sub-chunk if found
// _breakKeyword: for optimization, optional
int GetSubChunk(const char* _keyword, int _depth, int _occurence, WDL_FastString* _chunk = NULL, const char* _breakKeyword = NULL)
{
	int pos = -1;
	if (_keyword && _depth > 0) // min _depth==1, i.e. "<keyword .."
	{
		if (_chunk) _chunk->Set("");
		WDL_FastString startToken;
		startToken.SetFormatted((int)strlen(_keyword)+2, "<%s", _keyword);
		pos = Parse(SNM_GET_SUBCHUNK_OR_LINE, _depth, _keyword, startToken.Get(), _occurence, -1, (void*)_chunk, NULL, _breakKeyword);
		if (pos <= 0) {
			if (_chunk) _chunk->Set("");
			pos = -1; // force negative return value if 0 (not found)
		}
		else pos--; // see ParsePatchCore()
	}
	return pos;
}

// _depth: _keyword's depth
// _occurence: subchunk occurrence to be replaced (-1 to replace all)
// _newSubChunk: the replacing string (so "" will remove the subchunk)
// returns false if nothing done (e.g. subchunk not found)
bool ReplaceSubChunk(const char* _keyword, int _depth, int _occurence, const char* _newSubChunk, const char* _breakKeyword = NULL)
{
	if (_keyword && _depth > 0) // min _depth==1, i.e. "<keyword .."
	{
		WDL_FastString startToken;
		startToken.SetFormatted((int)strlen(_keyword)+2, "<%s", _keyword);
		return (ParsePatch(SNM_REPLACE_SUBCHUNK_OR_LINE, _depth, _keyword, startToken.Get(), _occurence, 0, (void*)_newSubChunk, NULL, _breakKeyword) > 0);
	}
	return false;
}

// returns false if nothing done (e.g. sub-chunk not found)
bool RemoveSubChunk(const char* _keyword, int _depth, int _occurence, const char* _breakKeyword = NULL) {
	return ReplaceSubChunk(_keyword, _depth, _occurence, "", _breakKeyword);
}

// replace characters in the chunk from _pos to the next eol
// _str: the replacing string or NULL to remove characters
bool ReplaceLine(int _pos, const char* _str = NULL)
{
	if (_pos >=0 && GetChunk()->GetLength() > _pos) // + indirectly cache chunk if needed
	{
		int pos = _pos;
		const char* pChunk = m_chunk->Get();
		while (pChunk[pos] && pChunk[pos] != '\n') pos++;
		if (pChunk[pos] == '\n')
		{
			m_chunk->DeleteSub(_pos, (pos+1) - _pos);
			if (_str && *_str)
				m_chunk->Insert(_str, _pos);
			m_updates++;
			return true;
		}
	}
	return false;
}

// replace line(s) beginning with _keyword
bool ReplaceLine(const char* _parent, const char* _keyword, int _depth, int _occurence, const char* _newSubChunk = "", const char* _breakKeyword = NULL)
{
	if (_keyword && _depth >= 0) // can be 0, e.g. .rfxchain file
		return (ParsePatch(SNM_REPLACE_SUBCHUNK_OR_LINE, _depth, _parent, _keyword, _occurence, 0, (void*)_newSubChunk, NULL, _breakKeyword) > 0);
	return false;
}

// remove line(s) beginning with _keyword
bool RemoveLine(const char* _parent, const char* _keyword, int _depth, int _occurence, const char* _breakKeyword = NULL) {
	return ReplaceLine(_parent, _keyword, _depth, _occurence, "", _breakKeyword);
}

// remove line(s) containing or beginning with _keyword
// this one is faster but it does not check depth, parent, etc.. 
// => beware of nested data! (FREEZE sub-chunks, for example)
int RemoveLines(const char* _removedKeyword, bool _checkBOL = true, int _checkEOLChar = 0) {
	return SetUpdates(RemoveChunkLines(GetChunk(), _removedKeyword, _checkBOL, _checkEOLChar));
}

// remove line(s) containing or beginning with any string of _removedKeywords
// this one is faster but it does not check depth, parent, etc.. 
// => beware of nested data! (FREEZE sub-chunks, for example)
int RemoveLines(WDL_PtrList<const char>* _removedKeywords, bool _checkBOL = true, int _checkEOLChar = 0) {
	return SetUpdates(RemoveChunkLines(GetChunk(), _removedKeywords, _checkBOL, _checkEOLChar));
}

//inserts _str either after (_dir=1) or before (_dir=0) _keyword (i.e. at next/previous start of line)
bool InsertAfterBefore(int _dir, const char* _str, const char* _parent, const char* _keyword, int _depth, int _occurence, const char* _breakKeyword = NULL)
{
	if (_str && *_str && _keyword)
	{
		int pos = GetLinePos(_dir, _parent, _keyword, _depth, _occurence, _breakKeyword);
		if (pos >= 0) {
			m_chunk->Insert(_str, pos);
			m_updates++;
			return true;
		}
	}
	return false;
}

// returns the current, next or previous line (start) position for the searched _keyword
// _dir: -1 previous line, 0 current line, +1 next line
int GetLinePos(int _dir, const char* _parent, const char* _keyword, int _depth, int _occurence, const char* _breakKeyword = NULL)
{
	int pos = Parse(SNM_GET_CHUNK_CHAR, _depth, _parent, _keyword, _occurence, 0, NULL, NULL, _breakKeyword);
	if (pos > 0)
	{
		pos--; // See ParsePatchCore()
		if (_dir == -1 || _dir == 1)
		{
			const char* pChunk = m_chunk->Get();
			if (_dir == -1 && pos >= 2)
				pos-=2; // zap the previous '\n'
			while (pChunk[pos] && pChunk[pos] != '\n') pos += _dir;
			if (pChunk[pos] && pChunk[pos+1])
				return pos+1;
		}
		else
			return pos;
	}
	return -1;
}

const char* GetParent(WDL_PtrList<WDL_FastString>* _parents, int _ancestor=1) {
	int sz = _parents ? _parents->GetSize() : 0;
	if (sz >= _ancestor)
		return _parents->Get(sz-_ancestor)->Get();
	return "";
}

bool IsChildOf(WDL_PtrList<WDL_FastString>* _parents, const char* _ancestor) {
	for (int i=0; i < _parents->GetSize(); i++)
		if (!strcmp(_parents->Get(i)->Get(), _ancestor))
			return true;
	return false;
}


///////////////////////////////////////////////////////////////////////////////
protected:

	WDL_FastString* m_chunk;
	bool m_autoCommit;
	void* m_reaObject;
	WDL_FastString* m_originalChunk;
	int m_updates;

	// *** advanced/optional optimization flags ***

	// base-64 and in-project MIDI data as well as FREEZE sub-chunks
	// are ignored by default when parsing (+ bulk recopy when patching)
	bool m_processBase64, m_processInProjectMIDI, m_processFreeze;

	// useful when parsing: REAPER returns minimal states
	// note: such states must not be patched back (corrupted/incomplete states)
	bool m_minimalState;

	// this one is READ-ONLY (automatically set when parsing SOURCE sub-chunks)
	bool m_isParsingSource;

	// can be enabled to break parsing (+ bulk recopy when patching)
	bool m_breakParsePatch;


const char* SNM_GetSetObjectState(void* _obj, WDL_FastString* _str)
{
	const char* p = NULL;
#ifdef _SWS_EXTENSION
	p = SWS_GetSetObjectState(_obj, _str, m_minimalState);
#else
	int fxstate = SNM_PreObjectState(_str, m_minimalState);
	p = GetSetObjectState(_obj, _str ? _str->Get() : NULL);
	SNM_PostObjectState(fxstate);
#endif
#ifdef _SNM_DEBUG
	char fn[SNM_MAX_PATH] = "";
	int l = snprintf(fn, sizeof(fn), "%s%cSNM_CPP_last%s.log", GetExePath(), PATH_SLASH_CHAR, _str ? "Set" : "Get");
	if (l>0 && l<SNM_MAX_PATH)
		if (FILE* f = fopenUTF8(fn, "w")) {
			fputs(_str ? _str->Get() : (p?p:"NULL"), f);
			fclose(f);
		}
#endif
	return p;
}

bool WriteChunkLine(WDL_FastString* _chunkLine, const char* _value, int _tokenPos, LineParser* _lp)
{
	bool updated = false;
	if (strcmp(_lp->gettoken_str(_tokenPos), _value))
	{
		int numtokens = _lp->getnumtokens();
		for (int i=0; i < numtokens; i++) 
		{
			if (i == _tokenPos) {
				_chunkLine->Append(_value);
				updated = true;
			}
			else _chunkLine->Append(_lp->gettoken_str(i));
			_chunkLine->Append(i == (numtokens-1) ? "\n" : " ");
		}
	}
	return updated;
}


///////////////////////////////////////////////////////////////////////////////
// Parsing callbacks: to be implemented for SAX-ish parsing style
// Parameters:
// _mode: parsing mode, see ParsePatchCore(), <0 for custom parsing modes
// _lp: the line being parsed as a LineParser
// _parsedLine: the line being parsed (for facility: can be built from _lp)
// _linePos: start position in the original chunk of the line being parsed 
// _parsedParents: the parsed line's parent, grand-parent, etc.. up to the root
//                 The number of items is also the parsed depth (so 1-based)
// _newChunk: the chunk being built (while parsing)
// _updates: number of updates in comparison with the original chunk
//
// Return values: 
// - true if the chunk has been altered 
//   => THE LINE BEING PARSED IS REPLACED WITH _newChunk
// - false otherwise
//   => THE LINE BEING PARSED REMAINS AS IT IS
//
// Those callbacks are *always* triggered, except NotifyChunkLine() that 
// is triggered depending on Parse() or ParsePatch() parameters/criteria 
// => for optimization: the more criteria, the less calls!
///////////////////////////////////////////////////////////////////////////////

virtual void NotifyStartChunk(int _mode) {}

virtual void NotifyEndChunk(int _mode) {}

virtual bool NotifyStartElement(int _mode, 
	LineParser* _lp, const char* _parsedLine,  int _linePos, 
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates) {return false;} // no update

virtual bool NotifyEndElement(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos,
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates) {return false;} // no update

virtual bool NotifyChunkLine(int _mode, 
	LineParser* _lp, const char* _parsedLine, int _linePos, 
	int _parsedOccurence, WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates) {return false;} // no update

virtual bool NotifySkippedSubChunk(int _mode, 
	const char* _subChunk, int _subChunkLength, int _subChunkPos,
	WDL_PtrList<WDL_FastString>* _parsedParents, 
	WDL_FastString* _newChunk, int _updates) {return false;} // no update


///////////////////////////////////////////////////////////////////////////////
private:

// just to avoid duplicate strcmp() calls in ParsePatchCore()
void IsMatchingParsedLine(bool* _tolerantMatch, bool* _strictMatch, 
		int _expectedDepth, int _parsedDepth,
		const char* _expectedParent, const char* _parsedParent,
		const char* _expectedKeyword, const char* _parsedKeyword)
{
	*_tolerantMatch = false;
	*_strictMatch = false;

	if (_expectedDepth == -1)
		*_tolerantMatch = true;
	else if (_expectedDepth == _parsedDepth) {
		if (!_expectedParent)
			*_tolerantMatch = true;
		else if (!strcmp(_parsedParent, _expectedParent)) {
			if (!_expectedKeyword)
				*_tolerantMatch = true;
			else if (!strcmp(_parsedKeyword, _expectedKeyword))
				*_strictMatch = *_tolerantMatch = true;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// ParsePatchCore()
// Globally, the func is tolerant; the less parameters provided, the more parsed
// lines will be notified to inherited instances (through NotifyChunkLine()) or, 
// when it's used directly, the more lines will be read/altered.
// Examples: parse all lines, is the FX n bypassed under parent 'FXCHAIN'? etc..
// Note: sometimes there are dependencies between parameters (most of the time 
//       with _mode), must return -1 if it's not respected.
// Parameters: see below. 
// Return values:
//   Always return -1 on error/bad usage,
//   or returns the number of updates done when altering (0 nothing done),
//   or returns the first found position +1 in the chunk (0 reserved for "not found")
///////////////////////////////////////////////////////////////////////////////

int ParsePatchCore(
	bool _write,        // optimization flag (if false: no re-copy)
	int _mode,          // can be <0 for custom modes (useful for inheritance)
	int _depth,         // usually 1-based but 0 is allowed (e.g. for .rfxchain files that do not start with "<...")
	const char* _expectedParent, 
	const char* _keyWord, 
	int _occurence,     // 0-based (-1: ignored, all occurrences notified)
	int _tokenPos,      // 0-based (-1: ignored, may be mandatory depending on _mode)
	void* _value,       // value to get/set (NULL: ignored)
	void* _valueExcept, // value to get/set for the "except case" (NULL: ignored)
	const char* _breakKeyword = NULL) // // for optimization, optional (if specified, processing is stopped when this keyword is encountred - be carefull with that!)
{
#ifdef _SNM_DEBUG 
	// check params
	if (_occurence < 0 &&
		(_mode == SNM_GET_CHUNK_CHAR || _mode == SNM_GET_SUBCHUNK_OR_LINE || _mode == SNM_GET_SUBCHUNK_OR_LINE_EOL))
		return -1;
	if ((!_value || _tokenPos < 0) && 
		(_mode == SNM_SET_CHUNK_CHAR || _mode == SNM_SETALL_CHUNK_CHAR_EXCEPT || _mode == SNM_GETALL_CHUNK_CHAR_EXCEPT))
		return -1;
	if (_tokenPos < 0 && _mode == SNM_GET_CHUNK_CHAR)
		return -1;
	// sub-chunk processing: depth must always be provided but sometimes _value can be optional
	if ((!_value || _depth <= 0) && _mode == SNM_REPLACE_SUBCHUNK_OR_LINE)
		return -1;
	// sub-chunk processing: depth must always be provided 
	if (_depth <= 0 && (_mode == SNM_GET_SUBCHUNK_OR_LINE || _mode == SNM_GET_SUBCHUNK_OR_LINE_EOL))
		return -1;
	// count keywords: no occurrence should be specified
	if (_mode == SNM_COUNT_KEYWORD && _occurence != -1)
		return -1;
#endif

	// get/cache the chunk
	const char* cData = GetChunk() ? GetChunk()->Get() : NULL;
	if (!cData)
		return -1;

	NotifyStartChunk(_mode);

	LineParser lp(false);
	WDL_FastString* newChunk = _write ? new WDL_FastString(SNM_HEAPBUF_GRANUL) : NULL; 
	char curLine[SNM_MAX_CHUNK_LINE_LENGTH] = "";
	int updates = 0, occurence = 0, posStartOfSubchunk = -1, linePos, curLineLen;
	WDL_FastString* subChunkKeyword = NULL;
	WDL_PtrList_DeleteOnDestroy<WDL_FastString> parents;
	const char* pEOL = cData-1, *keyword, *pLine, *pEOSkippedChunk;
	bool alter, tolerantMatch, strictMatch;

	m_isParsingSource = false;
	for(;;)
	{
		pLine = pEOL+1;
		pEOL = strchr(pLine, '\n');

		// break conditions
		if (!pEOL)
			break;

		if (m_breakParsePatch)
		{
			if (_write)
				newChunk->Append(pLine);
			break;
		}

		// will avoid strlen() calls..
		curLineLen = (int)(pEOL-pLine);


		// *** optional optimization: skip some data and sub-chunks ***
		pEOSkippedChunk = NULL;

		// skip base64 data (e.g. FX states, sysEx events, ..)
		if (!m_processBase64 &&
			curLineLen>2 && *(pEOL-1)=='=' && *(pEOL-2)=='=')
		{
			pEOSkippedChunk = strstr(pLine, ">\n");
		}
		// skip in-project MIDI data
		else if (!m_processInProjectMIDI && m_isParsingSource && (
			(curLineLen>2 && !_strnicmp(pLine, "E ", 2)) ||
			(curLineLen>3 && !_strnicmp(pLine, "Em ", 3))))
		{
			pEOSkippedChunk = strstr(pLine, "GUID {");
		}
		// skip track freeze sub-chunks
		else if (!m_processFreeze && parents.GetSize()==1 && 
			curLineLen>8 && !strncmp(pLine, "<FREEZE ", 8))
		{
			int skippedLen = FindEndOfSubChunk(pLine, 0);
			while (skippedLen >= 0) // in case of multiple freeze
			{
				pEOSkippedChunk = (char*)(pLine+skippedLen);
				if (!strncmp(pEOSkippedChunk, "<FREEZE ", 8))
					skippedLen = FindEndOfSubChunk(pLine, skippedLen);
				else
					skippedLen = -1;
			}
		}
		
		if (pEOSkippedChunk) 
		{
			bool alter = NotifySkippedSubChunk(_mode, pLine, (int)(pEOSkippedChunk-pLine), (int)(pLine-cData), &parents, newChunk, updates);
			alter |= (subChunkKeyword && _mode == SNM_REPLACE_SUBCHUNK_OR_LINE);
			if (_write && !alter)
				newChunk->Insert(pLine, newChunk->GetLength(), (int)(pEOSkippedChunk-pLine));
			if ((_mode == SNM_GET_SUBCHUNK_OR_LINE || _mode == SNM_GET_SUBCHUNK_OR_LINE_EOL) && subChunkKeyword && _value)
				((WDL_FastString*)_value)->Insert(pLine, ((WDL_FastString*)_value)->GetLength(), (int)(pEOSkippedChunk-pLine));

			pLine = pEOSkippedChunk;
			pEOL = strchr(pEOSkippedChunk, '\n');
			curLineLen = (int)(pEOL-pLine);
		}


		// get the next line (trimmed if too long)
		alter = false;
		memcpy(curLine, pLine, curLineLen >= SNM_MAX_CHUNK_LINE_LENGTH ? SNM_MAX_CHUNK_LINE_LENGTH-1 : curLineLen);
		curLine[curLineLen >= SNM_MAX_CHUNK_LINE_LENGTH ? SNM_MAX_CHUNK_LINE_LENGTH-1 : curLineLen] = '\0';
		linePos = (int)(pLine-cData);

		// zap this line?
		if (lp.parse(curLine) || !lp.getnumtokens())
			continue;

		// zap this line?
		keyword = lp.gettoken_str(0);
		if (!*keyword)
			continue;

		// sub chunk?
		if (*keyword == '<')
		{
			m_isParsingSource |= (lp.getnumtokens()==2 && curLineLen>9 /* e.g. <SOURCE MIDI*/ && !strcmp(keyword+1, "SOURCE"));

			// notify & update parent list
			parents.Add(new WDL_FastString(keyword+1)); // +1 to zap '<'
			alter = NotifyStartElement(_mode, &lp, curLine, linePos, &parents, newChunk, updates);
		}
		// end of sub chunk?
		else if (*keyword == '>')
		{
			// end of processed sub-chunk ?
			if (subChunkKeyword && _depth == parents.GetSize() && 
				subChunkKeyword == parents.Get(_depth-1)) // => valid _depth must be provided
			{
				subChunkKeyword = NULL;

				// don't recopy '>' depending on the modes..
				if (_mode == SNM_REPLACE_SUBCHUNK_OR_LINE) {
					alter = true;
					m_breakParsePatch = (_occurence != -1);
				}
				// .. but recopy it for some
				else if (_mode == SNM_GET_SUBCHUNK_OR_LINE || _mode == SNM_GET_SUBCHUNK_OR_LINE_EOL)
				{
					if (_value) 
						((WDL_FastString*)_value)->Append(">\n",2);

					// SNM_GET_SUBCHUNK_OR_LINE:
					// => returns 1st *KEYWORD* position of the sub-chunk + 1 (0 reserved for "not found")
					// otherwise
					// => return the *EOL* position of the sub-chunk + 1 (0 reserved for "not found")
					return (_mode == SNM_GET_SUBCHUNK_OR_LINE ? posStartOfSubchunk : (int)(pEOL-cData+1));
				}
			}

			if (m_isParsingSource) 
				m_isParsingSource = !!strcmp(GetParent(&parents), "SOURCE");

			// notify & update parent list
			alter |= NotifyEndElement(_mode, &lp, curLine, linePos, &parents, newChunk, updates);
			parents.Delete(parents.GetSize()-1, true);
		}

		// end of chunk lines (">") are not processed/notified (but copied if needed!)
		if (parents.GetSize())
		{
			WDL_FastString* currentParent = parents.Get(parents.GetSize()-1);

			IsMatchingParsedLine(
				&tolerantMatch, &strictMatch, 
				_depth, parents.GetSize(), 
				_expectedParent, currentParent->Get(), 
				_keyWord, keyword);

			if (tolerantMatch && _mode < 0)
			{
				if (_occurence == occurence || _occurence == -1)
					alter |= NotifyChunkLine(_mode, &lp, curLine, linePos, occurence, &parents, newChunk, updates); 
				occurence++;
			}
			else if (strictMatch && _mode >= 0)
			{
				// this occurrence match
				if (_occurence == occurence || _occurence == -1)
				{
					switch (_mode)
					{
						case SNM_GET_CHUNK_CHAR:
						{
							if (_value) strcpy((char*)_value, lp.gettoken_str(_tokenPos));
							const char* p = strstr(pLine, _keyWord);
							// returns the *KEYWORD* position + 1 ('cause 0 reserved for "not found")
							return (p ? ((int)(p-cData+1)) : -1); 
						}
						case SNM_SET_CHUNK_CHAR:
							alter |= WriteChunkLine(newChunk, (char*)_value, _tokenPos, &lp); 
							m_breakParsePatch = (_occurence != -1);
							break;
						case SNM_SETALL_CHUNK_CHAR_EXCEPT:
						case SNM_TOGGLE_CHUNK_INT_EXCEPT:
							if (_valueExcept)
							{
/* done in WriteChunkLine()
								if (strcmp((char*)_valueExcept, lp.gettoken_str(_tokenPos)))
*/
									alter |= WriteChunkLine(newChunk, (char*)_valueExcept, _tokenPos, &lp); 
							}
							break;
						case SNM_PARSE_AND_PATCH:
						case SNM_PARSE:
							alter |= NotifyChunkLine(_mode, &lp, curLine, linePos, occurence, &parents, newChunk, updates);
							m_breakParsePatch = (_occurence != -1);
							break;
						case SNM_TOGGLE_CHUNK_INT:
						{
							char bufConv[16] = "";
							int l = snprintf(bufConv, sizeof(bufConv), "%d", !lp.gettoken_int(_tokenPos));
							if (l<=0 || l>=16) break;
							alter |= WriteChunkLine(newChunk, bufConv, _tokenPos, &lp); 
							m_breakParsePatch = (_occurence != -1);
						}
						break; 
						case SNM_REPLACE_SUBCHUNK_OR_LINE:
							newChunk->Append((const char*)_value);
							if (*_keyWord == '<') subChunkKeyword = currentParent;
							alter=true;
							break;
						case SNM_GET_SUBCHUNK_OR_LINE:
						{
							// faster than AppendFormatted(, "%s\n",);
							if (_value) {
								((WDL_FastString*)_value)->Append(curLine);
								((WDL_FastString*)_value)->Append("\n");
							}
							const char* pSub = strstr(pLine, _keyWord);
							// *KEYWORD* position + 1 (0 reserved for "not found")
							posStartOfSubchunk = (pSub ? ((int)(pSub-cData+1)) : -1);
							if (_value && *_keyWord == '<') subChunkKeyword = currentParent;
							else return posStartOfSubchunk; 
						}
						break;
						case SNM_GET_SUBCHUNK_OR_LINE_EOL:
						{
							if (_value) {
								((WDL_FastString*)_value)->Append(curLine);
								((WDL_FastString*)_value)->Append("\n");
							}
							if (*_keyWord == '<') // no test on _value: has to go to the end of subchunk
								subChunkKeyword = currentParent;
							else return ((int)(pEOL-cData+1)); // *EOL* position + 1 (0 reserved for "not found")
						}
						break;
						case SNM_D_ADD:
						case SNM_D_MUL:
						{
							int success; double d = lp.gettoken_float(_tokenPos, &success);
							if (success) {
								if (_mode == SNM_D_ADD) d += *(double*)_value;
								else d *= *(double*)_value;
								char bufConv[326]{};
								int l = snprintf(bufConv, sizeof(bufConv), "%.14f", d);
								if (l<=0 || l>=64) break;
								alter |= WriteChunkLine(newChunk, bufConv, _tokenPos, &lp); 
								m_breakParsePatch = (_occurence != -1);
							}
						}
						break; 
					}
				}
				// this occurrence doesn't match
				else 
				{
					switch (_mode)
					{
						case SNM_SETALL_CHUNK_CHAR_EXCEPT:
/* done in WriteChunkLine()
							if (strcmp((char*)_value, lp.gettoken_str(_tokenPos)))
*/
								alter |= WriteChunkLine(newChunk, (char*)_value, _tokenPos, &lp); 
							break;
						case SNM_GETALL_CHUNK_CHAR_EXCEPT:
							if (strcmp((char*)_value, lp.gettoken_str(_tokenPos)))
								return 0;
							break;
						case SNM_PARSE_AND_PATCH_EXCEPT:
						case SNM_PARSE_EXCEPT:
							alter |= NotifyChunkLine(_mode, &lp, curLine, linePos, occurence, &parents, newChunk, updates); 
							break;
						case SNM_TOGGLE_CHUNK_INT_EXCEPT:
						{
							char bufConv[16] = "";
							int l = snprintf(bufConv, sizeof(bufConv), "%d", !lp.gettoken_int(_tokenPos));
							if (l<=0 || l>=16) break;
							alter |= WriteChunkLine(newChunk, bufConv, _tokenPos, &lp); 
						}
						break;
					}
				}
				occurence++;
			}
			// breaking keyword? (brutal: no check on depth, parent, etc..)
			else if (!subChunkKeyword && _keyWord && _breakKeyword && !strcmp(keyword, _breakKeyword))
			{
				m_breakParsePatch = true;
			}
			// are we in a sub-chunk?
			else if (subChunkKeyword) 
			{
				alter = (_mode == SNM_REPLACE_SUBCHUNK_OR_LINE);
				if (_value && (_mode == SNM_GET_SUBCHUNK_OR_LINE || _mode == SNM_GET_SUBCHUNK_OR_LINE_EOL)) {
					((WDL_FastString*)_value)->Append(curLine);
					((WDL_FastString*)_value)->Append("\n");
				}
			}
		}
		updates += (_write && alter);

		// copy current line only in RW mode & if it wasn't altered above
		if (_write && !alter) {
			newChunk->Append(curLine);
			newChunk->Append("\n");
		}
	}

	// update cache if needed
	if (_write && newChunk)
	{
		if (updates && newChunk->GetLength())
		{
			m_updates += updates;

			// avoids buffer re-copy
			WDL_FastString* oldChunk = m_chunk;
			m_chunk = newChunk;
			delete oldChunk;
		}
		else
			delete newChunk;
	}

	NotifyEndChunk(_mode);

	// return values
	int retVal = -1;
	switch (_mode)
	{
		// *** READ ONLY ***
		case SNM_GET_CHUNK_CHAR:
		case SNM_GET_SUBCHUNK_OR_LINE:
		case SNM_GET_SUBCHUNK_OR_LINE_EOL:
			retVal = 0; // if we're here: not found
			break;
		case SNM_GETALL_CHUNK_CHAR_EXCEPT:
			retVal = 1; // if we're here: found (returns 0 on 1st unmatching)
			break;
		case SNM_PARSE:
		case SNM_PARSE_EXCEPT:
			retVal = 1; // read ok..
			break;
		case SNM_COUNT_KEYWORD:
			retVal = occurence;
			break;

		// *** R/W ***
		case SNM_PARSE_AND_PATCH:
		case SNM_PARSE_AND_PATCH_EXCEPT:
		case SNM_SET_CHUNK_CHAR:
		case SNM_SETALL_CHUNK_CHAR_EXCEPT:
		case SNM_TOGGLE_CHUNK_INT:
		case SNM_TOGGLE_CHUNK_INT_EXCEPT:
		case SNM_REPLACE_SUBCHUNK_OR_LINE:
		case SNM_D_ADD:
		case SNM_D_MUL:
			retVal = updates;
			break;

		default: // for custom _mode (e.g. <0)
			retVal = (_write ? updates : 0);
			break;
	}

	m_breakParsePatch = false; // safer (if inherited classes forget to do it)
	return retVal;
}

};


///////////////////////////////////////////////////////////////////////////////
// Fast chunk helpers
///////////////////////////////////////////////////////////////////////////////

static int RemoveChunkLines(char* _chunk, const char* _searchStr, bool _checkBOL, int _checkEOLChar)
{
	if (!_chunk || !_searchStr)
		return 0;

	int updates = 0;
	char* idStr = strstr(_chunk, _searchStr);
	while(idStr) 
	{
		char* eol = strchr(idStr, '\n');
		char* bol = idStr;
		while (*bol && bol > _chunk && *bol != '\n') bol--;
		if (eol && bol && (*bol == '\n' || bol == _chunk) &&
			// additional optional checks (safety)
			(!_checkEOLChar || *((char*)(eol-1)) == _checkEOLChar) &&
			(!_checkBOL || idStr == (char*)(bol + ((bol == _chunk ? 0 : 1)))))
		{
			updates++;
			if (bol != _chunk) bol++;
			memset(bol,' ', (int)(eol-bol)); // REAPER supports blank lines, much faster than memmove()
			idStr = strstr(bol, _searchStr);
		}
		else 
			idStr = strstr((char*)(idStr+1), _searchStr);
	}
	return updates;
}

static int RemoveChunkLines(char* _chunk, WDL_PtrList<const char>* _searchStrs, bool _checkBOL, int _checkEOLChar) {
	int updates = 0;
	if (_chunk && _searchStrs) {
		// faster than parsing+checking each keyword
		for (int i=0; i < _searchStrs->GetSize(); i++)
			updates += RemoveChunkLines(_chunk, _searchStrs->Get(i), _checkBOL, _checkEOLChar);
	}
	return updates;
}

// WDL_FastString wrappers
static int RemoveChunkLines(WDL_FastString* _chunk, const char* _searchStr, bool _checkBOL, int _checkEOLChar) {
	// cast to char* OK here: the WDL_FastString length is not updated
	return (_chunk ? RemoveChunkLines((char*)_chunk->Get(), _searchStr, _checkBOL, _checkEOLChar) : 0);
}
static int RemoveChunkLines(WDL_FastString* _chunk, WDL_PtrList<const char>* _searchStrs, bool _checkBOL, int _checkEOLChar) {
	// cast to char* OK here: the WDL_FastString length is not updated
	return (_chunk ? RemoveChunkLines((char*)_chunk->Get(), _searchStrs, _checkBOL, _checkEOLChar) : 0);
}

// returns the end position of the sub-chunk starting at _startPos in _chunk (-1 if failed)
// no deep checks here: faster but it relies on the chunk consistency (+ left-trimed lines)
static int FindEndOfSubChunk(const char* _chunk, int _startPos)
{
	if (_chunk && _startPos >= 0) {
		int depth = 1;
		const char* boc = strstr(_chunk+_startPos+1, "\n<");
		const char* eoc = strstr(_chunk+_startPos+1, "\n>\n");
		while ((boc || eoc) && depth > 0) {
			if (boc && (!eoc || boc < eoc)) {
				depth++;
				boc = strstr(boc+1, "\n<");
			}
			else if (eoc && (!boc || eoc < boc)) {
				depth--;
				if (!depth)
					return (int)(eoc-_chunk+3); // +3 to zap "\n>\n" 
				eoc = strstr((char*)(eoc+1), "\n>\n");
			}
		}
	}
	return -1;
}

// returns to pointer to the next valid keyword in _chunk
// returns a valid left trimmed line pointers, separators and empty lines are skipped
static const char* FindKeyword(const char* _chunk) {
	for(;;) {
		_chunk++;
		if (!*_chunk) break;
		while ((*_chunk==' ' || *_chunk=='\t')) _chunk++;
		if (!*_chunk) break;
		else if (*_chunk=='\n' || *_chunk=='\r') { continue; }
		return _chunk;
	}
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// Other static helpers
///////////////////////////////////////////////////////////////////////////////

extern int g_disable_chunk_guid_filtering;
static int SNM_PreObjectState(WDL_FastString* _str, bool _wantsMinState)
{
	// when altering: remove all ids 
	if (_str)
	{
		if (!g_disable_chunk_guid_filtering)
			RemoveAllIds(_str);
	}
	// when getting: enables/disables the "VST full state" pref (that also minimize AU states, if REAPER >= v4)
	// it also fixes possible incomplete chunk bug (pref overrided when needed)
	else if (ConfigVar<int> fxstate = "vstfullstate") {
		int old = *fxstate;
		int tmp = _wantsMinState ? *fxstate&0xFFFFFFFE : *fxstate|1;
		if (old != tmp) // prevents useless RW access to REAPER.ini
			*fxstate = tmp;
		return old;
	}
	return -1;
}

static void SNM_PostObjectState(int _oldfxstate)
{
	// restore the "VST full state" pref, if needed
	if (_oldfxstate >= 0)
		if (ConfigVar<int> fxstate = "vstfullstate")
			if (*fxstate != _oldfxstate)
				*fxstate = _oldfxstate;
}

static SNM_ChunkParserPatcher* SNM_FindCPPbyObject(WDL_PtrList<SNM_ChunkParserPatcher>* _list, void* _object)
{
	if (_list && _object)
		for (int i=0; i < _list->GetSize(); i++)
			if (_list->Get(i)->GetObject() == _object)
				return _list->Get(i);
	return NULL;
}

#endif
