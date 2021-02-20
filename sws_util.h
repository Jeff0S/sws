/******************************************************************************
/ sws_util.h
/
/ Copyright (c) 2012 and later Tim Payne (SWS), Jeffos
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

#if defined(_SWS_DEBUG)
  #define WDL_PtrList_DOD WDL_PtrList_DeleteOnDestroy
#else
  #define WDL_PtrList_DOD WDL_PtrList
#endif

#define BUFFER_SIZE				2048
#define SWS_THEMING				true
#define SWS_INI					"SWS"
#define SWS_SEPARATOR			"SEPARATOR"
#define LAST_COMMAND			((char*)(INT_PTR)-1)
#define SWS_STARTSUBMENU		((char*)(INT_PTR)-2)
#define SWS_ENDSUBMENU			((char*)(INT_PTR)-3)
#define DEFACCEL				{ 0, 0, 0 }

#define UTF8_BULLET				"\xE2\x80\xA2"
#define UTF8_CIRCLE				"\xE2\x97\xA6"
#define UTF8_BOX				"\xE2\x96\xA1"
#define UTF8_BBOX				"\xE2\x96\xA0"
#define UTF8_INFINITY			"\xE2\x88\x9E"
#define UTF8_CHECKMARK          "\xE2\x9C\x93"
#define UTF8_MULTIPLICATION     "\xE2\x9C\x95"

#define SWS_CMD_SHORTNAME(_ct)	(_ct ? GetLocalizedActionName(_ct->accel.desc) + IsSwsAction(_ct->accel.desc) : "")
#define __ARRAY_SIZE(x)			(sizeof(x) / sizeof(x[0]))
#define BOUNDED(x,lo,hi)		((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
#define FREE_NULL(p)			{free(p);p=NULL;}
#define DELETE_NULL(p)			{delete(p); p=NULL;}
#define STR_HELPER(x)			#x
#define STR(x)					STR_HELPER(x)

// For checking to see if items are adjacent
// Found one case of items after split having diff edges 5e-11 apart, 1e-9 (still much greater than one sample)
#define SWS_ADJACENT_ITEM_THRESHOLD 1.0e-9

#ifdef _WIN32
#define PATH_SLASH_CHAR '\\'
#else
#define PATH_SLASH_CHAR '/'
#endif

// Aliases to keys
#define SWS_ALT		LVKF_ALT		// 1
#define SWS_CTRL	LVKF_CONTROL	// 2
#define SWS_SHIFT	LVKF_SHIFT		// 4

typedef struct MODIFIER
{
	int iModifier;
	const char* cDesc;
} MODIFIER;

#define NUM_MODIFIERS 8
extern MODIFIER g_modifiers[]; // sws_util.h

typedef struct COMMAND_T
{
	gaccel_register_t accel;
	const char* id;
	void (*doCommand)(COMMAND_T*);
	const char* menuText;
	INT_PTR user;
	int (*getEnabled)(COMMAND_T*);
	int uniqueSectionId;
	void(*onAction)(COMMAND_T*, int, int, int, HWND);
	bool fakeToggle;
} COMMAND_T;


template<class PTRTYPE> class SWSProjConfig
{
protected:
	WDL_PtrList<void> m_projects;
	WDL_PtrList<PTRTYPE> m_data;

public:
	SWSProjConfig() {}
	virtual ~SWSProjConfig() { Empty(); }
	PTRTYPE* Get()
	{
		ReaProject* pProj = (ReaProject*)GetCurrentProjectInLoadSave();
		if (!pProj) // If not in a project load/save context, above returns NULL
			pProj = EnumProjects(-1, NULL, 0);
		int i = m_projects.Find(pProj);
		if (i >= 0)
			return m_data.Get(i);
		m_projects.Add(pProj);
		return m_data.Add(new PTRTYPE);
	}
	PTRTYPE* Get(int iProj) { return m_data.Get(iProj); }
	int GetNumProj() { return m_data.GetSize(); }
	void Empty()
	{
		m_projects.Empty();
		m_data.Empty(true);
	}
	void Cleanup()
	{
		if (m_projects.GetSize())
		{
			for (int i = m_projects.GetSize() - 1; i >= 0; i--)
			{
				int j = 0;
				ReaProject* pProj;
				while ((pProj = EnumProjects(j++, NULL, 0)))
					if (m_projects.Get(i) == pProj)
						break;
				if (!pProj)
				{
					m_projects.Delete(i);
					m_data.Delete(i, true);
				}
			}
		}
	}
};

extern REAPER_PLUGIN_HINSTANCE g_hInst;
extern HWND g_hwndParent;
extern double g_d0;
extern int g_i0;
extern int g_i1;
extern int g_i2;
extern bool g_bTrue;
extern bool g_bFalse;
extern int g_iFirstCommand;
extern int g_iLastCommand;
extern MTRand g_MTRand;

// Stuff to do in swell someday
#ifndef _WIN32
#define BS_MULTILINE 0
#define _strdup strdup
#define _strndup strndup
#define _stricmp stricmp
#define _strnicmp strnicmp
#define LVKF_ALT 1
#define LVKF_CONTROL 2
#define LVKF_SHIFT 4
extern const GUID GUID_NULL;

// sws_util.mm
void SWS_GetDateString(int time, char* buf, int bufsize);
void SWS_GetTimeString(int time, char* buf, int bufsize);

#ifdef __APPLE__
int GetCustomColors(COLORREF custColors[]);
void SetCustomColors(COLORREF custColors[]);
void ShowColorChooser(COLORREF initialCol);
bool GetChosenColor(COLORREF* pColor);
void HideColorChooser();
void SetMenuItemSwatch(HMENU hMenu, UINT pos, int size, COLORREF color);
#endif

HCURSOR SWS_LoadCursor(int id);
#define MOUSEEVENTF_LEFTDOWN    0x0002 /* left button down */
#define MOUSEEVENTF_LEFTUP      0x0004 /* left button up */
#define MOUSEEVENTF_RIGHTDOWN   0x0008 /* right button down */
#define MOUSEEVENTF_RIGHTUP     0x0010 /* right button up */
void mouse_event(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData, ULONG_PTR dwExtraInfo);
int GetMenuString(HMENU hMenu, UINT uIDItem, char* lpString, int nMaxCount, UINT uFlag);
#endif

// Command/action handling, sws_extension.cpp
int SWSRegisterCmd(COMMAND_T* pCommand, const char* cFile, int cmdId = 0, bool localize = true);
COMMAND_T* SWSUnregisterCmd(int id);

int SWSRegisterCmds(COMMAND_T* pCommands, const char* cFile, bool localize); // Multiple commands in a table, terminated with LAST_COMMAND
#define SWSRegisterCommands(c) SWSRegisterCmds(c, __FILE__, true)

int SWSCreateRegisterDynamicCmd(int uniqueSectionId, int cmdId, void(*doCommand)(COMMAND_T*), void(*onAction)(COMMAND_T*, int, int, int, HWND), int(*getEnabled)(COMMAND_T*), const char* cID, const char* cDesc, const char* cMenu, INT_PTR user, const char* cFile, bool localize);
#define SWSRegisterCommandExt(a, b, c, d, e) SWSCreateRegisterDynamicCmd(0, 0, a, NULL, NULL, b, c, "", d, __FILE__, e)
bool SWSFreeUnregisterDynamicCmd(int id);

void ActionsList(COMMAND_T*);
int SWSGetCommandID(void (*cmdFunc)(COMMAND_T*), INT_PTR user = 0, const char** pMenuText = NULL);
COMMAND_T* SWSGetCommandByID(int cmdId);
int IsSwsAction(const char* _actionName);

HMENU SWSCreateMenuFromCommandTable(COMMAND_T pCommands[], HMENU hMenu = NULL, int* iIndex = NULL);;

// Utility functions, sws_util.cpp
void SaveWindowPos(HWND hwnd, const char* cKey);
void RestoreWindowPos(HWND hwnd, const char* cKey, bool bRestoreSize = true);
void SetWindowPosAtMouse(HWND hwnd);

MediaTrack* GetFirstSelectedTrack();
int NumSelTracks();
void SaveSelected();
void RestoreSelected();
void ClearSelected();
int GetFolderDepth(MediaTrack* tr, int* iType, MediaTrack** nextTr);
int GetTrackVis(MediaTrack* tr); // &1 == mcp, &2 == tcp
void SetTrackVis(MediaTrack* tr, int vis); // &1 == mcp, &2 == tcp
int AboutBoxInit(); // Not worth its own .h
HWND GetTrackWnd();
HWND GetRulerWnd();
const GUID* TrackToGuid(ReaProject*, MediaTrack*);
inline const GUID* TrackToGuid(MediaTrack* tr) { return TrackToGuid(nullptr, tr); }
MediaTrack* GuidToTrack(ReaProject*, const GUID*);
inline MediaTrack* GuidToTrack(const GUID* guid) { return GuidToTrack(nullptr, guid); }
bool GuidsEqual(const GUID* g1, const GUID* g2);
bool TrackMatchesGuid(ReaProject*, MediaTrack*, const GUID*);
inline bool TrackMatchesGuid(MediaTrack* tr, const GUID* g) { return TrackMatchesGuid(nullptr, tr, g); }
const char *stristr(const char* a, const char* b);

// NF: fix / workaround for setting take start offset doesn't work if containing stretch markers
// see https://forum.cockos.com/showthread.php?t=180571
// probably all functions setting take start offset should use this for now, until it's changed in REAPER
// caller must check for take != NULL
void UpdateStretchMarkersAfterSetTakeStartOffset(MediaItem_Take* take, double takeStartOffset_multiplyPlayrate);

// returns source filename also if source is section/reversed (see PCM_source::GetFilename() comment)
const char* SWS_GetSourceFileName(PCM_source* src);

#ifdef _WIN32
  void dprintf(const char* format, ...);
#else
  #define dprintf printf
  #ifndef OutputDebugString
    #define OutputDebugString(x) printf("%s", x)
  #endif
#endif

void SWS_GetAllTracks(WDL_TypedBuf<MediaTrack*>* buf, bool bMaster = false);
void SWS_GetSelectedTracks(WDL_TypedBuf<MediaTrack*>* buf, bool bMaster = false);
void SWS_GetSelectedMediaItems(WDL_TypedBuf<MediaItem*>* buf);
void SWS_GetSelectedMediaItemsOnTrack(WDL_TypedBuf<MediaItem*>* buf, MediaTrack* tr);
int SWS_GetModifiers();
bool SWS_IsWindow(HWND hwnd);

// Localization, sws_util.cpp
#define _SWS_LOCALIZATION
WDL_FastString* GetLangPack();
bool IsLocalized();
const char* GetLocalizedActionName(const char* _defaultStr, int _flags = 0, const char* _section = "sws_actions");
bool IsLocalizableAction(const char* _customId);
TrackEnvelope* SWS_GetTakeEnvelopeByName(MediaItem_Take* take, const char* envname);
TrackEnvelope* SWS_GetTrackEnvelopeByName(MediaTrack* track, const char* envname);

// Functions export to reascript and c++ plugins, Reascript.cpp
bool RegisterExportedFuncs(reaper_plugin_info_t* _rec);
void UnregisterExportedFuncs();
bool RegisterExportedAPI(reaper_plugin_info_t* _rec);

