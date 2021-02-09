/******************************************************************************
/ BR_Util.cpp
/
/ Copyright (c) 2013-2015 Dominik Martin Drzic
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

#include "BR_Util.h"
#include "BR.h"
#include "BR_EnvelopeUtil.h"
#include "BR_MidiUtil.h"
#include "BR_Misc.h"
#include "../SnM/SnM.h"
#include "../SnM/SnM_Chunk.h"
#include "../SnM/SnM_Dlg.h"
#include "../SnM/SnM_Item.h"
#include "../SnM/SnM_Util.h"

#include <WDL/localize/localize.h>
#include <WDL/lice/lice.h>
#include <WDL/lice/lice_bezier.h>
#include <WDL/projectcontext.h>

/******************************************************************************
* Constants                                                                   *
******************************************************************************/
const int TCP_MASTER_GAP        = 5;
const int ITEM_LABEL_MIN_HEIGHT = 28;
const int ENV_GAP               = 4;    // bottom gap may seem like 3 when selected, but that
const int ENV_LINE_WIDTH        = 1;    // first pixel is used to "bold" selected envelope

const int TAKE_MIN_HEIGHT_COUNT = 10;
const int TAKE_MIN_HEIGHT_HIGH  = 12;   // min height when take count <= TAKE_MIN_HEIGHT_COUNT
const int TAKE_MIN_HEIGHT_LOW   = 6;    // min height when take count >  TAKE_MIN_HEIGHT_COUNT

const int PROJ_CONTEXT_LINE     = 4096; // same length used by ProjectContext

/******************************************************************************
* Miscellaneous                                                               *
******************************************************************************/
vector<int> GetDigits (int val)
{
	int count = (int)(log10((float)val)) + 1;

	vector<int> digits;
	digits.resize(count);

	for (int i = count-1; i >= 0; --i)
	{
		digits[i] = GetLastDigit(val);
		val /= 10;
	}
	return digits;
}

int GetFirstDigit (int val)
{
	val = abs(val);
	while (val >= 10)
		val /= 10;
	return val;
}

int GetLastDigit (int val)
{
	return abs(val) % 10;
}

int BinaryToDecimal (const char* binaryString)
{
	int base10 = 0;
	for (size_t i = 0; i < strlen(binaryString); ++i)
	{
		int digit = binaryString[i] - '0';
		if (digit == 0 || digit == 1)
			base10 = base10 << 1 | digit;
	}
	return base10;
}

int GetBit (int val, int bit)
{
	return (val & 1 << bit) != 0;
}

int SetBit (int val, int bit, bool set)
{
	if (set) return SetBit(val, bit);
	else     return ClearBit(val, bit);
}

int SetBit (int val, int bit)
{
	return val | 1 << bit;
}

int ClearBit (int val, int bit)
{
	return val & ~(1 << bit);
}

int ToggleBit (int val, int bit)
{
	return val ^= 1 << bit;
}

int RoundToInt (double val)
{
	if (val < 0)
		return (int)(val - 0.5);
	else
		return (int)(val + 0.5);
}

int TruncToInt (double val)
{
	return (int)val;
}

double Round (double val)
{
	return (double)(int)(val + ((val < 0) ? (-0.5) : (0.5)));
}

double RoundToN (double val, double n)
{
	double shift = pow(10.0, n);
	return RoundToInt(val * shift) / shift;
}

double Trunc (double val)
{
	return (val < 0) ? ceil(val) : floor(val);
}

double TranslateRange (double value, double oldMin, double oldMax, double newMin, double newMax)
{
	double oldRange = oldMax - oldMin;
	double newRange = newMax - newMin;
	double newValue = ((value - oldMin) * newRange / oldRange) + newMin;

	return SetToBounds(newValue, newMin, newMax);
}

double AltAtof (char* str)
{
	replace(str, str+strlen(str), ',', '.' );
	return atof(str);
}

bool IsFraction (char* str, double& convertedFraction)
{
	int size = strlen(str);
	replace(str, str + size, ',', '.' );
	char* buf = strstr(str,"/");

	if (!buf)
	{
		convertedFraction = atof(str);
		snprintf(str, size + 1, "%g", convertedFraction);
		return false;
	}
	else
	{
		int num = atoi(str);
		int den = atoi(buf+1);
		snprintf(str, size + 1, "%d/%d", num, den);
		if (den != 0)
			convertedFraction = (double)num/(double)den;
		else
			convertedFraction = 0;
		return true;
	}
}

void ReplaceAll (string& str, string oldStr, string newStr)
{
	if (oldStr.empty())
		return;

	size_t pos = 0, oldLen = oldStr.length(), newLen = newStr.length();
	while ((pos = str.find(oldStr, pos)) != std::string::npos)
	{
		str.replace(pos, oldLen, newStr);
		pos += newLen;
	}
}

void AppendLine (WDL_FastString& str, const char* line)
{
	str.Append(line);
	str.Append("\n");
}

const char* strstr_last (const char* haystack, const char* needle)
{
	if (!haystack || !needle || *needle == '\0')
		return haystack;

	const char *result = NULL;
	while (true)
	{
		const char *p = strstr(haystack, needle);
		if (!p) break;
		result   = p;
		haystack = p + 1;
	}
	return result;
}

/******************************************************************************
* General                                                                     *
******************************************************************************/
vector<double> GetProjectMarkers (bool timeSel, double timeSelDelta /*=0*/)
{
	double tStart, tEnd;
	GetSet_LoopTimeRange2(NULL, false, false, &tStart, &tEnd, false);

	int i = 0;
	bool region;
	double pos;
	vector<double> markers;
	while (EnumProjectMarkers2(NULL, i, &region, &pos, NULL, NULL, NULL))
	{
		bool pRegion;
		double pPos;
		int previous = EnumProjectMarkers2(NULL, i-1, &pRegion, &pPos, NULL, NULL, NULL);
		if (!region)
		{
			// In certain corner cases involving regions, reaper will output the same marker more than
			// once. This should prevent those duplicate values from being written into the vector.
			// Note: this might have been fixed in 4.60, but it doesn't hurt to leave it...
			if (pos != pPos || (pos == pPos && pRegion) || !previous)
			{
				if (timeSel)
				{
					if (pos > tEnd + timeSelDelta)
						break;
					if (pos >= tStart - timeSelDelta)
						markers.push_back(pos);
				}
				else
					markers.push_back(pos);
			}
		}
		++i;
	}
	return markers;
}

WDL_FastString FormatTime (double position, int mode /*=-1*/)
{
	WDL_FastString string;
	const int projTimeMode = ConfigVar<int>("projtimemode").value_or(0);

	if (mode == 1 || (mode == -1 && projTimeMode == 1))
	{
		char formatedPosition[128];
		format_timestr_pos(position, formatedPosition, sizeof(formatedPosition), 2);
		string.AppendFormatted(sizeof(formatedPosition), "%s", formatedPosition);

		format_timestr_pos(position, formatedPosition, sizeof(formatedPosition), 0);
		string.AppendFormatted(sizeof(formatedPosition), " / %s", formatedPosition);
	}
	else
	{
		char formatedPosition[128];
		format_timestr_pos(position, formatedPosition, sizeof(formatedPosition), mode);
		string.AppendFormatted(sizeof(formatedPosition), "%s", formatedPosition);
	}

	return string;
}

const char *GetCurrentTheme (std::string* themeNameOut)
{
	const char *fullThemePath = GetLastColorThemeFile();

	if (themeNameOut) {
		std::string fileName(strlen(fullThemePath), '\0');
		GetFilenameNoExt(fullThemePath, &fileName[0], fileName.size()+1);
		std::swap(fileName, *themeNameOut);
	}

	return fullThemePath;
}

int FindClosestProjMarkerIndex (double position)
{
	int first = 0;
	int last  = CountProjectMarkers(NULL, NULL, NULL);
	if (last < 0)
		return -1;

	// Find previous marker/region
	while (first != last)
	{
		int mid = (first + last) /2;
		double currentPos; EnumProjectMarkers3(NULL, mid, NULL, &currentPos, NULL, NULL, NULL, NULL);

		if (currentPos < position) first = mid + 1;
		else                       last  = mid;
	}
	int prevId = first - ((first == 0) ? 0 : 1);

	// Make sure we get marker index, not region
	int id = prevId + 1; bool region; double prevPosition;
	while (EnumProjectMarkers3(NULL, --id, &region, &prevPosition, NULL, NULL, NULL, NULL))
	{
		if (!region)
			break;
	}
	if (id < 0)
		return -1;

	// Check against next marker
	int nextId = prevId;
	double nextPosition;
	while (EnumProjectMarkers3(NULL, ++nextId, &region, &nextPosition, NULL, NULL, NULL, NULL))
	{
		if (!region)
		{
			if (abs(nextPosition - position) < abs(position - prevPosition)) id = nextId;
			if (id >= CountProjectMarkers(NULL, NULL, NULL))                 id = -1;
			break;
		}
	}

	return id;
}

int CountProjectTabs ()
{
	int count = 0;
	while (EnumProjects(count, NULL, 0))
		count++;
	return count;
}

double EndOfProject (bool markers, bool regions)
{
	double projEnd = 0;
	int tracks = GetNumTracks();
	for (int i = 0; i < tracks; ++i)
	{
		MediaTrack* track = GetTrack(0, i);
		MediaItem* item = GetTrackMediaItem(track, GetTrackNumMediaItems(track) - 1);
		double itemEnd = GetMediaItemInfo_Value(item, "D_POSITION") + GetMediaItemInfo_Value(item, "D_LENGTH");
		if (itemEnd > projEnd)
			projEnd = itemEnd;
	}

	if (markers || regions)
	{
		bool region; double start, end; int i = 0;
		while ((i = EnumProjectMarkers(i, &region, &start, &end, NULL, NULL)))
		{
			if (regions)
				if (region && end > projEnd)
					projEnd = end;
			if (markers)
				if (!region && start > projEnd)
					projEnd = start;
		}
	}
	return projEnd;
}

double GetMidiOscVal (double min, double max, double step, double currentVal, int commandVal, int commandValhw, int commandRelmode)
{
	double returnVal = currentVal;

	// 14-bit resolution (MIDI pitch...OSC doesn't seem to work)
	if (commandValhw >= 0)
	{
		returnVal = TranslateRange(SetToBounds((double)(commandValhw | commandVal << 7), 0.0, 16383.0), 0, 16383, min, max);
	}
	// MIDI
	else if (commandValhw == -1 && commandVal >= 0 && commandVal < 128)
	{
		// Absolute mode
		if (!commandRelmode)
			returnVal = TranslateRange(commandVal, 0, 127, min, max);
		// Relative modes
		else
		{
			if      (commandRelmode == 1) {if (commandVal >= 0x40) commandVal |= ~0x3f;}               // sign extend if 0x40 set
			else if (commandRelmode == 2) {commandVal -= 0x40;}                                        // offset by 0x40
			else if (commandRelmode == 3) {if (commandVal &  0x40) commandVal = -(commandVal & 0x3f);} // 0x40 is sign bit
			else                                                   commandVal = 0;

			returnVal = currentVal + (commandVal * step);
		}
	}
	return SetToBounds(returnVal, min, max);
}

double GetPositionFromTimeInfo (int hours, int minutes, int seconds, int frames)
{
	WDL_FastString timeString;
	timeString.AppendFormatted(256, "%d:%d:%d:%d", hours, minutes, seconds, frames);

	return parse_timestr_len(timeString.Get(), 0, 5);
}

void GetTimeInfoFromPosition (double position, int* hours, int* minutes, int* seconds, int* frames)
{
	char timeStr[512];
	format_timestr_len(position, timeStr, sizeof(timeStr), 0, 5);

	WDL_FastString str[4];
	int i     = -1;
	int strId = 0;
	while (timeStr[++i])
	{
		if (timeStr[i] == ':') ++strId;
		else                   str[strId].AppendRaw(&timeStr[i], 1);
	}

	WritePtr(hours,   ((str[0].GetLength() > 0) ? atoi(str[0].Get()) : 0));
	WritePtr(minutes, ((str[1].GetLength() > 0) ? atoi(str[1].Get()) : 0));
	WritePtr(seconds, ((str[2].GetLength() > 0) ? atoi(str[2].Get()) : 0));
	WritePtr(frames,  ((str[3].GetLength() > 0) ? atoi(str[3].Get()) : 0));
}

void GetSetLastAdjustedSend (bool set, MediaTrack** track, int* sendId, BR_EnvType* type)
{
	static MediaTrack* s_lastTrack = NULL;
	static int         s_lastId    = -1;
	static BR_EnvType  s_lastType  = UNKNOWN;

	if (set)
	{
		s_lastTrack = *track;
		s_lastId    = *sendId;
		s_lastType  = *type;
	}
	else
	{
		WritePtr(track,  s_lastTrack);
		WritePtr(sendId, s_lastId);
		WritePtr(type,   s_lastType);
	}
}

void GetSetFocus (bool set, HWND* hwnd, int* context)
{
	if (set)
	{
		HWND currentHwnd;                                   // check for current focus (using SetCursorContext() can focus arrange
		int currentContext;                                 // and then SetFocus() can change it to midi editor which was already
		GetSetFocus(false, &currentHwnd, &currentContext);  // focused, and will make it flicker due to the brief focus change

		if (context && *context != currentContext) // context first (it may refocus main window)
		{
			TrackEnvelope* envelope = GetSelectedEnvelope(NULL);
			SetCursorContext((*context == 2 && !envelope) ? 1 : *context, (*context == 2) ? envelope : NULL);
		}
		if (hwnd && *hwnd != currentHwnd)
			SetFocus(*hwnd);
	}
	else
	{
		WritePtr(hwnd,    GetFocus());
		WritePtr(context, GetCursorContext2(true)); // last_valid_focus: example - select item and focus mixer. Pressing delete
	}                                               // tells us context is still 1, but GetCursorContext2(false) would return 0
}

void SetAllCoordsToZero (RECT* r)
{
	if (r)
	{
		r->top    = 0;
		r->bottom = 0;
		r->left   = 0;
		r->right  = 0;
	}
}

void RegisterCsurfPlayState (bool set, void (*CSurfPlayState)(bool,bool,bool), const vector<void(*)(bool,bool,bool)>** registeredFunctions /*= NULL*/, bool cleanup /*= false*/)
{
	static vector<void(*)(bool,bool,bool)> s_functions;

	if (CSurfPlayState)
	{
		if (set)
		{
			bool found = false;
			for (size_t i = 0; i < s_functions.size(); ++i)
			{
				if (s_functions[i] == CSurfPlayState)
				{
					found = true;
					break;
				}
			}
			if (!found)
				s_functions.push_back(CSurfPlayState);
		}
		else
		{
			for (size_t i = 0; i < s_functions.size(); ++i)
			{
				if (s_functions[i] == CSurfPlayState)
					s_functions[i] = NULL;
			}
		}
	}

	// Because CSurfPlayState can remove itself from vector (and mess stuff up in BR_CSurfSetPlayState when iterating over all stored functions), instead of deleting
	// functions from vector as soon as they are deregistered, we set them to NULL and erase afterward from BR_CSurfSetPlayState once we called all of them
	if (cleanup)
	{
		for (size_t i = 0; i < s_functions.size(); ++i)
		{
			if (s_functions[i] == NULL)
			{
				s_functions.erase(s_functions.begin() + i);
				--i;
			}
		}
	}

	if (registeredFunctions)
		*registeredFunctions = &s_functions;
}

void StartPlayback (ReaProject* proj, double position)
{
	double editCursor = GetCursorPositionEx(proj);

	PreventUIRefresh(1);
	SetEditCurPos2(proj, position, false, false);
	OnPlayButtonEx(proj);
	SetEditCurPos2(proj, editCursor, false, false);
	PreventUIRefresh(-1);
}

bool IsPlaying (ReaProject* proj)
{
	return (GetPlayStateEx(proj) & 1) == 1;
}

bool IsPaused (ReaProject* proj)
{
	return (GetPlayStateEx(proj) & 2) == 2;
}

bool IsRecording (ReaProject* proj)
{
	return (GetPlayStateEx(proj) & 4) == 4;
}

bool AreAllCoordsZero (RECT& r)
{
	return (r.bottom == 0 && r.left == 0 && r.right == 0 && r.top == 0);
}

PCM_source* DuplicateSource (PCM_source* source)
{
	if (source && !strcmp(source->GetType(), "MIDI")) // check "MIDI" only (trim pref doesn't apply to pooled MIDI)
	{
		const ConfigVar<int> trimMidiOnSplit("trimmidionsplit");
		if (trimMidiOnSplit && GetBit(*trimMidiOnSplit, 1))
		{
			ConfigVarOverride<int> temp(trimMidiOnSplit, ClearBit(*trimMidiOnSplit, 1));
			PCM_source* newSource = source->Duplicate();
			return newSource;
		}
	}
	return source ? source->Duplicate() : NULL;
}

/******************************************************************************
* Tracks                                                                      *
******************************************************************************/
int GetEffectiveCompactLevel (MediaTrack* track)
{
	int compact = 0;
	MediaTrack* parent = track;
	while ((parent = (MediaTrack*)GetSetMediaTrackInfo(parent, "P_PARTRACK", NULL)))
	{
		int tmp = (int)GetMediaTrackInfo_Value(parent, "I_FOLDERCOMPACT");
		if (tmp > compact)
		{
			compact = tmp;
			if (compact == 2)
				break;
		}
	}
	return compact;
}

int GetTrackFreezeCount (MediaTrack* track)
{
	int freezeCount = 0;

	if (track)
	{
		char* trackState = GetSetObjectState(track, "");
		char* token = strtok(trackState, "\n");

		WDL_FastString newState;
		LineParser lp(false);

		int blockCount = 0;
		while (token != NULL)
		{
			lp.parse(token);

			if (blockCount == 1 && !strcmp(lp.gettoken_str(0), "<FREEZE"))
				++freezeCount;

			if      (lp.gettoken_str(0)[0] == '<') ++blockCount;
			else if (lp.gettoken_str(0)[0] == '>') --blockCount;
			token = strtok(NULL, "\n");
		}
		FreeHeapPtr(trackState);
	}

	return freezeCount;
}

bool TcpVis (MediaTrack* track)
{
	if (track)
	{
		if ((GetMasterTrack(NULL) == track))
		{
			const int master = ConfigVar<int>("showmaintrack").value_or(0);
			if (master == 0 || (int)GetMediaTrackInfo_Value(track, "I_WNDH") == 0)
				return false;
			else
				return true;
		}
		else
		{
			if (!GetMediaTrackInfo_Value(track, "B_SHOWINTCP") || !GetMediaTrackInfo_Value(track, "I_WNDH"))
				return false;
			else
				return true;
		}
	}
	else
		return false;
}

/******************************************************************************
* Items and takes                                                             *
******************************************************************************/
vector<MediaItem*> GetSelItems (MediaTrack* track)
{
	vector<MediaItem*> items;
	int count = CountTrackMediaItems(track);
	for (int i = 0; i < count ; ++i)
		if (*(bool*)GetSetMediaItemInfo(GetTrackMediaItem(track, i), "B_UISEL", NULL))
			items.push_back(GetTrackMediaItem(track, i));
	return items;
}

double ProjectTimeToItemTime (MediaItem* item, double projTime)
{
	if (item)
		return projTime - GetMediaItemInfo_Value(item, "D_POSITION");
	else
		return projTime;
}

double ItemTimeToProjectTime (MediaItem* item, double itemTime)
{
	if (item)
		return GetMediaItemInfo_Value(item, "D_POSITION") + itemTime;
	else
		return itemTime;
}

int GetTakeId (MediaItem_Take* take, MediaItem* item /*= NULL*/)
{
	item = (item) ? item : GetMediaItemTake_Item(take);

	for (int i = 0; i < CountTakes(item); ++i)
	{
		if (GetTake(item, i) == take)
			return i;
	}
	return -1;
}

int GetLoopCount (MediaItem_Take* take, double position, int* loopIterationForPosition)
{
	int    loopCount   = 0;
	int    currentLoop = -1;

	MediaItem* item = GetMediaItemTake_Item(take);
	if (item && take)
	{
		double itemStart = GetMediaItemInfo_Value(item, "D_POSITION");
		double itemEnd   = GetMediaItemInfo_Value(GetMediaItemTake_Item(take), "D_LENGTH") + itemStart;

		if (GetMediaItemInfo_Value(item, "B_LOOPSRC"))
		{
			if (IsMidi(take, NULL))
			{
				double itemEndPPQ    = MIDI_GetPPQPosFromProjTime(take, itemEnd);
				double sourceLenPPQ  = GetMidiSourceLengthPPQ(take, true);
				double itemLenPPQ = itemEndPPQ; // gotcha: the same cause PPQ starts counting from 0 from item start, making it mover obvious this way

				loopCount = static_cast<int>(itemLenPPQ / sourceLenPPQ);

				// fmod works great here because ppq are always rounded to whole numbers
				if(fmod(itemLenPPQ, sourceLenPPQ) == 0)
					loopCount--;

				if (loopIterationForPosition && CheckBounds(position, itemStart, itemEnd))
				{
					double takeOffset   = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");
					double itemStartEffPPQ = MIDI_GetPPQPosFromProjTime(take, itemStart - takeOffset);
					double positionPPQ     = MIDI_GetPPQPosFromProjTime(take, position);
					double itemEndEffPPQ = positionPPQ - itemStartEffPPQ;

					currentLoop = (int)(itemEndEffPPQ / sourceLenPPQ);
					if      (currentLoop > loopCount) currentLoop = loopCount;
					else if (currentLoop < 0)         currentLoop = 0;
				}
			}
			else
			{
				double takePlayrate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
				double takeOffset   = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");

				double itemStartEff = itemStart - takeOffset / takePlayrate;
				double itemLen   = (itemEnd - itemStartEff);
				double sourceLen = GetMediaItemTake_Source(take)->GetLength() / takePlayrate;

				loopCount = (int)(itemLen/sourceLen);
				if (loopCount > 0 && CheckBounds(itemStartEff + sourceLen*(loopCount), itemEnd - 0.00000000000001, itemEnd))
					--loopCount;

				if (loopIterationForPosition && CheckBounds(position, itemStart, itemEnd))
				{
					currentLoop = (int)((position - itemStartEff) / sourceLen);
					if      (currentLoop > loopCount) currentLoop = loopCount;
					else if (currentLoop < 0)         currentLoop = 0;
				}
			}
		}
		else
		{
			if (CheckBounds(position, itemStart, itemEnd))
			{
				currentLoop = 0;
			}
		}
	}
	WritePtr(loopIterationForPosition, currentLoop);
	return loopCount;
}

int GetEffectiveTakeId (MediaItem_Take* take, MediaItem* item, int id, int* effectiveTakeCount)
{
	MediaItem*      validItem = (item) ? (item) : (GetMediaItemTake_Item(take));
	MediaItem_Take* validTake = (take) ? (take) : (GetTake(validItem, id));

	int count = CountTakes(validItem);
	int effectiveCount = 0;
	int effectiveId = -1;

	// Empty takes not displayed
	const int emptyLanes = ConfigVar<int>("takelanes").value_or(0);
	if (GetBit(emptyLanes, 2))
	{
		for (int i = 0; i < count; ++i)
		{
			if (MediaItem_Take* currentTake = GetTake(validItem, i))
			{
				if (currentTake == validTake)
					effectiveId = effectiveCount;
				++effectiveCount;
			}
		}
	}
	// Empty takes displayed
	else
	{
		if (take)
		{
			for (int i = 0; i < count; ++i)
			{
				if (GetTake(validItem, i) == validTake)
				{
					effectiveId = i;
					break;
				}
			}
		}
		else
		{
			effectiveId = id;
		}

		effectiveCount = count;
	}

	WritePtr(effectiveTakeCount, effectiveCount);
	return effectiveId;
}

SourceType GetSourceType (PCM_source* source)
{
	if (!source)
		return SourceType::Unknown;

	const char* type = source->GetType();

	if (!strcmp(type, "SECTION"))
		return GetSourceType(source->GetSource());
	if (!strcmp(type, "MIDIPOOL") || !strcmp(type, "MIDI"))
		return SourceType::MIDI;
	else if (!strcmp(type, "VIDEO"))
		return SourceType::Video;
	else if (!strcmp(type, "CLICK"))
		return SourceType::Click;
	else if (!strcmp(type, "LTC"))
		return SourceType::Timecode;
	else if (!strcmp(type, "RPP_PROJECT"))
		return SourceType::Project;
	else if (!strcmp(type, "VIDEOEFFECT"))
		return SourceType::VideoEffect;
	else if (!strcmp(type, ""))
		return SourceType::Unknown;
	else
		return SourceType::Audio;
}

SourceType GetSourceType (MediaItem_Take* take)
{
	if (!take)
		return SourceType::Unknown;

	return GetSourceType(GetMediaItemTake_Source(take));
}

int GetEffectiveTimebase (MediaItem* item)
{
	int timeBase = 0;
	if (item)
	{
		timeBase = (int)(char)GetMediaItemInfo_Value(item , "C_BEATATTACHMODE");
		if (timeBase != 0 && timeBase != 1 && timeBase != 2)
		{
			timeBase = (int)(char)GetMediaTrackInfo_Value(GetMediaItemTrack(item), "C_BEATATTACHMODE");
			if (timeBase != 0 && timeBase != 1 && timeBase != 2)
				timeBase = ConfigVar<int>("itemtimelock").value_or(0);
		}
	}
	return timeBase;
}

int GetTakeFXCount (MediaItem_Take* take)
{
	int count = 0;

	MediaItem* item = GetMediaItemTake_Item(take);
	int takeId = GetTakeId(take, item);
	if (takeId >= 0)
	{
		SNM_TakeParserPatcher p(item, CountTakes(item));
		WDL_FastString takeChunk;
		if (p.GetTakeChunk(takeId, &takeChunk))
		{
			SNM_ChunkParserPatcher ptk(&takeChunk, false);
			count = ptk.Parse(SNM_COUNT_KEYWORD, 1, "TAKEFX", "WAK");
		}
	}
	return count;
}

bool GetMidiTakeTempoInfo (MediaItem_Take* take, bool* ignoreProjTempo, double* bpm, int* num, int* den)
{
	bool   _ignoreTempo = false;
	double _bpm         = 0;
	int    _num         = 0;
	int    _den         = 0;

	bool succes = false;
	if (take && IsMidi(take, NULL))
	{
		MediaItem* item = GetMediaItemTake_Item(take);
		int takeId = GetTakeId(take, item);
		if (takeId >= 0)
		{
			SNM_TakeParserPatcher p(item, CountTakes(item));
			WDL_FastString takeChunk;
			int tkPos, tklen;
			if (p.GetTakeChunk(takeId, &takeChunk, &tkPos, &tklen))
			{
				SNM_ChunkParserPatcher ptk(&takeChunk, false);
				WDL_FastString tempoLine;
				if (ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "IGNTEMPO", 0, -1, &tempoLine))
				{
					LineParser lp(false);
					lp.parse(tempoLine.Get());

					_ignoreTempo = !!lp.gettoken_int(1);
					_bpm         = lp.gettoken_float(2);
					_num         = lp.gettoken_int(3);
					_den         = lp.gettoken_int(4);
					succes = true;
				}
			}
		}
	}

	WritePtr(ignoreProjTempo, _ignoreTempo);
	WritePtr(bpm,             _bpm);
	WritePtr(num,             _num);
	WritePtr(den,             _den);
	return succes;
}

bool SetIgnoreTempo (MediaItem* item, bool ignoreTempo, double bpm, int num, int den, bool skipItemsWithSameIgnoreState)
{
	bool midiFound = false;
	for (int i = 0; i < CountTakes(item); ++i)
	{
		if (IsMidi(GetMediaItemTake(item, i)))
		{
			midiFound = true;
			break;
		}
	}
	if (!midiFound)
		return false;

	double timeBase = GetMediaItemInfo_Value(item, "C_BEATATTACHMODE");
	SetMediaItemInfo_Value(item, "C_BEATATTACHMODE", 0);

	WDL_FastString newState;
	char* chunk = GetSetObjectState(item, "");
	bool stateChanged = false;

	char* token = strtok(chunk, "\n");
	while (token != NULL)
	{
		if (!strncmp(token, "IGNTEMPO ", sizeof("IGNTEMPO ") - 1))
		{
			LineParser lp(false);
			lp.parse(token);

			if (!skipItemsWithSameIgnoreState || !!lp.gettoken_int(1) != ignoreTempo)
			{
				for (int i = 0; i < lp.getnumtokens(); ++i)
				{
					if      (i == 0) newState.AppendFormatted(256, "%s",     lp.gettoken_str(i));
					else if (i == 1) newState.AppendFormatted(256, "%d",     ignoreTempo ? 1 : 0);
					else if (i == 2) newState.AppendFormatted(256, "%.14lf", ignoreTempo ? bpm : lp.gettoken_float(i));
					else if (i == 3) newState.AppendFormatted(256, "%d",     ignoreTempo ? num : lp.gettoken_int(i));
					else if (i == 4) newState.AppendFormatted(256, "%d",     ignoreTempo ? den : lp.gettoken_int(i));
					else             newState.AppendFormatted(256, "%s",     lp.gettoken_str(i));
					newState.Append(" ");
				}
				newState.Append("\n");
				stateChanged = true;
			}
			else
			{
				AppendLine(newState, token);
			}
		}
		else
		{
			AppendLine(newState, token);
		}
		token = strtok(NULL, "\n");
	}

	if (stateChanged)
		GetSetObjectState(item, newState.Get());
	FreeHeapPtr(chunk);

	SetMediaItemInfo_Value(item, "C_BEATATTACHMODE", timeBase);
	return stateChanged;
}

bool DoesItemHaveMidiEvents (MediaItem* item)
{
	for (int i = 0; i < CountTakes(item); ++i)
	{
		if (MIDI_CountEvts(GetTake(item, i), NULL, NULL, NULL))
			return true;
	}
	return false;
}

bool TrimItem (MediaItem* item, double start, double end, bool adjustTakesEnvelopes, bool force /*=false*/)
{
	if (!item)
		return false;

	if (start > end)
		swap(start, end);
	if (start < 0)
		start = 0;

	double newLen  = end - start;
	if (newLen <= 0)
		return false;

	double itemPos = GetMediaItemInfo_Value(item, "D_POSITION");
	double itemLen = GetMediaItemInfo_Value(item, "D_LENGTH");

	bool itemLooped = !!GetMediaItemInfo_Value(item, "B_LOOPSRC");
	MediaItem_Take* activeTake = GetActiveTake(item);
	if (force || start != itemPos || newLen != itemLen)
	{
		double startDif = start - itemPos;
		SetMediaItemInfo_Value(item, "D_LENGTH", newLen);
		SetMediaItemInfo_Value(item, "D_POSITION", start);

		for (int i = 0; i < CountTakes(item); ++i)
		{
			MediaItem_Take* take = GetTake(item, i);
			double playrate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
			double offset   = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");
			SetMediaItemTakeInfo_Value(take, "D_STARTOFFS", offset + playrate*startDif);

			if (IsMidi(take))
			{
				SetActiveTake(take);
				if (!itemLooped)
					MIDI_SetItemExtents(item, TimeMap_timeToQN(start), TimeMap_timeToQN(end)); // this will update source length (but in case of looped midi item we don't want that (it also disabled looping for item)
			}

			if (adjustTakesEnvelopes) {
				DoAdjustTakesEnvelopes(take, startDif); 
			}
			
		}

		SetActiveTake(activeTake);
		return true;
	}
	return false;
}

bool TrimItem_UseNativeTrimActions(MediaItem* item, double start, double end, bool force /*=false*/)
{
	if (!item)
		return false;

	// Fetch the current start and length for the item
	double itemPos = GetMediaItemInfo_Value(item, "D_POSITION");
	double itemLen = GetMediaItemInfo_Value(item, "D_LENGTH");

	// If start is negative then keep the current start position
	if (start < 0)
		start = itemPos;

	// If end is negative then keep the current end position
	if (end < 0)
		end = itemPos + itemLen;

	if (start > end)
		swap(start, end);

	double newLen = end - start;
	if (newLen <= 0)
		return false;


	if (force || start != itemPos || newLen != itemLen) {
		// NF: ugly code ahead for fixing
		// https://github.com/reaper-oss/sws/issues/950#issuecomment-384251678
		// but it avoids having to deal with stretch markers (see other comments in #950)

		// save item selection
		WDL_TypedBuf<MediaItem*> selItems;
		SWS_GetSelectedMediaItems(&selItems);
		
		PreventUIRefresh(1);

		// disable item grouping
		// https://github.com/reaper-oss/sws/pull/979#issuecomment-457760578
		ConfigVarOverride<int> projgroupover("projgroupover", 1);

		// unselect all items
		for (int i = 0; i < selItems.GetSize(); i++) {
			SetMediaItemInfo_Value(selItems.Get()[i], "B_UISEL", 0);
		}

		SetMediaItemInfo_Value(item, "B_UISEL", 1); // select item currently being worked on

		double origCurPos = GetCursorPosition();
		SetEditCurPos(start, false, false);
		Main_OnCommand(41305, 0); // Trim left edge of item to edit cursor
		SetEditCurPos(end, false, false);
		Main_OnCommand(41311, 0); // Trim right edge of item to edit cursor 
		SetEditCurPos(origCurPos, false, false);

		SetMediaItemInfo_Value(item, "B_UISEL", 0); // unselect previously manually selected item

		projgroupover.rollback(); // reenable item grouping (if necessary)

		// restore original item selection
		for (int i = 0; i < selItems.GetSize(); i++) {
			SetMediaItemInfo_Value(selItems.Get()[i], "B_UISEL", 1);
		}

		PreventUIRefresh(-1);
		return true;
	}
	return false;
}

void DoAdjustTakesEnvelopes(MediaItem_Take* take, double offset)
{
	int envCount = CountTakeEnvelopes(take);
	double takeRate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");

	for (int i = 0; i < envCount; i++) {
		TrackEnvelope* env = GetTakeEnvelope(take, i);
		int envPointsCount = CountEnvelopePoints(env);

		double time; double value; int shape; double tension; bool selected;

		for (int j = 0; j < envPointsCount; j++) {
			bool isEnvPoint = GetEnvelopePoint(env, j, &time, &value, &shape, &tension, &selected);

			if (isEnvPoint) {
				double newTime = time - (offset * takeRate);
				SetEnvelopePoint(env, j, &newTime, &value, &shape, &tension, &selected, &g_bTrue);
			}
		}
		Envelope_SortPoints(env);
	}
}


bool GetMediaSourceProperties (MediaItem_Take* take, bool* section, double* start, double* length, double* fade, bool* reverse)
{
	bool sucess         = false;
	bool returnSection  = false;
	bool returnReverse  = false;
	double returnStart  = 0;
	double returnLength = 0;
	double returnFade   = 0;

	PCM_source* source = GetMediaItemTake_Source(take);
	if (source && !IsMidi(take))
	{
		bool foundSectionInChunk = false;
		int mode = 0;

		WDL_HeapBuf hb;
		ProjectStateContext* ctx = ProjectCreateMemCtx(&hb);
		source->SaveState(ctx);
		LineParser lp(false);
		while (ProjectContext_GetNextLine(ctx, &lp))
		{
			if (!strcmp(lp.gettoken_str(0), "LENGTH"))
			{
				foundSectionInChunk = true;
				returnSection = true;
				returnLength = lp.gettoken_float(1);
			}
			else if (!strcmp(lp.gettoken_str(0), "STARTPOS"))
				returnStart = lp.gettoken_float(1);
			else if (!strcmp(lp.gettoken_str(0), "OVERLAP"))
				returnFade = lp.gettoken_float(1);
			else if (!strcmp(lp.gettoken_str(0), "MODE"))
				mode = lp.gettoken_int(1);
			else if (lp.gettoken_str(0)[0] == '<')
				ProjectContext_EatCurrentBlock(ctx);
		}
		delete ctx;

		// Mode token doesn't have to be after other section info, so check it only after parsing everything
		if (mode == 2)
			returnReverse = true;
		else if (mode == 3)
		{
			returnReverse = true;
			returnSection = false;
		}

		// Even if section is turned off, it does display section info so get it
		if (!foundSectionInChunk)
		{
			returnLength = GetMediaItemInfo_Value(GetMediaItemTake_Item(take), "D_LENGTH");
			returnStart = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");
			returnFade = 0;
		}
		sucess = true;
	}

	WritePtr(section, returnSection);
	WritePtr(start, returnStart);
	WritePtr(length, returnLength);
	WritePtr(fade, returnFade);
	WritePtr(reverse, returnReverse);
	return sucess;
}

bool SetMediaSourceProperties (MediaItem_Take* take, bool section, double start, double length, double fade, bool reverse)
{
	PCM_source* source = GetMediaItemTake_Source(take);
	if (take && source && !IsMidi(take))
	{
		// If existing source has properties already set, it constitutes a special source named "section" that holds
		// another source within which the "real" media resides
		PCM_source* mediaSource = (!strcmp(source->GetType(), "SECTION")) ? (source->GetSource()) : (source);

		PCM_source* newSource = NULL;
		if (!section && !reverse)
		{
			newSource = mediaSource->Duplicate();
		}
		else if ((newSource = PCM_Source_CreateFromType("SECTION")))
		{
			newSource->SetSource(DuplicateSource(mediaSource));
			WDL_FastString sourceStr;

			// Get default section values if needed
			bool getSectionDefaults = !section;
			if (!strcmp(source->GetType(), "SECTION"))
			{
				WDL_HeapBuf hb;
				if (ProjectStateContext* ctx = ProjectCreateMemCtx(&hb))
				{
					source->SaveState(ctx);
					char line[PROJ_CONTEXT_LINE];
					LineParser lp(false);
					while(!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
					{
						if (!strcmp(lp.gettoken_str(0), "LENGTH"))
						{
							if (getSectionDefaults)
								length = lp.gettoken_float(1);
						}
						else if (!strcmp(lp.gettoken_str(0), "STARTPOS"))
						{
							if (getSectionDefaults)
								start = lp.gettoken_float(1);
						}
						else if (!strcmp(lp.gettoken_str(0), "OVERLAP"))
						{
							if (getSectionDefaults)
								fade = lp.gettoken_float(1);
						}
						else if (!strcmp(lp.gettoken_str(0), "MODE"))
						{
							continue;
						}
						else if (!strcmp(lp.gettoken_str(0), "<SOURCE")) // already got it in mediaSource
						{
							ProjectContext_EatCurrentBlock(ctx);
						}
						else                                             // in case things get added in the future
						{
							AppendLine(sourceStr, line);
						}
					}
				}
			}
			else if (getSectionDefaults)
			{
				length = GetMediaItemInfo_Value(GetMediaItemTake_Item(take), "D_LENGTH");
				start = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");
				fade = 0.01;
			}

			sourceStr.AppendFormatted(256, "%s%lf\n", "LENGTH ", length);
			sourceStr.AppendFormatted(256, "%s%lf\n", "STARTPOS ", start);
			sourceStr.AppendFormatted(256, "%s%lf\n", "OVERLAP ", fade);
			if (section && reverse) sourceStr.AppendFormatted(256, "%s%d\n", "MODE ", 2);
			else if (reverse)       sourceStr.AppendFormatted(256, "%s%d\n", "MODE ", 3);
			AppendLine(sourceStr, GetSourceChunk(mediaSource).Get());

			WDL_HeapBuf hb;
			if (void* p = hb.Resize(sourceStr.GetLength(), false))
			{
				if (ProjectStateContext* ctx = ProjectCreateMemCtx(&hb))
				{
					memcpy(p, sourceStr.Get(), sourceStr.GetLength());
					WDL_String firstLine; firstLine.AppendFormatted(256, "%s", "<SOURCE SECTION");
					newSource->LoadState(firstLine.Get(), ctx);
					delete ctx;
				}
				else
					newSource = NULL;
			}
			else
				newSource = NULL;
		}

		if (newSource)
		{
			GetSetMediaItemTakeInfo(take,"P_SOURCE", newSource);
			delete source;
			return true;
		}
	}
	return false;
}

bool SetTakeSourceFromFile (MediaItem_Take* take, const char* filename, bool inProjectData, bool keepSourceProperties)
{
	if (take && file_exists(filename))
	{
		if (PCM_source* oldSource = (PCM_source*)GetSetMediaItemTakeInfo(take,"P_SOURCE",NULL))
		{
			// Getting and setting properties appears faster than source->SetFileName(filename), testing
			// ReaScript export on 10000 items results in 3x performance
			bool section, reverse;
			double start, len, fade;
			bool properties = false;
			if (keepSourceProperties)
				properties = GetMediaSourceProperties(take, &section, &start, &len, &fade, &reverse);

			PCM_source* newSource = PCM_Source_CreateFromFileEx(filename, !inProjectData);
			GetSetMediaItemTakeInfo(take,"P_SOURCE", newSource);
			delete oldSource;

			if (properties)
				SetMediaSourceProperties(take, section, start, len, fade, reverse);
			return true;
		}
	}
	return false;
}

GUID GetItemGuid (MediaItem* item)
{
	return (item) ? (*(GUID*)GetSetMediaItemInfo(item, "GUID", NULL)) : GUID_NULL;
}

MediaItem* GuidToItem (const GUID* guid, ReaProject* proj /*=NULL*/)
{
	if (guid)
	{
		const int itemCount = CountMediaItems(proj);
		for (int i = 0; i < itemCount; ++i)
		{
			MediaItem* item = GetMediaItem(proj, i);
			if (GuidsEqual((GUID*)GetSetMediaItemInfo(item, "GUID", NULL), guid))
				return item;
		}
	}
	return NULL;
}

WDL_FastString GetSourceChunk (PCM_source* source)
{
	WDL_FastString sourceStr;
	if (source)
	{
		WDL_HeapBuf hb;
		if (ProjectStateContext* ctx = ProjectCreateMemCtx(&hb))
		{
			source->SaveState(ctx);

			sourceStr.AppendFormatted(PROJ_CONTEXT_LINE, "%s%s\n", "<SOURCE ", source->GetType());
			char line[PROJ_CONTEXT_LINE];
			while (!ctx->GetLine(line, sizeof(line)))
				AppendLine(sourceStr, line);
			sourceStr.Append(">");

			delete ctx;
		}
	}
	return sourceStr;
}

/******************************************************************************
* Fades (not 100% accurate - http://askjf.com/index.php?q=2976s)              *
******************************************************************************/
static void AssignDir(double* x, double* y, const double* b)
{
	/* Code bits courtesy of Cockos, http://askjf.com/index.php?q=2967s */

	x[1]=b[0];
	y[1]=b[1];
	x[2]=b[2];
	y[2]=b[3];
}

static void ApplyDir(double* x, double* y, double w0, double w1, const double* b0, const double* b1)
{
	/* Code bits courtesy of Cockos, http://askjf.com/index.php?q=2967s */

	x[1]=w0*b0[0]+w1*b1[0];
	y[1]=w0*b0[1]+w1*b1[1];
	x[2]=w0*b0[2]+w1*b1[2];
	y[2]=w0*b0[3]+w1*b1[3];
}

static bool GetMediaItemFadeBezParms(int shape, double dir, bool isfadeout, double* x, double* y)
{
	/* Code bits courtesy of Cockos, http://askjf.com/index.php?q=2967s *
	*                                                                   *
	*  x[4], y[4] .. returns true if the fade is linear                 */

	static const double b0[4] = { 0.5, 0.5, 0.5, 0.5 };
	static const double b1[4] = { 0.25, 0.5, 0.625, 1.0 };
	static const double b2[4] = { 0.375, 0.0, 0.75, 0.5 };
	static const double b3[4] = { 0.25, 1.0, 0.5, 1.0 };
	static const double b4[4] = { 0.5, 0.0, 0.75, 0.0 };
	static const double b5[4] = { 0.375, 0.0, 0.625, 1.0 };
	static const double b6[4] = { 0.875, 0.0, 0.125, 1.0 };
	static const double b7[4] = { 0.25, 0.375, 0.625, 1.0 };

	static const double b4i[4] = { 0.0, 1.0, 0.125, 1.0 };
	static const double b50[4] = { 0.25, 0.25, 0.25, 1.0 };
	static const double b51[4] = { 0.75, 0.0, 0.75, 0.75 };
	static const double b60[4] = { 0.375, 0.25, 0.0, 1.0 };
	static const double b61[4] = { 1.0, 0.0, 0.625, 0.75 };

	if (!isfadeout)
	{
		x[0]=y[0]=0.0;
		x[3]=y[3]=1.0;
	}
	else
	{
		x[0]=y[3]=0.0;
		x[3]=y[0]=1.0;
	}

	if (shape < 0 || shape >= 8)
	{
		shape=0;
		dir=0.0;
	}

	if (isfadeout)
		dir=-dir;

	bool linear=false;

	if (dir < 0.0)
	{
		double w0=-dir;
		double w1=1.0+dir;
		if      (shape == 1) ApplyDir(x, y, w0, w1, b4i, b1);
		else if (shape == 2) ApplyDir(x, y, w0, w1, b1,  b0);
		else if (shape == 5) ApplyDir(x, y, w0, w1, b50, b5);
		else if (shape == 6) ApplyDir(x, y, w0, w1, b60, b6);
		else if (shape == 7) ApplyDir(x, y, w0, w1, b4i, b7);
		else                 ApplyDir(x, y, w0, w1, b3,  b0);
	}
	else if (dir > 0.0)
	{
		double w0=1.0-dir;
		double w1=dir;
		if      (shape == 1) ApplyDir(x, y, w0, w1, b1, b4);
		else if (shape == 2) ApplyDir(x, y, w0, w1, b0, b2);
		else if (shape == 5) ApplyDir(x, y, w0, w1, b5, b51);
		else if (shape == 6) ApplyDir(x, y, w0, w1, b6, b61);
		else if (shape == 7) ApplyDir(x, y, w0, w1, b7, b4);
		else                 ApplyDir(x, y, w0, w1, b0, b4);
	}
	else
	{
		if      (shape == 1) AssignDir(x, y, b1);
		else if (shape == 5) AssignDir(x, y, b5);
		else if (shape == 6) AssignDir(x, y, b6);
		else if (shape == 7) AssignDir(x, y, b7);
		else
		{
			AssignDir(x, y, b0);
			linear=true;
		}
	}

	if (isfadeout && !linear)
	{
		double ox1=x[1];
		double ox2=x[2];
		x[1]=1.0-ox2;
		x[2]=1.0-ox1;
		swap(y[1], y[2]);
	}

	return linear;
}

double GetNormalizedFadeValue (double position, double fadeStart, double fadeEnd, int fadeShape, double fadeCurve, bool isFadeOut)
{
	if (position < fadeStart)
		return (isFadeOut) ? 1 : 0;
	else if (position > fadeEnd)
		return (isFadeOut) ? 0 : 1;
	else
	{
		double x[4], y[4];
		GetMediaItemFadeBezParms(fadeShape, fadeCurve, isFadeOut, x, y);

		double t = (position - fadeStart) / (fadeEnd - fadeStart);
		return LICE_CBezier_GetY(x[0], x[1], x[2], x[3], y[0], y[1], y[2], y[3], t);
	}
}

double GetEffectiveFadeLength (MediaItem* item, bool isFadeOut, int* fadeShape /*= NULL*/, double* fadeCurve /*= NULL*/)
{
	if (item)
	{
		WritePtr(fadeShape, ((isFadeOut) ? (int)GetMediaItemInfo_Value(item, "C_FADEOUTSHAPE") : (int)GetMediaItemInfo_Value(item, "C_FADEINSHAPE")));
		WritePtr(fadeCurve, ((isFadeOut) ?      GetMediaItemInfo_Value(item, "D_FADEOUTDIR")   :      GetMediaItemInfo_Value(item, "D_FADEINDIR")));

		double fadeLength = GetMediaItemInfo_Value(item, ((isFadeOut) ? "D_FADEOUTLEN_AUTO" : "D_FADEINLEN_AUTO"));
		return (fadeLength > 0) ? fadeLength : GetMediaItemInfo_Value(item, ((isFadeOut) ? "D_FADEOUTLEN" : "D_FADEINLEN"));
	}
	else
	{
		WritePtr(fadeShape, 0);
		WritePtr(fadeCurve, 0.0);
		return 0;
	}
}

/******************************************************************************
* Stretch markers                                                             *
******************************************************************************/
int FindPreviousStretchMarker (MediaItem_Take* take, double position)
{
	int first = 0;
	int last = GetTakeNumStretchMarkers(take);

	while (first != last)
	{
		int mid = (first + last) / 2;
		double currentPos; GetTakeStretchMarker(take, mid, &currentPos, NULL);

		if (currentPos < position) first = mid + 1;
		else                       last  = mid;

	}
	return first - 1;
}

int FindNextStretchMarker (MediaItem_Take* take, double position)
{
	int first = 0;
	int last = GetTakeNumStretchMarkers(take);
	int count = last;

	while (first != last)
	{
		int mid = (first + last) /2;
		double currentPos; GetTakeStretchMarker(take, mid, &currentPos, NULL);

		if (position >= currentPos) first = mid + 1;
		else                        last  = mid;
	}
	return (first < count) ? first: -1;
}

int FindClosestStretchMarker (MediaItem_Take* take, double position)
{
	int prevId = FindPreviousStretchMarker(take, position);
	int nextId = prevId + 1;

	int count = GetTakeNumStretchMarkers(take);
	if (prevId == -1)
	{
		return (nextId < count) ? nextId : -1;
	}
	else
	{
		if (nextId < count)
		{
			double prevPos, nextPos;
			GetTakeStretchMarker(take, prevId, &prevPos, NULL);
			GetTakeStretchMarker(take, nextId, &nextPos, NULL);
			return (GetClosestVal(position, nextPos, prevPos) == nextPos) ? nextId : prevId;
		}
		else
		{
			return prevId;
		}
	}
}

int FindStretchMarker (MediaItem_Take* take, double position, double surroundingRange /*= 0*/)
{
	int count = GetTakeNumStretchMarkers(take);
	if (count == 0)
		return -1;

	int prevId = FindPreviousStretchMarker(take, position);
	int nextId = prevId + 1;

	double prevPos, nextPos;
	GetTakeStretchMarker(take, prevId, &prevPos, NULL);
	GetTakeStretchMarker(take, nextId, &nextPos, NULL);

	double distanceFromPrev = (CheckBounds(prevId, 0, count-1)) ? (position - prevPos) : (abs(surroundingRange) + 1);
	double distanceFromNext = (CheckBounds(nextId, 0, count-1)) ? (nextPos - position) : (abs(surroundingRange) + 1);

	int id = -1;
	if (distanceFromPrev <= distanceFromNext)
	{
		if (distanceFromPrev <= surroundingRange)
			id = prevId;
	}
	else
	{
		if (distanceFromNext <= surroundingRange)
			id = nextId;
	}
	return (id < count) ? id : -1;
}

bool InsertStretchMarkersInAllItems (const vector<double>& stretchMarkers, bool doBeatsTimebaseOnly /*=false*/, double hardCheckPositionsDelta /*=-1*/, bool obeySwsOptions /*=true*/)
{
	bool updated = true;

	if (!IsLocked(ITEM_FULL) && !IsLocked(STRETCH_MARKERS) && (!obeySwsOptions || (obeySwsOptions && IsSetAutoStretchMarkersOn(NULL))))
	{
		const int itemCount = CountMediaItems(NULL);
		for (int i = 0; i < itemCount; ++i)
		{
			MediaItem* item = GetMediaItem(NULL, i);
			int itemEffectiveTimebase = (doBeatsTimebaseOnly) ? GetEffectiveTimebase(item) : 1;
			if (itemEffectiveTimebase > 0 && !IsItemLocked(item))
			{
				vector<pair<MediaItem_Take*,double> > takes;
				int allTakesCount = CountTakes(item);
				for (int j = 0; j < allTakesCount; ++j)
				{
					MediaItem_Take* take = GetMediaItemTake(item, j);
					if (take && !IsMidi(take))
						takes.push_back(make_pair(take, GetMediaItemTakeInfo_Value(take, "D_PLAYRATE")));
				}

				size_t takeCount = takes.size();
				if (takeCount > 0)
				{
					double itemStart = GetMediaItemInfo_Value(item, "D_POSITION");
					double itemEnd = itemStart + GetMediaItemInfo_Value(item, "D_LENGTH");
					size_t markersCount = stretchMarkers.size();
					for (size_t j = 0; j < markersCount; ++j)
					{
						double position = stretchMarkers[j];
						if (CheckBounds(position, itemStart, itemEnd))
						{
							for (size_t h = 0; h < takeCount; ++h)
							{
								if (hardCheckPositionsDelta < 0 || FindStretchMarker(takes[h].first, (position - itemStart) * takes[h].second , hardCheckPositionsDelta) < 0)
								{
									if (SetTakeStretchMarker(takes[h].first, -1, (position - itemStart) * takes[h].second, NULL) > 0)
										updated = true;
								}
							}
						}
					}
				}
			}
		}
	}
	return updated;
}

bool InsertStretchMarkerInAllItems (double position, bool doBeatsTimebaseOnly /*=false*/, double hardCheckPositionDelta /*=-1*/, bool obeySwsOptions /*=true*/)
{
	vector<double> stretchMarker;
	stretchMarker.push_back(position);
	return InsertStretchMarkersInAllItems(stretchMarker, doBeatsTimebaseOnly, hardCheckPositionDelta, obeySwsOptions);
}

/******************************************************************************
* Grid                                                                        *
******************************************************************************/
double GetGridDivSafe ()
{
	ConfigVar<double> gridDiv("projgriddiv");
	if (!gridDiv || *gridDiv < MAX_GRID_DIV)
	{
		gridDiv.try_set(MAX_GRID_DIV);
		return MAX_GRID_DIV;
	}
	else
		return *gridDiv;
}

double GetNextGridDiv (double position)
{
	/* This got a tiny bit complicated, but we're trying to replicate        *
	*  REAPER behavior as closely as possible...I guess the ultimate test    *
	*  would be inserting a bunch of really strange time signatures coupled  *
	*  with possibly even stranger grid divisions, things any sane person    *
	*  would never use - like 11/7 with grid division of 2/13...haha. As it  *
	*  stands now, at REAPER v4.7, this function handles it all              */

	if (position < 0)
		return 0;

	double nextGridPosition = position;

	const int projgridframe = ConfigVar<int>("projgridframe").value_or(0);
	if (projgridframe&1) // frames grid line spacing
	{
		int hours, minutes, seconds, frames;
		GetTimeInfoFromPosition(position, &hours, &minutes, &seconds, &frames);
		++frames;

		nextGridPosition = GetPositionFromTimeInfo(hours, minutes, seconds, frames);
	}
	else
	{
		// Grid dividing starts again from the measure in which tempo marker is so find location of first measure grid (obvious if grid division spans more measures)
		int gridDivStartTempo = FindPreviousTempoMarker(position);
		double gridDivStart;
		if (gridDivStartTempo >= 0)
		{
			if (GetTempoTimeSigMarker(NULL, gridDivStartTempo + 1, &gridDivStart, NULL, NULL, NULL, NULL, NULL, NULL))
			{
				if (gridDivStart > position)
					GetTempoTimeSigMarker(NULL, gridDivStartTempo, &gridDivStart, NULL, NULL, NULL, NULL, NULL, NULL);
				else
					gridDivStartTempo += 1;
			}
			else
				GetTempoTimeSigMarker(NULL, gridDivStartTempo, &gridDivStart, NULL, NULL, NULL, NULL, NULL, NULL);
		}
		else
		{
			gridDivStart = 0;
			gridDivStartTempo = 0; // if position is right at project start this would be -1 even if first tempo marker exists
		}

		// Get grid division translated into current time signature
		int gridDivStartMeasure, num, den;
		TimeMap2_timeToBeats(0, gridDivStart, &gridDivStartMeasure, &num, NULL, &den);
		double gridDiv;
		if (projgridframe&64) // measure grid line spacing
			gridDiv = num;
		else
		{
			gridDiv = GetGridDivSafe();
			gridDiv = (den*gridDiv) / 4;
		}
		
		// How much measures must pass for grid diving to start anew? (again, obvious when grid division spans more measures)
		int measureStep = (int)(gridDiv/num);
		if (measureStep == 0) measureStep = 1;

		// Find closest measure to our position, where grid diving starts again
		int positionMeasure;
		TimeMap2_timeToBeats(0, position, &positionMeasure, NULL, NULL, NULL);
		gridDivStartMeasure += (int)((positionMeasure - gridDivStartMeasure) / measureStep) * measureStep;
		gridDivStart = TimeMap2_beatsToTime(0, 0, &gridDivStartMeasure);

		// Finally find next grid position (different cases for measures and beats)
		if (gridDiv > num)
		{
			int gridDivEndMeasure = gridDivStartMeasure + measureStep;
			double gridDivEnd = TimeMap2_beatsToTime(0, 0, &gridDivEndMeasure);

			// Same as before, existing tempo markers can move end measure grid (where our next grid should be) so find if that's the case
			if (measureStep > 1)
			{
				double tempoPosition;
				while (GetTempoTimeSigMarker(NULL, ++gridDivStartTempo, &tempoPosition, NULL, NULL, NULL, NULL, NULL, NULL) && tempoPosition <= gridDivEnd)
				{
					int currentMeasure;
					TimeMap2_timeToBeats(0, tempoPosition, &currentMeasure, NULL, NULL, NULL);

					gridDivEndMeasure = currentMeasure + ((currentMeasure == positionMeasure) ? (measureStep) : (0));
					gridDivEnd = TimeMap2_beatsToTime(0, 0, &gridDivEndMeasure);
				}
			}

			nextGridPosition = TimeMap2_beatsToTime(0, 0, &gridDivEndMeasure);
		}
		else
		{
			double positionBeats = TimeMap2_timeToBeats(0, position, NULL, NULL, NULL, NULL) - TimeMap2_timeToBeats(0, gridDivStart, NULL, NULL, NULL, NULL);
			//double nextGridBeats = ((int)(positionBeats / gridDiv) + 1) * gridDiv;
			double nextGridBeats = (int)((positionBeats + gridDiv) / gridDiv) * gridDiv;
			while (abs(nextGridBeats - positionBeats) < 1E-6) nextGridBeats += gridDiv; // rounding errors, yuck...

			nextGridPosition = TimeMap2_beatsToTime(0, nextGridBeats, &gridDivStartMeasure);

			// Check it didn't pass over into next measure
			int gridDivEndMeasure = gridDivStartMeasure + measureStep;
			double gridDivEnd     = TimeMap2_beatsToTime(0, 0, &gridDivEndMeasure);
			if (nextGridPosition > gridDivEnd)
				nextGridPosition = gridDivEnd;
		}

		// Not so perfect fix for this issue: http://forum.cockos.com/project.php?issueid=5263
		if (nextGridPosition < position)
		{
			double gridDiv = GetGridDivSafe();

			double tempoPosition, beat; int measure; GetTempoTimeSigMarker(NULL, gridDivStartTempo, &tempoPosition, &measure, &beat, NULL, NULL, NULL, NULL);
			double offset =  gridDiv - fmod(TimeMap_timeToQN(TimeMap2_beatsToTime(0, 0, &measure)), gridDiv);

			double gridLn = TimeMap_timeToQN(tempoPosition);
			gridLn = TimeMap_QNToTime(gridLn - offset - fmod(gridLn, gridDiv));
			while (gridLn < position + (MIN_GRID_DIST/2))
				gridLn = TimeMap_QNToTime(TimeMap_timeToQN(gridLn) + gridDiv);

			nextGridPosition = gridLn;
		}
	}

	// calculate swing grid
	int swingmode; double division, swingamt;
	GetSetProjectGrid(nullptr, false, &division, &swingmode, &swingamt);
	if (swingmode && swingamt && division > 0.0)
	{
		/*
		// https://forum.cockos.com/showthread.php?p=2349247#post2349247
		// disable "Show grid line spacing" and disable "Grid snap settings follow grid visibility"
		const ConfigVar<int> projShowGrid("projshowgrid");
		ConfigVarOverride<int> tempProjShowGrid(projShowGrid, projShowGrid.value_or(0) & (~1) | 32768);

		// copy 'line spacing' value to 'grid snap spacing' value
		const ConfigVar<double> projGridDiv("projgriddiv");
		const ConfigVar<double> projGridDivSnap("projgriddivsnap");
		ConfigVarOverride<double> tempProjGridDivSnap(projGridDivSnap, *projGridDiv);

		nextGridPosition = SnapToGrid(nullptr, nextGridPosition);
		*/
	
		// alternative (shorter) version (also courtesy of Stevie):
		// enable "Grid snap settings follow grid visibility"
		const ConfigVar<int> projShowGrid("projshowgrid");
		ConfigVarOverride<int> tempProjShowGrid(projShowGrid, projShowGrid.value_or(0)& (~32768));
		
		// set "line spacing minimum" to 0
		// -> 'what you see is what you get'
		const ConfigVar<int> projGridMin("projgridmin");
		ConfigVarOverride<int> tempProjGridMin(projGridMin, 0);

		nextGridPosition = SnapToGrid(nullptr, nextGridPosition);	
	}

	return nextGridPosition;
}

double GetPrevGridDiv (double position)
{
	if (position <= 0)
		return 0;

	double prevGridDivPos = position;

	const int projgridframe = ConfigVar<int>("projgridframe").value_or(0);
	if (projgridframe&1) // frames grid line spacing
	{
		int hours, minutes, seconds, frames;
		GetTimeInfoFromPosition(position, &hours, &minutes, &seconds, &frames);

		GetHZoomLevel();
		double currentFramePos = GetPositionFromTimeInfo(hours, minutes, seconds, frames);
		if (IsEqual(currentFramePos, position, SNM_FUDGE_FACTOR))
		{
			--frames;
			prevGridDivPos = GetPositionFromTimeInfo(hours, minutes, seconds, frames);
		}
		else
		{
			prevGridDivPos = currentFramePos;
		}
	}
	else
	{
		// GetNextGridDiv is complicated enough, so let's not reinvent it here but reuse it (while less efficient than the real deal, it's really not that slower since GetNextGridDiv() is quite optimized)
		prevGridDivPos = TimeMap_QNToTime_abs(NULL, TimeMap_timeToQN_abs(NULL, position) - 1.5*GetGridDivSafe());
		while (true)
		{
			double tmp = GetNextGridDiv(prevGridDivPos);
			if (tmp >= position)
				break;
			else
				prevGridDivPos = tmp;
		}
	}
	return prevGridDivPos;
}

double GetClosestGridDiv (double position)
{
	double gridDiv = 0;
	if (position > 0)
	{
		double prevGridDiv = GetPrevGridDiv(position);
		double nextGridDiv = GetNextGridDiv(prevGridDiv);
		gridDiv = GetClosestVal(position, prevGridDiv, nextGridDiv);
	}
	return gridDiv;
}

double GetNextGridLine (double position)
{
	double nextGridLine = 0;

	if (position >= 0)
	{
		PreventUIRefresh(1);
		double editCursor = GetCursorPositionEx(NULL);

		// prevent edit cursor undo
		const ConfigVar<int> editCursorUndo("undomask");
		ConfigVarOverride<int> tmpUndoMask(editCursorUndo,
			ClearBit(editCursorUndo.value_or(0), 3));

		SetEditCurPos(position, false, false);
		Main_OnCommand(40647, 0); // View: Move cursor right to grid division
		nextGridLine = GetCursorPositionEx(NULL);

		SetEditCurPos(editCursor, false, false);
		PreventUIRefresh(-1);
	}

	return nextGridLine;
}

double GetPrevGridLine (double position)
{
	double prevGridLine = 0;
	if (position > 0)
	{
		PreventUIRefresh(1);
		double editCursor = GetCursorPositionEx(NULL);

		// prevent edit cursor undo
		const ConfigVar<int> editCursorUndo("undomask");
		ConfigVarOverride<int> tmpUndoMask(editCursorUndo, ClearBit(*editCursorUndo, 3));

		SetEditCurPos(position, false, false);
		Main_OnCommand(40646, 0); // View: Move cursor left to grid division
		prevGridLine = GetCursorPositionEx(NULL);

		SetEditCurPos(editCursor, false, false);
		PreventUIRefresh(-1);
	}
	return prevGridLine;
}

double GetClosestGridLine (double position)
{
	/* All other GridLine (but not GridDiv) functions   *
	*  are depending on this, but it appears SnapToGrid *
	*  is broken in certain non-real-world testing      *
	*  situations, so it should probably be rewritten   *
	*  using the stuff from GetNextGridDiv()            */

	// enable snap and snapping following grid visibility
	const ConfigVar<int> snap("projshowgrid");
	ConfigVarOverride<int> projshowgrid(snap, snap.value_or(0) & (~0x8100));

	return SnapToGrid(NULL, position);
}

double GetClosestMeasureGridLine (double position)
{
	double gridDiv = GetGridDivSafe();
	int num, den; TimeMap_GetTimeSigAtTime(0, position, &num, &den, NULL);

	double gridLine = 0;
	if ((den*gridDiv) / 4 < num)
	{
		// temporarily set grid div to full measure of current time signature
		ConfigVarOverride<double> tmpProjgriddiv("projgriddiv", 4*(double)num/(double)den);
		gridLine = GetClosestGridLine(position);
	}
	else
	{
		gridLine = GetClosestGridLine(position);
	}
	return gridLine;
}

double GetClosestLeftSideGridLine (double position)
{
	double grid = GetClosestGridLine(position);
	if (position >= grid)
		return grid;

	double gridDiv = GetGridDivSafe();
	const int minGridPx = ConfigVar<int>("projgridmin").value_or(0);
	double hZoom = GetHZoomLevel();

	int num, den; TimeMap_GetTimeSigAtTime(0, position, &num, &den, NULL);

	if ((den*gridDiv) / 4 < num)
	{
		double beats; int denom;
		TimeMap2_timeToBeats(NULL, grid, NULL, NULL, &beats, &denom);
		gridDiv *= (double)denom/4;

		double leftSideGrid  = TimeMap2_beatsToTime(NULL, beats - gridDiv, NULL);
		while ((grid - leftSideGrid) * hZoom < minGridPx)
		{
			gridDiv *= 2;
			leftSideGrid = TimeMap2_beatsToTime(NULL, beats - gridDiv, NULL);
		}
		return leftSideGrid;
	}
	else
	{
		gridDiv = (int)gridDiv/4;
		int measure;
		TimeMap2_timeToBeats(NULL, grid, &measure, NULL, NULL, NULL);

		measure -= (int)gridDiv;
		double leftSideGrid  = TimeMap2_beatsToTime(NULL, 0, &measure);
		while ((grid - leftSideGrid) * hZoom < minGridPx)
		{
			measure -= (int)gridDiv;
			leftSideGrid  = TimeMap2_beatsToTime(NULL, 0, &measure);
			gridDiv *= 2;
		}
		return leftSideGrid;
	}
}

double GetClosestRightSideGridLine (double position)
{
	double grid = GetClosestGridLine(position);
	if (position <= grid)
		return grid;

	double gridDiv = GetGridDivSafe();
	const int minGridPx = ConfigVar<int>("projgridmin").value_or(0);
	double hZoom = GetHZoomLevel();

	int num, den; TimeMap_GetTimeSigAtTime(0, position, &num, &den, NULL);

	if ((den*gridDiv) / 4 < num)
	{
		double beats; int denom;
		TimeMap2_timeToBeats(NULL, grid, NULL, NULL, &beats, &denom);
		gridDiv *= (double)denom/4;

		double rightSideGrid = TimeMap2_beatsToTime(NULL, beats + gridDiv, NULL);
		while ((rightSideGrid - grid) * hZoom < minGridPx)
		{
			gridDiv *= 2;
			rightSideGrid = TimeMap2_beatsToTime(NULL, beats + gridDiv, NULL);
		}
		return rightSideGrid;
	}
	else
	{
		gridDiv = (int)gridDiv/4;
		int measure;
		TimeMap2_timeToBeats(NULL, grid, &measure, NULL, NULL, NULL);

		measure += (int)gridDiv;
		double rightSideGrid = TimeMap2_beatsToTime(NULL, 0, &measure);
		while ((rightSideGrid - grid) * hZoom < minGridPx)
		{
			measure += (int)gridDiv;
			rightSideGrid = TimeMap2_beatsToTime(NULL, 0, &measure);
			gridDiv *= 2;
		}
		return rightSideGrid;
	}
}

/******************************************************************************
* Locking                                                                     *
******************************************************************************/
bool IsLockingActive ()
{
	const int projSelLock = ConfigVar<int>("projsellock").value_or(0);
	return !!GetBit(projSelLock, 14);
}

bool IsLocked (int lockElements)
{
	const int projSelLock = ConfigVar<int>("projsellock").value_or(0);

	if (!GetBit(projSelLock, 14))
		return false;
	else
		return ((projSelLock & lockElements) == lockElements);
}

bool IsItemLocked (MediaItem* item)
{
	char itemLock = (char)GetMediaItemInfo_Value(item, "C_LOCK");
	return (itemLock & 1);
}

/******************************************************************************
* Height                                                                      *
******************************************************************************/
int GetTrackHeightFromVZoomIndex (MediaTrack* track, int vZoom)
{
	int height = 0;
	if (track)
	{
		IconTheme* theme = SNM_GetIconTheme();

		int compact = GetEffectiveCompactLevel(track);
		if      (compact == 2) height = theme->tcp_supercollapsed_height; // Compacted track is not affected by vertical
		else if (compact == 1) height = theme->tcp_small_height;          // zoom if there is no height override
		else
		{
			if (vZoom <= 4)
			{
				const int fullArm = ConfigVar<int>("zoomshowarm").value_or(0);
				int armed =  (int)GetMediaTrackInfo_Value(track, "I_RECARM");

				if ((GetMasterTrack(NULL) == track)) height = theme->tcp_master_min_height;
				else if (fullArm && armed)           height = theme->tcp_full_height;
				else if (vZoom == 0)                 height = theme->tcp_small_height;
				else if (vZoom == 1)                 height = (theme->tcp_small_height + theme->tcp_medium_height) / 2;
				else if (vZoom == 2)                 height = theme->tcp_medium_height;
				else if (vZoom == 3)                 height = (theme->tcp_medium_height + theme->tcp_full_height) / 2;
				else if (vZoom == 4)                 height = theme->tcp_full_height;
			}
			else
			{
				// Get max and min height
				RECT r; GetClientRect(GetArrangeWnd(), &r);
				int maxHeight = r.bottom - r.top;
				int minHeight = theme->tcp_full_height;

				// Calculate track height (method stolen from Zoom.cpp - Thanks Tim!)
				int normalizedZoom = (vZoom < 30) ? (vZoom - 4) : (30 + 3*(vZoom - 30) - 4);
				height = minHeight + ((maxHeight-minHeight)*normalizedZoom) / 56;
			}
		}
	}
	return height;
}

int GetEnvHeightFromTrackHeight (int trackHeight)
{
	int height = TruncToInt((double)trackHeight * 0.75);

	int minHeight = SNM_GetIconTheme()->envcp_min_height;
	if (height < minHeight)
		height = minHeight;

	return height;
}

int GetMasterTcpGap ()
{
	return TCP_MASTER_GAP;
}

int GetTrackHeight (MediaTrack* track, int* offsetY, int* topGap /*=NULL*/, int* bottomGap /*=NULL*/)
{
	if (offsetY)
	{
		// Get track's start Y coordinate
		SCROLLINFO si{sizeof(SCROLLINFO), SIF_POS};
		CoolSB_GetScrollInfo(GetArrangeWnd(), SB_VERT, &si);

		*offsetY = si.nPos + static_cast<int>(GetMediaTrackInfo_Value(track, "I_TCPY"));
	}

	if (!TcpVis(track))
		return 0;

	int compact = GetEffectiveCompactLevel(track); // Track can't get resized at compact 2 and "I_HEIGHTOVERRIDE" could return
	if (compact == 2)                              // wrong value if track was resized prior to compacting so return here
		return SNM_GetIconTheme()->tcp_supercollapsed_height;

	const int height = static_cast<int>(GetMediaTrackInfo_Value(track, "I_TCPH"));

	if (topGap || bottomGap)
		GetTrackGap(height, topGap, bottomGap);

	return height;
}

int GetItemHeight (MediaItem* item, int* offsetY)
{
	int trackOffsetY;
	int trackH = GetTrackHeight(GetMediaItem_Track(item), &trackOffsetY);
	return GetItemHeight(item, offsetY, trackH, trackOffsetY);
}

int GetTakeHeight (MediaItem_Take* take, int* offsetY)
{
	return GetTakeHeight(take, NULL, 0, offsetY, false);
}

int GetTakeHeight (MediaItem* item, int id, int* offsetY)
{
	return GetTakeHeight(NULL, item, id, offsetY, false);
}

int GetTakeEnvHeight (MediaItem_Take* take, int* offsetY)
{
	// Last take may have different height than the others, but envelope is always positioned based
	// on take height of other takes so we get average take height (relevant for last take only)
	int envelopeH = GetTakeHeight(take, NULL, 0, offsetY, true) - 2*ENV_GAP;
	envelopeH = (envelopeH > ENV_LINE_WIDTH) ? envelopeH : 0;

	if (take && offsetY) *offsetY = *offsetY + ENV_GAP;
	return envelopeH;
}

int GetTakeEnvHeight (MediaItem* item, int id, int* offsetY)
{
	// Last take may have different height than the others, but envelope is always positioned based
	// on take height of other takes so we get average take height (relevant for last take only)
	int envelopeH = GetTakeHeight(NULL, item, id, offsetY, true) - 2*ENV_GAP;
	envelopeH = (envelopeH > ENV_LINE_WIDTH) ? envelopeH : 0;

	if (item && offsetY) *offsetY = *offsetY + ENV_GAP;
	return envelopeH;
}

int GetTrackEnvHeight (TrackEnvelope* envelope, int* offsetY, bool drawableRangeOnly, MediaTrack* parent /*=NULL*/)
{
	MediaTrack* track = parent ? parent : GetEnvParent(envelope);
	if (!track || !envelope)
	{
		WritePtr(offsetY, 0);
		return 0;
	}

	if (offsetY) {
		SCROLLINFO si{sizeof(SCROLLINFO), SIF_POS};
		CoolSB_GetScrollInfo(GetArrangeWnd(), SB_VERT, &si);

		const int trackY = si.nPos + static_cast<int>(GetMediaTrackInfo_Value(track, "I_TCPY"));

		const int envelopeY = static_cast<int>(
			GetEnvelopeInfo_Value(envelope, drawableRangeOnly ? "I_TCPY_USED" : "I_TCPY") );

		*offsetY = trackY + envelopeY;
	}

	const int envelopeHeight = static_cast<int>(
		GetEnvelopeInfo_Value(envelope, drawableRangeOnly ? "I_TCPH_USED" : "I_TCPH"));

	return envelopeHeight;
}

/******************************************************************************
* Arrange                                                                     *
******************************************************************************/
void MoveArrange (double amountTime)
{
	RECT r;
	double startTime, endTime;
	GetWindowRect(GetArrangeWnd(), &r);

	GetSetArrangeView(NULL, false, &startTime, &endTime);
	startTime += amountTime;
	endTime += amountTime;
	GetSetArrangeView(NULL, true, &startTime, &endTime);
}

void GetSetArrangeView (ReaProject* proj, bool set, double* start, double* end)
{
	if (!start || !end) return;

	if (!set) {
		double s, e;
		GetSet_ArrangeView2(proj, false, 0, 0, &s, &e); // full arrange view's start/end time -- v5.12pre4+ only
		*start = s; *end = e;
		return;
	}

	GetSet_ArrangeView2(proj, true, 0, 0, start, end);
}

void CenterArrange (double position)
{
	RECT r;
	double startTime, endTime;
	GetWindowRect(GetArrangeWnd(), &r);

	GetSetArrangeView(NULL, false, &startTime, &endTime);
	double halfSpan = (endTime - startTime) / 2;
	startTime = position - halfSpan;
	endTime = position + halfSpan;
	GetSetArrangeView(NULL, true, &startTime, &endTime);
}

void SetArrangeStart (double start)
{
	SCROLLINFO si = { sizeof(SCROLLINFO), };
	si.fMask = SIF_ALL;
	CoolSB_GetScrollInfo(GetArrangeWnd(), SB_HORZ, &si);

	si.nPos = RoundToInt(start * GetHZoomLevel()); // OCD alert: GetSet_ArrangeView2() can sometimes be off for one pixel (probably round vs trunc issue)
	CoolSB_SetScrollInfo(GetArrangeWnd(), SB_HORZ, &si, true);
	SendMessage(GetArrangeWnd(), WM_HSCROLL, SB_THUMBPOSITION, 0);
}

void MoveArrangeToTarget (double target, double reference)
{
	if (!IsOffScreen(target))
	{
		if (IsOffScreen(reference))
			MoveArrange(target - reference);
		else
			CenterArrange(target);
	}
}

void ScrollToTrackIfNotInArrange (MediaTrack* track)
{
	int offsetY;
	int height = GetTrackHeight(track, &offsetY);

	HWND hwnd = GetArrangeWnd();
	SCROLLINFO si = { sizeof(SCROLLINFO), };
	si.fMask = SIF_ALL;
	CoolSB_GetScrollInfo(hwnd, SB_VERT, &si);

	int trackEnd = offsetY + height;
	int pageEnd = si.nPos + (int)si.nPage + SCROLLBAR_W;

	if (offsetY < si.nPos || trackEnd > pageEnd)
	{
		si.nPos = offsetY;
		CoolSB_SetScrollInfo(hwnd, SB_VERT, &si, true);
		SendMessage(hwnd, WM_VSCROLL, si.nPos << 16 | SB_THUMBPOSITION, 0);
	}
}

bool IsOffScreen (double position)
{
	double startTime, endTime;
	GetSetArrangeView(NULL, false, &startTime, &endTime);

	if (position >= startTime && position <= endTime)
		return true;
	else
		return false;
}

RECT GetDrawableArrangeArea ()
{
	RECT r;

	HWND arrangeWnd = GetArrangeWnd();
	GetWindowRect(arrangeWnd, &r);
	#ifdef _WIN32
		r.right  -= SCROLLBAR_W;
		r.bottom -= SCROLLBAR_W;
	#else
		r.right  -= SCROLLBAR_W - 1;
		r.bottom += SCROLLBAR_W + 2;
	#endif

	int arrangeEnd = 0;
	if (TcpVis(GetMasterTrack(NULL)))
		arrangeEnd += (int)GetMediaTrackInfo_Value(GetMasterTrack(NULL), "I_WNDH") + GetMasterTcpGap();
	for (int i = 0; i < CountTracks(NULL); ++i)
		arrangeEnd += TcpVis(GetTrack(NULL, i)) ? (int)GetMediaTrackInfo_Value(GetTrack(NULL, i), "I_WNDH") : 0;

	SCROLLINFO si = { sizeof(SCROLLINFO), };
	si.fMask = SIF_ALL;
	CoolSB_GetScrollInfo(arrangeWnd, SB_VERT, &si);
	#ifdef _WIN32
		int pageEnd = si.nPos + si.nPage + SCROLLBAR_W + 1;
	#else
		int pageEnd = si.nPos + si.nPage + SCROLLBAR_W - 3;
	#endif

	if (pageEnd > arrangeEnd)
	{
		#ifdef _WIN32
			r.bottom -= (pageEnd - arrangeEnd);
		#else
			if (r.top > r.bottom) r.bottom += (pageEnd - arrangeEnd);
			else                  r.bottom -= (pageEnd - arrangeEnd);
		#endif
	}

	return r;
}

/******************************************************************************
* Window                                                                      *
******************************************************************************/
static void PrepareLocalizedString (const char* name, char* buf, int buf_sz)
{
	/* If all the letters in name would be part of user set locale in win32  *
	*  this would not be needed - however, that doesn't have to be the case  *
	*  so we convert name to follow user locale - in turn it can be used by  *
	*  ANSI version of GetWindowText()                                       */

	#ifdef _WIN32
		wchar_t widetext[2048];
		MultiByteToWideChar(CP_UTF8, 0, name, -1, widetext,  sizeof(widetext)/sizeof(wchar_t));
		WideCharToMultiByte(CP_ACP, 0, widetext, -1, buf, buf_sz, NULL, NULL);
		MultiByteToWideChar(CP_ACP, 0, buf, -1, widetext,  sizeof(widetext)/sizeof(wchar_t));
		WideCharToMultiByte(CP_UTF8, 0, widetext, -1, buf, buf_sz, NULL, NULL);
	#endif
}

static void AllocPreparedString (const char* name, char** destination)
{
	#ifdef _WIN32
		if (IsLocalized())
		{
			char buf[2048];
			PrepareLocalizedString(name, buf, sizeof(buf));

			int size = strlen(buf) + 1;
			if (WritePtr(destination, new (nothrow) char[size]))
				strncpy(*destination, buf, size);
		}
		else
	#endif
		{
			*destination = const_cast<char*>(name);
		}
}

static HWND SearchChildren (const char* name, HWND hwnd, HWND startHwnd = NULL, bool windowHasNoChildren = false)
{
	/* FindWindowEx alternative that works with name prepared by *
	*  PrepareLocalizedString() (relevant for win32 only)        */
	#ifdef _WIN32
		if (IsLocalized())
		{
			if (!hwnd) hwnd = GetDesktopWindow();
			hwnd = (startHwnd) ? GetWindow(startHwnd, GW_HWNDNEXT) : GetWindow(hwnd, GW_CHILD);

			HWND returnHwnd = NULL;
			do
			{
				char wndName[2048];
				GetWindowText(hwnd, wndName, sizeof(wndName));
				if (!strcmp(wndName, name))
				{
					if (!windowHasNoChildren || (windowHasNoChildren && !GetWindow(hwnd, GW_CHILD)))
						returnHwnd = hwnd;
				}
			}
			while ((hwnd = GetWindow(hwnd, GW_HWNDNEXT)) && !returnHwnd);

			return returnHwnd;
		}
		else
	#endif
		{
			HWND returnHwnd = startHwnd;
			while (true)
			{
				returnHwnd = FindWindowEx(hwnd, returnHwnd, NULL, name);
				if (!returnHwnd || !windowHasNoChildren || (windowHasNoChildren && !GetWindow(returnHwnd, GW_CHILD)))
					return returnHwnd;
			}
		}
}

static HWND SearchFloatingDockers (const char* name, const char* dockerName, bool windowHasNoChildren = false)
{
	HWND docker = FindWindowEx(NULL, NULL, NULL, dockerName);
	while (docker)
	{
		if (GetParent(docker) == g_hwndParent)
		{
			HWND insideDocker = FindWindowEx(docker, NULL, NULL, "REAPER_dock");
			while (insideDocker)
			{
				if (HWND w = SearchChildren(name, insideDocker, NULL, windowHasNoChildren))
					return w;
				insideDocker = FindWindowEx(docker, insideDocker, NULL, "REAPER_dock");
			}
		}
		docker = FindWindowEx(NULL, docker, NULL, dockerName);
	}
	return NULL;
}

static HWND FindInFloatingDockers (const char* name, bool windowHasNoChildren = false)
{
	#ifdef _WIN32
		HWND hwnd = SearchFloatingDockers(name, NULL, windowHasNoChildren);
	#else
		HWND hwnd = SearchFloatingDockers(name, __localizeFunc("Docker", "docker", 0), windowHasNoChildren);
		if (!hwnd)
		{
			WDL_FastString dockerName;
			dockerName.AppendFormatted(256, "%s%s", name, __localizeFunc(" (docked)", "docker", 0));
			hwnd = SearchFloatingDockers(name, dockerName.Get(), windowHasNoChildren);
		}
		if (!hwnd)
			hwnd = SearchFloatingDockers(name, __localizeFunc("Toolbar Docker", "docker", 0), windowHasNoChildren);
	#endif

	return hwnd;
}

static HWND FindInReaperDockers (const char* name)
{
	HWND docker = FindWindowEx(g_hwndParent, NULL, NULL, "REAPER_dock");
	while (docker)
	{
		if (HWND hwnd = SearchChildren(name, docker))
			return hwnd;
		docker = FindWindowEx(g_hwndParent, docker, NULL, "REAPER_dock");
	}
	return NULL;
}

static HWND FindFloating (const char* name, bool checkForNoCaption = false, bool windowHasNoChildren = false)
{
	HWND hwnd = SearchChildren(name, NULL);
	while (hwnd)
	{
		if (GetParent(hwnd) == g_hwndParent)
		{
			if ((!checkForNoCaption   || (checkForNoCaption   && !(GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CAPTION))) &&
				(!windowHasNoChildren || (windowHasNoChildren && !GetWindow(hwnd, GW_CHILD)))
			)
				return hwnd;
		}
			hwnd = SearchChildren(name, NULL, hwnd);
	}
	return NULL;
}

static HWND FindInReaper (const char* name)
{
	return SearchChildren(name, g_hwndParent);
}

static HWND FindReaperWndByPreparedString (const char* name)
{
	if (name)
	{
		HWND hwnd = FindInReaperDockers(name);
		if (!hwnd) hwnd = FindFloating(name);
		if (!hwnd) hwnd = FindInFloatingDockers(name);
		if (!hwnd) hwnd = FindInReaper(name);
		return hwnd;
	}
	return NULL;
}

HWND FindReaperWndByName (const char* name)
{
	#ifdef _WIN32
		if (IsLocalized())
		{
			char preparedName[2048];
			PrepareLocalizedString(name, preparedName, sizeof(preparedName));
			return FindReaperWndByPreparedString(preparedName);
		}
		else
	#endif
		{
			return FindReaperWndByPreparedString(name);
		}
}

HWND FindFloatingToolbarWndByName (const char* toolbarName)
{
	const int customMenu = ConfigVar<int>("custommenu").value_or(0);
	bool checkForNoCaption = !!GetBit(customMenu, 8);

	#ifdef _WIN32
		if (IsLocalized())
		{
			char preparedName[2048];
			PrepareLocalizedString(toolbarName, preparedName, sizeof(preparedName));
			HWND hwnd = FindFloating(preparedName, checkForNoCaption, true);
			if (!hwnd) hwnd = FindInFloatingDockers(preparedName, true);
			return hwnd;
		}
		else
	#endif
		{
			HWND hwnd = FindFloating(toolbarName, checkForNoCaption, true);
			if (!hwnd) hwnd = FindInFloatingDockers(toolbarName, true);
			return hwnd;
		}
}

HWND GetArrangeWnd ()
{
	static HWND s_hwnd = NULL; /* More efficient and Justin says it's safe: http://askjf.com/index.php?q=2653s */

	if (!s_hwnd)
	{
		#ifdef _WIN32
			if (IsLocalized())
			{
				char preparedName[2048];
				PrepareLocalizedString(__localizeFunc("trackview", "DLG_102", 0), preparedName, sizeof(preparedName));

				HWND wnd = SearchChildren(preparedName, g_hwndParent);
				while (wnd)
				{
					char buf[256];
					GetClassName(wnd, buf, sizeof(buf));
					if (!strcmp(buf, "REAPERTrackListWindow"))
					{
						s_hwnd = wnd;
						break;
					}
					wnd = SearchChildren(preparedName, g_hwndParent, wnd);
				}
			}
			else
			{
				s_hwnd = FindWindowEx(g_hwndParent, 0, "REAPERTrackListWindow", __localizeFunc("trackview", "DLG_102", 0));
			}
		#else
			return GetWindow(g_hwndParent, GW_CHILD);
		#endif
	}
	return s_hwnd;
}

HWND GetRulerWndAlt ()
{
	static HWND s_hwnd = NULL; /* More efficient and Justin says it's safe: http://askjf.com/index.php?q=2653s */
	if (!s_hwnd)
	{
		#ifdef _WIN32
			if (IsLocalized())
			{
				char preparedName[2048];
				PrepareLocalizedString(__localizeFunc("timeline", "DLG_102", 0), preparedName, sizeof(preparedName));

				HWND wnd = SearchChildren(preparedName, g_hwndParent);
				while (wnd)
				{
					char buf[256];
					GetClassName(wnd, buf, sizeof(buf));
					if (!strcmp(buf, "REAPERTimeDisplay"))
					{
						s_hwnd = wnd;
						break;
					}
					wnd = SearchChildren(preparedName, g_hwndParent, wnd);
				}
			}
			else
			{
				s_hwnd = FindWindowEx(g_hwndParent, 0, "REAPERTimeDisplay", __localizeFunc("timeline", "DLG_102", 0));
			}
		#else
			return GetWindow(GetArrangeWnd(), GW_HWNDNEXT);
		#endif
	}
	return s_hwnd;
}

HWND GetTransportWnd ()
{
	static char* s_name = NULL;
	if (!s_name)
		AllocPreparedString(__localizeFunc("Transport", "DLG_188", 0), &s_name);

	if (s_name)
	{
		HWND hwnd = FindInReaper(s_name);
		if (!hwnd) hwnd = FindInReaperDockers(s_name);
		if (!hwnd) hwnd = FindInFloatingDockers(s_name);
		if (!hwnd) hwnd = FindFloating(s_name); // transport can't float by itself but search in case this changes in the future
		return hwnd;
	}
	return NULL;
}

HWND GetMixerWnd ()
{
	static char* s_name = NULL;
	if (!s_name)
		AllocPreparedString(__localizeFunc("Mixer", "DLG_151", 0), &s_name);
	return FindReaperWndByPreparedString(s_name);
}

HWND GetMixerMasterWnd (HWND mixer)
{
	static char* s_name = NULL;
	if (!s_name)
		AllocPreparedString(__localizeFunc("Mixer Master", "mixer", 0), &s_name);

	HWND master = FindReaperWndByPreparedString(s_name);

	if (!master) // v6 in mixer
		master = FindWindowEx(mixer, nullptr, nullptr, "master");

	return master;
}

HWND GetMediaExplorerWnd ()
{
	static char* s_name = NULL;
	if (!s_name)
		AllocPreparedString(__localizeFunc("Media Explorer", "explorer", 0), &s_name);
	return FindReaperWndByPreparedString(s_name);
}

HWND GetMcpWnd (bool &isContainer)
{
	isContainer = false;
	if (HWND mixer = GetMixerWnd())
	{
		HWND hwnd = FindWindowEx(mixer, NULL, "REAPERMCPDisplay", "");
#ifdef __APPLE__
		// workaround: old macOS swell FindWindowEx() is broken when searching by classname
		char buf[1024];
		if (hwnd && (!GetClassName(hwnd,buf,sizeof(buf)) || strcmp(buf,"REAPERMCPDisplay")))
			hwnd=NULL;
#endif
		if (hwnd)
		{
			isContainer = true;
			return hwnd;
		}

		// legacy - 5.x releases
		hwnd = FindWindowEx(mixer, NULL, NULL, NULL);
		while (hwnd)
		{
			if ((MediaTrack*)GetWindowLongPtr(hwnd, GWLP_USERDATA) != GetMasterTrack(NULL)) // skip master track
				return hwnd;
			hwnd = FindWindowEx(mixer, hwnd, NULL, NULL);
		}
	}
	return NULL;
}

HWND GetTcpWnd (bool &isContainer)
{
	static bool s_is_container;
	static HWND s_hwnd = NULL;

	if (!s_hwnd)
	{
		s_hwnd = FindWindowEx(g_hwndParent, NULL, "REAPERTCPDisplay", NULL);
#ifdef __APPLE__
		// workaround: old macOS swell FindWindowEx() is broken when searching by classname
		char buf[1024];
		if (s_hwnd && (!GetClassName(s_hwnd,buf,sizeof(buf)) || strcmp(buf,"REAPERTCPDisplay")))
			s_hwnd=NULL;
#endif
		if (s_hwnd) s_is_container = true;
	}

	if (!s_hwnd)
	{
		// legacy - 5.x releases
		MediaTrack* track = GetTrack(NULL, 0);
		for (int i = 0; i < CountTracks(NULL); ++i)
		{
			MediaTrack* currentTrack = GetTrack(NULL, i);
			if (GetMediaTrackInfo_Value(currentTrack, "B_SHOWINTCP") && GetMediaTrackInfo_Value(currentTrack, "I_WNDH") != 0)
			{
				track = currentTrack;
				break;
			}
		}
		MediaTrack* master = GetMasterTrack(NULL);
		bool IsMasterVisible = TcpVis(master) ? true : false;

		// Can't find TCP if there are no tracks in project, kinda silly to expect no tracks in a DAW but oh well... :)
		MediaTrack* firstTrack = (IsMasterVisible) ? (master) : (track ? track : master);
		int masterVis = 0;
		if (!track && !IsMasterVisible)
		{
			masterVis = SetMasterTrackVisibility(GetMasterTrackVisibility() | 1);
		}

		// TCP hierarchy: TCP owner -> TCP hwnd -> tracks
		bool found = false;
		HWND tcpOwner = FindWindowEx(g_hwndParent, NULL, NULL, "");
		while (tcpOwner && !found)
		{
			HWND tcpHwnd = FindWindowEx(tcpOwner, NULL, NULL, "");
			while (tcpHwnd && !found)
			{
				HWND trackHwnd = FindWindowEx(tcpHwnd, NULL, NULL, "");
				while (trackHwnd && !found)
				{
					if ((MediaTrack*)GetWindowLongPtr(trackHwnd, GWLP_USERDATA) == firstTrack)
					{
						s_hwnd = tcpHwnd;
						found = true;
					}
					trackHwnd = FindWindowEx(tcpHwnd, trackHwnd, NULL, "");
				}
				tcpHwnd = FindWindowEx(tcpOwner, tcpHwnd, NULL, "");
			}
			tcpOwner = FindWindowEx(g_hwndParent, tcpOwner, NULL, "");
		}

		// Restore TCP master to previous state
		if (!track && !IsMasterVisible)
		{
			SetMasterTrackVisibility(masterVis);
		}
	}

	isContainer = s_is_container;
	return s_hwnd;
}

HWND GetTcpTrackWnd (MediaTrack* track, bool &isContainer)
{
	HWND par = GetTcpWnd(isContainer);
	if (isContainer)
		return par;

	HWND hwnd = GetWindow(par, GW_CHILD);
	do
	{
		if ((MediaTrack*)GetWindowLongPtr(hwnd, GWLP_USERDATA) == track)
			return hwnd;
	}
	while ((hwnd = GetWindow(hwnd, GW_HWNDNEXT)));

	return NULL;
}

HWND GetNotesView (HWND midiEditor)
{
	if (MIDIEditor_GetMode(midiEditor) != -1)
	{
		#ifdef _WIN32
			static char* s_name  = (IsLocalized()) ? (NULL) : (const_cast<char*>(__localizeFunc("midiview", "midi_DLG_102", 0)));

			if (IsLocalized())
			{
				if (!s_name)
					AllocPreparedString(__localizeFunc("midiview", "midi_DLG_102", 0),&s_name);
				return SearchChildren(s_name, midiEditor);
			}
			else
				return FindWindowEx(midiEditor, NULL, NULL , s_name);
		#else
			return GetWindow(GetWindow(midiEditor, GW_CHILD), GW_HWNDNEXT);
		#endif
	}
	else
		return NULL;
}

HWND GetPianoView (HWND midiEditor)
{
	if (MIDIEditor_GetMode(midiEditor) != -1)
	{
		#ifdef _WIN32
			static char* s_name  = (IsLocalized()) ? (NULL) : (const_cast<char*>(__localizeFunc("midipianoview", "midi_DLG_102", 0)));

			if (IsLocalized())
			{
				if (!s_name)
					AllocPreparedString(__localizeFunc("midipianoview", "midi_DLG_102", 0),&s_name);
				return SearchChildren(s_name, midiEditor);
			}
			else
				return FindWindowEx(midiEditor, NULL, NULL , s_name);
		#else
			return GetWindow(midiEditor, GW_CHILD);
		#endif
	}
	else
		return NULL;
}

HWND GetTrackView (HWND midiEditor)
{
	HWND trackListHwnd = NULL;
	if (midiEditor)
	{
		if (GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 40818) > 0)// Contents: Show/hide track list
		{
			RECT r; GetWindowRect(midiEditor, &r);
			POINT p = {r.right - 20, r.top + ((r.bottom - r.top) / 2)};


			HWND child = GetWindow(midiEditor, GW_CHILD);
			while (true)
			{
				if (!child)
					break;
				if (IsWindowVisible(child))
				{
					RECT r;
					GetWindowRect(child, &r);
					if (CheckBounds(p.x, r.left, r.right) && CheckBounds(p.y, r.top, r.bottom))
						trackListHwnd = child; // don't break, track list is last in z-order
				}
				child = GetWindow(child, GW_HWNDNEXT);
			}
		}
	}
	return trackListHwnd;
}

MediaTrack* HwndToTrack (HWND hwnd, int* hwndContext, POINT ptScreen)
{
	MediaTrack* track = NULL;
	HWND hwndParent = GetParent(hwnd);
	int context = 0;

	if (!track)
	{
		bool is_container;
		HWND tcp = GetTcpWnd(is_container);
		if (is_container)
		{
			if (hwnd == tcp)
			{
				POINT ptloc = ptScreen;
				ScreenToClient(hwnd,&ptloc);
				int tracks = GetNumTracks();
				for (int i = !*ConfigVar<int>{"showmaintrack"}; i <= tracks; ++i)
				{
					MediaTrack* chktrack = CSurf_TrackFromID(i, false);

					if (!GetMediaTrackInfo_Value(chktrack, "B_SHOWINTCP"))
						continue;

					const double ypos = GetMediaTrackInfo_Value(chktrack, "I_TCPY"),
					             h    = GetMediaTrackInfo_Value(chktrack, "I_TCPH");

					if (ptloc.y >= ypos && ptloc.y < ypos + h)
					{
						track = chktrack;
						break;
					}
				}
			}
		}
		else if (hwndParent == tcp)                                               // hwnd is a track
			track = (MediaTrack*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		else if (GetParent(hwndParent) == tcp)                               // hwnd is vu meter inside track
			track = (MediaTrack*)GetWindowLongPtr(hwndParent, GWLP_USERDATA);

		if (track)
			context = 1;
		else if (hwnd == tcp)
			context = 1;
	}

	if (!track)
	{
		bool is_container;
		HWND mcp = GetMcpWnd(is_container);
		HWND mixer = GetParent(mcp);
		HWND mixerMaster = GetMixerMasterWnd(mixer);
		HWND hwndPParent = GetParent(hwndParent);

		if (is_container)
		{
			if (hwnd == mcp)
			{
				POINT ptloc = ptScreen;
				ScreenToClient(hwnd,&ptloc);
				int tracks = GetNumTracks();
				for (int i = 0; i < tracks; ++i)
				{
					MediaTrack* chktrack = GetTrack(NULL, i);
					void *p;
					if (!(p=GetSetMediaTrackInfo(chktrack,"B_SHOWINMIXER",NULL)) || !*(bool *)p) 
						continue;
					p = GetSetMediaTrackInfo(chktrack,"I_MCPX",NULL);
					const int xpos = p ? *(int *)p : 0;
					p = GetSetMediaTrackInfo(chktrack,"I_MCPW",NULL);
					const int w = p ? *(int *)p : 0;
					if (ptloc.x < xpos || ptloc.x >= xpos + w) 
						continue;
					p = GetSetMediaTrackInfo(chktrack,"I_MCPY",NULL);
					const int ypos = p ? *(int *)p : 0;
					p = GetSetMediaTrackInfo(chktrack,"I_MCPH",NULL);
					const int h = p ? *(int *)p : 0;
					if (ptloc.y >= ypos && ptloc.y < ypos + h)
					{
						track = chktrack;
						break;
					}
				}
			}
			else if (mixerMaster && (hwnd == mixerMaster || hwndParent == mixerMaster))
				track = GetMasterTrack(NULL);
		}
		else if (hwndParent == mcp || hwndParent == mixer || hwndParent == mixerMaster)         // hwnd is a track
		{
			track = (MediaTrack*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (hwndParent == mixerMaster && !track) track = GetMasterTrack(NULL);
		}
		else if (hwndPParent == mcp || hwndPParent == mixer || hwndPParent == mixerMaster) // hwnd is vu meter inside track
			track = (MediaTrack*)GetWindowLongPtr(hwndParent, GWLP_USERDATA);

		if (track)
			context = 2;
		else if (hwnd == mcp || hwnd == mixerMaster)
			context = 2;
	}

	if (!ValidatePtr(track, "MediaTrack*"))
		track = NULL;

	WritePtr(hwndContext, context);
	return track;
}

TrackEnvelope* HwndToEnvelope (HWND hwnd, POINT ptScreen)
{
	bool is_container;
	HWND tcp = GetTcpWnd(is_container);
	if (is_container)
	{
		if (hwnd == tcp)
		{
			POINT ptloc = ptScreen;
			ScreenToClient(hwnd,&ptloc);
			const int tracks = GetNumTracks();
			for (int i = -1; i < tracks; ++i)
			{
				MediaTrack* chktrack = i<0 ? GetMasterTrack(NULL) : GetTrack(NULL, i);
				void *p;
				if (!(p=GetSetMediaTrackInfo(chktrack,"B_SHOWINTCP",NULL)) || !*(bool *)p) 
					continue;
				p = GetSetMediaTrackInfo(chktrack,"I_TCPY",NULL);
				int ypos = p ? *(int *)p : 0;
				p = GetSetMediaTrackInfo(chktrack,"I_WNDH",NULL); // includes env lanes
				int h = p ? *(int *)p : 0;
				if (ptloc.y < ypos || ptloc.y >= ypos + h) continue;

				// enumerate track envelopes
				const int envs = CountTrackEnvelopes(chktrack);
				for (int e = 0; e < envs; ++e)
				{
					TrackEnvelope* env = GetTrackEnvelope(chktrack,e);
					if (GetEnvelopeInfo_Value) // should always be true if a container
					{
						double y = GetEnvelopeInfo_Value(env,"I_TCPY");
						double h = GetEnvelopeInfo_Value(env,"I_TCPH");
						if (ptloc.y >= ypos + y && ptloc.y < ypos + y + h)
							return env;
					}
				}

				break;
			}
		}
	}
	else if (GetParent(hwnd) == tcp)
	{
		TrackEnvelope* envelope = (TrackEnvelope*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		if (ValidatePtr(envelope, "TrackEnvelope*"))
			return envelope;
	}
	return NULL;
}

void CenterDialog (HWND hwnd, HWND target, HWND zOrder)
{
	RECT r; GetWindowRect(hwnd, &r);
	RECT r1; GetWindowRect(target, &r1);

	int hwndW = r.right - r.left;
	int hwndH = r.bottom - r.top;
	int targW = r1.right - r1.left;
	int targH = r1.bottom - r1.top;

	r.left = (targW-hwndW) / 2 + r1.left;
	r.top  = (targH-hwndH) / 2 + r1.top;

	EnsureNotCompletelyOffscreen(&r);
	SetWindowPos(hwnd, zOrder, r.left, r.top, 0, 0, SWP_NOSIZE);
}

void GetMonitorRectFromPoint (const POINT& p, bool workingAreaOnly, RECT* monitorRect)
{
	if (!monitorRect)
		return;

	#ifdef _WIN32
		if (HMONITOR monitor = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST))
		{
			MONITORINFO monitorInfo = {sizeof(MONITORINFO)};
			GetMonitorInfo(monitor, &monitorInfo);
			*monitorRect = (workingAreaOnly) ? monitorInfo.rcWork : monitorInfo.rcMonitor;
		}
		else
		{
			monitorRect->left   = 0;
			monitorRect->top    = 0;
			monitorRect->right  = 800;
			monitorRect->bottom = 600;
		}
	#else
		RECT pRect = {p.x, p.y, p.x, p.y};
		SWELL_GetViewPort(monitorRect, &pRect, workingAreaOnly);
	#endif
}

void GetMonitorRectFromRect (const RECT& r, bool workingAreaOnly, RECT* monitorRect)
{
	if (!monitorRect)
		return;

	#ifdef _WIN32
		if (HMONITOR monitor = MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST))
		{
			MONITORINFO monitorInfo = {sizeof(MONITORINFO)};
			GetMonitorInfo(monitor, &monitorInfo);
			*monitorRect = (workingAreaOnly) ? monitorInfo.rcWork : monitorInfo.rcMonitor;
		}
		else
		{
			monitorRect->left   = 0;
			monitorRect->top    = 0;
			monitorRect->right  = 800;
			monitorRect->bottom = 600;
		}
	#else
		RECT pRect = {r.left, r.top, r.right, r.bottom};
		SWELL_GetViewPort(monitorRect, &pRect, workingAreaOnly);
	#endif
}

void BoundToRect (const RECT& boundingRect, RECT* r)
{
	if (!r)
		return;

	RECT tmp = boundingRect;
	#ifndef _WIN32
		if (tmp.top > tmp.bottom)
			swap(tmp.top, tmp.bottom);
	#endif

	if (r->top < tmp.top || r->bottom > tmp.bottom || r->left < tmp.left || r->right > tmp.right)
	{
		int w = r->right  - r->left;
		int h = r->bottom - r->top;

		if (r->top < tmp.top)
		{
			r->top    = tmp.top;
			r->bottom = tmp.top + h;
		}
		else if (r->bottom > tmp.bottom)
		{
			r->bottom = tmp.bottom;
			r->top    = tmp.bottom - h;
		}

		if (r->left < tmp.left)
		{
			r->left  = tmp.left;
			r->right = tmp.left + w;
		}
		else if (r->right > tmp.right)
		{
			r->right = tmp.right;
			r->left  = tmp.right - w;
		}
	}

	EnsureNotCompletelyOffscreen(r); // just in case
}

void CenterOnPoint (RECT* rect, const POINT& point, int horz, int vert, int xOffset, int yOffset)
{
	if (!rect)
		return;

	int h = rect->bottom - rect->top;
	int w = rect->right  - rect->left;

	if      (horz == -1) rect->left = point.x - w;
	else if (horz ==  0) rect->left = point.x - (w/2);
	else if (horz ==  1) rect->left = point.x;

	#ifdef _WIN32
		if      (vert == -1) rect->top = point.y;
		else if (vert ==  0) rect->top = point.y - (h/2);
		else if (vert ==  1) rect->top = point.y - h;
	#else
		if      (vert == -1) rect->top = point.y - h;
		else if (vert == 0) rect->top = point.y - (h/2);
		else                rect->top = point.y;
	#endif

	#ifdef _WIN32
		rect->top  += yOffset;
	#else
		rect->top  -= yOffset;
	#endif
	rect->left += xOffset;

	rect->right  = rect->left + w;
	rect->bottom = rect->top  + h;
}

void SimulateMouseClick (HWND hwnd, POINT point, bool keepCurrentFocus)
{
	if (hwnd)
	{
		HWND focusedHwnd;
		int focusedContext;
		if (keepCurrentFocus)
			GetSetFocus(false, &focusedHwnd, &focusedContext);

		HWND captureHwnd = GetCapture();
		SetCapture(hwnd);
		SendMessage(hwnd, WM_LBUTTONDOWN, 0, MAKELPARAM((UINT)(point.x), (UINT)(point.y)));
		SendMessage(hwnd, WM_LBUTTONUP,   0, MAKELPARAM((UINT)(point.x), (UINT)(point.y)));
		ReleaseCapture();
		SetCapture(captureHwnd);

		if (keepCurrentFocus)
			GetSetFocus(true, &focusedHwnd, &focusedContext);
	}
}

bool IsFloatingTrackFXWindow (HWND hwnd)
{
	for (int i = 0; i < CountTracks(NULL); ++i)
	{
		MediaTrack* track = GetTrack(NULL, i);
		for (int j = 0; j < TrackFX_GetCount(track); ++j)
		{
			if (hwnd == TrackFX_GetFloatingWindow(track, j))
				return true;
		}
	}
	return false;
}

/******************************************************************************
* Menus                                                                       *
******************************************************************************/
set<int> GetAllMenuIds (HMENU hMenu)
{
	set<int> menuIds;

	int count = GetMenuItemCount(hMenu);
	for (int i = 0; i < count; ++i)
	{
		if (HMENU subMenu = GetSubMenu(hMenu, i))
		{
			set<int> subMenuIds = GetAllMenuIds(subMenu);
			menuIds.insert(subMenuIds.begin(), subMenuIds.end());
		}
		else
		{
			menuIds.insert(GetMenuItemID(hMenu, i));
		}
	}

	return menuIds;
}

int GetUnusedMenuId (HMENU hMenu)
{
	set<int> menuIds = GetAllMenuIds(hMenu);
	int unusedId = 0;

	if (menuIds.empty())
	{
		unusedId = 1;
	}
	else
	{
		int last = *menuIds.rbegin();
		if (last < (std::numeric_limits<int>::max)()) // () outside is due to max() macro!
		{
			if (last < 0)
				unusedId = 1;
			else
				unusedId = last + 1;
		}
		else
		{
			for (set<int>::iterator it = menuIds.begin(); it != menuIds.end(); ++it)
			{
				if (++it != menuIds.end())
				{
					int first = *it;
					int last  = *(++it);

					bool found = false;
					if (last > 0 && last - first > 1)
					{
						while (first != last)
						{
							++first;
							if (first > 0 && first != last)
							{
								unusedId = first;
								found = true;
								break;
							}
						}
					}
					if (found)
						break;
				}
			}
		}
	}

	return unusedId;
}

/******************************************************************************
* File paths                                                                  *
******************************************************************************/
const char* GetReaperMenuIni ()
{
	static WDL_FastString s_iniPath;
	if (s_iniPath.GetLength() == 0)
		s_iniPath.SetFormatted(SNM_MAX_PATH, "%s/reaper-menu.ini", GetResourcePath());
	return s_iniPath.Get();
}

const char* GetIniFileBR ()
{
	static WDL_FastString s_iniPath;
	if (s_iniPath.GetLength() == 0)
		s_iniPath.SetFormatted(SNM_MAX_PATH, "%s/BR.ini", GetResourcePath());

	return s_iniPath.Get();
}

/******************************************************************************
* Theming                                                                     *
******************************************************************************/
void DrawTooltip (LICE_IBitmap* bm, const char* text)
{
	if (bm)
	{
		static LICE_CachedFont* s_font = NULL;
		if (!s_font)
		{
			if (HFONT ttFont = (HFONT)SendMessage(GetTooltipWindow(), WM_GETFONT, 0, 0))
			{
				if ((s_font = new (nothrow) LICE_CachedFont()))
				{
					#ifdef _WIN32
						s_font->SetFromHFont(ttFont, LICE_FONT_FLAG_OWNS_HFONT | LICE_FONT_FLAG_FORCE_NATIVE);
					#else
						s_font->SetFromHFont(ttFont, LICE_FONT_FLAG_OWNS_HFONT);
					#endif
					s_font->SetBkMode(TRANSPARENT);
					s_font->SetTextColor(LICE_RGBA(0, 0, 0, 255));
				}
			}
		}

		if (s_font)
		{
			RECT r = {0, 0, 0, 0};
			s_font->DrawText(NULL, text, -1, &r, DT_CALCRECT);
			bm->resize(r.right + 8, r.bottom + 2);

			#ifdef _WIN32
				LICE_FillRect(bm, 1, 1, bm->getWidth(), bm->getHeight(), LICE_RGBA(255, 255, 225, 255), 1.0, LICE_BLIT_MODE_COPY);
			#else
				LICE_FillRect(bm, 1, 1, bm->getWidth(), bm->getHeight(), LICE_RGBA(255, 240, 200, 255), 1.0, LICE_BLIT_MODE_COPY);
			#endif
			LICE_DrawRect(bm, 0, 0, bm->getWidth()-1, bm->getHeight()-1, LICE_RGBA(0, 0, 0, 255), 1.0, LICE_BLIT_MODE_COPY);

			r.left   += 3;
			r.right  += 3;
			r.bottom += 1;
			r.top    += 1;
			s_font->DrawText(bm, text, -1, &r, 0);
		}
	}
}

void SetWndIcon (HWND hwnd)
{
	#ifdef _WIN32
		static HICON s_icon = NULL;
		if (!s_icon)
		{
			wchar_t path[SNM_MAX_PATH];
			if (GetModuleFileNameW(NULL, path, sizeof(path)/sizeof(wchar_t)))
				s_icon = ExtractIconW(g_hInst, path, 0); // WM_GETICON isn't working so use this instead
		}

		if (s_icon)
			SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)s_icon);
	#endif
}

bool ThemeListViewInProc (HWND hwnd, int uMsg, LPARAM lParam, HWND list, bool grid)
{
	if (SWS_THEMING)
	{
		int colors[LISTVIEW_COLORHOOK_STATESIZE];
		int columns = (grid) ? (Header_GetItemCount(ListView_GetHeader(list))) : (0);
		if (ListView_HookThemeColorsMessage(hwnd, uMsg, lParam, colors, GetWindowLong(list,GWL_ID), 0, columns))
			return true;
	}
	return false;
}

/******************************************************************************
* Cursors                                                                     *
******************************************************************************/
HCURSOR GetSwsMouseCursor (BR_MouseCursor cursor)
{
	// Invalid cursor requested
	if (cursor < 0 || cursor >= CURSOR_COUNT)
	{
		return NULL;
	}
	else
	{
		static HCURSOR s_cursors[CURSOR_COUNT]; // set to NULL by compiler

		// Cursor not yet loaded
		if (!s_cursors[cursor])
		{
			const char* cursorFile = NULL;
			int         idc_resVal = -1;

			if      (cursor == CURSOR_ENV_PEN_GRID)    {idc_resVal = IDC_ENV_PEN_GRID;      cursorFile = "sws_env_pen_grid";}
			else if (cursor == CURSOR_ENV_PT_ADJ_VERT) {idc_resVal = IDC_ENV_PT_ADJ_VERT;   cursorFile = "sws_env_pt_adj_vert";}
			else if (cursor == CURSOR_GRID_WARP)       {idc_resVal = IDC_GRID_WARP;         cursorFile = "sws_grid_warp";}
			else if (cursor == CURSOR_MISC_SPEAKER)    {idc_resVal = IDC_MISC_SPEAKER;      cursorFile = "sws_misc_speaker";}
			else if (cursor == CURSOR_ZOOM_DRAG)       {idc_resVal = IDC_ZOOM_DRAG;         cursorFile = "sws_zoom_drag";}
			else if (cursor == CURSOR_ZOOM_IN)         {idc_resVal = IDC_ZOOM_IN;           cursorFile = "sws_zoom_in";}
			else if (cursor == CURSOR_ZOOM_OUT)        {idc_resVal = IDC_ZOOM_OUT;          cursorFile = "sws_zoom_out";}
			else if (cursor == CURSOR_ZOOM_UNDO)       {idc_resVal = IDC_ZOOM_UNDO;         cursorFile = "sws_zoom_undo";}

			// NF Eraser tool
			else if (cursor == CURSOR_ERASER)          {idc_resVal = IDC_ERASER;            cursorFile = "sws_eraser";}

			

			// Check for custom cursor file first
			if (cursorFile)
			{
				#ifdef _WIN32
					const wstring &resourcePathWide = win32::widen(GetResourcePath());
					const wstring &cursorFileWide   = win32::widen(cursorFile);

					wstring cursorPath(resourcePathWide);
					cursorPath.append(L"\\Cursors\\");
					cursorPath.append(cursorFileWide);
					cursorPath.append(L".cur");

					DWORD fileAttributes = GetFileAttributesW(cursorPath.c_str());
					if (fileAttributes != INVALID_FILE_ATTRIBUTES && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY))
						s_cursors[cursor] = LoadCursorFromFileW(cursorPath.c_str());
				#else
					WDL_FastString cursorPath;
					cursorPath.SetFormatted(SNM_MAX_PATH, "%s/Cursors/%s.cur", GetResourcePath(), cursorFile);
					if (file_exists(cursorPath.Get()))
						s_cursors[cursor] = SWELL_LoadCursorFromFile(cursorPath.Get());
				#endif
			}

			// No suitable file found, load default resource
			if (idc_resVal != -1 && !s_cursors[cursor])
			{
				#ifdef _WIN32
					s_cursors[cursor] = LoadCursor(g_hInst, MAKEINTRESOURCE(idc_resVal));
				#else
					s_cursors[cursor] = SWS_LoadCursor(idc_resVal);
				#endif
			}
		}
		return s_cursors[cursor];
	}
}

/******************************************************************************
* Height helpers (mostly for optimization, exposed here for BR_MouseUtil)     *
******************************************************************************/
void GetTrackGap (int trackHeight, int* top, int* bottom)
{
	/* Calculates the part above the item where buttons and labels *
	*  reside and retrieves user setting for the bottom gap        */

	if (top)
	{
		const int label = ConfigVar<int>("labelitems2").value_or(0);
		//   draw mode        ||   name                 pitch,              gain (opposite)
		if (!GetBit(label, 3) || (!GetBit(label, 0) && !GetBit(label, 2) && GetBit(label, 4)))
			*top = 0;
		else
		{
			int labelH = abs(SNM_GetColorTheme()->mediaitem_font.lfHeight) * 11/8;
			const int labelMinH = ConfigVar<int>("itemlabel_minheight").value_or(0);

			if (trackHeight - labelH < labelMinH)
			{
				*top = 0;
			}
			else
			{
				// draw label inside item if it's making item too short
				if (GetBit(label, 5) && trackHeight - labelH < ITEM_LABEL_MIN_HEIGHT)
					*top = 0;
				else
					*top = labelH;
			}
		}
	}
	if (bottom)
		*bottom = ConfigVar<int>("trackitemgap").value_or(0);
}

bool IsLastTakeTooTall (int itemHeight, int averageTakeHeight, int effectiveTakeCount, int* lastTakeHeight)
{
	/* When placing takes into item, REAPER just divides item height by number of takes *
	*  losing division reminder - last take gets all of those reminders. However, if    *
	*  it ends up to tall (>= 1.5 of average take height) it gets cropped               */

	int lastTakeH = itemHeight - averageTakeHeight * (effectiveTakeCount - 1);
	if (lastTakeH < (int)(averageTakeHeight * 1.5))
	{
		WritePtr(lastTakeHeight, lastTakeH);
		return false;
	}
	else
	{
		WritePtr(lastTakeHeight, averageTakeHeight);
		return true;
	}
}

int GetItemHeight (MediaItem* item, int* offsetY, int trackHeight, int trackOffsetY)
{
	/* Reaper takes into account FIPM and overlapping media item lanes *
	 * so no gotchas here besides track height checking since I_LASTH  *
	 * might return wrong value when track height is 0                 */

	if (trackHeight == 0 || !item)
	{
		WritePtr(offsetY, !item ? 0 : trackOffsetY);
		return 0;
	}

	WritePtr(offsetY, trackOffsetY + *(int*)GetSetMediaItemInfo(item, "I_LASTY", NULL));
	return *(int*)GetSetMediaItemInfo(item, "I_LASTH", NULL);
}

int GetMinTakeHeight (MediaTrack* track, int takeCount, int trackHeight, int itemHeight)
{
	/* Take lanes have minimum height, after which they are hidden and *
	*  and only active take is displayed. This height is determined on *
	*  per track basis                                                 */

	if (!track)
		return 0;

	int trackTop, trackBottom;
	GetTrackGap(trackHeight, &trackTop, &trackBottom);

	int overlappingItems = ConfigVar<int>("projtakelane").value_or(0);
	if (GetBit(overlappingItems, 1))
	{
		if (itemHeight == (trackHeight - trackTop - trackBottom))
			overlappingItems = 0;
	}
	else
		overlappingItems = 0;

	int takeH;
	if (*(bool*)GetSetMediaTrackInfo(track, "B_FREEMODE", NULL) || overlappingItems)
	{
		int maxTakeH = (trackHeight - trackTop - trackBottom) / takeCount; // max take height when item occupies the whole track lane

		// If max take height is over limits, reaper lets takes get
		// resized to 1 px in FIPM or when in overlapping lane
		if (takeCount > TAKE_MIN_HEIGHT_COUNT) takeH = (maxTakeH < TAKE_MIN_HEIGHT_LOW)  ? TAKE_MIN_HEIGHT_LOW  : 1;
		else                                   takeH = (maxTakeH < TAKE_MIN_HEIGHT_HIGH) ? TAKE_MIN_HEIGHT_HIGH : 1;
	}
	else
	{
		if (takeCount > TAKE_MIN_HEIGHT_COUNT) takeH = TAKE_MIN_HEIGHT_LOW;
		else                                   takeH = TAKE_MIN_HEIGHT_HIGH;
	}

	return takeH;
}

int GetTakeHeight (MediaItem_Take* take, MediaItem* item, int id, int* offsetY, bool averagedLast)
{
	MediaItem* validItem = (item) ? (item) : (GetMediaItemTake_Item(take));
	int trackOffset;
	int trackHeight = GetTrackHeight(GetMediaItem_Track(validItem), &trackOffset);
	return GetTakeHeight(take, item, id, offsetY, averagedLast, trackHeight, trackOffset);
}

int GetTakeHeight (MediaItem_Take* take, MediaItem* item, int id, int* offsetY, bool averagedLast, int trackHeight, int trackOffset)
{
	/* Calculates take height using all the input variables. Exists as *
	*  a separate function since sometimes trackHeight and trackOffset *
	*  will be known prior to calling this, so let the caller optimize *
	*  to prevent calculating the same thing twice                     */

	if ((!take && !item) || (take && item))
	{
		WritePtr(offsetY, 0);
		return 0;
	}
	MediaItem_Take* validTake = (take) ? (take) : (GetTake(item, id));
	MediaItem*      validItem = (item) ? (item) : (GetMediaItemTake_Item(take));
	MediaTrack*     track     = GetMediaItem_Track(validItem);

	// This gets us the offset to start of the item
	int takeOffset = trackOffset;
	int itemH = GetItemHeight(validItem, &takeOffset, trackHeight, takeOffset);

	const int takeLanes = ConfigVar<int>("projtakelane").value_or(0);
	int takeH = 0;

	// Take lanes displayed
	if (GetBit(takeLanes, 0))
	{
		int effTakeCount;
		int effTakeId = GetEffectiveTakeId(take, item, id, &effTakeCount); // using original pointers on purpose here
		takeH = itemH / effTakeCount; // average take height

		if (takeH < GetMinTakeHeight(track, effTakeCount, trackHeight, itemH))
		{
			takeH = (GetActiveTake(validItem) == validTake) ? itemH : 0;
		}
		else
		{
			if (effTakeId >= 0)
				takeOffset = takeOffset + takeH * effTakeId;

			// Last take gets leftover pixels from integer division
			if (!averagedLast && (effTakeId == effTakeCount - 1))
				IsLastTakeTooTall(itemH, takeH, effTakeCount, &takeH);
		}
	}
	// Take lanes not displayed
	else
	{
		if (GetActiveTake(validItem) == validTake)
			takeH = itemH;
		else
			takeH = 0;
	}

	WritePtr(offsetY, takeOffset);
	return takeH;
}
