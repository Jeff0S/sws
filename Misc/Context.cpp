/******************************************************************************
/ Context.cpp
/
/ Copyright (c) 2010 Tim Payne (SWS)
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

// Actions that behave differently based on the cursor context

#include "stdafx.h"
#include "Context.h"
#include "../SnM/SnM_Routing.h"
#include "../Utility/RazorEditArea.hpp"

bool AreThereSelItemsInTimeSel()
{
	double t1, t2;
	GetSet_LoopTimeRange(false, false, &t1, &t2, false);
	if (t1 != t2)
	{
		// see if sel items are in the time selection
		const int cnt=CountSelectedMediaItems(NULL);
		for (int i = 0; i < cnt; i++)
		{
			MediaItem* item = GetSelectedMediaItem(0, i);
			double dItemPos = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
			double dItemLen = *(double*)GetSetMediaItemInfo(item, "D_LENGTH", NULL);
			if ((dItemPos >= t1 && dItemPos < t2) || (dItemPos < t1 && dItemLen > t1 - dItemPos))
				return true;
		}
	}
	return false;
}

bool AreThereItemsUnderCursor(bool bSel)
{
	double dCursor = GetCursorPosition();
	const int cnt = (bSel ? CountSelectedMediaItems(NULL) : CountMediaItems(NULL));
	for (int i = 0; i < cnt; i++)
	{
		MediaItem* item = bSel ? GetSelectedMediaItem(0, i) : GetMediaItem(0, i);
		double dItemStart = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
		double dItemEnd   = *(double*)GetSetMediaItemInfo(item, "D_LENGTH", NULL) + dItemStart;
		if (dItemStart < dCursor && dItemEnd > dCursor)
				return true;
	}
	return false;
}

void SmartCopy(COMMAND_T* ct)
{
	if (GetCursorContext() == 1 && AreThereSelItemsInTimeSel())
		Main_OnCommand(40060, 0); // Copy sel area of items (handles razor edit area(s) (rea), but pass through if rea present, see below)
	else if (GetCursorContext() == 0)
		CopyWithIOs(ct);
	else // if rea present all items are unselected
		Main_OnCommand(40057, 0); // Std copy (handles rea)
}

void SmartCut(COMMAND_T* ct)
{
	if (GetCursorContext() == 1 && AreThereSelItemsInTimeSel())
		Main_OnCommand(40307, 0); // Cut sel area of items (handles rae, but pass through if rae present)
	else if (GetCursorContext() == 0)
		CutWithIOs(ct);
	else
		Main_OnCommand(40059, 0); // Std cut (handles rea)
}

void SmartRemove(COMMAND_T*)
{
	if (GetCursorContext() == 1 && AreThereSelItemsInTimeSel())
		Main_OnCommand(40312, 0); // Remove sel area of items (handles rae, but pass through if rae present)
	else
		Main_OnCommand(40697, 0); // Std remove (w/ prompt) (handles rea)
}

void SmartSplit(COMMAND_T*)
{
	double t1, t2;
	GetSet_LoopTimeRange(false, false, &t1, &t2, false);
	if (AreThereSelItemsInTimeSel() || (t1 != t2 && !CountSelectedMediaItems(0)) 
		// don't get items which are fully enclosed by a rea, matches AreThereSelItemsInTimeSel() behaviour 
		|| razor::RazorEditArea::GetMediaItemsCrossingRazorEditAreas(false).size())
		Main_OnCommand(40061, 0); // Split at time sel (handles rea)
	else
		Main_OnCommand(40012, 0); // Std split at cursor
}

// #796, splits at edit cursor also during playback
void SmartSplit2(COMMAND_T*)
{
	double t1, t2;
	GetSet_LoopTimeRange(false, false, &t1, &t2, false);
	if (AreThereSelItemsInTimeSel() || (t1 != t2 && !CountSelectedMediaItems(0)) 
		|| razor::RazorEditArea::GetMediaItemsCrossingRazorEditAreas(false).size())
		Main_OnCommand(40061, 0); // Split at time sel
	else
		Main_OnCommand(40757, 0); // Split at edit cursor (no change sel)
}


void TripleSplit(COMMAND_T*)
{
	if (AreThereSelItemsInTimeSel() 
		|| razor::RazorEditArea::GetMediaItemsCrossingRazorEditAreas(false).size())
		Main_OnCommand(40061, 0); // Split at time sel
	else if (AreThereItemsUnderCursor(false))
		Main_OnCommand(40012, 0); // Std split at cursor
	else
		Main_OnCommand(40746, 0); // Split at mouse cursor
}

void TripleSplit2(COMMAND_T*) // #796, splits at edit cursor also during playback
{
	if (AreThereSelItemsInTimeSel() 
		|| razor::RazorEditArea::GetMediaItemsCrossingRazorEditAreas(false).size())
		Main_OnCommand(40061, 0); // Split at time sel
	else if (AreThereItemsUnderCursor(false))
		Main_OnCommand(40757, 0); // Split at edit cursor (no change sel)
	else
		Main_OnCommand(40746, 0); // Split at mouse cursor
}

void SmartUnsel(COMMAND_T*)
{
	switch(GetCursorContext())
	{
	case 0: //Tracks
		Main_OnCommand(40297, 0);
		break;
	case 1: //Items
		Main_OnCommand(40289, 0);
		break;
	case 2: //Envelopes
		Main_OnCommand(40331, 0);
		break;
	}
}

void UnselAll(COMMAND_T*)
{
	Main_OnCommand(40297, 0);
	Main_OnCommand(40289, 0);
	Main_OnCommand(40331, 0);
}

void SafeTiemSel(COMMAND_T*)
{
	double t1, t2;
	GetSet_LoopTimeRange(false, false, &t1, &t2, false);
	if (t1 == t2)
		Main_OnCommand(40290, 0);
}

//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions
static COMMAND_T g_commandTable[] =
{
	{ { DEFACCEL, "SWS: Copy items/tracks/env (obey time selection/razor edit areas)" },
	  "SWS_SMARTCOPY",SmartCopy, NULL, },
	{ { DEFACCEL, "SWS: Cut items/tracks/env (obey time selection/razor edit areas)" },
	  "SWS_SMARTCUT",SmartCut, NULL, },
	{ { DEFACCEL, "SWS: Remove items/tracks/env, (obey time selection/razor edit areas)" },
	  "SWS_SMARTREMOVE",SmartRemove, NULL, },
	{ { DEFACCEL, "SWS: Split items at time selection/razor edit areas (if exists), play cursor (during playback), else at edit cursor" },
	  "SWS_SMARTSPLIT",SmartSplit, NULL, },
	{ { DEFACCEL, "SWS: Split items at time selection/razor edit areas (if exists), else at edit cursor (also during playback)" },
	  "SWS_SMARTSPLIT2",SmartSplit2, NULL, },
	{ { DEFACCEL, "SWS: Split items at time selection/razor edit areas, edit cursor, play cursor (during playback), or mouse cursor" },
	  "SWS_TRIPLESPLIT",TripleSplit, NULL, },
	{ { DEFACCEL, "SWS: Split items at time selection/razor edit areas, edit cursor (also during playback), or mouse cursor" },
	  "SWS_TRIPLESPLIT2",TripleSplit2, NULL, },
	{ { DEFACCEL, "SWS: Unselect all items/tracks/env points (depending on focus)" },
	  "SWS_SMARTUNSEL",SmartUnsel, NULL, },
	{ { DEFACCEL, "SWS: Unselect all items/tracks/env points" },
	  "SWS_UNSELALL",UnselAll, NULL, },
	{ { DEFACCEL, "SWS: Set time selection to selected items (skip if time selection exists)" },
	  "SWS_SAFETIMESEL",SafeTiemSel, NULL, },

	{ {}, LAST_COMMAND, }, // Denote end of table
};
//!WANT_LOCALIZE_1ST_STRING_END

int ContextInit()
{
	SWSRegisterCommands(g_commandTable);

	return 1;
}
