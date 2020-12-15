/******************************************************************************
/ Freeze.cpp
/
/ Copyright (c) 2010 Tim Payne (SWS)
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
#include "TrackItemState.h"
#include "ItemSelState.h"
#include "MuteState.h"
#include "ActiveTake.h"
#include "TimeState.h"

//#define TESTCODE

#ifdef TESTCODE
#include "Prompt.h"
#pragma message("TESTCODE Defined")
void TestFunc(COMMAND_T* = NULL)
{
}

void PrintGuids(COMMAND_T* = NULL)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		char debugStr[256];
		char guidStr[64];
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		guidToString((GUID*)GetSetMediaTrackInfo(tr, "GUID", NULL), guidStr);
		snprintf(debugStr, sizeof(debugStr), "Track %d %s GUID %s\n", i, (char*)GetSetMediaTrackInfo(tr, "P_NAME", NULL), guidStr);
#ifdef _WIN32
		OutputDebugString(debugStr);
#endif
	}
}

void PrintSubMenu(HMENU subMenu)
{
	static int iDepth = 0;
	char debugStr[256];
	char menuStr[64];
	MENUITEMINFO mi={sizeof(MENUITEMINFO),};
	mi.fMask = MIIM_STATE | MIIM_ID | MIIM_SUBMENU | MIIM_FTYPE | MIIM_STRING;
	mi.dwTypeData = menuStr;

	for (int i = 0; i < GetMenuItemCount(subMenu); i++)
	{
		mi.cch = 64;
		GetMenuItemInfo(subMenu, i, true, &mi);

		for (int j = 0; j < iDepth; j++)
			OutputDebugString("  ");
		if (mi.fType & MFT_SEPARATOR)
			OutputDebugString("---------------------\n");
		else if (mi.hSubMenu)
		{
			snprintf(debugStr, sizeof(debugStr), "%s\n", mi.dwTypeData ? mi.dwTypeData : "<null>");
			OutputDebugString(debugStr);
			iDepth++;
			PrintSubMenu(mi.hSubMenu);
			iDepth--;
		}
		else
		{
			snprintf(debugStr, sizeof(debugStr), "%5d %s %s\n", mi.wID, mi.fState == MFS_CHECKED ? "*" : (mi.fState == MFS_DISABLED ? "-" : " "), mi.dwTypeData ? mi.dwTypeData : "<null>");
			OutputDebugString(debugStr);
		}
	}
}

// TODO: replace g_hwndParent with GetMainHwnd()
void PrintMenu(COMMAND_T* = NULL)
{
	PrintSubMenu(GetMenu(g_hwndParent));
}

void RunAction(COMMAND_T* = NULL)
{
	static char resp[10] = "";
	if (PromptUserForString(GetMainHwnd(), "Enter action #", resp, 10))
	{
		int iAction = atoi(resp);
		if (iAction > 0)
			Main_OnCommand(iAction, 0);
	}
}

void DumpItems(COMMAND_T* = NULL)
{
	for (int i = 0; i < CountSelectedMediaItems(NULL); i++)
	{
		MediaItem* item = GetSelectedMediaItem(NULL, i);
		MediaTrack* tr = (MediaTrack*)GetSetMediaItemInfo(item, "P_TRACK", NULL);
		double dStart = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
		double dEnd   = *(double*)GetSetMediaItemInfo(item, "D_LENGTH", NULL) + dStart;
		double dBStart = TimeMap_timeToQN(dStart);
		double dBEnd   = TimeMap_timeToQN(dEnd);
		char str[1953];
		double dSnapOffset = *(double*)GetSetMediaItemInfo(item, "D_SNAPOFFSET", NULL);
		double dSourceOffset = 0.0;
		MediaItem_Take* take = GetMediaItemTake(item, -1);
		if (take)
			dSourceOffset = *(double*)GetSetMediaItemTakeInfo(take, "D_STARTOFFS", NULL);
		sprintf(str, "%2d %.14f/%.14f %.14f/%.14f P=%.4f S=%.4f\n", CSurf_TrackToID(tr, false), dStart, dBStart, dEnd, dBEnd, dSnapOffset, dSourceOffset);
		OutputDebugString(str);
	}
}

#endif

static bool ProcessExtensionLine(const char *line, ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	LineParser lp(false);
	if (lp.parse(line) || lp.getnumtokens() < 1)
		return false;
	if (strcmp(lp.gettoken_str(0), "<TRACKSTATE") == 0)
	{
		TrackState* ts = g_tracks.Get()->Add(new TrackState(&lp));

		char linebuf[4096];
		while(!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
		{
			if (lp.gettoken_str(0)[0] == '>')
				break;
			else if (strcmp("ITEMSTATE", lp.gettoken_str(0)) == 0)
				ts->m_items.Add(new ItemState(&lp));
		}
		return true;
	}
	else if (strcmp(lp.gettoken_str(0), "<MUTESTATE") == 0)
	{
		MuteState* ms = g_muteStates.Get()->Add(new MuteState(&lp));

		char linebuf[4096];
		while(!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
		{
			if (lp.gettoken_str(0)[0] == '>')
				break;
			else if (strcmp("CHILD", lp.gettoken_str(0)) == 0)
				ms->m_children.Add(new MuteItem(&lp));
			else if (strcmp("RECEIVE", lp.gettoken_str(0)) == 0)
				ms->m_receives.Add(new MuteItem(&lp));
		}
		return true;
	}
	else if (strcmp(lp.gettoken_str(0), "<SELTRACKITEMSELSTATE") == 0)
	{
		SelItemsTrack* sit = g_selItemsTrack.Get()->Add(new SelItemsTrack(&lp));
		char linebuf[4096];
		while(!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
		{
			if (lp.gettoken_str(0)[0] == '>')
				break;
			else if (strcmp(lp.gettoken_str(0), "<SLOT") == 0)
			{
				int iSlot = lp.gettoken_int(1) - 1;
				while(!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
				{
					sit->Add(&lp, iSlot); // Add even a single '>' to ensure "empty" slots are restored properly
					if (lp.gettoken_str(0)[0] == '>')
						break;
				}
			}
		}
		return true;
	}
	else if (strcmp(lp.gettoken_str(0), "<ITEMSELSTATE") == 0)
	{
		char linebuf[4096];
		// Ignore old-style take-as-item-GUID
		if (lp.getnumtokens() == 2 && strlen(lp.gettoken_str(1)) != 38)
		{
			g_selItems.Get()->Empty();
			while(!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
			{
				if (lp.gettoken_str(0)[0] == '>')
					break;
				g_selItems.Get()->Add(&lp);
			}
		}
		else
		{
			int iDepth = 0;
			while(!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
			{
				if (lp.gettoken_str(0)[0] == '<')
					++iDepth;
				else if (lp.gettoken_str(0)[0] == '>')
					--iDepth;
				if (iDepth < 0)
					break;
			}
		}
		return true;
	}
	else if (strcmp(lp.gettoken_str(0), "<ACTIVETAKESTRACK") == 0)
	{
		ActiveTakeTrack* att = g_activeTakeTracks.Get()->Add(new ActiveTakeTrack(&lp));
		char linebuf[4096];
		while(!ctx->GetLine(linebuf,sizeof(linebuf)) && !lp.parse(linebuf))
		{
			if (lp.gettoken_str(0)[0] == '>')
				break;
			else if (strcmp(lp.gettoken_str(0), "ITEM") == 0)
				att->m_items.Add(new ActiveTake(&lp));
		}
		return true;
	}
	else if (strcmp(lp.gettoken_str(0), "TIMESEL") == 0)
	{
		g_timeSel.Get()->Add(new TimeSelection(&lp));
		return true;
	}
	return false;
}

static void SaveExtensionConfig(ProjectStateContext *ctx, bool isUndo, struct project_config_extension_t *reg)
{
	char str[4096];

	for (int i = 0; i < g_tracks.Get()->GetSize(); i++)
	{
		ctx->AddLine("%s",g_tracks.Get()->Get(i)->ItemString(str, 4096)); 
		for (int j = 0; j < g_tracks.Get()->Get(i)->m_items.GetSize(); j++)
			ctx->AddLine("%s",g_tracks.Get()->Get(i)->m_items.Get(j)->ItemString(str, 4096));
		ctx->AddLine(">");
	}
	bool bDone;
	for (int i = 0; i < g_muteStates.Get()->GetSize(); i++)
	{
		do
			ctx->AddLine("%s", g_muteStates.Get()->Get(i)->ItemString(str, 4096, &bDone));
		while (!bDone);
	}
	for (int i = 0; i < g_selItemsTrack.Get()->GetSize(); i++)
	{
		do
			ctx->AddLine("%s",g_selItemsTrack.Get()->Get(i)->ItemString(str, 4096, &bDone));
		while (!bDone);
	}
	if (g_selItems.Get()->NumItems())
	{
		ctx->AddLine("<ITEMSELSTATE 1");
		do
			ctx->AddLine("%s",g_selItems.Get()->ItemString(str, 4096, &bDone));
		while (!bDone);
	}
	for (int i = 0; i < g_activeTakeTracks.Get()->GetSize(); i++)
	{
		ctx->AddLine("%s",g_activeTakeTracks.Get()->Get(i)->ItemString(str, 4096)); 
		for (int j = 0; j < g_activeTakeTracks.Get()->Get(i)->m_items.GetSize(); j++)
			ctx->AddLine("%s",g_activeTakeTracks.Get()->Get(i)->m_items.Get(j)->ItemString(str, 4096));
		ctx->AddLine(">");
	}
	for (int i = 0; i < g_timeSel.Get()->GetSize(); i++)
		ctx->AddLine("%s",g_timeSel.Get()->Get(i)->ItemString(str, 4096));
}

static void BeginLoadProjectState(bool isUndo, struct project_config_extension_t *reg)
{
	g_tracks.Get()->Empty(true);
	g_tracks.Cleanup();
	g_muteStates.Get()->Empty(true);
	g_muteStates.Cleanup();
	g_selItemsTrack.Get()->Empty(true);
	g_selItemsTrack.Cleanup();
	g_selItems.Get()->Empty();
	g_selItems.Cleanup();
	g_activeTakeTracks.Get()->Empty(true);
	g_activeTakeTracks.Cleanup();
	g_timeSel.Get()->Empty(true);
	g_timeSel.Cleanup();
}

static project_config_extension_t g_projectconfig = { ProcessExtensionLine, SaveExtensionConfig, BeginLoadProjectState, NULL };

//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions
static COMMAND_T g_commandTable[] = 
{
	// ActiveTake.cpp
	{ { DEFACCEL, "SWS: Save active takes on selected track(s)" },					"SWS_SAVEACTTAKES",		SaveActiveTakes,			},
	{ { DEFACCEL, "SWS: Restore active takes on selected track(s)" },				"SWS_RESTACTTAKES",		RestoreActiveTakes,			},

	// ItemSelState.cpp
	{ { DEFACCEL, "SWS: Save selected track(s) selected item(s), slot 1" },			"SWS_SAVESELITEMS1",	SaveSelTrackSelItems,		NULL, 0 },
	{ { DEFACCEL, "SWS: Save selected track(s) selected item(s), slot 2" },			"SWS_SAVESELITEMS2",	SaveSelTrackSelItems,		NULL, 1 },
	{ { DEFACCEL, "SWS: Save selected track(s) selected item(s), slot 3" },			"SWS_SAVESELITEMS3",	SaveSelTrackSelItems,		NULL, 2 },
	{ { DEFACCEL, "SWS: Save selected track(s) selected item(s), slot 4" },			"SWS_SAVESELITEMS4",	SaveSelTrackSelItems,		NULL, 3 },
	{ { DEFACCEL, "SWS: Save selected track(s) selected item(s), slot 5" },			"SWS_SAVESELITEMS5",	SaveSelTrackSelItems,		NULL, 4 },
	{ { DEFACCEL, "SWS: Restore selected track(s) selected item(s), slot 1" },		"SWS_RESTSELITEMS1",	RestoreSelTrackSelItems,	NULL, 0 },
	{ { DEFACCEL, "SWS: Restore selected track(s) selected item(s), slot 2" },		"SWS_RESTSELITEMS2",	RestoreSelTrackSelItems,	NULL, 1 },
	{ { DEFACCEL, "SWS: Restore selected track(s) selected item(s), slot 3" },		"SWS_RESTSELITEMS3",	RestoreSelTrackSelItems,	NULL, 2 },
	{ { DEFACCEL, "SWS: Restore selected track(s) selected item(s), slot 4" },		"SWS_RESTSELITEMS4",	RestoreSelTrackSelItems,	NULL, 3 },
	{ { DEFACCEL, "SWS: Restore selected track(s) selected item(s), slot 5" },		"SWS_RESTSELITEMS5",	RestoreSelTrackSelItems,	NULL, 4 },
	{ { DEFACCEL, "SWS: Restore last item selection on selected track(s)" },		"SWS_RESTLASTSEL",		RestoreLastSelItemTrack,	NULL, },
	{ { DEFACCEL, "SWS: Save selected item(s)" },									"SWS_SAVEALLSELITEMS1",	SaveSelItems,				NULL, 0 }, // Slots aren't supported here (yet?)
	{ { DEFACCEL, "SWS: Restore saved selected item(s)" },							"SWS_RESTALLSELITEMS1",	RestoreSelItems,			NULL, 0 },

	// MuteState.cpp
	{ { DEFACCEL, "SWS: Save selected track(s) mutes (+receives, children)" },		"SWS_SAVEMUTES",		SaveMutes,					},
	{ { DEFACCEL, "SWS: Restore selected track(s) mutes (+receives, children)" },	"SWS_RESTRMUTES",		RestoreMutes,				},

	// TimeState.cpp
	{ { DEFACCEL, "SWS: Restore time selection, next slot" },						"SWS_RESTTIMENEXT",		RestoreTimeNext, },
	{ { DEFACCEL, "SWS: Restore loop selection, next slot" },						"SWS_RESTLOOPNEXT",		RestoreLoopNext, },

	// TrackItemState.cpp	
	{ { DEFACCEL, "SWS: Save selected track(s) items' states" },					"SWS_SAVETRACK",		SaveTrack,			},
	{ { DEFACCEL, "SWS: Restore selected track(s) items' states" },					"SWS_RESTORETRACK",		RestoreTrack,		},
	{ { DEFACCEL, "SWS: Save selected track(s) selected items' states" },			"SWS_SAVESELONTRACK",	SaveSelOnTrack,		},
	{ { DEFACCEL, "SWS: Restore selected track(s) selected items' states" },		"SWS_RESTSELONTRACK",	RestoreSelOnTrack,	},
	{ { DEFACCEL, "SWS: Select item(s) with saved state on selected track(s)" },	"SWS_SELWITHSTATE",		SelItemsWithState,	},

#ifdef TESTCODE
	{ { DEFACCEL, "SWS: [Internal] Test" }, "SWS_TEST",  TestFunc, },
	{ { DEFACCEL, "SWS: [Internal] Print track GUIDs" }, "SWS_PRINTGUIDS",  PrintGuids, },
	{ { DEFACCEL, "SWS: [Internal] Print menu tree" }, "SWS_PRINTMENU",  PrintMenu, },
	{ { DEFACCEL, "SWS: [Internal] Run action..." }, "SWS_RUNACTION",  RunAction, },
	{ { DEFACCEL, "SWS: [Internal] Print sel items' times" }, "SWS_DUMPITEMS",  DumpItems, },

#endif

	{ {}, LAST_COMMAND, }, // Denote end of table
};
//!WANT_LOCALIZE_1ST_STRING_END

int FreezeInit()
{
	SWSRegisterCommands(g_commandTable);
	if (!plugin_register("projectconfig",&g_projectconfig))
		return 0;

	return 1;
}

void FreezeExit()
{
	plugin_register("-projectconfig",&g_projectconfig);
}
