/*****************************************************************************
/ BR_ReaScript.cpp
/
/ Copyright (c) 2014-2015 Dominik Martin Drzic
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

#include "BR_ReaScript.h"
#include "BR_EnvelopeUtil.h"
#include "BR_MidiUtil.h"
#include "BR_MouseUtil.h"
#include "BR_Util.h"
#include "../SnM/SnM.h"
#include "../SnM/SnM_Chunk.h"
#include "../SnM/SnM_Util.h"

/******************************************************************************
* Globals                                                                     *
******************************************************************************/
static BR_MouseInfo g_mouseInfo(BR_MouseInfo::MODE_ALL, false);
WDL_PtrList_DOD<BR_Envelope> g_script_brenvs; // just to validate function parameters

/******************************************************************************
* ReaScript export                                                            *
******************************************************************************/
BR_Envelope* BR_EnvAlloc (TrackEnvelope* envelope, bool takeEnvelopesUseProjectTime)
{
	if (envelope)
		return g_script_brenvs.Add(new BR_Envelope(envelope, takeEnvelopesUseProjectTime));
	return NULL;
}

int BR_EnvCountPoints (BR_Envelope* envelope)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
		return envelope->CountPoints();
	return 0;
}

bool BR_EnvDeletePoint (BR_Envelope* envelope, int id)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
		return envelope->DeletePoint(id);
	return false;
}

int BR_EnvFind (BR_Envelope* envelope, double position, double delta)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
	{
		int id = envelope->Find(position, delta);
		if (envelope->ValidateId(id))
			return id;
	}
	return -1;
}

int BR_EnvFindNext (BR_Envelope* envelope, double position)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
	{
		int id = envelope->FindNext(position);
		if (envelope->ValidateId(id))
			return id;
	}
	return -1;
}

int BR_EnvFindPrevious (BR_Envelope* envelope, double position)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
	{
		int id = envelope->FindPrevious(position);
		if (envelope->ValidateId(id))
			return id;
	}
	return -1;
}

bool BR_EnvFree (BR_Envelope* envelope, bool commit)
{
	int idx=g_script_brenvs.Find(envelope);
	if (envelope && idx>=0)
	{
		bool commited = (commit) ? envelope->Commit() : false;
		g_script_brenvs.Delete(idx, true);
		return commited;
	}
	return false;
}

MediaItem_Take* BR_EnvGetParentTake (BR_Envelope* envelope)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
		return envelope->GetTake();
	return NULL;
}

MediaTrack* BR_EnvGetParentTrack (BR_Envelope* envelope)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
	{
		if (envelope->IsTakeEnvelope())
			return NULL;
		else
			return envelope->GetParent();
	}
	return NULL;
}

bool BR_EnvGetPoint (BR_Envelope* envelope, int id, double* positionOut, double* valueOut, int* shapeOut, bool* selectedOut, double* bezierOut)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
	{
		WritePtr(selectedOut, envelope->GetSelection(id));
		return envelope->GetPoint(id, positionOut, valueOut, shapeOut, bezierOut);
	}
	return false;
}

void BR_EnvGetProperties (BR_Envelope* envelope, bool* activeOut, bool* visibleOut, bool* armedOut, bool* inLaneOut, int* laneHeightOut, int* defaultShapeOut, double* minValueOut, double* maxValueOut, double* centerValueOut, int* typeOut, bool* faderScalingOut, int* AIoptionsOut)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
	{
		WritePtr(activeOut,          envelope->IsActive());
		WritePtr(visibleOut,         envelope->IsVisible());
		WritePtr(armedOut,           envelope->IsArmed());
		WritePtr(inLaneOut,          envelope->IsInLane());
		WritePtr(laneHeightOut,      envelope->GetLaneHeight());
		WritePtr(defaultShapeOut,    envelope->GetDefaultShape());
		WritePtr(minValueOut,        envelope->LaneMinValue());
		WritePtr(maxValueOut,        envelope->LaneMaxValue());
		WritePtr(centerValueOut,     envelope->LaneCenterValue());
		WritePtr(faderScalingOut,    envelope->IsScaledToFader());
		WritePtr(AIoptionsOut,       envelope->GetAIoptions());

		if (typeOut)
		{
			int value = envelope->Type();
			if      (value == VOLUME)       value = 0;
			else if (value == VOLUME_PREFX) value = 1;
			else if (value == PAN)          value = 2;
			else if (value == PAN_PREFX)    value = 3;
			else if (value == WIDTH)        value = 4;
			else if (value == WIDTH_PREFX)  value = 5;
			else if (value == MUTE)         value = 6;
			else if (value == PITCH)        value = 7;
			else if (value == PLAYRATE)     value = 8;
			else if (value == TEMPO)        value = 9;
			else if (value == PARAMETER)    value = 10;
			else                            value = -1;

			*typeOut = value;
		}
	}
	else
	{
		WritePtr(activeOut,          false);
		WritePtr(visibleOut,         false);
		WritePtr(armedOut,           false);
		WritePtr(inLaneOut,          false);
		WritePtr(laneHeightOut,      0);
		WritePtr(defaultShapeOut,    0);
		WritePtr(minValueOut,        0.0);
		WritePtr(maxValueOut,        0.0);
		WritePtr(centerValueOut,     0.0);
		WritePtr(typeOut,            -1);
		WritePtr(faderScalingOut,    false);
		WritePtr(AIoptionsOut,       -1);
	}
}

bool BR_EnvSetPoint (BR_Envelope* envelope, int id, double position, double value, int shape, bool selected, double bezier)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
		return envelope->SetCreatePoint(id, position, value, shape, bezier, selected);
	return false;
}

void BR_EnvSetProperties (BR_Envelope* envelope, bool active, bool visible, bool armed, bool inLane, int laneHeight, int defaultShape, bool faderScaling, int* AIoptionsOptional)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
	{
		envelope->SetActive(active);
		envelope->SetVisible(visible);
		envelope->SetArmed(armed);
		envelope->SetInLane(inLane);
		envelope->SetLaneHeight(laneHeight);
		envelope->SetScalingToFader(faderScaling);
		if (defaultShape >= 0 && defaultShape <= 5)
			envelope->SetDefaultShape(defaultShape);

		double runningReaVer = atof(GetAppVersion());
		if (runningReaVer >= 5.979)
		{
			// optional param, != NULL -> caller provided this param
			// works properly since REAPER v5.979
			if (AIoptionsOptional != NULL && *AIoptionsOptional >= -1 && *AIoptionsOptional <= 6) 
				envelope->SetAIoptions(*AIoptionsOptional);
		}
	}
}

void BR_EnvSortPoints (BR_Envelope* envelope)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
		envelope->Sort();
}

double BR_EnvValueAtPos (BR_Envelope* envelope, double position)
{
	if (envelope && g_script_brenvs.Find(envelope)>=0)
		return envelope->ValueAtPosition(position);
	return 0;
}

void BR_GetArrangeView (ReaProject* proj, double* startPositionOut, double* endPositionOut)
{
	double start, end;
	GetSetArrangeView(proj, false, &start, &end);

	WritePtr(startPositionOut, start);
	WritePtr(endPositionOut, end);
}

double BR_GetClosestGridDivision (double position)
{
	return GetClosestGridDiv(position);
}

void BR_GetCurrentTheme (char* themePathOut, int themePathOut_sz, char* themeNameOut, int themeNameOut_sz)
{
	std::string themeName;
	const char *fullThemePath = GetCurrentTheme(&themeName);

	snprintf(themePathOut, themePathOut_sz, "%s", fullThemePath);
	snprintf(themeNameOut, themeNameOut_sz, "%s", themeName.c_str());
}

MediaItem* BR_GetMediaItemByGUID (ReaProject* proj, const char* guidStringIn)
{
	if (guidStringIn)
	{
		GUID guid;
		stringToGuid(guidStringIn, &guid);
		return GuidToItem(&guid, proj);
	}
	else
		return NULL;
}

void BR_GetMediaItemGUID (MediaItem* item, char* guidStringOut, int guidStringOut_sz)
{
	if (item && guidStringOut && guidStringOut_sz > 0)
	{
		char guid[64];
		if (item) guidToString((GUID*)GetSetMediaItemInfo(item, "GUID", NULL), guid);
		else      guidToString(&GUID_NULL, guid);
		snprintf(guidStringOut, guidStringOut_sz, "%s", guid);
	}
}

bool BR_GetMediaItemImageResource (MediaItem* item, char* imageOut, int imageOut_sz, int* imageFlagsOut)
{
	bool resourceFound = false;
	if (item)
	{
		SNM_ChunkParserPatcher p(item);
		p.SetWantsMinimalState(true);

		char image[SNM_MAX_PATH]      = "";
		char imageFlags[SNM_MAX_PATH] = "0";

		resourceFound =  !!p.Parse(SNM_GET_CHUNK_CHAR, 1, "ITEM", "RESOURCEFN",       0, 1, image,      NULL, "VOLPAN");
		if (resourceFound) p.Parse(SNM_GET_CHUNK_CHAR, 1, "ITEM", "IMGRESOURCEFLAGS", 0, 1, imageFlags, NULL, "VOLPAN");

		if (imageOut && imageOut_sz > 0) snprintf(imageOut, imageOut_sz, "%s", image);
		WritePtr(imageFlagsOut, atoi(imageFlags));
	}
	return resourceFound;
}

void BR_GetMediaItemTakeGUID (MediaItem_Take* take, char* guidStringOut, int guidStringOut_sz)
{
	if (take && guidStringOut && guidStringOut_sz > 0)
	{
		char guid[64];
		if (take) guidToString((GUID*)GetSetMediaItemTakeInfo(take, "GUID", NULL), guid);
		else      guidToString(&GUID_NULL, guid);
		snprintf(guidStringOut, guidStringOut_sz, "%s", guid);
	}
}

bool BR_GetMediaSourceProperties (MediaItem_Take* take, bool* sectionOut, double* startOut, double* lengthOut, double* fadeOut, bool* reverseOut)
{
	return GetMediaSourceProperties(take, sectionOut, startOut, lengthOut, fadeOut, reverseOut);
}

MediaTrack* BR_GetMediaTrackByGUID (ReaProject* proj, const char* guidStringIn)
{
	if (guidStringIn)
	{
		GUID guid;
		stringToGuid(guidStringIn, &guid);
		for (int i = 0; i < CountTracks(proj); ++i)
		{
			MediaTrack* track = GetTrack(proj, i);
			if (GuidsEqual(&guid, TrackToGuid(proj, track)))
				return track;
		}
	}
	return NULL;
}

int BR_GetMediaTrackFreezeCount (MediaTrack* track)
{
	return GetTrackFreezeCount(track);
}

void BR_GetMediaTrackGUID (MediaTrack* track, char* guidStringOut, int guidStringOut_sz)
{
	if (track && guidStringOut && guidStringOut_sz > 0)
	{
		char guid[64];
		if (track) guidToString(TrackToGuid(track), guid);
		else       guidToString(&GUID_NULL, guid);
		snprintf(guidStringOut, guidStringOut_sz, "%s", guid);
	}
}

void BR_GetMediaTrackLayouts (MediaTrack* track, char* mcpLayoutNameOut, int mcpLayoutNameOut_sz, char* tcpLayoutNameOut, int tcpLayoutNameOut_sz)
{
	if (mcpLayoutNameOut && mcpLayoutNameOut_sz > 0) *mcpLayoutNameOut=0;
	if (tcpLayoutNameOut && tcpLayoutNameOut_sz > 0) *tcpLayoutNameOut=0;
	if (track)
	{
		if (mcpLayoutNameOut)
		{
			const char *l = (const char*)GetSetMediaTrackInfo(track, "P_MCP_LAYOUT", NULL);
			if (l) snprintf(mcpLayoutNameOut, mcpLayoutNameOut_sz, "%s", l);
		}
		if (tcpLayoutNameOut)
		{
			const char *l = (const char*)GetSetMediaTrackInfo(track, "P_TCP_LAYOUT", NULL);
			if (l) snprintf(tcpLayoutNameOut, tcpLayoutNameOut_sz, "%s", l);
		}
	}
}

TrackEnvelope* BR_GetMediaTrackSendInfo_Envelope (MediaTrack* track, int category, int sendidx, int envelopeType)
{
	const char* sendType = (envelopeType == 0) ? ("<VOLENV") : (envelopeType == 1) ? ("<PANENV") : (envelopeType == 2) ? ("<MUTEENV") : NULL;

	if (sendType)
		return (TrackEnvelope*)GetSetTrackSendInfo(track, category, sendidx, "P_ENV", (void*)sendType);
	else
		return NULL;
}

MediaTrack* BR_GetMediaTrackSendInfo_Track (MediaTrack* track, int category, int sendidx, int trackType)
{
	if (trackType == 0 || trackType == 1)
		return (MediaTrack*)GetSetTrackSendInfo(track, category, sendidx, ((trackType == 0) ? "P_SRCTRACK" : "P_DESTTRACK"), NULL);
	else
		return NULL;
}

double BR_GetMidiSourceLenPPQ (MediaItem_Take* take)
{
	double length = -1;
	if (take)
	{
		bool isMidi = false;
		length = GetMidiSourceLengthPPQ(take, false, &isMidi);
		if (!isMidi) length = -1;
	}
	return length;
}

bool BR_GetMidiTakePoolGUID (MediaItem_Take* take, char* guidStringOut, int guidStringOut_sz)
{
	if (take && IsMidi(take, NULL) && guidStringOut)
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
				WDL_FastString pooledEventsLine;
				if (ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "POOLEDEVTS", 0, -1, &pooledEventsLine))
				{
					LineParser lp(false);
					lp.parse(pooledEventsLine.Get());
					snprintf(guidStringOut, guidStringOut_sz, "%s", lp.gettoken_str(1));
				}
			}
		}
	}

	if (PCM_source* source = GetMediaItemTake_Source(take))
		return !strcmp(source->GetType(), "MIDIPOOL");
	else
		return false;
}

bool BR_GetMidiTakeTempoInfo (MediaItem_Take* take, bool* ignoreProjTempoOut, double* bpmOut, int* numOut, int* denOut)
{
	return GetMidiTakeTempoInfo (take, ignoreProjTempoOut, bpmOut, numOut, denOut);
}

void BR_GetMouseCursorContext (char* windowOut, int windowOut_sz, char* segmentOut, int segmentOut_sz, char* detailsOut, int detailsOut_sz)
{
	g_mouseInfo.Update();

	if (windowOut  && windowOut_sz  > 0) snprintf(windowOut,  windowOut_sz,  "%s", g_mouseInfo.GetWindow());
	if (segmentOut && segmentOut_sz > 0) snprintf(segmentOut, segmentOut_sz, "%s", g_mouseInfo.GetSegment());
	if (detailsOut && detailsOut_sz > 0) snprintf(detailsOut, detailsOut_sz, "%s", g_mouseInfo.GetDetails());
}

TrackEnvelope* BR_GetMouseCursorContext_Envelope (bool* takeEnvelopeOut)
{
	WritePtr(takeEnvelopeOut, g_mouseInfo.IsTakeEnvelope());
	return g_mouseInfo.GetEnvelope();
}

MediaItem* BR_GetMouseCursorContext_Item ()
{
	return g_mouseInfo.GetItem();
}

void* BR_GetMouseCursorContext_MIDI (bool* inlineEditorOut, int* noteRowOut, int* ccLaneOut, int* ccLaneValOut, int* ccLaneIdOut)
{
	if (g_mouseInfo.GetNoteRow() == -1)
		WritePtr(noteRowOut, -1);
	else
		WritePtr(noteRowOut, g_mouseInfo.GetNoteRow());

	if (g_mouseInfo.GetCCLane(NULL, NULL, NULL))
	{
		g_mouseInfo.GetCCLane(ccLaneOut, ccLaneValOut, ccLaneIdOut);
		WritePtr(ccLaneOut, MapVelLaneToReaScriptCC(*ccLaneOut));
	}
	else
	{
		WritePtr(ccLaneOut,    -1);
		WritePtr(ccLaneValOut, -1);
		WritePtr(ccLaneIdOut,  -1);
	}

	WritePtr(inlineEditorOut, g_mouseInfo.IsInlineMidi());
	return (void*)g_mouseInfo.GetMidiEditor();
}

double BR_GetMouseCursorContext_Position ()
{
	double position = g_mouseInfo.GetPosition();
	if (position == -1)
		return -1;
	else
		return position;
}

int BR_GetMouseCursorContext_StretchMarker ()
{
	int id = g_mouseInfo.GetStretchMarker();
	if (id == -1)
		return -1;
	else
		return id;
}

MediaItem_Take* BR_GetMouseCursorContext_Take ()
{
	return g_mouseInfo.GetTake();
}

MediaTrack* BR_GetMouseCursorContext_Track ()
{
	return g_mouseInfo.GetTrack();
}

double BR_GetNextGridDivision (double position)
{
	if (position >= 0)
		return GetNextGridDiv(position);
	else
		return 0;
}

double BR_GetPrevGridDivision (double position)
{
	if (position > 0)
		return GetPrevGridDiv(position);
	else
		return 0;
}

double BR_GetSetTrackSendInfo (MediaTrack* track, int category, int sendidx, const char* parmname, bool setNewValue, double newValue)
{
	double returnValue = 0;
	if (track)
	{
		if (setNewValue)
		{
			if (!strcmp(parmname, "B_MUTE") || !strcmp(parmname, "B_PHASE") || !strcmp(parmname, "B_MONO"))
			{
				bool valueToSet = !!newValue;
				GetSetTrackSendInfo(track, category, sendidx, parmname, (void*)&valueToSet);
				returnValue = 1;
			}
			else if (!strcmp(parmname, "D_VOL") || !strcmp(parmname, "D_PAN") || !strcmp(parmname, "D_PANLAW"))
			{
				double valueToSet = (double)newValue;
				GetSetTrackSendInfo(track, category, sendidx, parmname, (void*)&valueToSet);
				returnValue = 1;
			}
			else if (!strcmp(parmname, "I_SENDMODE") || !strcmp(parmname, "I_SRCCHAN") || !strcmp(parmname, "I_DSTCHAN") || !strcmp(parmname, "I_MIDIFLAGS"))
			{
				int valueToSet = (int)newValue;
				GetSetTrackSendInfo(track, category, sendidx, parmname, (void*)&valueToSet);
				returnValue = 1;
			}
			else if (!strcmp(parmname, "I_MIDI_LINK_VOLPAN") || !strcmp(parmname, "I_MIDI_SRCCHAN") || !strcmp(parmname, "I_MIDI_DSTCHAN") || !strcmp(parmname, "I_MIDI_SRCBUS") || !strcmp(parmname, "I_MIDI_DSTBUS"))
			{
				if (void* sendInfo = GetSetTrackSendInfo(track, category, sendidx, "I_MIDIFLAGS", NULL))
				{
					int valueToSet = *(int*)sendInfo;
					if      (newValue == -1)                          valueToSet |= 0x3FC01F;                                            // set all bits for source channel and bus to 1
					else if (!strcmp(parmname, "I_MIDI_LINK_VOLPAN")) valueToSet = SetBit(valueToSet, 10, (newValue != 0));
					else if (!strcmp(parmname, "I_MIDI_SRCCHAN"))     valueToSet = (valueToSet & ~(0x1F))       | ((int)newValue);       // source chan is bits 0-4
					else if (!strcmp(parmname, "I_MIDI_DSTCHAN"))     valueToSet = (valueToSet & ~(0x1F << 5))  | ((int)newValue << 5);  // destination chan is bits 5-9
					else if (!strcmp(parmname, "I_MIDI_SRCBUS"))      valueToSet = (valueToSet & ~(0xFF << 14)) | ((int)newValue << 14); // source MIDI bus is bits 14-21
					else if (!strcmp(parmname, "I_MIDI_DSTBUS"))      valueToSet = (valueToSet & ~(0xFF << 22)) | ((int)newValue << 22); // destination MIDI bus is bits 22-29
					GetSetTrackSendInfo(track, category, sendidx, "I_MIDIFLAGS", (void*)&valueToSet);
					returnValue = 1;
				}
			}
		}
		else
		{
			if (!strcmp(parmname, "I_MIDI_LINK_VOLPAN") || !strcmp(parmname, "I_MIDI_SRCCHAN") || !strcmp(parmname, "I_MIDI_DSTCHAN") || !strcmp(parmname, "I_MIDI_SRCBUS") || !strcmp(parmname, "I_MIDI_DSTBUS"))
			{
				if (void* sendInfo = GetSetTrackSendInfo(track, category, sendidx, "I_MIDIFLAGS", NULL))
				{
					int midiFlags = *(int*)sendInfo;
					if      ((midiFlags & 31) == 31)                  returnValue = -1;
					else if (!strcmp(parmname, "I_MIDI_LINK_VOLPAN")) returnValue = (GetBit(midiFlags, 10) ? 1 : 0);
					else if (!strcmp(parmname, "I_MIDI_SRCCHAN"))     returnValue = midiFlags         & 0x1F; // source chan is bits 0-4
					else if (!strcmp(parmname, "I_MIDI_DSTCHAN"))     returnValue = (midiFlags >> 5)  & 0x1F; // destination chan is bits 5-9
					else if (!strcmp(parmname, "I_MIDI_SRCBUS"))      returnValue = (midiFlags >> 14) & 0xFF; // source MIDI bus is bits 14-21
					else if (!strcmp(parmname, "I_MIDI_DSTBUS"))      returnValue = (midiFlags >> 22) & 0xFF; // destination MIDI bus is bits 22-29
				}
			}
			else if (void* sendInfo = GetSetTrackSendInfo(track, category, sendidx, parmname, NULL))
			{
				if (!strcmp(parmname, "B_MUTE") || !strcmp(parmname, "B_PHASE") || !strcmp(parmname, "B_MONO"))
				{
					returnValue = (double)(*(bool*)sendInfo);
				}
				else if (!strcmp(parmname, "D_VOL") || !strcmp(parmname, "D_PAN") || !strcmp(parmname, "D_PANLAW"))
				{
					returnValue = *(double*)sendInfo;
				}
				else if (!strcmp(parmname, "I_SENDMODE") || !strcmp(parmname, "I_SRCCHAN") || !strcmp(parmname, "I_DSTCHAN"))
				{
					returnValue = (double)(*(int*)sendInfo);
				}

			}
		}
	}

	return returnValue;
}

int BR_GetTakeFXCount (MediaItem_Take* take)
{
	return GetTakeFXCount(take);
}

bool BR_IsTakeMidi (MediaItem_Take* take, bool* inProjectMidiOut)
{
	return IsMidi(take, inProjectMidiOut);
}

bool BR_IsMidiOpenInInlineEditor(MediaItem_Take* take)
{
	return IsOpenInInlineEditor(take);
}

MediaItem* BR_ItemAtMouseCursor (double* positionOut)
{
	return ItemAtMouseCursor(positionOut);
}

bool BR_MIDI_CCLaneRemove (void* midiEditor, int laneId)
{
	MediaItem_Take* take = MIDIEditor_GetTake((HWND)midiEditor);
	if (take)
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

				// NF: prevent trying to remove the only visible one, results in weird behaviour
				if (ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "VELLANE", laneId  == 0 ? 1 : laneId, -1))
				{
					ptk.RemoveLine("SOURCE", "VELLANE", 1, laneId);
					return p.ReplaceTake(tkPos, tklen, ptk.GetChunk());
				}
			}
		}
	}
	return false;
}

bool BR_MIDI_CCLaneReplace (void* midiEditor, int laneId, int newCC)
{
	MediaItem_Take* take = MIDIEditor_GetTake((HWND)midiEditor);
	int newLane = MapReaScriptCCToVelLane(newCC);

	if (take && IsVelLaneValid(newLane))
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

				if (ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "VELLANE", laneId, -1))
				{
					WDL_FastString newLaneStr;
					newLaneStr.AppendFormatted(256, "%d", newLane);
					ptk.ParsePatch(SNM_SET_CHUNK_CHAR, 1, "SOURCE", "VELLANE", laneId, 1, (void*)newLaneStr.Get());
					return p.ReplaceTake(tkPos, tklen, ptk.GetChunk());
				}
			}
		}
	}
	return false;
}

double BR_PositionAtMouseCursor (bool checkRuler)
{
	return PositionAtMouseCursor(checkRuler, true);
}

void BR_SetArrangeView (ReaProject* proj, double startPosition, double endPosition)
{
	GetSetArrangeView(proj, true, &startPosition, &endPosition);
}

bool BR_SetItemEdges (MediaItem* item, double startTime, double endTime)
{
	return TrimItem_UseNativeTrimActions(item, startTime, endTime);
}

void BR_SetMediaItemImageResource (MediaItem* item, const char* imageIn, int imageFlags)
{
	if (item)
	{
		char* itemState = GetSetObjectState(item, "");
		char* token = strtok(itemState, "\n");

		WDL_FastString newState;
		LineParser lp(false);

		bool didImage      = !imageIn;
		bool didImageFlags = false;
		bool doImageFlags  = (!imageIn || (imageIn && strcmp(imageIn, "")));
		bool commitTwice   = false; // in case image path is the same as one already set in the item, reaper will remove it in case we supply chunk with the same path (and sometimes we just want to change the flag)
		bool skipCommit    = false;

		int blockCount = 0;
		while (token != NULL)
		{
			lp.parse(token);
			if      (lp.gettoken_str(0)[0] == '<')  ++blockCount;
			else if (lp.gettoken_str(0)[0] == '>')  --blockCount;

			if (blockCount == 1)
			{
				if (!strcmp(lp.gettoken_str(0), "RESOURCEFN") && !didImage)
				{
					if (strcmp(imageIn, ""))
					{
						for (int i = 0; i < lp.getnumtokens(); ++i)
						{
							if (i == 1) newState.AppendFormatted(SNM_MAX_PATH + 2, "\"%s\"", imageIn);
							else        newState.Append(lp.gettoken_str(i));
							newState.Append(" ");
						}
						newState.Append("\n");

						if (!strcmp(imageIn, lp.gettoken_str(1)))
							commitTwice = true;
					}
					else
					{
						doImageFlags = false;
					}


					didImage = true;
				}
				else if (!strcmp(lp.gettoken_str(0), "IMGRESOURCEFLAGS") && !didImageFlags)
				{
					if (doImageFlags)
					{
						for (int i = 0; i < lp.getnumtokens(); ++i)
						{
							if (i == 1) newState.AppendFormatted(128, "%d", imageFlags);
							else        newState.Append(lp.gettoken_str(i));
							newState.Append(" ");
						}
						newState.Append("\n");

						if (commitTwice && lp.gettoken_int(1) == imageFlags)
							skipCommit = true;
					}


					didImageFlags = true;
				}
				else if (lp.gettoken_str(0)[0] == '>')
				{
					AppendLine(newState, token);

					if (!didImage && strcmp(imageIn, ""))
					{
						newState.Append("RESOURCEFN \"");
						newState.Append(imageIn);
						newState.Append("\"");
						newState.Append("\n");
						didImage = true;
					}

					if (didImage && !didImageFlags && doImageFlags)
					{
						newState.Append("IMGRESOURCEFLAGS ");
						newState.AppendFormatted(128, "%d", imageFlags);
						newState.Append("\n");
						didImageFlags = true;
					}
				}
				else
				{
					AppendLine(newState, token);
				}
			}
			else
				AppendLine(newState, token);

			token = strtok(NULL, "\n");
		}

		if (!skipCommit)
		{
			GetSetObjectState(item, newState.Get());
			if (commitTwice)
				GetSetObjectState(item, newState.Get());
		}
		FreeHeapPtr(itemState);
	}
}

bool BR_SetMediaSourceProperties (MediaItem_Take* take, bool section, double start, double length, double fade, bool reverse)
{
	return SetMediaSourceProperties(take, section, start, length, fade, reverse);
}

bool BR_SetMediaTrackLayouts (MediaTrack* track, const char* mcpLayoutNameIn, const char* tcpLayoutNameIn)
{
	bool changedLayouts = false;
	if (track)
	{
		if (mcpLayoutNameIn)
		{
			const char* currentMpc = (const char*)GetSetMediaTrackInfo(track, "P_MCP_LAYOUT", NULL);
			if (currentMpc && strcmp(currentMpc, mcpLayoutNameIn))
			{
				GetSetMediaTrackInfo(track, "P_MCP_LAYOUT", (void*)mcpLayoutNameIn);
				changedLayouts = true;
			}
		}

		if (tcpLayoutNameIn)
		{
			const char* currentMpc = (const char*)GetSetMediaTrackInfo(track, "P_TCP_LAYOUT", NULL);
			if (currentMpc && strcmp(currentMpc, tcpLayoutNameIn))
			{
				GetSetMediaTrackInfo(track, "P_TCP_LAYOUT", (void*)tcpLayoutNameIn);
				changedLayouts = true;
			}
		}
	}
	return changedLayouts;
}

bool BR_SetMidiTakeTempoInfo (MediaItem_Take* take, bool ignoreProjTempo, double bpm, int num, int den)
{
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
				if (int position = ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "IGNTEMPO", 0, -1, &tempoLine))
				{
					LineParser lp(false);
					lp.parse(tempoLine.Get());

					if (ignoreProjTempo != !!lp.gettoken_int(1) ||  bpm != lp.gettoken_float(2) || num != lp.gettoken_int(3) || den != lp.gettoken_int(4))
					{
						WDL_FastString newLine;
						for (int i = 0; i < lp.getnumtokens(); ++i)
						{
							if      (i == 1) newLine.AppendFormatted(256, "%d",  ignoreProjTempo ? 1 : 0);
							else if (i == 2) newLine.AppendFormatted(256, "%lf", bpm);
							else if (i == 3) newLine.AppendFormatted(256, "%d",  num);
							else if (i == 4) newLine.AppendFormatted(256, "%d",  den);
							else             newLine.AppendFormatted(256, "%s", lp.gettoken_str(i));
							newLine.Append(" ");
						}
						newLine.Append("\n");
						ptk.ReplaceLine(position - 1, newLine.Get());
						succes = p.ReplaceTake(tkPos, tklen, ptk.GetChunk());
					}

				}
			}
		}
	}

	return succes;
}

bool BR_SetTakeSourceFromFile (MediaItem_Take* take, const char* filenameIn, bool inProjectData)
{
	return SetTakeSourceFromFile(take, filenameIn, inProjectData, false);
}

bool BR_SetTakeSourceFromFile2 (MediaItem_Take* take, const char* filenameIn, bool inProjectData, bool keepSourceProperties)
{
	return SetTakeSourceFromFile(take, filenameIn, inProjectData, keepSourceProperties);
}

MediaItem_Take* BR_TakeAtMouseCursor (double* positionOut)
{
	return TakeAtMouseCursor(positionOut);
}

MediaTrack* BR_TrackAtMouseCursor (int* contextOut, double* positionOut)
{
	return TrackAtMouseCursor(contextOut, positionOut);
}

bool BR_TrackFX_GetFXModuleName (MediaTrack* track, int fx, char* nameOut, int nameOutSz)
{
	WDL_FastString module;
	bool found = false;

	if (track)
	{
		SNM_FXSummaryParser trackFxs(track);
		if (SNM_FXSummary* summary =  trackFxs.GetSummaries()->Get(fx))
		{
			module = summary->m_realName;
			found = true;
		}
	}

	snprintf(nameOut, nameOutSz, "%s", module.Get());
	return found;
}

int BR_Win32_CB_FindString(void* comboBoxHwnd, int startId, const char* string)
{
	if (comboBoxHwnd && string)
		return (int)SendMessage((HWND)comboBoxHwnd, CB_FINDSTRING, (WPARAM)startId, (LPARAM)string);
	else
		return CB_ERR;
}

int BR_Win32_CB_FindStringExact(void* comboBoxHwnd, int startId, const char* string)
{
	if (comboBoxHwnd && string)
		return (int)SendMessage((HWND)comboBoxHwnd, CB_FINDSTRINGEXACT, (WPARAM)startId, (LPARAM)string);
	else
		return CB_ERR;
}

void BR_Win32_ClientToScreen(void* hwnd, int xIn, int yIn, int* xOut, int* yOut)
{
	POINT p;
	p.x = xIn;
	p.y = yIn;

	ClientToScreen((HWND)hwnd, &p);
	WritePtr(xOut, (int)p.x);
	WritePtr(yOut, (int)p.y);
}

void* BR_Win32_FindWindowEx(const char* hwndParent, const char* hwndChildAfter, const char* className, const char* windowName, bool searchClass, bool searchName)
{
	return (void*)FindWindowEx((HWND)BR_Win32_StringToHwnd(hwndParent), (HWND)BR_Win32_StringToHwnd(hwndChildAfter), searchClass ? className : NULL, searchName ? windowName : NULL);
}

int BR_Win32_GET_X_LPARAM(int lParam)
{
	return GET_X_LPARAM(lParam);
}

int BR_Win32_GET_Y_LPARAM(int lParam)
{
	return GET_Y_LPARAM(lParam);
}

int BR_Win32_GetConstant(const char* constantName)
{
#ifndef _WIN32
#define SW_MAXIMIZE         0x3
#define SWP_NOOWNERZORDER   0x0200
#define WS_MAXIMIZE         0x01000000L
#define WS_MAXIMIZEBOX      0x00010000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_OVERLAPPED       0x00000000L
#define WS_OVERLAPPEDWINDOW WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
#endif

	int constant = -1;
	if (constantName)
	{
		if (!strcmp(constantName, "CB_ERR"))              constant = CB_ERR;
		else if (!strcmp(constantName, "CB_GETCOUNT"))         constant = CB_GETCOUNT;
		else if (!strcmp(constantName, "CB_GETCURSEL"))        constant = CB_GETCURSEL;
		else if (!strcmp(constantName, "CB_SETCURSEL"))        constant = CB_SETCURSEL;
		else if (!strcmp(constantName, "EM_SETSEL"))           constant = EM_SETSEL;
		else if (!strcmp(constantName, "GW_CHILD"))            constant = GW_CHILD;
		else if (!strcmp(constantName, "GW_HWNDFIRST"))        constant = GW_HWNDFIRST;
		else if (!strcmp(constantName, "GW_HWNDLAST"))         constant = GW_HWNDLAST;
		else if (!strcmp(constantName, "GW_HWNDNEXT"))         constant = GW_HWNDNEXT;
		else if (!strcmp(constantName, "GW_HWNDPREV"))         constant = GW_HWNDPREV;
		else if (!strcmp(constantName, "GW_OWNER"))            constant = GW_OWNER;
		else if (!strcmp(constantName, "GWL_STYLE"))           constant = GWL_STYLE;
		else if (!strcmp(constantName, "SW_HIDE"))             constant = SW_HIDE;
		else if (!strcmp(constantName, "SW_MAXIMIZE"))         constant = SW_MAXIMIZE;
		else if (!strcmp(constantName, "SW_SHOW"))             constant = SW_SHOW;
		else if (!strcmp(constantName, "SW_SHOWMINIMIZED"))    constant = SW_SHOWMINIMIZED;
		else if (!strcmp(constantName, "SW_SHOWNA"))           constant = SW_SHOWNA;
		else if (!strcmp(constantName, "SW_SHOWNOACTIVATE"))   constant = SW_SHOWNOACTIVATE;
		else if (!strcmp(constantName, "SW_SHOWNORMAL"))       constant = SW_SHOWNORMAL;
		else if (!strcmp(constantName, "SWP_FRAMECHANGED"))    constant = SWP_FRAMECHANGED;
		else if (!strcmp(constantName, "SWP_NOACTIVATE"))      constant = SWP_FRAMECHANGED;
		else if (!strcmp(constantName, "SWP_NOMOVE"))          constant = SWP_NOMOVE;
		else if (!strcmp(constantName, "SWP_NOOWNERZORDER"))   constant = SWP_NOOWNERZORDER;
		else if (!strcmp(constantName, "SWP_NOSIZE"))          constant = SWP_NOSIZE;
		else if (!strcmp(constantName, "SWP_NOZORDER"))        constant = SWP_NOZORDER;
		else if (!strcmp(constantName, "VK_DOWN"))             constant = VK_DOWN;
		else if (!strcmp(constantName, "VK_UP"))               constant = VK_UP;
		else if (!strcmp(constantName, "WM_CLOSE"))            constant = WM_CLOSE;
		else if (!strcmp(constantName, "WM_KEYDOWN"))          constant = WM_KEYDOWN;
		else if (!strcmp(constantName, "WS_MAXIMIZE"))         constant = WS_MAXIMIZE;
		else if (!strcmp(constantName, "WS_OVERLAPPEDWINDOW")) constant = WS_OVERLAPPEDWINDOW;
		else                                                   constant = -1;
	}

#ifndef _WIN32
#undef SW_MAXIMIZE
#undef SWP_NOOWNERZORDER
#undef WS_MAXIMIZE
#undef WS_MAXIMIZE
#undef WS_MAXIMIZEBOX
#undef WS_MINIMIZEBOX
#undef WS_OVERLAPPED
#undef WS_OVERLAPPEDWINDOW
#endif
	return constant;
}

bool BR_Win32_GetCursorPos(int* xOut, int* yOut)
{
	POINT p;
	bool result;
#ifdef _WIN32
	result = !!GetCursorPos(&p);
#else
	result = true;
	GetCursorPos(&p);
#endif

	WritePtr(xOut, (int)p.x);
	WritePtr(yOut, (int)p.y);
	return result;
}

void* BR_Win32_GetFocus()
{
	return (void*)GetFocus();
}

void* BR_Win32_GetForegroundWindow()
{
	return (void*)GetForegroundWindow();
}

void* BR_Win32_GetMainHwnd()
{
	return (void*)g_hwndParent;
}

void* BR_Win32_GetMixerHwnd(bool* isDockedOut)
{
	HWND mixerHwnd = GetMixerWnd();

	WritePtr(isDockedOut, DockIsChildOfDock(mixerHwnd, NULL) != -1);
	return (void*)mixerHwnd;
}

void BR_Win32_GetMonitorRectFromRect(bool workingAreaOnly, int leftIn, int topIn, int rightIn, int bottomIn, int* leftOut, int* topOut, int* rightOut, int* bottomOut)
{
	RECT r = { leftIn, topIn, rightIn, bottomIn };
	RECT monitorRect = { 0, 0 ,0 , 0 };

	GetMonitorRectFromRect(r, workingAreaOnly, &monitorRect);

	WritePtr(leftOut, (int)monitorRect.left);
	WritePtr(topOut, (int)monitorRect.top);
	WritePtr(rightOut, (int)monitorRect.right);
	WritePtr(bottomOut, (int)monitorRect.bottom);
}

void* BR_Win32_GetParent(void* hwnd)
{
	return (void*)GetParent((HWND)hwnd);
}

int BR_Win32_GetPrivateProfileString(const char* sectionName, const char* keyName, const char* defaultString, const char* filePath, char* stringOut, int stringOut_sz)
{
	if(!strlen(keyName))
		return 0;

	return (int)GetPrivateProfileString(sectionName, keyName, defaultString, stringOut, stringOut_sz, filePath);
}

bool BR_Win32_WritePrivateProfileString(const char* sectionName, const char* keyName, const char* value, const char* filePath)
{
	if (!strlen(keyName))
		return 0;
	if (value && !strlen(value)) // delete key if passing an empty string
		value = nullptr;

	return !!WritePrivateProfileString(sectionName, keyName, value, filePath);
}

void* BR_Win32_GetWindow(void* hwnd, int cmd)
{
	return (void*)GetWindow((HWND)hwnd, cmd);
}

int BR_Win32_GetWindowLong(void* hwnd, int index)
{
	return (int)GetWindowLong((HWND)hwnd, index);
}

bool BR_Win32_GetWindowRect(void* hwnd, int* leftOut, int* topOut, int* rightOut, int* bottomOut)
{
	RECT r;
	bool result = !!GetWindowRect((HWND)hwnd, &r);

	WritePtr(leftOut, (int)r.left);
	WritePtr(topOut, (int)r.top);
	WritePtr(rightOut, (int)r.right);
	WritePtr(bottomOut, (int)r.bottom);
	return result;
}

int BR_Win32_GetWindowText(void* hwnd, char* textOut, int textOut_sz)
{
	return GetWindowText((HWND)hwnd, textOut, textOut_sz);
}

int BR_Win32_HIBYTE(int value)
{
	return HIBYTE(value);
}

int BR_Win32_HIWORD(int value)
{
	return HIWORD(value);
}

void BR_Win32_HwndToString(void* hwnd, char* stringOut, int stringOut_sz)
{
	snprintf(stringOut, stringOut_sz, "%lld", (long long)(HWND)hwnd);
}

bool BR_Win32_IsWindow(void* hwnd)
{
	return SWS_IsWindow((HWND)hwnd);
}

bool BR_Win32_IsWindowVisible(void* hwnd)
{
	return !!IsWindowVisible((HWND)hwnd);
}

int BR_Win32_LOBYTE(int value)
{
	return LOBYTE(value);
}

int BR_Win32_LOWORD(int value)
{
	return LOWORD(value);
}

int BR_Win32_MAKELONG(int low, int high)
{
	return MAKELONG(low, high);
}

int BR_Win32_MAKELPARAM(int low, int high)
{
	return MAKELPARAM(low, high);
}

int BR_Win32_MAKELRESULT(int low, int high)
{
	return MAKELRESULT(low, high);
}

int BR_Win32_MAKEWORD(int low, int high)
{
	return MAKEWORD(low, high);
}

int BR_Win32_MAKEWPARAM(int low, int high)
{
	return MAKEWPARAM(low, high);
}

void* BR_Win32_MIDIEditor_GetActive()
{
	return (void*)MIDIEditor_GetActive();
}

void BR_Win32_ScreenToClient(void* hwnd, int xIn, int yIn, int* xOut, int* yOut)
{
	POINT p;
	p.x = xIn;
	p.y = yIn;

	ScreenToClient((HWND)hwnd, &p);
	WritePtr(xOut, (int)p.x);
	WritePtr(yOut, (int)p.y);
}

int BR_Win32_SendMessage(void* hwnd, int msg, int lParam, int wParam)
{
	return (int)SendMessage((HWND)hwnd, msg, lParam, wParam);
}

void* BR_Win32_SetFocus(void* hwnd)
{
#ifdef _WIN32
	return (void*)SetFocus((HWND)hwnd);
#else
	SetFocus((HWND)hwnd);
	return hwnd;
#endif
}

int BR_Win32_SetForegroundWindow(void* hwnd)
{
#ifdef _WIN32
	return SetForegroundWindow((HWND)hwnd);
#else
	SetForegroundWindow((HWND)hwnd);
	return !!hwnd;
#endif
}

int BR_Win32_SetWindowLong(void* hwnd, int index, int newLong)
{
	return SetWindowLong((HWND)hwnd, index, newLong);
}

bool BR_Win32_SetWindowPos(void* hwnd, const char* hwndInsertAfter, int x, int y, int width, int height, int flags)
{
	HWND insertAfter = NULL;
	if (!strcmp(hwndInsertAfter, "HWND_BOTTOM"))    insertAfter = HWND_BOTTOM;
	else if (!strcmp(hwndInsertAfter, "HWND_NOTOPMOST")) insertAfter = HWND_NOTOPMOST;
	else if (!strcmp(hwndInsertAfter, "HWND_TOP"))       insertAfter = HWND_TOP;
	else if (!strcmp(hwndInsertAfter, "HWND_TOPMOST"))   insertAfter = HWND_TOPMOST;
	else                                                 insertAfter = (HWND)BR_Win32_StringToHwnd(hwndInsertAfter);

#ifdef _WIN32
	return !!SetWindowPos((HWND)hwnd, insertAfter, x, y, width, height, flags);
#else
	SetWindowPos((HWND)hwnd, insertAfter, x, y, width, height, flags);
	return !!hwnd;
#endif
}

int BR_Win32_ShellExecute(const char* operation, const char* file, const char* parameters, const char* directoy, int showFlags)
{
	return (int)(UINT_PTR)ShellExecute(g_hwndParent, operation, file, parameters, directoy, showFlags);
}

bool BR_Win32_ShowWindow(void* hwnd, int cmdShow)
{
#ifdef _WIN32
	return !!ShowWindow((HWND)hwnd, cmdShow);
#else
	ShowWindow((HWND)hwnd, cmdShow);
	return !!hwnd;
#endif
}

void* BR_Win32_StringToHwnd(const char* string)
{
	long long hwnd = 0;
	sscanf(string, "%256lld", &hwnd);
	return (void*)(HWND)hwnd;
}

void* BR_Win32_WindowFromPoint(int x, int y)
{
	POINT p;
	p.x = x;
	p.y = y;
	return (void*)WindowFromPoint(p);
}
