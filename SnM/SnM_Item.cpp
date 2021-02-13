/******************************************************************************
/ SnM_Item.cpp
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
#include "SnM_Item.h"
#include "SnM_Resources.h"
#include "SnM_Track.h"
#include "SnM_Util.h"
#include "SnM_Window.h"
#include "../Misc/Context.h"

#include <WDL/localize/localize.h>

///////////////////////////////////////////////////////////////////////////////
// General item helpers
// note: primitive funcs (no undo)
///////////////////////////////////////////////////////////////////////////////

char* GetName(MediaItem* _item)
{
	MediaItem_Take* tk = _item ? GetActiveTake(_item) : NULL;
	char* takeName = tk ? (char*)GetSetMediaItemTakeInfo(tk, "P_NAME", NULL) : NULL;
	return takeName;
}

// returns -1 if not found
int GetTakeIndex(MediaItem* _item, MediaItem_Take* _take)
{
	if (_item)
		for (int i=0; i < CountTakes(_item); i++)
			if (_take == GetTake(_item, i)) // note: NULL take is an empty take since v4
				return i;
	return -1;
}

// deletes an item if it's empty or if it's only made of NULL empty takes, returns true if deleted
bool DeleteMediaItemIfNeeded(MediaItem* _item)
{
	bool deleted = false;
	MediaTrack* tr = _item ? GetMediaItem_Track(_item) : NULL;
	if (tr && _item)
	{
		int countTk = CountTakes(_item);
		if (!countTk)
		{
			deleted = true;
		}
		else
		{
			int i=0, countEmptyTk=0; 
			while(i < countTk && !GetMediaItemTake(_item, i++))
				countEmptyTk++;
			deleted = (countTk == countEmptyTk);
		}
		if (deleted)
			deleted &= DeleteTrackMediaItem(tr, _item);
	}
	return deleted;
}

bool DeleteMediaItemsByName(const char* tkname)
{
	int cnt=0;
	for (int ii=1; ii <= GetNumTracks(); ii++) // skip master
		if (MediaTrack* tt = CSurf_TrackFromID(ii, false))
			for (int j=GetTrackNumMediaItems(tt)-1; j>=0 ; j--)
				if (MediaItem* item = GetTrackMediaItem(tt,j))
					if (MediaItem_Take* tk=GetActiveTake(item))
						if (!strcmp((char*)GetSetMediaItemTakeInfo(tk, "P_NAME", NULL), tkname))
						{
							DeleteTrackMediaItem(tt, item);
							cnt++;
						}
	return !!cnt;
}

void SNM_GetSelectedItems(ReaProject* _proj, WDL_PtrList<MediaItem>* _items, bool _onSelTracks)
{
	int count = _items ? CountTracks(_proj) : 0;
	for (int i=1; i <= count; i++) // skip master
		if (MediaTrack* tr = SNM_GetTrack(_proj, i))
			if (!_onSelTracks || GetMediaTrackInfo_Value(tr, "I_SELECTED"))
				for (int j=0; j < GetTrackNumMediaItems(tr); j++)
					if (MediaItem* item = GetTrackMediaItem(tr,j))
						if (*(bool*)GetSetMediaItemInfo(item, "B_UISEL", NULL))
							_items->Add(item);
}

bool SNM_SetSelectedItems(ReaProject* _proj, WDL_PtrList<MediaItem>* _items, bool _onSelTracks) // primitive func
{
	bool updated = false;
	int count = _items && _items->GetSize() ? CountTracks(_proj) : 0;
	for (int i=1; i <= count; i++) // skip master
		if (MediaTrack* tr = SNM_GetTrack(_proj, i))
			if (!_onSelTracks || GetMediaTrackInfo_Value(tr, "I_SELECTED"))
				for (int j=0; j < GetTrackNumMediaItems(tr); j++)
					if (MediaItem* item = GetTrackMediaItem(tr, j))
						for (int k=0; k < _items->GetSize(); k++)
							if (_items->Get(k) == item)
								if (*(bool*)GetSetMediaItemInfo(item, "B_UISEL", NULL) == false) {
									GetSetMediaItemInfo(item, "B_UISEL", &g_bTrue);
									updated = true;
								}
	return updated;
}

bool SNM_ClearSelectedItems(ReaProject* _proj, bool _onSelTracks) // primitive func
{
	bool updated = false;
	int count = CountTracks(_proj);
	for (int i=1; i <= count; i++) // skip master
		if (MediaTrack* tr = SNM_GetTrack(_proj, i))
			if (!_onSelTracks || GetMediaTrackInfo_Value(tr, "I_SELECTED"))
				for (int j=0; j < GetTrackNumMediaItems(tr); j++)
					if (MediaItem* item = GetTrackMediaItem(tr, j))
						if (*(bool*)GetSetMediaItemInfo(item, "B_UISEL", NULL)) {
							GetSetMediaItemInfo(item, "B_UISEL", &g_bFalse);
							updated = true;
						}
	return updated;
}

bool IsItemInInterval(MediaItem* _item, double _pos1, double _pos2, bool _inclusive)
{
	if (_item)
	{
		double pos = *(double*)GetSetMediaItemInfo(_item,"D_POSITION",NULL);
		double end = pos + *(double*)GetSetMediaItemInfo(_item,"D_LENGTH",NULL);
		if (_inclusive)
		{
			if ((pos >= _pos1 && pos <= _pos2) && // starts within?
				(end >= _pos1 && end <= _pos2))   // ends within?
				return true;
		}
		else
		{
			if ((pos <= _pos1 && end >= _pos2) || // larger?
				(pos >= _pos1 && pos <= _pos2) || // starts within?
				(end >= _pos1 && end <= _pos2))   // ends within?
				return true;
		}
	}
	return false;
}

bool GetItemsInInterval(WDL_PtrList<void>* _items, double _pos1, double _pos2, bool _inclusive)
{
	if (_items)
	{
		_items->Empty();
		for (int i=1; i <= GetNumTracks(); i++) // skip master
			if (MediaTrack* tr = CSurf_TrackFromID(i, false))
				for (int j=0; j < GetTrackNumMediaItems(tr); j++)
					if (MediaItem* item = GetTrackMediaItem(tr,j))
						if (IsItemInInterval(item, _pos1, _pos2, _inclusive))
							_items->Add(item);
	}
	return (_items && _items->GetSize());
}

bool GenerateItemsInInterval(WDL_PtrList<void>* _items, double _pos1, double _pos2, const char* tkname)
{
	int cnt=0;
	if (_items) _items->Empty();
	for (int i=1; i <= GetNumTracks(); i++) // skip master
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
			if (MediaItem* item=CreateNewMIDIItemInProj(tr, _pos1, _pos2, NULL))
			{
				if (_items) _items->Add(item);
				if (tkname)
				if (MediaItem_Take* tk=GetActiveTake(item))
					GetSetMediaItemTakeInfo(tk, "P_NAME", (void*)tkname);
				cnt++;
			}
	return !!cnt;
}

void GetAllItemPointers(WDL_PtrList<void>* _items)
{
	if (_items)
	{
		_items->Empty();
		for (int i=1; i <= GetNumTracks(); i++) // skip master
			if (MediaTrack* tr = CSurf_TrackFromID(i, false))
				for (int j=0; j < GetTrackNumMediaItems(tr); j++)
					if (MediaItem* item = GetTrackMediaItem(tr,j))
						_items->Add(item);
	}
}

void DiffItemPointers(WDL_PtrList<void>* _oldItemsIn, WDL_PtrList<void>* _newItemsOut)
{
	if (_oldItemsIn && _newItemsOut)
	{
		GetAllItemPointers(_newItemsOut);
		for (int j=_newItemsOut->GetSize(); j >= 0; j--)
			for (int k=0; k < _oldItemsIn->GetSize(); k++)
				if (_oldItemsIn->Get(k) == _newItemsOut->Get(j))
					_newItemsOut->Delete(j);
	}
}

// ovverdies ApplyNudge() so that source items remain at their original positions
// note: callers must surround this func with Undo_BeginBlockX/Undo_EndBlockX
// _undoTitle: NULL == no undo point
// _newItemsOut: optionnal
bool DupSelItems(const char* _undoTitle, double _nudgePos, WDL_PtrList<void>* _newItemsOut)
{
	bool updated=false;

	if (_undoTitle)
		Undo_BeginBlock2(NULL); // cannot use Undo_OnStateChangeEx2(NULL, ) because of ApplyNudge()

	WDL_PtrList<MediaItem> items;
	SNM_GetSelectedItems(NULL, &items);

	WDL_PtrList<void> tmpItems;
	GetAllItemPointers(&tmpItems);

	if (ApplyNudge(NULL, 0, 5, 1, _nudgePos, false, 1))
	{
		updated=true;

		WDL_PtrList<void> newItems;
		DiffItemPointers(&tmpItems, &newItems);
		if (newItems.GetSize() == items.GetSize())
			for (int i=0; i < newItems.GetSize(); i++)
				if (MediaItem* newItem = (MediaItem*)newItems.Get(i))
				{
					double oldPos = *(double*)GetSetMediaItemInfo(newItem, "D_POSITION", NULL);
					double newPos = *(double*)GetSetMediaItemInfo(items.Get(i), "D_POSITION", NULL);
					GetSetMediaItemInfo(newItem, "D_POSITION", &newPos);
					GetSetMediaItemInfo(items.Get(i), "D_POSITION", &oldPos);
					if (_newItemsOut && _newItemsOut->Find(newItem)<0)
						_newItemsOut->Add(newItem);
				}
	}

	if (_undoTitle) {
		UpdateArrange();
		Undo_EndBlock2(NULL, _undoTitle, UNDO_STATE_ALL);
	}
	return updated;
}


///////////////////////////////////////////////////////////////////////////////
// Split
///////////////////////////////////////////////////////////////////////////////

void SplitMidiAudio(COMMAND_T* _ct)
{
	bool updated = false;
	for (int i=1; i <= GetNumTracks(); i++) // skip master
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
			for (int j=0; j < GetTrackNumMediaItems(tr); j++)
				if (MediaItem* item = GetTrackMediaItem(tr,j))
					if (*(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
					{
						double pos = *(double*)GetSetMediaItemInfo(item,"D_POSITION",NULL);
						double end = pos + *(double*)GetSetMediaItemInfo(item,"D_LENGTH",NULL);
						bool toBeSplitted = (GetCursorPositionEx(NULL) > pos && GetCursorPositionEx(NULL) < end);

						if (!updated && toBeSplitted)
							Undo_BeginBlock2(NULL);

						updated |= toBeSplitted;

						if (toBeSplitted)
						{
							bool split=false;
							if (MediaItem_Take* tk = GetActiveTake(item))
							{
								if (PCM_source* pcm = (PCM_source*)GetSetMediaItemTakeInfo(tk, "P_SOURCE", NULL))
								{
									if (pcm->GetFileName()) // valid src ? (rmk: "" for in-project)
										split = (!strcmp(pcm->GetType(), "MIDI") || !strcmp(pcm->GetType(), "MIDIPOOL"));
									else // v3 empty take
										split = true;

									// "split prior zero crossing" in all other cases
									// => includes all sources: "WAVE", "VORBIS", "SECTION",..
									if (!split)
										Main_OnCommand(40792,0);
									else
										SplitMediaItem(item, GetCursorPositionEx(NULL));
								}
							}
							else // v4 empty takes are null
								SplitMediaItem(item, GetCursorPositionEx(NULL));
						}
					}

	if (updated)
	{
		UpdateArrange();
		// hard coded undo label: action name too long + consistent with
		// the unique native wording (whatever is the split action)
		Undo_EndBlock2(NULL, __LOCALIZE("Split selected items","sws_undo"), UNDO_STATE_ALL);
	}
}

void SmartSplitMidiAudio(COMMAND_T* _ct)
{
	double t1, t2;
	GetSet_LoopTimeRange(false, false, &t1, &t2, false);
	if (AreThereSelItemsInTimeSel() || (t1 != t2 && !CountSelectedMediaItems(NULL)))
		Main_OnCommand(40061, 0); // split at time sel
	else
		SplitMidiAudio(_ct);
}

#ifdef _SNM_MISC 
//  Deprecated: contrary to their native versions, the following code was spliting selected items *and only them*, 
//  see http://forum.cockos.com/showthread.php?t=51547.
//  Due to REAPER v3.67's new native pref "If no items are selected, some split/trim/delete actions affect all items at the edit cursor", 
//  those actions are less useful: they would still split only selected items, even if that native pref is ticked. 
//  Also removed because of the spam in the action list (many split actions).
void SplitSelectedItems(COMMAND_T* _ct) {
	if (CountSelectedMediaItems(NULL))
		Main_OnCommand((int)_ct->user, 0);
}
#endif

void GoferSplitSelectedItems(COMMAND_T* _ct)
{
	if (CountSelectedMediaItems(NULL))
	{
		Undo_BeginBlock2(NULL);
		Main_OnCommand(40513, 0); // move edit cursor to mouse cursor (obey snapping)
		Main_OnCommand(40757, 0); // split at edit cursor (no selection change)
		Undo_EndBlock2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL);
	}
}

// primitive (no undo point)
bool SplitSelectItemsInInterval(MediaTrack* _tr, double _pos1, double _pos2, WDL_PtrList<void>* _newItemsOut)
{
	bool updated = false;

	// new items might be created during the loop!
	for (int j=0; _tr && j < GetTrackNumMediaItems(_tr); j++)
	{
		if (MediaItem* item = GetTrackMediaItem(_tr, j))
		{
			if (MediaItem* newItem = SplitMediaItem(item, _pos1)) {
				updated=true;
				if (_newItemsOut)
					_newItemsOut->Add(newItem);
			}
			if (MediaItem* newItem = SplitMediaItem(item, _pos2)) {
				updated=true;
				if (_newItemsOut)
					_newItemsOut->Add(newItem);
			}

			double pos = *(double*)GetSetMediaItemInfo(item,"D_POSITION",NULL);
			double end = pos + *(double*)GetSetMediaItemInfo(item,"D_LENGTH",NULL);
			bool sel = *(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL);
			if ((pos+SNM_FUDGE_FACTOR) >= _pos1 && (end-SNM_FUDGE_FACTOR) <= _pos2) 
			{
				if (!sel) {
					updated = true;
					GetSetMediaItemInfo(item,"B_UISEL",&g_bTrue);
				}
			}
			else if (sel) {
				updated = true;
				GetSetMediaItemInfo(item,"B_UISEL",&g_bFalse);
			}
		}
	}
	return updated;
}

// !_undoTitle: use as a primitive (no undo point, no ui refresh)
// _selTracks: true split in sel tracks only, false: split all tracks
bool SplitSelectItemsInInterval(const char* _undoTitle, double _pos1, double _pos2, bool _selTracks, WDL_PtrList<void>* _newItemsOut)
{
	bool updated = false;
  
	PreventUIRefresh(1);
	for (int i=1; i <= GetNumTracks(); i++) // skip master
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
			if (!_selTracks || GetMediaTrackInfo_Value(tr, "I_SELECTED"))
				updated |= SplitSelectItemsInInterval(tr, _pos1, _pos2, _newItemsOut);
	PreventUIRefresh(-1);

	if (updated && _undoTitle)
	{
		UpdateArrange();
		Undo_OnStateChangeEx2(NULL, _undoTitle, UNDO_STATE_ALL, -1);
	}
	return updated;
}

void SplitSelectAllItemsInRegion(COMMAND_T* _ct)
{
	double cursorPos = GetCursorPositionEx(NULL);
	int x=0; double dPos, dEnd; bool isRgn;
	while ((x = EnumProjectMarkers2(NULL, x, &isRgn, &dPos, &dEnd, NULL, NULL)))
	{
		if (isRgn && cursorPos >= dPos && cursorPos <= dEnd) {
			SplitSelectItemsInInterval(SWS_CMD_SHORTNAME(_ct), dPos, dEnd);
			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// Takes
///////////////////////////////////////////////////////////////////////////////

WDL_PtrList_DOD<WDL_FastString> g_takesClipoard;

void CopyCutTakes(COMMAND_T* _ct)
{
	bool updated = false;
	g_takesClipoard.Empty(true);

	for (int i=1; i <= GetNumTracks(); i++) // skip master
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
			for (int j=0; j < GetTrackNumMediaItems(tr); j++)
				if (MediaItem* item = GetTrackMediaItem(tr,j))
					if (*(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
					{
						SNM_TakeParserPatcher p(item, CountTakes(item));
						WDL_FastString* tkChunk = g_takesClipoard.Add(new WDL_FastString);
						int activeTake = *(int*)GetSetMediaItemInfo(item, "I_CURTAKE", NULL);
						if (p.GetTakeChunk(activeTake, tkChunk))
						{
							// cut take?
							if ((int)_ct->user && p.RemoveTake(activeTake))
							{
								p.Commit();
								DeleteMediaItemIfNeeded(item); // do not leave an impty item!
								updated = true;
							}
						}
					}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}

void PasteTakes(COMMAND_T* _ct)
{
	bool updated = false;
	int idxClipboard = 0;
	for (int i=1; i <= GetNumTracks(); i++) // skip master
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
			for (int j=0; j < GetTrackNumMediaItems(tr); j++)
				if (MediaItem* item = GetTrackMediaItem(tr,j))
					if (*(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
					{
						SNM_TakeParserPatcher p(item, CountTakes(item));
						int activeTake = *(int*)GetSetMediaItemInfo(item, "I_CURTAKE", NULL) + (int)_ct->user;
						updated |= (p.InsertTake(activeTake, g_takesClipoard.Get(idxClipboard++)) >= 0);
					}
	if (updated)
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
}


///////////////////////////////////////////////////////////////////////////////
// Take lanes: clear take, activate lanes, ...
///////////////////////////////////////////////////////////////////////////////

//JFB!! TODO: replace Padre's MidiItemProcessor which has bugs, see MidiItemProcessor::getMidiEventsList()
bool IsEmptyMidi(MediaItem_Take* _take)
{
	bool emptyMidi = false;
	if (_take) // a v4 empty take isn't a empty midi take!
	{
		MidiItemProcessor p("S&M");
		if (_take && p.isMidiTake(_take))
		{
			MIDI_eventlist* evts = MIDI_eventlist_Create();
			if(p.getMidiEventsList(_take, evts))
			{
				int pos=0;
				emptyMidi = !evts->EnumItems(&pos);
				MIDI_eventlist_Destroy(evts);
			}
		}
	}
	return emptyMidi;
}

void SetEmptyTakeChunk(WDL_FastString* _chunk, int _recPass, int _color, bool _v4style)
{
	// v4 empty take (but w/o take color)
	if (_v4style) 
	{
		_chunk->Set("TAKE NULL\n");
	}
	// v3 empty take style (take with empty source)
	else
	{
		_chunk->Set("TAKE\n");
		_chunk->Append("NAME \"\"\n");
		_chunk->Append("VOLPAN 1.000000 0.000000 1.000000 -1.000000\n");
		_chunk->Append("SOFFS 0.00000000000000\n");
		_chunk->Append("PLAYRATE 1.00000000000000 1 0.00000000000000 -1\n");
		_chunk->Append("CHANMODE 0\n");
		if (_color > 0)
			_chunk->AppendFormatted(32, "TAKECOLOR %d\n", _color);
		if (_recPass > 0)
			_chunk->AppendFormatted(32, "RECPASS %d\n", _recPass);
		_chunk->Append("<SOURCE EMPTY\n");
		_chunk->Append(">\n");
	}
}

#ifdef _SNM_MISC
// !_undoTitle: no undo point
// _mode: 0=for selected tracks, 1=for selected items
int BuildLanes(const char* _undoTitle, int _mode) 
{
	int updates = RemoveEmptyTakes((const char*)NULL, true, false, (_mode==0), (_mode==1));
	bool badRecPass = false;
	for (int i = 1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && (_mode || GetMediaTrackInfo_Value(tr, "I_SELECTED")))
		{
			WDL_PtrList<void> items;
			WDL_IntKeyedArray<int> recPassColors; 
			WDL_PtrList_DeleteOnDestroy<int> itemRecPasses(free);
			int maxRecPass = -1;

			// Get items, record pass ids & take colors in one go
			for (int j = 0; tr && j < GetTrackNumMediaItems(tr); j++)
			{
				MediaItem* item = GetTrackMediaItem(tr,j);
				if (item && (!_mode || GetMediaItemInfo_Value(item,"B_UISEL")))
				{
					int* recPasses = new int[SNM_RECPASSPARSER_MAX_TAKES];
					int takeColors[SNM_RECPASSPARSER_MAX_TAKES];

					SNM_RecPassParser p(item, CountTakes(item));
					int itemMaxRecPass = p.GetMaxRecPass(recPasses, takeColors); 
					maxRecPass = max(itemMaxRecPass, maxRecPass);

					// 1st loop to check rec pass ids
					bool badRecPassItem = false;
					for (int k=0; !badRecPassItem && k < CountTakes(item) && k < SNM_RECPASSPARSER_MAX_TAKES; k++)
						badRecPassItem = (recPasses[k] == 0);

					badRecPass |= badRecPassItem;
					if (!badRecPassItem)
					{
						items.Add(item);
						itemRecPasses.Add(recPasses);
						// 2nd loop to *update* colors by rec pass id
						for (int k=0; k < CountTakes(item) && k < SNM_RECPASSPARSER_MAX_TAKES; k++)
								recPassColors.Insert(recPasses[k], takeColors[k]);
					}
				}
			}

			WDL_PtrList_DeleteOnDestroy<SNM_TakeParserPatcher> ps; // auto commit on destroy!
			for (int j = 0; j < items.GetSize(); j++)
			{
				MediaItem* item = (MediaItem*)items.Get(j);
				SNM_TakeParserPatcher* p = new SNM_TakeParserPatcher(item, CountTakes(item));
				ps.Add(p);

				WDL_PtrList_DeleteOnDestroy<WDL_FastString> chunks;
				for (int k=0; k < maxRecPass; k++)
					chunks.Add(new WDL_FastString());

				// Optimz: use a cache for take chunks
				WDL_IntKeyedArray<char*> oldChunks(freecharptr);
				int tkIdx = 0;
				while (tkIdx < CountTakes(item)) {
					WDL_FastString c;
					oldChunks.Insert(tkIdx, _strdup(p->GetTakeChunk(tkIdx, &c) ? c.Get() : ""));
					tkIdx++;
				}

				// Create 'maxRecPass' take chunks (ordered, stuffing with empty takes)
				int* recPasses = itemRecPasses.Get(j);
				for (int k=1; k <= maxRecPass; k++)
				{
					tkIdx = 0;
					bool found = false;
					while (tkIdx < CountTakes(item))
					{
						if (recPasses[tkIdx] == k) {
							chunks.Get(k-1)->Set(oldChunks.Get(tkIdx));
							found = true;
							break;
						}
						tkIdx++;
					}

					if (!found)
						SetEmptyTakeChunk(chunks.Get(k-1), k, recPassColors.Get(k));
				}

				// insert re-ordered takes..
				int insertPos=-1, k2=0;
				for (int k=0; k < chunks.GetSize(); k++)
				{
					if (recPassColors.Get(k+1, -1) != -1) //.. but skip empty lanes
					{
						insertPos = p->InsertTake(k2, chunks.Get(k), insertPos);
						if (insertPos > 0) {
							updates++;
							k2++;
						}
					}
				}

				// remove old takes
				if (updates)
					p->GetChunk()->DeleteSub(insertPos, strlen((char*)ps.Get(j)->GetChunk()->Get()+insertPos) - 2); //-2 for >\n
			}
		}
	}
	if (updates > 0) {
		UpdateArrange();
		if (_undoTitle)
			Undo_OnStateChangeEx2(NULL, _undoTitle, UNDO_STATE_ALL, -1);
	}
	return (badRecPass ? -1 : updates);
}
#endif

// primitive (no undo)
bool RemoveEmptyTakes(MediaTrack* _tr, bool _empty, bool _midiEmpty, bool _trSel, bool _itemSel)
{
	bool updated = false;
	for (int j = 0; _tr && j < GetTrackNumMediaItems(_tr); j++)
	{
		if (!_trSel || GetMediaTrackInfo_Value(_tr, "I_SELECTED"))
		{
			MediaItem* item = GetTrackMediaItem(_tr,j);
			if (item && (!_itemSel || GetMediaItemInfo_Value(item,"B_UISEL")))
			{
				SNM_TakeParserPatcher p(item, CountTakes(item));
				int k=0, kOriginal=0;
				while (k < p.CountTakesInChunk())
				{
					if ((_empty && p.IsEmpty(k)) ||	(_midiEmpty && IsEmptyMidi(GetTake(item, kOriginal))))
					{
						bool removed = p.RemoveTake(k);
						if (removed) k--; //++ below!
						updated |= removed;
					}
					k++;
					kOriginal++;
				}

				// Removes the item if needed
				if (p.CountTakesInChunk() == 0)
				{
					p.CancelUpdates(); // prevent a useless SNM_ChunkParserPatcher commit
					bool removed = DeleteTrackMediaItem(_tr, item);
					if (removed) j--; 
					updated |= removed;
				}
				// in case we removed empty *MIDI* items but the only remaining takes
				// are empty (i.e. NULL) ones
				else if (p.Commit() && DeleteMediaItemIfNeeded(item))
				{
					j--;
					updated = true;
				}
			}
		}
	}
	return updated;
}

// no undo point if !_undoTitle
bool RemoveEmptyTakes(const char* _undoTitle, bool _empty, bool _midiEmpty, bool _trSel, bool _itemSel)
{
	bool updated = false;
	for (int i=0; i < GetNumTracks(); i++)
		updated |= RemoveEmptyTakes(CSurf_TrackFromID(i+1,false), _empty, _midiEmpty, _trSel, _itemSel);
	if (updated)
	{
		UpdateArrange();
		if (_undoTitle)
			Undo_OnStateChangeEx2(NULL, _undoTitle, UNDO_STATE_ALL, -1);
	}
	return updated;
}

void ClearTake(COMMAND_T* _ct)
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
				int activeTake = *(int*)GetSetMediaItemInfo(item, "I_CURTAKE", NULL);

				SNM_TakeParserPatcher p(item, CountTakes(item));
				int pos, len;
				WDL_FastString emptyTk("TAKE NULL SEL\n");
				if (p.GetTakeChunkPos(activeTake, &pos, &len))
				{
					updated |= p.ReplaceTake(pos, len, &emptyTk);

					// empty takes only => remove the item
					if (!strstr(p.GetChunk()->Get(), "\nNAME "))
					{	
						p.CancelUpdates(); // prevent a useless SNM_ChunkParserPatcher commit
						if (DeleteTrackMediaItem(tr, item)) {
							j--; 
							updated = true;
						}
					}
				}
			}
		}
	}
	if (updated) {
		UpdateArrange();
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
	}
}

#ifdef _SNM_MISC // deprecated: native actions "Rotate take lanes forward/backward" added in REAPER v3.67
void MoveTakes(COMMAND_T* _ct)
{
	bool updated = false;
	int dir = (int)_ct->user;
	for (int i = 1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			for (int j = 0; tr && j < GetTrackNumMediaItems(tr); j++)
			{
				MediaItem* item = GetTrackMediaItem(tr,j);
				int newActive = 0;
				if (item && *(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
				{		
					SNM_TakeParserPatcher p(item, CountTakes(item));
					int active = *(int*)GetSetMediaItemInfo(item, "I_CURTAKE", NULL);
					int nbTakes = CountTakes(item);
					if (dir == 1)
					{
						newActive = (active == (nbTakes-1) ? 0 : (active+1));
						// Remove last take and re-add it as 1st one
						WDL_FastString chunk;
						updated = p.RemoveTake(nbTakes-1, &chunk);
						if (updated)
							p.InsertTake(0, &chunk);
					}
					else if (dir == -1)
					{
						newActive = (!active ? (nbTakes-1) : (active-1));
						// Remove 1st take and re-add it as last one
						WDL_FastString chunk;
						updated = p.RemoveTake(0, &chunk);
						if (updated)
							p.AddLastTake(&chunk);
					}
				}
				GetSetMediaItemInfo(item, "I_CURTAKE", &newActive);
			}
		}
	}
	if (updated) {
		UpdateArrange();
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
	}
}
#endif

void MoveActiveTake(COMMAND_T* _ct)
{
	bool updated = false;
	int dir = (int)_ct->user;
	for (int i = 1; i <= GetNumTracks(); i++) // skip master
	{
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
		{
			for (int j = 0; j < GetTrackNumMediaItems(tr); j++)
			{
				MediaItem* item = GetTrackMediaItem(tr,j);
				int active = *(int*)GetSetMediaItemInfo(item, "I_CURTAKE", NULL);
				if (item && *(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
				{
					int initialNbbTakes = CountTakes(item);

					// cycle (last<->first) ?
					bool swapFirstLast = 
						((dir == 1 && active == (initialNbbTakes-1)) ||
						(dir == -1 && !active));

					WDL_FastString removedChunk;
					SNM_TakeParserPatcher p(item, CountTakes(item));
					if (swapFirstLast) {
						updated |= p.RemoveTake(dir == -1 ? 0 : (initialNbbTakes-1), &removedChunk);
						updated |= (p.InsertTake(dir == -1 ? initialNbbTakes : 0, &removedChunk) > 0);
						active = (dir == 1 ? 0 : (dir == -1 ? (initialNbbTakes-1) : 0));
					}
					else {
						updated |= p.RemoveTake(active, &removedChunk);
						updated |= (p.InsertTake(active+dir, &removedChunk) > 0);
						active += dir;
					}
				}
				GetSetMediaItemInfo(item, "I_CURTAKE", &active);
			}
		}
	}
	if (updated) {
		UpdateArrange();
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
	}
}

#ifdef _SNM_MISC
// ifdef'd out => removed __LOCALIZE() to avoid useless langpack entry
void BuildLanes(COMMAND_T* _ct) {
	if (BuildLanes(SWS_CMD_SHORTNAME(_ct), (int)_ct->user) < 0)
		MessageBox(GetMainHwnd(), "Some items were ignored, probable causes:\n- Items not recorded or recorded before REAPER v3.66 (no record pass id)\n- Imploded takes with duplicated record pass ids", __LOCALIZE("S&M - Warning","sws_mbox"), MB_OK);
}
#endif

void ActivateLaneFromSelItem(COMMAND_T* _ct)
{
	bool updated = false;
	for (int i=1; i <= GetNumTracks(); i++) // skip master
	{
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
		{
			// get the 1st selected item and its active take
			int active = -1;
			for (int j = 0; active == -1 && j < GetTrackNumMediaItems(tr); j++)
			{
				MediaItem* item = GetTrackMediaItem(tr,j);
				if (item && *(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
				{
					active = *(int*)GetSetMediaItemInfo(item,"I_CURTAKE",NULL);
					break;
				}
			}

			// activate take for all items
			if (active >= 0)
			{
				for (int j=0; j < GetTrackNumMediaItems(tr); j++)
				{
					MediaItem* item = GetTrackMediaItem(tr,j);
					// "active" validity check relies on GetSetMediaItemInfo()
					if (item && active < CountTakes(item) && (*(int*)GetSetMediaItemInfo(item,"I_CURTAKE",NULL)) != active)
					{
						GetSetMediaItemInfo(item,"I_CURTAKE",&active);
						updated = true;
					}
				}
			}
		}
	}
	if (_ct && updated) {
		UpdateArrange();
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
	}
}

void ActivateLaneUnderMouse(COMMAND_T* _ct)
{
	Undo_BeginBlock2(NULL);

	WDL_PtrList<MediaItem> selItems;
	SNM_GetSelectedItems(NULL, &selItems);

	PreventUIRefresh(1);
	Main_OnCommand(40528,0); // select item under mouse cursor
	Main_OnCommand(41342,0); // activate take under mouse cursor
	ActivateLaneFromSelItem(NULL);
	SNM_ClearSelectedItems(NULL);
	SNM_SetSelectedItems(NULL, &selItems);
	PreventUIRefresh(-1);
	
	UpdateArrange();
	Undo_EndBlock2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL);
}

void RemoveEmptyTakes(COMMAND_T* _ct) {
	RemoveEmptyTakes(SWS_CMD_SHORTNAME(_ct), true, false);
}

void RemoveEmptyMidiTakes(COMMAND_T* _ct) {
	RemoveEmptyTakes(SWS_CMD_SHORTNAME(_ct), false, true);
}

void RemoveAllEmptyTakes(COMMAND_T* _ct) {
	RemoveEmptyTakes(SWS_CMD_SHORTNAME(_ct), true, true);
}

// note: no undo due to file deletion
bool DeleteTakeAndMedia(int _mode)
{
	bool deleteFileOK = true, cancel = false;
	WDL_StringKeyedArray<int> removeFiles(false);
	for (int i=1; !cancel && i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		WDL_PtrList<void> removedItems; 
		for (int j=0; !cancel && tr && j < GetTrackNumMediaItems(tr); j++)
		{
			MediaItem* item = GetTrackMediaItem(tr,j);
			if (item && *(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
			{
				int originalTkIdx = 0;
				int originalActiveTkIdx = *(int*)GetSetMediaItemInfo(item, "I_CURTAKE", NULL);
				int nbRemainingTakes = GetMediaItemNumTakes(item);
				for (int k = 0; k < GetMediaItemNumTakes(item); k++) // nb of takes changes here!
				{
					MediaItem_Take* tk = GetMediaItemTake(item,k);
					if ((_mode == 1 || _mode == 2 || // all takes
						((_mode == 3 || _mode == 4) && originalActiveTkIdx == k))) // active take only
					{
						char tkName[SNM_MAX_PATH] = "";
						if (tk) lstrcpyn(tkName, (char*)GetSetMediaItemTakeInfo(tk,"P_NAME",NULL), sizeof(tkName));

						char tkDisplayName[SNM_MAX_PATH] = "[empty take]"; // no localization here..
						PCM_source* pcm = tk ? (PCM_source*)GetSetMediaItemTakeInfo(tk,"P_SOURCE",NULL) : NULL;
						if (pcm && pcm->GetFileName())
						{
							if (*pcm->GetFileName())
								lstrcpyn(tkDisplayName, pcm->GetFileName(), sizeof(tkDisplayName));
							else
								lstrcpyn(tkDisplayName, tkName, sizeof(tkDisplayName));
						}

						// not already removed ?
						int rc = removeFiles.Get(tkDisplayName, -1);
						if (rc == -1)
						{
							if (_mode == 1 || _mode == 3)
							{
								char buf[SNM_MAX_PATH];
								if (pcm && pcm->GetFileName() && *pcm->GetFileName() && _stricmp(GetFileExtension(pcm->GetFileName()), "rpp"))
									snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("[Track %d, item %d, take %d] Delete take '%s' and its media file '%s'?","sws_mbox"), i, j+1, originalTkIdx+1, tkName, tkDisplayName);
								else if (pcm && pcm->GetFileName() && *pcm->GetFileName() && !_stricmp(GetFileExtension(pcm->GetFileName()), "rpp"))
									snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("[Track %d, item %d, take %d] Delete take '%s'?\r\nNote: the media file '%s' will not be deleted (project within project)","sws_mbox"), i, j+1, originalTkIdx+1, tkName, tkDisplayName);
								else if (pcm && pcm->GetFileName() && !*(pcm->GetFileName())) 
									snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("[Track %d, item %d, take %d] Delete take '%s' (MIDI in-project)?","sws_mbox"), i, j+1, originalTkIdx+1, tkName);
								else 
									snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("[Track %d, item %d] Delete take %d (empty take) ?","sws_mbox"), i, j+1, originalTkIdx+1); // v3 or v4 empty takes

								rc = MessageBox(GetMainHwnd(), buf, __LOCALIZE("S&M - Delete take and source files (no undo!)","sws_mbox"), MB_YESNOCANCEL);
								if (rc == IDCANCEL) {
									cancel = true;
									break;
								}
							}
							else
								rc = IDYES;
							removeFiles.Insert(tkDisplayName, rc);
						}

						if (rc==IDYES)
						{
							nbRemainingTakes--;
							if (pcm && FileOrDirExists(pcm->GetFileName()) && _stricmp(GetFileExtension(pcm->GetFileName()), "rpp"))
							{
								// set all media offline (yeah, EACH TIME! Fails otherwise: http://github.com/reaper-oss/sws/issues/175#c3)
								Main_OnCommand(40100,0); 
								if (SNM_DeleteFile(pcm->GetFileName(), true))
									SNM_DeletePeakFile(pcm->GetFileName(), true); // no delete check (peaks files can be absent)
								else
									deleteFileOK = false;
							}

							// removes the take (cannot factorize chunk updates here)
							int cntTakes = CountTakes(item);
							SNM_TakeParserPatcher p(item, cntTakes);
							if (cntTakes > 1 && p.RemoveTake(k)) // > 1 because item removed otherwise
							{
								// active tale only?
								if (_mode == 3 || _mode == 4) break;
								else k--; 
							}
						}
					}
					originalTkIdx++;
				}

				if (!nbRemainingTakes)
					removedItems.Add(item);
			}
		}
		
		// if needed, delete items from this track
		for (int j=0;j<removedItems.GetSize();j++)
			DeleteTrackMediaItem(tr, (MediaItem*)removedItems.Get(j));

		removedItems.Empty();
	}
	removeFiles.DeleteAll();
	Main_OnCommand(40101,0); // set all media online
	return deleteFileOK;
}

void DeleteTakeAndMedia(COMMAND_T* _ct) {
	if (!DeleteTakeAndMedia((int)_ct->user))
		MessageBox(GetMainHwnd(), __LOCALIZE("Warning: at least one file could not be deleted.\nTips: are you an administrator? File used by another process?","sws_mbox"), __LOCALIZE("S&M - Delete take and source files","sws_mbox"), MB_OK);
}


///////////////////////////////////////////////////////////////////////////////
// Take envs
///////////////////////////////////////////////////////////////////////////////

int GetPitchTakeEnvRangeFromPrefs()
{
	int range = *ConfigVar<int>("pitchenvrange");
	// "snap to semitones" bit set ?
	if (range > 0xFF)
		range &= 0xFF;
	return min(231, range); // clamps like REAPER does
}

// callers must use UpdateArrange() at some point if it returns true..
bool PatchTakeEnvelopeActVis(MediaItem* _item, int _takeIdx, const char* _envKeyword, const char* _vis, const WDL_FastString* _defaultPoint, bool _reset, bool patchVisibilityOnly)
{
	bool updated = false;
	if (_item)
	{
		SNM_TakeParserPatcher p(_item, CountTakes(_item));

		WDL_FastString takeChunk;
		int tkPos, tklen;
		if (p.GetTakeChunk(_takeIdx, &takeChunk, &tkPos, &tklen))
		{
			bool takeUpdate = false, buildDefaultEnv = false;
			char vis[2] = {'\0','\0'}; 
			*vis = *_vis;

			// env already exists?
			if (strstr(takeChunk.Get(), _envKeyword))
			{
				if (_reset)
				{
					SNM_ChunkParserPatcher ptk(&takeChunk);
					takeUpdate = ptk.RemoveSubChunk(_envKeyword, 1, -1);
					buildDefaultEnv = true;
				}
				else
				{
					// toggle?
					if (!strlen(vis))
					{
						SNM_ChunkParserPatcher ptk(&takeChunk);
						char currentVis[32];
						if (ptk.Parse(SNM_GET_CHUNK_CHAR, 1, _envKeyword, "VIS", 0, 1, (void*)currentVis) > 0)
						{
							// skip if visibility is different from 0 or 1
							if (*currentVis == '1') *vis = '0';
							else if (*currentVis == '0') *vis = '1';
						}
						// just in case..
						if (!vis[0]) 
							return false;
					}

					// prepare the new visibility (in one go)
					{
						SNM_TakeEnvParserPatcher pEnv(&takeChunk);
						if (patchVisibilityOnly)
							pEnv.SetPatchVisibilityOnly(true);
						takeUpdate = pEnv.SetVal(_envKeyword, atoi(vis));
					}
				}
			}
			else
				buildDefaultEnv = true;

			// build a default env, if needed
			if (buildDefaultEnv)
			{
				if (!vis[0]) 
					*vis = '1'; // toggle?
				if (*vis == '1')
				{
					takeChunk.Append("<");
					takeChunk.Append(_envKeyword);
					takeChunk.Append("\nACT ");
					takeChunk.Append(vis);
					takeChunk.Append("\nVIS ");
					takeChunk.Append(vis);
					takeChunk.Append(" 1 1.000000\nLANEHEIGHT 0 0\nARM ");
					takeChunk.Append(vis);
					takeChunk.Append("\nDEFSHAPE 0\n");
					// NF: #1054, obey volume fader scaling pref.
					const ConfigVar<int> volenvrange("volenvrange");
					if (volenvrange && *volenvrange & (1 << 1))
						takeChunk.Append("VOLTYPE 1\n");
					takeChunk.Append(_defaultPoint);
					takeChunk.Append("\n>\n");
					takeUpdate = true;
				}
			}

			// update take (with new visibility)
			if (takeUpdate)
				updated = p.ReplaceTake(tkPos, tklen, &takeChunk);
		}
	}
	return updated;
}

bool PatchTakeEnvelopeActVis(const char* _undoTitle, const char* _envKeyword, const char* _vis, const WDL_FastString* _defaultPoint, bool _reset, bool patchVisibilityOnly) 
{
	bool updated = false;
	for (int i = 1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		for (int j = 0; tr && j < GetTrackNumMediaItems(tr); j++)
		{
			MediaItem* item = GetTrackMediaItem(tr,j);
			if (item && *(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
				updated |= PatchTakeEnvelopeActVis(item, *(int*)GetSetMediaItemInfo(item,"I_CURTAKE",NULL), _envKeyword, _vis, _defaultPoint, _reset, patchVisibilityOnly);
		}
	}

	if (updated)
	{
		UpdateArrange();
		if (_undoTitle)
			Undo_OnStateChangeEx2(NULL, _undoTitle, UNDO_STATE_ALL, -1);
	}
	return updated;
}

void PanTakeEnvelope(COMMAND_T* _ct) 
{
	WDL_FastString defaultPoint("PT 0.000000 ");
	defaultPoint.AppendFormatted(128, "%d.000000 0", (int)_ct->user);
	PatchTakeEnvelopeActVis(SWS_CMD_SHORTNAME(_ct), "PANENV", "1", &defaultPoint, true, false);
}

void BypassUnbypassShowHideTakeVolEnvelope(COMMAND_T* _ct) 
{
	char cVis[2] = ""; // empty means toggle
	int value = (int)_ct->user;
	if (value >= 0 && snprintfStrict(cVis, sizeof(cVis), "%d", value) < 0)
		return;
	WDL_FastString defaultPoint("PT 0.000000 1.000000 0");
	PatchTakeEnvelopeActVis(SWS_CMD_SHORTNAME(_ct), "VOLENV", cVis, &defaultPoint, false, false);
}

void BypassUnbypassShowHideTakePanEnvelope(COMMAND_T* _ct) 
{
	char cVis[2] = ""; // empty means toggle
	int value = (int)_ct->user;
	if (value >= 0 && snprintfStrict(cVis, sizeof(cVis), "%d", value) < 0)
		return;
	WDL_FastString defaultPoint("PT 0.000000 0.000000 0");
	PatchTakeEnvelopeActVis(SWS_CMD_SHORTNAME(_ct), "PANENV", cVis, &defaultPoint, false, false);
}

void BypassUnbypassShowHideTakeMuteEnvelope(COMMAND_T* _ct) 
{
	char cVis[2] = ""; // empty means toggle
	int value = (int)_ct->user;
	if (value >= 0 && snprintfStrict(cVis, sizeof(cVis), "%d", value) < 0)
		return;
	WDL_FastString defaultPoint("PT 0.000000 1.000000 1");
	PatchTakeEnvelopeActVis(SWS_CMD_SHORTNAME(_ct), "MUTEENV", cVis, &defaultPoint, false, false);
}

void BypassUnbypassShowHideTakePitchEnvelope(COMMAND_T* _ct) 
{
	char cVis[2] = ""; // empty means toggle
	int value = (int)_ct->user;
	if (value >= 0 && snprintfStrict(cVis, sizeof(cVis), "%d", value) < 0)
		return;
	WDL_FastString defaultPoint("PT 0.000000 0.000000 0");
	PatchTakeEnvelopeActVis(SWS_CMD_SHORTNAME(_ct), "PITCHENV", cVis, &defaultPoint, false, false);
}

// NF: these *only* show/hide (no bypass/unbypass), #1078
void ShowHideTakeVolEnvelope(COMMAND_T* _ct)
{
	char cVis[2] = ""; // empty means toggle
	int value = (int)_ct->user;
	if (value >= 0 && snprintfStrict(cVis, sizeof(cVis), "%d", value) < 0)
		return;
	WDL_FastString defaultPoint("PT 0.000000 1.000000 0");
	PatchTakeEnvelopeActVis(SWS_CMD_SHORTNAME(_ct), "VOLENV", cVis, &defaultPoint, false, true);
}

void ShowHideTakePanEnvelope(COMMAND_T* _ct)
{
	char cVis[2] = ""; // empty means toggle
	int value = (int)_ct->user;
	if (value >= 0 && snprintfStrict(cVis, sizeof(cVis), "%d", value) < 0)
		return;
	WDL_FastString defaultPoint("PT 0.000000 0.000000 0");
	PatchTakeEnvelopeActVis(SWS_CMD_SHORTNAME(_ct), "PANENV", cVis, &defaultPoint, false, true);
}

void ShowHideTakeMuteEnvelope(COMMAND_T* _ct)
{
	char cVis[2] = ""; // empty means toggle
	int value = (int)_ct->user;
	if (value >= 0 && snprintfStrict(cVis, sizeof(cVis), "%d", value) < 0)
		return;
	WDL_FastString defaultPoint("PT 0.000000 1.000000 1");
	PatchTakeEnvelopeActVis(SWS_CMD_SHORTNAME(_ct), "MUTEENV", cVis, &defaultPoint, false, true);
}

void ShowHideTakePitchEnvelope(COMMAND_T* _ct)
{
	char cVis[2] = ""; // empty means toggle
	int value = (int)_ct->user;
	if (value >= 0 && snprintfStrict(cVis, sizeof(cVis), "%d", value) < 0)
		return;
	WDL_FastString defaultPoint("PT 0.000000 0.000000 0");
	PatchTakeEnvelopeActVis(SWS_CMD_SHORTNAME(_ct), "PITCHENV", cVis, &defaultPoint, false, true);
}

// *** some wrappers for Padre ***
bool ShowTakeEnv(MediaItem_Take* _take, const char* _envKeyword, const WDL_FastString* _defaultPoint)
{
	bool shown = false;
	MediaItem* item = (_take ? GetMediaItemTake_Item(_take) : NULL);
	if (item) 
	{
		int idx = GetTakeIndex(item, _take);
		if (idx >= 0) 
			shown = PatchTakeEnvelopeActVis(item, idx, _envKeyword , "1", _defaultPoint, false, false);
	}
	return shown;
}

bool ShowTakeEnvVol(MediaItem_Take* _take) {
	WDL_FastString defaultPoint("PT 0.000000 1.000000 0");
	return ShowTakeEnv(_take, "VOLENV", &defaultPoint);
}

bool ShowTakeEnvPan(MediaItem_Take* _take) {
	WDL_FastString defaultPoint("PT 0.000000 0.000000 0");
	return ShowTakeEnv(_take, "PANENV", &defaultPoint);
}

bool ShowTakeEnvMute(MediaItem_Take* _take) {
	WDL_FastString defaultPoint("PT 0.000000 1.000000 1");
	return ShowTakeEnv(_take, "MUTEENV", &defaultPoint);
}

bool ShowTakeEnvPitch(MediaItem_Take* _take) {
	WDL_FastString defaultPoint("PT 0.000000 0.000000 0");
	return ShowTakeEnv(_take, "PITCHENV", &defaultPoint);
}


///////////////////////////////////////////////////////////////////////////////
// Toolbar item selection toggles
///////////////////////////////////////////////////////////////////////////////

WDL_PtrList<void> g_toolbarItemSel[SNM_ITEM_SEL_COUNT];
WDL_PtrList<void> g_toolbarItemSelToggle[SNM_ITEM_SEL_COUNT];

void RefreshOffscreenItems()
{
	for(int i=0; i<SNM_ITEM_SEL_COUNT; i++)
		g_toolbarItemSel[i].Empty();

	if (CountSelectedMediaItems(NULL))
	{
		// left/right item sel.
		double pos,len,start_time,end_time;
		bool horizontal = false;

		if (HWND w = GetTrackWnd()) // works on OSX too
		{
			RECT r; GetWindowRect(w, &r);
			//JFB!! -17 = width of the vert. scrollbar, oh well
			GetSet_ArrangeView2(NULL, false, r.left, r.right-17, &start_time, &end_time);
			horizontal = true;
		}

		// up/down item sel.
		WDL_PtrList<MediaTrack> trList;
		GetVisibleTCPTracks(&trList);
		bool vertical = (trList.GetSize() > 0);

		for (int i=1; (horizontal || vertical) && i <= GetNumTracks(); i++) // skip master
		{
			MediaTrack* tr = CSurf_TrackFromID(i, false);
			for (int j = 0; tr && j < GetTrackNumMediaItems(tr); j++)
			{
				MediaItem* item = GetTrackMediaItem(tr,j);
				if (item && *(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
				{
					if (horizontal) 
					{
						pos = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
						if (end_time < pos)
							g_toolbarItemSel[SNM_ITEM_SEL_RIGHT].Add(item);

						len = *(double*)GetSetMediaItemInfo(item, "D_LENGTH", NULL);
						if (start_time > (pos + len))
							g_toolbarItemSel[SNM_ITEM_SEL_LEFT].Add(item);
					}
					if (vertical)
					{
						int minVis=0xFFFF, maxVis=-1;
						for (int k=0; k < trList.GetSize(); k++)
							if (MediaTrack* tr2 = (MediaTrack*)trList.Get(k))
							{
								int trIdx = CSurf_TrackToID(tr2, false);
								// >0: no items on master track..
								if (trIdx > 0 && trIdx < minVis) minVis = trIdx;
								if (trIdx > 0 && trIdx > maxVis) maxVis = trIdx;
							}

						if (minVis <= maxVis)
						{
							MediaTrack* tr = GetMediaItem_Track(item);
							if (tr && trList.Find(tr) == -1)
							{
								int trIdx = CSurf_TrackToID(tr, false);
								if (trIdx <= minVis)
									g_toolbarItemSel[SNM_ITEM_SEL_UP].Add(item);
								else if (trIdx >= maxVis)
									g_toolbarItemSel[SNM_ITEM_SEL_DOWN].Add(item);
							}
						}
					}
				}
			}
		}
	}
}

// deselects offscreen items and reselects those items -on toggle-
bool ToggleOffscreenSelItems(int _dir) // primitive func
{
	bool updated = false;

	int dir1=_dir, dir2=_dir+1;
	if (_dir<0) {
		dir1=0;
		dir2=SNM_ITEM_SEL_COUNT;
	}

	for (int i=dir1; i<dir2; i++)
	{
		bool toggle = false;
		WDL_PtrList<void>* l1 = NULL;
		WDL_PtrList<void>* l2 = NULL;

		if (g_toolbarItemSel[i].GetSize()) 
		{
			l1 = &(g_toolbarItemSel[i]);
			l2 = &(g_toolbarItemSelToggle[i]);
		}
		else if (g_toolbarItemSelToggle[i].GetSize())
		{
			l2 = &(g_toolbarItemSel[i]);
			l1 = &(g_toolbarItemSelToggle[i]);
			toggle = true;
		}

		if (l1 && l2) 
		{
			l2->Empty();
			for (int j=0; j < l1->GetSize(); j++)
			{
				GetSetMediaItemInfo((MediaItem*)l1->Get(j), "B_UISEL", &toggle);
				l2->Add((MediaItem*)l1->Get(j));
				updated = true;
			}
			l1->Empty();

			// in case auto-refresh toolbar option is off..
			RefreshToolbar(SWSGetCommandID(ToggleOffscreenSelItems, i));
		}
	}
	return updated;
}

void ToggleOffscreenSelItems(COMMAND_T* _ct)
{
	int dir = (int)_ct->user;

	RefreshOffscreenItems();

	PreventUIRefresh(1);
	bool updated = ToggleOffscreenSelItems(dir);
	PreventUIRefresh(-1);
  
	if (updated)
	{
		UpdateArrange();
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
	}
}

// returns the toggle state as fast as possible:
// background job done in RefreshOffscreenItems() 
int HasOffscreenSelItems(COMMAND_T* _ct)
{
	// force refresh if not auto
	if (!g_SNM_ToolbarRefresh) 
		RefreshOffscreenItems();

	int dir = (int)_ct->user;
	if (dir<0)
	{
		for(dir=0; dir<SNM_ITEM_SEL_COUNT; dir++)
			if (g_toolbarItemSel[dir].GetSize() > 0)
				return true;
	}
	else
		return (g_toolbarItemSel[dir].GetSize() > 0);
	return false;
}

// deselects offscreen items
void UnselectOffscreenItems(COMMAND_T* _ct)
{
	RefreshOffscreenItems();

	bool updated = false;
	PreventUIRefresh(1);
	for(int i=0; i<SNM_ITEM_SEL_COUNT; i++)
		if (g_toolbarItemSel[i].GetSize() > 0)
			updated |= ToggleOffscreenSelItems(i);
	PreventUIRefresh(-1);

	if (updated) {
		UpdateArrange();
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
	}
}


///////////////////////////////////////////////////////////////////////////////
// Others
///////////////////////////////////////////////////////////////////////////////

// scroll to item, no undo!
void ScrollToSelItem(MediaItem* _item)
{
	if (_item)
	{
		// horizontal scroll to selected item
		PreventUIRefresh(1);
		double curPos = GetCursorPositionEx(NULL);
		SetEditCurPos2(NULL, *(double*)GetSetMediaItemInfo(_item, "D_POSITION", NULL), true, false);
		SetEditCurPos2(NULL, curPos, false, false);
		PreventUIRefresh(-1);

		// vertical scroll to selected item
		if (MediaTrack* tr = GetMediaItem_Track(_item))
			ScrollTrack(tr, true, false);
	}
}

void ScrollToSelItem(COMMAND_T* _ct) {
	ScrollToSelItem(GetSelectedMediaItem(NULL, 0));
}

// pan take
void SetPan(COMMAND_T* _ct)
{
	bool updated = false;
	double value = (double)_ct->user/100;
	for (int i=1; i <= GetNumTracks(); i++) // skip master
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		for (int j=0; tr && j < GetTrackNumMediaItems(tr); j++)
		{
			MediaItem* item = GetTrackMediaItem(tr,j);
			if (item && *(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
			{
				if (MediaItem_Take* tk = GetActiveTake(item))
				{
					double curValue = *(double*)GetSetMediaItemTakeInfo(tk, "D_PAN", NULL);
					if (fabs(curValue - value) > 0.0001) {
						GetSetMediaItemTakeInfo(tk, "D_PAN", &value);
						updated = true;
					}
				}
			}
		}
	}
	if (updated)
	{
		Undo_OnStateChangeEx2(NULL, SWS_CMD_SHORTNAME(_ct), UNDO_STATE_ALL, -1);
		UpdateTimeline();
	}	
}

void OpenMediaPathInExplorerFinder(COMMAND_T*)
{
	if (!CountSelectedMediaItems(NULL))
		return;

	for (int i=1; i <= GetNumTracks(); i++) // skip master
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
			for (int j=0; j < GetTrackNumMediaItems(tr); j++)
				if (MediaItem* item = GetTrackMediaItem(tr,j))
					if (*(bool*)GetSetMediaItemInfo(item,"B_UISEL",NULL))
						if (MediaItem_Take* tk = GetActiveTake(item))
							if (PCM_source* pcm = (PCM_source*)GetSetMediaItemTakeInfo(tk, "P_SOURCE", NULL)) {
								RevealFile(pcm->GetFileName());
								return;
							}
	// if we are here, it means the above failed
	MessageBox(GetMainHwnd(), 
		__LOCALIZE("Cannot show path in explorer/finder!\nProbable cause: empty source, in-project MIDI source, etc...","sws_mbox"), 
		__LOCALIZE("S&M - Error","sws_mbox"), 
		MB_OK);
}
