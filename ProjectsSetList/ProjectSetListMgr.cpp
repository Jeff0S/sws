/******************************************************************************
/ ProjectSetListMgr.cpp
/
/ Copyright (c) 2020 RS
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

#include "stdafx.h"
#include "../reaper/localize.h"
#include "../SnM/SnM_Dlg.h"
#include "ProjectSetListMgr.h"
#include "ProjectSetList.h"

// Globals
WDL_PtrList<SWS_SongItem> g_SongItems;
string g_setListNameLoadedFromFileSystem;

// ****************************************************************************
// **** Project Set list  *****************************************************
// ****************************************************************************
void SaveProjectSetList(COMMAND_T*)
{
	char filename[MAX_PATH]{};

	if (g_SongItems.GetSize() == 0) {
		MessageBox(g_hwndParent, __LOCALIZE("Set List is empty.", "sws_mbox"), __LOCALIZE("SWS Project List Save", "sws_mbox"), MB_OK);
		ProjectSetListUpdate();
		return;
	}

	char cPath[MAX_PATH];
	GetProjectPath(cPath, sizeof(cPath));
	if (!BrowseForSaveFile(__LOCALIZE("Select project list", "sws_mbox"), cPath, nullptr, "Reaper Project List (*.RPL)\0*.RPL\0All Files\0*.*\0", filename, sizeof(filename))) {
		ProjectSetListUpdate();
		return;
	}

	g_setListNameLoadedFromFileSystem = filename;

	ofstream file(win32::widen(filename).c_str());

	if (!file)
	{
		MessageBox(g_hwndParent, __LOCALIZE("Unable to write to file.", "sws_mbox"), __LOCALIZE("SWS Project List Save", "sws_mbox"), MB_OK);
		return;
	}

	for (int i = 0; i < g_SongItems.GetSize(); i++) {
		if (SWS_SongItem* item = (SWS_SongItem*)g_SongItems.Get(i)) {
			file << item->m_song_name.Get() << "\n";
		}
	}

	file.close();
	ProjectSetListUpdate();

	int enabled = ProjectSetListEnabled(NULL);
	if (!enabled)
		OpenProjectSetList(NULL);
}

void OpenProjectsFromSetList(COMMAND_T*)
{
	char directory[MAX_PATH]{};
	GetProjectPath(directory, sizeof(directory));

	char* filename = BrowseForFiles(__LOCALIZE("Select project list", "sws_mbox"), directory, nullptr, false, "Reaper Project List (*.RPL)\0*.RPL\0All Files\0*.*\0");
	if (!filename) {
		ProjectSetListUpdate();
		return;
	}
	g_setListNameLoadedFromFileSystem = filename;

	ifstream file(win32::widen(filename).c_str());
	free(filename);

	if (!file)
	{
		MessageBox(g_hwndParent, __LOCALIZE("Unable to open file.", "sws_mbox"), __LOCALIZE("SWS Project List Open", "sws_mbox"), MB_OK);
		ProjectSetListUpdate();
		return;
	}

	g_SongItems.Empty(true);

	string line;
	int slot = 1;
	while (getline(file, line))
	{
		if (line.empty())
			continue;
		g_SongItems.Add(new SWS_SongItem(slot++, line.c_str()));
	}
	file.close();
	ProjectSetListUpdate();

	int enabled = ProjectSetListEnabled(NULL);
	if (!enabled)
		OpenProjectSetList(NULL);
}

//!WANT_LOCALIZE_SWS_CMD_TABLE_BEGIN:sws_actions
COMMAND_T g_projSetListMgrCmdTable[] =
{
	{ { DEFACCEL, "SWS: Save set list" }, "SWS_PROJSETLISTSAVE", SaveProjectSetList, "Save set list...", },
	{ { DEFACCEL, "SWS: Open set list" }, "SWS_PROJSETLISTSOPEN",	OpenProjectsFromSetList, "Open set list...", },
	{ { DEFACCEL, "SWS: Show project set list window" }, "SWS_PROJSETLIST_OPEN", OpenProjectSetList, "SWS Project Set List", 0, ProjectSetListEnabled },
	{ {}, LAST_COMMAND, }, // Denote end of table
};
//!WANT_LOCALIZE_SWS_CMD_TABLE_END

int ProjectSetListMgrInit()
{
	SWSRegisterCommands(g_projSetListMgrCmdTable);
	return 1;
}

void ProjectSetListMgrExit()
{
	plugin_register("-projectconfig", &g_projSetListMgrCmdTable);
}
