/******************************************************************************
/ BR_Loudness.h
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
#pragma once
#include "BR_EnvelopeUtil.h"

/******************************************************************************
* Loudness object                                                             *
******************************************************************************/
class BR_LoudnessObject
{
public:
	BR_LoudnessObject ();
	explicit BR_LoudnessObject (MediaTrack* track);
	explicit BR_LoudnessObject (MediaItem_Take* take);
	~BR_LoudnessObject ();

	/* Analyze */
	bool Analyze (bool integratedOnly, bool doTruePeak, bool doHighPrecisionMode, bool doDualMonoMode);
	void AbortAnalyze ();
	bool IsRunning ();
	double GetProgress ();

	/* For populating list view in analyze loudness dialog */
	double GetColumnVal (int column, int mode);                    // mode: 0->LUFS, 1->LU (LU will follow global format settings)
	void GetColumnStr (int column, char* str, int strSz, int mode);

	/* For serializing */
	void SaveObject (ProjectStateContext* ctx);
	bool RestoreObject (ProjectStateContext* ctx);

	/* Various options and manipulation (mainly for analyze loudness dialog) */
	double GetAudioLength ();
	double GetAudioStart ();
	double GetAudioEnd ();
	double GetMaxMomentaryPos (bool projectTime);
	double GetMaxShorttermPos (bool projectTime);
	double GetTruePeakPos (bool projectTime);
	bool IsTrack ();
	bool IsTargetValid ();
	bool CheckTarget (MediaTrack* track);
	bool CheckTarget (MediaItem_Take* take);
	bool IsSelectedInProject ();
	bool CreateGraph (BR_Envelope& envelope, double minLUFS, double maxLUFS, bool momentary, bool highPrecisionMode, HWND warningHwnd = g_hwndParent);
	bool NormalizeIntegrated (double targetLUFS); // uses existing analyze data
	void SetSelectedInProject (bool selected);
	void GoToTarget ();
	void GoToMomentaryMax (bool timeSelection);
	void GoToShortTermMax (bool timeSelection);
	void GoToTruePeak ();
	int GetTrackNumber ();
	int GetItemNumber ();

	// NF: functions made public for ReaScript access
	void GetAnalyzeData(double* integrated, double* range, double* truePeak, double* truePeakPos, double* shortTermMax, double* momentaryMax, vector<double>* shortTermValues, vector<double>* momentaryValues);
	void SetDoTruePeak(bool doTruePeak);
	bool GetDoTruePeak();
	// 'high precision' analyzing mode
	// 100 Hz refresh rate / 10 ms buffer  (rather than 5 Hz / 200 ms) for BS.1770-4 compliance (correct max. momentary calc.),
	// see https://github.com/jiixyj/libebur128/issues/93#issuecomment-429043548
	void SetDoHighPrecisionMode(bool doHighPrecisionMode);
	bool GetDoHighPrecisionMode();
	void SetDoDualMonoMode(bool doDualMonoMode);
	bool GetDoDualMonoMode();

private:
	struct AudioData
	{
		AudioAccessor* audio;
		char audioHash[128];
		int samplerate, channels, channelMode;
		double audioStart, audioEnd;
		double volume, pan;
		BR_Envelope volEnv, volEnvPreFX;
		AudioData();
	};

	static unsigned WINAPI AnalyzeData (void* loudnessObject);
	int CheckSetAudioData (); // call from the main thread only, returns 0->target doesn't exist anymore, 1->old accessor still valid, 2->accessor got updated
	void SetAudioData (const AudioData& audioData);
	AudioData GetAudioData ();
	void SetRunning (bool running);
	void SetProgress (double progress);
	void SetAnalyzeData (double integrated, double range, double truePeak, double truePeakPos, double shortTermMax, double momentaryMax, const vector<double>& shortTermValues, const vector<double>& momentaryValues);	
	void SetAnalyzedStatus (bool analyzed);
	bool GetAnalyzedStatus ();
	void SetIntegratedOnly (bool integratedOnly);
	bool GetIntegratedOnly ();
	void SetTruePeakAnalyzed (bool analyzed);
	bool GetTruePeakAnalyzeStatus ();
	void SetKillFlag (bool killFlag);
	bool GetKillFlag ();
	void SetProcess (HANDLE process);
	HANDLE GetProcess ();
	WDL_FastString GetTakeName ();
	WDL_FastString GetTrackName ();
	MediaItem* GetItem ();
	MediaTrack* GetTrack ();
	MediaItem_Take* GetTake ();
	GUID GetGuid ();
	void SetTrack (MediaTrack* track);
	void SetTake (MediaItem_Take* take);
	void SetGuid (GUID guid);
	void operator= (const BR_LoudnessObject&);    // no need for assignment or copy constructor so disable
	BR_LoudnessObject (const BR_LoudnessObject&); // for now! (it could destroy perfectly valid accessor)

	AudioData m_audioData;
	MediaTrack* m_track;
	MediaItem_Take* m_take;
	GUID m_guid;
	double m_integrated, m_truePeak, m_truePeakPos, m_shortTermMax, m_momentaryMax, m_range;
	double m_progress;
	bool m_running, m_analyzed, m_killFlag, m_integratedOnly, m_doTruePeak, m_truePeakAnalyzed, m_doHighPrecisionMode, m_doDualMonoMode;
	HANDLE m_process;
	SWS_Mutex m_mutex;
	vector<double> m_shortTermValues;
	vector<double> m_momentaryValues;
};

/******************************************************************************
* Loudness preferences                                                        *
******************************************************************************/
class BR_LoudnessPref
{
public:
	/* No constructor - singleton design */
	static BR_LoudnessPref& Get ();

	/* These obey active project preferences */
	double GetGraphMin ();
	double GetGraphMax ();
	double GetReferenceLU ();
	double LUtoLUFS (double lu);
	double LUFStoLU (double lufs);
	WDL_FastString GetFormatedLUString ();

	/* Preference dialog */
	void ShowPreferenceDlg (bool show);
	void UpdatePreferenceDlg ();
	bool IsPreferenceDlgVisible ();

	/* Global and project state saving */
	void SaveGlobalPref ();
	void LoadGlobalPref ();
	void SaveProjPref  (ProjectStateContext *ctx);
	void LoadProjPref  (ProjectStateContext *ctx);
	void CleanProjPref ();

private:
	struct ProjData
	{
		ProjData ();
		bool useProjLU, useProjGraph;
		double valueLU, graphMin, graphMax;
		WDL_FastString stringLU;
	};
	enum LUFormat        {LU = 0, LU_AT_K, LU_K, K, LU_FORMAT_COUNT};
	enum PrefWndMessages {READ_PROJDATA = 0xF001, SAVE_PROJDATA, UPDATE_LU_FORMAT_PREVIEW};

	BR_LoudnessPref ();
	BR_LoudnessPref (const BR_LoudnessPref&);
	void operator=  (const BR_LoudnessPref&);
	WDL_FastString GetFormatedLUString (int mode, double* valueLU = NULL); // see LUFormat for modes (valueLU is used only in ProjData() constructor)
	static WDL_DLGRET GlobalLoudnessPrefProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	SWSProjConfig<BR_LoudnessPref::ProjData> m_projData;
	HWND m_prefWnd;
	double m_valueLU, m_graphMin, m_graphMax;
	int m_globalLUFormat;
};

/******************************************************************************
* Normalize loudness                                                          *
******************************************************************************/
struct BR_NormalizeData
{
	WDL_PtrList<BR_LoudnessObject>* items;
	double targetLufs;
	bool quickMode;   // analyze integrated loudness only (used when normalizing with actions)
	bool normalized;  // did items get successfully normalized?
};

void NormalizeAndShowProgress (BR_NormalizeData* normalizeData);

// #880
void NFAnalyzeItemsLoudnessAndShowProgress(BR_NormalizeData* normalizeData);

/******************************************************************************
* Analyze loudness list view                                                  *
******************************************************************************/
class BR_AnalyzeLoudnessView : public SWS_ListView
{
public:
	BR_AnalyzeLoudnessView (HWND hwndList, HWND hwndEdit);

protected:
	virtual void GetItemText (SWS_ListItem* item, int iCol, char* str, int iStrMax);
	virtual void GetItemList (SWS_ListItemList* pList);
	virtual void OnItemSelChanged (SWS_ListItem* item, int iState);
	virtual void OnItemDblClk (SWS_ListItem* item, int iCol);
	virtual void OnItemSortEnd ();
	virtual int OnItemSort (SWS_ListItem* item1, SWS_ListItem* item2);
};

/******************************************************************************
* Analyze loudness window                                                     *
******************************************************************************/
class BR_AnalyzeLoudnessWnd : public SWS_DockWnd
{
public:
	BR_AnalyzeLoudnessWnd ();
	~BR_AnalyzeLoudnessWnd ();

	void Update (bool updateList = true);
	void KillNormalizeDlg ();
	bool GetProperty (int propertySpecifier);
	enum PropertySpecifier { TIME_SEL_OVER_MAX, DOUBLECLICK_GOTO_TARGET, USING_LU, MIRROR_PROJ_SELECTION, DO_HIGH_PRECISION_MODE, DO_DUAL_MONO_MODE };
	// NF: for setting 'use high precision mode' / other Loudness options from actions (or ReaScript in future)
	// returns true if Property is set successfully
	bool SetProperty (int propertySpecifier, bool newVal); 
	void LoadProperties();
	void SaveProperties();
	void ClearList(); // NF: made public to be callable when toggling high precision mode via action to force re-analyzing
	
protected:
	BR_LoudnessObject* IsObjectInList (MediaTrack* track);
	BR_LoudnessObject* IsObjectInList (MediaItem_Take* take);
	void AbortAnalyze ();
	void AbortReanalyze ();
	void SetAnalyzing (bool, bool reanalyze);
	void ShowExportFormatDialog (bool show);
	void ShowNormalizeDialog (bool show);
	void SaveRecentFormatPattern (WDL_FastString pattern);
	WDL_FastString GetRecentFormatPattern (int id);
	WDL_FastString GetRecentFormatPatternKey (int id);
	WDL_FastString CreateExportString (bool previewOnly);
	static WDL_DLGRET ExportFormatDialogProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static WDL_DLGRET NormalizeDialogProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void OnInitDlg ();
	virtual void OnCommand (WPARAM wParam, LPARAM lParam);
	virtual void OnTimer (WPARAM wParam);
	virtual void OnDestroy ();
	virtual void GetMinSize (int* w, int* h);
	virtual int OnKey (MSG* msg, int iKeyState);
	virtual HMENU OnContextMenu (int x, int y, bool* wantDefaultItems);
	virtual bool ReprocessContextMenu();
	virtual INT_PTR OnUnhandledMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);

	struct Properties
	{
		bool analyzeTracks;
		bool analyzeOnNormalize;
		bool mirrorProjSelection;
		bool doubleClickGoToTarget;
		bool timeSelOverMax;
		bool clearEnvelope;
		bool clearAnalyzed;
		bool doTruePeak;
		bool usingLU;
		bool doHighPrecisionMode;
		bool doDualMonoMode;
		WDL_FastString exportFormat;
		Properties ();
		void Load ();
		void Save ();
	} m_properties;
	double m_objectsLen;
	int m_currentObjectId;
	bool m_analyzeInProgress;
	BR_AnalyzeLoudnessView* m_list;
	HWND m_normalizeWnd, m_exportFormatWnd;                                          // never delete objects in reanalyzeQueue when removing them from list!!
	WDL_PtrList_DeleteOnDestroy<BR_LoudnessObject> m_analyzeQueue, m_reanalyzeQueue; // m_analyzeQueue is ok if the object didn't enter g_analyzedObjects

	enum NormalizeWndMessages    {READ_PROJDATA = 0xF001};
	enum ExportFormatWndMessages {UPDATE_FORMAT_AND_PREVIEW = 0xF001};
};

/******************************************************************************
* Loudness init/exit                                                          *
******************************************************************************/
int LoudnessInitExit (bool init);
void LoudnessUpdate (bool updatePreferencesDlg = true);

/******************************************************************************
* Commands                                                                    *
******************************************************************************/
void NormalizeLoudness (COMMAND_T*);
void AnalyzeLoudness (COMMAND_T*);
void ToggleLoudnessPref (COMMAND_T*);
void ToggleHighPrecisionOption(COMMAND_T*);
void ToggleDualMonoOption(COMMAND_T*);

// #880
bool NFDoAnalyzeTakeLoudness_IntegratedOnly(MediaItem_Take*, double* lufsIntegrated);
bool NFDoAnalyzeTakeLoudness(MediaItem_Take*, bool analyzeTruePeak, double* lufsIntegrated, double* range, double* truePeak, double* truePeakPos, double* shortTermMax, double* momentaryMax);
bool NFDoAnalyzeTakeLoudness2(MediaItem_Take*, bool analyzeTruePeak, double* lufsIntegrated, double* range, double* truePeak, double* truePeakPos, double* shortTermMax, double* momentaryMax, double* shortTermMaxPos, double* momentaryMaxPos); // adds shortTermMaxPos and momentaryMaxPos


/******************************************************************************
* Toggle states                                                               *
******************************************************************************/
int IsNormalizeLoudnessVisible (COMMAND_T*);
int IsAnalyzeLoudnessVisible (COMMAND_T*);
int IsLoudnessPrefVisible (COMMAND_T*);
int IsHighPrecisionOptionEnabled(COMMAND_T*);
int IsDualMonoOptionEnabled(COMMAND_T*);
