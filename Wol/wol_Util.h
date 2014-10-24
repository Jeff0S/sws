/******************************************************************************
/ wol_Util.h
/
/ Copyright (c) 2014 wol
/ http://forum.cockos.com/member.php?u=70153
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

#include "../SnM/SnM_VWnd.h"



void wol_UtilInit();

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Track
///////////////////////////////////////////////////////////////////////////////////////////////////
bool SaveSelectedTracks();
bool RestoreSelectedTracks();

/* refreshes UI too */
void SetTrackHeight(MediaTrack* track, int height);

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Tcp
///////////////////////////////////////////////////////////////////////////////////////////////////
int GetTcpEnvMinHeight();
int GetTcpTrackMinHeight();
int GetCurrentTcpMaxHeight();
void ScrollToTrackEnvelopeIfNotInArrange(TrackEnvelope* envelope);
void ScrollToTrackIfNotInArrange(TrackEnvelope* envelope);

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Envelope
///////////////////////////////////////////////////////////////////////////////////////////////////
struct EnvelopeHeight
{
	TrackEnvelope* env;
	int h;
};
int CountVisibleTrackEnvelopesInTrackLane(MediaTrack* track);

/*  Return   -1 for envelope not in track lane or not visible, *laneCount and *envCount not modified
			 0 for overlap disabled, *laneCount = *envCount = number of lanes (same as visible envelopes)
			 1 for overlap enabled, envelope height < overlap limit -> single lane (one envelope visible), *laneCount = *envCount = 1
			 2 for overlap enabled, envelope height < overlap limit -> single lane (overlapping), *laneCount = 1, *envCount = number of visible envelopes
			 3 for overlap enabled, envelope height > overlap limit -> single lane (one envelope visible), *laneCount = *envCount = 1
			 4 for overlap enabled, envelope height > overlap limit -> multiple lanes, *laneCount = *envCount = number of lanes (same as visible envelopes) */
int GetEnvelopeOverlapState(TrackEnvelope* envelope, int* laneCount = NULL, int* envCount = NULL);

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Midi
///////////////////////////////////////////////////////////////////////////////////////////////////
int AdjustRelative(int _adjmode, int _reladj);

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Ini
///////////////////////////////////////////////////////////////////////////////////////////////////
const char* GetWolIni();
void SaveIniSettings(const char* section, const char* key, bool val, const char* path = GetWolIni());
void SaveIniSettings(const char* section, const char* key, int val, const char* path = GetWolIni());
void SaveIniSettings(const char* section, const char* key, const char* val, const char* path = GetWolIni());
bool GetIniSettings(const char* section, const char* key, bool defVal, const char* path = GetWolIni());
int GetIniSettings(const char* section, const char* key, int defVal, const char* path = GetWolIni());
int GetIniSettings(const char* section, const char* key, const char* defVal, char* outBuf, int maxOutBufSize, const char* path = GetWolIni());
void DeleteIniSection(const char* section, const char* path = GetWolIni());
void DeleteIniKey(const char* section, const char* key, const char* path = GetWolIni());
#ifdef _WIN32
void FlushIni(const char* path = GetWolIni());
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Messages
///////////////////////////////////////////////////////////////////////////////////////////////////
/* msg is localized in "sws_mbox" if localizeMsg is true, title is localized in "sws_mbox" if localizeTitle is true.
   uIcon not used in OSX.*/
int ShowMessageBox2(const char* msg, const char* title, UINT uType = MB_OK, UINT uIcon = 0, bool localizeMsg = true, bool localizeTitle = true, HWND hwnd = GetMainHwnd());
int ShowErrorMessageBox(const char* msg, const char* title = "SWS/wol - Error", bool localizeMsg = true, bool localizeTitle = true, UINT uType = MB_OK, HWND hwnd = GetMainHwnd());
int ShowWarningMessageBox(const char* msg, const char* title = "SWS/wol - Warning", bool localizeMsg = true, bool localizeTitle = true, UINT uType = MB_OK, HWND hwnd = GetMainHwnd());

///////////////////////////////////////////////////////////////////////////////////////////////////
/// User input
///////////////////////////////////////////////////////////////////////////////////////////////////
#define CMD_SETUNDOSTR 20
class UserInputAndSlotsEditorWnd : public SWS_DockWnd
{
public:
	UserInputAndSlotsEditorWnd(const char* wndtitle, const char* title, const char* id, int cmdId);

	void Update();
	void UseTwoKnobs(bool two = true) { m_twoknobs = two; }
	void SetupKnob1(int min, int max, int center, int pos, double factor, const char* title, const char* suffix, const char* zerotext); //must be called in constructor
	void SetupKnob2(int min, int max, int center, int pos, double factor, const char* title, const char* suffix, const char* zerotext); //must be called in constructor (two knobs only)
	void SetupOnCommandCallback(void(*OnCommandCallback)(int cmd, int* min, int* max)); //must be called in constructor
	void SetupOKText(const char* text);
	void SetupQuestion(const char* questiontxt, const char* questiontitle, UINT type);

	enum
	{
		CMD_LOAD = 0,
		CMD_SAVE = 8,
		CMD_CLOSE = 30,
		CMD_USERANSWER
	};

protected:
	virtual void OnInitDlg();
	virtual void OnDestroy();
	virtual void OnCommand(WPARAM wParam, LPARAM lParam);
	virtual void DrawControls(LICE_IBitmap* bm, const RECT* r, int* tooltipHeight = NULL);
	virtual INT_PTR OnUnhandledMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);

	void(*m_OnCommandCallback)(int cmd, int* min, int* max); //for single knob dialogs max is not used

	SNM_Knob m_kn1, m_kn2;
	SNM_KnobCaption m_kn1Text, m_kn2Text;

	HWND m_btnL, m_btnR;

	bool m_twoknobs, m_kn1rdy, m_kn2rdy, m_cbrdy, m_askquestion;
	UINT m_questiontype;
	string m_wndtitlebar, m_oktxt, m_questiontxt, m_questiontitle;
};