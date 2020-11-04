/******************************************************************************
/ TimeState.cpp
/
/ Copyright (c) 2009 Tim Payne (SWS)
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
#include "TimeState.h"

//*****************************************************
//Globals

SWSProjConfig<WDL_PtrList_DOD<TimeSelection> > g_timeSel;

//*****************************************************
// TimeSelection class
TimeSelection::TimeSelection(int iSlot, bool bIsLoop)
{
	m_bIsLoop = bIsLoop;
	m_iSlot = iSlot;
	GetSet_LoopTimeRange(false, bIsLoop, &m_dStart, &m_dEnd, false);
}

TimeSelection::TimeSelection(LineParser* lp)
{
	m_bIsLoop = lp->gettoken_int(1) ? true : false;
	m_iSlot   = lp->gettoken_int(2);
	m_dStart  = lp->gettoken_float(3);
	m_dEnd    = lp->gettoken_float(4);
}

void TimeSelection::Restore()
{
	GetSet_LoopTimeRange(true, m_bIsLoop, &m_dStart, &m_dEnd, false);
}

char* TimeSelection::ItemString(char* str, int maxLen)
{
	snprintf(str, maxLen, "TIMESEL %d %d %.14f %.14f", m_bIsLoop ? 1 : 0, m_iSlot, m_dStart, m_dEnd);
	return str;
}

void SaveTimeSel(const char *undoMsg, int iSlot, bool bIsLoop)
{
	// In order?  nah.  Only 10 max at the moment.
	for (int i = 0; i < g_timeSel.Get()->GetSize(); i++)
	{
		TimeSelection* ts = g_timeSel.Get()->Get(i);
		if (ts->m_iSlot == iSlot && ts->m_bIsLoop == bIsLoop)
		{
			if(undoMsg)
				Undo_BeginBlock2(NULL);
			delete ts;
			g_timeSel.Get()->Set(i, new TimeSelection(iSlot, bIsLoop));
			if(undoMsg)
				Undo_EndBlock2(NULL, undoMsg, UNDO_STATE_ALL);
			return;
		}
	}
	// Didn't find one
	if(undoMsg)
		Undo_BeginBlock2(NULL);
	g_timeSel.Get()->Add(new TimeSelection(iSlot, bIsLoop));
	if(undoMsg)
		Undo_EndBlock2(NULL, undoMsg, UNDO_STATE_ALL);
}

void RestoreTimeSel(const char *undoMsg, int iSlot, bool bIsLoop)
{
	bool updated=false;
	for (int i = 0; i < g_timeSel.Get()->GetSize(); i++)
	{
		TimeSelection* ts = g_timeSel.Get()->Get(i);
		if (ts->m_iSlot == iSlot && ts->m_bIsLoop == bIsLoop)
		{
			ts->Restore();
			if(!updated)
			{
				if(undoMsg)
					Undo_BeginBlock2(NULL);
				updated=true;
			}
		}
	}
	UpdateTimeline();
	if(updated && undoMsg)
		Undo_EndBlock2(NULL, undoMsg, UNDO_STATE_ALL);

}

void RestoreNext(const char *undoMsg, int* iCur, bool bIsLoop)
{
	int iNext = 666;
	int iFirst = 666;

	for (int i = 0; i < g_timeSel.Get()->GetSize(); i++)
	{
		TimeSelection* ts = g_timeSel.Get()->Get(i);
		if (ts->m_bIsLoop == bIsLoop)
		{
			if (ts->m_iSlot < iFirst)
				iFirst = ts->m_iSlot;
			if (ts->m_iSlot > *iCur && ts->m_iSlot < iNext)
				iNext = ts->m_iSlot;
		}
	}
	
	if (iNext != 666)
		*iCur = iNext;
	else if (iFirst != 666)
		*iCur = iFirst;

	RestoreTimeSel(undoMsg, *iCur, bIsLoop);
}

void SaveTimeSel(COMMAND_T* cs) { SaveTimeSel(SWS_CMD_SHORTNAME(cs), (int)cs->user+1, false); }
void SaveLoopSel(COMMAND_T* cs) { SaveTimeSel(SWS_CMD_SHORTNAME(cs), (int)cs->user, true);  }
void RestoreTimeSel(COMMAND_T* cs) { RestoreTimeSel(SWS_CMD_SHORTNAME(cs), (int)cs->user+1, false); }
void RestoreLoopSel(COMMAND_T* cs) { RestoreTimeSel(SWS_CMD_SHORTNAME(cs), (int)cs->user, true); }
void RestoreTimeNext(COMMAND_T* cs) { static int iCur = -1; RestoreNext(SWS_CMD_SHORTNAME(cs), &iCur, false); }
void RestoreLoopNext(COMMAND_T* cs) { static int iCur = -1; RestoreNext(SWS_CMD_SHORTNAME(cs), &iCur, true); }
