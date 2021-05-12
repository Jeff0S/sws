/******************************************************************************
/ SnM_Notes.h
/
/ Copyright (c) 2010 and later Jeffos
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

//#pragma once

#ifndef _SNM_NOTES_H_
#define _SNM_NOTES_H_

#include "SnM_Marker.h"
#include "SnM_VWnd.h"

#define NOTES_UPDATE_FREQ		150

// note types (g_notesType)
enum {
  SNM_NOTES_TRACK=0,
  SNM_NOTES_ITEM,
  SNM_NOTES_PROJECT,
  SNM_NOTES_PROJECT_EXTRA,
  SNM_NOTES_GLOBAL,
  SNM_NOTES_MKR_NAME,      // these should remain consecutive ---->
  SNM_NOTES_RGN_NAME,
  SNM_NOTES_MKRRGN_NAME,
  SNM_NOTES_MKR_SUB,
  SNM_NOTES_RGN_SUB,
  SNM_NOTES_MKRRGN_SUB,    // <-----
#ifdef WANT_ACTION_HELP
  SNM_NOTES_ACTION_HELP    // must remain the last item: no OSX support yet 
#endif
};

class SNM_TrackNotes {
public:
	SNM_TrackNotes(ReaProject* project, const GUID* guid, const char* notes)
		: m_project{project}, m_guid{*guid}, m_notes{notes}
	{
		// The current project (project == NULL) doen't always match
		// the notes' project when saving in SaveExtensionConfig [#1141]

		if (!project)
			m_project = EnumProjects(-1, nullptr, 0);
	}

	MediaTrack* GetTrack() { return GuidToTrack(m_project, &m_guid); }
	const GUID* GetGUID() { return &m_guid; }
	const char *GetNotes() const { return m_notes.Get(); }
	int GetNotesLength() const { return m_notes.GetLength(); }
	void SetNotes(const char *notes) { m_notes.Set(notes); }

private:
	ReaProject *m_project;
	GUID m_guid;
	WDL_FastString m_notes;
};

class SNM_RegionSubtitle {
public:
	SNM_RegionSubtitle(ReaProject* project, const int id, const char* notes)
		: m_project{project}, m_id{id}, m_notes{notes}
	{
		if (!project) // see SNM_TrackNotes's constructor
			m_project = EnumProjects(-1, nullptr, 0);
	}

	int GetId() const { return m_id; }
	bool IsValid() const { return GetMarkerRegionIndexFromId(m_project, m_id) >= 0; }
	const char *GetNotes() const { return m_notes.Get(); }
	int GetNotesLength() const { return m_notes.GetLength(); }
	void SetNotes(const char *notes) { m_notes.Set(notes); }

private:
	ReaProject *m_project;
	int m_id;
	WDL_FastString m_notes;
};

class NotesUpdateJob : public ScheduledJob {
public:
	NotesUpdateJob(int _approxMs) : ScheduledJob(SNM_SCHEDJOB_NOTES_UPDATE, _approxMs) {}
protected:
	void Perform();
};

#ifdef _SNM_SWELL_ISSUES
class OSXForceTxtChangeJob : public ScheduledJob {
public:
	OSXForceTxtChangeJob() : ScheduledJob(SNM_SCHEDJOB_OSX_FIX, 50) {} // ~fast enough to follow key strokes 
protected:
	void Perform();
};
#endif

class NotesMarkerRegionListener : public SNM_MarkerRegionListener {
public:
	NotesMarkerRegionListener() : SNM_MarkerRegionListener() {}
	void NotifyMarkerRegionUpdate(int _updateFlags);
};

class NotesWnd : public SWS_DockWnd
{
public:
	NotesWnd();
	~NotesWnd();

	void SetType(int _type);
	void SetText(const char* _str, bool _addRN = true);
	void RefreshGUI();
	void SetFontsizeFrominiFile(); // looks for Fontsize key in S&M.ini > [Notes] section
	void SetWrapText(bool wrap);
	void OnCommand(WPARAM wParam, LPARAM lParam);
	void GetMinSize(int* _w, int* _h) { *_w=MIN_DOCKWND_WIDTH; *_h=140; }
	void ToggleLock();

	void SaveCurrentText(int _type, bool _wantUndo = true);
	void SaveCurrentProjectNotes(bool _wantUndo = true);
	void SaveCurrentExtraProjectNotes(bool _wantUndo = true);
	void SaveCurrentItemNotes(bool _wantUndo = true);
	void SaveCurrentTrackNotes(bool _wantUndo = true);
	void SaveCurrentGlobalNotes(bool _wantUndo = true);
	void SaveCurrentMkrRgnNameOrSub(int _type, bool _wantUndo = true);
#ifdef WANT_ACTION_HELP
	void SaveCurrentHelp(); // no undo for action help (saved in a .ini)
#endif
	void Update(bool _force = false);
#ifdef WANT_ACTION_HELP
	int UpdateActionHelp();
#endif
	int UpdateItemNotes();
	int UpdateTrackNotes();
	int UpdateMkrRgnNameOrSub(int _type);
	void ForceUpdateMkrRgnNameOrSub(int _type); // NF: trigger update after setting sub from ReaScript
	HWND GetEditControl() const { return m_edit; }

protected:
	void OnInitDlg();
	void OnDestroy();
	bool IsActive(bool bWantEdit = false);
	HMENU OnContextMenu(int x, int y, bool* wantDefaultItems);
	int OnKey(MSG* msg, int iKeyState);
	void OnTimer(WPARAM wParam=0);
	void OnResize();
	void DrawControls(LICE_IBitmap* _bm, const RECT* _r, int* _tooltipHeight = NULL);
	bool GetToolTipString(int _xpos, int _ypos, char* _bufOut, int _bufOutSz);

private:
	SNM_VirtualComboBox m_cbType;
	WDL_VirtualIconButton m_btnLock;
#ifdef WANT_ACTION_HELP
	SNM_ToolbarButton m_btnAlr, m_btnActionList;
#endif
	SNM_ToolbarButton m_btnImportSub, m_btnExportSub;
	WDL_VirtualStaticText m_txtLabel;
	SNM_DynSizedText m_bigNotes;

	NotesMarkerRegionListener m_mkrRgnListener;
	HWND m_edit;
};

#ifdef WANT_ACTION_HELP
void LoadHelp(const char* _cmdName, char* _buf, int _bufSize);
void SaveHelp(const char* _cmdName, const char* _help);
void SetActionHelpFilename(COMMAND_T*);
#endif
bool GetStringFromNotesChunk(WDL_FastString* _notes, char* _buf, int _bufMaxSize);
bool GetNotesChunkFromString(const char* _buf, WDL_FastString* _notes, const char* _startLine = NULL);


extern SWSProjConfig<WDL_PtrList_DOD<SNM_TrackNotes> > g_SNM_TrackNotes;


void NotesSetTrackTitle();
void NotesSetTrackListChange();

int NotesInit();
void NotesExit();
void ImportSubTitleFile(COMMAND_T*);
void ExportSubTitleFile(COMMAND_T*);
void OpenNotes(COMMAND_T*);
int IsNotesDisplayed(COMMAND_T*);
void ToggleNotesLock(COMMAND_T*);
int IsNotesLocked(COMMAND_T*);
void WriteGlobalNotesToFile();

// ReaScript export
const char* NFDoGetSWSTrackNotes(MediaTrack* track);
void NFDoSetSWSTrackNotes(MediaTrack* track, const char* buf);

const char* NFDoGetSWSMarkerRegionSub(int mkrRgnIdx);
bool NFDoSetSWSMarkerRegionSub(const char* mkrRgnSubIn, int mkrRgnIdx);
void NF_DoUpdateSWSMarkerRegionSubWindow();

#endif
