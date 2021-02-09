/******************************************************************************
/ BR_Util.h
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
#pragma once

#include "BR_EnvelopeUtil.h"

/******************************************************************************
* Constants                                                                   *
******************************************************************************/
extern int SCROLLBAR_W;
const int NEGATIVE_INF    = -150;
const double VOLUME_DELTA = 0.0000000000001;
const double PAN_DELTA    = 0.001;

const double MIN_BPM                   = 1;
const double MAX_BPM                   = 960;
const int    MIN_SIG                   = 1;
const int    MAX_SIG                   = 255;
const double MIN_TEMPO_DIST            = 0.001;
const double MIN_TIME_SIG_PARTIAL_DIFF = 0.00001;
const double MIN_ENV_DIST              = 0.000001;
const double MIN_GRID_DIST             = 0.00097;         // 1/256 at 960 BPM (can't set it to more than 1/256 in grid settings, (other actions like Adjust grid can, but things get broken then)
const double MAX_GRID_DIV              = 1.0 / 256.0 * 4; // 1/256 because things can get broken after that (http://forum.cockos.com/project.php?issueid=5263)
const double GRID_DIV_DELTA            = 0.00000001;

const int SECTION_MAIN           = 0;
const int SECTION_MAIN_ALT       = 100;
const int SECTION_MIDI_EDITOR    = 32060;
const int SECTION_MIDI_LIST      = 32061;
const int SECTION_MIDI_INLINE    = 32062;
const int SECTION_MEDIA_EXPLORER = 32063;

/******************************************************************************
* Macros                                                                      *
******************************************************************************/
#define SKIP(counter, count) {(counter)+=(count); continue;}
#define BINARY(x)            BinaryToDecimal(#x)

/******************************************************************************
* Miscellaneous                                                               *
******************************************************************************/
vector<int> GetDigits (int val); // for 123 returns [0] = 1, [1] = 2 etc...
int GetFirstDigit (int val);     // for 123 returns 1
int GetLastDigit (int val);      // for 123 returns 3
int BinaryToDecimal (const char* binaryString);
int GetBit (int val, int bit);
int SetBit (int val, int bit, bool set);
int SetBit (int val, int bit);
int ClearBit (int val, int bit);
int ToggleBit (int val, int bit);
int RoundToInt (double val);
int TruncToInt (double val);
double Round (double val);
double RoundToN (double val, double n);
double Trunc (double val);
double TranslateRange (double value, double oldMin, double oldMax, double newMin, double newMax);
double AltAtof (char* str);
bool IsFraction (char* str, double& convertedFraction);
void ReplaceAll (string& str, string oldStr, string newStr);
void AppendLine (WDL_FastString& str, const char* line);
const char* strstr_last (const char* haystack, const char* needle);
template <typename T> bool WritePtr (T* ptr, T val)  {if (ptr){*ptr = val; return true;} return false;}
template <typename T> bool ReadPtr  (T* ptr, T& val) {if (ptr){val = *ptr; return true;} return false;}
template <typename T> bool AreOverlapped   (T x1, T x2, T y1, T y2) {if (x1 > x2) swap(x1, x2); if (y1 > y2) swap(y1, y2); return x1 <= y2 && y1 <= x2;}
template <typename T> bool AreOverlappedEx (T x1, T x2, T y1, T y2) {if (x1 > x2) swap(x1, x2); if (y1 > y2) swap(y1, y2); return x1 <  y2 && y1 <  x2;}
template <typename T> bool CheckBounds   (T val, T min, T max)    {if (min > max) swap(min, max); if (val < min)  return false; if (val > max)  return false; return true;}
template <typename T> bool CheckBoundsEx (T val, T min, T max)    {if (min > max) swap(min, max); if (val <= min) return false; if (val >= max) return false; return true;}
template <typename T> T    SetToBounds   (T val, T min, T max)    {if (min > max) swap(min, max); if (val < min)  return min;   if (val > max)  return max;   return val;}
template <typename T> T    IsEqual (T a, T b, T epsilon) {epsilon = abs(epsilon); return CheckBounds(a, b - epsilon, b + epsilon);}
template <typename T> T    GetClosestVal (T val, T targetVal1, T targetVal2) {if (abs(targetVal1 - val) <= abs(targetVal2 - val)) return targetVal1; else return targetVal2;}

/******************************************************************************
* General                                                                     *
******************************************************************************/
vector<double> GetProjectMarkers (bool timeSel, double timeSelDelta = 0);
WDL_FastString FormatTime (double position, int mode = -1); // same as format_timestr_pos but handles "measures.beats + time" properly
const char *GetCurrentTheme (std::string* themeName);
int FindClosestProjMarkerIndex (double position);
int CountProjectTabs ();
double EndOfProject (bool markers, bool regions);
double GetMidiOscVal (double min, double max, double step, double currentVal, int commandVal, int commandValhw, int commandRelmode);
double GetPositionFromTimeInfo (int hours, int minutes, int seconds, int frames);
void GetTimeInfoFromPosition (double position, int* hours, int* minutes, int* seconds, int* frames);
void GetSetLastAdjustedSend (bool set, MediaTrack** track, int* sendId, BR_EnvType* type);
void GetSetFocus (bool set, HWND* hwnd, int* context);
void SetAllCoordsToZero (RECT* r);
void RegisterCsurfPlayState (bool set, void (*CSurfPlayState)(bool,bool,bool), const vector<void(*)(bool,bool,bool)>** registeredFunctions = NULL, bool cleanup = false);
void StartPlayback (ReaProject* proj, double position);
bool IsPlaying (ReaProject* proj);
bool IsPaused (ReaProject* proj);
bool IsRecording (ReaProject* proj);
bool AreAllCoordsZero (RECT& r);
PCM_source* DuplicateSource (PCM_source* source); // if the option "Toggle pooled (ghost) MIDI source data when copying media items", using PCM_source->Duplicate() will pool original and new source...use this function to escape this when necessary

/******************************************************************************
* Tracks                                                                      *
******************************************************************************/
int GetEffectiveCompactLevel (MediaTrack* track);           // displayed compact level is determined by the highest compact level of any successive parent
int GetTrackFreezeCount (MediaTrack* track);
bool TcpVis (MediaTrack* track);

/******************************************************************************
* Items and takes                                                             *
******************************************************************************/
vector<MediaItem*> GetSelItems (MediaTrack* track);
double ProjectTimeToItemTime (MediaItem* item, double projTime);
double ItemTimeToProjectTime (MediaItem* item, double itemTime);
int GetTakeId (MediaItem_Take* take, MediaItem* item = NULL);
int GetLoopCount (MediaItem_Take* take, double position, int* loopIterationForPosition);
int GetEffectiveTakeId (MediaItem_Take* take, MediaItem* item, int id, int* effectiveTakeCount); // empty takes could be hidden, so displayed id and real id can differ (pass either take or item and take id)
enum class SourceType {
	Unknown,
	Audio,
	MIDI,
	Video,
	Click,
	Timecode,
	Project,
	VideoEffect,
};
SourceType GetSourceType (PCM_source*);
SourceType GetSourceType (MediaItem_Take*);
int GetEffectiveTimebase (MediaItem* item); // returns 0=time, 1=allbeats, 2=beatsosonly
int GetTakeFXCount (MediaItem_Take* take);
bool GetMidiTakeTempoInfo (MediaItem_Take* take, bool* ignoreProjTempo, double* bpm, int* num, int* den);
bool SetIgnoreTempo (MediaItem* item, bool ignoreTempo, double bpm, int num, int den, bool skipItemsWithSameIgnoreState);
bool DoesItemHaveMidiEvents (MediaItem* item);

// NF: fix #950
// added bool adjustTakesEnvelopes (if true, DoAdjustTakesEnvelopes() is called in TrimItem()
bool TrimItem (MediaItem* item, double start, double end, bool adjustTakesEnvelopes, bool force = false);
// use trim actions rather than API to avoid take env's + stretch markers offset
bool TrimItem_UseNativeTrimActions(MediaItem* item, double start, double end, bool force = false);
// adjust takes env's after trimming item to prevent offset
void DoAdjustTakesEnvelopes(MediaItem_Take* take, double offset); 

bool GetMediaSourceProperties (MediaItem_Take* take, bool* section, double* start, double* length, double* fade, bool* reverse);
bool SetMediaSourceProperties (MediaItem_Take* take, bool section, double start, double length, double fade, bool reverse);
bool SetTakeSourceFromFile (MediaItem_Take* take, const char* filename, bool inProjectData, bool keepSourceProperties);
GUID GetItemGuid (MediaItem* item);
MediaItem* GuidToItem (const GUID* guid, ReaProject* proj = NULL);
WDL_FastString GetSourceChunk (PCM_source* source);

/******************************************************************************
* Fades (not 100% accurate - http://askjf.com/index.php?q=2976s)              *
******************************************************************************/
double GetNormalizedFadeValue (double position, double fadeStart, double fadeEnd, int fadeShape, double fadeCurve, bool isFadeOut);
double GetEffectiveFadeLength (MediaItem* item, bool isFadeOut, int* fadeShape = NULL, double* fadeCurve = NULL);

/******************************************************************************
* Stretch markers                                                             *
******************************************************************************/
int FindPreviousStretchMarker (MediaItem_Take* take, double proj);
int FindNextStretchMarker (MediaItem_Take* take, double position);
int FindClosestStretchMarker (MediaItem_Take* take, double position);
int FindStretchMarker (MediaItem_Take* take, double position, double surroundingRange = 0);
bool InsertStretchMarkersInAllItems (const vector<double>& stretchMarkers, bool doBeatsTimebaseOnly = false, double hardCheckPositionsDelta = -1, bool obeySwsOptions = true); // if obeySwsOptions == true, then markers are inserted only if IsSetAutoStretchMarkersOn() returns true
bool InsertStretchMarkerInAllItems (double position, bool doBeatsTimebaseOnly = false, double hardCheckPositionDelta = -1, bool obeySwsOptions = true);

/******************************************************************************
* Grid                                                                        *
******************************************************************************/
double GetGridDivSafe (); // makes sure grid div is never over MAX_GRID_DIV
double GetNextGridDiv (double position);    // unlike other functions,
double GetPrevGridDiv (double position);    // these don't care about
double GetClosestGridDiv (double position); // grid visibility
double GetNextGridLine (double position);
double GetPrevGridLine (double position);
double GetClosestGridLine (double position);
double GetClosestMeasureGridLine (double position);
double GetClosestLeftSideGridLine (double position);
double GetClosestRightSideGridLine (double position);

/******************************************************************************
* Locking                                                                     *
******************************************************************************/
enum BR_LockElements
{
	TIME_SEL          = 1,
	ITEM_FULL         = 2,
	ITEM_HORZ_MOVE    = 64,
	ITEM_VERT_MOVE    = 128,
	ITEM_EDGES        = 256,
	FADES_VOL_HANDLE  = 512,
	STRETCH_MARKERS   = 4096,
	TAKE_ENV          = 2048,
	TRACK_ENV         = 4,
	REGIONS           = 16,
	MARKERS           = 8,
	TEMPO_MARKERS     = 32,
	LOOP_POINTS       = 1024
};
bool IsLockingActive ();
bool IsLocked (int lockElements);
bool IsItemLocked (MediaItem* item);

/******************************************************************************
* Height                                                                      *
******************************************************************************/
int GetTrackHeightFromVZoomIndex (MediaTrack* track, int vZoom); // also takes track compacting into account
int GetEnvHeightFromTrackHeight (int trackHeight);               // what is envelope height in case its height override is 0 ?
int GetMasterTcpGap ();
int GetTrackHeight (MediaTrack* track, int* offsetY, int* topGap = NULL, int* bottomGap = NULL);
int GetItemHeight (MediaItem* item, int* offsetY);
int GetTakeHeight (MediaItem_Take* take, int* offsetY);
int GetTakeHeight (MediaItem* item, int id, int* offsetY);
int GetTakeEnvHeight (MediaItem_Take* take, int* offsetY);
int GetTakeEnvHeight (MediaItem* item, int id, int* offsetY);
int GetTrackEnvHeight (TrackEnvelope* envelope, int* offsetY, bool drawableRangeOnly, MediaTrack* parent = NULL);

/******************************************************************************
* Arrange                                                                     *
******************************************************************************/
void MoveArrange (double amountTime);
void GetSetArrangeView (ReaProject* proj, bool set, double* start, double* end);
void CenterArrange (double position);
void SetArrangeStart (double start);
void MoveArrangeToTarget (double target, double reference);
void ScrollToTrackIfNotInArrange (MediaTrack* track);
bool IsOffScreen (double position);
RECT GetDrawableArrangeArea ();

/******************************************************************************
* Window                                                                      *
******************************************************************************/
HWND FindReaperWndByName (const char* name);
HWND FindFloatingToolbarWndByName (const char* toolbarName);
HWND GetArrangeWnd ();
HWND GetArrangeWnd_new();
HWND GetRulerWndAlt ();
HWND GetRulerWndAlt_new();
HWND GetTransportWnd ();
HWND GetMixerWnd ();
HWND GetMixerMasterWnd (HWND mixer);
HWND GetMediaExplorerWnd ();
HWND GetMcpWnd (bool &isContainer);
HWND GetTcpWnd (bool &isContainer);
HWND GetTcpTrackWnd (MediaTrack* track, bool &isContainer);
HWND GetNotesView (HWND midiEditor);
HWND GetNotesView_new(HWND midiEditor);
HWND GetPianoView (HWND midiEditor);
HWND GetPianoView_new(HWND midiEditor);
HWND GetTrackView (HWND midiEditor);
MediaTrack* HwndToTrack (HWND hwnd, int* hwndContext, POINT ptScreen);  // context: 0->unknown, 1->TCP, 2->MCP (works even if hwnd is not a track but something else in mcp/tcp). 
TrackEnvelope* HwndToEnvelope (HWND hwnd, POINT ptScreen);
void CenterDialog (HWND hwnd, HWND target, HWND zOrder);
void GetMonitorRectFromPoint (const POINT& p, bool workingAreaOnly, RECT* monitorRect);
void GetMonitorRectFromRect (const RECT& r, bool workingAreaOnly, RECT* monitorRect);
void BoundToRect (const RECT& boundingRect, RECT* r);
void CenterOnPoint (RECT* rect, const POINT& point, int horz, int ver, int xOffset, int yOffset); // horz -> -1 left, 0 center, 1 right ..... vert -> -1 bottom, 0 center, 1 top (every other value will mean to do nothing)
void SimulateMouseClick (HWND hwnd, POINT point, bool keepCurrentFocus);
bool IsFloatingTrackFXWindow (HWND hwnd);

/******************************************************************************
* Menus                                                                       *
******************************************************************************/
set<int> GetAllMenuIds (HMENU hMenu);
int GetUnusedMenuId (HMENU hMenu);

/******************************************************************************
* File paths                                                                  *
******************************************************************************/
const char* GetReaperMenuIni ();
const char* GetIniFileBR ();

/******************************************************************************
* Theming                                                                     *
******************************************************************************/
void DrawTooltip (LICE_IBitmap* bm, const char* text);
void SetWndIcon (HWND hwnd); // win32 only
bool ThemeListViewInProc (HWND hwnd, int uMsg, LPARAM lParam, HWND list, bool grid);

/******************************************************************************
* Cursors                                                                     *
******************************************************************************/
enum BR_MouseCursor
{
	CURSOR_ENV_PEN_GRID = 0,
	CURSOR_ENV_PT_ADJ_VERT,
	CURSOR_GRID_WARP,
	CURSOR_MISC_SPEAKER,
	CURSOR_ZOOM_DRAG,
	CURSOR_ZOOM_IN,
	CURSOR_ZOOM_OUT,
	CURSOR_ZOOM_UNDO,
	CURSOR_ERASER,

	CURSOR_COUNT
};
HCURSOR GetSwsMouseCursor (BR_MouseCursor cursor);

/******************************************************************************
* Height helpers (exposed here for usage outside of BR_Util.h)                *
******************************************************************************/
void GetTrackGap (int trackHeight, int* top, int* bottom);
bool IsLastTakeTooTall (int itemHeight, int averageTakeHeight, int effectiveTakeCount, int* lastTakeHeight);
int GetMinTakeHeight (MediaTrack* track, int takeCount, int trackHeight, int itemHeight);
int GetItemHeight (MediaItem* item, int* offsetY, int trackHeight, int trackOffsetY);
int GetTakeHeight (MediaItem_Take* take, MediaItem* item, int id, int* offsetY, bool averagedLast);                                   // pass either take or
int GetTakeHeight (MediaItem_Take* take, MediaItem* item, int id, int* offsetY, bool averagedLast, int trackHeight, int trackOffset); // item and take id
