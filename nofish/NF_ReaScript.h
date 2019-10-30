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
int            NF_ReadAudioFileBitrate(const char* fn);


// #974
/*
void            NF_GetGlobalStartupAction_Desc(char *buf, int bufSize);
void            NF_GetGlobalStartupAction_CmdID(char *buf, int bufSize);
bool            NF_SetGlobalStartupAction(const char* buf);
bool            NF_ClearGlobalStartupAction();

void            NF_GetProjectStartupAction_Desc(char *buf, int bufSize);
void            NF_GetProjectStartupAction_CmdID(char *buf, int bufSize);
bool            NF_SetProjectStartupAction(const char* buf);
bool            NF_ClearProjectStartupAction();

// ReaScript.cpp
	// #974 Global/project startup actions
	{ APIFUNC(NF_GetGlobalStartupAction_Desc), "void", "char*,int", "buf,buf_sz", "Returns action description if global startup action is set, otherwise empty string.", },
	{ APIFUNC(NF_GetGlobalStartupAction_CmdID), "void", "char*,int", "buf,buf_sz", "Returns command ID number (for native actions) or named command IDs / identifier strings (for extension actions /ReaScripts) if global startup action is set, otherwise \"0\". Named command IDs start with underscore (\"_...\").", },
	{ APIFUNC(NF_SetGlobalStartupAction), "bool", "const char*", "str", "Returns true if global startup action was set successfully (i.e. valid action ID). Note: For SWS / S & M actions and macros / scripts, you must use identifier strings (e.g. \"_SWS_ABOUT\", \"_f506bc780a0ab34b8fdedb67ed5d3649\"), not command IDs (e.g. \"47145\").\nTip: to copy such identifiers, right - click the action in the Actions window > Copy selected action cmdID / identifier string.\nNOnly works for actions / scripts from Main action section.", },
	{ APIFUNC(NF_ClearGlobalStartupAction), "bool", "", "", "Returns true if global startup action was cleared successfully.", },
	{ APIFUNC(NF_GetProjectStartupAction_Desc), "void", "char*,int", "buf,buf_sz", "Returns action description if project startup action is set, otherwise empty string", },
	{ APIFUNC(NF_GetProjectStartupAction_CmdID), "void", "char*,int", "buf,buf_sz", "Returns command ID number (for native actions) or named command IDs / identifier strings (for extension actions /ReaScripts) if project startup action is set, otherwise \"0\". Named command IDs start with underscore (\"_...\").", },
	{ APIFUNC(NF_SetProjectStartupAction), "bool", "const char*", "str", "Returns true if project startup action was set successfully (i.e. valid action ID). Note: For SWS / S & M actions and macros / scripts, you must use identifier strings (e.g. \"_SWS_ABOUT\", \"_f506bc780a0ab34b8fdedb67ed5d3649\"), not command IDs (e.g. \"47145\").\nTip: to copy such identifiers, right - click the action in the Actions window > Copy selected action cmdID / identifier string.\nOnly works for actions / scripts from Main action section. Project must be saved after setting project startup action to be persistent.", },
	{ APIFUNC(NF_ClearProjectStartupAction), "bool", "", "", "Returns true if project startup action was cleared successfully.", },

// whatsnew.txt
+Issue 974 (Lua example https://github.com/reaper-oss/sws/issues/974#issuecomment-386480161|here|):
 -NF_GetGlobalStartupAction_Desc()
 -NF_GetGlobalStartupAction_CmdID()
 -NF_SetGlobalStartupAction()
 -NF_ClearGlobalStartupAction()
 -NF_GetProjectStartupAction_Desc()
 -NF_GetProjectStartupAction_CmdID()
 -NF_SetProjectStartupAction()
 -NF_ClearProjectStartupAction()
*/

