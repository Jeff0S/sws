/******************************************************************************
/ NF_ReaScript.cpp
/
/ Copyright (c) 2018 nofish
/ http://forum.cockos.com/member.php?u=6870
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
#ifdef _WIN32
#  include <cstdint> // uint32_t
#endif

#include "NF_ReaScript.h"

#include "../Breeder/BR_Misc.h" // #974, GetProjectTrackSelectionAction()
#include "../Breeder/BR_Loudness.h" // #880
#include "../Misc/Analysis.h" // #781
#include "../SnM/SnM.h" // ScheduledJob
#include "../SnM/SnM_Chunk.h" // SNM_FXSummaryParser
#include "../SnM/SnM_Notes.h" // #755
#include "../SnM/SnM_Project.h" // #974, GetProjectLoadAction(), GetGlobalStartupAction()
#include "../SnM/SnM_Util.h" // #974, SNM_NamedCommandLookup(), CheckSwsMacroScriptNumCustomId()

// #781, peak/RMS
double DoGetMediaItemMaxPeakAndMaxPeakPos(MediaItem* item, double* maxPeakPosOut) // maxPeakPosOut == NULL: peak only
{
	if (!item) return -150.0;
	double sampleRate = ((PCM_source*)item)->GetSampleRate(); // = 0.0 for MIDI/empty item
	if (!sampleRate) return -150.0;

	double curPeak = -150.0;
	double maxPeak = -150.0;

	int iChannels = ((PCM_source*)item)->GetNumChannels();
	if (iChannels)
	{
		ANALYZE_PCM a;
		memset(&a, 0, sizeof(a));
		a.iChannels = iChannels;
		a.dPeakVals = new double[iChannels];

		if (AnalyzeItem(item, &a))
		{
			for (int i = 0; i < iChannels; i++) {
				curPeak = VAL2DB(a.dPeakVals[i]);
				if (maxPeak < curPeak) {
					maxPeak = curPeak;
				}
			}
			if (maxPeakPosOut)
				*maxPeakPosOut = a.peakSample / sampleRate; // relative to item position
		}
		delete[] a.dPeakVals;
		return maxPeak;
	}
	else // empty item or failed for some reason
		return -150.0;
}

double DoGetMediaItemAverageRMS(MediaItem* item) // average RMS of all channels combined over entire item
{
	if (!item || !((PCM_source*)item)->GetSampleRate())
		return -150;

	int iChannels = ((PCM_source*)item)->GetNumChannels();
	if (iChannels)
	{
		ANALYZE_PCM a;
		memset(&a, 0, sizeof(a));
		a.dWindowSize = 0.0; // non-windowed

		if (AnalyzeItem(item, &a))
			return VAL2DB(a.dRMS);
	}
	else
		return -150.0;

	return -150;
}

double DoGetMediaItemPeakRMS_NonWindowed(MediaItem* item) // highest RMS of all channels over entire item
{
	if (!item || !((PCM_source*)item)->GetSampleRate())
		return -150.0;

	double curPeakRMS = -150.0;
	double maxPeakRMS = -150.0;

	int iChannels = ((PCM_source*)item)->GetNumChannels();
	if (iChannels)
	{
		ANALYZE_PCM a;
		memset(&a, 0, sizeof(a));
		a.iChannels = iChannels;
		a.dRMSs = new double[iChannels];

		if (AnalyzeItem(item, &a))
		{
			for (int i = 0; i < iChannels; i++) {
				curPeakRMS = VAL2DB(a.dRMSs[i]);
				if (maxPeakRMS < curPeakRMS) {
					maxPeakRMS = curPeakRMS;
				}
			}
		}

		delete[] a.dRMSs;
		return maxPeakRMS;
	}
	else
		return -150.0;
}

double DoGetMediaItemPeakRMS_Windowed(MediaItem* item) // highest RMS window of all channels
{
	if (!item || !((PCM_source*)item)->GetSampleRate())
		return -150.0;

	ANALYZE_PCM a;
	memset(&a, 0, sizeof(a));
	NF_GetRMSOptions(NULL, &a.dWindowSize);

	if (AnalyzeItem(item, &a))
		return VAL2DB(a.dRMS);
	else
		return -150.0;
}

// exported functions
double NF_GetMediaItemMaxPeak(MediaItem* item)
{
	return DoGetMediaItemMaxPeakAndMaxPeakPos(item, NULL);
}

double NF_GetMediaItemMaxPeakAndMaxPeakPos(MediaItem* item, double* maxPeakPosOut) // #953
{
	return DoGetMediaItemMaxPeakAndMaxPeakPos(item, maxPeakPosOut);
}

double NF_GetMediaItemAverageRMS(MediaItem* item)
{
	return DoGetMediaItemAverageRMS(item);
}

double NF_GetMediaItemPeakRMS_Windowed(MediaItem* item)
{
	return DoGetMediaItemPeakRMS_Windowed(item);
}

double NF_GetMediaItemPeakRMS_NonWindowed(MediaItem* item)
{
	return DoGetMediaItemPeakRMS_NonWindowed(item);
}


double GetPosInItem(INT64 peakSample, double sampleRate); // fwd. decl.
bool NF_AnalyzeMediaItemPeakAndRMS(MediaItem* item, double windowSize, void* reaperarray_peaks, void* reaperarray_peakpositions, void* reaperarray_RMSs, void* reaperarray_RMSpositions)
{
	if (!item) return false;
	double samplerate = ((PCM_source*)item)->GetSampleRate();
	if (!samplerate) return false;

	bool success = false;
	ANALYZE_PCM a;
	memset(&a, 0, sizeof(a));
	a.dWindowSize = windowSize;
	a.iChannels = ((PCM_source*)item)->GetNumChannels();

	// alloc. i/o arrays
	a.dPeakVals = new double[a.iChannels];
	a.peakSamples = new INT64[a.iChannels];
	a.dRMSs = new double[a.iChannels];
	a.peakRMSsamples = new INT64[a.iChannels];
	
	if (a.iChannels) 
	{
		if (AnalyzeItem(item, &a))
			success = true;

		// cast reaperarrays to double*
		double* d_reaperarray_peaks = static_cast<double*>(reaperarray_peaks);
		double* d_reaperarray_peakpositions = static_cast<double*>(reaperarray_peakpositions);
		double* d_reaperarray_RMSs = static_cast<double*>(reaperarray_RMSs);
		double* d_reaperarray_RMSpositions = static_cast<double*>(reaperarray_RMSpositions);
		
		// check space in reaperarrays
		// https://forum.cockos.com/showthread.php?t=211620
		uint32_t& d_reaperarray_peaksCurSize = ((uint32_t*)(d_reaperarray_peaks))[0];
		uint32_t& d_reaperarray_peakpositionsCurSize = ((uint32_t*)(d_reaperarray_peakpositions))[0];
		uint32_t& d_reaperarray_RMSsCurSize = ((uint32_t*)(d_reaperarray_RMSs))[0];
		uint32_t& d_reaperarray_RMSpositionsCurSize = ((uint32_t*)(d_reaperarray_RMSpositions))[0];

		// write analyzed values to reaperarrays
		// never write to [0] in reaperarrays!!!
		for (int i = 1; i <= a.iChannels; i++) {
			if (d_reaperarray_peaksCurSize < ((uint32_t*)(d_reaperarray_peaks))[1]) { // higher 32 bits in 1st entry: max alloc. size
				d_reaperarray_peaks[i] = VAL2DB(a.dPeakVals[i - 1]);
				d_reaperarray_peaksCurSize++;
			}
			else
				break;
		}

		for (int i = 1; i <= a.iChannels; i++) {
			if (d_reaperarray_peakpositionsCurSize < ((uint32_t*)(d_reaperarray_peakpositions))[1]) {
				d_reaperarray_peakpositions[i] = GetPosInItem(a.peakSamples[i - 1], samplerate);
				d_reaperarray_peakpositionsCurSize++;
			}
			else
				break;
		}

		for (int i = 1; i <= a.iChannels; i++) {
			if (d_reaperarray_RMSsCurSize < ((uint32_t*)(d_reaperarray_RMSs))[1]) {
				d_reaperarray_RMSs[i] = VAL2DB(a.dRMSs[i - 1]);
				d_reaperarray_RMSsCurSize++;
			}
			else
				break;
		}

		for (int i = 1; i <= a.iChannels; i++) {
			if (d_reaperarray_RMSpositionsCurSize < ((uint32_t*)(d_reaperarray_RMSpositions))[1]) {
				d_reaperarray_RMSpositions[i] = GetPosInItem(a.peakRMSsamples[i - 1], samplerate);
				d_reaperarray_RMSpositionsCurSize++;
			}
			else
				break;
		}
	}

	delete[] a.dPeakVals;
	delete[] a.peakSamples;
	delete[] a.dRMSs;
	delete[] a.peakRMSsamples;
	return success;
}

void NF_GetSWS_RMSoptions(double* targetOut, double* windowSizeOut)
{
	NF_GetRMSOptions(targetOut, windowSizeOut);
}

bool NF_SetSWS_RMSoptions(double target, double windowSize)
{
	return NF_SetRMOptions(target, windowSize);
}

double GetPosInItem(INT64 peakSample, double sampleRate) // relative to item start
{	
	if (peakSample == -666)
		return -666;
	else
		return peakSample / sampleRate; 
}

// #880, Loudness
bool NF_AnalyzeTakeLoudness_IntegratedOnly(MediaItem_Take * take, double* lufsIntegratedOut)
{
	return NFDoAnalyzeTakeLoudness_IntegratedOnly(take, lufsIntegratedOut);
}

bool NF_AnalyzeTakeLoudness(MediaItem_Take * take, bool analyzeTruePeak, double* lufsIntegratedOut, double* rangeOut, double* truePeakOut, double* truePeakPosOut, double* shorTermMaxOut, double* momentaryMaxOut)
{
	return NFDoAnalyzeTakeLoudness(take, analyzeTruePeak, lufsIntegratedOut, rangeOut, truePeakOut, truePeakPosOut, shorTermMaxOut, momentaryMaxOut);
}

bool NF_AnalyzeTakeLoudness2(MediaItem_Take * take, bool analyzeTruePeak, double* lufsIntegratedOut, double* rangeOut, double* truePeakOut, double* truePeakPosOut, double* shorTermMaxOut, double* momentaryMaxOut, double* shortTermMaxPosOut, double* momentaryMaxPosOut)
{
	return NFDoAnalyzeTakeLoudness2(take, analyzeTruePeak, lufsIntegratedOut, rangeOut, truePeakOut, truePeakPosOut, shorTermMaxOut, momentaryMaxOut, shortTermMaxPosOut, momentaryMaxPosOut);
}

// #755, Track notes
const char* NF_GetSWSTrackNotes(MediaTrack* track)
{
	return NFDoGetSWSTrackNotes(track);
}

void NF_SetSWSTrackNotes(MediaTrack* track, const char* buf)
{
	NFDoSetSWSTrackNotes(track, buf);
}

const char* NF_GetSWSMarkerRegionSub(int mkrRgnIdx)
{
	return NFDoGetSWSMarkerRegionSub(mkrRgnIdx);
}

bool NF_SetSWSMarkerRegionSub(const char* mkrRgnSub, int mkrRgnIdx)
{
	return NFDoSetSWSMarkerRegionSub(mkrRgnSub, mkrRgnIdx);
}

void NF_UpdateSWSMarkerRegionSubWindow()
{
	NF_DoUpdateSWSMarkerRegionSubWindow();
}

bool NF_TakeFX_GetFXModuleName(MediaItem * item, int fx, char * nameOut, int nameOutSz)
{
	WDL_FastString module;
	bool found = false;

	if (item)
	{
		SNM_FXSummaryParser takeFxs(item);
		if (SNM_FXSummary* summary = takeFxs.GetSummaries()->Get(fx))
		{
			module = summary->m_realName;
			found = true;
		}
	}

	snprintf(nameOut, nameOutSz, "%s", module.Get());
	return found;
}

int NF_Win32_GetSystemMetrics(int nIndex)
{
	return GetSystemMetrics(nIndex);
}

// #974, global actions
enum class GlobalActionTypes {
	startupAction = 0
};

// wantDesc == true: return action text, false: return command ID number (native actions) or named command (extension/ReaScript)
void DoGetGlobalAction(char* buf, int bufSize, WDL_FastString* action, bool wantDesc)
{
	WDL_FastString str;
	if (action && SNM_NamedCommandLookup(action->Get()))
	{
		if (wantDesc)
			str.Set(kbd_getTextFromCmd(SNM_NamedCommandLookup(action->Get()), nullptr));	
		else
			str.Set(action->Get());
	}
	else
	{
		if (!wantDesc)
			str.Set("0");
	}
	snprintf(buf, bufSize, "%s", str.Get());
}

bool DoSetGlobalAction(const char* buf, WDL_FastString* action, GlobalActionTypes actionType)
{
	if (action && SNM_NamedCommandLookup(buf) && !CheckSwsMacroScriptNumCustomId(buf))
	{
		action->Set(buf);
		switch (actionType)
		{
		case GlobalActionTypes::startupAction:
			WritePrivateProfileString("Misc", "GlobalStartupAction", buf, g_SNM_IniFn.Get());
			break;
		}
		return true;
	}
	return false;
}

bool DoClearGlobalAction(WDL_FastString* action, GlobalActionTypes actionType)
{
	if (action) 
	{
		action->Set("");
		switch (actionType)
		{
		case GlobalActionTypes::startupAction:
			WritePrivateProfileString("Misc", "GlobalStartupAction", nullptr, g_SNM_IniFn.Get());
			break;
		}
		return true;
	}
	return false;
}

// #974, project actions
// wantDesc == true: return action text, false : return command ID number(native actions) or named command(extension / ReaScript)
void DoGetProjectAction(char* buf, int bufSize, SWSProjConfig<WDL_FastString>* action, bool wantDesc)
{
	WDL_FastString str;
	if (action && SNM_NamedCommandLookup(action->Get()->Get()))
	{
		if (wantDesc)
			str.Set(kbd_getTextFromCmd(SNM_NamedCommandLookup(action->Get()->Get()), nullptr));
		else
			str.Set(action->Get());
	}
	else
	{
		if (!wantDesc)
			str.Set("0");
	}
	snprintf(buf, bufSize, "%s", str.Get());
}

bool DoSetProjectAction(const char* buf, SWSProjConfig<WDL_FastString>* action)
{
	if (action && SNM_NamedCommandLookup(buf) && !CheckSwsMacroScriptNumCustomId(buf))
	{
		action->Get()->Set(buf);
		return true;
	}	
	return false;
}

bool DoClearProjectAction(SWSProjConfig<WDL_FastString>* action)
{
	if (action) {
		action->Get()->Set("");
		return true;
	}
	return false;
}

// SnM global startup action
void NF_GetGlobalStartupAction_Desc(char* descOut, int descOut_sz)
{
	DoGetGlobalAction(descOut, descOut_sz, GetGlobalStartupAction(), true);
}
void NF_GetGlobalStartupAction_CmdID(char* cmdIdOut, int cmdIdOut_sz)
{
	DoGetGlobalAction(cmdIdOut, cmdIdOut_sz, GetGlobalStartupAction(), false);
}

bool NF_SetGlobalStartupAction(const char* buf)
{
	return DoSetGlobalAction(buf, GetGlobalStartupAction(), GlobalActionTypes::startupAction);
}

bool NF_ClearGlobalStartupAction()
{
	return DoClearGlobalAction(GetGlobalStartupAction(), GlobalActionTypes::startupAction);
}

// SnM project startup action 
void NF_GetProjectStartupAction_Desc(char * descOut, int descOut_sz)
{
	DoGetProjectAction(descOut, descOut_sz, GetProjectLoadAction(), true);
}

void NF_GetProjectStartupAction_CmdID(char * cmdIdOut, int cmdIdOut_sz)
{
	DoGetProjectAction(cmdIdOut, cmdIdOut_sz, GetProjectLoadAction(), false);
}

bool NF_SetProjectStartupAction(const char* buf)
{
	return DoSetProjectAction(buf, GetProjectLoadAction());
}

bool NF_ClearProjectStartupAction()
{
	return DoClearProjectAction(GetProjectLoadAction());
}

// BR Project track selection action
void NF_GetProjectTrackSelectionAction_Desc(char* descOut, int descOut_sz)
{
	DoGetProjectAction(descOut, descOut_sz, GetProjectTrackSelectionAction(), true);
}

void NF_GetProjectTrackSelectionAction_CmdID(char* descOut, int descOut_sz)
{
	DoGetProjectAction(descOut, descOut_sz, GetProjectTrackSelectionAction(), false);
}

bool NF_SetProjectTrackSelectionAction(const char* buf)
{
	return DoSetProjectAction(buf, GetProjectTrackSelectionAction());
}

bool NF_ClearProjectTrackSelectionAction()
{
	return DoClearProjectAction(GetProjectTrackSelectionAction());
}
