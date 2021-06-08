/******************************************************************************
/ NF_ReaScript.h
/
/ Copyright (c) 2018 nofish
/ http://forum.cockos.com/member.php?u=6870
/ https://github.com/nofishonfriday/sws
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

#pragma once

// #781
double          NF_GetMediaItemMaxPeak(MediaItem* item);
double          NF_GetMediaItemMaxPeakAndMaxPeakPos(MediaItem* item, double* maxPeakPosOut); // // #953
double          NF_GetMediaItemAverageRMS(MediaItem* item);
double          NF_GetMediaItemPeakRMS_NonWindowed(MediaItem* item);
double          NF_GetMediaItemPeakRMS_Windowed(MediaItem* item);
                // combines all above functions
bool            NF_AnalyzeMediaItemPeakAndRMS(MediaItem* item, double windowSize, void* reaperarray_peaks, void* reaperarray_peakpositions, void* reaperarray_RMSs, void* reaperarray_RMSpositions);
void            NF_GetSWS_RMSoptions(double* targetOut, double* windowSizeOut);
bool            NF_SetSWS_RMSoptions(double target, double windowSize);

// #880
bool            NF_AnalyzeTakeLoudness_IntegratedOnly(MediaItem_Take* take, double* lufsIntegratedOut);
bool            NF_AnalyzeTakeLoudness(MediaItem_Take* take, bool analyzeTruePeak, double* lufsOut, double* rangeOut, double* truePeakOut, double* truePeakPosOut, double* shorTermMaxOut, double* momentaryMaxOut);
bool            NF_AnalyzeTakeLoudness2(MediaItem_Take* take, bool analyzeTruePeak, double* lufsOut, double* rangeOut, double* truePeakOut, double* truePeakPosOut, double* shorTermMaxOut, double* momentaryMaxOut, double* shortTermMaxPosOut, double* momentaryMaxPosOut);

// #755
const char*    NF_GetSWSTrackNotes(MediaTrack* track);
void           NF_SetSWSTrackNotes(MediaTrack* track, const char* buf);
const char*    NF_GetSWSMarkerRegionSub(int mkrRgnIdx);
bool           NF_SetSWSMarkerRegionSub(const char* mkrRgnSub, int mkrRgnIdx);
void           NF_UpdateSWSMarkerRegionSubWindow();

bool           NF_TakeFX_GetFXModuleName(MediaItem* item, int fx, char* nameOut, int nameOutSz);
int            NF_Win32_GetSystemMetrics(int nIndex);

// #974
void           NF_GetGlobalStartupAction_Desc(char *buf, int bufSize);
void           NF_GetGlobalStartupAction_CmdID(char *buf, int bufSize);
bool           NF_SetGlobalStartupAction(const char* buf);
bool           NF_ClearGlobalStartupAction();

void           NF_GetProjectStartupAction_Desc(char *buf, int bufSize);
void           NF_GetProjectStartupAction_CmdID(char *buf, int bufSize);
bool           NF_SetProjectStartupAction(const char* buf);
bool           NF_ClearProjectStartupAction();

void           NF_GetProjectTrackSelectionAction_Desc(char* buf, int bufSize);
void           NF_GetProjectTrackSelectionAction_CmdID(char* buf, int bufSize);
bool           NF_SetProjectTrackSelectionAction(const char* buf);
bool           NF_ClearProjectTrackSelectionAction();
