/******************************************************************************
/ BR_Misc.h
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

/******************************************************************************
* Commands: Misc continuous actions                                           *
******************************************************************************/
void PlaybackAtMouseCursorInit ();

/******************************************************************************
* Commands: Misc                                                              *
******************************************************************************/
void ToggleMousePlayback (COMMAND_T*);
void PlaybackAtMouseCursor (COMMAND_T*);
void SplitItemAtTempo (COMMAND_T*);
void SplitItemAtStretchMarkers (COMMAND_T*);
void MarkersAtTempo (COMMAND_T*);
void MarkersAtNotes (COMMAND_T*);
void MarkersAtStretchMarkers (COMMAND_T*);
void MarkerAtMouse (COMMAND_T*);
void MarkersRegionsAtItems (COMMAND_T*);
void MoveClosestMarker (COMMAND_T*);
void MidiItemTempo (COMMAND_T*);
void MidiItemTrim (COMMAND_T*);
void FocusArrangeTracks (COMMAND_T*);
void MoveActiveWndToMouse (COMMAND_T*);
void ToggleItemOnline (COMMAND_T*);
void ItemSourcePathToClipBoard (COMMAND_T*);
void DeleteTakeUnderMouse (COMMAND_T*);
void SelectTrackUnderMouse (COMMAND_T*);
constexpr int ObeyTimeSelection = 1<<31;
void SelectItemsByType (COMMAND_T*);
void SaveCursorPosSlot (COMMAND_T*);
void RestoreCursorPosSlot (COMMAND_T*);
void SaveItemMuteStateSlot (COMMAND_T*);
void RestoreItemMuteStateSlot (COMMAND_T*);
void SaveTrackSoloMuteStateSlot (COMMAND_T*);
void RestoreTrackSoloMuteStateSlot (COMMAND_T*);

/******************************************************************************
* Commands: Misc - REAPER preferences                                         *
******************************************************************************/
void SnapFollowsGridVis (COMMAND_T*);
void PlaybackFollowsTempoChange (COMMAND_T*);
void TrimNewVolPanEnvs (COMMAND_T*);
void ToggleDisplayItemLabels (COMMAND_T*);
void SetMidiResetOnPlayStop (COMMAND_T*);
void SetOptionsFX (COMMAND_T*);
void SetMoveCursorOnPaste (COMMAND_T*);
void SetPlaybackStopOptions (COMMAND_T*);
void SetGridMarkerZOrder (COMMAND_T*);
void CycleRecordModes (COMMAND_T*);
void SetAutoStretchMarkers (COMMAND_T*);

/******************************************************************************
* Commands: Misc - Media item preview                                         *
******************************************************************************/
void PreviewItemAtMouse (COMMAND_T*);

/******************************************************************************
* Commands: Misc - Adjust playrate                                            *
******************************************************************************/
void AdjustPlayrate (COMMAND_T*, int, int, int, HWND);

/******************************************************************************
* Commands: Misc - Project track selection action                             *
******************************************************************************/
int ProjectTrackSelInitExit (bool init);
void ExecuteTrackSelAction ();
void SetProjectTrackSelAction (COMMAND_T*);
void ShowProjectTrackSelAction (COMMAND_T*);
void ClearProjectTrackSelAction (COMMAND_T*);
SWSProjConfig<WDL_FastString>* GetProjectTrackSelectionAction(); // for ReaScript export

/******************************************************************************
* Toggle states: Misc                                                         *
******************************************************************************/
int IsSnapFollowsGridVisOn (COMMAND_T*);
int IsPlaybackFollowsTempoChangeOn (COMMAND_T*);
int IsTrimNewVolPanEnvsOn (COMMAND_T*);
int IsToggleDisplayItemLabelsOn (COMMAND_T*);
int IsSetMidiResetOnPlayStopOn (COMMAND_T*);
int IsSetOptionsFXOn (COMMAND_T*);
int IsSetMoveCursorOnPasteOn (COMMAND_T*);
int IsSetPlaybackStopOptionsOn (COMMAND_T*);
int IsSetGridMarkerZOrderOn (COMMAND_T*);
int IsSetAutoStretchMarkersOn (COMMAND_T*);
int IsAdjustPlayrateOptionsVisible (COMMAND_T*);
int IsTitleBarDisplayOptionOn (COMMAND_T*);
