/******************************************************************************
/ XenakiosExts.h
/
/ Copyright (c) 2009 Tim Payne (SWS), original code by Xenakios
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

#pragma once

using namespace std;

//#define _XEN_LOG

//===========================================================
// AutoRename.cpp
void DoAutoRename(COMMAND_T*);

//===========================================================
// BroadCastWavCommands.cpp
void DoRenameTakesWithBWAVDesc(COMMAND_T*);
void DoRenameTakeDlg(COMMAND_T*);
void DoOpenRPPofBWAVdesc(COMMAND_T*);
void DoNudgeSectionLoopStartPlus(COMMAND_T*);
void DoNudgeSectionLoopStartMinus(COMMAND_T*);
void DoNudgeSectionLoopLenPlus(COMMAND_T*);
void DoNudgeSectionLoopLenMinus(COMMAND_T*);
void DoNudgeSectionLoopOlapPlus(COMMAND_T*);
void DoNudgeSectionLoopOlapMinus(COMMAND_T*);

//===========================================================
// CommandRegistering.cpp
extern COMMAND_T g_XenCommandTable[];

//===========================================================
// CreateTrax.cpp
void DoCreateTraxDlg(COMMAND_T*);

//===========================================================
// DiskSpaceCalculator.cpp
void DoShowDiskspaceCalc(COMMAND_T*);

//===========================================================
// Envelope_actions.cpp
void DoShiftEnvelope(COMMAND_T*);

//===========================================================
// ExoticCommands.cpp
void DoJumpEditCursorByRandomAmount(COMMAND_T*);
void DoNudgeSelectedItemsPositions(bool UseConfig,bool Positive,double NudgeTime);
void DoSetItemFadesConfLen(COMMAND_T*);
void DoNudgeItemPitches(double NudgeAmount,bool Resampled);
void DoNudgeItemPitchesDown(COMMAND_T*);
void DoNudgeItemPitchesUp(COMMAND_T*);
void DoNudgeItemPitchesDownB(COMMAND_T*);
void DoNudgeItemPitchesUpB(COMMAND_T*);
void DoNudgeUpTakePitchResampledA(COMMAND_T*);
void DoNudgeDownTakePitchResampledA(COMMAND_T*);
void DoNudgeUpTakePitchResampledB(COMMAND_T*);
void DoNudgeDownTakePitchResampledB(COMMAND_T*);
void DoNudgeItemsBeatsBased(bool UseConf,bool Positive,double theNudgeAmount);
void DoNudgeItemsLeftBeatsAndConfBased(COMMAND_T*);
void DoNudgeItemsRightBeatsAndConfBased(COMMAND_T*);
void DoNudgeSamples(COMMAND_T*);
void DoSplitItemsAtTransients(COMMAND_T*);
void DoNudgeItemVols(bool UseConf,bool Positive,double TheNudgeAmount);
void DoNudgeItemVolsDown(COMMAND_T*);
void DoNudgeItemVolsUp(COMMAND_T*);
void DoNudgeTakeVols(bool UseConf,bool Positive,double TheNudgeAmount);
void DoNudgeTakeVolsDown(COMMAND_T*);
void DoNudgeTakeVolsUp(COMMAND_T*);
void DoResetItemVol(COMMAND_T*);
void DoResetTakeVol(COMMAND_T*);
void DoScaleItemPosStaticDlg(COMMAND_T*);
void DoRandomizePositionsDlg(COMMAND_T*);
void DoShowTakeMixerDlg(COMMAND_T*);
void DoHoldKeyTest1(COMMAND_T*);
void DoHoldKeyTest2(COMMAND_T*);
void DoInsertMediaFromClipBoard(COMMAND_T*);
void TokenizeString(const string& str,vector<string>&tokens,const string& delimiters = " ");
void DoSearchTakesDLG(COMMAND_T*);
void DoMoveCurConfPixRight(COMMAND_T*);
void DoMoveCurConfPixLeft(COMMAND_T*);
void DoMoveCurConfPixRightCts(COMMAND_T*);
void DoMoveCurConfPixLeftCts(COMMAND_T*);
void DoMoveCurConfSecsLeft(COMMAND_T*);
void DoMoveCurConfSecsRight(COMMAND_T*);
void DoStoreEditCursorPosition(COMMAND_T*);
void DoRecallEditCursorPosition(COMMAND_T*);

//===========================================================
// FloatingInspector.cpp
extern HWND g_hItemInspector;
WDL_DLGRET MyItemInspectorDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
void DoTglFltItemInspector(COMMAND_T*);

//===========================================================
// fractions.cpp
double parseFrac(const char *buf);
double GetBeatValueFromTable(int indx);
void InitFracBox(HWND hwnd, const char *buf);

//===========================================================
// ItemTakeCommands.cpp
int XenGetProjectItems(vector<MediaItem*>& TheItems,bool OnlySelectedItems=true, bool IncEmptyItems=false);
int XenGetProjectTakes(vector<MediaItem_Take*>& TheTakes,bool OnlyActive,bool OnlyFromSelectedItems);
void DoMoveItemsLeftByItemLen(COMMAND_T*);
void DoToggleTakesNormalize(COMMAND_T*);
void DoShowItemVolumeDialog(COMMAND_T*);
void DoShowVolPanDialog(COMMAND_T*);
void DoChooseNewSourceFileForSelTakes(COMMAND_T*);
void DoInvertItemSelection(COMMAND_T*);
bool DoLaunchExternalTool(const char *ExeFilename);
void DoRepeatPaste(COMMAND_T*);
void DoSkipSelectAllItemsOnTracks(COMMAND_T*);
void DoSkipSelectFromSelectedItems(COMMAND_T*);
void DoShuffleSelectTakesInItems(COMMAND_T*);
void DoMoveItemsToEditCursor(COMMAND_T*);
void DoRemoveItemFades(COMMAND_T*);
void DoTrimLeftEdgeToEditCursor(COMMAND_T*);
void DoTrimRightEdgeToEditCursor(COMMAND_T*);
void DoResetItemRateAndPitch(COMMAND_T*);
void DoApplyTrackFXStereoAndResetVol(COMMAND_T*);
void DoApplyTrackFXMonoAndResetVol(COMMAND_T*);
void DoSelItemsToEndOfTrack(COMMAND_T*);
void DoSelItemsToStartOfTrack(COMMAND_T*);
void DoPanTakesSymmetricallyWithUndo(COMMAND_T*);
void DoImplodeTakesSetPlaySetSymPans(COMMAND_T*);
void DoRenderItemsWithTail(COMMAND_T*);
void DoOpenAssociatedRPP(COMMAND_T*);
void DoReposItemsDlg(COMMAND_T*);
void DoSpeadSelItemsOverNewTx(COMMAND_T*);
void DoOpenInExtEditor1(COMMAND_T*);
void DoOpenInExtEditor2(COMMAND_T*);
void DoMatrixItemImplode(COMMAND_T*);
void DoSwingItemPositions(COMMAND_T*);
void DoTimeSelAdaptDelete(COMMAND_T*);
void DoDeleteMutedItems(COMMAND_T*);

//===========================================================
// main.cpp
int IsRippleOneTrack(COMMAND_T*);
int IsRippleAll(COMMAND_T*);
void DoToggleRippleOneTrack(COMMAND_T*);
void DoToggleRippleAll(COMMAND_T*);
void DoSelectFiles(COMMAND_T*);
void DoInsertRandom(COMMAND_T*);
void DoInsRndFileRndLen(COMMAND_T*);
void DoInsRndFileAtTimeSel(COMMAND_T*);
void DoInsRndFileRndOffset(COMMAND_T*);
void DoInsRndFileRndOffsetAtTimeSel(COMMAND_T*);
void DoRoundRobinSelectTakes(COMMAND_T*);
void DoSelectFirstTakesInItems(COMMAND_T*);
void DoSelectLastTakesInItems(COMMAND_T*);
void DoInsertShuffledRandomFile(COMMAND_T*);
void DoNudgeItemsLeftSecsAndConfBased(COMMAND_T*);
void DoNudgeItemsRightSecsAndConfBased(COMMAND_T*);
void DoSaveMarkersAsTextFile(COMMAND_T*);
void DoResampleTakeOneSemitoneDown(COMMAND_T*);
void DoResampleTakeOneSemitoneUp(COMMAND_T*);
void DoLoopAndPlaySelectedItems(COMMAND_T*);
void DoPlayItemsOnce(COMMAND_T*);
void DoMoveCurNextTransMinusFade(COMMAND_T*);
void DoMoveCurPrevTransMinusFade(COMMAND_T*);
void DoMoveCursor10pixRight(COMMAND_T*);
void DoMoveCursor10pixLeft(COMMAND_T*);
void DoMoveCursor10pixLeftCreateSel(COMMAND_T*);
void DoMoveCursor10pixRightCreateSel(COMMAND_T*);
void ItemPreviewPlayState (bool play, bool rec);
void ItemPreview (int mode, MediaItem* item, MediaTrack* track, double volume, double startOffset, double measureSync, bool pauseDuringPrev);
void DoPreviewSelectedItem(COMMAND_T*);
void DoScrollTVPageDown(COMMAND_T*);
void DoScrollTVPageUp(COMMAND_T*);
void DoScrollTVHome(COMMAND_T*);
void DoScrollTVEnd(COMMAND_T*);
void DoRenameMarkersWithAscendingNumbers(COMMAND_T*);
int IsStopAtEndOfTimeSel(COMMAND_T*);
void DoToggleSTopAtEndOfTimeSel(COMMAND_T*);
extern WDL_String g_XenIniFilename;

int XenakiosInit();
void XenakiosExit();

//===========================================================
// MediaDialog.cpp
string RemoveDoubleBackSlashes(string TheFileName);
void DoShowProjectMediaDlg(COMMAND_T*);
void DoFindMissingMedia(COMMAND_T*);

//===========================================================
// MixerActions.cpp
extern project_config_extension_t xen_reftrack_pcreg;
typedef vector<MediaTrack*> t_vect_of_Reaper_tracks;
int XenGetProjectTracks(t_vect_of_Reaper_tracks& RefVecTracks, bool OnlySelected);
void DoSelectNextTrack(COMMAND_T*);
void DoSelectPreviousTrack(COMMAND_T*);
void DoSelectNextTrackKeepCur(COMMAND_T*);
void DoSelectPrevTrackKeepCur(COMMAND_T*);
void DoTraxPanLaw(COMMAND_T*);
void DoTraxRecArmed(COMMAND_T*);
void DoBypassFXofSelTrax(COMMAND_T*);
void DoResetTracksVolPan(COMMAND_T*);
void DoSetSymmetricalpansL2R(COMMAND_T*);
void DoSetSymmetricalpansR2L(COMMAND_T*);
void DoPanTracksRandom(COMMAND_T*);
void DoSetSymmetricalpans(COMMAND_T*);
void DoSelRandomTrack(COMMAND_T*);
void DoSelTraxHeight(COMMAND_T*);
void DoStoreSelTraxHeights(COMMAND_T*);
void DoRecallSelectedTrackHeights(COMMAND_T*);
void DoSelectTracksWithNoItems(COMMAND_T*);
void DoSelectTracksContainingBuss(COMMAND_T*);
void DoUnSelectTracksContainingBuss(COMMAND_T*);
void DoSetSelectedTrackNormal(COMMAND_T*);
void DoSetSelectedTracksAsFolder(COMMAND_T*);
void DoRenameTracksDlg(COMMAND_T*);
void DoSelectFirstOfSelectedTracks(COMMAND_T*);
void DoSelectLastOfSelectedTracks(COMMAND_T*);
void DoInsertNewTrackAtTop(COMMAND_T*);
void DoLabelTraxDefault(COMMAND_T*);
void DoTraxLabelPrefix(COMMAND_T*);
void DoTraxLabelSuffix(COMMAND_T*);
void DoMinMixSendPanelH(COMMAND_T*);
void DoMinMixSendAndFxPanelH(COMMAND_T*);
void DoMaxMixFxPanHeight(COMMAND_T*);
void DoRemoveTimeSelectionLeaveLoop(COMMAND_T*);
void DoToggleTrackHeightAB(COMMAND_T*);
void DoPanTracksCenter(COMMAND_T*);
void DoPanTracksLeft(COMMAND_T*);
void DoPanTracksRight(COMMAND_T*);
void DoSetTrackVolumeToZero(COMMAND_T*);
void DoRenderReceivesAsStems(COMMAND_T*);
void DoSetRenderSpeedToRealtime2(COMMAND_T*);
void DoSetRenderSpeedToNonLim(COMMAND_T*);
void DoStoreRenderSpeed(COMMAND_T*);
void DoRecallRenderSpeed(COMMAND_T*);
void DoSetSelTrackAsRefTrack(COMMAND_T*);
int IsRefTrack(COMMAND_T*);
void DoToggleReferenceTrack(COMMAND_T*);
void DoNudgeMasterVol1dbUp(COMMAND_T*);
void DoNudgeMasterVol1dbDown(COMMAND_T*);
void DoNudgeSelTrackVolumeUp(COMMAND_T*);
void DoNudgeSelTrackVolumeDown(COMMAND_T*);
void DoSetMasterToZeroDb(COMMAND_T*);

//===========================================================
// MoreItemCommands.cpp
extern void(*g_KeyUpUndoHandler)();
void DoItemPosRemapDlg(COMMAND_T*);
void ExtractFileNameEx(const char *FullFileName,char *Filename,bool StripExtension);
void DoSwitchItemToNextCue(COMMAND_T*);
void DoSwitchItemToPreviousCue(COMMAND_T*);
void DoSwitchItemToNextCuePresvLen(COMMAND_T*);
void DoSwitchItemToPreviousCuePresvLen(COMMAND_T*);
void DoSwitchItemToFirstCue(COMMAND_T*);
void DoSwitchItemToRandomCue(COMMAND_T*);
void DoSwitchItemToRandomCuePresvLen(COMMAND_T*);
void DoStoreSelectedTakes(COMMAND_T*);
void DoRecallSelectedTakes(COMMAND_T*);
void DoDeleteItemAndMedia(COMMAND_T*);
int SendFileToRecycleBin(const char *FileName);
void DoDelSelItemAndSendActiveTakeMediaToRecycler(COMMAND_T*);
void DoNukeTakeAndSourceMedia(COMMAND_T*);
void DoDeleteActiveTakeAndRecycleSourceMedia(COMMAND_T*);
void DoSelectFirstItemInSelectedTrack(COMMAND_T*);
void DoSetNextFadeInShape(COMMAND_T*);
void DoSetPrevFadeInShape(COMMAND_T*);
void DoSetNextFadeOutShape(COMMAND_T*);
void DoSetPrevFadeOutShape(COMMAND_T*);
void DoSetFadeToCrossfade(COMMAND_T*);
void DoSetFadeToDefaultFade(COMMAND_T*);
void DoItemPitch2Playrate(COMMAND_T*);
void DoItemPlayrate2Pitch(COMMAND_T*);
void DoSpreadSelItemsOver4Tracks(COMMAND_T*);
void DoShowSpreadItemsDlg(COMMAND_T*);
void DoToggleSelectedItemsRndDlg(COMMAND_T*);
void InitUndoKeyUpHandler01();
void RemoveUndoKeyUpHandler01();
void DoSlipItemContentsOneSampleLeft(COMMAND_T*);
void DoSlipItemContentsOneSampleRight(COMMAND_T*);
void DoReplaceItemFileWithNextInFolder(COMMAND_T*);
void DoReplaceItemFileWithPrevInFolder(COMMAND_T*);
void DoReplaceItemFileWithRandomInFolder(COMMAND_T*);
void DoReplaceItemFileWithNextRPPInFolder(COMMAND_T*);
void DoReplaceItemFileWithPrevRPPInFolder(COMMAND_T*);
void DoReverseItemOrder(COMMAND_T*);
void DoShuffleItemOrder(COMMAND_T*);
void DoShuffleItemOrder2(COMMAND_T*);
void DoCreateMarkersFromSelItems1(COMMAND_T*);
void DoSaveItemAsFile1(COMMAND_T*);
void DoSelectItemUnderEditCursorOnSelTrack(COMMAND_T*);
void DoNormalizeSelTakesTodB(COMMAND_T*);
void DoResetItemsOffsetAndLength(COMMAND_T*);
void DoFadesOfSelItemsToConfC(COMMAND_T*);
void DoFadesOfSelItemsToConfD(COMMAND_T*);
void DoFadesOfSelItemsToConfE(COMMAND_T*);
void DoFadesOfSelItemsToConfF(COMMAND_T*);



//===========================================================
// Parameters.cpp -- see parameters.h!

//===========================================================
// PropertyInterpolator.cpp
void DoShowItemInterpDLG(COMMAND_T*);

//===========================================================
// TakeRenaming.cpp
void DoRenameTakeAndSourceFileDialog(COMMAND_T*);
void DoRenameTakeDialog666(COMMAND_T*);
void DoRenameTakeAllDialog666(COMMAND_T*);

//===========================================================
// TrackTemplateActions.cpp
void SplitFileNameComponents(string FullFileName,vector<string>& FNComponents);
void DoOpenTrackTemplate(COMMAND_T*);
void DoOpenProjectTemplate(COMMAND_T*);

//===========================================================
// XenQueryDlg.cpp
int XenSingleStringQueryDlg(HWND hParent,const char *QueryTitle,char *QueryResult,int maxchars);

//===========================================================
// XenUtils.cpp functions, globals:
extern std::vector<std::string> g_filenames;
extern char g_CurrentScanFile[1024];
extern bool g_bAbortScan;
int GetActiveTakes(WDL_PtrList<MediaItem_Take> *MediaTakes);
int SearchDirectory(vector<string> &refvecFiles, const char* cDir, const char* cExt, bool bSubdirs = true);
// Browse fcns to match SWELL
#ifdef _WIN32
bool BrowseForSaveFile(const char *text, const char *initialdir, const char *initialfile, const char *extlist, char *fn, int fnsize);
char *BrowseForFiles(const char *text, const char *initialdir, const char *initialfile, bool allowmul, const char *extlist);
bool BrowseForDirectory(const char *text, const char *initialdir, char *fn, int fnsize);
#endif
bool FileExists(const char* file);
