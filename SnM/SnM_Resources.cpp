/******************************************************************************
/ SnM_Resources.cpp
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

//JFB
// TODO: turn "Attach bookmark files to this project" into "In-project bookmark"
//       requires some API updates first though : http://askjf.com/index.php?q=2465s
//       => done in REAPER v4.58: file_in_project_ex2!

#include "stdafx.h"

#include "SnM.h"
#include "SnM_Chunk.h"
#include "SnM_Dlg.h"
#include "SnM_FXChain.h"
#include "SnM_Item.h"
#include "SnM_Project.h"
#include "SnM_Resources.h"
#include "SnM_Track.h"
#include "SnM_Util.h"
#include "../Prompt.h"
#ifdef _WIN32
#include "../DragDrop.h"
#endif

#include <WDL/localize/localize.h>
#include <WDL/projectcontext.h>

#define RES_WND_ID					"SnMResources"
#define IMG_WND_ID					"SnMImage"
#define RES_INI_SEC					"Resources"

#define RES_TIE_TAG					" [x]" //UTF8_BULLET


enum {
  // common cmds
  AUTOFILL_MSG = 0xF000,
  AUTOFILL_DIR_MSG,
  AUTOFILL_PRJ_MSG,
  AUTOFILL_DEFAULT_MSG,
  AUTOFILL_SYNC_MSG,
  CLR_SLOTS_MSG,
  DEL_SLOTS_MSG,
  DEL_FILES_MSG,
  ADD_SLOT_MSG,
  INSERT_SLOT_MSG,
  EDIT_MSG,
  EXPLORE_MSG,
  EXPLORE_SAVEDIR_MSG,
  EXPLORE_FILLDIR_MSG,
  LOAD_MSG,
  AUTOSAVE_MSG,
  AUTOSAVE_DIR_MSG,
  AUTOSAVE_DIR_PRJ_MSG,
  AUTOSAVE_DIR_PRJ2_MSG,
  AUTOSAVE_DIR_DEFAULT_MSG,
  AUTOSAVE_SYNC_MSG,
  FILTER_BY_NAME_MSG,
  FILTER_BY_PATH_MSG,
  FILTER_BY_COMMENT_MSG,
  RENAME_MSG,
  TIE_ACTIONS_MSG,
  TIE_PROJECT_MSG,
  UNTIE_PROJECT_MSG,
#ifdef _WIN32
  DIFF_MSG,
#endif
  // fx chains cmds
  FXC_START_MSG,                         // 8 cmds --->
  FXC_END_MSG = FXC_START_MSG+7,         // <---
  FXC_AUTOSAVE_INPUTFX_MSG,
  FXC_AUTOSAVE_TR_MSG,
  FXC_AUTOSAVE_ITEM_MSG,
  FXC_AUTOSAVE_DEFNAME_MSG,
  FXC_AUTOSAVE_FX1NAME_MSG,

  // track template cmds
  TRT_START_MSG,                         // 5 cmds --->
  TRT_END_MSG	= TRT_START_MSG+4,         // <---
  TRT_AUTOSAVE_WITEMS_MSG,
  TRT_AUTOSAVE_WENVS_MSG,

  // project template cmds
  PRJ_START_MSG,                         // 2 cmds --->
  PRJ_END_MSG = PRJ_START_MSG+1,         // <---
  PRJ_AUTOFILL_RECENTS_MSG,

  // media file cmds
  MED_START_MSG,                         // 5 cmds --->
  MED_END_MSG = MED_START_MSG+4,         // <---
  MED_OPT_START_MSG,                     // 5 options --->
  MED_OPT_END_MSG = MED_OPT_START_MSG+4, // <---

  // image file cmds
  IMG_START_MSG,                         // 3 cmds --->
  IMG_END_MSG = IMG_START_MSG+2,         // <---

  // theme file cmds
  THM_START_MSG,                         // 2 cmds (only 1 theme cmd used ATM) --->
  THM_END_MSG = THM_START_MSG+1,         // <---

  // (newer) common cmds
  LOAD_TIED_PRJ_MSG,
  LOAD_TIED_PRJ_TAB_MSG,
  COPY_BOOKMARK_MSG,
  DEL_BOOKMARK_MSG,
  REN_BOOKMARK_MSG,
  NEW_BOOKMARK_START_MSG,                            // 512 bookmarks max -->
  NEW_BOOKMARK_END_MSG = NEW_BOOKMARK_START_MSG+512, // <--

  LAST_MSG // keep as last item!
};

enum {
  BTNID_AUTOFILL=LAST_MSG,
  BTNID_AUTOSAVE,
  CMBID_TYPE,
  TXTID_TIED_PRJ,
  WNDID_ADD_DEL,
  BTNID_ADD_BOOKMARK,
  BTNID_DEL_BOOKMARK,
  BTNID_OFFSET_TR_TEMPLATE,
  TXTID_DBL_TYPE,
  CMBID_DBLCLICK_TYPE
};


// labels dropdown box & popup menu items
#define FXC_APPLY_TR_STR			__LOCALIZE("Paste (replace) to selected tracks","sws_DLG_150")
#define FXCIN_APPLY_TR_STR			__LOCALIZE("Paste (replace) as input FX to selected tracks","sws_DLG_150")
#define FXC_PASTE_TR_STR			__LOCALIZE("Paste to selected tracks","sws_DLG_150")
#define FXCIN_PASTE_TR_STR			__LOCALIZE("Paste as input FX to selected tracks","sws_DLG_150")
#define FXC_APPLY_TAKE_STR			__LOCALIZE("Paste (replace) to selected items","sws_DLG_150")
#define FXC_APPLY_ALLTAKES_STR		__LOCALIZE("Paste (replace) to selected items, all takes","sws_DLG_150")
#define FXC_PASTE_TAKE_STR			__LOCALIZE("Paste to selected items","sws_DLG_150")
#define FXC_PASTE_ALLTAKES_STR		__LOCALIZE("Paste to selected items, all takes","sws_DLG_150")
#define TRT_APPLY_STR				__LOCALIZE("Apply to selected tracks","sws_DLG_150")
#define TRT_APPLY_WITH_ENV_ITEM_STR	__LOCALIZE("Apply to selected tracks (+items/envelopes)","sws_DLG_150")
#define TRT_IMPORT_STR				__LOCALIZE("Import tracks","sws_DLG_150")
#define TRT_REPLACE_ITEMS_STR		__LOCALIZE("Paste (replace) template items to selected tracks","sws_DLG_150")
#define TRT_PASTE_ITEMS_STR			__LOCALIZE("Paste template items to selected tracks","sws_DLG_150")
#define PRJ_SELECT_LOAD_STR			__LOCALIZE("Open project","sws_DLG_150")
#define PRJ_SELECT_LOAD_TAB_STR		__LOCALIZE("Open project (new tab)","sws_DLG_150")
#define TIED_PRJ_SELECT_LOAD_STR	__LOCALIZE_VERFMT("Open attached project %s","sws_DLG_150")
#define TIED_PRJ_SELECT_LOADTAB_STR	__LOCALIZE_VERFMT("Open attached project %s (new tab)","sws_DLG_150")
#define MED_PLAY_STR				__LOCALIZE("Play in selected tracks (toggle)","sws_DLG_150")
#define MED_LOOP_STR				__LOCALIZE("Loop in selected tracks (toggle)","sws_DLG_150")
#define MED_ADD_TR_STR				__LOCALIZE("Add to current track","sws_DLG_150")
#define MED_ADD_NEWTR_STR			__LOCALIZE("Add to new tracks","sws_DLG_150")
#define MED_ADD_ITEM_STR			__LOCALIZE("Add to selected items","sws_DLG_150")
#define IMG_SHOW_STR				__LOCALIZE("Show image","sws_DLG_150")
#define IMG_TRICON_STR				__LOCALIZE("Set as icon for selected tracks","sws_DLG_150")
#define THM_LOAD_STR				__LOCALIZE("Load theme","sws_DLG_150")

// no default filter text on OSX (cannot catch EN_SETFOCUS/EN_KILLFOCUS)
#ifndef _SNM_SWELL_ISSUES
#define FILTER_DEFAULT_STR		__LOCALIZE("Filter","sws_DLG_150")
#else
#define FILTER_DEFAULT_STR		""
#endif
#define DRAGDROP_EMPTY_SLOT		">Empty<" // no localization: internal stuff
#define AUTOSAVE_ERR_STR		__LOCALIZE("Probable cause: no selection, nothing to save, cannot write file, file in use, invalid filename, etc...","sws_DLG_150")
#define AUTOFILL_ERR_STR		__LOCALIZE("Probable cause: all files are already present, empty directory, directory not found, etc...","sws_DLG_150")
// #define NO_SLOT_ERR_STR			__LOCALIZE("No slot found in the bookmark \"%s\" of Resources window!","sws_DLG_150")

enum {
  COL_SLOT=0,
  COL_NAME,
  COL_PATH,
  COL_COMMENT,
  COL_COUNT
};


// JFB important remarks!
//
// All WDL_PtrList_DOD global vars below used to be WDL_PtrList_DeleteOnDestroy but
// something weird could occur when REAPER was unloading the extension: hangs or crashes 
// on Win 7 (e.g. issues 292 & 380, those lists were probably already unallocated when 
// saving ini files? or unallocated concurrently?).
//
// Anyway, with WDL_PtrList_DOD this issue can now only occur in debug builds, which 
// is "good to have" since I was never able to duplicate it...

SNM_WindowManager<ResourcesWnd> g_resWndMgr(RES_WND_ID);
WDL_PtrList_DOD<ResourceList> g_SNM_ResSlots;
WDL_PtrList_DOD<ResourceItem> g_dragResourceItems; 

// prefs
int g_resType = -1; // current type (user selection)
int g_tiedSlotActions[SNM_NUM_DEFAULT_SLOTS]; // slot actions of default type/idx are tied to type/value
int g_dblClickPrefs[SNM_MAX_SLOT_TYPES];
WDL_FastString g_filter; // see init + localization in ResourcesInit()
int g_filterPref = 1; // bitmask: &1 = filter by name, &2 = filter by path, &4 = filter by comment
WDL_PtrList_DOD<WDL_FastString> g_autoSaveDirs;
WDL_PtrList_DOD<WDL_FastString> g_autoFillDirs;
WDL_PtrList_DOD<WDL_FastString> g_tiedProjects;
bool g_syncAutoDirPrefs[SNM_MAX_SLOT_TYPES];

// auto-save prefs
int g_asTrTmpltPref = 3; // &1: with items, &2: with envs
int g_asFXChainPref = FXC_AUTOSAVE_PREF_TRACK;
int g_asFXChainNamePref = 0;

// add media options (GUI + add media file slot actions, bitmask)
int g_addMedPref = 0;	// 0=add as is, &4=stretch/loop to fit time sel
						// &8=try to match tempo 1x, &16=try to match tempo 0.5x, &32=try to match tempo 2x

// vars used to track project switches
// note: ReaPoject* is not enough as this pointer can be re-used when loading RPP files
char g_curProjectFn[SNM_MAX_PATH]="";
ReaProject* g_curProject = NULL;

// for next/prev project actions
int g_prjCurSlot = -1; // 0-based


///////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////

const char* GetAutoSaveDir(int _type = -1) {
	return g_autoSaveDirs.Get(_type<0 ? g_resType : _type)->Get();
}

void SetAutoSaveDir(const char* _path, int _type = -1) {
	if (_type < 0) _type = g_resType;
	g_autoSaveDirs.Get(_type)->Set(_path);
	if (g_syncAutoDirPrefs[_type])
		g_autoFillDirs.Get(_type)->Set(_path);
}

const char* GetAutoFillDir(int _type = -1) {
	return g_autoFillDirs.Get(_type<0 ? g_resType : _type)->Get();
}

void SetAutoFillDir(const char* _path, int _type = -1) {
	if (_type < 0) _type = g_resType;
	g_autoFillDirs.Get(_type)->Set(_path);
	if (g_syncAutoDirPrefs[_type])
		g_autoSaveDirs.Get(_type)->Set(_path);
}

int GetTypeForUser(int _type = -1)
{
	if (_type < 0) _type = g_resType;
	if (_type >= SNM_NUM_DEFAULT_SLOTS)
		for (int i=0; i < SNM_NUM_DEFAULT_SLOTS; i++)
			if (!_stricmp(g_SNM_ResSlots.Get(_type)->GetFileExtStr(), g_SNM_ResSlots.Get(i)->GetFileExtStr()))
				return i;
	return _type;
}

bool IsMultiType(int _type = -1)
{
	if (_type < 0) _type = g_resType;
	int typeForUser = GetTypeForUser(_type);
	for (int i=0; i < g_SNM_ResSlots.GetSize(); i++)
		if (i != _type && GetTypeForUser(i) == typeForUser)
			return true;
	return false;
}

void GetIniSectionName(int _type, char* _bufOut, size_t _bufOutSz)
{
	if (_type >= SNM_NUM_DEFAULT_SLOTS)
	{
		*_bufOut = '\0';
		snprintf(_bufOut, _bufOutSz, "CustomSlotType%d", _type-SNM_NUM_DEFAULT_SLOTS+1);
		return;
	}
	// relative path needed, for ex: data\track_icons
	lstrcpyn(_bufOut, GetFileRelativePath(g_SNM_ResSlots.Get(_type)->GetResourceDir()), _bufOutSz);
	*_bufOut = toupper(*_bufOut);
}

// it is up to the caller to unalloc.. 
void GetIniSectionNames(WDL_PtrList<WDL_FastString>* _iniSections)
{
	char iniSec[64]="";
	for (int i=0; i < g_SNM_ResSlots.GetSize(); i++) {
		GetIniSectionName(i, iniSec, sizeof(iniSec));
		_iniSections->Add(new WDL_FastString(iniSec));
	}
}

bool IsFiltered() {
	return (g_filter.GetLength() && strcmp(g_filter.Get(), FILTER_DEFAULT_STR));
}

// attach _fn to _bookmarkType's project if the project is open and 
// and if _fn is not already present (optional, not verified by default: safer with some corner cases, 
// e.g. unexisting file referenced in duplicated slots, etc..)
// note: better to re-attach files (no-op) than missing them!
void TieResFileToProject(const char* _fn, int _bookmarkType, bool _checkDup = false, bool _tie = true)
{
	if (!_fn || !*_fn)
		return;

	if (_bookmarkType>=SNM_NUM_DEFAULT_SLOTS && g_tiedProjects.Get(_bookmarkType)->GetLength())
		if (ResourceList* fl = g_SNM_ResSlots.Get(_bookmarkType))
			if (!_checkDup || (_checkDup && fl->FindByPath(_fn)<0))
			{
				int i=0;
				char path[SNM_MAX_PATH] = "";
				while (ReaProject* prj = EnumProjects(i++, path, sizeof(path)))
				{
					if (!_stricmp(g_tiedProjects.Get(_bookmarkType)->Get(), path))
					{
						TieFileToProject(_fn, prj, _tie);
						return;
					}
				}
			}
}

// detach _fn from _bookmarkType's project if the project is open and 
// if _fn is indeed not present anymore (optional, to avoid to datach duplicated slots)
void UntieResFileFromProject(const char* _fn, int _bookmarkType, bool _checkDup = true)
{
	if (!_fn || !*_fn)
		return;

	if (_bookmarkType>=SNM_NUM_DEFAULT_SLOTS && g_tiedProjects.Get(_bookmarkType)->GetLength())
		if (ResourceList* fl = g_SNM_ResSlots.Get(_bookmarkType))
			if (!_checkDup || (_checkDup && fl->FindByPath(_fn)<0))
				TieResFileToProject(_fn, _bookmarkType, false, false);
}

// attach all files to _prj
// _type<0: attach all files of all bookmaks, given bookmark otherwise
void TieAllResFileToProject(ReaProject* _prj, const char* _prjPath, int _type = -1, bool _tie = true)
{
	if (_prj && _prjPath && *_prjPath)
	{
		for (int i = (_type<0 ? SNM_NUM_DEFAULT_SLOTS : _type); 
			i <= (_type<0 ? g_SNM_ResSlots.GetSize()-1 : _type); 
			i++)
		{
			if (g_tiedProjects.Get(i)->GetLength())
			{
				if (!_stricmp(g_tiedProjects.Get(i)->Get(), _prjPath))
				{
					char fullpath[SNM_MAX_PATH]="";
					for (int j=0; j<g_SNM_ResSlots.Get(i)->GetSize(); j++)
						if (g_SNM_ResSlots.Get(i)->GetFullPath(j, fullpath, sizeof(fullpath)))
							TieFileToProject(fullpath, _prj, _tie);
				}
			}
		}
	}
}

// detach all files from _prj
// _type<0: detach all files of all bookmaks, given bookmark otherwise
void UntieAllResFileFromProject(ReaProject* _prj, const char* _prjPath, int _type = -1) {
	TieAllResFileToProject(_prj, _prjPath, _type, false);
}


///////////////////////////////////////////////////////////////////////////////
// ResourceList
///////////////////////////////////////////////////////////////////////////////

ResourceList::ResourceList(const char* _resDir, const char* _name, const char* _ext, int _flags)
	: m_name(_name), m_ext(_ext), m_flags(_flags), WDL_PtrList<ResourceItem>()
{
	char tmp[512]="";

	// resource directory: set specific OS path separator, sanitize path
	int i;
	lstrcpyn(tmp, _resDir, sizeof(tmp));
#ifdef _WIN32
	i=-1;
	while (tmp[++i])
		if (tmp[i]=='/')
			tmp[i]=PATH_SLASH_CHAR;
#endif
	i = strlen(tmp);
	if (tmp[i-1] == PATH_SLASH_CHAR)
		tmp[i-1] = '\0';
	m_resDir.Set(tmp);

	// split extensions
	char ext[16];
	bool first = true;
	lstrcpyn(tmp, _ext, sizeof(tmp));
	char* tok = strtok(tmp, ",");
	while (tok)
	{
		lstrcpyn(ext, tok, sizeof(ext));
		tok = strtok(NULL, ",");

		// upgrade if needed
		if (first && !tok && !_stricmp("RPP", ext))
			m_exts.Add(new WDL_FastString("RPP*"));
		// upgrade + PNG*, see GetFileFilter()
		else if (first && !tok && (!_stricmp("PNG", ext) || !_stricmp("PNG*", ext)))
			SplitStrings(SNM_REAPER_IMG_EXTS, &m_exts);
		else
			m_exts.Add(new WDL_FastString(ext));
		first = false;
	}

	// upgrade if needed
	// in versions <= v2.3.0 #16, the extension "" was meaning "all media files"
	if (!m_exts.GetSize())
		m_exts.Add(new WDL_FastString("WAV*"));
}

// _path: short resource path or full path
ResourceItem* ResourceList::AddSlot(const char* _path, const char* _desc)
{
	return Add(new ResourceItem(GetShortResourcePath(m_resDir.Get(), _path), _desc));
}

// _path: short resource path or full path
ResourceItem* ResourceList::InsertSlot(int _slot, const char* _path, const char* _desc)
{
	ResourceItem* item = NULL;
	const char* shortPath = GetShortResourcePath(m_resDir.Get(), _path);
	if (_slot >=0 && _slot < GetSize())
		item = Insert(_slot, new ResourceItem(shortPath, _desc));
	else
		item = Add(new ResourceItem(shortPath, _desc));
	return item;
}

int ResourceList::FindByPath(const char* _fullPath)
{
	char fullpath[SNM_MAX_PATH];
	if (_fullPath)
		for (int i=0; i<GetSize(); i++)
			if (GetFullPath(i, fullpath, sizeof(fullpath)))
				if (!_stricmp(_fullPath, fullpath))
					return i;
	return -1;
}

bool ResourceList::GetFullPath(int _slot, char* _fullFn, int _fullFnSz)
{
	if (ResourceItem* item = Get(_slot)) {
		GetFullResourcePath(m_resDir.Get(), item->m_shortPath.Get(), _fullFn, _fullFnSz);
		return true;
	}
	return false;
}

bool ResourceList::SetFromFullPath(int _slot, const char* _fullPath)
{
	if (ResourceItem* item = Get(_slot))
	{
		item->m_shortPath.Set(GetShortResourcePath(m_resDir.Get(), _fullPath));
		return true;
	}
	return false;
}

bool ResourceList::IsValidFileExt(const char* _ext)
{
	if (_ext && *_ext && m_exts.GetSize())
	{
		// wildcard?
		if (!strcmp("*", m_exts.Get(0)->Get()))
			return true;

/* JFB replaced with code below => single place to support new file extensions == GetFileFilter()
		for (int i=0; i<m_exts.GetSize(); i++)
			if (!strcmp("*", m_exts.Get(i)->Get()) || !_stricmp(_ext, m_exts.Get(i)->Get()))
				return true;
*/
		WDL_FastString ext;
		ext.SetFormatted(16, "*.%s", _ext);
		char fileFilter[2048]=""; // room needed for file filters!
		GetFileFilter(fileFilter, sizeof(fileFilter), false);
		return (stristr(fileFilter, ext.Get()) != NULL);
	}
	return false;
}

void ResourceList::GetFileFilter(char* _filter, size_t _filterSz, bool _dblNull)
{
	if (_filter)
	{
		memset(_filter, '\0', _filterSz);

		bool transcode = false;
		if (m_exts.GetSize())
		{
/*JFB commented: linking against LICE/PNG only atm..
			// better filter from LICE for images
			if (!_strnicmp("PNG*", m_exts.Get(0)->Get(), 4)) {
				memcpy(_filter, LICE_GetImageExtensionList(), _filterSz);
				transcode = !_dblNull;
			}
			else 
*/ 
			if (!_strnicmp("RPP*", m_exts.Get(0)->Get(), 4)) {
				memcpy(_filter, plugin_getImportableProjectFilterList(), _filterSz);
				transcode = !_dblNull;
			}
			else if (!_strnicmp("WAV*", m_exts.Get(0)->Get(), 4)) {
				memcpy(_filter, plugin_getFilterList(), _filterSz);
				transcode = !_dblNull;
			}
			// generic filter
			else
			{
				WDL_FastString exts;
				if (strcmp("*", m_exts.Get(0)->Get())) // not wildcard?
				{
					if (m_exts.GetSize()>1)
					{
						exts.Append(__LOCALIZE("All supported files","sws_DLG_150"));
						exts.Append("|");
						for (int i=0; i<m_exts.GetSize(); i++)
							exts.AppendFormatted(128, "*.%s;", m_exts.Get(i)->Get());
						exts.Append("|");
					}

					for (int i=0; i<m_exts.GetSize(); i++)
						exts.AppendFormatted(128, "*.%s|*.%s|", m_exts.Get(i)->Get(), m_exts.Get(i)->Get());
				}
				else if (!_dblNull)
					exts.Set("*");

				if (_dblNull) {
					exts.Append(__LOCALIZE("All files","sws_DLG_150"));
					exts.Append(" (*.*)|*.*|");
				}

				lstrcpyn(_filter, exts.Get(), _filterSz);
				transcode = _dblNull;
			}
		}

		if (transcode)
		{
			int i = -1;
			if (_dblNull) // trick: '|' -> '\0'
			{
				while ((++i) < (int)(_filterSz-2)) // -2: safety
					if (_filter[i] == '|')
						_filter[i] = '\0';
			}
			else // trick: '\0' -> ' '
			{
				while ((++i) < (int)(_filterSz-1)) // -1: safety
					if (_filter[i] == '\0' && _filter[i+1] != '\0')
						_filter[i] = ' ';
			}
		}
	}
}

bool ResourceList::ClearSlot(int _slot)
{
	if (_slot>=0 && _slot<GetSize()) {
		Get(_slot)->Clear();
		return true;
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// ResourcesView
///////////////////////////////////////////////////////////////////////////////

// !WANT_LOCALIZE_STRINGS_BEGIN:sws_DLG_150
static SWS_LVColumn s_resListCols[] = { {65,2,"Slot"}, {100,1,"Name"}, {250,2,"Path"}, {200,1,"Comment"} };
// !WANT_LOCALIZE_STRINGS_END

ResourcesView::ResourcesView(HWND hwndList, HWND hwndEdit)
	: SWS_ListView(hwndList, hwndEdit, COL_COUNT, s_resListCols, "ResourcesViewState", false, "sws_DLG_150")
{
}

void ResourcesView::GetItemText(SWS_ListItem* item, int iCol, char* str, int iStrMax)
{
	if (str) *str = '\0';
	if (ResourceItem* pItem = (ResourceItem*)item)
	{
		switch (iCol)
		{
			case COL_SLOT:
			{
				ResourceList* fl = g_SNM_ResSlots.Get(g_resType);
				int slot = fl ? fl->Find(pItem) : -1;
				if (slot >= 0)
				{
					slot++;
					if (g_resType==g_tiedSlotActions[SNM_SLOT_PRJ] && g_prjCurSlot>=0 && g_prjCurSlot+1==slot)
						snprintf(str, iStrMax, "%5.d %s", slot, UTF8_BULLET);
					else
						snprintf(str, iStrMax, "%5.d", slot);
				}
				break;
			}
			case COL_NAME:
				GetFilenameNoExt(pItem->m_shortPath.Get(), str, iStrMax);
				break;
			case COL_PATH:
				lstrcpyn(str, pItem->m_shortPath.Get(), iStrMax);
				break;
			case COL_COMMENT:
				lstrcpyn(str, pItem->m_comment.Get(), iStrMax);
				break;
		}
	}
}

bool ResourcesView::IsEditListItemAllowed(SWS_ListItem* item, int iCol)
{
	if (ResourceItem* pItem = (ResourceItem*)item)
	{
		ResourceList* fl = g_SNM_ResSlots.Get(g_resType);
		int slot = fl ? fl->Find(pItem) : -1;
		if (slot>=0)
		{
			switch (iCol)
			{
				case COL_NAME: // file renaming
					if (!pItem->IsDefault()) {
						char fn[SNM_MAX_PATH] = "";
						return (fl->GetFullPath(slot, fn, sizeof(fn)) && FileOrDirExists(fn));
					}
					break;
				case COL_COMMENT:
					return true;
			}
		}
	}
	return false;
}

void ResourcesView::SetItemText(SWS_ListItem* item, int iCol, const char* str)
{
	ResourceItem* pItem = (ResourceItem*)item;
	ResourceList* fl = g_SNM_ResSlots.Get(g_resType);
	int slot = fl ? fl->Find(pItem) : -1;
	if (pItem && slot>=0)
	{
		switch (iCol)
		{
			case COL_NAME: // file renaming
			{
				if (!IsValidFilenameErrMsg(str, true))
					return;

				char fn[SNM_MAX_PATH] = "";
				if (fl->GetFullPath(slot, fn, sizeof(fn)) && !pItem->IsDefault() && FileOrDirExistsErrMsg(fn))
				{
					const char* ext = GetFileExtension(fn);
					char newFn[SNM_MAX_PATH]="", path[SNM_MAX_PATH]="";
					lstrcpyn(path, fn, sizeof(path));
					if (char* p = strrchr(path, PATH_SLASH_CHAR))
						*p = '\0';
					else
						break; // safety

					if (snprintfStrict(newFn, sizeof(newFn), "%s%c%s.%s", path, PATH_SLASH_CHAR, str, ext) > 0)
					{
						if (FileOrDirExists(newFn)) 
						{
							char msg[SNM_MAX_PATH]="";
							snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("File already exists. Overwrite ?\n%s","sws_mbox"), newFn);
							int res = MessageBox(g_resWndMgr.GetMsgHWND(), msg, __LOCALIZE("S&M - Warning","sws_DLG_150"), MB_YESNO);
							if (res == IDYES)
							{
								if (SNM_DeleteFile(newFn, false))
									UntieResFileFromProject(newFn, g_resType, false);
								else
									break;
							}
							else
								break;
						}

						if (MoveFile(fn, newFn) && fl->SetFromFullPath(slot, newFn))
						{
							UntieResFileFromProject(fn, g_resType);
							TieResFileToProject(newFn, g_resType);

							ListView_SetItemText(m_hwndList, GetEditingItem(), DisplayToDataCol(2), (LPSTR)pItem->m_shortPath.Get());
							// ^^ direct GUI update because Update() is no-op when editing
						}
					}
				}
				break;
			}
			case COL_COMMENT:
				pItem->m_comment.Set(str);
				pItem->m_comment.Ellipsize(128,128);
				Update();
				break;
		}
	}
}

void ResourcesView::OnItemDblClk(SWS_ListItem* item, int iCol) {
	Perform(g_dblClickPrefs[g_resType]);
}

void ResourcesView::GetItemList(SWS_ListItemList* pList)
{
	ResourceList* fl = g_SNM_ResSlots.Get(g_resType);
	if (!fl)
		return;

	if (IsFiltered())
	{
		char buf[SNM_MAX_PATH] = "";
		LineParser lp(false);
		if (!lp.parse(g_filter.Get()))
		{
			for (int i=0; i < fl->GetSize(); i++)
			{
				if (ResourceItem* item = fl->Get(i))
				{
					bool match = false;
					for (int j=0; !match && j < lp.getnumtokens(); j++)
					{
						if (g_filterPref&1) // name
						{
							GetFilenameNoExt(item->m_shortPath.Get(), buf, sizeof(buf));
							match |= (stristr(buf, lp.gettoken_str(j)) != NULL);
						}
						if (!match && (g_filterPref&2)) // path
						{
							if (fl->GetFullPath(i, buf, sizeof(buf)))
								if (char* p = strrchr(buf, PATH_SLASH_CHAR)) {
									*p = '\0';
									match |= (stristr(buf, lp.gettoken_str(j)) != NULL);
								}
						}
						if (!match && (g_filterPref&4)) // comment
							match |= (stristr(item->m_comment.Get(), lp.gettoken_str(j)) != NULL);
					}
					if (match)
						pList->Add((SWS_ListItem*)item);
				}
			}
		}
	}
	else
	{
		for (int i=0; i < fl->GetSize(); i++)
			pList->Add((SWS_ListItem*)fl->Get(i));
	}
}

void ResourcesView::OnBeginDrag(SWS_ListItem* _item)
{
#ifdef _WIN32
	ResourceList* fl = g_SNM_ResSlots.Get(g_resType);
	if (!fl)
		return;

	g_dragResourceItems.Empty(false);

	// store dragged items (for internal drag-drop) + full paths + get the amount of memory needed
	int iMemNeeded = 0, x=0;
	WDL_PtrList_DeleteOnDestroy<WDL_FastString> fullPaths;
	while (ResourceItem* pItem = (ResourceItem*)EnumSelected(&x))
	{
		int slot = fl->Find(pItem);
		char fullPath[SNM_MAX_PATH] = "";
		if (fl->GetFullPath(slot, fullPath, sizeof(fullPath)))
		{
			bool empty = (pItem->IsDefault() || *fullPath == '\0');
			iMemNeeded += (int)((empty ? strlen(DRAGDROP_EMPTY_SLOT) : strlen(fullPath)) + 1);
			fullPaths.Add(new WDL_FastString(empty ? DRAGDROP_EMPTY_SLOT : fullPath));
			g_dragResourceItems.Add(pItem);
		}
	}
	if (!iMemNeeded)
		return;

	iMemNeeded += sizeof(DROPFILES) + 1;

	HGLOBAL hgDrop = GlobalAlloc (GHND | GMEM_SHARE, iMemNeeded);
	DROPFILES* pDrop = NULL;
	if (hgDrop)
		pDrop = (DROPFILES*)GlobalLock(hgDrop); // 'spose should do some error checking...
	
	// for safety..
	if (!hgDrop || !pDrop)
		return;

	pDrop->pFiles = sizeof(DROPFILES);
	pDrop->fWide = false;
	char* pBuf = (char*)pDrop + pDrop->pFiles;

	// add files to the DROPFILES struct, double-NULL terminated
	for (int i=0; i < fullPaths.GetSize(); i++) {
		strcpy(pBuf, fullPaths.Get(i)->Get());
		pBuf += strlen(pBuf) + 1;
	}
	*pBuf = 0;

	GlobalUnlock(hgDrop);
	FORMATETC etc = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
	STGMEDIUM stgmed = { TYMED_HGLOBAL, { 0 }, 0 };
	stgmed.hGlobal = hgDrop;

	SWS_IDataObject* dataObj = new SWS_IDataObject(&etc, &stgmed);
	SWS_IDropSource* dropSrc = new SWS_IDropSource;
	DWORD effect;

	DoDragDrop(dataObj, dropSrc, DROPEFFECT_COPY, &effect);
#endif
}

void ResourcesView::Perform(int _what)
{
	int x=0;
	WDL_FastString undo;
	ResourceItem* pItem = (ResourceItem*)EnumSelected(&x);
	int slot = g_SNM_ResSlots.Get(g_resType)->Find(pItem);
	while (pItem && slot >= 0)
	{
		switch(GetTypeForUser())
		{
			case SNM_SLOT_FXC:
				switch(_what)
				{
					case 0:
						SNM_GetActionName("S&M_PASTE_TRACKFXCHAIN", &undo, slot);
						ApplyTracksFXChainSlot(g_resType, undo.Get(), slot, false, false);
						goto done;
					case 1:
						SNM_GetActionName("S&M_PASTE_INFXCHAIN", &undo, slot);
						ApplyTracksFXChainSlot(g_resType, undo.Get(), slot, false, true);
						goto done;
					case 2:
						SNM_GetActionName("S&M_TRACKFXCHAIN", &undo, slot);
						ApplyTracksFXChainSlot(g_resType, undo.Get(), slot, true, false);
						goto done;
					case 3:
						SNM_GetActionName("S&M_INFXCHAIN", &undo, slot);
						ApplyTracksFXChainSlot(g_resType, undo.Get(), slot, true, true);
						goto done;
					case 4:
						SNM_GetActionName("S&M_PASTE_TAKEFXCHAIN", &undo, slot);
						ApplyTakesFXChainSlot(g_resType, undo.Get(), slot, true, false);
						goto done;
					case 5:
						SNM_GetActionName("S&M_PASTE_FXCHAIN_ALLTAKES", &undo, slot);
						ApplyTakesFXChainSlot(g_resType, undo.Get(), slot, false, false);
						goto done;
					case 6:
						SNM_GetActionName("S&M_TAKEFXCHAIN", &undo, slot);
						ApplyTakesFXChainSlot(g_resType, undo.Get(), slot, true, true);
						goto done;
					case 7:
						SNM_GetActionName("S&M_FXCHAIN_ALLTAKES", &undo, slot);
						ApplyTakesFXChainSlot(g_resType, undo.Get(), slot, false, true);
						goto done;
				}
				break;
			case SNM_SLOT_TR:
				switch(_what)
				{
					case 0:
						SNM_GetActionName("S&M_ADD_TRTEMPLATE", &undo, slot);
						ImportTrackTemplateSlot(g_resType, undo.Get(), slot);
						break;
					case 1:
						SNM_GetActionName("S&M_APPLY_TRTEMPLATE", &undo, slot);
						ApplyTrackTemplateSlot(g_resType, undo.Get(), slot, false, false);
						goto done;
					case 2:
						SNM_GetActionName("S&M_APPLY_TRTEMPLATE_ITEMSENVS", &undo, slot);
						ApplyTrackTemplateSlot(g_resType, undo.Get(), slot, true, true);
						goto done;
					case 3:
						SNM_GetActionName("S&M_PASTE_TEMPLATE_ITEMS", &undo, slot);
						ReplacePasteItemsTrackTemplateSlot(g_resType, undo.Get(), slot, true);
						break;
					case 4:
						SNM_GetActionName("S&M_REPLACE_TEMPLATE_ITEMS", &undo, slot);
						ReplacePasteItemsTrackTemplateSlot(g_resType, undo.Get(), slot, false);
						break;
				}
				break;
			case SNM_SLOT_PRJ:
				switch(_what)
				{
					case 0:
//						SNM_GetActionName("S&M_APPLY_PRJTEMPLATE", &undo, slot);
						LoadOrSelectProjectSlot(g_resType, /*undo.Get()*/ NULL, slot, false);
						goto done;
					case 1:
//						SNM_GetActionName("S&M_NEWTAB_PRJTEMPLATE", &undo, slot);
						LoadOrSelectProjectSlot(g_resType, /*undo.Get()*/ NULL, slot, true);
						break;
				}
				break;
			case SNM_SLOT_MEDIA:
			{
				int insertMode = -1;
				switch(_what) 
				{
					case 0:
//						SNM_GetActionName("S&M_PLAYMEDIA_SELTRACK", &undo, slot);
						TogglePlaySelTrackMediaSlot(g_resType, /*undo.Get()*/ NULL, slot, false, false);
						break;
					case 1:
//						SNM_GetActionName("S&M_LOOPMEDIA_SELTRACK", &undo, slot);
						TogglePlaySelTrackMediaSlot(g_resType, /*undo.Get()*/ NULL, slot, false, true);
						break;
					case 2:
						SNM_GetActionName("S&M_ADDMEDIA_CURTRACK", &undo, slot);
						insertMode = 0;
						break;
					case 3:
						SNM_GetActionName("S&M_ADDMEDIA_NEWTRACK", &undo, slot);
						insertMode = 1;
						break;
					case 4:
						SNM_GetActionName("S&M_ADDMEDIA_SELITEM", &undo, slot);
						insertMode = 3;
						break;
				}
				if (insertMode >= 0)
				{
					AddMediaOptionName(&undo);
					InsertMediaSlot(g_resType, undo.Get(), slot, insertMode|g_addMedPref);
				}
				break;
			}
			case SNM_SLOT_IMG:
				switch(_what)
				{
					case 0:
//						SNM_GetActionName("S&M_SHOW_IMG", &undo, slot);
						ShowImageSlot(g_resType, /*undo.Get()*/ NULL, slot);
						goto done; // single instance window ATM
					case 1:
						SNM_GetActionName("S&M_SET_TRACK_ICON", &undo, slot);
						SetSelTrackIconSlot(g_resType, undo.Get(), slot);
						goto done;
					case 2:
						SNM_GetActionName("S&M_ADDMEDIA_CURTRACK", &undo, slot);
						InsertMediaSlot(g_resType, undo.Get(), slot, 0);
						break;
				}
				break;
	 		case SNM_SLOT_THM:
//				SNM_GetActionName("S&M_LOAD_THEME", &undo, slot);
				LoadThemeSlot(g_resType, /*undo.Get()*/ NULL, slot);
				goto done;
			default:
				// works on OSX too
				if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(g_resType, &slot)) 
				{
					fnStr->Insert("\"", 0);
					fnStr->Append("\"");
					ShellExecute(GetMainHwnd(), "open", fnStr->Get(), NULL, NULL, SW_SHOWNORMAL);
					delete fnStr;
				}
				goto done;
		}

		pItem = (ResourceItem*)EnumSelected(&x);
		slot = g_SNM_ResSlots.Get(g_resType)->Find(pItem);
	}

done:
	// update in case slected slots' content have changed, e.g.:
	// action ran on empty sel slot -> auto-prompt to fill slot (macro firendly) -> Perform()
	Update();
}


///////////////////////////////////////////////////////////////////////////////
// ResourcesWnd
///////////////////////////////////////////////////////////////////////////////

// S&M windows lazy init: below's "" prevents registering the SWS' screenset callback
// (use the S&M one instead - already registered via SNM_WindowManager::Init())
ResourcesWnd::ResourcesWnd()
	: SWS_DockWnd(IDD_SNM_RESOURCES, __LOCALIZE("Resources","sws_DLG_150"), "")
{
	m_id.Set(RES_WND_ID);
	// Must call SWS_DockWnd::Init() to restore parameters and open the window if necessary
	Init();
}

ResourcesWnd::~ResourcesWnd()
{
	m_btnsAddDel.RemoveAllChildren(false);
	m_btnsAddDel.SetRealParent(NULL);
}

void ResourcesWnd::OnInitDlg()
{
	m_resize.init_item(IDC_LIST, 0.0, 0.0, 1.0, 1.0);
	m_resize.init_item(IDC_FILTER, 1.0, 1.0, 1.0, 1.0);

	SetWindowLongPtr(GetDlgItem(m_hwnd, IDC_FILTER), GWLP_USERDATA, 0xdeadf00b);
	SetWindowLongPtr(GetDlgItem(m_hwnd, IDC_EDIT), GWLP_USERDATA, 0xdeadf00b);

	m_pLists.Add(new ResourcesView(GetDlgItem(m_hwnd, IDC_LIST), GetDlgItem(m_hwnd, IDC_EDIT)));


	LICE_CachedFont* font = SNM_GetThemeFont();

	m_vwnd_painter.SetGSC(WDL_STYLE_GetSysColor);
	m_parentVwnd.SetRealParent(m_hwnd);

	m_cbType.SetID(CMBID_TYPE);
	m_cbType.SetFont(font);
	FillTypeCombo();
	m_parentVwnd.AddChild(&m_cbType);

	m_txtTiedPrj.SetID(TXTID_TIED_PRJ);
	m_txtTiedPrj.SetFont(g_SNM_ClearType ? SNM_GetFont(0) : SNM_GetThemeFont()); // SNM_GetFont(0) to support alpha
																				 //JFB!! todo? mod/inherit WDL_VirtualStaticText?
	m_parentVwnd.AddChild(&m_txtTiedPrj);

	m_cbDblClickType.SetID(CMBID_DBLCLICK_TYPE);
	m_cbDblClickType.SetFont(font);
	m_parentVwnd.AddChild(&m_cbDblClickType);
	FillDblClickCombo();

	m_btnAutoFill.SetID(BTNID_AUTOFILL);
	m_parentVwnd.AddChild(&m_btnAutoFill);

	m_btnAutoSave.SetID(BTNID_AUTOSAVE);
	m_parentVwnd.AddChild(&m_btnAutoSave);

	m_txtDblClickType.SetID(TXTID_DBL_TYPE);
	m_txtDblClickType.SetFont(font);
	m_txtDblClickType.SetText(__LOCALIZE("Double-click:","sws_DLG_150"));
	m_parentVwnd.AddChild(&m_txtDblClickType);

	m_btnAdd.SetID(BTNID_ADD_BOOKMARK);
	m_btnsAddDel.AddChild(&m_btnAdd);
	m_btnDel.SetID(BTNID_DEL_BOOKMARK);
	m_btnsAddDel.AddChild(&m_btnDel);
	m_btnsAddDel.SetID(WNDID_ADD_DEL);
	m_parentVwnd.AddChild(&m_btnsAddDel);

	m_btnOffsetTrTemplate.SetID(BTNID_OFFSET_TR_TEMPLATE);
	m_btnOffsetTrTemplate.SetFont(font);
	m_btnOffsetTrTemplate.SetTextLabel(__LOCALIZE("Offset by edit cursor","sws_DLG_150"));
	m_parentVwnd.AddChild(&m_btnOffsetTrTemplate);

	// restores the text filter when docking/undocking + indirect call to Update()
	SetDlgItemText(m_hwnd, IDC_FILTER, g_filter.Get());

/* see above comment
	Update();
*/
}

void ResourcesWnd::OnDestroy() 
{
	m_cbType.Empty();
	m_cbDblClickType.Empty();
	m_btnsAddDel.RemoveAllChildren(false);
	m_btnsAddDel.SetRealParent(NULL);
}

void ResourcesWnd::SetType(int _type)
{
	int prevType = g_resType;
	g_resType = _type;
	m_cbType.SetCurSel2(_type);
	if (prevType != g_resType) {
		FillDblClickCombo();
		Update();
	}
}

int ResourcesWnd::SetType(const char* _name)
{
	if (_name && *_name)
	{
		int sel=0;
		for (int i=0; i<m_cbType.GetCount(); i++)
		{
			// zap " [x]" if needed (issue 615)
			WDL_FastString item;
			const char* p1 = m_cbType.GetItem(i);
			const char* p2 = strstr(p1, RES_TIE_TAG);
			item.Set(p1, p2 ? (int)(p2-p1) : 0);

			if (strcmp("<SEP>", item.Get()))
			{
				if (!_stricmp(item.Get(), _name)) {
					SetType(sel);
					return sel;
				}
				sel++;
			}
		}
	}
	return -1;
}

void ResourcesWnd::Update()
{
	if (m_pLists.GetSize())
		GetListView()->Update();
	m_parentVwnd.RequestRedraw(NULL);
}

void ResourcesWnd::FillTypeCombo()
{
	m_cbType.Empty();
	WDL_String typeName; // no fast string here, the buffer gets mangled
	for (int i=0; i<g_SNM_ResSlots.GetSize(); i++)
	{
		if (i==SNM_NUM_DEFAULT_SLOTS)
			m_cbType.AddItem("<SEP>");

		typeName.Set(g_SNM_ResSlots.Get(i)->GetName());
		*typeName.Get() = toupper(*typeName.Get()); // 1st char to upper
		if (IsMultiType(i))
			typeName.Append(g_tiedSlotActions[GetTypeForUser(i)] == i ? RES_TIE_TAG : "");
		m_cbType.AddItem(typeName.Get());
	}
	m_cbType.SetCurSel2((g_resType>0 && g_resType<g_SNM_ResSlots.GetSize()) ? g_resType : SNM_SLOT_FXC);
}

void ResourcesWnd::FillDblClickCombo()
{
	int typeForUser = GetTypeForUser();
	m_cbDblClickType.Empty();
	switch(typeForUser)
	{
		case SNM_SLOT_FXC:
			m_cbDblClickType.AddItem(FXC_PASTE_TR_STR);
			m_cbDblClickType.AddItem(FXCIN_PASTE_TR_STR);
			m_cbDblClickType.AddItem(FXC_APPLY_TR_STR);
			m_cbDblClickType.AddItem(FXCIN_APPLY_TR_STR);
			m_cbDblClickType.AddItem("<SEP>");
			m_cbDblClickType.AddItem(FXC_PASTE_TAKE_STR);
			m_cbDblClickType.AddItem(FXC_PASTE_ALLTAKES_STR);
			m_cbDblClickType.AddItem(FXC_APPLY_TAKE_STR);
			m_cbDblClickType.AddItem(FXC_APPLY_ALLTAKES_STR);
			break;
		case SNM_SLOT_TR:
			m_cbDblClickType.AddItem(TRT_IMPORT_STR);
			m_cbDblClickType.AddItem(TRT_APPLY_STR);
			m_cbDblClickType.AddItem(TRT_APPLY_WITH_ENV_ITEM_STR);
			m_cbDblClickType.AddItem("<SEP>");
			m_cbDblClickType.AddItem(TRT_PASTE_ITEMS_STR);
			m_cbDblClickType.AddItem(TRT_REPLACE_ITEMS_STR);
			break;
		case SNM_SLOT_PRJ:
			m_cbDblClickType.AddItem(PRJ_SELECT_LOAD_STR);
			m_cbDblClickType.AddItem(PRJ_SELECT_LOAD_TAB_STR);
			break;
		case SNM_SLOT_MEDIA:
		{
			m_cbDblClickType.AddItem(MED_PLAY_STR);
			m_cbDblClickType.AddItem(MED_LOOP_STR);
			m_cbDblClickType.AddItem("<SEP>");

			WDL_FastString name;
			name.Set(MED_ADD_TR_STR); AddMediaOptionName(&name);
			m_cbDblClickType.AddItem(name.Get());
			name.Set(MED_ADD_NEWTR_STR); AddMediaOptionName(&name);
			m_cbDblClickType.AddItem(name.Get());
			name.Set(MED_ADD_ITEM_STR); AddMediaOptionName(&name);
			m_cbDblClickType.AddItem(name.Get());
			break;
		}
		case SNM_SLOT_IMG:
			m_cbDblClickType.AddItem(IMG_SHOW_STR);
			m_cbDblClickType.AddItem("<SEP>");
			m_cbDblClickType.AddItem(IMG_TRICON_STR);
			m_cbDblClickType.AddItem(MED_ADD_TR_STR);
			break;
		case SNM_SLOT_THM:
			m_cbDblClickType.AddItem(THM_LOAD_STR);
			break;
		default:
			if (typeForUser >= SNM_NUM_DEFAULT_SLOTS) // custom bookmark?
				m_cbDblClickType.AddItem(__LOCALIZE("Open file in default application","sws_DLG_150"));
			break;
	}
	m_cbDblClickType.SetCurSel2(g_dblClickPrefs[g_resType]);
}

void ResourcesWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	ResourceList* fl = g_SNM_ResSlots.Get(g_resType);
	if (!fl || !m_pLists.GetSize())
		return;

	int x=0;
	ResourceItem* item = (ResourceItem*)GetListView()->EnumSelected(&x);
	int slot = fl->Find(item);

	switch(LOWORD(wParam))
	{
		case IDC_FILTER:
			if (HIWORD(wParam)==EN_CHANGE)
			{
				char filter[128]="";
				GetWindowText(GetDlgItem(m_hwnd, IDC_FILTER), filter, sizeof(filter));
				g_filter.Set(filter);
				Update();
			}
#ifndef _SNM_SWELL_ISSUES // EN_SETFOCUS, EN_KILLFOCUS not supported yet
			else if (HIWORD(wParam)==EN_SETFOCUS)
			{
				HWND hFilt = GetDlgItem(m_hwnd, IDC_FILTER);
				char filter[128]="";
				GetWindowText(hFilt, filter, sizeof(filter));
				if (!strcmp(filter, FILTER_DEFAULT_STR))
					SetWindowText(hFilt, "");
			}
			else if (HIWORD(wParam)==EN_KILLFOCUS)
			{
				HWND hFilt = GetDlgItem(m_hwnd, IDC_FILTER);
				char filter[128]="";
				GetWindowText(hFilt, filter, sizeof(filter));
				if (*filter == '\0') 
					SetWindowText(hFilt, FILTER_DEFAULT_STR);
			}
#endif
			break;

		// ***** Common *****
		case ADD_SLOT_MSG:
			AddSlot();
			break;
		case INSERT_SLOT_MSG:
			InsertAtSelectedSlot();
			break;
		case DEL_SLOTS_MSG:
			ClearDeleteSlotsFiles(g_resType, 1);
			break;
		case CLR_SLOTS_MSG:
			ClearDeleteSlotsFiles(g_resType, 2);
			break;
		case DEL_FILES_MSG:
			ClearDeleteSlotsFiles(g_resType, 2|4);
			break;
		case LOAD_MSG:
			if (item && BrowseSlot(g_resType, slot, true))
				Update();
			break;
		case EDIT_MSG:
			if (item && slot>=0 && slot<fl->GetSize())
			{
				char fullPath[SNM_MAX_PATH] = "";
				if (fl->GetFullPath(slot, fullPath, sizeof(fullPath)) && FileOrDirExistsErrMsg(fullPath))
				{
					if (fl->IsText())
					{
						// do not use ShellExecute's "edit" mode here as some file would be opened in
						// REAPER instead of the default text editor (e.g. .RTrackTemplate files)
						ShellExecute(GetMainHwnd(), "open", "notepad", fullPath, NULL, SW_SHOWNORMAL);
					}
				}
			}
			break;
#ifdef _WIN32
		case DIFF_MSG:
		{
			ResourceItem* item2 = (ResourceItem*)GetListView()->EnumSelected(&x);
			if (item && item2 && g_SNM_DiffToolFn.GetLength())
			{
				char fn1[SNM_MAX_PATH], fn2[SNM_MAX_PATH];
				if (slot>=0 && 
					fl->GetFullPath(slot, fn1, sizeof(fn1)) && FileOrDirExistsErrMsg(fn1) &&
					fl->GetFullPath(fl->Find(item2), fn2, sizeof(fn2)) && FileOrDirExistsErrMsg(fn2))
				{
					WDL_FastString prmStr;
					prmStr.SetFormatted(sizeof(fn1)*3, " \"%s\" \"%s\"", fn1, fn2);
					ShellExecute(GetMainHwnd(), "open", g_SNM_DiffToolFn.Get(), prmStr.Get(), NULL, SW_SHOWNORMAL);
				}
			}
			break;
		}
#endif
		case EXPLORE_MSG:
		{
			char fullPath[SNM_MAX_PATH];
			if (fl->GetFullPath(slot, fullPath, sizeof(fullPath)))
				RevealFile(fullPath);
			break;
		}
		case EXPLORE_FILLDIR_MSG:
			ShellExecute(NULL, "open", GetAutoFillDir(), NULL, NULL, SW_SHOWNORMAL);
			break;
		case EXPLORE_SAVEDIR_MSG:
			ShellExecute(NULL, "open", GetAutoSaveDir(), NULL, NULL, SW_SHOWNORMAL);
			break;
		// auto-fill
		case BTNID_AUTOFILL:
		case AUTOFILL_MSG:
			AutoFill(g_resType);
			break;
		case AUTOFILL_DIR_MSG:
		{
			char path[SNM_MAX_PATH] = "";
			if (BrowseForDirectory(__LOCALIZE("Set auto-fill directory","sws_DLG_150"), GetAutoFillDir(), path, sizeof(path)))
				SetAutoFillDir(path);
			break;
		}
		case AUTOFILL_PRJ_MSG:
		{
			char path[SNM_MAX_PATH] = "";
			GetProjectPath(path, sizeof(path));
			SetAutoFillDir(path);
			break;
		}
		case AUTOFILL_DEFAULT_MSG:
		{
			char path[SNM_MAX_PATH] = "";
			if (snprintfStrict(path, sizeof(path), "%s%c%s", GetResourcePath(), PATH_SLASH_CHAR, fl->GetResourceDir()) > 0)
			{
				if (!FileOrDirExists(path))
					CreateDirectory(path, NULL);
				SetAutoFillDir(path);
			}
			break;
		}
		case AUTOFILL_SYNC_MSG: // i.e. sync w/ auto-save
			g_syncAutoDirPrefs[g_resType] = !g_syncAutoDirPrefs[g_resType];
			if (g_syncAutoDirPrefs[g_resType])
				g_autoFillDirs.Get(g_resType)->Set(GetAutoSaveDir());  // do not use SetAutoFillDir() here!
			break;
		// auto-save
		case BTNID_AUTOSAVE:
		case AUTOSAVE_MSG:
			AutoSave(g_resType, true, GetTypeForUser()==SNM_SLOT_TR ? g_asTrTmpltPref : GetTypeForUser()==SNM_SLOT_FXC ? g_asFXChainPref : 0);
			break;
		case AUTOSAVE_DIR_MSG:
		{
			char path[SNM_MAX_PATH] = "";
			if (BrowseForDirectory(__LOCALIZE("Set auto-save directory","sws_DLG_150"), GetAutoSaveDir(), path, sizeof(path)))
				SetAutoSaveDir(path);
			break;
		}
		case AUTOSAVE_DIR_PRJ_MSG:
		{
			char prjPath[SNM_MAX_PATH] = "", path[SNM_MAX_PATH] = "";
			GetProjectPath(prjPath, sizeof(prjPath));

			// see GetFileRelativePath..
			if (snprintfStrict(path, sizeof(path), "%s%c%s", prjPath, PATH_SLASH_CHAR, GetFileRelativePath(fl->GetResourceDir())) > 0)
			{
				if (!FileOrDirExists(path))
					CreateDirectory(path, NULL);
				SetAutoSaveDir(path);
			}
			break;
		}
		case AUTOSAVE_DIR_PRJ2_MSG:
		{
			char prjPath[SNM_MAX_PATH] = "";
			GetProjectPath(prjPath, sizeof(prjPath));
			SetAutoSaveDir(prjPath);
			break;
		}
		case AUTOSAVE_DIR_DEFAULT_MSG:
		{
			char path[SNM_MAX_PATH] = "";
			if (snprintfStrict(path, sizeof(path), "%s%c%s", GetResourcePath(), PATH_SLASH_CHAR, fl->GetResourceDir()) > 0) {
				if (!FileOrDirExists(path))
					CreateDirectory(path, NULL);
				SetAutoSaveDir(path);
			}
			break;
		}
		case AUTOSAVE_SYNC_MSG: // i.e. sync w/ auto-fill
			g_syncAutoDirPrefs[g_resType] = !g_syncAutoDirPrefs[g_resType];
			if (g_syncAutoDirPrefs[g_resType])
				g_autoSaveDirs.Get(g_resType)->Set(GetAutoFillDir()); // do not use SetAutoSaveDir() here!
			break;
		// text filter mode
		case FILTER_BY_NAME_MSG:
			if (g_filterPref&1) g_filterPref &= 6; // 110
			else g_filterPref |= 1;
			Update();
//			SetFocus(GetDlgItem(m_hwnd, IDC_FILTER));
			break;
		case FILTER_BY_PATH_MSG:
			if (g_filterPref&2) g_filterPref &= 5; // 101
			else g_filterPref |= 2;
			Update();
//			SetFocus(GetDlgItem(m_hwnd, IDC_FILTER));
			break;
		case FILTER_BY_COMMENT_MSG:
			if (g_filterPref&4) g_filterPref &= 3; // 011
			else g_filterPref |= 4;
			Update();
//			SetFocus(GetDlgItem(m_hwnd, IDC_FILTER));
			break;
		case RENAME_MSG:
			if (item)
			{
				char fullPath[SNM_MAX_PATH] = "";
				if (fl->GetFullPath(slot, fullPath, sizeof(fullPath)) && FileOrDirExistsErrMsg(fullPath))
					GetListView()->EditListItem((SWS_ListItem*)item, COL_NAME);
			}
			break;
		case REN_BOOKMARK_MSG:
			RenameBookmark(g_resType);
			break;
		case BTNID_ADD_BOOKMARK:
			NewBookmark(g_resType, false);
			break;
		case COPY_BOOKMARK_MSG:
			NewBookmark(g_resType, true);
			break;
		case BTNID_DEL_BOOKMARK:
		case DEL_BOOKMARK_MSG:
			DeleteBookmark(g_resType);
			break;

		// new bookmark cmds are in an interval of cmds: see "default" case below..

		case LOAD_TIED_PRJ_MSG:
		case LOAD_TIED_PRJ_TAB_MSG:
			if (g_tiedProjects.Get(g_resType)->GetLength())
				LoadOrSelectProject(g_tiedProjects.Get(g_resType)->Get(), LOWORD(wParam)==LOAD_TIED_PRJ_TAB_MSG);
			break;
		case TIE_ACTIONS_MSG:
			g_tiedSlotActions[GetTypeForUser()] = g_resType;
			FillTypeCombo(); // update [x] tag
			Update();
			break;
		case UNTIE_PROJECT_MSG:
		{
			char path[SNM_MAX_PATH]="";
			ReaProject* prj = EnumProjects(-1, path, sizeof(path));
			UntieAllResFileFromProject(prj, path, g_resType);
			g_tiedProjects.Get(g_resType)->Set("");
			m_parentVwnd.RequestRedraw(NULL);
			break;
		}
		case TIE_PROJECT_MSG:
		{
			char path[SNM_MAX_PATH]="";
			ReaProject* prj = EnumProjects(-1, path, sizeof(path));
			UntieAllResFileFromProject(prj, path, g_resType);
			g_tiedProjects.Get(g_resType)->Set(path);
			TieAllResFileToProject(prj, path, g_resType);
			m_parentVwnd.RequestRedraw(NULL);
			break;
		}

		// ***** FX chain *****
		case FXC_AUTOSAVE_INPUTFX_MSG:
			g_asFXChainPref = FXC_AUTOSAVE_PREF_INPUT_FX;
			break;
		case FXC_AUTOSAVE_TR_MSG:
			g_asFXChainPref = FXC_AUTOSAVE_PREF_TRACK;
			break;
		case FXC_AUTOSAVE_ITEM_MSG:
			g_asFXChainPref = FXC_AUTOSAVE_PREF_ITEM;
			break;
		case FXC_AUTOSAVE_DEFNAME_MSG:
		case FXC_AUTOSAVE_FX1NAME_MSG:
			g_asFXChainNamePref = g_asFXChainNamePref ? 0:1;
			break;

		// ***** Track template *****
		case BTNID_OFFSET_TR_TEMPLATE:
			if (ConfigVar<int> offsPref = "templateditcursor") { // >= REAPER v4.15
				if (*offsPref) *offsPref = 0;
				else *offsPref = 1;
			}
			break;
		case TRT_AUTOSAVE_WITEMS_MSG:
			if (g_asTrTmpltPref & 1) g_asTrTmpltPref &= 0xE;
			else g_asTrTmpltPref |= 1;
			break;
		case TRT_AUTOSAVE_WENVS_MSG:
			if (g_asTrTmpltPref & 2) g_asTrTmpltPref &= 0xD;
			else g_asTrTmpltPref |= 2;
			break;

		// ***** Project template *****
		case PRJ_AUTOFILL_RECENTS_MSG:
		{
			int startSlot = fl->GetSize();
			int nbRecents = GetPrivateProfileInt("REAPER", "numrecent", 0, get_ini_file());
			nbRecents = min(98, nbRecents); // just in case: 2 digits max..
			if (nbRecents)
			{
				char key[16], path[SNM_MAX_PATH];
				WDL_PtrList_DeleteOnDestroy<WDL_FastString> prjs;
				for (int i=0; i < nbRecents; i++) {
					if (snprintfStrict(key, sizeof(key), "recent%02d", i+1) > 0) {
						GetPrivateProfileString("Recent", key, "", path, sizeof(path), get_ini_file());
						if (*path)
							prjs.Add(new WDL_FastString(path));
					}
				}
				for (int i=0; i < prjs.GetSize(); i++)
					if (fl->FindByPath(prjs.Get(i)->Get()) < 0) { // skip if already present
						fl->AddSlot(prjs.Get(i)->Get());
						TieResFileToProject(prjs.Get(i)->Get(), g_resType);
					}
			}
			if (startSlot != fl->GetSize()) {
				Update();
				SelectBySlot(startSlot, fl->GetSize());
			}
			else 
			{
				char msg[SNM_MAX_PATH] = "";
				snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("No new recent projects found!\n%s","sws_DLG_150"), AUTOFILL_ERR_STR);
				MessageBox(m_hwnd, msg, __LOCALIZE("S&M - Warning","sws_DLG_150"), MB_OK);
			}
			break;
		}

			// ***** WDL GUI & others *****
		case CMBID_TYPE:
			if (HIWORD(wParam)==CBN_SELCHANGE)
			{
				// stop cell editing (changing the list content would be ignored otherwise,
				// leading to unsynchronized dropdown box vs list view)
				GetListView()->EditListItemEnd(false);
				SetType(m_cbType.GetCurSel2());
			}
			break;
		case CMBID_DBLCLICK_TYPE:
			if (HIWORD(wParam)==CBN_SELCHANGE) {
				g_dblClickPrefs[g_resType] = m_cbDblClickType.GetCurSel2();
				m_parentVwnd.RequestRedraw(NULL); // some options can be hidden for new sel
			}
			break;

		default:
			// fx chain commands
			if (LOWORD(wParam) >= FXC_START_MSG && LOWORD(wParam) <= FXC_END_MSG)
				((ResourcesView*)GetListView())->Perform(LOWORD(wParam)-FXC_START_MSG);

			// track template commands
			else if (LOWORD(wParam) >= TRT_START_MSG && LOWORD(wParam) <= TRT_END_MSG)
				((ResourcesView*)GetListView())->Perform(LOWORD(wParam)-TRT_START_MSG);

			// project commands
			else if (LOWORD(wParam) >= PRJ_START_MSG && LOWORD(wParam) <= PRJ_END_MSG)
				((ResourcesView*)GetListView())->Perform(LOWORD(wParam)-PRJ_START_MSG);

			// media file commands/options
			else if (LOWORD(wParam) >= MED_START_MSG && LOWORD(wParam) <= MED_END_MSG)
				((ResourcesView*)GetListView())->Perform(LOWORD(wParam)-MED_START_MSG);
			else if (LOWORD(wParam) >= MED_OPT_START_MSG && LOWORD(wParam) <= MED_OPT_END_MSG)
				SetMediaOption(LOWORD(wParam)-MED_OPT_START_MSG);
			
			// image commands
			else if (LOWORD(wParam) >= IMG_START_MSG && LOWORD(wParam) <= IMG_END_MSG)
				((ResourcesView*)GetListView())->Perform(LOWORD(wParam)-IMG_START_MSG);

			// theme commands
			else if (LOWORD(wParam) >= THM_START_MSG && LOWORD(wParam) <= THM_END_MSG)
				((ResourcesView*)GetListView())->Perform(LOWORD(wParam)-THM_START_MSG);

			// new bookmark
			else if (LOWORD(wParam) >= NEW_BOOKMARK_START_MSG && LOWORD(wParam) < (NEW_BOOKMARK_START_MSG+SNM_NUM_DEFAULT_SLOTS))
				NewBookmark(LOWORD(wParam)-NEW_BOOKMARK_START_MSG, false);

			// new custom bookmark
			else if (LOWORD(wParam) == (NEW_BOOKMARK_START_MSG+SNM_NUM_DEFAULT_SLOTS))
				NewBookmark(-1, false);

			else
				Main_OnCommand((int)wParam, (int)lParam);
			break;
	}
}

HMENU ResourcesWnd::AutoSaveContextMenu(HMENU _menu, bool _saveItem)
{
	int typeForUser = GetTypeForUser();
	char autoPath[SNM_MAX_PATH] = "";
	snprintf(autoPath, sizeof(autoPath), __LOCALIZE_VERFMT("[Current auto-save path: %s]","sws_DLG_150"), *GetAutoSaveDir() ? GetAutoSaveDir() : __LOCALIZE("undefined","sws_DLG_150"));
	AddToMenu(_menu, autoPath, 0, -1, false, MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Show auto-save path in explorer/finder...","sws_DLG_150"), EXPLORE_SAVEDIR_MSG, -1, false, *GetAutoSaveDir() ? MF_ENABLED : MF_GRAYED);

	AddToMenu(_menu, __LOCALIZE("Sync auto-save and auto-fill paths","sws_DLG_150"), AUTOFILL_SYNC_MSG, -1, false, g_syncAutoDirPrefs[g_resType] ? MFS_CHECKED : MFS_UNCHECKED);

	if (_saveItem) {
		AddToMenu(_menu, SWS_SEPARATOR, 0);
		AddToMenu(_menu, __LOCALIZE("Auto-save","sws_DLG_150"), AUTOSAVE_MSG, -1, false, !IsFiltered() ? MF_ENABLED : MF_GRAYED);
	}

	AddToMenu(_menu, SWS_SEPARATOR, 0);
	AddToMenu(_menu, __LOCALIZE("Set auto-save directory...","sws_DLG_150"), AUTOSAVE_DIR_MSG);
	AddToMenu(_menu, __LOCALIZE("Set auto-save directory to default resource path","sws_DLG_150"), AUTOSAVE_DIR_DEFAULT_MSG);
	AddToMenu(_menu, __LOCALIZE("Set auto-save directory to project path","sws_DLG_150"), AUTOSAVE_DIR_PRJ2_MSG);
	switch(typeForUser)
	{
		case SNM_SLOT_FXC:
			AddToMenu(_menu, __LOCALIZE("Set auto-save directory to project path (/FXChains)","sws_DLG_150"), AUTOSAVE_DIR_PRJ_MSG);
			AddToMenu(_menu, SWS_SEPARATOR, 0);
			AddToMenu(_menu, __LOCALIZE("Auto-save FX chains from track selection","sws_DLG_150"), FXC_AUTOSAVE_TR_MSG, -1, false, g_asFXChainPref == FXC_AUTOSAVE_PREF_TRACK ? MFS_CHECKED : MFS_UNCHECKED);
			AddToMenu(_menu, __LOCALIZE("Auto-save FX chains from item selection","sws_DLG_150"), FXC_AUTOSAVE_ITEM_MSG, -1, false, g_asFXChainPref == FXC_AUTOSAVE_PREF_ITEM ? MFS_CHECKED : MFS_UNCHECKED);
			AddToMenu(_menu, __LOCALIZE("Auto-save input FX chains from track selection","sws_DLG_150"), FXC_AUTOSAVE_INPUTFX_MSG, -1, false, g_asFXChainPref == FXC_AUTOSAVE_PREF_INPUT_FX ? MFS_CHECKED : MFS_UNCHECKED);
			AddToMenu(_menu, SWS_SEPARATOR, 0);
			AddToMenu(_menu, __LOCALIZE("Generate filename from track/item name","sws_DLG_150"), FXC_AUTOSAVE_DEFNAME_MSG, -1, false, !g_asFXChainNamePref ? MFS_CHECKED : MFS_UNCHECKED);
			AddToMenu(_menu, __LOCALIZE("Generate filename from first FX name","sws_DLG_150"), FXC_AUTOSAVE_FX1NAME_MSG, -1, false, g_asFXChainNamePref ? MFS_CHECKED : MFS_UNCHECKED);
			break;
		case SNM_SLOT_TR:
			AddToMenu(_menu, __LOCALIZE("Set auto-save directory to project path (/TrackTemplates)","sws_DLG_150"), AUTOSAVE_DIR_PRJ_MSG);
			AddToMenu(_menu, SWS_SEPARATOR, 0);
			AddToMenu(_menu, __LOCALIZE("Include track items in templates","sws_DLG_150"), TRT_AUTOSAVE_WITEMS_MSG, -1, false, (g_asTrTmpltPref&1) ? MFS_CHECKED : MFS_UNCHECKED);
			AddToMenu(_menu, __LOCALIZE("Include envelopes in templates","sws_DLG_150"), TRT_AUTOSAVE_WENVS_MSG, -1, false, (g_asTrTmpltPref&2) ? MFS_CHECKED : MFS_UNCHECKED);
			break;
		case SNM_SLOT_PRJ:
			AddToMenu(_menu, __LOCALIZE("Set auto-save directory to project path (/ProjectTemplates)","sws_DLG_150"), AUTOSAVE_DIR_PRJ_MSG);
			break;
	}
	return _menu;
}

HMENU ResourcesWnd::AutoFillContextMenu(HMENU _menu, bool _fillItem)
{
	int typeForUser = GetTypeForUser();
	char autoPath[SNM_MAX_PATH] = "";
	snprintf(autoPath, sizeof(autoPath), __LOCALIZE_VERFMT("[Current auto-fill path: %s]","sws_DLG_150"), *GetAutoFillDir() ? GetAutoFillDir() : __LOCALIZE("undefined","sws_DLG_150"));
	AddToMenu(_menu, autoPath, 0, -1, false, MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Show auto-fill path in explorer/finder...","sws_DLG_150"), EXPLORE_FILLDIR_MSG, -1, false, *GetAutoFillDir() ? MF_ENABLED : MF_GRAYED);

	if (g_SNM_ResSlots.Get(g_resType)->IsAutoSave())
		AddToMenu(_menu, __LOCALIZE("Sync auto-save and auto-fill paths","sws_DLG_150"), AUTOSAVE_SYNC_MSG, -1, false, g_syncAutoDirPrefs[g_resType] ? MFS_CHECKED : MFS_UNCHECKED);

	if (_fillItem) {
		AddToMenu(_menu, SWS_SEPARATOR, 0);
		AddToMenu(_menu, __LOCALIZE("Auto-fill","sws_DLG_150"), AUTOFILL_MSG, -1, false, !IsFiltered() ? MF_ENABLED : MF_GRAYED);
	}
	if (typeForUser == SNM_SLOT_PRJ) {
		if (!_fillItem) AddToMenu(_menu, SWS_SEPARATOR, 0);
		AddToMenu(_menu, __LOCALIZE("Auto-fill with recent projects","sws_DLG_150"), PRJ_AUTOFILL_RECENTS_MSG, -1, false, !IsFiltered() ? MF_ENABLED : MF_GRAYED);
	}
	AddToMenu(_menu, SWS_SEPARATOR, 0);
	AddToMenu(_menu, __LOCALIZE("Set auto-fill directory...","sws_DLG_150"), AUTOFILL_DIR_MSG, -1, false, !IsFiltered() ? MF_ENABLED : MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Set auto-fill directory to default resource path","sws_DLG_150"), AUTOFILL_DEFAULT_MSG, -1, false, !IsFiltered() ? MF_ENABLED : MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Set auto-fill directory to project path","sws_DLG_150"), AUTOFILL_PRJ_MSG, -1, false, !IsFiltered() ? MF_ENABLED : MF_GRAYED);
	return _menu;
}

HMENU ResourcesWnd::AttachPrjContextMenu(HMENU _menu, bool _openSelPrj)
{
	// bookmark, custom type?
	if (g_resType>=SNM_NUM_DEFAULT_SLOTS) //no! && g_tiedProjects.Get(g_resType)->GetLength())
	{
		char buf[128] = "";
		bool curPrjTied = (g_tiedProjects.Get(g_resType)->GetLength() && !_stricmp(g_tiedProjects.Get(g_resType)->Get(), g_curProjectFn));

		// tied prj but not open?
		if (_openSelPrj && g_tiedProjects.Get(g_resType)->GetLength() && !curPrjTied)
		{
			if (GetMenuItemCount(_menu))
				AddToMenu(_menu, SWS_SEPARATOR, 0);
			snprintf(buf, sizeof(buf), TIED_PRJ_SELECT_LOAD_STR, GetFileRelativePath(g_tiedProjects.Get(g_resType)->Get()));
			AddToMenu(_menu, buf, LOAD_TIED_PRJ_MSG);
			snprintf(buf, sizeof(buf), TIED_PRJ_SELECT_LOADTAB_STR, GetFileRelativePath(g_tiedProjects.Get(g_resType)->Get()));
			AddToMenu(_menu, buf, LOAD_TIED_PRJ_TAB_MSG);
		}

		if (GetMenuItemCount(_menu))
			AddToMenu(_menu, SWS_SEPARATOR, 0);

		// make sure g_curProjectFn is up to date
		AttachResourceFiles();

		snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("Attach bookmark files to %s","sws_DLG_150"), *g_curProjectFn ? GetFileRelativePath(g_curProjectFn) : __LOCALIZE("(unsaved project?)","sws_DLG_150"));
		AddToMenu(_menu, buf, TIE_PROJECT_MSG, -1, false, *g_curProjectFn && !curPrjTied ? MF_ENABLED : MF_GRAYED);

		snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("Detach bookmark files from %s","sws_DLG_150"), GetFileRelativePath(g_tiedProjects.Get(g_resType)->Get()));
		AddToMenu(_menu, g_tiedProjects.Get(g_resType)->GetLength() ? buf : __LOCALIZE("Detach bookmark files","sws_DLG_150"), UNTIE_PROJECT_MSG, -1, false, g_tiedProjects.Get(g_resType)->GetLength() ? MF_ENABLED : MF_GRAYED);
	}
	return _menu;
}

HMENU ResourcesWnd::BookmarkContextMenu(HMENU _menu)
{
	int typeForUser = GetTypeForUser();

	HMENU hNewBookmarkSubMenu = CreatePopupMenu();
	AddSubMenu(_menu, hNewBookmarkSubMenu, __LOCALIZE("New bookmark","sws_DLG_150"));
	for (int i=0; i < SNM_NUM_DEFAULT_SLOTS; i++)
		if (char* p = _strdup(g_SNM_ResSlots.Get(i)->GetName())) {
			*p = toupper(*p); // 1st char to upper
			AddToMenu(hNewBookmarkSubMenu, p, NEW_BOOKMARK_START_MSG+i);
			free(p);
		}
	AddToMenu(hNewBookmarkSubMenu, __LOCALIZE("Custom...","sws_DLG_150"), NEW_BOOKMARK_START_MSG+SNM_NUM_DEFAULT_SLOTS);

	AddToMenu(_menu, __LOCALIZE("Copy bookmark...","sws_DLG_150"), COPY_BOOKMARK_MSG);
	AddToMenu(_menu, __LOCALIZE("Rename bookmark...","sws_DLG_150"), REN_BOOKMARK_MSG, -1, false, g_resType >= SNM_NUM_DEFAULT_SLOTS ? MF_ENABLED : MF_GRAYED);
	AddToMenu(_menu, __LOCALIZE("Delete bookmark","sws_DLG_150"), DEL_BOOKMARK_MSG, -1, false, g_resType >= SNM_NUM_DEFAULT_SLOTS ? MF_ENABLED : MF_GRAYED);

	AttachPrjContextMenu(_menu, false);

	if (IsMultiType())
	{
		char buf[128] = "";
		AddToMenu(_menu, SWS_SEPARATOR, 0);
		snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("Attach %s slot actions to this bookmark","sws_DLG_150"), g_SNM_ResSlots.Get(typeForUser)->GetName());
		AddToMenu(_menu, buf, TIE_ACTIONS_MSG, -1, false, g_tiedSlotActions[typeForUser] == g_resType ? MFS_CHECKED : MFS_UNCHECKED);
	}
	return _menu;
}

HMENU ResourcesWnd::AddMediaOptionsContextMenu(HMENU _menu)
{
	int msg = MED_OPT_START_MSG;
	HMENU hAddMediaOptSubMenu = CreatePopupMenu();
	AddSubMenu(_menu, hAddMediaOptSubMenu,__LOCALIZE("Add media file options","sws_DLG_150"));
	AddToMenu(hAddMediaOptSubMenu, __LOCALIZE("Default","sws_DLG_150"), msg++, -1, false, IsMediaOption(0) ? MFS_CHECKED : MFS_UNCHECKED);
	AddToMenu(hAddMediaOptSubMenu, __LOCALIZE("Stretch/loop to fit time selection","sws_DLG_150"), msg++, -1, false, IsMediaOption(1) ? MFS_CHECKED : MFS_UNCHECKED);
	AddToMenu(hAddMediaOptSubMenu, __LOCALIZE("Try to match tempo 0.5x","sws_DLG_150"), msg++, -1, false, IsMediaOption(2) ? MFS_CHECKED : MFS_UNCHECKED);
	AddToMenu(hAddMediaOptSubMenu, __LOCALIZE("Try to match tempo 1x","sws_DLG_150"), msg++, -1, false, IsMediaOption(3) ? MFS_CHECKED : MFS_UNCHECKED);
	AddToMenu(hAddMediaOptSubMenu, __LOCALIZE("Try to match tempo 2x","sws_DLG_150"), msg++, -1, false, IsMediaOption(4) ? MFS_CHECKED : MFS_UNCHECKED);
	return _menu;
}

HMENU ResourcesWnd::OnContextMenu(int _x, int _y, bool* _wantDefaultItems)
{
	ResourceList* fl = g_SNM_ResSlots.Get(g_resType);
	if (!fl)
		return NULL;

	HMENU hMenu = CreatePopupMenu();

	// specific context menu for auto-fill/auto-save buttons
	POINT p; GetCursorPos(&p);
	ScreenToClient(m_hwnd,&p);
	if (WDL_VWnd* v = m_parentVwnd.VirtWndFromPoint(p.x,p.y,1))
	{
		switch (v->GetID())
		{
			case BTNID_AUTOFILL:
				*_wantDefaultItems = false;
				return AutoFillContextMenu(hMenu, false);
			case BTNID_AUTOSAVE:
				if (fl->IsAutoSave()) {
					*_wantDefaultItems = false;
					return AutoSaveContextMenu(hMenu, false);
				}
				break;
			case CMBID_TYPE:
				*_wantDefaultItems = false;
				return BookmarkContextMenu(hMenu);
			case TXTID_TIED_PRJ:
				if (g_tiedProjects.Get(g_resType)->GetLength())
				{
					*_wantDefaultItems = false;
					AttachPrjContextMenu(hMenu, true);
					return hMenu;
				}
				break;
		}
	}

	// general context menu
	int iCol, typeForUser = GetTypeForUser();
	ResourceItem* pItem = (ResourceItem*)GetListView()->GetHitItem(_x, _y, &iCol);
/*JFB no! dumb issue vs multi-selection
	UINT enabled = (pItem && !pItem->IsDefault()) ? MF_ENABLED : MF_GRAYED;
*/
	UINT enabled = pItem ? MF_ENABLED : MF_GRAYED;
	if (pItem && iCol >= 0)
	{
		int msg;
		*_wantDefaultItems = false;
		switch(typeForUser)
		{
			case SNM_SLOT_FXC:
				msg = FXC_START_MSG;
				AddToMenu(hMenu, FXC_PASTE_TR_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, FXCIN_PASTE_TR_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, FXC_APPLY_TR_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, FXCIN_APPLY_TR_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				AddToMenu(hMenu, FXC_PASTE_TAKE_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, FXC_PASTE_ALLTAKES_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, FXC_APPLY_TAKE_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, FXC_APPLY_ALLTAKES_STR, msg++, -1, false, enabled);
				break;
			case SNM_SLOT_TR:
				msg = TRT_START_MSG;
				AddToMenu(hMenu, TRT_IMPORT_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, TRT_APPLY_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, TRT_APPLY_WITH_ENV_ITEM_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				AddToMenu(hMenu, TRT_PASTE_ITEMS_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, TRT_REPLACE_ITEMS_STR, msg++, -1, false, enabled);
				break;
			case SNM_SLOT_PRJ:
			{
				msg = PRJ_START_MSG;
				AddToMenu(hMenu, PRJ_SELECT_LOAD_STR,  msg++, -1, false, enabled);
				AddToMenu(hMenu, PRJ_SELECT_LOAD_TAB_STR,  msg++, -1, false, enabled);
				break;
			}
			case SNM_SLOT_MEDIA:
			{
				msg = MED_START_MSG;
				AddToMenu(hMenu, MED_PLAY_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, MED_LOOP_STR, msg++, -1, false, enabled);

				AddToMenu(hMenu, SWS_SEPARATOR, 0);

				WDL_FastString name;
				name.Set(MED_ADD_TR_STR); AddMediaOptionName(&name);
				AddToMenu(hMenu, name.Get(), msg++, -1, false, enabled);
				name.Set(MED_ADD_NEWTR_STR); AddMediaOptionName(&name);
				AddToMenu(hMenu, name.Get(), msg++, -1, false, enabled);
				name.Set(MED_ADD_ITEM_STR); AddMediaOptionName(&name);
				AddToMenu(hMenu, name.Get(), msg++, -1, false, enabled);

				AddMediaOptionsContextMenu(hMenu);
				break;
			}
			case SNM_SLOT_IMG:
				msg = IMG_START_MSG;
				AddToMenu(hMenu, IMG_SHOW_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, SWS_SEPARATOR, 0);
				AddToMenu(hMenu, IMG_TRICON_STR, msg++, -1, false, enabled);
				AddToMenu(hMenu, MED_ADD_TR_STR, msg++, -1, false, enabled);
				break;
			case SNM_SLOT_THM:
				msg = THM_START_MSG;
				AddToMenu(hMenu, THM_LOAD_STR, msg++, -1, false, enabled);
				break;
		}
	}

	if (GetMenuItemCount(hMenu))
		AddToMenu(hMenu, SWS_SEPARATOR, 0);

	// always displayed, even if the list is empty
	AddToMenu(hMenu, __LOCALIZE("Add slot","sws_DLG_150"), ADD_SLOT_MSG, -1, false, !IsFiltered() ? MF_ENABLED : MF_GRAYED);

	if (pItem && iCol >= 0)
	{
		AddToMenu(hMenu, __LOCALIZE("Insert slot","sws_DLG_150"), INSERT_SLOT_MSG, -1, false, !IsFiltered() ? MF_ENABLED : MF_GRAYED);
		AddToMenu(hMenu, __LOCALIZE("Clear slots","sws_DLG_150"), CLR_SLOTS_MSG, -1, false, enabled);
		AddToMenu(hMenu, __LOCALIZE("Delete slots","sws_DLG_150"), DEL_SLOTS_MSG);

		AddToMenu(hMenu, SWS_SEPARATOR, 0);
		AddToMenu(hMenu, __LOCALIZE("Load slot/file...","sws_DLG_150"), LOAD_MSG);
		AddToMenu(hMenu, __LOCALIZE("Delete files","sws_DLG_150"), DEL_FILES_MSG, -1, false, enabled);
		AddToMenu(hMenu, __LOCALIZE("Rename file","sws_DLG_150"), RENAME_MSG, -1, false, enabled);
		if (fl->IsText())
		{
#ifdef _WIN32
			int x=0, nbsel=0; while(GetListView()->EnumSelected(&x)) nbsel++;
			AddToMenu(hMenu, __LOCALIZE("Diff files...","sws_DLG_150"), DIFF_MSG, -1, false, nbsel==2 && enabled==MF_ENABLED && g_SNM_DiffToolFn.GetLength() ? MF_ENABLED:MF_GRAYED);
#endif
			AddToMenu(hMenu, __LOCALIZE("Edit file...","sws_DLG_150"), EDIT_MSG, -1, false, enabled);
		}	
		AddToMenu(hMenu, __LOCALIZE("Show path in explorer/finder...","sws_DLG_150"), EXPLORE_MSG, -1, false, enabled);
	}
	else
	{
		// auto-fill
		AddToMenu(hMenu, SWS_SEPARATOR, 0);
		HMENU hAutoFillSubMenu = CreatePopupMenu();
		AddSubMenu(hMenu, hAutoFillSubMenu, __LOCALIZE("Auto-fill","sws_DLG_150"));
		AutoFillContextMenu(hAutoFillSubMenu, true);

		// auto-save
		if (fl->IsAutoSave())
		{
			HMENU hAutoSaveSubMenu = CreatePopupMenu();
			AddSubMenu(hMenu, hAutoSaveSubMenu, __LOCALIZE("Auto-save","sws_DLG_150"));
			AutoSaveContextMenu(hAutoSaveSubMenu, true);
		}

		// bookmarks
		AddToMenu(hMenu, SWS_SEPARATOR, 0);
		HMENU hBookmarkSubMenu = CreatePopupMenu();
		AddSubMenu(hMenu, hBookmarkSubMenu, __LOCALIZE("Bookmark","sws_DLG_150"));
		BookmarkContextMenu(hBookmarkSubMenu);

		// Add media file options
		if (typeForUser == SNM_SLOT_MEDIA) {
			AddToMenu(hMenu, SWS_SEPARATOR, 0);
			AddMediaOptionsContextMenu(hMenu);
		}

		// filter prefs
		AddToMenu(hMenu, SWS_SEPARATOR, 0);
		HMENU hFilterSubMenu = CreatePopupMenu();
		AddSubMenu(hMenu, hFilterSubMenu, __LOCALIZE("Filter on","sws_DLG_150"));
		AddToMenu(hFilterSubMenu, __LOCALIZE("Name","sws_DLG_150"), FILTER_BY_NAME_MSG, -1, false, (g_filterPref&1) ? MFS_CHECKED : MFS_UNCHECKED);
		AddToMenu(hFilterSubMenu, __LOCALIZE("Path","sws_DLG_150"), FILTER_BY_PATH_MSG, -1, false, (g_filterPref&2) ? MFS_CHECKED : MFS_UNCHECKED);
		AddToMenu(hFilterSubMenu, __LOCALIZE("Comment","sws_DLG_150"), FILTER_BY_COMMENT_MSG, -1, false, (g_filterPref&4) ? MFS_CHECKED : MFS_UNCHECKED);
	}
	return hMenu;
}

HWND ResourcesWnd::IDC_FILTER_GetFocus(HWND _hwnd, MSG* _msg)
{
	HWND h = GetDlgItem(_hwnd, IDC_FILTER);
#ifdef _WIN32
	if (_msg->hwnd == h)
#else
	if (GetFocus() == h)
#endif
		return h;
	else
		return NULL;
}

int ResourcesWnd::OnKey(MSG* _msg, int _iKeyState) 
{
	if (_msg->message == WM_KEYDOWN)
	{
		if (!_iKeyState)
		{
			switch(_msg->wParam)
			{
				case VK_F2:
					OnCommand(RENAME_MSG, 0);
					return 1;
				case VK_DELETE:
				{
					// #1119, don't block Del key in Filter
					if (IDC_FILTER_GetFocus(m_hwnd, _msg))
						return 0;
					else
					{
						ClearDeleteSlotsFiles(g_resType, 1);
						return 1;
					}
				}
				case VK_INSERT:
					InsertAtSelectedSlot();
					return 1;
				case VK_RETURN:
				{
					// #1119, Enter key in Filter -> Focus list
					if (IDC_FILTER_GetFocus(m_hwnd, _msg))
					{
						(SetFocus(GetListView()->GetHWND()));
						return 1;
					}
					else
					{
						((ResourcesView*)GetListView())->Perform(g_dblClickPrefs[g_resType]);
						return 1;
					}
				}

			}
		}
		// ctrl+A => select all
		else if (_iKeyState == LVKF_CONTROL && _msg->wParam == 'A')
		{
			if (HWND h = IDC_FILTER_GetFocus(m_hwnd, _msg))
			{
				SetFocus(h);
				SendMessage(h, EM_SETSEL, 0, -1);
				return 1; // eat
			}
		}
	}
	return 0;
}

int ResourcesWnd::GetValidDroppedFilesCount(HDROP _h)
{
	int validCnt=0;
	int iFiles = DragQueryFile(_h, 0xFFFFFFFF, NULL, 0);
	char fn[SNM_MAX_PATH] = "";
	for (int i=0; i < iFiles; i++) 
	{
		DragQueryFile(_h, i, fn, sizeof(fn));
		if (!strcmp(fn, DRAGDROP_EMPTY_SLOT) || g_SNM_ResSlots.Get(GetTypeForUser())->IsValidFileExt(GetFileExtension(fn)))
			validCnt++;
	}
	return validCnt;
}

void ResourcesWnd::OnDroppedFiles(HDROP _h)
{
	ResourceList* fl = g_SNM_ResSlots.Get(g_resType);
	if (!fl)
		return;

	int dropped = 0; //nb of successfully dropped files
	int iFiles = DragQueryFile(_h, 0xFFFFFFFF, NULL, 0);
	int iValidFiles = GetValidDroppedFilesCount(_h);

	POINT pt;
	DragQueryPoint(_h, &pt);

	RECT r; // ClientToScreen doesn't work right, wtf?
	GetWindowRect(m_hwnd, &r);
	pt.x += r.left;
	pt.y += r.top;

	ResourceItem* pItem = (ResourceItem*)GetListView()->GetHitItem(pt.x, pt.y, NULL);
	int dropSlot = fl->Find(pItem);

	// internal drag-drop?
	if (g_dragResourceItems.GetSize())
	{
		int srcSlot = fl->Find(g_dragResourceItems.Get(0));
		// drag-drop slot to the bottom
		if (srcSlot >= 0 && srcSlot < dropSlot)
			dropSlot++; // drag-drop will be more "natural"
	}

	// drop but not on a slot => create slots
	if (!pItem || dropSlot < 0 || dropSlot >= fl->GetSize()) 
	{
		dropSlot = fl->GetSize();
		for (int i=0; i < iValidFiles; i++)
			fl->Add(new ResourceItem());
	}
	// drop on a slot => insert slots at drop point
	else 
	{
		for (int i=0; i < iValidFiles; i++)
			fl->InsertSlot(dropSlot);
	}

	// re-sync pItem 
	pItem = fl->Get(dropSlot); 

	// patch slots from drop data
	char fn[SNM_MAX_PATH] = "";
	int slot;
	for (int i=0; pItem && i < iFiles; i++)
	{
		slot = fl->Find(pItem);
		DragQueryFile(_h, i, fn, sizeof(fn));

		// internal drag-drop?
		if (g_dragResourceItems.GetSize())
		{
			if (ResourceItem* item = fl->Get(slot))
			{
				item->m_shortPath.Set(g_dragResourceItems.Get(i)->m_shortPath.Get());
				item->m_comment.Set(g_dragResourceItems.Get(i)->m_comment.Get());
				dropped++;
				pItem = fl->Get(slot+1); 
			}
		}
		// .rfxchain? .rTrackTemplate? etc..
		else if (g_SNM_ResSlots.Get(GetTypeForUser())->IsValidFileExt(GetFileExtension(fn)))
		{
			if (fl->SetFromFullPath(slot, fn))
			{
				dropped++;
				pItem = fl->Get(slot+1);
				TieResFileToProject(fn, g_resType);
			}
		}
	}

	if (dropped)
	{
		// internal drag-drop: move (=> delete previous slots)
		for (int i=0; i < g_dragResourceItems.GetSize(); i++)
			for (int j=fl->GetSize()-1; j >= 0; j--)
				if (fl->Get(j) == g_dragResourceItems.Get(i))
				{
					if (j < dropSlot)
						dropSlot--;
					fl->Delete(j, false);
				}

		Update();

		// Select item at drop point
		if (dropSlot >= 0)
			SelectBySlot(dropSlot);
	}

	g_dragResourceItems.Empty(false);
	DragFinish(_h);
}

void ResourcesWnd::DrawControls(LICE_IBitmap* _bm, const RECT* _r, int* _tooltipHeight)
{
	ResourceList* fl = g_SNM_ResSlots.Get(g_resType);
	if (!fl)
		return;

	// 1st row of controls (top)

	int x0=_r->left+SNM_GUI_X_MARGIN, h=SNM_GUI_TOP_H;
	if (_tooltipHeight)
		*_tooltipHeight = h;

	IconTheme* it = SNM_GetIconTheme();

	// "auto-fill" button
	SNM_SkinButton(&m_btnAutoFill, it ? &(it->toolbar_open) : NULL, __LOCALIZE("Auto-fill","sws_DLG_150"));
	if (SNM_AutoVWndPosition(DT_LEFT, &m_btnAutoFill, NULL, _r, &x0, _r->top, h, 0))
	{
		// "auto-save" button
		m_btnAutoSave.SetGrayed(!fl->IsAutoSave());
		SNM_SkinButton(&m_btnAutoSave, it ? &(it->toolbar_save) : NULL, __LOCALIZE("Auto-save","sws_DLG_150"));
		if (SNM_AutoVWndPosition(DT_LEFT, &m_btnAutoSave, NULL, _r, &x0, _r->top, h))
		{
			// type dropdown
			if (SNM_AutoVWndPosition(DT_LEFT, &m_cbType, NULL, _r, &x0, _r->top, h, 4))
			{
				// add/del bookmark buttons
				m_btnDel.SetEnabled(g_resType >= SNM_NUM_DEFAULT_SLOTS);
				if (SNM_AutoVWndPosition(DT_LEFT, &m_btnsAddDel, NULL, _r, &x0, _r->top, h))
				{
					if (g_resType>=SNM_NUM_DEFAULT_SLOTS && g_tiedProjects.Get(g_resType)->GetLength())
					{
						char buf[128] = "";
						// "Bookmark files attached to %s" would be more consistent, but too long...
						snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("Files attached to %s","sws_DLG_150"), GetFileRelativePath(g_tiedProjects.Get(g_resType)->Get()));
						m_txtTiedPrj.SetText(buf);

						// plain text if current project == tied project, alpha otherwise
						if (ColorTheme* ct = SNM_GetColorTheme())
							m_txtTiedPrj.SetColors(LICE_RGBA_FROMNATIVE(ct->main_text, *g_curProjectFn && !_stricmp(g_tiedProjects.Get(g_resType)->Get(), g_curProjectFn) ? 255 : 127));
					}
					else
						m_txtTiedPrj.SetText("");

					if (SNM_AutoVWndPosition(DT_LEFT, &m_txtTiedPrj, NULL, _r, &x0, _r->top, h, 5))
						SNM_AddLogo(_bm, _r, x0, h);
				}
			}
		}
	}

	// 2nd row of controls (bottom)

	x0 = _r->left+SNM_GUI_X_MARGIN; h=SNM_GUI_BOT_H;
	int y0 = _r->bottom-h;

	// defines a new rect r that takes the filter edit box into account (contrary to _r)
	RECT r;
	GetWindowRect(GetDlgItem(m_hwnd, IDC_FILTER), &r);
	ScreenToClient(m_hwnd, (LPPOINT)&r);
	ScreenToClient(m_hwnd, ((LPPOINT)&r)+1);
	r.top = _r->top;
	r.bottom = _r->bottom;
	r.right = r.left;
	r.left = _r->left;

	// "dbl-click to"
	if (fl->IsDblClick())
	{
		if (!SNM_AutoVWndPosition(DT_LEFT, &m_txtDblClickType, NULL, &r, &x0, y0, h, 5))
			return;
		if (!SNM_AutoVWndPosition(DT_LEFT, &m_cbDblClickType, &m_txtDblClickType, &r, &x0, y0, h))
			return;

		if (GetTypeForUser() == SNM_SLOT_TR)
		{
			if (const ConfigVar<int> offsPref = "templateditcursor") // >= REAPER v4.15
			{
				if (g_dblClickPrefs[g_resType] != 1) // propose offset option except if "apply to sel tracks"
				{
					m_btnOffsetTrTemplate.SetCheckState(*offsPref ? 1 : 0);
					if (!SNM_AutoVWndPosition(DT_LEFT, &m_btnOffsetTrTemplate, NULL, &r, &x0, y0, h, 5))
						return;
				}
			}
		}
	}
}

bool ResourcesWnd::GetToolTipString(int _xpos, int _ypos, char* _bufOut, int _bufOutSz)
{
	if (WDL_VWnd* v = m_parentVwnd.VirtWndFromPoint(_xpos,_ypos,1))
	{
		int typeForUser = GetTypeForUser();
		switch (v->GetID())
		{
			case BTNID_AUTOFILL:
				return (snprintfStrict(_bufOut, _bufOutSz, 
					__LOCALIZE_VERFMT("Auto-fill %s slots (right-click for options)\nfrom %s","sws_DLG_150"), 
					g_SNM_ResSlots.Get(typeForUser)->GetName(), 
					*GetAutoFillDir() ? GetAutoFillDir() : __LOCALIZE("undefined","sws_DLG_150")) > 0);
			case BTNID_AUTOSAVE:
				if (g_SNM_ResSlots.Get(g_resType)->IsAutoSave())
				{
					switch (typeForUser)
					{
						case SNM_SLOT_FXC:
							return (snprintfStrict(_bufOut, _bufOutSz, __LOCALIZE_VERFMT("%s (right-click for options)\nto %s","sws_DLG_150"),
								g_asFXChainPref == FXC_AUTOSAVE_PREF_TRACK ? __LOCALIZE("Auto-save FX chains for selected tracks","sws_DLG_150") :
								g_asFXChainPref == FXC_AUTOSAVE_PREF_ITEM ? __LOCALIZE("Auto-save FX chains for selected items","sws_DLG_150") :
								g_asFXChainPref == FXC_AUTOSAVE_PREF_INPUT_FX ? __LOCALIZE("Auto-save input FX chains for selected tracks","sws_DLG_150")
									: __LOCALIZE("Auto-save FX chain slots","sws_DLG_150"),
								*GetAutoSaveDir() ? GetAutoSaveDir() : __LOCALIZE("undefined","sws_DLG_150")) > 0);
						case SNM_SLOT_TR:
							return (snprintfStrict(_bufOut, _bufOutSz, __LOCALIZE_VERFMT("Auto-save track templates%s%s for selected tracks (right-click for options)\nto %s","sws_DLG_150"),
								(g_asTrTmpltPref&1) ? __LOCALIZE(" w/ items","sws_DLG_150") : "",
								(g_asTrTmpltPref&2) ? __LOCALIZE(" w/ envs","sws_DLG_150") : "",
								*GetAutoSaveDir() ? GetAutoSaveDir() : __LOCALIZE("undefined","sws_DLG_150")) > 0);
						default:
							return (snprintfStrict(_bufOut, _bufOutSz,
								__LOCALIZE_VERFMT("Auto-save %s slots (right-click for options)\nto %s","sws_DLG_150"), 
								g_SNM_ResSlots.Get(typeForUser)->GetName(), 
								*GetAutoSaveDir() ? GetAutoSaveDir() : __LOCALIZE("undefined","sws_DLG_150")) > 0);
					}
				}
				break;
			case CMBID_TYPE:
				return (snprintfStrict(_bufOut, _bufOutSz, __LOCALIZE_VERFMT("Bookmarks (right-click for options)\nA bookmark name ends with%s when slot actions are attached to it","sws_DLG_150"), RES_TIE_TAG) > 0);
			case BTNID_ADD_BOOKMARK:
				return (snprintfStrict(_bufOut, _bufOutSz, __LOCALIZE_VERFMT("New %s bookmark","sws_DLG_150"), g_SNM_ResSlots.Get(typeForUser)->GetName()) > 0);
			case BTNID_DEL_BOOKMARK:
				lstrcpyn(_bufOut, __LOCALIZE("Delete bookmark","sws_DLG_150"), _bufOutSz);
				return true;
			case TXTID_TIED_PRJ:
				return (g_tiedProjects.Get(g_resType)->GetLength() && 
					snprintfStrict(_bufOut, _bufOutSz, __LOCALIZE_VERFMT("Bookmark files attached to:\n%s","sws_DLG_150"), g_tiedProjects.Get(g_resType)->Get()) > 0);
		}
	}
	return false;
}

void ResourcesWnd::ClearListSelection()
{
	SWS_ListView* lv = GetListView();
	HWND hList = lv ? lv->GetHWND() : NULL;
	if (hList) // can be called when the view is closed!
		ListView_SetItemState(hList, -1, 0, LVIS_SELECTED);
}

// select a range of slots (contiguous, when the list view is not sorted) 
// or a single slot (when _slot2 < 0)
void ResourcesWnd::SelectBySlot(int _slot1, int _slot2, bool _selectOnly)
{
	SWS_ListView* lv = GetListView();
	HWND hList = lv ? lv->GetHWND() : NULL;
	if (lv && hList) // can be called when the view is closed!
	{
		int slot1=_slot1, slot2=_slot2;

		// check params
		if (_slot1 < 0)
			return;
		if (_slot2 < 0)
			slot2 = slot1;
		else if (_slot2 < _slot1) {
			slot1 = _slot2;
			slot2 = _slot1;
		}

		if (_selectOnly)
			ListView_SetItemState(hList, -1, 0, LVIS_SELECTED);

		int firstSel = -1;
		for (int i=0; i < lv->GetListItemCount(); i++)
		{
			ResourceItem* item = (ResourceItem*)lv->GetListItem(i);
			int slot = g_SNM_ResSlots.Get(g_resType)->Find(item);
			if (item && slot >= _slot1 && slot <= slot2) 
			{
				if (firstSel < 0)
					firstSel = i;
				ListView_SetItemState(hList, i, LVIS_SELECTED, LVIS_SELECTED);
				if (_slot2 < 0) // one slot to be selected?
					break;
			}
		}
		if (firstSel >= 0)
			ListView_EnsureVisible(hList, firstSel, true);
	}
}

// gets selected slots and returns the number of non empty slots among them
void ResourcesWnd::GetSelectedSlots(WDL_PtrList<ResourceItem>* _selSlots, WDL_PtrList<ResourceItem>* _selEmptySlots)
{
	if (m_pLists.GetSize())
	{
		int x=0;
		while (ResourceItem* pItem = (ResourceItem*)GetListView()->EnumSelected(&x))
		{
			if (_selEmptySlots && pItem->IsDefault())
				_selEmptySlots->Add(pItem);
			else
				_selSlots->Add(pItem);
		}
	}
}

void ResourcesWnd::AddSlot()
{
	if (ResourceList* fl = g_SNM_ResSlots.Get(g_resType))
	{
		int idx = fl->GetSize();
		if (fl->Add(new ResourceItem())) {
			Update();
			SelectBySlot(idx);
		}
	}
}

void ResourcesWnd::InsertAtSelectedSlot()
{
	ResourceList* fl = g_SNM_ResSlots.Get(g_resType);
	if (fl && fl->GetSize() && m_pLists.GetSize())
	{
		if (ResourceItem* item = (ResourceItem*)GetListView()->EnumSelected(NULL))
		{
			int slot = fl->Find(item);
			if (slot>=0 && fl->InsertSlot(slot) != NULL)
			{
				Update();
				SelectBySlot(slot);
				return; // <-- !!
			}
		}
	}
	AddSlot(); // empty list, no selection, etc.. => add
}


///////////////////////////////////////////////////////////////////////////////
// Auto-save, auto-fill
///////////////////////////////////////////////////////////////////////////////

bool CheckSetAutoDirectory(const char* _title, int _type, bool _autoSave)
{
	WDL_FastString* dir = _autoSave ? g_autoSaveDirs.Get(_type) : g_autoFillDirs.Get(_type);
	if (!FileOrDirExists(dir->Get()))
	{
		char buf[SNM_MAX_PATH] = "";
		snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("%s directory not found!\n%s%sDo you want to define one ?","sws_DLG_150"), _title, dir->Get(), dir->GetLength()?"\n":"");
		if (IDYES == MessageBox(g_resWndMgr.GetMsgHWND(), buf, __LOCALIZE("S&M - Warning","sws_DLG_150"), MB_YESNO)) {
			if (BrowseForDirectory(_autoSave ? __LOCALIZE("Set auto-save directory","sws_DLG_150") : __LOCALIZE("Set auto-fill directory","sws_DLG_150"), GetResourcePath(), buf, sizeof(buf))) {
				if (_autoSave) SetAutoSaveDir(buf, _type);
				else SetAutoFillDir(buf, _type);
			}
		}
		else
			return false;
		return FileOrDirExists(dir->Get()); // re-check (browse cancelled, etc..)
	}
	return true;
}

bool AutoSaveChunkSlot(const void* _obj, const char* _fn) {
	return SaveChunk(_fn, (WDL_FastString*)_obj, true);
}

// overwitre a slot -or- genenerate a new filename + add a slot
// _owSlots: slots to overwrite, if any
// note: just a helper func for AutoSaveTrackSlots(), AutoSaveMediaSlot(), etc..
bool AutoSaveSlot(int _type, const char* _dirPath, 
				  const char* _name, const char* _ext,
				  WDL_PtrList<ResourceItem>* _owSlots, int* _owIdx, 
				  bool (*SaveSlot)(const void*, const char*), const void* _obj) // see AutoSaveChunkSlot() example
{
	bool saved = false;
	char fn[SNM_MAX_PATH] = "";
	int todo = 1; // 0: nothing, 1: gen filename + add slot, 2: gen filename + replace slot

	if (*_owIdx < _owSlots->GetSize() && _owSlots->Get(*_owIdx))
	{
		// gets the filename to overwrite
		GetFullResourcePath(g_SNM_ResSlots.Get(_type)->GetResourceDir(), _owSlots->Get(*_owIdx)->m_shortPath.Get(), fn, sizeof(fn));
		if (!*fn)
		{
			todo=2;
		}
		else
		{
			todo=0;
			if (_stricmp(_ext, GetFileExtension(fn))) // corner case, ex: try to write .mid over .wav
			{
				if (char* p = strrchr(fn, '.'))
				{
					strcpy(p+1, _ext);
					const char* shortPath = GetShortResourcePath(g_SNM_ResSlots.Get(_type)->GetResourceDir(), fn);
					_owSlots->Get(*_owIdx)->m_shortPath.Set(shortPath);
				}
			}
			saved = (SaveSlot ? SaveSlot(_obj, fn) : SNM_CopyFile(fn, _name));
		}
		*_owIdx = *_owIdx + 1;
	}

	if (todo)
	{
		char fnNoExt[SNM_MAX_PATH] = "";
		GetFilenameNoExt(_name, fnNoExt, sizeof(fnNoExt));
		Filenamize(fnNoExt);
		if (GenerateFilename(_dirPath, fnNoExt, _ext, fn, sizeof(fn)) && (SaveSlot ? SaveSlot(_obj, fn) : SNM_CopyFile(fn, _name)))
		{
			saved = true;
			if (todo==1)
			{
				g_SNM_ResSlots.Get(_type)->AddSlot(fn);
			}
			else 
			{
				const char* shortPath = GetShortResourcePath(g_SNM_ResSlots.Get(_type)->GetResourceDir(), fn);
				_owSlots->Get(*_owIdx-1)->m_shortPath.Set(shortPath);
			}
		}
	}
	return saved;
}

// _ow: true=add or prompt to overwrite, false=add new slot only
// _flags:
//    - for track templates: _flags&1 save template with items, _flags&2 save template with envs
//    - for fx chains: enum FXC_AUTOSAVE_PREF_INPUT_FX, FXC_AUTOSAVE_PREF_TRACK and FXC_AUTOSAVE_PREF_ITEM
//    - n/a otherwise
void AutoSave(int _type, bool _ow, int _flags)
{
	WDL_PtrList<ResourceItem> owSlots; // slots to overwrite
	WDL_PtrList<ResourceItem> selFilledSlots;

	// overwrite non empty slots?
	// disabled for macros: always save brand new slots (useful with  "last slot" actions) 
	ResourcesWnd* resWnd = g_resWndMgr.Get();
	if (_ow && resWnd)
		resWnd->GetSelectedSlots(&selFilledSlots, &owSlots); // &owSlots: empty slots are always overwritten

	int ow = IDNO;
	if (_ow && selFilledSlots.GetSize())
	{
		ow = MessageBox(g_resWndMgr.GetMsgHWND(),
				__LOCALIZE("Do you want to overwrite selected slots?\nIf you select No, new slots/files will be added.","sws_DLG_150"),
				__LOCALIZE("S&M - Confirmation","sws_DLG_150"),
				MB_YESNOCANCEL);
	}

	if (ow == IDYES) {
		for (int i=0; i<selFilledSlots.GetSize(); i++)
			owSlots.Add(selFilledSlots.Get(i));
	}
	else if (ow == IDCANCEL)
		return;

	if (!CheckSetAutoDirectory(__LOCALIZE("Auto-save","sws_DLG_150"), _type, true))
		return;


	// the meat -------------------------------------------------------------->
	//JFB! untie all files, save, tie all: brutal but safe
	char prjPath[SNM_MAX_PATH]="";
	ReaProject* prj = EnumProjects(-1, prjPath, sizeof(prjPath));

	UntieAllResFileFromProject(prj, prjPath, _type);

	bool saved = false;
	int oldSz = g_SNM_ResSlots.Get(_type)->GetSize();
	switch(GetTypeForUser(_type))
	{
		case SNM_SLOT_FXC:
			switch (_flags)
			{
				case FXC_AUTOSAVE_PREF_INPUT_FX:
				case FXC_AUTOSAVE_PREF_TRACK:
					saved = AutoSaveTrackFXChainSlots(_type, GetAutoSaveDir(_type), &owSlots, g_asFXChainNamePref==1, _flags==FXC_AUTOSAVE_PREF_INPUT_FX);
					break;
				case FXC_AUTOSAVE_PREF_ITEM:
					saved = AutoSaveItemFXChainSlots(_type, GetAutoSaveDir(_type), &owSlots, g_asFXChainNamePref==1);
					break;
			}
			break;
		case SNM_SLOT_TR:
			saved = AutoSaveTrackSlots(_type, GetAutoSaveDir(_type), &owSlots, (_flags&1)?false:true, (_flags&2)?false:true);
			break;
		case SNM_SLOT_PRJ:
			saved = AutoSaveProjectSlot(_type, GetAutoSaveDir(_type), &owSlots, true);
			break;
		case SNM_SLOT_MEDIA:
			saved = AutoSaveMediaSlots(_type, GetAutoSaveDir(_type), &owSlots);
			break;
	}

	TieAllResFileToProject(prj, prjPath, _type);
	// the meat <--------------------------------------------------------------


	if (saved)
	{
		if (resWnd && g_resType==_type)
		{
			resWnd->Update();

			// sel new slots
			bool first = true;
			if (oldSz != g_SNM_ResSlots.Get(_type)->GetSize()) {
				resWnd->SelectBySlot(oldSz, g_SNM_ResSlots.Get(_type)->GetSize(), first);
				first = false;
			}
			// sel overwriten slots
			for (int i=0; i<owSlots.GetSize(); i++)
			{
				int slot = g_SNM_ResSlots.Get(_type)->Find(owSlots.Get(i));
				if (slot >= 0) {
					resWnd->SelectBySlot(slot, slot, first);
					first = false;
				}
			}
		}
	}
	else
	{
		char msg[SNM_MAX_PATH];
		snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("Auto-save failed!\n%s","sws_DLG_150"), AUTOSAVE_ERR_STR);
		MessageBox(g_resWndMgr.GetMsgHWND(), msg, __LOCALIZE("S&M - Error","sws_DLG_150"), MB_OK);
	}
}

// recursive from auto-fill path
void AutoFill(int _type)
{
	ResourceList* fl = g_SNM_ResSlots.Get(_type);
	if (!fl)
		return;

	if (!CheckSetAutoDirectory(__LOCALIZE("Auto-fill","sws_DLG_150"), _type, false))
		return;

	int startSlot = g_SNM_ResSlots.Get(_type)->GetSize();

	char fileFilter[2048] = ""; // filters need some room!
	fl->GetFileFilter(fileFilter, sizeof(fileFilter), false);

	WDL_PtrList_DeleteOnDestroy<WDL_String> files; 
	ScanFiles(&files, GetAutoFillDir(_type), fileFilter, true);
	if (int sz = files.GetSize())
		for (int i=0; i<sz; i++)
			if (fl->FindByPath(files.Get(i)->Get()) < 0) { // skip if already present
				TieResFileToProject(files.Get(i)->Get(), _type);
				fl->AddSlot(files.Get(i)->Get());
			}

	if (startSlot != fl->GetSize())
	{
		if (g_resType==_type)
			if (ResourcesWnd* w = g_resWndMgr.Get()) {
				w->Update();
				w->SelectBySlot(startSlot, g_SNM_ResSlots.Get(_type)->GetSize());
			}
	}
	else
	{
		const char* path = GetAutoFillDir(_type);
		char msg[SNM_MAX_PATH]="";
		if (path && *path) snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("No slot added from: %s\n%s","sws_DLG_150"), path, AUTOFILL_ERR_STR);
		else snprintf(msg, sizeof(msg), __LOCALIZE_VERFMT("No slot added!\n%s","sws_DLG_150"), AUTOFILL_ERR_STR);
		MessageBox(g_resWndMgr.GetMsgHWND(), msg, __LOCALIZE("S&M - Warning","sws_DLG_150"), MB_OK);
	}
}


///////////////////////////////////////////////////////////////////////////////
// Get, load, clear, delete slots/files
///////////////////////////////////////////////////////////////////////////////

WDL_FastString g_lastBrowsedFn;

// returns false on cancel/err
bool BrowseSlot(int _type, int _slot, bool _tieUntiePrj, char* _fn, int _fnSz, bool* _updatedList)
{
	bool ok = false;
	if (ResourceList* fl = g_SNM_ResSlots.Get(_type))
	{
		if (_slot>=0 && _slot<fl->GetSize())
		{
			char untiePath[SNM_MAX_PATH] = "";
			if (_tieUntiePrj && _type>=SNM_NUM_DEFAULT_SLOTS && g_tiedProjects.Get(_type)->GetLength())
				fl->GetFullPath(_slot, untiePath, sizeof(untiePath));

			char title[512], fileFilter[2048]; // room needed for file filters!
			snprintf(title, sizeof(title), __LOCALIZE_VERFMT("S&M - Load resource file (slot %d)","sws_DLG_150"), _slot+1);
			fl->GetFileFilter(fileFilter, sizeof(fileFilter));

			if (char* fn = BrowseForFiles(title, g_lastBrowsedFn.GetLength()?g_lastBrowsedFn.Get():GetAutoFillDir(_type), NULL, false, fileFilter)) // single file
			{
				// have to test extension (because of the "All files (*.*)" filter..)
				if (fl->IsValidFileExt(GetFileExtension(fn)))
				{
					if (_fn)
						lstrcpyn(_fn, fn, _fnSz);
					if (fl->SetFromFullPath(_slot, fn))
					{
						ok = true;
						if (_updatedList)
							*_updatedList = true;
						if (_tieUntiePrj) {
							UntieResFileFromProject(untiePath, _type);
							TieResFileToProject(fn, _type);
						}
					}
				}
				else
				{
					WDL_FastString msg;
					msg.SetFormatted(512, 
						__LOCALIZE_VERFMT("The file extension \".%s\" is not supported in the bookmark \"%s\"","sws_DLG_150"),
						GetFileExtension(fn), fl->GetName());
					MessageBox(GetMainHwnd(), msg.Get(), __LOCALIZE("S&M - Error","sws_DLG_150"), MB_OK);
				}
				g_lastBrowsedFn.Set(fn);
				free(fn);
			}
		}
	}
	return ok;
}

// made as macro-friendly as possible
// returns NULL if failed, otherwise it's up to the caller to free the returned string
// _slot: in/out param, prompt for slot if -1
WDL_FastString* GetOrPromptOrBrowseSlot(int _type, int* _slot)
{
	ResourceList* fl = g_SNM_ResSlots.Get(_type);
	if (!fl)
		return NULL;

	switch (*_slot)
	{
		// prompt for slot
		case -1:
			*_slot = PromptForInteger(__LOCALIZE("S&M - Define resource slot","sws_DLG_150"), __LOCALIZE("Slot","sws_DLG_150"), 1, fl->GetSize()); // loops on err
			break;
		// last slot or "force" slot creation if empty (macro friendly)
		case -2:
			*_slot = fl->GetSize() ? fl->GetSize()-1 : 0;
			break;
	}

	// prompt cancelled?
	if (*_slot < 0)
		return NULL; 

	// adds the needed number of slots (macro friendly)
	bool uiUpdate = (*_slot >= fl->GetSize());
	while (*_slot >= fl->GetSize())
		fl->Add(new ResourceItem());

	// get filename or browse if the slot is empty (macro friendly)
	char fn[SNM_MAX_PATH]="";
	bool fnOk=false, listUpdate=false;
	if (fl->Get(*_slot)->IsDefault())
		fnOk = BrowseSlot(_type, *_slot, false, fn, sizeof(fn), &listUpdate);
	else if (fl->GetFullPath(*_slot, fn, sizeof(fn)))
		fnOk = FileOrDirExistsErrMsg(fn, !fl->Get(*_slot)->IsDefault());

	WDL_FastString* fnStr = NULL;
	if (fnOk)
	{
		fnStr = new WDL_FastString(fn);

		// in case a slot has been filled..
		if (listUpdate) {
			TieResFileToProject(fn, _type);
			uiUpdate = true;
		}
	}

	if (uiUpdate && g_resType==_type)
		if (ResourcesWnd* w = g_resWndMgr.Get())
			w->Update();

	if (!fnStr)
		*_slot = -1;
	return fnStr;
}

// _slot: slot index or -1 for selected slots
// _mode&1 = delete slot(s)
// _mode&2 = clear slot(s)
// _mode&4 = delete file(s) too
// _mode&8 = delete all slots (exclusive with everything above)
void ClearDeleteSlotsFiles(int _type, int _mode, int _slot)
{
	ResourceList* fl = g_SNM_ResSlots.Get(_type);
	if (!fl)
		return;

	WDL_PtrList<int> slots;
	while (slots.GetSize() != fl->GetSize())
		slots.Add(_mode&8 ? &g_i1 : &g_i0);

	ResourcesWnd* resWnd = g_resWndMgr.Get();
	if (_slot<0 && resWnd)
	{
		int x=0;
		if (SWS_ListView* lv = resWnd->GetListView())
		{
			while(ResourceItem* item = (ResourceItem*)lv->EnumSelected(&x))
			{
				int slot = fl->Find(item);
				slots.Delete(slot);
				slots.Insert(slot, &g_i1);
			}
		}
	}
	else if (_slot>=0)
	{
		slots.Delete(_slot);
		slots.Insert(_slot, &g_i1);
	}

	char fullPath[SNM_MAX_PATH] = "";
	WDL_PtrList_DeleteOnDestroy<ResourceItem> delItems; // DeleteOnDestroy: keep pointers until updated
	for (int slot=slots.GetSize()-1; slot >=0 ; slot--)
	{
		if (*slots.Get(slot)) // avoids new Find(item) calls
		{
			if (ResourceItem* item = fl->Get(slot))
			{
				*fullPath = '\0';
				if (_mode&4 || (_type>=SNM_NUM_DEFAULT_SLOTS && g_tiedProjects.Get(_type)->GetLength())) // avoid useless job
					fl->GetFullPath(slot, fullPath, sizeof(fullPath));

				if (_mode&4)
					SNM_DeleteFile(fullPath, true);

				if (_mode&1 || _mode&8) 
				{
					slots.Delete(slot, false); // keep the sel list "in sync"
					fl->Delete(slot, false); // remove slot, pointer not deleted yet
					delItems.Add(item); // for later pointer deletion..
				}
				else if (_mode&2)
					fl->ClearSlot(slot);

				UntieResFileFromProject(fullPath, _type, (_mode&4)!=4);
			}
		}
	}

	if (resWnd && g_resType==_type)
		resWnd->Update();

} // + delItems auto clean-up !


///////////////////////////////////////////////////////////////////////////////
// Ini file helpers
///////////////////////////////////////////////////////////////////////////////

void FlushCustomTypesIniFile()
{
	char iniSec[64]="";
	for (int i=SNM_NUM_DEFAULT_SLOTS; i<g_SNM_ResSlots.GetSize(); i++)
	{
		if (g_SNM_ResSlots.Get(i))
		{
			GetIniSectionName(i, iniSec, sizeof(iniSec));
			WritePrivateProfileStruct(iniSec, NULL, NULL, 0, g_SNM_IniFn.Get()); // flush section
			WritePrivateProfileString(RES_INI_SEC, iniSec, NULL, g_SNM_IniFn.Get());
			WDL_FastString str;
			str.SetFormatted(64, "AutoFillDir%s", iniSec);
			WritePrivateProfileString(RES_INI_SEC, str.Get(), NULL, g_SNM_IniFn.Get());
			str.SetFormatted(64, "AutoSaveDir%s", iniSec);
			WritePrivateProfileString(RES_INI_SEC, str.Get(), NULL, g_SNM_IniFn.Get());
			str.SetFormatted(64, "SyncAutoDirs%s", iniSec);
			WritePrivateProfileString(RES_INI_SEC, str.Get(), NULL, g_SNM_IniFn.Get());
			str.SetFormatted(64, "TiedActions%s", iniSec);
			WritePrivateProfileString(RES_INI_SEC, str.Get(), NULL, g_SNM_IniFn.Get());
			str.SetFormatted(64, "TiedProject%s", iniSec);
			WritePrivateProfileString(RES_INI_SEC, str.Get(), NULL, g_SNM_IniFn.Get());
			str.SetFormatted(64, "DblClick%s", iniSec);
			WritePrivateProfileString(RES_INI_SEC, str.Get(), NULL, g_SNM_IniFn.Get());
		}
	}
}

void ReadSlotIniFile(const char* _key, int _slot, char* _path, int _pathSize, char* _desc, int _descSize)
{
	char buf[32];
	if (snprintfStrict(buf, sizeof(buf), "Slot%d", _slot+1) > 0)
		GetPrivateProfileString(_key, buf, "", _path, _pathSize, g_SNM_IniFn.Get());
	if (snprintfStrict(buf, sizeof(buf), "Desc%d", _slot+1) > 0)
		GetPrivateProfileString(_key, buf, "", _desc, _descSize, g_SNM_IniFn.Get());
}

// adds bookmarks and custom slot types from the S&M.ini file
void AddCustomTypesFromIniFile()
{
	int i=0;
	char buf[SNM_MAX_PATH]=""; // can be used with paths..
	WDL_String iniKeyStr("CustomSlotType1");
	GetPrivateProfileString(RES_INI_SEC, iniKeyStr.Get(), "", buf, sizeof(buf), g_SNM_IniFn.Get());
	while (*buf && i<SNM_MAX_SLOT_TYPES)
	{
		AddCustomBookmark(buf);
		iniKeyStr.SetFormatted(32, "CustomSlotType%d", (++i)+1);
		GetPrivateProfileString(RES_INI_SEC, iniKeyStr.Get(), "", buf, sizeof(buf), g_SNM_IniFn.Get());
	}
}


///////////////////////////////////////////////////////////////////////////////
// Bookmarks
///////////////////////////////////////////////////////////////////////////////

// add a custom bookmark, 
// _definition: resource_directory_name,description,file_extensions
// examples:
// - "Configurations,ReaConfig,ReaperConfigZip"
// - "Docs,Document,txt,rtf,pdf"
// - "Misc,Any file,*
int AddCustomBookmark(char* _definition)
{
	if (_definition && *_definition && g_SNM_ResSlots.GetSize()<SNM_MAX_SLOT_TYPES)
	{
		// basic check (_definition can be a user-defined string)
		int i=-1, commas=0;
		while (_definition[++i])
			if (_definition[i]==',')
				commas++;
		if (commas>=2)
		{
			WDL_String tokenStrs[2];
			if (char* tok = strtok(_definition, ","))
			{
				tokenStrs[0].Set(tok);
				if ((tok = strtok(NULL, ",")))
				{
					tokenStrs[1].Set(tok);

					ResourceList* fl = new ResourceList(
						tokenStrs[0].Get(), 
						tokenStrs[1].Get(), 
						(const char*)(tok+strlen(tok)+1), // add extensions as defined
						0);
					g_SNM_ResSlots.Add(fl);

					// known slot type?
					int newType = g_SNM_ResSlots.GetSize()-1;
					int typeForUser = GetTypeForUser(newType);

					// (very) default inits
					char path[SNM_MAX_PATH]="";
					if (snprintfStrict(path, sizeof(path), "%s%c%s", GetResourcePath(), PATH_SLASH_CHAR, tokenStrs[0].Get()) <= 0)
						*path = '\0';
					g_autoSaveDirs.Add(new WDL_FastString(path));
					g_autoFillDirs.Add(new WDL_FastString(path));
					g_tiedProjects.Add(new WDL_FastString);
					g_syncAutoDirPrefs[newType] = true;
					g_dblClickPrefs[newType] = 0;
					if (newType != typeForUser) // this is a known type => enable some features
						fl->SetFlags(g_SNM_ResSlots.Get(typeForUser)->GetFlags());
					else // enable dbl-click for custom bookmarks
						fl->SetFlags(SNM_RES_MASK_DBLCLIK);
					return newType;
				}
			}
		}
	}
	return -1;
}

// new typed bookmark, new custom bookmark or copy bookmark
// _type: bookmark type to create, -1=custom bookmark
void NewBookmark(int _type, bool _copyCurrent)
{
	if (g_SNM_ResSlots.GetSize() < SNM_MAX_SLOT_TYPES)
	{
		char input[128] = "";
		if (_type>=0)
		{
			snprintf(input, sizeof(input), 
				__LOCALIZE_VERFMT("My %s slots","sws_DLG_150"), 
				g_SNM_ResSlots.Get(GetTypeForUser(_type))->GetName());
		}

		if (int saveOption = PromptUserForString(
				g_resWndMgr.GetMsgHWND(),
				_copyCurrent ? __LOCALIZE("S&M - Copy bookmark","sws_DLG_150") : _type<0 ? __LOCALIZE("S&M - Add custom bookmark","sws_DLG_150") : __LOCALIZE("S&M - Add bookmark","sws_DLG_150"),
				input, sizeof(input),
				true,
				_copyCurrent ? NULL : __LOCALIZE("Attach bookmark files to this project","sws_DLG_150")))
		{
			// 1) add a new slot list + new items to auto-fill, auto-save and attached project lists
			int newType = g_SNM_ResSlots.GetSize();

			// custom bookmark?
			if (_type<0)
			{
				if (AddCustomBookmark(input) < 0)
				{
					WDL_FastString msg(__LOCALIZE("Invalid bookmark definition!","sws_DLG_150"));
					msg.Append("\n");
					msg.Append(__LOCALIZE("Expected format: resource_directory_name,description,file_extensions","sws_DLG_150"));
					msg.Append("\n");
					msg.Append(__LOCALIZE("Example: Docs,Document,txt,rtf,pdf","sws_DLG_150"));
					MessageBox(g_resWndMgr.GetMsgHWND(), msg.Get(), __LOCALIZE("S&M - Error","sws_DLG_150"), MB_OK);
					return;
				}
			}
			// standard bookmark?
			else 
			{
				if (!*input || strchr(input, ',')) // check for comma as it is used as a separator..
				{
					WDL_FastString msg(__LOCALIZE("Invalid bookmark name!","sws_DLG_150"));
					msg.Append("\n");
					msg.Append(__LOCALIZE("Note: bookmark names cannot contain the character ,","sws_DLG_150"));
					MessageBox(g_resWndMgr.GetMsgHWND(), msg.Get(), __LOCALIZE("S&M - Error","sws_DLG_150"), MB_OK);
					return;
				}

				g_SNM_ResSlots.Add(new ResourceList(
					g_SNM_ResSlots.Get(_type)->GetResourceDir(), 
					input, 
					g_SNM_ResSlots.Get(_type)->GetFileExtStr(), 
					g_SNM_ResSlots.Get(_type)->GetFlags()));

				g_autoSaveDirs.Add(new WDL_FastString);
				g_autoFillDirs.Add(new WDL_FastString);
				g_tiedProjects.Add(new WDL_FastString);
			}

			// 2) update those new list items according to user inputs
			WDL_FastString *saveDir, *fillDir, *attachedPrj;
			saveDir = g_autoSaveDirs.Get(newType);
			fillDir = g_autoFillDirs.Get(newType);
			attachedPrj = g_tiedProjects.Get(newType);

			// attach files & auto-fill/save paths to the current project
			if (saveOption==2)
			{
				char curPrjFn[SNM_MAX_PATH]="";
				EnumProjects(-1, curPrjFn, sizeof(curPrjFn));

				WDL_FastString curPrjPath;
				if (const char* p = strrchr(curPrjFn, PATH_SLASH_CHAR))
					curPrjPath.Set(curPrjFn, (int)(p-curPrjFn));

				saveDir->Set(curPrjPath.Get());
				fillDir->Set(curPrjPath.Get());
				g_syncAutoDirPrefs[newType] = true;
				attachedPrj->Set(curPrjFn);
				g_dblClickPrefs[newType] = 0;
			}
			// copy
			else if (_copyCurrent && _type>=0) // exclusive param values
			{
				saveDir->Set(GetAutoSaveDir(_type));
				fillDir->Set(GetAutoFillDir(_type));
				g_syncAutoDirPrefs[newType] = g_syncAutoDirPrefs[_type];
				attachedPrj->Set(g_tiedProjects.Get(_type)->Get());
				g_dblClickPrefs[newType] = g_dblClickPrefs[_type];
			}
			// default init
			else if (_type>=0)
			{
				char path[SNM_MAX_PATH]="";
				if (snprintfStrict(path, sizeof(path), "%s%c%s", GetResourcePath(), PATH_SLASH_CHAR, g_SNM_ResSlots.Get(_type)->GetResourceDir()) > 0) {
					if (!FileOrDirExists(path))
						CreateDirectory(path, NULL);
				}
				else
					*path = '\0';

				saveDir->Set(path);
				fillDir->Set(path);
				g_syncAutoDirPrefs[newType] = true;
				attachedPrj->Set("");
				g_dblClickPrefs[newType] = 0;
			}
			// custom bookmark (_type<0), not attached to a project
			else
			{
				// use the default init made in AddCustomBookmark()
			}

			// 3) set other common properties
			int typeForUser = GetTypeForUser(newType);
			if (newType != typeForUser) // this is a known type => enable related features
				g_SNM_ResSlots.Get(newType)->SetFlags(g_SNM_ResSlots.Get(typeForUser)->GetFlags());
			else // enable dbl-click for custom bookmarks
				g_SNM_ResSlots.Get(newType)->SetFlags(SNM_RES_MASK_DBLCLIK);

			// 4) copy slots if needed
			if (_copyCurrent)
			{
				if (ResourceList* flCur = g_SNM_ResSlots.Get(g_resType))
					for (int i=0; i<flCur->GetSize(); i++)
						if (ResourceItem* slotItem = flCur->Get(i))
							g_SNM_ResSlots.Get(newType)->AddSlot(slotItem->m_shortPath.Get(), slotItem->m_comment.Get());
			}

			if (ResourcesWnd* w = g_resWndMgr.Get()) {
				w->FillTypeCombo();
				w->SetType(newType); // incl. UI update
			}
		}
	}
	else
		MessageBox(g_resWndMgr.GetMsgHWND(), __LOCALIZE("Too many resource types!","sws_DLG_150"), __LOCALIZE("S&M - Error","sws_DLG_150"), MB_OK);
}

void DeleteBookmark(int _bookmarkType)
{
	if (_bookmarkType >= SNM_NUM_DEFAULT_SLOTS)
	{
		int reply = IDNO;
		if (g_SNM_ResSlots.Get(_bookmarkType)->GetNonEmptySize()) // do not ask if empty
		{
			char title[128] = "";
			snprintf(title, sizeof(title),
				__LOCALIZE_VERFMT("S&M - Delete bookmark \"%s\"","sws_DLG_150"),
				g_SNM_ResSlots.Get(_bookmarkType)->GetName());

			reply = MessageBox(g_resWndMgr.GetMsgHWND(),
				__LOCALIZE("Delete all resource files too?","sws_DLG_150"),
				title, MB_YESNOCANCEL);
		}
		if (reply != IDCANCEL)
		{
			// cleanup ini file (types may not be contiguous anymore..)
			FlushCustomTypesIniFile();
			ClearDeleteSlotsFiles(_bookmarkType, 8|(reply==IDYES?4:0));

			// remove current slot type
			int oldType = _bookmarkType;
			int oldTypeForUser = GetTypeForUser(oldType);
			g_dblClickPrefs[oldType] = 0;
			if (g_tiedSlotActions[oldTypeForUser] == oldType)
				g_tiedSlotActions[oldTypeForUser] = oldTypeForUser;
			g_autoSaveDirs.Delete(oldType, true);
			g_autoFillDirs.Delete(oldType, true);
			g_tiedProjects.Delete(oldType, true);
			g_syncAutoDirPrefs[oldType] = true;
			g_SNM_ResSlots.Delete(oldType, true);

			if (ResourcesWnd* w = g_resWndMgr.Get()) {
				w->FillTypeCombo();
				w->SetType(oldType-1); // + GUI update
			}
		}
	}
}

void RenameBookmark(int _bookmarkType)
{
	if (_bookmarkType >= SNM_NUM_DEFAULT_SLOTS)
	{
		char newName[64] = "";
		lstrcpyn(newName, g_SNM_ResSlots.Get(_bookmarkType)->GetName(), sizeof(newName));
		if (PromptUserForString(g_resWndMgr.GetMsgHWND(), __LOCALIZE("S&M - Rename bookmark","sws_DLG_150"), newName, sizeof(newName), true) && *newName)
		{
			g_SNM_ResSlots.Get(_bookmarkType)->SetName(newName);
			if (ResourcesWnd* w = g_resWndMgr.Get()) {
				w->FillTypeCombo();
				w->Update();
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// project_config_extension_t
///////////////////////////////////////////////////////////////////////////////

// just to be notified of "save as"
// note: project load/switches are notified via ResourcesTrackListChange()
//JFB!!! TODO? auto-export if tied? + update paths? (+ msg?)
static void SaveExtensionConfig(ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	char path[SNM_MAX_PATH]="";
	if (!isUndo && // saving?
		IsActiveProjectInLoadSave(path, sizeof(path)) && // saving current project?
		_stricmp(path, g_curProjectFn)) // saving current project as new_name.rpp?
	{
		ScheduledJob::Schedule(new AttachResourceFilesJob(SNM_SCHEDJOB_ASYNC_DELAY_OPT));
	}
}

static project_config_extension_t s_projectconfig = {
	NULL, SaveExtensionConfig, NULL, NULL
};


///////////////////////////////////////////////////////////////////////////////

// detach/re-attach everything when switching projects (incl. save as new_name.RPP) 
void AttachResourceFiles()
{
	char newProjectFn[SNM_MAX_PATH]="";
	if (ReaProject* newProject = EnumProjects(-1, newProjectFn, sizeof(newProjectFn)))
	{
		// do the job only if needed (track project switches)
		if (_stricmp(g_curProjectFn, newProjectFn))
		{
			// detach all files of all bookmarks from previous active project, if any
			UntieAllResFileFromProject(g_curProject, g_curProjectFn);

			// attach files to new active project
			TieAllResFileToProject(newProject, newProjectFn);

			// set new current project
			g_curProject = newProject;
			lstrcpyn(g_curProjectFn, newProjectFn, sizeof(g_curProjectFn));

			// UI update: tied project name + alpha/plain text
			if (g_resType>=SNM_NUM_DEFAULT_SLOTS)
				if (ResourcesWnd* w = g_resWndMgr.Get())
					w->GetParentVWnd()->RequestRedraw(NULL);
		}
	}
}

void AttachResourceFilesJob::Perform() {
	AttachResourceFiles();
}


///////////////////////////////////////////////////////////////////////////////

// this is our only notification of active project tab change, so attach/detach everything
// ScheduledJob used because of multi-notifs
void ResourcesTrackListChange()
{
/*no! replaced with code below to fixe a potential hang when closing project tabs
	ScheduledJob::Schedule(new AttachResourceFilesJob(SNM_SCHEDJOB_ASYNC_DELAY_OPT));
*/
	AttachResourceFiles();
}

void ResourcesUpdate() {
	if (ResourcesWnd* w = g_resWndMgr.Get())
		w->Update();
}


///////////////////////////////////////////////////////////////////////////////

int ResourcesInit()
{
	// localization
	g_filter.Set(FILTER_DEFAULT_STR);

	g_SNM_ResSlots.Empty(true);

	// cross-platform resource definition (path separators will get replaced if needed)
	g_SNM_ResSlots.Add(new ResourceList("FXChains",			__LOCALIZE("FX chain","sws_DLG_150"),		"RfxChain",							SNM_RES_MASK_AUTOFILL|SNM_RES_MASK_AUTOSAVE|SNM_RES_MASK_DBLCLIK|SNM_RES_MASK_TEXT));
	g_SNM_ResSlots.Add(new ResourceList("TrackTemplates",	__LOCALIZE("track template","sws_DLG_150"),	"RTrackTemplate",					SNM_RES_MASK_AUTOFILL|SNM_RES_MASK_AUTOSAVE|SNM_RES_MASK_DBLCLIK|SNM_RES_MASK_TEXT));
	// RPP* is a keyword (supported project extensions)
	g_SNM_ResSlots.Add(new ResourceList("ProjectTemplates",	__LOCALIZE("project","sws_DLG_150"),		"RPP*",								SNM_RES_MASK_AUTOFILL|SNM_RES_MASK_AUTOSAVE|SNM_RES_MASK_DBLCLIK|SNM_RES_MASK_TEXT));
	// WAV* is a keyword (supported media extensions)
	g_SNM_ResSlots.Add(new ResourceList("MediaFiles",		__LOCALIZE("media file","sws_DLG_150"),		"WAV*",								SNM_RES_MASK_AUTOFILL|SNM_RES_MASK_AUTOSAVE|SNM_RES_MASK_DBLCLIK));
	// PNG* is a keyword (but we can't get image extensions at runtime, replaced with SNM_REAPER_IMG_EXTS) 
	g_SNM_ResSlots.Add(new ResourceList("Data/track_icons",	__LOCALIZE("image","sws_DLG_150"),			"PNG*",								SNM_RES_MASK_AUTOFILL|SNM_RES_MASK_DBLCLIK));
	g_SNM_ResSlots.Add(new ResourceList("ColorThemes",		__LOCALIZE("theme","sws_DLG_150"),			"ReaperthemeZip,ReaperTheme",		SNM_RES_MASK_AUTOFILL|SNM_RES_MASK_DBLCLIK));

	// -> add new resource types here + related enum on top of the .h

	// add saved bookmarks
	AddCustomTypesFromIniFile();

	// load general prefs
	g_resType = BOUNDED((int)GetPrivateProfileInt( // bounded for safety (some custom slot types may have been removed..)
		RES_INI_SEC, "Type", SNM_SLOT_FXC, g_SNM_IniFn.Get()), SNM_SLOT_FXC, g_SNM_ResSlots.GetSize()-1);

	g_filterPref = GetPrivateProfileInt(RES_INI_SEC, "Filter", 1, g_SNM_IniFn.Get());
	g_asFXChainPref = GetPrivateProfileInt(RES_INI_SEC, "AutoSaveFXChain", FXC_AUTOSAVE_PREF_TRACK, g_SNM_IniFn.Get());
	g_asFXChainNamePref = GetPrivateProfileInt(RES_INI_SEC, "AutoSaveFXChainName", 0, g_SNM_IniFn.Get());
	g_asTrTmpltPref = GetPrivateProfileInt(RES_INI_SEC, "AutoSaveTrTemplate", 3, g_SNM_IniFn.Get());
	g_addMedPref = GetPrivateProfileInt(RES_INI_SEC, "AddMediaFileOptions", 0, g_SNM_IniFn.Get());

	// auto-save, auto-fill directories, etc..
	g_autoSaveDirs.Empty(true);
	g_autoFillDirs.Empty(true);
	g_tiedProjects.Empty(true);
	memset(g_dblClickPrefs, 0, SNM_MAX_SLOT_TYPES*sizeof(int));
/*
	for (int i=0; i < SNM_NUM_DEFAULT_SLOTS; i++)
		g_tiedSlotActions[i]=i;
*/

	char defaultPath[SNM_MAX_PATH]="", path[SNM_MAX_PATH]="", iniSec[64]="", iniKey[64]="";
	for (int i=0; i < g_SNM_ResSlots.GetSize(); i++)
	{
		GetIniSectionName(i, iniSec, sizeof(iniSec));
		if (snprintfStrict(defaultPath, sizeof(defaultPath), "%s%c%s", GetResourcePath(), PATH_SLASH_CHAR, g_SNM_ResSlots.Get(i)->GetResourceDir()) < 0)
			*defaultPath = '\0';

		// g_autoFillDirs, g_autoSaveDirs and g_tiedProjects must always be filled
		// (even if auto-save is grayed for the user, etc..)
		WDL_FastString *fillPath, *savePath, *tiedPrj;

		savePath = new WDL_FastString(defaultPath);
		if (snprintfStrict(iniKey, sizeof(iniKey), "AutoSaveDir%s", iniSec) > 0) {
			GetPrivateProfileString(RES_INI_SEC, iniKey, defaultPath, path, sizeof(path), g_SNM_IniFn.Get());
			savePath->Set(path);
		}
		g_autoSaveDirs.Add(savePath);

		fillPath = new WDL_FastString(defaultPath);
		if (snprintfStrict(iniKey, sizeof(iniKey), "AutoFillDir%s", iniSec) > 0) {
			GetPrivateProfileString(RES_INI_SEC, iniKey, defaultPath, path, sizeof(path), g_SNM_IniFn.Get());
			fillPath->Set(path);
		}
		g_autoFillDirs.Add(fillPath);

		g_syncAutoDirPrefs[i] = true;
		if (snprintfStrict(iniKey, sizeof(iniKey), "SyncAutoDirs%s", iniSec) > 0)
			g_syncAutoDirPrefs[i] = (GetPrivateProfileInt(RES_INI_SEC, iniKey, 1, g_SNM_IniFn.Get()) == 1);
		if (g_syncAutoDirPrefs[i]) // consistency check (e.g. after sws upgrade)
			g_syncAutoDirPrefs[i] = (strcmp(savePath->Get(), fillPath->Get()) == 0);

		g_dblClickPrefs[i] = 0;
		if (g_SNM_ResSlots.Get(i)->IsDblClick() && snprintfStrict(iniKey, sizeof(iniKey), "DblClick%s", iniSec) > 0)
			g_dblClickPrefs[i] = LOWORD(GetPrivateProfileInt(RES_INI_SEC, iniKey, 0, g_SNM_IniFn.Get())); // LOWORD() for histrical reason..

		tiedPrj = new WDL_FastString;
		g_tiedProjects.Add(tiedPrj);

		// default type? (fx chains, track templates, etc...)
		if (i < SNM_NUM_DEFAULT_SLOTS)
		{
			// load tied actions
			g_tiedSlotActions[i] = i;
			if (snprintfStrict(iniKey, sizeof(iniKey), "TiedActions%s", iniSec) > 0)
				g_tiedSlotActions[i] = GetPrivateProfileInt(RES_INI_SEC, iniKey, i, g_SNM_IniFn.Get());
		}
		// bookmark, custom type?
		else
		{
			// load tied project
			if (snprintfStrict(iniKey, sizeof(iniKey), "TiedProject%s", iniSec) > 0) {
				GetPrivateProfileString(RES_INI_SEC, iniKey, "", path, sizeof(path), g_SNM_IniFn.Get());
				tiedPrj->Set(path);
			}
		}
	}

	// read slots from ini file
	char desc[128]="", maxSlotCount[16]="";
	for (int i=0; i < g_SNM_ResSlots.GetSize(); i++)
	{
		if (ResourceList* list = g_SNM_ResSlots.Get(i))
		{
			GetIniSectionName(i, iniSec, sizeof(iniSec));
			
			//JFB TODO? would be faster to read section in one go..
			GetPrivateProfileString(iniSec, "Max_slot", "0", maxSlotCount, sizeof(maxSlotCount), g_SNM_IniFn.Get()); 
			list->EmptySafe(true);
			int cnt = atoi(maxSlotCount);
			for (int j=0; j<cnt; j++) {
				ReadSlotIniFile(iniSec, j, path, sizeof(path), desc, sizeof(desc));
				list->Add(new ResourceItem(path, desc));
			}
		}
	}

	// instanciate the window if needed, can be NULL
	g_resWndMgr.Init();

	if (!plugin_register("projectconfig", &s_projectconfig))
		return 0;

	return 1;
}

void ResourcesExit()
{
	plugin_register("-projectconfig", &s_projectconfig);

	WDL_PtrList_DeleteOnDestroy<WDL_FastString> iniSections;
	GetIniSectionNames(&iniSections);

	// *** save the main ini section

	std::ostringstream iniSection;

	// save custom slot type definitions
	for (int i=0; i < g_SNM_ResSlots.GetSize(); i++)
	{
		if (i >= SNM_NUM_DEFAULT_SLOTS)
		{
			iniSection << iniSections.Get(i)->Get() << "=\""
			           << g_SNM_ResSlots.Get(i)->GetResourceDir() << ','
			           << g_SNM_ResSlots.Get(i)->GetName() << ','
			           << g_SNM_ResSlots.Get(i)->GetFileExtStr() << '"' << '\0';
		}
	}
	iniSection << "Type=" << g_resType << '\0';
	iniSection << "Filter=" << g_filterPref << '\0';

	// save slot type prefs
	for (int i=0; i < g_SNM_ResSlots.GetSize(); i++)
	{
		// save auto-fill & auto-save paths
		if (g_autoFillDirs.Get(i)->GetLength())
			iniSection << "AutoFillDir" << iniSections.Get(i)->Get() << "=\""
			           << g_autoFillDirs.Get(i)->Get() << '"' << '\0';

		if (g_autoSaveDirs.Get(i)->GetLength() && g_SNM_ResSlots.Get(i)->IsAutoSave())
			iniSection << "AutoSaveDir" << iniSections.Get(i)->Get() << "=\""
			           << g_autoSaveDirs.Get(i)->Get() << '"' << '\0';

		iniSection << "SyncAutoDirs" << iniSections.Get(i)->Get() << '='
		           << g_syncAutoDirPrefs[i] << '\0';

		// default type? (fx chains, track templates, etc...)
		if (i < SNM_NUM_DEFAULT_SLOTS)
		{
			// save tied slot actions
			iniSection << "TiedActions" << iniSections.Get(i)->Get() << '='
			           << g_tiedSlotActions[i] << '\0';
		}
		// bookmark, custom type?
		else
		{
			// save tied project
			if (g_tiedProjects.Get(i)->GetLength())
				iniSection << "TiedProject" << iniSections.Get(i)->Get() << "=\""
				           << g_tiedProjects.Get(i)->Get() << '"' << '\0';
		}

		// dbl-click pref
		if (g_SNM_ResSlots.Get(i)->IsDblClick())
			iniSection << "DblClick" << iniSections.Get(i)->Get() << '='
			           << g_dblClickPrefs[i] << '\0';

		// specific options (just for the ini file ordering..)
		switch (i)
		{
			case SNM_SLOT_FXC:
				iniSection << "AutoSaveFXChain=" << g_asFXChainPref << '\0';
				iniSection << "AutoSaveFXChainName=" << g_asFXChainNamePref << '\0';
				break;
			case SNM_SLOT_TR:
				iniSection << "AutoSaveTrTemplate=" << g_asTrTmpltPref << '\0';
				break;
			case SNM_SLOT_MEDIA:
				iniSection << "AddMediaFileOptions=" << g_addMedPref << '\0';
				break;
		}
	}
	SaveIniSection(RES_INI_SEC, iniSection.str(), g_SNM_IniFn.Get());


	// *** save slots ini sections

	for (int i=0; i < g_SNM_ResSlots.GetSize(); i++)
	{
		iniSection.str({});
		iniSection << "Max_slot=" << g_SNM_ResSlots.Get(i)->GetSize() << '\0';
		for (int j=0; j < g_SNM_ResSlots.Get(i)->GetSize(); j++)
		{
			if (ResourceItem* item = g_SNM_ResSlots.Get(i)->Get(j))
			{
				if (item->m_shortPath.GetLength())
					iniSection << "Slot" << (j+1) << "=\"" << item->m_shortPath.Get() << '"' << '\0';
				if (item->m_comment.GetLength())
					iniSection << "Desc" << (j+1) << "=\"" << item->m_comment.Get() << '"' << '\0';
			}
		}
		// write things in one go (avoid to slow down REAPER shutdown)
		SaveIniSection(iniSections.Get(i)->Get(), iniSection.str(), g_SNM_IniFn.Get());
	}

	g_resWndMgr.Delete();
}

void OpenResources(COMMAND_T* _ct)
{
	if (ResourcesWnd* w = g_resWndMgr.Create()) 
	{
		int newType = (int)_ct->user; // -1 means toggle current type
		if (newType == -1)
			newType = g_resType;
		w->Show(g_resType == newType /* i.e toggle */, true);
		w->SetType(newType);
//		SetFocus(GetDlgItem(w->GetHWND(), IDC_FILTER));
	}
}

int IsResourcesDisplayed(COMMAND_T* _ct) {
	if (ResourcesWnd* w = g_resWndMgr.Get())
		return w->IsWndVisible();
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
// ImageWnd
///////////////////////////////////////////////////////////////////////////////

enum {
  STRETCH_TO_FIT_MSG = 0xF000,
  IMGWND_LAST_MSG // keep as last item!
};

enum {
  IMGID=IMGWND_LAST_MSG
};

int g_lastImgSlot = -1;
bool g_stretchPref = false;
char g_lastImgFnPref[SNM_MAX_PATH] =  "";
SNM_WindowManager<ImageWnd> g_imgWndMgr(IMG_WND_ID);


// S&M windows lazy init: below's "" prevents registering the SWS' screenset callback
// (use the S&M one instead - already registered via SNM_WindowManager::Init())
ImageWnd::ImageWnd()
	: SWS_DockWnd(IDD_SNM_IMAGE, __LOCALIZE("Image","sws_DLG_162"), "")
{
	m_id.Set(IMG_WND_ID);
	m_stretch = g_stretchPref;
	m_img.SetImage(g_lastImgFnPref);

	// Must call SWS_DockWnd::Init() to restore parameters and open the window if necessary
	Init();
}

void ImageWnd::OnInitDlg()
{
	m_vwnd_painter.SetGSC(WDL_STYLE_GetSysColor);
	m_parentVwnd.SetRealParent(m_hwnd);
	
	m_img.SetID(IMGID);
	m_parentVwnd.AddChild(&m_img);
	RequestRedraw();
}

void ImageWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch(LOWORD(wParam))
	{
		case STRETCH_TO_FIT_MSG:
			m_stretch = !m_stretch;
			RequestRedraw();
			break;
		default:
			Main_OnCommand((int)wParam, (int)lParam);
			break;
	}
}

HMENU ImageWnd::OnContextMenu(int x, int y, bool* wantDefaultItems)
{
	HMENU hMenu = CreatePopupMenu();
	AddToMenu(hMenu, __LOCALIZE("Stretch to fit","sws_DLG_162"), STRETCH_TO_FIT_MSG, -1, false, m_stretch?MFS_CHECKED:MFS_UNCHECKED);
	return hMenu;
}

void ImageWnd::DrawControls(LICE_IBitmap* _bm, const RECT* _r, int* _tooltipHeight)
{
	int x0=_r->left+10, h=35;
	if (_tooltipHeight)
		*_tooltipHeight = h;

	m_img.SetVisible(true);
	if (!m_stretch)
	{
		int x = _r->left + int((_r->right-_r->left)/2 - m_img.GetWidth()/2 + 0.5);
		int y = _r->top + int((_r->bottom-_r->top)/2 - m_img.GetHeight()/2 + 0.5);
		RECT tr = {x, y, x+m_img.GetWidth(), y+m_img.GetHeight()};
		m_img.SetPosition(&tr);
		SNM_AddLogo(_bm, _r, x0, h);
	}
	else
		m_img.SetPosition(_r);
}

bool ImageWnd::GetToolTipString(int _xpos, int _ypos, char* _bufOut, int _bufOutSz)
{
	if (WDL_VWnd* v = m_parentVwnd.VirtWndFromPoint(_xpos,_ypos,1))
		if (v->GetID()==IMGID && g_lastImgSlot>=0 && *GetFilename())
			return (snprintfStrict(_bufOut, _bufOutSz, __LOCALIZE_VERFMT("Image slot %d: %s","sws_DLG_162"), g_lastImgSlot+1, GetFilename()) > 0);
	return false;
}

int ImageInit()
{
	// load prefs
	g_stretchPref = (GetPrivateProfileInt("ImageView", "Stretch", 0, g_SNM_IniFn.Get()) == 1);
	GetPrivateProfileString("ImageView", "LastImage", "", g_lastImgFnPref, sizeof(g_lastImgFnPref), g_SNM_IniFn.Get());

	// instanciate the window if needed, can be NULL
	g_imgWndMgr.Init();

	return 1;
}

void ImageExit()
{
	// save prefs
	if (ImageWnd* w = g_imgWndMgr.Get())
	{
		WritePrivateProfileString("ImageView", "Stretch", w->IsStretched()?"1":"0", g_SNM_IniFn.Get()); 

		const char* fn = w->GetFilename();
		if (*fn)
		{
			WDL_FastString str;
			str.SetFormatted(SNM_MAX_PATH, "\"%s\"", fn);
			WritePrivateProfileString("ImageView", "LastImage", str.Get(), g_SNM_IniFn.Get());
		}
		else
			WritePrivateProfileString("ImageView", "LastImage", NULL, g_SNM_IniFn.Get());
	}

	g_imgWndMgr.Delete();
}

void OpenImageWnd(COMMAND_T* _ct)
{
	bool isNew;
	if (ImageWnd* w = g_imgWndMgr.Create(&isNew))
	{
		if (isNew) {
			w->SetStretch(g_stretchPref);
			w->SetImage(g_lastImgFnPref);
		}
		w->Show(true, true);
		w->RequestRedraw();
	}
}

bool OpenImageWnd(const char* _fn)
{
	bool isNew, ok = false;
	if (ImageWnd* w = g_imgWndMgr.Create(&isNew))
	{
		if (isNew) {
			w->SetStretch(g_stretchPref);
			w->SetImage(g_lastImgFnPref);
		}
		ok = true;
		WDL_FastString prevFn(w->GetFilename());
		w->SetImage(_fn && *_fn ? _fn : NULL); // NULL clears the current image
		w->Show(!strcmp(prevFn.Get(), _fn) /* i.e toggle */, true);
		w->RequestRedraw();
	}
	return ok;
}

void ClearImageWnd(COMMAND_T*) {
	if (ImageWnd* w = g_imgWndMgr.Get()) {
		w->SetImage(NULL);
		w->RequestRedraw();
	}
}

int IsImageWndDisplayed(COMMAND_T*)
{
	if (ImageWnd* w = g_imgWndMgr.Get())
		return w->IsWndVisible();
	return 0;
}


///////////////////////////////////////////////////////////////////////////////
// Slot actions
///////////////////////////////////////////////////////////////////////////////

void ResourcesDeleteAllSlots(COMMAND_T* _ct) {
	ClearDeleteSlotsFiles(g_tiedSlotActions[(int)_ct->user], 8);
}

// _mode: see ClearDeleteSlotsFiles()
// _slot: 0-based slot index or -1 to prompt, or -2 for last slot
void ResourcesClearDeleteSlotsFiles(COMMAND_T* _ct, int _type, int _mode, int _slot)
{
	if (_type>=0 && _type<SNM_NUM_DEFAULT_SLOTS)
	{
		if (ResourceList* fl = g_SNM_ResSlots.Get(g_tiedSlotActions[_type]))
		{
/*JFB commented: not macro-friendly
			if (!fl->GetSize())
			{
				WDL_FastString msg;
				msg.SetFormatted(128, NO_SLOT_ERR_STR, fl->GetDesc());
				MessageBox(GetMainHwnd(), msg.Get(), __LOCALIZE("S&M - Error","sws_DLG_150"), MB_OK);
				return;
			}
*/
			switch (_slot)
			{
				case -1:
					_slot = PromptForInteger( // loops on err
						_ct ? SWS_CMD_SHORTNAME(_ct) : __LOCALIZE("S&M - Enter resource slot","sws_DLG_150"),
						__LOCALIZE("Slot","sws_DLG_150"),
						1,	fl->GetSize());
					break;
				case -2:
					_slot = fl->GetSize()-1;
					break;
			}

			if (_slot>=0 && _slot<fl->GetSize()) // <0: no slot, ot user cancelled prompt
				ClearDeleteSlotsFiles(g_tiedSlotActions[_type], _mode, _slot); // incl. UI update + detach file from project if needed
		}
	}
}

void ResourcesClearSlotPrompt(COMMAND_T* _ct) {
	ResourcesClearDeleteSlotsFiles(_ct, (int)_ct->user, 2, -1); 
}

void ResourcesDeleteLastSlot(COMMAND_T* _ct) {
	ResourcesClearDeleteSlotsFiles(_ct, (int)_ct->user, 1|4, -2); // 1|4: delete slot & file
}

void ResourcesClearFXChainSlot(COMMAND_T* _ct) {
	ResourcesClearDeleteSlotsFiles(_ct, SNM_SLOT_FXC, 2, (int)_ct->user);
}

void ResourcesClearTrTemplateSlot(COMMAND_T* _ct) {
	ResourcesClearDeleteSlotsFiles(_ct, SNM_SLOT_TR, 2, (int)_ct->user);
}

void ResourcesClearPrjTemplateSlot(COMMAND_T* _ct) {
	ResourcesClearDeleteSlotsFiles(_ct, SNM_SLOT_PRJ, 2, (int)_ct->user);
}

void ResourcesClearMediaSlot(COMMAND_T* _ct) {
	ResourcesClearDeleteSlotsFiles(_ct, SNM_SLOT_MEDIA, 2, (int)_ct->user);
	//JFB TODO? stop sound if needed?
}

void ResourcesClearImageSlot(COMMAND_T* _ct) {
	ResourcesClearDeleteSlotsFiles(_ct, SNM_SLOT_IMG, 2, (int)_ct->user);
}

void ResourcesClearThemeSlot(COMMAND_T* _ct) {
	ResourcesClearDeleteSlotsFiles(_ct, SNM_SLOT_THM, 2, (int)_ct->user);
}


// specific auto-save for fx chains
void ResourcesAutoSaveFXChain(COMMAND_T* _ct) {
	AutoSave(g_tiedSlotActions[SNM_SLOT_FXC], false, (int)_ct->user);
}

// specific auto-save for track templates
void ResourcesAutoSaveTrTemplate(COMMAND_T* _ct) {
	AutoSave(g_tiedSlotActions[SNM_SLOT_TR], false, (int)_ct->user);
}

// auto-save for all other types..
void ResourcesAutoSave(COMMAND_T* _ct)
{
	int type = g_tiedSlotActions[(int)_ct->user];
	if (type>=0 && type<g_SNM_ResSlots.GetSize())
		if (g_SNM_ResSlots.Get(type)->IsAutoSave())
			AutoSave(type, false, 0);
}


///////////////////////////////////////////////////////////////////////////////
// Track FX chain slots
///////////////////////////////////////////////////////////////////////////////

// _set=false => paste
void ApplyTracksFXChainSlot(int _slotType, const char* _title, int _slot, bool _set, bool _inputFX)
{
	WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot);
	if (fnStr && SNM_CountSelectedTracks(NULL, true))
	{
		WDL_FastString chain;
		if (LoadChunk(fnStr->Get(), &chain))
		{
			// remove all fx param envelopes
			// (were saved for track fx chains before SWS v2.1.0 #11)
			{
				SNM_ChunkParserPatcher p(&chain);
				p.RemoveSubChunk("PARMENV", 1, -1);
			}
			if (_set) SetTrackFXChain(_title, &chain, _inputFX);
			else  PasteTrackFXChain(_title, &chain, _inputFX);
		}
		delete fnStr;
	}
}

bool AutoSaveTrackFXChainSlots(int _slotType, const char* _dirPath, WDL_PtrList<ResourceItem>* _owSlots, bool _nameFromFx, bool _inputFX)
{
	bool saved = false;
	int owIdx = 0;
	for (int i=0; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i,false); 
		if (tr && *(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			WDL_FastString fxChain("");
			if (CopyTrackFXChain(&fxChain, _inputFX, i) == i)
			{
				RemoveAllIds(&fxChain);

				// add track channels as a comment (so that it does not bother REAPER)
				// i.e. best effort for http://github.com/reaper-oss/sws/issues/363
				WDL_FastString nbChStr;
				nbChStr.SetFormatted(32, "#NCHAN %d\n", *(int*)GetSetMediaTrackInfo(tr, "I_NCHAN", NULL));
				fxChain.Insert(nbChStr.Get(), 0);

				char name[SNM_MAX_FX_NAME_LEN] = "";
				if (_nameFromFx)
					TrackFX_GetFXName(tr, _inputFX ? 0x1000000 : 0, name, SNM_MAX_FX_NAME_LEN);
				else if (char* trName = (char*)GetSetMediaTrackInfo(tr, "P_NAME", NULL))
					lstrcpyn(name, trName, SNM_MAX_FX_NAME_LEN);

				saved |= AutoSaveSlot(_slotType, _dirPath, 
							!i ? __LOCALIZE("Master","sws_DLG_150") : (!*name ? __LOCALIZE("Untitled","sws_DLG_150") : name), 
							"RfxChain", _owSlots, &owIdx, AutoSaveChunkSlot, &fxChain);
			}
		}
	}
	return saved;
}

void LoadSetTrackFXChainSlot(COMMAND_T* _ct) {
	ApplyTracksFXChainSlot(g_tiedSlotActions[SNM_SLOT_FXC], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true, false);
}

void LoadPasteTrackFXChainSlot(COMMAND_T* _ct) {
	ApplyTracksFXChainSlot(g_tiedSlotActions[SNM_SLOT_FXC], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, false);
}

void LoadSetTrackInFXChainSlot(COMMAND_T* _ct) {
	ApplyTracksFXChainSlot(g_tiedSlotActions[SNM_SLOT_FXC], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true, true);
}

void LoadPasteTrackInFXChainSlot(COMMAND_T* _ct) {
	ApplyTracksFXChainSlot(g_tiedSlotActions[SNM_SLOT_FXC], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, true);
}


///////////////////////////////////////////////////////////////////////////////
// Take FX chain slots
///////////////////////////////////////////////////////////////////////////////

// _slot: 0-based, -1 will prompt for slot
// _set=false: paste, _set=true: set
void ApplyTakesFXChainSlot(int _slotType, const char* _title, int _slot, bool _activeOnly, bool _set)
{
	WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot);
	if (fnStr && CountSelectedMediaItems(NULL))
	{
		WDL_FastString chain;
		if (LoadChunk(fnStr->Get(), &chain))
		{
			// remove all fx param envelopes
			// (were saved for track fx chains before SWS v2.1.0 #11)
			{
				SNM_ChunkParserPatcher p(&chain);
				p.RemoveSubChunk("PARMENV", 1, -1);
			}
			if (_set) SetTakeFXChain(_title, &chain, _activeOnly);
			else PasteTakeFXChain(_title, &chain, _activeOnly);
		}
		delete fnStr;
	}
}

bool AutoSaveItemFXChainSlots(int _slotType, const char* _dirPath, WDL_PtrList<ResourceItem>* _owSlots, bool _nameFromFx)
{
	bool saved = false;
	int owIdx = 0;
	WDL_PtrList<MediaItem> items;
	SNM_GetSelectedItems(NULL, &items);
	for (int i=0; i < items.GetSize(); i++)
	{
		if (MediaItem* item = items.Get(i))
		{
			WDL_FastString fxChain("");
			if (CopyTakeFXChain(&fxChain, i) == i)
			{
				RemoveAllIds(&fxChain);

				char name[SNM_MAX_FX_NAME_LEN] = "";
				if (_nameFromFx)
				{
					SNM_FXSummaryParser p(&fxChain);
					WDL_PtrList<SNM_FXSummary>* summaries = p.GetSummaries();
					SNM_FXSummary* sum = summaries ? summaries->Get(0) : NULL;
					if (sum) lstrcpyn(name, sum->m_name.Get(), SNM_MAX_FX_NAME_LEN);
				}
				else if (GetName(item))
					lstrcpyn(name, GetName(item), SNM_MAX_FX_NAME_LEN);

				saved |= AutoSaveSlot(_slotType, _dirPath, 
							!*name ? __LOCALIZE("Untitled","sws_DLG_150") : name, 
							"RfxChain", _owSlots, &owIdx, AutoSaveChunkSlot, &fxChain);
			}
		}
	}
	return saved;
}

void LoadSetTakeFXChainSlot(COMMAND_T* _ct) {
	ApplyTakesFXChainSlot(g_tiedSlotActions[SNM_SLOT_FXC], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true, true);
}

void LoadPasteTakeFXChainSlot(COMMAND_T* _ct) {
	ApplyTakesFXChainSlot(g_tiedSlotActions[SNM_SLOT_FXC], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true, false);
}

void LoadSetAllTakesFXChainSlot(COMMAND_T* _ct) {
	ApplyTakesFXChainSlot(g_tiedSlotActions[SNM_SLOT_FXC], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, true);
}

void LoadPasteAllTakesFXChainSlot(COMMAND_T* _ct) {
	ApplyTakesFXChainSlot(g_tiedSlotActions[SNM_SLOT_FXC], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, false);
}


///////////////////////////////////////////////////////////////////////////////
// Track templates slots
///////////////////////////////////////////////////////////////////////////////

void ImportTrackTemplateSlot(int _slotType, const char* _title, int _slot)
{
	if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot))
	{
		Undo_BeginBlock2(NULL); // the API Main_openProject() includes an undo point
		Main_openProject((char*)fnStr->Get());
		Undo_EndBlock2(NULL, _title, UNDO_STATE_ALL);
		delete fnStr;
	}
}

// supports multi-selection & multiple tracks in the template file
// - when there are more selected tracks than tracks present in the file => cycle to 1st track in file
// - restoring routings does not make sense when "applying" though (track selection won't probably match),
//   it only makes sense when "importing"
void ApplyTrackTemplateSlot(int _slotType, const char* _title, int _slot, bool _itemsFromTmplt, bool _envsFromTmplt)
{
	bool updated = false;
	if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot))
	{
		WDL_FastString tmpltFile;
		if (SNM_CountSelectedTracks(NULL, true) && LoadChunk(fnStr->Get(), &tmpltFile) && tmpltFile.GetLength())
		{
			int tmpltIdx=0, tmpltIdxMax=0xFFFFFF; // trick to avoid useless calls to MakeSingleTrackTemplateChunk()
			WDL_PtrList_DeleteOnDestroy<WDL_FastString> tmplts; // cache
			for (int i=0; i <= GetNumTracks(); i++) // incl. master
				if (MediaTrack* tr = CSurf_TrackFromID(i, false))
					if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
					{
						WDL_FastString* tmplt = tmplts.Get(tmpltIdx);
						if (!tmplt) // lazy init of the cache..
						{
							tmplt = new WDL_FastString;
							if (tmpltIdx<tmpltIdxMax && MakeSingleTrackTemplateChunk(&tmpltFile, tmplt, !_itemsFromTmplt, !_envsFromTmplt, tmpltIdx))
							{
								tmplts.Add(tmplt);
							}
							else
							{
								delete tmplt;
								if (tmplts.Get(0)) { // cycle?
									tmplt = tmplts.Get(0);
									tmpltIdxMax = tmpltIdx;
									tmpltIdx = 0;
								}
								else { // invalid file!
									delete fnStr;
									return;
								}
							}
						}
						updated |= ApplyTrackTemplate(tr, tmplt, _itemsFromTmplt, _envsFromTmplt);
						tmpltIdx++;
					}
		}
		delete fnStr;
	}
	if (updated && _title)
		Undo_OnStateChangeEx2(NULL, _title, UNDO_STATE_ALL, -1);
}

void LoadApplyTrackTemplateSlot(COMMAND_T* _ct) {
	ApplyTrackTemplateSlot(g_tiedSlotActions[SNM_SLOT_TR], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, false);
}

void LoadApplyTrackTemplateSlotWithItemsEnvs(COMMAND_T* _ct) {
	ApplyTrackTemplateSlot(g_tiedSlotActions[SNM_SLOT_TR], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true, true);
}

void LoadImportTrackTemplateSlot(COMMAND_T* _ct) {
	ImportTrackTemplateSlot(g_tiedSlotActions[SNM_SLOT_TR], SWS_CMD_SHORTNAME(_ct), (int)_ct->user);
}

void ReplacePasteItemsTrackTemplateSlot(int _slotType, const char* _title, int _slot, bool _paste)
{
	bool updated = false;
	if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot))
	{
		WDL_FastString tmpltFile;
		if (CountSelectedTracks(NULL) && LoadChunk(fnStr->Get(), &tmpltFile) && tmpltFile.GetLength())
		{
			int tmpltIdx=0, tmpltIdxMax=0xFFFFFF; // trick to avoid useless calls to GetItemsSubChunk()
			WDL_PtrList_DeleteOnDestroy<WDL_FastString> tmplts; // cache
			for (int i=1; i <= GetNumTracks(); i++) // skip master
				if (MediaTrack* tr = CSurf_TrackFromID(i, false))
					if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
					{
						WDL_FastString* tmpltItems = tmplts.Get(tmpltIdx);
						if (!tmpltItems) // lazy init of the cache..
						{
							tmpltItems = new WDL_FastString;
							if (tmpltIdx<tmpltIdxMax && GetItemsSubChunk(&tmpltFile, tmpltItems, tmpltIdx)) {
								tmplts.Add(tmpltItems);
							}
							else
							{
								delete tmpltItems;
								if (tmplts.Get(0)) { // cycle?
									tmpltItems = tmplts.Get(0);
									tmpltIdxMax = tmpltIdx;
									tmpltIdx = 0;
								}
								else { // invalid file!
									delete fnStr;
									return;
								}
							}
						}
						updated |= ReplacePasteItemsFromTrackTemplate(tr, tmpltItems, _paste);
						tmpltIdx++;
					}
		}
		delete fnStr;
	}
	if (updated && _title)
		Undo_OnStateChangeEx2(NULL, _title, UNDO_STATE_ALL, -1);
}

void ReplaceItemsTrackTemplateSlot(COMMAND_T* _ct) {
	ReplacePasteItemsTrackTemplateSlot(g_tiedSlotActions[SNM_SLOT_TR], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false);
}

void PasteItemsTrackTemplateSlot(COMMAND_T* _ct) {
	ReplacePasteItemsTrackTemplateSlot(g_tiedSlotActions[SNM_SLOT_TR], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true);
}

bool AutoSaveTrackSlots(int _slotType, const char* _dirPath, WDL_PtrList<ResourceItem>* _owSlots, bool _delItems, bool _delEnvs)
{
	int owIdx = 0;
	WDL_FastString fullChunk;
	SaveSelTrackTemplates(_delItems, _delEnvs, &fullChunk);
	if (fullChunk.GetLength())
	{
		// get the 1st valid name
		int i; char name[256] = "";
		for (i=0; i <= GetNumTracks(); i++) // incl. master
			if (MediaTrack* tr = CSurf_TrackFromID(i, false))
				if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL)) {
					if (char* trName = (char*)GetSetMediaTrackInfo(tr, "P_NAME", NULL))
						lstrcpyn(name, trName, sizeof(name));
					break;
				}

		return AutoSaveSlot(_slotType, _dirPath, 
					!i ? __LOCALIZE("Master","sws_DLG_150") : (!*name ? __LOCALIZE("Untitled","sws_DLG_150") : name),
					"RTrackTemplate", _owSlots, &owIdx, AutoSaveChunkSlot, &fullChunk);
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// Project template slots
///////////////////////////////////////////////////////////////////////////////

// undo does not make sense here: _title ignored
void LoadOrSelectProjectSlot(int _slotType, const char* _title, int _slot, bool _newTab)
{
	if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot)) {
		LoadOrSelectProject(fnStr->Get(), _newTab);
		delete fnStr;
	}
}

bool AutoSaveProjectSlot(int _slotType, const char* _dirPath, WDL_PtrList<ResourceItem>* _owSlots, bool _saveCurPrj)
{
	if (_saveCurPrj)
		Main_OnCommand(40026,0);

	int owIdx = 0;
	char prjFn[SNM_MAX_PATH] = "";
	EnumProjects(-1, prjFn, sizeof(prjFn));
	return AutoSaveSlot(_slotType, _dirPath, prjFn, "RPP", _owSlots, &owIdx);
}

void LoadOrSelectProjectSlot(COMMAND_T* _ct) {
	LoadOrSelectProjectSlot(g_tiedSlotActions[SNM_SLOT_PRJ], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false);
}

void LoadOrSelectProjectTabSlot(COMMAND_T* _ct) {
	LoadOrSelectProjectSlot(g_tiedSlotActions[SNM_SLOT_PRJ], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true);
}

void LoadOrSelectNextPreviousProjectSlot(const char* _title, int _dir, bool _newtab)
{
	int slotType = g_tiedSlotActions[SNM_SLOT_PRJ];
	ResourceList* slots = g_SNM_ResSlots.Get(slotType);
	if (!slots)
		return;

	int startSlot = 1;
	int endSlot = slots->GetSize();

	{
		int cpt=0, slotCount = endSlot-startSlot+1;

		// (try to) find the current project in the slot range defined in the prefs
		if (g_prjCurSlot < 0)
		{
			char pCurPrj[SNM_MAX_PATH] = "";
			EnumProjects(-1, pCurPrj, sizeof(pCurPrj));
			if (*pCurPrj)
				for (int i=startSlot-1; g_prjCurSlot < 0 && i < endSlot; i++)
					if (!slots->Get(i)->IsDefault() && strstr(pCurPrj, slots->Get(i)->m_shortPath.Get()))
						g_prjCurSlot = i;
		}

		if (g_prjCurSlot < 0) // not found => default init
			g_prjCurSlot = (_dir > 0 ? startSlot-2 : endSlot); // why not ? startSlot-1 : endSlot-1);

		// the meat
		do
		{
			if ((_dir > 0 && (g_prjCurSlot+_dir) > (endSlot-1)) ||	
				(_dir < 0 && (g_prjCurSlot+_dir) < (startSlot-1)))
			{
				g_prjCurSlot = (_dir > 0 ? startSlot-1 : endSlot-1);
			}
			else g_prjCurSlot += _dir;
		}
		while (++cpt <= slotCount && (
			// RPP only! (e.g. a RPP-bak file would be opened as "unnamed")
			!HasFileExtension(slots->Get(g_prjCurSlot)->m_shortPath.Get(), "RPP")
/* the above indirectly exclude empty slots too...
			|| slots->Get(g_prjCurSlot)->IsDefault()
*/
			));

		// found one?
		if (cpt <= slotCount)
			LoadOrSelectProjectSlot(slotType, _title, g_prjCurSlot, _newtab);
		else
			g_prjCurSlot = -1;

		if (g_resType==slotType)
			ResourcesUpdate(); // for the list view's bullet
	}
}

void LoadOrSelectNextPreviousProjectSlot(COMMAND_T* _ct) {
	LoadOrSelectNextPreviousProjectSlot(SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false);
}

void LoadOrSelectNextPreviousProjectTabSlot(COMMAND_T* _ct) {
	LoadOrSelectNextPreviousProjectSlot(SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true);
}


///////////////////////////////////////////////////////////////////////////////
// Media file slots
///////////////////////////////////////////////////////////////////////////////

// undo does not make sense here: _title ignored
void PlaySelTrackMediaSlot(int _slotType, const char* _title, int _slot, bool _pause, bool _loop, double _msi) {
	if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot)) {
		SNM_PlaySelTrackPreviews(fnStr->Get(), _pause, _loop, _msi);
		delete fnStr;
	}
}

void PlaySelTrackMediaSlot(COMMAND_T* _ct) {
	PlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, false, -1.0);
}

void LoopSelTrackMediaSlot(COMMAND_T* _ct) {
	PlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, true, -1.0);
}

void SyncPlaySelTrackMediaSlot(COMMAND_T* _ct) {
	PlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, false, 1.0);
}

void SyncLoopSelTrackMediaSlot(COMMAND_T* _ct) {
	PlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, true, 1.0);
}

// undo does not make sense here: _title ignored
bool TogglePlaySelTrackMediaSlot(int _slotType, const char* _title, int _slot, bool _pause, bool _loop, double _msi)
{
	bool done = false;
	if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot)) {
		done = SNM_TogglePlaySelTrackPreviews(fnStr->Get(), _pause, _loop, _msi);
		delete fnStr;
	}
	return done;
}

// no sync
void TogglePlaySelTrackMediaSlot(COMMAND_T* _ct) {
	TogglePlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, false);
}

void ToggleLoopSelTrackMediaSlot(COMMAND_T* _ct) {
	TogglePlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, true);
}

void TogglePauseSelTrackMediaSlot(COMMAND_T* _ct) {
	TogglePlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true, false);
}

void ToggleLoopPauseSelTrackMediaSlot(COMMAND_T* _ct) {
	TogglePlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true, true);
}

// with sync
#ifdef _SNM_MISC
void SyncTogglePlaySelTrackMediaSlot(COMMAND_T* _ct) {
	TogglePlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, false, 1.0);
}

void SyncToggleLoopSelTrackMediaSlot(COMMAND_T* _ct) {
	TogglePlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, false, true, 1.0);
}

void SyncTogglePauseSelTrackMediaSlot(COMMAND_T* _ct) {
	TogglePlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true, false, 1.0);
}

void SyncToggleLoopPauseSelTrackMediaSlot(COMMAND_T* _ct) {
	TogglePlaySelTrackMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, true, true, 1.0);
}
#endif

void AddMediaOptionName(WDL_FastString* _name)
{
	if (_name && g_addMedPref)
	{
		_name->Append(" (");
		switch (g_addMedPref)
		{
			case 4:  _name->Append(__LOCALIZE("fit time selection","sws_DLG_150")); break;
			case 16: _name->Append(__LOCALIZE("tempo match 0.5x","sws_DLG_150")); break;
			case 8:  _name->Append(__LOCALIZE("tempo match 1x","sws_DLG_150")); break;
			case 32: _name->Append(__LOCALIZE("tempo match 2x","sws_DLG_150")); break;
		}
		_name->Append(")");
	}
}

void SetMediaOption(int _opt)
{
	switch (_opt)
	{
		case 1: g_addMedPref = 4; break;	// stretch/loop to fit time sel
		case 2: g_addMedPref = 16; break;	// try to match tempo 0.5x
		case 3: g_addMedPref = 8; break;	// try to match tempo 1x
		case 4: g_addMedPref = 32; break;	// try to match tempo 2x
		case 0: default: g_addMedPref = 0; break;
	}

	if (ResourcesWnd* w = g_resWndMgr.Get()) {
		w->FillDblClickCombo();
		w->Update();
	}

	char custId[SNM_MAX_ACTION_CUSTID_LEN];
	for (int i=0; i <= (MED_OPT_END_MSG-MED_OPT_START_MSG); i++)
		if (snprintfStrict(custId, sizeof(custId), "_S&M_ADDMEDIA_OPT%d", i) > 0)
			RefreshToolbar(NamedCommandLookup(custId));
}

void SetMediaOption(COMMAND_T* _ct) {
	SetMediaOption((int)_ct->user);
}

int IsMediaOption(int _opt)
{
	switch (_opt)
	{
		case 0: if (g_addMedPref == 0) return true; break;
		case 1: if (g_addMedPref == 4) return true; break;
		case 2: if (g_addMedPref == 16) return true; break;
		case 3: if (g_addMedPref == 8) return true; break;
		case 4: if (g_addMedPref == 32) return true; break;
	}
	return false;
}

int IsMediaOption(COMMAND_T* _ct) {
	return IsMediaOption((int)_ct->user);
}

// _insertMode: 0=add to current track, 1=add new track, 3=add to selected items as takes, 
//             &4=stretch/loop to fit time sel, &8=try to match tempo 1x, 
//             &16=try to match tempo 0.5x, &32=try to match tempo 2x
void InsertMediaSlot(int _slotType, const char* _title, int _slot, int _insertMode)
{
	if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot))
	{
		Undo_BeginBlock2(NULL); // the API InsertMedia() includes an undo point
		InsertMedia((char*)fnStr->Get(), _insertMode|g_addMedPref);
		Undo_EndBlock2(NULL, _title, UNDO_STATE_ALL);
		delete fnStr;
	}
}

void InsertMediaSlotCurTr(COMMAND_T* _ct) {
	InsertMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, 0);
}

void InsertMediaSlotNewTr(COMMAND_T* _ct) {
	InsertMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, 1);
}

void InsertMediaSlotTakes(COMMAND_T* _ct) {
	InsertMediaSlot(g_tiedSlotActions[SNM_SLOT_MEDIA], SWS_CMD_SHORTNAME(_ct), (int)_ct->user, 3);
}

typedef struct {
	PCM_source* src;
	double start_time;
	double length;
	double playrate;
} PCM_SourceSlice;

//JFB!! TODO: PCM_Sink
bool AutoSaveMediaSlot(const void* _obj, const char* _fn)
{
	PCM_SourceSlice* slice = (PCM_SourceSlice*)_obj;
	if (_fn && slice && slice->src)
	{
		return RenderFileSection(
				slice->src->GetFileName(), _fn, 
				slice->start_time / slice->src->GetLength(), 
				(slice->start_time+slice->length) / slice->src->GetLength(),
				slice->playrate);
	}
	return false;
}

bool AutoSaveInProjectMediaSlot(const void* _obj, const char* _fn)
{
	PCM_SourceSlice* slice = (PCM_SourceSlice*)_obj;
	if (_fn && slice && slice->src)
		return slice->src->Extended(PCM_SOURCE_EXT_EXPORTTOFILE, (void*)_fn, NULL, NULL) ? true: false; // warning C4800
	return false;
}

bool AutoSaveMediaSlots(int _slotType, const char* _dirPath, WDL_PtrList<ResourceItem>* _owSlots)
{
	int owIdx = 0;
	bool saved = false;
	PCM_SourceSlice slice;
	for (int i=1; i <= GetNumTracks(); i++) // skip master
		if (MediaTrack* tr = CSurf_TrackFromID(i, false))
			for (int j=0; j < GetTrackNumMediaItems(tr); j++)
				if (MediaItem* item = GetTrackMediaItem(tr,j))
					if (*(bool*)GetSetMediaItemInfo(item, "B_UISEL", NULL))
						if (MediaItem_Take* tk = GetActiveTake(item))
							if (PCM_source* src = (PCM_source*)GetSetMediaItemTakeInfo(tk, "P_SOURCE", NULL))
							{
								slice.src = src;
								slice.start_time = *(double*)GetSetMediaItemTakeInfo(tk, "D_STARTOFFS", NULL);
								slice.length = *(double*)GetSetMediaItemInfo(item, "D_LENGTH", NULL);
								slice.playrate = *(double*)GetSetMediaItemTakeInfo(tk, "D_PLAYRATE", NULL);

								// section?
								if (!src->GetFileName() && src->GetSource())
								{
									double offs, len;
									PCM_Source_GetSectionInfo(src, &offs, &len, NULL);
									slice.length = min(slice.length, fabs(len- slice.start_time));
									slice.start_time += offs;
									slice.src = src = src->GetSource();
								}

								if (src->GetFileName())
								{
									if(*src->GetFileName()) // ext file (.mid is supported too)
										saved |= AutoSaveSlot(_slotType, _dirPath, src->GetFileName(), GetFileExtension(src->GetFileName()), _owSlots, &owIdx, AutoSaveMediaSlot, &slice);
									else // in-project midi
										saved |= AutoSaveSlot(_slotType, _dirPath, (const char*)GetSetMediaItemTakeInfo(tk, "P_NAME", NULL), "mid", _owSlots, &owIdx, AutoSaveInProjectMediaSlot, &slice);
								}
							}
	return saved;
}


///////////////////////////////////////////////////////////////////////////////
// Image slots
///////////////////////////////////////////////////////////////////////////////

// undo does not make sense here: _title ignored
void ShowImageSlot(int _slotType, const char* _title, int _slot)
{
	if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot))
	{
		if (!_stricmp("png", GetFileExtension(fnStr->Get())))
		{
			if (OpenImageWnd(fnStr->Get()))
				g_lastImgSlot = _slot;
		}
		else
		{
			WDL_FastString msg;
			msg.SetFormatted(256, __LOCALIZE_VERFMT("Cannot load %s","sws_mbox"), fnStr->Get());
			msg.Append("\n");
			msg.Append(__LOCALIZE("Only PNG files are supported at the moment, sorry.","sws_mbox")); //JFB!!
			MessageBox(GetMainHwnd(), msg.Get(), __LOCALIZE("S&M - Error","sws_mbox"), MB_OK);
		}
		delete fnStr;
	}
}

void ShowImageSlot(COMMAND_T* _ct) {
	ShowImageSlot(g_tiedSlotActions[SNM_SLOT_IMG], SWS_CMD_SHORTNAME(_ct), (int)_ct->user);
}

void ShowNextPreviousImageSlot(COMMAND_T* _ct)
{
	int sz = g_SNM_ResSlots.Get(g_tiedSlotActions[SNM_SLOT_IMG])->GetSize();
	if (sz) {
		g_lastImgSlot += (int)_ct->user;
		if (g_lastImgSlot<0) g_lastImgSlot = sz-1;
		else if (g_lastImgSlot>=sz) g_lastImgSlot = 0;
	}
	ShowImageSlot(g_tiedSlotActions[SNM_SLOT_IMG], SWS_CMD_SHORTNAME(_ct), sz ? g_lastImgSlot : -1); // -1: err msg (empty list)
}

void SetSelTrackIconSlot(int _slotType, const char* _title, int _slot)
{
	bool updated = false;

	PreventUIRefresh(1);
	if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot))
	{
		for (int j=0; j <= GetNumTracks(); j++)
			if (MediaTrack* tr = CSurf_TrackFromID(j, false))
				if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
					updated |= !!GetSetMediaTrackInfo(tr, "P_ICON", (void*)fnStr->Get());
		delete fnStr;
	}
	PreventUIRefresh(-1);

	if (updated && _title)
		Undo_OnStateChangeEx2(NULL, _title, UNDO_STATE_TRACKCFG, -1);
}

void SetSelTrackIconSlot(COMMAND_T* _ct) {
	SetSelTrackIconSlot(g_tiedSlotActions[SNM_SLOT_IMG], SWS_CMD_SHORTNAME(_ct), (int)_ct->user);
}


///////////////////////////////////////////////////////////////////////////////
// Theme slots
///////////////////////////////////////////////////////////////////////////////

// undo does not make sense here: _title ignored
void LoadThemeSlot(int _slotType, const char* _title, int _slot)
{
	if (WDL_FastString* fnStr = GetOrPromptOrBrowseSlot(_slotType, &_slot))
	{
		OnColorThemeOpenFile(fnStr->Get());
		delete fnStr;
	}
}

void LoadThemeSlot(COMMAND_T* _ct) {
	LoadThemeSlot(g_tiedSlotActions[SNM_SLOT_THM], SWS_CMD_SHORTNAME(_ct), (int)_ct->user);
}


///////////////////////////////////////////////////////////////////////////////
// ReaScript export
///////////////////////////////////////////////////////////////////////////////

int SNM_SelectResourceBookmark(const char* _name)
{
	if (ResourcesWnd* w = g_resWndMgr.Get())
		return w->SetType(_name);
	return -1;
}

void SNM_TieResourceSlotActions(int _bookmarkId)
{
	int typeForUser = _bookmarkId>=0 ? GetTypeForUser(_bookmarkId) : -1;
	if (typeForUser>=0 && typeForUser<SNM_NUM_DEFAULT_SLOTS)
	{
		g_tiedSlotActions[typeForUser] = _bookmarkId;

		// update [x] tag
		if (ResourcesWnd* w = g_resWndMgr.Get()) {
			w->FillTypeCombo();
			w->Update();
		}
	}
}
