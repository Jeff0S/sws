/******************************************************************************
/ BR_ProjState.cpp
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

#include "BR_ProjState.h"
#include "BR_EnvelopeUtil.h"
#include "BR_Loudness.h"
#include "BR_MidiUtil.h"
#include "BR_Util.h"
#include "../SnM/SnM_Chunk.h"

/******************************************************************************
* Constants                                                                   *
******************************************************************************/
const char* const ENVELOPE_SEL             = "<BR_ENV_SEL_SLOT";
const char* const CURSOR_POS               = "<BR_CURSOR_POS";
const char* const NOTE_SEL                 = "<BR_NOTE_SEL_SLOT";
const char* const SAVED_CC_EVENTS          = "<BR_SAVED_CC_EVENTS";
const char* const ITEMS_MUTE_STATES        = "<BR_ITEMS_MUTE_STATE_SLOT";
const char* const TRACKS_SOLO_MUTE_STATES  = "<BR_TRACKS_SOLO_MUTE_STATE_SLOT";
const char* const SAVED_CC_LANES           = "<BR_SAVED_HIDDEN_CC_LANES";

/******************************************************************************
* Globals                                                                     *
******************************************************************************/
SWSProjConfig<WDL_PtrList_DOD<BR_EnvSel> >             g_envSel;
SWSProjConfig<WDL_PtrList_DOD<BR_CursorPos> >          g_cursorPos;
SWSProjConfig<WDL_PtrList_DOD<BR_MidiNoteSel> >        g_midiNoteSel;
SWSProjConfig<WDL_PtrList_DOD<BR_MidiCCEvents> >       g_midiCCEvents;
SWSProjConfig<WDL_PtrList_DOD<BR_ItemMuteState> >      g_itemMuteState;
SWSProjConfig<WDL_PtrList_DOD<BR_TrackSoloMuteState> > g_trackSoloMuteState;
SWSProjConfig<BR_MidiToggleCCLane>                     g_midiToggleHideCCLanes;

/******************************************************************************
* Project state init/exit                                                     *
******************************************************************************/
static bool ProcessExtensionLine (const char *line, ProjectStateContext *ctx, bool isUndo, project_config_extension_t *reg)
{
	if (isUndo)
		return false;

	LineParser lp(false);
	if (lp.parse(line) || lp.getnumtokens() < 1)
		return false;

	// Envelope point selection
	if (!strcmp(lp.gettoken_str(0), ENVELOPE_SEL))
	{
		g_envSel.Get()->Add(new BR_EnvSel(lp.gettoken_int(1), ctx));
		return true;
	}

	// Edit cursor position
	if (!strcmp(lp.gettoken_str(0), CURSOR_POS))
	{
		g_cursorPos.Get()->Add(new BR_CursorPos(lp.gettoken_int(1), ctx));
		return true;
	}

	// Midi note selection
	if (!strcmp(lp.gettoken_str(0), NOTE_SEL))
	{
		g_midiNoteSel.Get()->Add(new BR_MidiNoteSel(lp.gettoken_int(1), ctx));
		return true;
	}

	// Saved CC events
	if (!strcmp(lp.gettoken_str(0), SAVED_CC_EVENTS))
	{
		g_midiCCEvents.Get()->Add(new BR_MidiCCEvents(lp.gettoken_int(1), ctx));
		return true;
	}

	// Items mute state
	if (!strcmp(lp.gettoken_str(0), ITEMS_MUTE_STATES))
	{
		g_itemMuteState.Get()->Add(new BR_ItemMuteState(lp.gettoken_int(1), ctx));
		return true;
	}

	// Tracks solo/mute state
	if (!strcmp(lp.gettoken_str(0), TRACKS_SOLO_MUTE_STATES))
	{
		g_trackSoloMuteState.Get()->Add(new BR_TrackSoloMuteState(lp.gettoken_int(1), ctx));
		return true;
	}

	// Toggled hidden CC lanes
	if (!strcmp(lp.gettoken_str(0), SAVED_CC_LANES))
	{
		g_midiToggleHideCCLanes.Get()->LoadState(ctx);
		return true;
	}

	return false;
}

static void SaveExtensionConfig (ProjectStateContext *ctx, bool isUndo, project_config_extension_t *reg)
{
	if (isUndo)
		return;

	// Envelope point selection
	if (int count = g_envSel.Get()->GetSize())
	{
		for (int i = 0; i < count; ++i)
			g_envSel.Get()->Get(i)->SaveState(ctx);
	}

	// Edit cursor position
	if (int count = g_cursorPos.Get()->GetSize())
	{
		for (int i = 0; i < count; ++i)
			g_cursorPos.Get()->Get(i)->SaveState(ctx);
	}

	// Midi note selection
	if (int count = g_midiNoteSel.Get()->GetSize())
	{
		for (int i = 0; i < count; ++i)
			g_midiNoteSel.Get()->Get(i)->SaveState(ctx);
	}

	// Saved CC events
	if (int count = g_midiCCEvents.Get()->GetSize())
	{
		for (int i = 0; i < count; ++i)
			g_midiCCEvents.Get()->Get(i)->SaveState(ctx);
	}

	// Items mute state
	if (int count = g_itemMuteState.Get()->GetSize())
	{
		for (int i = 0; i < count; ++i)
			g_itemMuteState.Get()->Get(i)->SaveState(ctx);
	}

	// Tracks solo/mute state
	if (int count = g_trackSoloMuteState.Get()->GetSize())
	{
		for (int i = 0; i < count; ++i)
			g_trackSoloMuteState.Get()->Get(i)->SaveState(ctx);
	}

	// Toggled hidden CC lanes
	if (g_midiToggleHideCCLanes.Get()->IsHidden())
		g_midiToggleHideCCLanes.Get()->SaveState(ctx);
}

static void BeginLoadProjectState (bool isUndo, project_config_extension_t *reg)
{
	if (isUndo)
		return;

	// Envelope point selection
	g_envSel.Get()->Empty(true);
	g_envSel.Cleanup();

	// Edit cursor position
	g_cursorPos.Get()->Empty(true);
	g_cursorPos.Cleanup();

	// Midi note selection
	g_midiNoteSel.Get()->Empty(true);
	g_midiNoteSel.Cleanup();

	// Saved CC events
	g_midiCCEvents.Get()->Empty(true);
	g_midiCCEvents.Cleanup();

	// Items mute state
	g_itemMuteState.Get()->Empty(true);
	g_itemMuteState.Cleanup();

	// Tracks solo/mute state
	g_trackSoloMuteState.Get()->Empty(true);
	g_trackSoloMuteState.Cleanup();

	// Toggled hidden CC lanes
	g_midiCCEvents.Cleanup();
}

int ProjStateInitExit (bool init)
{
	static project_config_extension_t s_projectconfig = {ProcessExtensionLine, SaveExtensionConfig, BeginLoadProjectState, NULL};

	if (init)
	{
		return plugin_register("projectconfig", &s_projectconfig);
	}
	else
	{
		plugin_register("-projectconfig", &s_projectconfig);
		return 1;
	}
}

/******************************************************************************
* Envelope selection state                                                    *
******************************************************************************/
BR_EnvSel::BR_EnvSel (int slot, TrackEnvelope* envelope) :
m_slot (slot)
{
	this->Save(envelope);
}

BR_EnvSel::BR_EnvSel (int slot, ProjectStateContext* ctx) :
m_slot (slot)
{
	char line[64];
	LineParser lp(false);
	while(!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
	{
		if (!strcmp(lp.gettoken_str(0), ">"))
			break;
		m_selection.push_back(lp.gettoken_int(0));
	}
}

void BR_EnvSel::SaveState (ProjectStateContext* ctx)
{
	ctx->AddLine("%s %.2d", ENVELOPE_SEL, m_slot);
	for (size_t i = 0; i < m_selection.size(); ++i)
		ctx->AddLine("%d", m_selection[i]);
	ctx->AddLine(">");
}

void BR_EnvSel::Save (TrackEnvelope* envelope)
{
	m_selection = GetSelPoints(envelope);
	MarkProjectDirty(NULL);
}

bool BR_EnvSel::Restore (TrackEnvelope* envelope)
{
	BR_Envelope env(envelope);
	env.UnselectAll();

	for (size_t i = 0; i < m_selection.size(); ++i)
		env.SetSelection(m_selection[i], true);
	if (env.Commit())
		return true;
	else
		return false;
}

int BR_EnvSel::GetSlot ()
{
	return m_slot;
}

/******************************************************************************
* Cursor position state                                                       *
******************************************************************************/
BR_CursorPos::BR_CursorPos (int slot) :
m_slot     (slot),
m_position (0)
{
	this->Save();
}

BR_CursorPos::BR_CursorPos (int slot, ProjectStateContext* ctx) :
m_slot (slot)
{
	char line[64];
	LineParser lp(false);
	while(!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
	{
		if (!strcmp(lp.gettoken_str(0), ">"))
			break;
		m_position = lp.gettoken_float(0);
	}
}

void BR_CursorPos::SaveState (ProjectStateContext* ctx)
{
	ctx->AddLine("%s %.2d", CURSOR_POS, m_slot);
	ctx->AddLine("%.14lf", m_position);
	ctx->AddLine(">");
}

void BR_CursorPos::Save ()
{
	m_position = GetCursorPositionEx(NULL);
	MarkProjectDirty(NULL);
}

bool BR_CursorPos::Restore ()
{
	if (GetCursorPositionEx(NULL) == m_position)
		return false;
	else
	{
		SetEditCurPos2(NULL, m_position, true, false);
		return true;
	}
}

int BR_CursorPos::GetSlot ()
{
	return m_slot;
}

/******************************************************************************
* MIDI notes selection state                                                  *
******************************************************************************/
BR_MidiNoteSel::BR_MidiNoteSel (int slot, MediaItem_Take* take) :
m_slot      (slot)
{
	this->Save(take);
}

BR_MidiNoteSel::BR_MidiNoteSel (int slot, ProjectStateContext* ctx) :
m_slot (slot)
{
	char line[64];
	LineParser lp(false);
	while(!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
	{
		if (!strcmp(lp.gettoken_str(0), ">"))
			break;
		m_selection.push_back(lp.gettoken_int(0));
	}
}

void BR_MidiNoteSel::SaveState (ProjectStateContext* ctx)
{
	ctx->AddLine("%s %.2d", NOTE_SEL, m_slot);
	for (size_t i = 0; i < m_selection.size(); ++i)
		ctx->AddLine("%d", m_selection[i]);
	ctx->AddLine(">");
}

void BR_MidiNoteSel::Save (MediaItem_Take* take)
{
	m_selection = GetSelectedNotes(take);
	MarkProjectDirty(NULL);
}

bool BR_MidiNoteSel::Restore (MediaItem_Take* take)
{
	if (GetSelectedNotes(take) == m_selection)
		return false;
	else
	{
		SetSelectedNotes(take, m_selection, true);
		return true;
	}
}

int BR_MidiNoteSel::GetSlot ()
{
	return m_slot;
}

/******************************************************************************
* MIDI saved CC events state                                                  *
******************************************************************************/
BR_MidiCCEvents::BR_MidiCCEvents (int slot, BR_MidiEditor& midiEditor, int lane) :
m_slot           (slot),
m_sourceLane     (lane),
m_ppq            (960),
m_sourcePpqStart (-1)
{
	this->Save(midiEditor, m_sourceLane);
}

BR_MidiCCEvents::BR_MidiCCEvents (int slot, ProjectStateContext* ctx) :
m_slot       (slot),
m_sourceLane (0),
m_ppq        (960),
m_sourcePpqStart (-1)
{
	char line[512];
	LineParser lp(false);
	while(!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
	{
		if (!strcmp(lp.gettoken_str(0), "E"))
		{
			BR_MidiCCEvents::Event event(lp.gettoken_float(1),
			                             lp.gettoken_int(2),
			                             lp.gettoken_int(3),
			                             lp.gettoken_int(4),
			                             !!lp.gettoken_int(5)
			                            );
			m_events.push_back(event);
		}
		else if (!strcmp(lp.gettoken_str(0), "SOURCE_LANE"))
		{
			m_sourceLane = lp.gettoken_int(1);
		}
		else if (!strcmp(lp.gettoken_str(0), "PPQ"))
		{
			m_ppq = lp.gettoken_int(1);
		}
		else if (!strcmp(lp.gettoken_str(0), ">"))
			break;
	}
}

BR_MidiCCEvents::BR_MidiCCEvents (int slot, int lane):
m_slot           (slot),
m_sourceLane     (lane),
m_ppq            (0),
m_sourcePpqStart (-1)
{
}

void BR_MidiCCEvents::SaveState (ProjectStateContext* ctx)
{
	if (m_events.size() != 0)
	{
		ctx->AddLine("%s %.2d", SAVED_CC_EVENTS, m_slot);
		ctx->AddLine("%s %d",   "SOURCE_LANE",   m_sourceLane);
		ctx->AddLine("%s %d",   "PPQ",           m_ppq);
		for (size_t i = 0; i < m_events.size(); ++i)
			ctx->AddLine("E %lf %d %d %d %d",
			             m_events[i].positionPpq,
			             m_events[i].channel,
			             m_events[i].msg2,
			             m_events[i].msg3,
			             (int)m_events[i].mute
			            );

		ctx->AddLine(">");
	}
}

bool BR_MidiCCEvents::Save (BR_MidiEditor& midiEditor, int lane)
{
	if (midiEditor.IsValid())
	{
		MediaItem_Take* take = midiEditor.GetActiveTake();
		vector<BR_MidiCCEvents::Event> events;

		m_sourcePpqStart = -1;
		if ((lane >= 0 && lane <= 127))
		{
			int id = -1;

			while ((id = MIDI_EnumSelCC(take, id)) != -1)
			{
				int cc, chanMsg;
				if (MIDI_GetCC(take, id, NULL, NULL, NULL, &chanMsg, NULL, &cc, NULL) && midiEditor.IsCCVisible(take, id) && chanMsg == STATUS_CC && cc == lane)
				{
					BR_MidiCCEvents::Event event;
					MIDI_GetCC(take, id, NULL, &event.mute, &event.positionPpq, NULL, &event.channel, NULL, &event.msg3);
					events.push_back(event);
					if (m_sourcePpqStart == -1)
						m_sourcePpqStart = event.positionPpq;
				}
			}
		}
		else if (lane == CC_PITCH)
		{
			int id = -1;
			while ((id = MIDI_EnumSelCC(take, id)) != -1)
			{
				int chanMsg;
				if (MIDI_GetCC(take, id, NULL, NULL, NULL, &chanMsg, NULL, NULL, NULL) && midiEditor.IsCCVisible(take, id) &&chanMsg == STATUS_PITCH)
				{
					BR_MidiCCEvents::Event event;
					MIDI_GetCC(take, id, NULL, &event.mute, &event.positionPpq, NULL, &event.channel, &event.msg2, &event.msg3);
					events.push_back(event);
					if (m_sourcePpqStart == -1)
						m_sourcePpqStart = event.positionPpq;
				}
			}
		}
		else if (lane == CC_PROGRAM || lane == CC_CHANNEL_PRESSURE)
		{
			int id = -1;
			while ((id = MIDI_EnumSelCC(take, id)) != -1)
			{
				int chanMsg;
				if (MIDI_GetCC(take, id, NULL, NULL, NULL, &chanMsg, NULL, NULL, NULL) && midiEditor.IsCCVisible(take, id) && chanMsg == ((lane == CC_PROGRAM) ? STATUS_PROGRAM : STATUS_CHANNEL_PRESSURE))
				{
					BR_MidiCCEvents::Event event;
					MIDI_GetCC(take, id, NULL, &event.mute, &event.positionPpq, NULL, &event.channel, &event.msg3, &event.msg2); // messages are reversed
					events.push_back(event);
					if (m_sourcePpqStart == -1)
						m_sourcePpqStart = event.positionPpq;
				}
			}
		}
		else if (lane == CC_VELOCITY || lane == CC_VELOCITY_OFF)
		{
			int id = -1;
			while ((id = MIDI_EnumSelNotes(take, id)) != -1)
			{
				BR_MidiCCEvents::Event event;
				if (MIDI_GetNote(take, id, NULL, &event.mute, &event.positionPpq, NULL, &event.channel, NULL, &event.msg3) && midiEditor.IsNoteVisible(take, id))
				{
					events.push_back(event);
					if (m_sourcePpqStart == -1)
						m_sourcePpqStart = event.positionPpq;
				}
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
				if (MIDI_GetCC(take, id, NULL, NULL, NULL, &chanMsg, NULL, &cc, NULL) && midiEditor.IsCCVisible(take, id) && chanMsg == STATUS_CC && cc == cc1)
				{
					BR_MidiCCEvents::Event event;
					MIDI_GetCC(take, id, NULL, &event.mute, &event.positionPpq, NULL, &event.channel, NULL, &event.msg3);
					events.push_back(event);
					if (m_sourcePpqStart == -1)
						m_sourcePpqStart = event.positionPpq;

					int tmpId = id;
					while ((tmpId = MIDI_EnumSelCC(take, tmpId)) != -1)
					{
						double pos; int channel, value, chanMsg2;
						MIDI_GetCC(take, tmpId, NULL, NULL, &pos, &chanMsg2, &channel, &cc, &value);
						if (pos > event.positionPpq)
							break;
						if (chanMsg2 == STATUS_CC && cc == cc2 && channel == events.back().channel)
						{
							events.back().msg2 = value;
							break;
						}
					}
				}
			}
		}

		if (events.size())
		{
			// Make sure positions are saved in a relative manner
			for (size_t i = 1; i < events.size(); ++i)
				events[i].positionPpq -= events.front().positionPpq;
			events.front().positionPpq = 0;

			m_events     = events;
			m_ppq        = midiEditor.GetPPQ();
			m_sourceLane = lane;

			MarkProjectDirty(NULL);
			return true;
		}
	}
	return false;
}

bool BR_MidiCCEvents::Restore (BR_MidiEditor& midiEditor, int lane, bool allVisible, double startPositionPppq, bool showWarningForInvalidLane /*=true*/, bool moveEditCursor /*=true*/)
{
	bool update = false;
	if (m_events.size() && midiEditor.IsValid())
	{
		MediaItem_Take* take = midiEditor.GetActiveTake();
		MediaItem*      item = GetMediaItemTake_Item(take);

		set<int> lanesToProcess;
		if (allVisible)
		{
			for (int i = 0; i < midiEditor.CountCCLanes(); ++i)
			{
				int lane = midiEditor.GetCCLane(i);

				// 14 bit takes priority
				if (lane >= 0 && lane <= 63)
				{
					int lane14Bit = CC_14BIT_START + ((lane <= 31) ? (lane) : (lane - 32));
					if (midiEditor.FindCCLane(lane14Bit) == -1)
						lanesToProcess.insert(lane);
				}
				else
					lanesToProcess.insert(lane);
			}
		}
		else
			lanesToProcess.insert(lane);

		double moveOffsetMaxPPQ = -1;
		for (set<int>::iterator i = lanesToProcess.begin(); i != lanesToProcess.end(); ++i)
		{
			int targetLane  = *i;
			int targetLane2 = targetLane;
			if (!midiEditor.IsCCLaneVisible(targetLane) ||
			    targetLane == CC_TEXT_EVENTS            ||
			    targetLane == CC_SYSEX                  ||
			    targetLane == CC_BANK_SELECT            ||
			    targetLane == CC_VELOCITY               ||
			    targetLane == CC_VELOCITY_OFF
			)
				continue;

			double startLimitPPQ, endLimitPPQ;
			bool loopedItem;
			startPositionPppq = GetOriginalPpqPos(take, startPositionPppq,  &loopedItem, &startLimitPPQ, &endLimitPPQ);

			if (!loopedItem)
			{
				// Extend item if needed
				const int midiVu = ConfigVar<int>("midivu").value_or(0);
				if (GetBit(midiVu, 14))
				{
					double itemStart = GetMediaItemInfo_Value(item, "D_POSITION");
					double itemEnd = itemStart + GetMediaItemInfo_Value(item, "D_LENGTH");

					double newEnd  = MIDI_GetProjTimeFromPPQPos(take, startPositionPppq + m_events.back().positionPpq + 1);
					if (newEnd > itemEnd)
						SetMediaItemInfo_Value(item, "D_LENGTH", newEnd - itemStart);

					startLimitPPQ = MIDI_GetPPQPosFromProjTime(take, itemStart);
					endLimitPPQ   = MIDI_GetPPQPosFromProjTime(take, itemStart + GetMediaItemInfo_Value(item, "D_LENGTH"));
				}
			}

			// Restore events
			double ratioPPQ = (m_ppq <= 0) ? (1) : ((double)midiEditor.GetPPQ() / (double)m_ppq);
			bool do14bit    = (targetLane >= CC_14BIT_START)                                  ? true : false;
			bool reverseMsg = (targetLane == CC_PROGRAM || targetLane == CC_CHANNEL_PRESSURE) ? true : false;
			bool doMsg2     = (!do14bit && !CheckBounds(targetLane, 0, 127))                  ? true : false;
			int type        = (targetLane == CC_PROGRAM) ? (STATUS_PROGRAM) : (targetLane == CC_CHANNEL_PRESSURE ? STATUS_CHANNEL_PRESSURE : (targetLane == CC_PITCH ? STATUS_PITCH : STATUS_CC));

			targetLane  = (do14bit) ? targetLane - CC_14BIT_START : targetLane;
			targetLane2 = (do14bit) ? targetLane + 32             : targetLane2;
			double moveOffsetPPQ = -1;
			bool insertedOneEvent = false;
			for (size_t i = 0; i < m_events.size(); ++i)
			{
				double posAddPPQ = (ratioPPQ == 0) ? m_events[i].positionPpq : Round(m_events[i].positionPpq * ratioPPQ);
				double positionPpq = startPositionPppq + posAddPPQ;

				if (positionPpq < startLimitPPQ)
					continue;
				if (positionPpq > endLimitPPQ)
					break;
				moveOffsetPPQ = posAddPPQ;

				// Make sure events are unselected only when we know at least new one event will get inserted
				if (!insertedOneEvent)
				{
					insertedOneEvent = true;
					UnselectAllEvents(take, targetLane);
				}

				MIDI_InsertCC(take,
				              true,
				              m_events[i].mute,
				              positionPpq,
				              type,
				              midiEditor.IsChannelVisible(m_events[i].channel) ? m_events[i].channel : midiEditor.GetDrawChannel(),
				              (!doMsg2)    ? targetLane       : (reverseMsg ? m_events[i].msg3 : m_events[i].msg2),
				              (reverseMsg) ? m_events[i].msg2 : m_events[i].msg3
				);

				if (do14bit)
				{
					MIDI_InsertCC(take,
					              true,
					              m_events[i].mute,
					              positionPpq,
					              type,
					              midiEditor.IsChannelVisible(m_events[i].channel) ? m_events[i].channel : midiEditor.GetDrawChannel(),
					              targetLane2,
					              m_events[i].msg2
					);
				}
			}

			if (moveOffsetPPQ != -1 && moveOffsetPPQ > moveOffsetMaxPPQ)
				moveOffsetMaxPPQ = moveOffsetPPQ;
		}

		if (moveOffsetMaxPPQ != -1)
		{
			update = true;
			if (moveEditCursor)
			{
				double newPosPPQ = Trunc(moveOffsetMaxPPQ + MIDI_GetPPQPosFromProjTime(take, GetCursorPositionEx(NULL))); // trunc because reaper creates events exactly on ppq
				SetEditCurPos(MIDI_GetProjTimeFromPPQPos(take, newPosPPQ), true, false);
			}
		}
	}
	return update;
}

int BR_MidiCCEvents::GetSlot ()
{
	return m_slot;
}

int BR_MidiCCEvents::CountSavedEvents ()
{
	return (int)m_events.size();
}

double BR_MidiCCEvents::GetSourcePpqStart ()
{
	return m_sourcePpqStart;
}

void BR_MidiCCEvents::AddEvent (double ppqPos, int msg2, int msg3, int channel)
{
	BR_MidiCCEvents::Event event;
	event.positionPpq = ppqPos;
	event.msg2        = msg2;
	event.msg3        = msg3;
	event.channel     = channel;
	event.mute        = false;

	if (m_sourcePpqStart == -1)
	{
		m_sourcePpqStart = ppqPos;
		event.positionPpq = 0;
	}
	else
		event.positionPpq -= m_sourcePpqStart;

	m_events.push_back(event);
}

BR_MidiCCEvents::Event::Event () :
positionPpq (0),
channel     (0),
msg2        (0),
msg3        (0),
mute        (false)
{
}

BR_MidiCCEvents::Event::Event (double positionPpq, int channel, int msg2, int msg3, bool mute) :
positionPpq (positionPpq),
channel     (channel),
msg2        (msg2),
msg3        (msg3),
mute        (mute)
{
}

/******************************************************************************
* Item mute state                                                             *
******************************************************************************/
BR_ItemMuteState::BR_ItemMuteState (int slot, bool selectedOnly) :
m_slot (slot)
{
	this->Save(selectedOnly);
}

BR_ItemMuteState::BR_ItemMuteState (int slot, ProjectStateContext* ctx) :
m_slot (slot)
{
	char line[64];
	LineParser lp(false);
	while(!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
	{
		if (!strcmp(lp.gettoken_str(0), ">"))
			break;

		GUID guid;stringToGuid(lp.gettoken_str(0), &guid);

		BR_ItemMuteState::MuteState trackState;
		trackState.guid = guid;
		trackState.mute = lp.gettoken_int(1);
		m_items.push_back(trackState);
	}
}

void BR_ItemMuteState::SaveState (ProjectStateContext* ctx)
{
	if (m_items.size() != 0)
	{
		ctx->AddLine("%s %.2d", ITEMS_MUTE_STATES, m_slot);
		for (size_t i = 0; i < m_items.size(); ++i)
		{
			char guid[64];
			guidToString(&m_items[i].guid, guid);
			ctx->AddLine("%s %d", guid, m_items[i].mute);
		}
		ctx->AddLine(">");
	}
}

void BR_ItemMuteState::Save (bool selectedOnly)
{
	m_items.clear();

	const int count = (selectedOnly) ? CountSelectedMediaItems(NULL) : CountMediaItems(NULL);
	for (int i = 0; i < count; ++i)
	{
		MediaItem* item = (selectedOnly) ? GetSelectedMediaItem(NULL, i) : GetMediaItem(NULL, i);

		BR_ItemMuteState::MuteState trackState;
		trackState.guid = GetItemGuid(item);
		trackState.mute = (int)GetMediaItemInfo_Value(item, "B_MUTE");
		m_items.push_back(trackState);
	}
	MarkProjectDirty(NULL);
}

bool BR_ItemMuteState::Restore (bool selectedOnly)
{
	PreventUIRefresh(1);
	bool update = false;
	for (size_t i = 0; i < m_items.size(); ++i)
	{
		if (MediaItem* item = GuidToItem(&m_items[i].guid))
		{
			if (!selectedOnly || GetMediaItemInfo_Value(item, "B_UISEL"))
			{
				SetMediaItemInfo_Value(item, "B_MUTE", m_items[i].mute);
				update = true;
			}
		}
	}
	PreventUIRefresh(-1);
	UpdateArrange();
	return update;
}

int BR_ItemMuteState::GetSlot ()
{
	return m_slot;
}

/******************************************************************************
* Track solo/mute state                                                        *
******************************************************************************/
BR_TrackSoloMuteState::BR_TrackSoloMuteState (int slot, bool selectedOnly) :
m_slot (slot)
{
	this->Save(selectedOnly);
}

BR_TrackSoloMuteState::BR_TrackSoloMuteState (int slot, ProjectStateContext* ctx) :
m_slot (slot)
{
	char line[64];
	LineParser lp(false);
	while(!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
	{
		if (!strcmp(lp.gettoken_str(0), ">"))
			break;

		GUID guid; stringToGuid(lp.gettoken_str(0), &guid);

		BR_TrackSoloMuteState::SoloMuteState trackState;
		trackState.guid = guid;
		trackState.solo = lp.gettoken_int(1);
		trackState.mute = lp.gettoken_int(2);
		m_tracks.push_back(trackState);
	}
}

void BR_TrackSoloMuteState::SaveState (ProjectStateContext* ctx)
{
	if (m_tracks.size() != 0)
	{
		ctx->AddLine("%s %.2d", TRACKS_SOLO_MUTE_STATES, m_slot);
		for (size_t i = 0; i < m_tracks.size(); ++i)
		{
			char guid[64];
			guidToString(&m_tracks[i].guid, guid);
			ctx->AddLine("%s %d %d", guid, m_tracks[i].solo, m_tracks[i].mute);
		}
		ctx->AddLine(">");
	}
}

void BR_TrackSoloMuteState::Save (bool selectedOnly)
{
	m_tracks.clear();

	const int count = (selectedOnly) ? CountSelectedTracks(NULL) : CountTracks(NULL);
	for (int i = 0; i < count; ++i)
	{
		MediaTrack* track = (selectedOnly) ? GetSelectedTrack(NULL, i) : GetTrack(NULL, i);

		BR_TrackSoloMuteState::SoloMuteState trackState;
		trackState.guid = *GetTrackGUID(track);
		trackState.solo = (int)GetMediaTrackInfo_Value(track, "I_SOLO");
		trackState.mute = (int)GetMediaTrackInfo_Value(track, "B_MUTE");
		m_tracks.push_back(trackState);
	}
	MarkProjectDirty(NULL);
}

bool BR_TrackSoloMuteState::Restore (bool selectedOnly)
{
	PreventUIRefresh(1);
	bool update = false;
	for (size_t i = 0; i < m_tracks.size(); ++i)
	{
		if (MediaTrack* track = GuidToTrack(&m_tracks[i].guid))
		{
			if (!selectedOnly || GetMediaTrackInfo_Value(track, "I_SELECTED"))
			{
				SetMediaTrackInfo_Value(track, "I_SOLO", m_tracks[i].solo);
				SetMediaTrackInfo_Value(track, "B_MUTE", m_tracks[i].mute);
				update = true;
			}
		}
	}
	PreventUIRefresh(-1);
	return update;
}

int BR_TrackSoloMuteState::GetSlot ()
{
	return m_slot;
}

/******************************************************************************
* MIDI toggle hide CC lanes                                                   *
******************************************************************************/
BR_MidiToggleCCLane::BR_MidiToggleCCLane ()
{
}

void BR_MidiToggleCCLane::SaveState (ProjectStateContext* ctx)
{
	if (m_ccLanes.size() != 0)
	{
		ctx->AddLine("%s", SAVED_CC_LANES);
		for (size_t i = 0; i < m_ccLanes.size(); ++i)
			ctx->AddLine("%s", m_ccLanes[i].Get());
		ctx->AddLine(">");
	}
}

void BR_MidiToggleCCLane::LoadState (ProjectStateContext* ctx)
{
	char line[256];
	LineParser lp(false);
	m_ccLanes.clear();
	while(!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
	{
		if (!strcmp(lp.gettoken_str(0), ">"))
			break;
		else
		{
			WDL_FastString lane;
			lane.Append(line);
			m_ccLanes.push_back(lane);
		}
	}
}

bool BR_MidiToggleCCLane::Hide (HWND midiEditor, int laneToKeep, int editorHeight /*= -1*/, int inlineHeight /*= -1*/)
{
	bool update = false;
	if (m_ccLanes.size() == 0 && MIDIEditor_GetMode(midiEditor) != -1)
	{
		if (MediaItem_Take* take = MIDIEditor_GetTake(midiEditor))
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
					LineParser lp(false);

					// Remove lanes
					vector<WDL_FastString> savedCCLanes;
					WDL_FastString lineLane;
					bool lanesRemoved = false;
					int laneId = 0;
					while (int position = ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "VELLANE", laneId, -1, &lineLane))
					{
						WDL_FastString currentLane;
						currentLane.Append(lineLane.Get(), lineLane.GetLength() - 1); // -1 is to remove \n
						savedCCLanes.push_back(currentLane);

						lp.parse(lineLane.Get());
						if (lp.gettoken_int(1) != laneToKeep)
						{
							ptk.RemoveLine("SOURCE", "VELLANE", 1, laneId);
							lanesRemoved = true;
						}
						else
						{
							if ((editorHeight != -1 && editorHeight != lp.gettoken_int(2)) || (inlineHeight != -1 && inlineHeight != lp.gettoken_int(3)))
							{
								WDL_FastString newLane;
								for (int i = 0; i < lp.getnumtokens(); ++i)
								{
									if      (i == 1) newLane.AppendFormatted(256, "%d", laneToKeep);
									else if (i == 2) newLane.AppendFormatted(256, "%d", ((editorHeight == -1) ? lp.gettoken_int(2) : editorHeight));
									else if (i == 3) newLane.AppendFormatted(256, "%d", ((inlineHeight == -1) ? lp.gettoken_int(3) : inlineHeight));
									else             newLane.Append(lp.gettoken_str(i));
									newLane.Append(" ");
								}
								newLane.Append("\n");

								ptk.ReplaceLine(position - 1, newLane.Get());
								lanesRemoved = true;
							}
							++laneId;
						}

						lineLane.DeleteSub(0, lineLane.GetLength());
					}

					if (lanesRemoved && savedCCLanes.size() != 0 && p.ReplaceTake(tkPos, tklen, ptk.GetChunk()))
					{
						m_ccLanes.clear();
						m_ccLanes = savedCCLanes;
						update = true;
						MarkProjectDirty(NULL);
					}
				}
			}
		}
	}

	return update;
}

bool BR_MidiToggleCCLane::Restore (HWND midiEditor)
{
	bool update = false;
	if (m_ccLanes.size() != 0 && MIDIEditor_GetMode(midiEditor) != -1)
	{
		if (MediaItem_Take* take = MIDIEditor_GetTake(midiEditor))
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
					LineParser lp(false);
					if (int position = ptk.Parse(SNM_GET_SUBCHUNK_OR_LINE, 1, "SOURCE", "VELLANE", 0, 0))
					{
						if (ptk.RemoveLines("VELLANE"))
						{
							WDL_FastString newLanes;
							for (size_t i = 0; i <m_ccLanes.size(); ++i)
								AppendLine(newLanes, m_ccLanes[i].Get());
							ptk.GetChunk()->Insert(newLanes.Get(), position - 1);

							if (p.ReplaceTake(tkPos, tklen, ptk.GetChunk()))
							{
								m_ccLanes.clear();
								update = true;
							}
						}
					}
				}
			}
		}
	}

	return update;
}

bool BR_MidiToggleCCLane::IsHidden ()
{
	return (m_ccLanes.size() == 0) ? false : true;
}
