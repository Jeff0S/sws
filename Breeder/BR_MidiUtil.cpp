/******************************************************************************
/ BR_MidiUtil.cpp
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

#include "BR_MidiUtil.h"
#include "BR_MouseUtil.h"
#include "BR_Util.h"
#include "../SnM/SnM.h"
#include "../SnM/SnM_Chunk.h"

#include <WDL/localize/localize.h>

/******************************************************************************
* BR_MidiEditor                                                               *
******************************************************************************/
BR_MidiEditor::BR_MidiEditor () :
m_take                 (NULL),
m_midiEditor           (MIDIEditor_GetActive()),
m_startPos             (-1),
m_hZoom                (-1),
m_vPos                 (-1),
m_vZoom                (-1),
m_noteshow             (-1),
m_timebase             (-1),
m_pianoroll            (-1),
m_drawChannel          (-1),
m_ppq                  (-1),
m_lastLane             (-666),
m_filterChannel        (-1),
m_filterEventType      (-1),
m_filterEventParamLo   (-1),
m_filterEventParamHi   (-1),
m_filterEventValLo     (-1),
m_filterEventValHi     (-1),
m_filterEventPosRepeat (-1),
m_filterEventPosLo     (-1),
m_filterEventPosHi     (-1),
m_filterEventLenLo     (-1),
m_filterEventLenHi     (-1),
m_filterEnabled        (false),
m_filterInverted       (false),
m_filterEventParam     (false),
m_filterEventVal       (false),
m_filterEventPos       (false),
m_filterEventLen       (false),
m_valid                (false)
{
	if (m_midiEditor && MIDIEditor_GetMode(m_midiEditor) != -1)
	{
		m_valid        = this->Build();
		m_lastLane     = GetLastClickedVelLane(m_midiEditor);
		m_ccLanesCount = (int)m_ccLanes.size();
	}
}

BR_MidiEditor::BR_MidiEditor (HWND midiEditor) :
m_take                 (NULL),
m_midiEditor           (midiEditor),
m_startPos             (-1),
m_hZoom                (-1),
m_vPos                 (-1),
m_vZoom                (-1),
m_noteshow             (-1),
m_timebase             (-1),
m_pianoroll            (-1),
m_drawChannel          (-1),
m_ppq                  (-1),
m_lastLane             (-666),
m_filterChannel        (-1),
m_filterEventType      (-1),
m_filterEventParamLo   (-1),
m_filterEventParamHi   (-1),
m_filterEventValLo     (-1),
m_filterEventValHi     (-1),
m_filterEventPosRepeat (-1),
m_filterEventPosLo     (-1),
m_filterEventPosHi     (-1),
m_filterEventLenLo     (-1),
m_filterEventLenHi     (-1),
m_filterEnabled        (false),
m_filterInverted       (false),
m_filterEventParam     (false),
m_filterEventVal       (false),
m_filterEventPos       (false),
m_filterEventLen       (false),
m_valid                (false)
{
	if (m_midiEditor && MIDIEditor_GetMode(m_midiEditor) != -1)
	{
		m_valid        = this->Build();
		m_lastLane     = GetLastClickedVelLane(m_midiEditor);
		m_ccLanesCount = (int)m_ccLanes.size();
	}
}

BR_MidiEditor::BR_MidiEditor (MediaItem_Take* take) :
m_take                 (take),
m_midiEditor           (NULL),
m_startPos             (-1),
m_hZoom                (-1),
m_vPos                 (-1),
m_vZoom                (-1),
m_noteshow             (-1),
m_timebase             (-1),
m_pianoroll            (-1),
m_drawChannel          (-1),
m_ppq                  (-1),
m_lastLane             (-666),
m_filterChannel        (-1),
m_filterEventType      (-1),
m_filterEventParamLo   (-1),
m_filterEventParamHi   (-1),
m_filterEventValLo     (-1),
m_filterEventValHi     (-1),
m_filterEventPosRepeat (-1),
m_filterEventPosLo     (-1),
m_filterEventPosHi     (-1),
m_filterEventLenLo     (-1),
m_filterEventLenHi     (-1),
m_filterEnabled        (false),
m_filterInverted       (false),
m_filterEventParam     (false),
m_filterEventVal       (false),
m_filterEventPos       (false),
m_filterEventLen       (false),
m_valid                (false)
{
	m_valid        = this->Build();
	m_ccLanesCount = (int)m_ccLanes.size();
}

MediaItem_Take* BR_MidiEditor::GetActiveTake ()
{
	return m_take;
}

double BR_MidiEditor::GetStartPos ()
{
	return m_startPos;
}

double BR_MidiEditor::GetHZoom ()
{
	return m_hZoom;
}

int BR_MidiEditor::GetPPQ ()
{
	return m_ppq;
}

int BR_MidiEditor::GetVPos ()
{
	return m_vPos;
}

int BR_MidiEditor::GetVZoom ()
{
	return m_vZoom;
}

int BR_MidiEditor::GetNoteshow ()
{
	return m_noteshow;
}

int BR_MidiEditor::GetTimebase ()
{
	return m_timebase;
}

int BR_MidiEditor::GetPianoRoll ()
{
	return m_pianoroll;
}

int BR_MidiEditor::GetDrawChannel ()
{
	return m_drawChannel;
}

int BR_MidiEditor::CountCCLanes ()
{
	return m_ccLanesCount;
}

int BR_MidiEditor::GetCCLane (int idx)
{
	if (idx >= 0 && idx < m_ccLanesCount)
		return m_ccLanes[idx];
	else
		return -1;
}

int BR_MidiEditor::GetCCLaneHeight (int idx)
{
	if (idx >= 0 && idx < m_ccLanesCount)
		return m_ccLanesHeight[idx];
	else
		return -1;
}

int BR_MidiEditor::GetLastClickedCCLane ()
{
	return m_lastLane;
}

int BR_MidiEditor::FindCCLane (int lane)
{
	int id = -1;
	for (size_t i = 0; i < m_ccLanes.size(); ++i)
	{
		if (m_ccLanes[i] == lane)
		{
			id = i;
			break;
		}
	}
	return id;
}

int BR_MidiEditor::GetCCLanesFullheight (bool keyboardView)
{
	int fullHeight = 0;
	for (int i = 0; i < m_ccLanesCount; ++i)
		fullHeight += m_ccLanesHeight[i];

	if (m_midiEditor && keyboardView)
		fullHeight += SCROLLBAR_W - 3;   // cc lane selector is not completely aligned with CC lane

	return fullHeight;
}

bool BR_MidiEditor::IsCCLaneVisible (int lane)
{
	for (size_t i = 0; i < m_ccLanes.size(); ++i)
	{
		if (m_ccLanes[i] == lane)
			return true;
	}
	return false;
}

bool BR_MidiEditor::SetCCLaneLastClicked (int idx)
{
	bool success = false;
	if (m_midiEditor && idx >= 0 && idx < (int)m_ccLanes.size() && m_ccLanes.size() > 0)
	{
		if (HWND keyboardWnd = GetPianoView(m_midiEditor))
		{
			RECT r; GetClientRect(keyboardWnd, &r);
			int ccStart = abs(r.bottom - r.top) - this->GetCCLanesFullheight(true) + 1;

			for (int i = 0; i < idx; ++i)
				ccStart += m_ccLanesHeight[i];

			POINT point;
			point.y = ccStart + MIDI_CC_LANE_CLICK_Y_OFFSET;
			point.x = 0;
			SimulateMouseClick(keyboardWnd, point, true);
			success = true;
		}
	}
	return success;
}

bool BR_MidiEditor::IsNoteVisible (MediaItem_Take* take, int id)
{
	bool visible = false;
	if (take)
	{
		if (!m_filterEnabled)
		{
			visible = true;
		}
		else
		{
			double start, end;
			int channel, velocity, pitch;
			if (MIDI_GetNote(take, id, NULL, NULL, &start, &end, &channel, &pitch, &velocity))
				visible = this->CheckVisibility(take, STATUS_NOTE_ON, start, end, channel, pitch, velocity);
		}
	}

	return visible;
}

bool BR_MidiEditor::IsCCVisible (MediaItem_Take* take, int id)
{
	bool visible = false;
	if (take)
	{
		if (!m_filterEnabled)
		{
			visible = true;
		}
		else
		{
			double position;
			int chanMsg, channel, msg2, msg3;
			if (MIDI_GetCC(take, id, NULL, NULL, &position, &chanMsg, &channel, &msg2, &msg3))
				visible = this->CheckVisibility(take, chanMsg, position, 0, channel, msg2, msg3);
		}
	}

	return visible;
}

bool BR_MidiEditor::IsSysVisible (MediaItem_Take* take, int id)
{
	bool visible = false;

	if (take)
	{
		if (!m_filterEnabled)
		{
			visible = true;
		}
		else
		{
			double position;
			if (MIDI_GetTextSysexEvt(take, id, NULL, NULL, &position, NULL, NULL, NULL))
				visible = this->CheckVisibility(take, STATUS_SYS, position, 0, 0, 0, 0);
		}
	}

	return visible;
}

bool BR_MidiEditor::IsChannelVisible (int channel)
{
	if (m_filterEnabled)
		return !!GetBit(((m_filterInverted) ? (~m_filterChannel) : (m_filterChannel)), channel);
	else
		return true;
}

HWND BR_MidiEditor::GetEditor ()
{
	return m_midiEditor;
}

bool BR_MidiEditor::IsValid ()
{
	return m_valid;
}

bool BR_MidiEditor::CheckVisibility (MediaItem_Take* take, int chanMsg, double position, double end, int channel, int param, int value)
{
	bool channelVis   = (chanMsg == STATUS_SYS) ? true : !!GetBit(m_filterChannel, channel);
	bool eventTypeVis = (chanMsg == m_filterEventType || m_filterEventType == -1);

	if (eventTypeVis)
	{
		if (chanMsg == STATUS_PITCH)
			value =  (value << 7) | param;

		bool paramVis = (!m_filterEventParam ? true : CheckBounds(param,           m_filterEventParamLo, m_filterEventParamHi));
		bool valVis   = (!m_filterEventVal   ? true : CheckBounds(value,           m_filterEventValLo,   m_filterEventValHi));
		bool lenVis   = (!m_filterEventLen   ? true : CheckBounds(end - position,  m_filterEventLenLo,   m_filterEventLenHi));
		bool posVis   = (!m_filterEventPos   ? true : false);

		if (!posVis)
		{
			double measureEnd   = MIDI_GetPPQPos_EndOfMeasure(take, position);
			double measureStart = MIDI_GetPPQPos_StartOfMeasure(take, measureEnd);

			/* Shrink measureStart and measureEnd to obey repeat rate */
			if (m_filterEventPosRepeat != 0)
			{
				double tmp = measureStart + (m_filterEventPosRepeat * TruncToInt((position - measureStart) / m_filterEventPosRepeat));
				if (tmp < measureEnd)
					measureStart = tmp;

				tmp = measureStart + m_filterEventPosRepeat;
				if (tmp < measureEnd)
					measureEnd = tmp;
			}
			double positionStart = measureStart + m_filterEventPosLo; if (positionStart > measureEnd) positionStart = measureEnd;
			double positionEnd   = measureStart + m_filterEventPosHi; if (positionEnd   > measureEnd) positionEnd   = measureEnd;

			posVis = (position < positionStart) ? false : ((position >= positionEnd) ? false : true);
		}

		eventTypeVis = paramVis && valVis && lenVis && posVis;
	}

	bool visible = (chanMsg == STATUS_SYS) ? (eventTypeVis) : (channelVis && eventTypeVis);
	return ((m_filterInverted) != visible);

}

bool BR_MidiEditor::Build ()
{
	m_take = (m_midiEditor) ? MIDIEditor_GetTake(m_midiEditor) : m_take;

	if (m_take)
	{
		MediaItem* item = GetMediaItemTake_Item(m_take);
		int takeId = GetTakeId(m_take, item);
		if (takeId >= 0)
		{
			SNM_TakeParserPatcher p(item, CountTakes(item));
			WDL_FastString takeChunk;
			if (p.GetTakeChunk(takeId, &takeChunk))
			{
				SNM_ChunkParserPatcher ptk(&takeChunk, false);
				LineParser lp(false);

				int laneId = 0;
				WDL_FastString lineLane;
				while (ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "VELLANE", laneId++, -1, &lineLane))
				{
					lp.parse(lineLane.Get());
					m_ccLanes.push_back(lp.gettoken_int(1));
					m_ccLanesHeight.push_back(lp.gettoken_int(((m_midiEditor) ? 2 : 3)));
					if (!m_midiEditor && m_ccLanesHeight.back() == 0)
						m_ccLanesHeight.back() = INLINE_MIDI_LANE_DIVIDER_H; // sometimes REAPER will return 0 when lane is completely hidden, but divider will still be visible
					lineLane.DeleteSub(0, lineLane.GetLength());
				}

				WDL_FastString dataLine;
				if (ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "HASDATA", 0, -1, &dataLine))
				{
					lp.parse(dataLine.Get());
					m_ppq = lp.gettoken_int(2);
				}
				else if (ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "FILE", 0, -1, &dataLine))
				{
					lp.parse(dataLine.Get());
					m_ppq = GetMIDIFilePPQ (lp.gettoken_str(1));
					if (!m_ppq)
						return false;
				}
				else
					return false;

				WDL_FastString lineView;
				if (ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "CFGEDITVIEW", 0, -1, &lineView))
				{
					lp.parse(lineView.Get());
					m_startPos = (m_midiEditor) ? lp.gettoken_float(1) : GetMediaItemInfo_Value(GetMediaItemTake_Item(m_take), "D_POSITION");
					m_hZoom    = (m_midiEditor) ? lp.gettoken_float(2) : GetHZoomLevel();
					m_vPos     = (m_midiEditor) ? lp.gettoken_int(3) : lp.gettoken_int(7);
					m_vZoom    = (m_midiEditor) ? lp.gettoken_int(4) : lp.gettoken_int(6);
				}
				else
					return false;

				WDL_FastString lineFilter;
				if (ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "EVTFILTER", 0, -1, &lineFilter))
				{
					lp.parse(lineFilter.Get());
					m_filterEnabled        = !!GetBit(lp.gettoken_int(7), 0);
					m_filterInverted       = !!GetBit(lp.gettoken_int(7), 2);
					m_filterChannel        = lp.gettoken_int(1);
					m_filterEventType      = lp.gettoken_int(2);
					m_filterEventParam     = !!lp.gettoken_int(16);
					m_filterEventVal       = !!lp.gettoken_int(8);
					m_filterEventPos       = !!lp.gettoken_int(14);
					m_filterEventLen       = !!lp.gettoken_int(9);
					m_filterEventParamLo   = lp.gettoken_int(17);
					m_filterEventParamHi   = lp.gettoken_int(18);
					m_filterEventValLo     = lp.gettoken_int(4);
					m_filterEventValHi     = lp.gettoken_int(5);
					m_filterEventPosRepeat = lp.gettoken_float(15);
					m_filterEventPosLo     = lp.gettoken_float(12);
					m_filterEventPosHi     = lp.gettoken_float(13);
					m_filterEventLenLo     = lp.gettoken_float(10);
					m_filterEventLenHi     = lp.gettoken_float(11);
				}
				else
					return false;

				WDL_FastString lineProp;
				if (ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "CFGEDIT", 0, -1, &lineProp))
				{
					lp.parse(lineProp.Get());
					m_pianoroll    = (m_midiEditor) ? lp.gettoken_int(6) : 0; // inline midi editor doesn't have piano roll modes
					m_drawChannel  = lp.gettoken_int(9) - 1;
					m_noteshow     = lp.gettoken_int(18);
					m_timebase     = (m_midiEditor) ? lp.gettoken_int(19) : PROJECT_SYNC;
				}
				else
					return false;

				// A few "corrections" for easier manipulation afterwards
				if (m_filterChannel == 0)     m_filterChannel = ~m_filterChannel;
				if (m_filterEventParamLo < 0) m_filterEventParamLo = 0;
				if (m_filterEventParamHi < 0) m_filterEventParamHi = INT_MAX;
				if (m_filterEventValLo   < 0) m_filterEventValLo   = 0;
				if (m_filterEventValHi   < 0) m_filterEventValHi   = INT_MAX;
				if (m_filterEventPosLo   < 0) m_filterEventPosLo   = 0;
				if (m_filterEventPosHi   < 0) m_filterEventPosHi   = INT_MAX;
				m_filterEventLenLo     = (m_filterEventLenLo     < 0) ? (0)       : (m_ppq * 4 * m_filterEventLenLo);
				m_filterEventLenHi     = (m_filterEventLenHi     < 0) ? (INT_MAX) : (m_ppq * 4 * m_filterEventLenHi);
				m_filterEventPosLo     = (m_filterEventPosLo     < 0) ? (0)       : (m_ppq * 4 * m_filterEventPosLo);
				m_filterEventPosHi     = (m_filterEventPosHi     < 0) ? (INT_MAX) : (m_ppq * 4 * m_filterEventPosHi);
				m_filterEventPosRepeat = (m_filterEventPosRepeat < 0) ? (0)       : (m_ppq * 4 * m_filterEventPosRepeat);

				return true;
			}
		}
	}
	return false;
}

/******************************************************************************
* BR_MidiItemTimePos                                                          *
******************************************************************************/
BR_MidiItemTimePos::BR_MidiItemTimePos (MediaItem* item) :
item         (item),
position     (GetMediaItemInfo_Value(item, "D_POSITION")),
length       (GetMediaItemInfo_Value(item, "D_LENGTH")),
timeBase     (GetMediaItemInfo_Value(item, "C_BEATATTACHMODE")),
looped       (!!GetMediaItemInfo_Value(item, "B_LOOPSRC")),
loopStart    (-1),
loopEnd      (-1),
loopedOffset (0)
{
	// When restoring position and lenght, the only way to restore it for looped MIDI items is to use MIDI_SetItemExtents which will disable looping
	// for the whole item. Since we have no idea what will happen before this->Restore() is called take data for active MIDI item right now instead of
	// in this->Restore() (for example, if client calls SetIgnoreTempo() from BR_Util.h before restoring to disable "ignore project tempo" length of MIDI item source may change)
	if (looped)
	{
		if (MediaItem_Take* activeTake = GetActiveTake(item))
		{
			double sourceLenPPQ = GetMidiSourceLengthPPQ(activeTake, true);
			loopStart = MIDI_GetProjTimeFromPPQPos(activeTake, sourceLenPPQ); // we're not using MIDI_GetProjTimeFromPPQPos(activeTake, 0) because later we will use MIDI_SetItemExtents to make sure looped item
			loopEnd = MIDI_GetProjTimeFromPPQPos(activeTake, sourceLenPPQ*2); // position info is restored and 0 PPQ in take could theoretically be behind project start in which case MIDI_SetItemExtents wont' work properly
			loopedOffset = GetMediaItemTakeInfo_Value(activeTake, "D_STARTOFFS");
		}
	}

	int takeCount = CountTakes(item);
	for (int i = 0; i < takeCount; ++i)
	{
		MediaItem_Take* take = GetTake(item, i);

		int noteCount, ccCount, textCount;
		int midiEventCount = MIDI_CountEvts(take, &noteCount, &ccCount, &textCount);

		// In case of looped item, if active take wasn't midi, get looped position here for first MIDI take
		if (looped && loopStart == -1 && loopEnd == -1 && IsMidi(take, NULL) && (midiEventCount > 0 || i == takeCount - 1))
		{
			double sourceLenPPQ = GetMidiSourceLengthPPQ(take, true);
			loopStart = MIDI_GetProjTimeFromPPQPos(take, sourceLenPPQ);
			loopEnd = MIDI_GetProjTimeFromPPQPos(take, sourceLenPPQ*2);
			loopedOffset = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");
		}


		if (midiEventCount > 0)
		{
			savedMidiTakes.push_back(BR_MidiItemTimePos::MidiTake(take, noteCount, ccCount, textCount));
			BR_MidiItemTimePos::MidiTake* midiTake = &savedMidiTakes.back();

			for (int i = 0; i < noteCount; ++i)
				midiTake->noteEvents.push_back(BR_MidiItemTimePos::MidiTake::NoteEvent(take, i));

			for (int i = 0; i < ccCount; ++i)
				midiTake->ccEvents.push_back(BR_MidiItemTimePos::MidiTake::CCEvent(take, i));

			for (int i = 0; i < textCount; ++i)
				midiTake->sysEvents.push_back(BR_MidiItemTimePos::MidiTake::SysEvent(take, i));
		}
	}
}

void BR_MidiItemTimePos::Restore (double timeOffset /*=0*/)
{
	SetMediaItemInfo_Value(item, "C_BEATATTACHMODE", 0);
	for (size_t i = 0; i < savedMidiTakes.size(); ++i)
	{
		BR_MidiItemTimePos::MidiTake* midiTake = &savedMidiTakes[i];
		MediaItem_Take* take = midiTake->take;

		int noteCount, ccCount, textCount;
		if (MIDI_CountEvts(take, &noteCount, &ccCount, &textCount))
		{
			for (int i = 0; i < noteCount; ++i) MIDI_DeleteNote(take, 0);
			for (int i = 0; i < ccCount;   ++i) MIDI_DeleteCC(take, 0);
			for (int i = 0; i < textCount; ++i) MIDI_DeleteTextSysexEvt(take, 0);
		}

		if (looped && loopStart != -1 && loopEnd != -1)
		{
			SetMediaItemTakeInfo_Value(take, "D_STARTOFFS", 0);
			MIDI_SetItemExtents(item, TimeMap_timeToQN(loopStart), TimeMap_timeToQN(loopEnd));

			SetMediaItemInfo_Value(item , "B_LOOPSRC", 1); // because MIDI_SetItemExtents() disables looping
			TrimItem(item, position, position + length, false, true); // NF: unsure if takes env's should be adjusted here
			SetMediaItemTakeInfo_Value(take, "D_STARTOFFS", loopedOffset);
		}
		else
		{
			TrimItem(item, position, position + length, true, true);
		}

		for (size_t i = 0; i < midiTake->noteEvents.size(); ++i)
			midiTake->noteEvents[i].InsertEvent(take, timeOffset);

		for (size_t i = 0; i < midiTake->ccEvents.size(); ++i)
			midiTake->ccEvents[i].InsertEvent(take, timeOffset);

		for (size_t i = 0; i < midiTake->sysEvents.size(); ++i)
			midiTake->sysEvents[i].InsertEvent(take, timeOffset);
	}

	SetMediaItemInfo_Value(item, "C_BEATATTACHMODE", timeBase);
}

BR_MidiItemTimePos::MidiTake::NoteEvent::NoteEvent (MediaItem_Take* take, int id)
{
	MIDI_GetNote(take, id, &selected, &muted, &pos, &end, &chan, &pitch, &vel);
	pos = MIDI_GetProjTimeFromPPQPos(take, pos);
	end = MIDI_GetProjTimeFromPPQPos(take, end);
}

void BR_MidiItemTimePos::MidiTake::NoteEvent::InsertEvent (MediaItem_Take* take, double offset)
{
	double posPPQ = MIDI_GetPPQPosFromProjTime(take, pos + offset);
	double endPPQ = MIDI_GetPPQPosFromProjTime(take, end + offset);
	MIDI_InsertNote(take, selected, muted, posPPQ, endPPQ, chan, pitch, vel, NULL);
}

BR_MidiItemTimePos::MidiTake::CCEvent::CCEvent (MediaItem_Take* take, int id)
{
	MIDI_GetCC(take, id, &selected, &muted, &pos, &chanMsg, &chan, &msg2, &msg3);
	if (MIDI_GetCCShape) // v6.0+
		MIDI_GetCCShape(take, id, &shape, &beztension);
	pos = MIDI_GetProjTimeFromPPQPos(take, pos);
}

void BR_MidiItemTimePos::MidiTake::CCEvent::InsertEvent (MediaItem_Take* take, double offset)
{
	const double posPPQ = MIDI_GetPPQPosFromProjTime(take, pos + offset);
	const bool ok = MIDI_InsertCC(take, selected, muted, posPPQ, chanMsg, chan, msg2, msg3);
	if (ok && MIDI_SetCCShape /* v6.0 */ && (shape || beztension)) {
		int ccCount;
		MIDI_CountEvts(take, nullptr, &ccCount, nullptr);
		MIDI_SetCCShape(take, ccCount - 1, shape, beztension, nullptr);
	}
}

BR_MidiItemTimePos::MidiTake::SysEvent::SysEvent (MediaItem_Take* take, int id)
{
	msg.Resize(32768);
	msg_sz = msg.GetSize();
	MIDI_GetTextSysexEvt(take, id, &selected, &muted, &pos, &type, msg.Get(), &msg_sz);
	pos = MIDI_GetProjTimeFromPPQPos(take, pos);
}

void BR_MidiItemTimePos::MidiTake::SysEvent::InsertEvent (MediaItem_Take* take, double offset)
{
	double posPPQ = MIDI_GetPPQPosFromProjTime(take, pos + offset);
	MIDI_InsertTextSysexEvt(take, selected, muted, posPPQ, type, msg.Get(), msg_sz);
}

BR_MidiItemTimePos::MidiTake::MidiTake (MediaItem_Take* take, int noteCount /*=0*/, int ccCount /*=0*/, int sysCount /*=0*/) :
take (take)
{
	noteEvents.reserve(noteCount);
	ccEvents.reserve(ccCount),
	sysEvents.reserve(sysCount);
}

/******************************************************************************
* Mouse cursor                                                                *
******************************************************************************/
double ME_PositionAtMouseCursor (bool checkRuler, bool checkCCLanes)
{
	BR_MouseInfo mouseInfo(BR_MouseInfo::MODE_MIDI_EDITOR_ALL);
	if (mouseInfo.GetMidiEditor())
	{
		if (checkRuler && checkCCLanes)                                 return mouseInfo.GetPosition(); // no need for strcmp, if position is invalid it's already -1
		if (!strcmp(mouseInfo.GetSegment(),"notes_view"))               return mouseInfo.GetPosition();
		if (checkRuler   && !strcmp(mouseInfo.GetSegment(), "ruler"))	return mouseInfo.GetPosition();
		if (checkCCLanes && !strcmp(mouseInfo.GetSegment(), "cc_lane")) return mouseInfo.GetPosition();
	}
	return -1;
}

/******************************************************************************
* Miscellaneous                                                               *
******************************************************************************/
vector<int> GetUsedNamedNotes (HWND midiEditor, MediaItem_Take* take, bool used, bool named, int channelForNames)
{
	/* Not really reliable, user could have changed default draw channel  *
	*  but without resetting note view settings, view won't get updated   */

	vector<bool> allNotesStatus(128, false);
	MediaItem_Take* midiTake = midiEditor ? MIDIEditor_GetTake(midiEditor) : take;

	if (named)
	{
		MediaTrack* track = GetMediaItemTake_Track(midiTake);
		for (size_t i = 0; i < allNotesStatus.size(); ++i)
			if (GetTrackMIDINoteNameEx(NULL, track, static_cast<int>(i), channelForNames))
				allNotesStatus[i] = true;
	}

	if (used)
	{
		int noteCount;
		if (MIDI_CountEvts(midiTake, &noteCount, NULL, NULL))
		{
			for (int i = 0; i < noteCount; ++i)
			{
				int pitch;
				MIDI_GetNote(midiTake, i, NULL, NULL, NULL, NULL, NULL, &pitch, NULL);
				allNotesStatus[pitch] = true;
			}
		}
	}

	vector<int> notes;
	notes.reserve(allNotesStatus.size());
	for (size_t i = 0; i < allNotesStatus.size(); ++i)
	{
		if (allNotesStatus[i])
			notes.push_back(i);
	}

	return notes;
}

vector<int> GetSelectedNotes (MediaItem_Take* take)
{
	vector<int> selectedNotes;

	int noteCount;
	MIDI_CountEvts(take, &noteCount, NULL, NULL);
	for (int i = 0; i < noteCount; ++i)
	{
		bool selected = false;
		MIDI_GetNote(take, i, &selected, NULL, NULL, NULL, NULL, NULL, NULL);
		if (selected)
			selectedNotes.push_back(i);
	}
	return selectedNotes;
}

vector<int> MuteUnselectedNotes (MediaItem_Take* take)
{
	vector<int> muteStatus;

	int noteCount;
	if (MIDI_CountEvts(take, &noteCount, NULL, NULL))
	{
		for (int i = 0; i < noteCount; ++i)
		{
			bool selected, muted;
			MIDI_GetNote(take, i, &selected, &muted, NULL, NULL, NULL, NULL, NULL);
			muteStatus.push_back((int)muted);
			if (!selected)
				MIDI_SetNote(take, i, NULL, &g_bTrue, NULL, NULL, NULL, NULL, NULL, NULL);
		}
	}
	return muteStatus;
}

set<int> GetUsedCCLanes (HWND midiEditor, int detect14bit, bool selectedEventsOnly)
{
	MediaItem_Take* take = MIDIEditor_GetTake(midiEditor);
	set<int> usedCC;

	int noteCount, ccCount, sysCount;
	if (take && MIDI_CountEvts(take, &noteCount, &ccCount, &sysCount))
	{
		BR_MidiEditor editor(midiEditor);

		set<int> unpairedMSB;
		for (int id = 0; id < ccCount; ++id)
		{
			int chanMsg, chan, msg2;
			bool selected;
			if (!MIDI_GetCC(take, id, &selected, NULL, NULL, &chanMsg, &chan, &msg2, NULL) || !editor.IsCCVisible(take, id) || (selectedEventsOnly && !selected))
				continue;

			if      (chanMsg == STATUS_PROGRAM)
			{
				usedCC.insert(CC_PROGRAM);
				usedCC.insert(CC_BANK_SELECT);
			}
			else if (chanMsg == STATUS_CHANNEL_PRESSURE) usedCC.insert(CC_CHANNEL_PRESSURE);
			else if (chanMsg == STATUS_PITCH)            usedCC.insert(CC_PITCH);
			else if (chanMsg == STATUS_CC)
			{
				if (msg2 > 63 || detect14bit == 0)
					usedCC.insert(msg2);
				else
				{
					if (detect14bit == 1)
						usedCC.insert(msg2);

					// If MSB also check for LSB that makes up 14 bit event
					if (msg2 <= 31)
					{
						int tmpId = id;
						if (MIDI_GetCC(take, tmpId + 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
						{
							double pos;
							MIDI_GetCC(take, id, NULL, NULL, &pos, NULL, NULL, NULL, NULL);

							while (true)
							{
								double nextPos;
								int nextChanMsg, nextChan, nextMsg2;
								MIDI_GetCC(take, ++tmpId, NULL, NULL, &nextPos, &nextChanMsg, &nextChan, &nextMsg2, NULL);
								if (tmpId >= ccCount)
									break;
								if (nextPos > pos)
								{
									if (detect14bit == 2)
									{
										usedCC.insert(msg2);
										unpairedMSB.insert(msg2);
									}
									break;
								}

								if (nextChanMsg == STATUS_CC && msg2 == nextMsg2 - 32 && chan == nextChan)
								{
									usedCC.insert(msg2 + CC_14BIT_START);
									break;
								}
							}
						}
						else
						{
							if (detect14bit == 2)
							{
								usedCC.insert(msg2);
								unpairedMSB.insert(msg2);
							}
						}
					}
					// If LSB, just make sure it was paired
					else if (detect14bit == 2)
					{
						int tmpId = id;
						if (MIDI_GetCC(take, tmpId - 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
						{
							double pos;
							MIDI_GetCC(take, id, NULL, NULL, &pos, NULL, NULL, NULL, NULL);

							while (true)
							{
								double prevPos;
								int prevChanMsg, prevChan, prevMsg2;
								MIDI_GetCC(take, --tmpId, NULL, NULL, &prevPos, &prevChanMsg, &prevChan, &prevMsg2, NULL);
								if (prevPos < pos)
								{
									if (detect14bit == 2)
										usedCC.insert(msg2);
									break;
								}

								if (prevChanMsg == STATUS_CC && msg2 == prevMsg2 + 32 && chan == prevChan)
									break;
							}
						}
						else
							usedCC.insert(msg2);
					}
				}
			}
		}

		if (detect14bit == 2)
		{
			for (set<int>::iterator it = unpairedMSB.begin(); it != unpairedMSB.end(); ++it)
			{
				if (usedCC.find(*it + CC_14BIT_START) != usedCC.end())
				{
					usedCC.erase(*it + CC_14BIT_START);
					usedCC.insert(*it + 32);            // MSB is already there, LSB doesn't have to be so add it
				}
			}
		}

		for (int i = 0; i < noteCount; ++i)
		{
			bool selected; int chan;
			MIDI_GetNote(take, i, &selected, NULL, NULL, NULL, &chan, NULL, NULL);
			if (editor.IsChannelVisible(chan) && (!selectedEventsOnly || selected))
			{
				usedCC.insert(-1);
				break;
			}
		}

		bool foundText = false, foundSys = false;
		for (int i = 0; i < sysCount; ++i)
		{
			bool selected;
			int type = 0;
			MIDI_GetTextSysexEvt(take, i, &selected, NULL, NULL, &type, NULL, NULL);

			if (type == -1)
			{
				if (!foundSys && (!selectedEventsOnly || selected))
				{
					usedCC.insert(CC_SYSEX);
					foundSys = true;
					if (foundText)
						break;
				}
			}
			else
			{
				if (!foundText && (!selectedEventsOnly || selected))
				{
					usedCC.insert(CC_TEXT_EVENTS);
					foundText = true;
					if (foundSys)
						break;
				}
			}
		}
	}

	return usedCC;
}

double EffectiveMidiTakeStart (MediaItem_Take* take, bool ignoreMutedEvents, bool ignoreTextEvents, bool ignoreEventsOutsideItemBoundaries)
{
	int noteCount, ccCount, sysCount;
	if (take && MIDI_CountEvts(take, &noteCount, &ccCount, &sysCount))
	{
		MediaItem* item = GetMediaItemTake_Item(take);
		double itemStart = GetMediaItemInfo_Value(item, "D_POSITION");

		double itemStartPPQ = MIDI_GetPPQPosFromProjTime(take, itemStart);
		double itemEndPPQ   = (!ignoreEventsOutsideItemBoundaries) ? 0 : MIDI_GetPPQPosFromProjTime(take, GetMediaItemInfo_Value(item, "D_LENGTH") + itemStart);
		int    loopCount    = (!ignoreEventsOutsideItemBoundaries) ? 0 : GetLoopCount(take, 0, NULL);
		double sourceLenPPQ = (!ignoreEventsOutsideItemBoundaries) ? 0 : GetMidiSourceLengthPPQ(take, true);

		bool validNote = false, validCC = false, validSys = false;
		double noteStart, ccStart, sysStart;

		double loopOffset = 0;
		for (int i = 0; i < noteCount; ++i)
		{
			bool muted; double start, end;
			MIDI_GetNote(take, i, NULL, &muted, &start, &end, NULL, NULL, NULL);
			if ((ignoreMutedEvents && !muted) || !ignoreMutedEvents)
			{
				if (!ignoreEventsOutsideItemBoundaries)
				{
					noteStart = start;
					validNote = true;
					break;
				}
				else
				{
					start += loopOffset;
					end   += loopOffset;

					if (AreOverlapped(start, end, itemStartPPQ, itemEndPPQ))
					{
						if (itemStartPPQ > start) start = itemStartPPQ;
						noteStart = start;
						validNote = true;
						break;
					}
				}
			}

			if (ignoreEventsOutsideItemBoundaries && loopCount > 0 && i == noteCount - 1)
			{
				if (sourceLenPPQ > 0 && loopOffset == 0)
				{
					loopOffset = sourceLenPPQ;
					i = -1;
				}
			}
		}

		loopOffset = 0;
		for (int i = 0; i < ccCount; ++i)
		{
			bool muted; double pos;
			MIDI_GetCC(take, i, NULL, &muted, &pos, NULL, NULL, NULL, NULL);
			if ((ignoreMutedEvents && !muted) || !ignoreMutedEvents)
			{
				if (!ignoreEventsOutsideItemBoundaries)
				{
					ccStart = pos;
					validCC = true;
					break;
				}
				else
				{
					pos += loopOffset;

					if (CheckBounds(pos, itemStartPPQ, itemEndPPQ))
					{
						ccStart = pos;
						validCC = true;
						break;
					}
				}
			}

			if (ignoreEventsOutsideItemBoundaries && loopCount > 0 && i == ccCount - 1)
			{
				if (sourceLenPPQ > 0 && loopOffset == 0)
				{
					loopOffset = sourceLenPPQ;
					i = -1;
				}
			}
		}

		for (int i = 0; i < sysCount; ++i)
		{
			bool muted; double pos; int type;
			MIDI_GetTextSysexEvt(take, i, NULL, &muted, &pos, &type, NULL, NULL);
			if (((ignoreMutedEvents && !muted) || !ignoreMutedEvents) && ((ignoreTextEvents && type == -1) || !ignoreTextEvents))
			{
				if (!ignoreEventsOutsideItemBoundaries)
				{
					sysStart = pos;
					validSys = true;
					break;
				}
				else
				{
					pos += loopOffset;

					if (CheckBounds(pos, itemStartPPQ, itemEndPPQ))
					{
						sysStart = pos;
						validSys = true;
						break;
					}
				}
			}

			if (ignoreEventsOutsideItemBoundaries && loopCount > 0 && i == sysCount - 1)
			{
				if (sourceLenPPQ > 0 && loopOffset == 0)
				{
					loopOffset = sourceLenPPQ;
					i = -1;
				}
			}
		}

		if (validNote || validCC || validSys)
		{
			if (!validNote) noteStart = (validSys) ? sysStart : ccStart;
			if (!validCC)   ccStart   = (validSys) ? sysStart : noteStart;
			if (!validSys)  sysStart  = (ccStart)  ? ccStart  : noteStart;
			return MIDI_GetProjTimeFromPPQPos(take, min(min(noteStart, ccStart), sysStart));
		}
		else
		{
			return GetMediaItemInfo_Value(item, "D_POSITION");
		}
	}
	else
	{
		return GetMediaItemInfo_Value(GetMediaItemTake_Item(take), "D_POSITION");
	}
}

double EffectiveMidiTakeEnd (MediaItem_Take* take, bool ignoreMutedEvents, bool ignoreTextEvents, bool ignoreEventsOutsideItemBoundaries)
{
	int noteCount, ccCount, sysCount;
	if (take && MIDI_CountEvts(take, &noteCount, &ccCount, &sysCount))
	{
		MediaItem* item = GetMediaItemTake_Item(take);
		double itemStart = GetMediaItemInfo_Value(item, "D_POSITION");
		double itemStartPPQ = MIDI_GetPPQPosFromProjTime(take, itemStart);
		double itemEndPPQ   = (!ignoreEventsOutsideItemBoundaries) ? 0 : MIDI_GetPPQPosFromProjTime(take, GetMediaItemInfo_Value(item, "D_LENGTH") + itemStart);

		int    loopCount     = GetLoopCount(take, 0, NULL);
		double sourceLenPPQ  = GetMidiSourceLengthPPQ(take, true);
		double effectiveTakeEndPPQ = -1;

		for (int i = 0; i < noteCount; ++i)
		{
			bool muted; double noteStart, noteEnd;
			MIDI_GetNote(take, i, NULL, &muted, &noteStart, &noteEnd, NULL, NULL, NULL);
			if (((ignoreMutedEvents && !muted) || !ignoreMutedEvents))
			{
				noteEnd += loopCount*sourceLenPPQ;

				if (!ignoreEventsOutsideItemBoundaries)
				{
					if (noteEnd > effectiveTakeEndPPQ)
						effectiveTakeEndPPQ = noteEnd;
				}
				else
				{
					noteStart += loopCount*sourceLenPPQ;

					if (CheckBounds(noteStart, itemStartPPQ, itemEndPPQ))
					{
						if (noteEnd > itemEndPPQ) noteEnd = itemEndPPQ;
						if (noteEnd > effectiveTakeEndPPQ)
							effectiveTakeEndPPQ = noteEnd;
					}
					else if (loopCount > 0)
					{
						noteStart -= sourceLenPPQ;
						if (CheckBounds(noteStart, itemStartPPQ, itemEndPPQ))
						{
							noteEnd -= sourceLenPPQ;
							if (noteEnd > itemEndPPQ) noteEnd = itemEndPPQ;
							if (noteEnd > effectiveTakeEndPPQ)
								effectiveTakeEndPPQ = noteEnd;
						}
					}
				}
			}
		}

		for (int i = ccCount - 1; i >= 0; --i)
		{
			bool muted; double pos;
			MIDI_GetCC(take, i, NULL, &muted, &pos, NULL, NULL, NULL, NULL);
			if ((ignoreMutedEvents && !muted) || !ignoreMutedEvents)
			{
				pos += loopCount*sourceLenPPQ;

				if (!ignoreEventsOutsideItemBoundaries)
				{
					if (pos > effectiveTakeEndPPQ)
						effectiveTakeEndPPQ = pos + 1; // add 1 ppq so length definitely includes that last event
					break;
				}
				else
				{
					if (CheckBounds(pos, itemStartPPQ, itemEndPPQ))
					{
						if (pos > effectiveTakeEndPPQ)
						{
							effectiveTakeEndPPQ = pos + 1;
							break;
						}
					}
					else if (loopCount > 0)
					{
						pos -= sourceLenPPQ;
						if (CheckBounds(pos, itemStartPPQ, itemEndPPQ))
						{
							if (pos > effectiveTakeEndPPQ)
							{
								effectiveTakeEndPPQ = pos + 1;
								break;
							}
						}
					}
				}
			}
		}

		for (int i = 0; i < sysCount; ++i)
		{
			bool muted; double pos; int type;
			MIDI_GetTextSysexEvt(take, i, NULL, &muted, &pos, &type, NULL, NULL);
			if (((ignoreMutedEvents && !muted) || !ignoreMutedEvents) && ((ignoreTextEvents && type == -1) || !ignoreTextEvents))
			{
				pos += loopCount*sourceLenPPQ;
				if (!ignoreEventsOutsideItemBoundaries)
				{
					if (pos > effectiveTakeEndPPQ)
						effectiveTakeEndPPQ = pos + 1; // add 1 ppq so length definitely includes that last event
				}
				else
				{
					if (CheckBounds(pos, itemStartPPQ, itemEndPPQ))
					{
						if (pos > effectiveTakeEndPPQ)
							effectiveTakeEndPPQ = pos + 1;
					}
					else if (loopCount > 0)
					{
						pos -= sourceLenPPQ;
						if (CheckBounds(pos, itemStartPPQ, itemEndPPQ))
						{
							if (pos > effectiveTakeEndPPQ)
								effectiveTakeEndPPQ = pos + 1;
						}
					}
				}
			}
		}

		return (effectiveTakeEndPPQ == -1) ? itemStart : MIDI_GetProjTimeFromPPQPos(take, effectiveTakeEndPPQ);
	}
	return 0;
}

double GetMidiSourceLengthPPQ (MediaItem_Take* take, bool accountPlayrateIfIgnoringProjTempo, bool* isMidiSource /*=NULL*/)
{
	bool   isMidi = false;
	double length = 0;
	if (take && IsMidi(take))
	{
		MediaItem* item = GetMediaItemTake_Item(take);
		double itemStart    = GetMediaItemInfo_Value(item, "D_POSITION");
		double takeOffset   = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");
		double sourceLength = GetMediaItemTake_Source(take)->GetLength();
		double startPPQ = MIDI_GetPPQPosFromProjTime(take, itemStart - takeOffset);
		double endPPQ   = MIDI_GetPPQPosFromProjTime(take, itemStart - takeOffset + sourceLength);

		isMidi = true;
		length = endPPQ - startPPQ;

		if (accountPlayrateIfIgnoringProjTempo)
		{
			bool ignoreProjTempo;
			if (GetMidiTakeTempoInfo(take, &ignoreProjTempo, NULL, NULL, NULL) && ignoreProjTempo)
				length /= GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
		}
	}

	WritePtr(isMidiSource, isMidi);
	return length;
}

double GetOriginalPpqPos (MediaItem_Take* take, double ppqPos, bool* loopedItem, double* posVisInsertStartPpq, double* posVisInsertEndPpq)
{
	double returnPos = 0;
	MediaItem* item = GetMediaItemTake_Item(take);
	if (!take || !item || !IsMidi(take, NULL))
	{
		WritePtr(loopedItem,          false);
		WritePtr(posVisInsertStartPpq, 0.0);
		WritePtr(posVisInsertEndPpq,   0.0);
	}
	else
	{
		double itemStart = GetMediaItemInfo_Value(item, "D_POSITION");
		double itemEnd   = itemStart + GetMediaItemInfo_Value(item, "D_LENGTH");

		if (GetMediaItemInfo_Value(item, "B_LOOPSRC") == 0)
		{
			WritePtr(loopedItem,           false);
			WritePtr(posVisInsertStartPpq, MIDI_GetPPQPosFromProjTime(take, itemStart));
			WritePtr(posVisInsertEndPpq,   MIDI_GetPPQPosFromProjTime(take, itemEnd));
			returnPos = ppqPos;
		}
		else
		{
			WritePtr(loopedItem, true);

			double visibleItemStartPpq = MIDI_GetPPQPosFromProjTime(take, itemStart);
			double visibleItemEndPpq   = MIDI_GetPPQPosFromProjTime(take, itemEnd);
			double sourceLenPpq = GetMidiSourceLengthPPQ(take, true);

			// Deduct take offset to get correct current loop iteration
			double itemStartPpq = MIDI_GetPPQPosFromProjTime(take, itemStart - GetMediaItemTakeInfo_Value(take, "D_STARTOFFS"));
			int currentLoop;
			int loopCount = GetLoopCount(take, MIDI_GetProjTimeFromPPQPos(take, ppqPos), &currentLoop);

			returnPos = (ppqPos >= visibleItemStartPpq) ? (ppqPos - (currentLoop * sourceLenPpq)) : ppqPos;

			if (ppqPos > visibleItemEndPpq)                            // position after item end
			{
				WritePtr(posVisInsertStartPpq, 0.0);
				WritePtr(posVisInsertEndPpq,   0.0);
			}
			else if (ppqPos < visibleItemStartPpq || currentLoop == 0) // position in first loop iteration or before it
			{
				WritePtr(posVisInsertStartPpq, visibleItemStartPpq);
				WritePtr(posVisInsertEndPpq,   (visibleItemEndPpq - visibleItemStartPpq >= sourceLenPpq) ? itemStartPpq + sourceLenPpq : visibleItemEndPpq);
			}
			else if (currentLoop == loopCount)                        // position in last loop iteration
			{
				WritePtr(posVisInsertStartPpq, itemStartPpq);
				WritePtr(posVisInsertEndPpq,   itemStartPpq + (visibleItemEndPpq - (currentLoop * sourceLenPpq)));
			}
			else                                                     // position in other loop iterations
			{
				WritePtr(posVisInsertStartPpq, itemStartPpq);
				WritePtr(posVisInsertEndPpq,   itemStartPpq + sourceLenPpq);
			}
		}
	}

	return returnPos;
}

void SetMutedNotes (MediaItem_Take* take, const vector<int>& muteStatus)
{
	int noteCount = muteStatus.size();
	for (int i = 0; i < noteCount; ++i)
	{
		bool muted = !!muteStatus[i];
		MIDI_SetNote(take, i, NULL, &muted, NULL, NULL, NULL, NULL, NULL, NULL);
	}
}

void SetSelectedNotes (MediaItem_Take* take, const vector<int>& selectedNotes, bool unselectOthers)
{
	int selectedCount = selectedNotes.size();
	int selectedId = (selectedCount > 0) ? (0) : (1);

	int noteCount;
	MIDI_CountEvts(take, &noteCount, NULL, NULL);
	for (int i = 0; i < noteCount; ++i)
	{
		bool selected = false;
		if (selectedId < selectedCount && i == selectedNotes[selectedId])
		{
			selected = true;
			++selectedId;
		}

		MIDI_SetNote(take, i, unselectOthers ? &selected : (selected ? &selected : NULL), NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	}
}

void UnselectAllEvents (MediaItem_Take* take, int lane)
{
	if (take)
	{
		if ((lane >= 0 && lane <= 127))
		{
			int id = -1;
			while ((id = MIDI_EnumSelCC(take, id)) != -1)
			{
				int cc, chanMsg;
				if (MIDI_GetCC(take, id, NULL, NULL, NULL, &chanMsg, NULL, &cc, NULL) && chanMsg == STATUS_CC && cc == lane)
					MIDI_SetCC(take, id, &g_bFalse, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
			}
		}
		else if (lane == CC_PROGRAM || lane == CC_CHANNEL_PRESSURE || lane == CC_PITCH || lane == CC_BANK_SELECT)
		{
			if (lane == CC_BANK_SELECT)
				lane = CC_PROGRAM;

			int type = (lane == CC_PROGRAM) ? (STATUS_PROGRAM) : ((lane == CC_CHANNEL_PRESSURE) ? STATUS_CHANNEL_PRESSURE : STATUS_PITCH);
			int id = -1;
			while ((id = MIDI_EnumSelCC(take, id)) != -1)
			{
				int chanMsg;
				if (MIDI_GetCC(take, id, NULL, NULL, NULL, &chanMsg, NULL, NULL, NULL) && chanMsg == type)
					MIDI_SetCC(take, id, &g_bFalse, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
			}
		}
		else if (lane == CC_VELOCITY || lane == CC_VELOCITY_OFF)
		{
			int id = -1;
			while ((id = MIDI_EnumSelNotes(take, id)) != -1)
				MIDI_SetNote(take, id, &g_bFalse, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		}
		else if (lane == CC_TEXT_EVENTS || lane == CC_SYSEX)
		{
			int id = -1;
			while ((id = MIDI_EnumSelTextSysexEvts(take, id)) != -1)
			{
				int type = 0;
				if (MIDI_GetTextSysexEvt(take, id, NULL, NULL, NULL, &type, NULL, NULL) && ((lane == CC_SYSEX && type == -1) || (lane == CC_TEXT_EVENTS && type != -1)))
					MIDI_SetTextSysexEvt(take, id, &g_bFalse, NULL, NULL, NULL, NULL, 0, NULL);
			}
		}
		else if (lane >= CC_14BIT_START)
		{
			int id = -1;

			int cc1 = lane - CC_14BIT_START;
			int cc2 = cc1 + 32;
			while ((id = MIDI_EnumSelCC(take, id)) != -1)
			{
				int cc, chanMsg;
				if (MIDI_GetCC(take, id, NULL, NULL, NULL, &chanMsg, NULL, &cc, NULL) && chanMsg == STATUS_CC && (cc == cc1 || cc == cc2))
					MIDI_SetCC(take, id, &g_bFalse, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
			}
		}
	}
}

bool AreAllNotesUnselected (MediaItem_Take* take)
{
	bool firstNote = false;
	MIDI_GetNote(take, 0, &firstNote, NULL, NULL, NULL, NULL, NULL, NULL);

	if (!firstNote && MIDI_EnumSelNotes(take, 0) == -1)
		return true;
	else
		return false;
}

bool IsMidi (MediaItem_Take* take, bool* inProject /*= NULL*/)
{
	if (PCM_source* source = GetMediaItemTake_Source(take))
	{
		const char* type = source->GetType();
		if (!strcmp(type, "MIDI") || !strcmp(type, "MIDIPOOL"))
		{
			if (inProject)
			{
				const char* fileName = source->GetFileName();
				*inProject = (fileName && !strcmp(fileName, ""));
			}
			return true;
		}
	}
	WritePtr(inProject, false);
	return false;
}

bool IsOpenInInlineEditor (MediaItem_Take* take)
{
	bool inProject = false;
	if (GetActiveTake(GetMediaItemTake_Item(take)) == take && IsMidi(take, &inProject) && inProject)
	{
		if (PCM_source* source = GetMediaItemTake_Source(take))
		{
			if (source->Extended(PCM_SOURCE_EXT_INLINEEDITOR, 0, 0, 0) > 0)
				return true;
		}
	}
	return false;
}

bool IsMidiNoteBlack (int note)
{
	note = 1 << (note % 12);     // note position in an octave (first bit is C, second bit is C# etc...)
	return (note & 0x054A) != 0; // 54A = all black notes bits set to 1
}

bool IsVelLaneValid (int lane)
{
	if (lane >= CC_VELOCITY && lane <= CC_VELOCITY_OFF)
		return true;
	else
		return false;
}

bool DeleteEventsInLane (MediaItem_Take* take, int lane, bool selectedOnly, double startRangePpq, double endRangePpq, bool doRange)
{
	bool update = false;

	if (lane == CC_SYSEX || lane == CC_TEXT_EVENTS)
	{
		int sysCount; MIDI_CountEvts(take, NULL, NULL, &sysCount);
		for (int i = 0; i < sysCount; ++i)
		{
			bool selected;
			double position;
			int type;
			MIDI_GetTextSysexEvt(take, i, &selected, NULL, &position, &type, NULL, 0);

			if ((!doRange || CheckBounds(position, startRangePpq, endRangePpq)) && ((lane == CC_SYSEX && type == -1) || (lane == CC_TEXT_EVENTS && CheckBounds(type, 1, 7))))
			{
				if (!selectedOnly || selected)
				{
					MIDI_DeleteTextSysexEvt(take, i);
					--i;
					update = true;
				}
			}
		}
	}
	else
	{
		int eventType = ConvertLaneToStatusMsg(lane);
		int lane1     = (lane >= CC_14BIT_START) ? lane - CC_14BIT_START : lane;
		int lane2     = (lane >= CC_14BIT_START) ? lane + 32             : lane;

		int ccCount; MIDI_CountEvts(take, NULL, &ccCount, NULL);
		for (int i = 0; i < ccCount; ++i)
		{
			bool selected;
			double position;
			int chanMsg, msg2, msg3;
			MIDI_GetCC(take, i, &selected, NULL, &position, &chanMsg, NULL, &msg2, &msg3);

			if ((!doRange || CheckBounds(position, startRangePpq, endRangePpq)) && chanMsg == eventType && (eventType != STATUS_CC || (lane1 == msg2 || lane2 == msg2)))
			{
				if (!selectedOnly || selected)
				{
					MIDI_DeleteCC(take, i);
					--i;
					update = true;
				}
			}
		}
	}
	return update;
}

int FindFirstSelectedNote (MediaItem_Take* take, BR_MidiEditor* midiEditorFilterSettings)
{
	int id = -1;
	int foundId = -1;
	while ((id = MIDI_EnumSelNotes(take, id)) != -1)
	{
		if (midiEditorFilterSettings)
		{
			if (midiEditorFilterSettings->IsNoteVisible(take, id))
			{
				foundId = id;
				break;
			}
		}
		else
		{
			foundId = id;
			break;
		}
	}
	return foundId;
}

int FindFirstSelectedCC (MediaItem_Take* take, BR_MidiEditor* midiEditorFilterSettings)
{
	int id = -1;
	int foundId = -1;
	while ((id = MIDI_EnumSelCC(take, id)) != -1)
	{
		if (midiEditorFilterSettings)
		{
			if (midiEditorFilterSettings->IsCCVisible(take, id))
			{
				foundId = id;
				break;
			}
		}
		else
		{
			foundId = id;
			break;
		}
	}
	return foundId;
}

int FindFirstNote (MediaItem_Take* take, BR_MidiEditor* midiEditorFilterSettings)
{
	int foundId = -1;
	int count; MIDI_CountEvts(take, &count, NULL, NULL);
	for (int i = 0; i < count; ++i)
	{
		if (midiEditorFilterSettings)
		{
			if (midiEditorFilterSettings->IsNoteVisible(take, i))
			{
				foundId = i;
				break;
			}
		}
		else
		{
			foundId = i;
			break;
		}
	}
	return foundId;
}

int GetMIDIFilePPQ (const char* fp)
{
	WDL_FileRead file(fp);

	char line[14] = "";
	if (file.Read(line, sizeof(line)) && !strncmp(line, "MThd\0\0\0\x6", 8))
	{
		int byte1 = (int)((unsigned char)line[12]);
		int byte2 = (int)((unsigned char)line[13]);
		return byte1 << 8 | byte2;
	}
	else
		return 0;
}

int GetLastClickedVelLane (HWND midiEditor)
{
	if (midiEditor)
	{
		int cc = MIDIEditor_GetSetting_int(midiEditor, "last_clicked_cc_lane");
		return MapReaScriptCCToVelLane(cc);
	}
	else
	{
		return -2; // because -1 stands for velocity lane
	}
}

int GetMaxCCLanesFullHeight (HWND midiEditor)
{
	int fullHeight = 0;
	if (HWND hwnd = GetNotesView(midiEditor))
	{
		RECT r; GetWindowRect(hwnd, &r);
		int wndH = abs(r.bottom - r.top);
		fullHeight = wndH - MIDI_RULER_H - MIDI_MIN_NOTE_VIEW_H;
		if (fullHeight <= 0)
		{
			fullHeight = (int)(wndH - MIDI_RULER_H) / 2;
			if (fullHeight < 0) fullHeight = 0;
		}
	}

	return fullHeight;
}

int ConvertLaneToStatusMsg (int lane)
{
	if (lane == CC_VELOCITY)                                   return STATUS_NOTE_ON;
	if (lane == CC_VELOCITY_OFF)                               return STATUS_NOTE_OFF;
	if (lane == CC_PITCH)                                      return STATUS_PITCH;
	if (lane == CC_PROGRAM)                                    return STATUS_PROGRAM;
	if (lane == CC_CHANNEL_PRESSURE)                           return STATUS_CHANNEL_PRESSURE;
	if (lane == CC_BANK_SELECT)                                return STATUS_PROGRAM;
	if (lane == CC_SYSEX)                                      return STATUS_SYS;
	if (lane >= 0 && lane <= 127)                              return STATUS_CC;
	if (lane >= CC_14BIT_START && lane <= CC_14BIT_START + 32) return STATUS_CC;
	return 0;
};

int MapVelLaneToReaScriptCC (int lane)
{
	if (lane == CC_VELOCITY)        return 0x200;
	if (lane == CC_VELOCITY_OFF)    return 0x207;
	if (lane == CC_NOTATION_EVENTS) return 0x208;
	if (lane >= 0   && lane <= 127) return lane;
	if (lane >= 128 && lane <= 133) return 0x200 | ((lane+1) & 0x7F);
	if (lane >= 134 && lane <= 165) return 0x100 | (lane - 134);
	else                            return -1;
}

int MapReaScriptCCToVelLane (int cc)
{
	if (cc == 0x200)                return CC_VELOCITY;
	if (cc == 0x207)                return CC_VELOCITY_OFF;
	if (cc == 0x208)                return CC_NOTATION_EVENTS;
	if (cc >= 0     && cc <= 127)   return cc;
	if (cc >= 0x201 && cc <= 0x206) return cc - 385;
	if (cc >= 0x100 && cc <= 0x11F) return cc - 122;
	else                            return -2; // because -1 stands for velocity lane
}
