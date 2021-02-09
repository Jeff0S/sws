/******************************************************************************
/ SnM.h
/
/ Copyright (c) 2009 and later Jeffos
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

#ifndef _SNM_H_
#define _SNM_H_


///////////////////////////////////////////////////////////////////////////////
// Activate/deactivate some features, traces, etc.
///////////////////////////////////////////////////////////////////////////////

//#define _SNM_DEBUG
//#define _SNM_SCREENSET_DEBUG
//#define _SNM_DYN_FONT_DEBUG
//#define _SNM_RGNPL_DEBUG1
//#define _SNM_RGNPL_DEBUG2
//#define _SNM_MISC           // not released, deprecated, tests, etc..
//#define _SNM_WDL            // if my WDL version is used
#define _SNM_HOST_AW          // host Adam's stuff
//#define _SNM_OVERLAYS       // look bad with some themes ATM
//#define _SNM_LAZY_SLOTS     // WIP


///////////////////////////////////////////////////////////////////////////////
// Just to ease sustaining vs S&M/API/REAPER/WDL issues: 
// #undef to activate the "expected code", #define to activate workarounds
///////////////////////////////////////////////////////////////////////////////

#ifdef __APPLE__
#define _SNM_SWELL_ISSUES // workaround some SWELL issues
                          // last test: WDL d1d8d2 - Dec. 20 2012
                          // - native font rendering won't draw multiple lines
                          // - missing EN_CHANGE msg, see SnM_Notes.cpp
                          // - EN_SETFOCUS, EN_KILLFOCUS not supported yet
#endif

#define _SNM_NO_ASYNC_UPDT		// disable async UI updates (seems ok on Win, unstable on OSX)


///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////

#define SNM_INI_FILE_VERSION		9

#ifdef _WIN32

#define SNM_FORMATED_INI_FILE      "%s\\S&M.ini"
#define SNM_OLD_FORMATED_INI_FILE  "%s\\Plugins\\S&M.ini"
#define SNM_ACTION_HELP_INI_FILE   "%s\\S&M_Action_help_en.ini"
#define SNM_CYCLACTION_INI_FILE    "%s\\S&M_Cyclactions.ini"
#define SNM_CYCLACTION_BAK_FILE    "%s\\S&M_Cyclactions.bak"
#define SNM_CYCLACTION_EXPORT_FILE "%s\\S&M_Cyclactions_export.ini"
#define SNM_KB_INI_FILE            "%s\\reaper-kb.ini"
#define SNM_CONSOLE_FILE           "%s\\reaconsole_customcommands.txt"
#define SNM_REAPER_EXE_FILE        "%s\\reaper.exe"
#define SNM_FONT_NAME              "MS Shell Dlg"
#define SNM_FONT_HEIGHT            14
#define SNM_DYN_FONT_NAME          "Arial" // good default for UTF8
#define SNM_1PIXEL_Y               1
#define SNM_GUI_TOP_H              36
#define SNM_GUI_BOT_H              41
#define SNM_COL_RED_MONITOR        0x0000BE

#else

#define SNM_FORMATED_INI_FILE      "%s/S&M.ini"
#define SNM_OLD_FORMATED_INI_FILE  "%s/Plugins/S&M.ini"
#define SNM_ACTION_HELP_INI_FILE   "%s/S&M_Action_help_en.ini"
#define SNM_CYCLACTION_INI_FILE    "%s/S&M_Cyclactions.ini"
#define SNM_CYCLACTION_BAK_FILE    "%s/S&M_Cyclactions.bak"
#define SNM_CYCLACTION_EXPORT_FILE "%s/S&M_Cyclactions_export.ini"
#define SNM_KB_INI_FILE            "%s/reaper-kb.ini"
#define SNM_CONSOLE_FILE           "%s/reaconsole_customcommands.txt"
#ifdef __LP64__
#define SNM_REAPER_EXE_FILE        "%s/REAPER64.app"
#else
#define SNM_REAPER_EXE_FILE        "%s/REAPER.app"
#endif
#define SNM_FONT_NAME              "Lucia grande"
#define SNM_FONT_HEIGHT            10
#define SNM_DYN_FONT_NAME          "Arial" // good default for UTF8
#define SNM_1PIXEL_Y               (-1)
#define SNM_GUI_TOP_H              37
#define SNM_GUI_BOT_H              43
#define SNM_COL_RED_MONITOR        0xBE0000

#endif

#define SNM_LIVECFG_NB_CONFIGS     8    //JFB do not change this value, contact me plz thx!
#define SNM_LIVECFG_NB_ROWS        128  //JFB do not change this value, contact me plz thx!

#define SNM_PRESETS_NB_FX          8
#define SNM_CSURF_RUN_TICK_MS      27.0 // monitored average, 1 tick ~= 27ms
#define SNM_MKR_RGN_UPDATE_FREQ    500  // gentle value (ms) not to stress REAPER
#define SNM_OFFSCREEN_UPDATE_FREQ  1000	// gentle value (ms) not to stress REAPER
#define SNM_DEF_TOOLBAR_RFRSH_FREQ 300  // default frequency in ms for the "auto-refresh toolbars" option 
#define SNM_FUDGE_FACTOR           0.0000000001
#define SNM_CSURF_EXT_UNREGISTER   0x00016666
#define SNM_REAPER_IMG_EXTS        "png,pcx,jpg,jpeg,jfif,ico,bmp" // img exts supported by REAPER (v4.32), can't get those at runtime yet
#define SNM_INI_EXT_LIST           "INI files (*.INI)\0*.INI\0All Files\0*.*\0"
#define SNM_SUB_EXT_LIST           "SubRip subtitle files (*.SRT)\0*.SRT\0"
#define SNM_TXT_EXT_LIST           "Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0"

#define SNM_MARKER_MASK            1
#define SNM_REGION_MASK            2

#define SNM_GUI_X_MARGIN           6
#define SNM_GUI_X_MARGIN_OLD       8
#define SNM_GUI_X_MARGIN_LOGO      SNM_GUI_X_MARGIN
#define SNM_GUI_Y_MARGIN           SNM_GUI_X_MARGIN
#define SNM_GUI_Y_MARGIN_LOGO      10
#define SNM_GUI_W_H_KNOB           17
#define SNM_3D_COLORS_DELTA        25

#define SNM_MAX_PATH               2048
#define SNM_MAX_HW_OUTS            8
#define SNM_MAX_CC_LANE_ID         167 // v5pre11
#define SNM_MAX_CC_LANES_LEN       4096
#define SNM_MAX_TRACK_GROUPS       64
#define SNM_MAX_CUE_BUSS_CONFS     8
//#define SNM_MAX_TAKES              1024
#define SNM_MAX_FX                 128
#define SNM_MAX_PRESETS            0xFFFF
#define SNM_MAX_INI_SECTION        0xFFFF // definitive limit for WritePrivateProfileSection

#define SNM_MAX_ACTION_CUSTID_LEN  128
#define SNM_MAX_MACRO_CUSTID_LEN   32
#define SNM_MAX_ACTION_NAME_LEN    512 // can be localized
#define SNM_MAX_DYN_ACTIONS        0xFF     // if > 255, DYN_COMMAND_T must be updated
#define SNM_MAX_ENV_CHUNKNAME_LEN  32
#define SNM_MAX_MARKER_NAME_LEN    64     // + regions
#define SNM_MAX_TRACK_NAME_LEN     128
#define SNM_MAX_PRESET_NAME_LEN    SNM_MAX_PATH // vst3 presets can be full filenames (.vstpreset files)
#define SNM_MAX_FX_NAME_LEN        128
#define SNM_MAX_OSC_MSG_LEN        256

#define SNM_LOGO_PNG_FILE "iVBORw0KGgoAAAANSUhEUgAAADIAAAAUCAMAAAGPE64+AAAAFXRFWA==\n\
dENyZWF0aW9uIFRpbWUAB9oHEQEwJvx8Q0oAAAAHdElNRQfaBxAXJA==\n\
C+Ibk70AAAAJcEhZcwAACvAAAArwAUKsNJgAAAMAUExURQAAAAgICA==\n\
EBAQGBgYISEhKSkpMTExOTk5QkJCSkpKUlJSWlpaY2Nja2trc3Nzew==\n\
e3uEhISMjIyUlJScnJylpaWtra21tbW9vb3GxsbOzs7W1tbe3t7n5w==\n\
5+/v7/f39////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
/////////////////////////////////////////////////////w==\n\
//9msyl1AAAAAnRSTlP/AOW3MEoAAAGoSURBVHjanVLbcqswDJQwlA==\n\
W0KggYCBeP//L7symLYv58zU4xhHK8mrlUSPJQp9AvzuvXc1v4c5Fw==\n\
OEAm3Yve0UgX21tvriLACA2QZwpI2VpUFSwphGfHoLyeGguircWO3g==\n\
HpPQKkrvc/FnApF4XGf8cWNzN4bOllQQAn+IiE5gphF4ZygXsIYWaw==\n\
RAJdlgKBpkww3PBwaPU3Uf3N+p9IYpVoSuL2NgZ2PpSlwVR5RmRFVQ==\n\
1NpBsKh6IvdwMgDWTPVmRVDhFTk6uIhkTPZBHPmbcXdW00NOBg0Gag==\n\
TcxV1oa7PxAHvm2t2LHpbDfuyLre8KLLhufkKbP2rfL8mwb/CxHXvQ==\n\
1nUeijg2g9+W+4G4kauX89LJFVLjWN4wk4+dWiP8aeadV5PWZD1Dcg==\n\
/puT7HTa3EIZDDLZFms0K+a1/O7aK77xbizkFvO2wuEbZWCbODfFDA==\n\
z64F+VlLOQZ6Dua4dpaajpNwpPDxaQ9twrF7XSHyaDJROqMpYYyy1Q==\n\
3iwJ0Fssw64SzlIi+8dZ/OISxbhGsZobtpoDrVUq5ZrAYwjPaYw7LQ==\n\
vezn/Q+t/AIQiCv/Q4iRxAAAAABJRU5ErkJggg==\n"


///////////////////////////////////////////////////////////////////////////////
// Misc global/common classes, vars, etc.
///////////////////////////////////////////////////////////////////////////////

extern int g_SNM_Beta, g_SNM_LearnPitchAndNormOSC, g_SNM_MediaFlags, g_SNM_ToolbarRefreshFreq;
extern WDL_FastString g_SNM_IniFn, g_SNM_CyclIniFn, g_SNM_DiffToolFn;
extern bool g_SNM_ToolbarRefresh;


class SNM_TrackInt {
public:
	SNM_TrackInt(MediaTrack* _tr, int _i) : m_tr(_tr), m_int(_i) {}
	~SNM_TrackInt() {}
	MediaTrack* m_tr;
	int m_int;
};

// unsigned chars are enough ATM..
typedef struct DYN_COMMAND_T {
	const char* desc;
	const char* id;
	void (*doCommand)(COMMAND_T*);
	unsigned char count;
	unsigned char max;
	int (*getEnabled)(COMMAND_T*);
	int uniqueSectionId;
	void(*onAction)(COMMAND_T*, int, int, int, HWND);

	bool Register(const int slot) const;
} DYN_COMMAND_T;

typedef struct SECTION_INFO_T {
	int unique_id;
	const char* ca_cust_id;
	const char* ca_ini_sec;
} SECTION_INFO_T;


///////////////////////////////////////////////////////////////////////////////
// Scheduled jobs
///////////////////////////////////////////////////////////////////////////////

// job ids
enum {
  SNM_SCHEDJOB_LIVECFG_APPLY = 0,
  SNM_SCHEDJOB_LIVECFG_PRELOAD = SNM_SCHEDJOB_LIVECFG_APPLY + SNM_LIVECFG_NB_CONFIGS,
  SNM_SCHEDJOB_LIVECFG_UPDATE = SNM_SCHEDJOB_LIVECFG_PRELOAD + SNM_LIVECFG_NB_CONFIGS,
  SNM_SCHEDJOB_UNDO,
  SNM_SCHEDJOB_NOTES_UPDATE,
  SNM_SCHEDJOB_SEL_PRJ,
  SNM_SCHEDJOB_TRIG_PRESET,
  SNM_SCHEDJOB_RES_ATTACH = SNM_SCHEDJOB_TRIG_PRESET + SNM_PRESETS_NB_FX + 1, // +1 for the "selected fx" preset action
  SNM_SCHEDJOB_PLAYLIST_UPDATE,
  SNM_SCHEDJOB_OSX_FIX	//JFB!! removeme some day, due to _SNM_SWELL_ISSUES/missing EN_CHANGE messages
};

#define SNM_SCHEDJOB_DEFAULT_DELAY    250
#define SNM_SCHEDJOB_SLOW_DELAY       500
#ifdef _SNM_NO_ASYNC_UPDT
#define SNM_SCHEDJOB_ASYNC_DELAY_OPT  0
#else
#define SNM_SCHEDJOB_ASYNC_DELAY_OPT  SNM_SCHEDJOB_SLOW_DELAY
#endif


// scheduled jobs are added in a queue and wait for _approxMs before 
// being performed. if a job with the same _id is already present in 
// the queue, it is replaced and re-waits for _approxMs (by default).
// to use this, you just need to implement Perform() in most cases,
// if you need to process all intermediate values before jobs are performed, 
// just override Init() - which is called once when the job is actually 
// added to the processing queue.
class ScheduledJob
{
public:
	// _approxMs==0 means "to be performed immediately" (not added to the processing queue)
	ScheduledJob(int _id, int _approxMs)
		: m_id(_id),m_approxMs(_approxMs),m_scheduled(false),m_time(GetTickCount()+_approxMs) {}
	virtual ~ScheduledJob() {}

	static void Schedule(ScheduledJob* _job);
	static void Run(); // polled from the main thread via SNM_CSurfRun()

	// not safe to make anything public: 1-jobs are auto-deleted, 2-Init() may not have been called

protected:
	virtual void Perform() {}
	virtual void Init(ScheduledJob* _replacedJob = NULL) {}
	bool IsImmediate() { return m_approxMs==0; }
	int m_id, m_approxMs; // really approx since Run() is called on timer

private:
	void InitSafe(ScheduledJob* _replacedJob = NULL) { if (!m_scheduled) Init(_replacedJob); m_scheduled=true; }
	void PerformSafe() { InitSafe(); Perform(); }
	bool m_scheduled;
	DWORD m_time;
};


class MidiOscActionJob : public ScheduledJob
{
public:
	MidiOscActionJob(int _jobId, int _approxMs, int _val, int _valhw, int _relmode) 
		: ScheduledJob(_jobId, _approxMs),m_val(_val),m_valhw(_valhw),m_relmode(_relmode),m_absval(0.0)
	{
		// can't call pure virtual funcs in constructor, this is where C++ sucks
		// => Init() will do the job later on..
	}
protected:
	virtual void Init(ScheduledJob* _replacedJob = NULL);
	double GetValue() { return m_absval; }
	int GetIntValue() { return int(0.5+GetValue()); }

	// pure virtual callbacks, see MidiOscActionJob::Init()
	virtual double GetCurrentValue() = 0;
	virtual double GetMinValue() = 0;
	virtual double GetMaxValue() = 0;
	int m_val, m_valhw, m_relmode; // values from the controller
private:
	int AdjustRelative(int _adjmode, int _reladj);
	double m_absval; // internal absolute value
};


// avoid undo points flooding (i.e. async undo => handle with care!)
class UndoJob : public ScheduledJob
{
public:
	UndoJob(const char* _desc, int _flags, int _tr = -1) 
		: ScheduledJob(SNM_SCHEDJOB_UNDO, SNM_SCHEDJOB_DEFAULT_DELAY), 
			m_desc(_desc), m_flags(_flags), m_tr(_tr) {}
protected:
	void Perform() { Undo_OnStateChangeEx2(NULL, m_desc, m_flags, m_tr); }
private:
	const char* m_desc;
	int m_flags, m_tr;
};


///////////////////////////////////////////////////////////////////////////////
// Action sections
///////////////////////////////////////////////////////////////////////////////

// section indexes
enum {
  SNM_SEC_IDX_MAIN=0,
  SNM_SEC_IDX_MAIN_ALT,
  SNM_SEC_IDX_EPXLORER,
  SNM_SEC_IDX_ME,
  SNM_SEC_IDX_ME_EL,
  SNM_SEC_IDX_ME_INLINE,
  SNM_NUM_MANAGED_SECTIONS
};

// various properties indexed with the above enum: keep both in sync!
SECTION_INFO_T *SNM_GetActionSectionInfo(int _idx);

#define SNM_NUM_NATIVE_SECTIONS   SNM_NUM_MANAGED_SECTIONS
#define SNM_MAX_CA_SECTIONS       SNM_NUM_NATIVE_SECTIONS
#define SNM_MAX_SECTION_NAME_LEN  512 // can be localized


///////////////////////////////////////////////////////////////////////////////
// Common funcs
///////////////////////////////////////////////////////////////////////////////

int SNM_Init(reaper_plugin_info_t* _rec);
void SNM_Exit();

bool SNM_GetActionName(const char* _custId, WDL_FastString* _nameOut, int _slot = -1);
int GetFakeToggleState(COMMAND_T*);

DYN_COMMAND_T *FindDynamicAction(void (*doCommand)(COMMAND_T*));

static void freecharptr(char* _p) { FREE_NULL(_p); }
static void deleteintptr(int* _p) { DELETE_NULL(_p); }
static void deletefaststrptr(WDL_FastString* _p) { DELETE_NULL(_p); }

/* commented: replaced with a global SWS_CMD_SHORTNAME()
 #define SNM_CMD_SHORTNAME(_ct) (GetLocalizedActionName(_ct->id, _ct->accel.desc) + 9) // +9 to skip "SWS/S&M: "
 */


#endif
