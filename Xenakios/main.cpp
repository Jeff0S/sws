/******************************************************************************
/ main.cpp
/
/ Copyright (c) 2010 Tim Payne (SWS), original code by Xenakios
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

#include "Parameters.h"
#include "../SnM/SnM_Track.h"
#include "../Breeder/BR_MidiUtil.h"
#include "../Breeder/BR_Util.h"

#include <WDL/localize/localize.h>

using namespace std;

extern MTRand g_mtrand;

std::vector<int> g_ShuffledNumbers;
int g_ShuffledNumbersGenerated=0;

int IsRippleOneTrack(COMMAND_T*) { return *ConfigVar<int>("projripedit") == 1; }
int IsRippleAll(COMMAND_T*)      { return *ConfigVar<int>("projripedit") == 2; }

void DoToggleRippleOneTrack(COMMAND_T*)
{
	if (const ConfigVar<int> ripplemode = "projripedit")
	{
		if (*ripplemode == 1)
			Main_OnCommand(40309, 0);
		else
			Main_OnCommand(40310, 0);
	}
}

void DoToggleRippleAll(COMMAND_T*)
{
	if (const ConfigVar<int> ripplemode = "projripedit")
	{
		if (*ripplemode == 2)
			Main_OnCommand(40309, 0);
		else
			Main_OnCommand(40311, 0);
	}
}

bool GenerateShuffledRandomTable(std::vector<int>& IntTable,int numItems,int badFirstNumber)
{
	std::vector<int> CheckTable(1024);
	bool GoodFound=false;
	int IterCount=0;
	int rndInt=0;
	for (int i=0;i<1024;i++)
	{
		CheckTable[i]=0;
		IntTable[i]=0;
	}
	for (int i=0;i<numItems;i++)
	{
		GoodFound=false;
		while (!GoodFound)
		{
			rndInt=g_mtrand.randInt() % numItems;
			if ((CheckTable[rndInt]==0) && (rndInt!=badFirstNumber) && (i==0))
				GoodFound=true;
			if ((CheckTable[rndInt]==0) && (i>0))
				GoodFound=true;

			IterCount++;
			if (IterCount>10000000)
				break;
		}
		if (GoodFound)
		{
			IntTable[i]=rndInt;
			CheckTable[rndInt]=1;
		}
	}
	return false; // UGH, why does this always return false???
}

void DoSelectFiles(COMMAND_T*)
{
	char* cFiles = BrowseForFiles(__LOCALIZE("Select files","sws_mbox"), NULL, NULL, true, "WAV Files\0*.wav\0");
	if (cFiles)
	{
		g_filenames.clear();
		char* pStr = cFiles;
		while(*pStr)
		{
			g_filenames.push_back(pStr);
			pStr += strlen(pStr)+1;
		}
		free(cFiles);
	}
	g_ShuffledNumbersGenerated=0;
	GenerateShuffledRandomTable(g_ShuffledNumbers,g_filenames.size(),-1);
}

void DoInsertRandom(COMMAND_T*)
{
	// Using % to limit the random number isn't really correct. Should use the MTRand method
	// to get a rand int from a range, but should ensure first what is the actual range it will
	// return since we don't want to be indexing outside the array/list bounds
	if (g_filenames.size()>0)
		InsertMedia(g_filenames[g_mtrand.randInt() % g_filenames.size()].c_str(), 0);
}

void DoInsRndFileEx(bool RndLen,bool RndOffset,bool UseTimeSel)
{
	if (g_filenames.size()>0)
	{
		int filenameindex=g_mtrand.randInt() % g_filenames.size();

		t_vect_of_Reaper_tracks TheTracks;
		XenGetProjectTracks(TheTracks,true);
		if (TheTracks.size()>0)
		{
			PCM_source *NewPCM=PCM_Source_CreateFromFile(g_filenames[filenameindex].c_str());
			if (!NewPCM)
				return;

			MediaItem *NewItem=AddMediaItemToTrack(TheTracks[0]);
			MediaItem_Take *NewTake=AddTakeToMediaItem(NewItem);
			double TimeSelStart=0.0;
			double TimeSelEnd=NewPCM->GetLength();
			GetSet_LoopTimeRange(false,false,&TimeSelStart,&TimeSelEnd,false);
			double ItemPos=TimeSelStart;
			double ItemLen=NewPCM->GetLength();
			double MediaOffset=0.0;
			if (RndOffset)
			{
				MediaOffset=NewPCM->GetLength()*g_mtrand.rand();
				ItemLen-=MediaOffset;
			}
			if (RndLen)
				ItemLen = (NewPCM->GetLength() - MediaOffset)*g_mtrand.rand();
			if (UseTimeSel) ItemLen=TimeSelEnd-TimeSelStart;
			if (!UseTimeSel) ItemPos=GetCursorPosition();
			GetSetMediaItemTakeInfo(NewTake,"P_SOURCE",NewPCM);
			GetSetMediaItemTakeInfo(NewTake,"D_STARTOFFS",&MediaOffset);

			GetSetMediaItemInfo(NewItem,"D_POSITION",&ItemPos);
			GetSetMediaItemInfo(NewItem,"D_LENGTH",&ItemLen);
			Main_OnCommand(40047,0); // build any missing peaks
			SetEditCurPos(ItemPos+ItemLen,false,false);
			Undo_OnStateChangeEx(__LOCALIZE("Insert random file","sws_undo"),UNDO_STATE_ITEMS,-1);
			UpdateTimeline();
		}
	}
}

void DoInsRndFileRndLen(COMMAND_T*)
{
	DoInsRndFileEx(true,false,false);
}

void DoInsRndFileAtTimeSel(COMMAND_T*)
{
	DoInsRndFileEx(false,false,true);
}

void DoInsRndFileRndOffset(COMMAND_T*)
{
	DoInsRndFileEx(false,true,false);
}

void DoInsRndFileRndOffsetAtTimeSel(COMMAND_T*)
{
	DoInsRndFileEx(false,true,true);
}

void DoRoundRobinSelectTakes(COMMAND_T* ct)
{
	const int cnt=CountSelectedMediaItems(NULL);
	for (int i = 0; i < cnt; ++i)
	{
		MediaItem* item = GetSelectedMediaItem(NULL, i);
		int takeId = *(int*)GetSetMediaItemInfo(item,"I_CURTAKE",NULL);

		++takeId;
		if (takeId > CountTakes(item)-1)
			takeId = 0;
		GetSetMediaItemInfo(item,"I_CURTAKE",&takeId);
	}
	UpdateArrange();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(ct),UNDO_STATE_ITEMS,-1);
}


void DoSelectTakeInSelectedItems(int takeIndx) // -1 first -2 last take, otherwise index, if bigger than numtakes in item, the last
{
	MediaTrack* CurTrack=NULL;
	MediaItem* CurItem=NULL;
	bool ItemSelected=false;
	for (int trackID=0;trackID<GetNumTracks();trackID++)
	{
		CurTrack=CSurf_TrackFromID(trackID+1,FALSE);
		int numItems=GetTrackNumMediaItems(CurTrack);
		for (int itemID=0;itemID<numItems;itemID++)
		{
			CurItem = GetTrackMediaItem(CurTrack,itemID);
			ItemSelected=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
			if (ItemSelected==TRUE)
			{
				int numTakes=GetMediaItemNumTakes(CurItem);
				if (numTakes>0)
				{
					int TakeToSelect;
					if (takeIndx==-2) { TakeToSelect=numTakes-1; GetSetMediaItemInfo(CurItem,"I_CURTAKE",&TakeToSelect); }
					if (takeIndx==-1) { TakeToSelect=0; GetSetMediaItemInfo(CurItem,"I_CURTAKE",&TakeToSelect); }
					if (takeIndx>=0)
					{
						TakeToSelect=takeIndx;
						GetSetMediaItemInfo(CurItem,"I_CURTAKE",&TakeToSelect);
					}
				}
			}
		}
	}
}

void DoSelectFirstTakesInItems(COMMAND_T* ct)
{
	DoSelectTakeInSelectedItems(-1);
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(ct),4,-1);
	UpdateTimeline();
}

void DoSelectLastTakesInItems(COMMAND_T* ct)
{
	DoSelectTakeInSelectedItems(-2);
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(ct),4,-1);
	UpdateTimeline();
}

void DoInsertShuffledRandomFile(COMMAND_T*)
{
	if (g_filenames.size()>2)
	{
	 int FileToChoose=g_ShuffledNumbers[g_ShuffledNumbersGenerated];
	 char* filename=nullptr;
	 filename=(char*)g_filenames[FileToChoose].c_str();
	 InsertMedia(filename,0);
	 g_ShuffledNumbersGenerated++;
	 if (g_ShuffledNumbersGenerated==g_filenames.size())
	 {
		GenerateShuffledRandomTable(g_ShuffledNumbers,g_filenames.size(),FileToChoose);
		g_ShuffledNumbersGenerated=0;
	 }
	}
	else
		MessageBox(g_hwndParent, __LOCALIZE("Too few files for random shuffled insert!","sws_mbox"), __LOCALIZE("Xenakios - Error","sws_mbox"), MB_OK);

}

struct ItemTimesCompFunc
{
	bool operator()(MediaItem* ita, MediaItem* itb) const
	{
		double itapos = *(double*)GetSetMediaItemInfo(ita, "D_POSITION", 0);
		double itbpos = *(double*)GetSetMediaItemInfo(itb, "D_POSITION", 0);
		return itapos<itbpos;
	}
};

double g_FirstSelectedItemPos;
double g_LastSelectedItemEnd;

void DoSetLoopPointsToSelectedItems(bool SetTheLoop)
{
	vector<MediaItem*> selitems;
	XenGetProjectItems(selitems,true,true);
	if (selitems.size()>0)
	{
		double MinItemPos,MaxItemEnd=0.0;
		sort(selitems.begin(),selitems.end(),ItemTimesCompFunc());
		MinItemPos=*(double*)GetSetMediaItemInfo(selitems[0],"D_POSITION",0);
		double MaxItemPos=*(double*)GetSetMediaItemInfo(selitems[selitems.size()-1],"D_POSITION",0);
		MaxItemEnd=MaxItemPos+*(double*)GetSetMediaItemInfo(selitems[selitems.size()-1],"D_LENGTH",0);
		g_FirstSelectedItemPos=MinItemPos;
		g_LastSelectedItemEnd=MaxItemEnd;
		if (SetTheLoop)
			GetSet_LoopTimeRange(true,true,&MinItemPos,&MaxItemEnd,false);
	}
}

void DoNudgeItemsLeftSecsAndConfBased(COMMAND_T*)
{
	DoNudgeSelectedItemsPositions(true,false,0.0);
}

void DoNudgeItemsRightSecsAndConfBased(COMMAND_T*)
{
	DoNudgeSelectedItemsPositions(true,true,0.0);
}

void DoSaveMarkersAsTextFile(COMMAND_T*)
{
	// People can just use SWS track sheet export facility instead?  TRP Oct 20 2009
	char OutFileName[512];
	if (BrowseForSaveFile("Choose text file to save markers to", NULL, NULL, "TXT files\0*.txt\0", OutFileName, 512))
	{
		char TimeText1[32];
		int x=0;
		bool isrgn;
		double pos, rgnend;
		const char *name;
		int number;
		std::ofstream os(OutFileName);
		if (!os)
			return;
		os << "#\tPosition\tName" << "\n";
		while ((x=EnumProjectMarkers(x,&isrgn,&pos,&rgnend,&name,&number)))
		{
			format_timestr(pos, TimeText1,25);
			if (!isrgn)
				os << x << "\t" << TimeText1 << "\t" << name << "\n";
			else
				os << x << "\t" << TimeText1 << "\t" << name << " (Region)\n";
		}
	}
}

void DoResampleTakeOneSemitoneDown(COMMAND_T* ct)
{
	Undo_BeginBlock();
	Main_OnCommand(40518, 0);
	Main_OnCommand(40205, 0);
	Undo_EndBlock(SWS_CMD_SHORTNAME(ct),0);
}

void DoResampleTakeOneSemitoneUp(COMMAND_T* ct)
{
	Undo_BeginBlock();
	Main_OnCommand(40517, 0);
	Main_OnCommand(40204, 0);
	Undo_EndBlock(SWS_CMD_SHORTNAME(ct),0);
}

void DoLoopAndPlaySelectedItems(COMMAND_T*)
{
	Main_OnCommand(1016, 0); // Transport Stop
	DoSetLoopPointsToSelectedItems(true);
	Main_OnCommand(40632, 0); // Move cursor to start of loop
	GetSetRepeat(1); // set repeat on
	Main_OnCommand(1007, 0); // Transport Play
}

static bool g_PlayItemsOncePlaying=false;
static double g_loopStart, g_loopEnd=0.0;

void PlayItemsOnceTimer()
{
	if (g_PlayItemsOncePlaying)
	{
		if (GetPlayState() == 0 || GetPlayPosition2() < g_FirstSelectedItemPos)
			g_PlayItemsOncePlaying = false;
		else if (GetPlayState() & 1 && GetPlayPosition2() >= g_LastSelectedItemEnd)
		{
			g_PlayItemsOncePlaying = false;
			Main_OnCommand(1016, 0); // Transport Stop
		}

		if (!g_PlayItemsOncePlaying)
		{
			plugin_register("-timer", (void*)PlayItemsOnceTimer);
			GetSet_LoopTimeRange(true, true, &g_loopStart, &g_loopEnd, false); // restore orig. loop points
		}	
	}
}

void DoPlayItemsOnce(COMMAND_T*)
{
	if (CountSelectedMediaItems(NULL) > 0)
	{
		if (g_PlayItemsOncePlaying)
		{
			g_PlayItemsOncePlaying = false;
			plugin_register("-timer", (void*)PlayItemsOnceTimer);
			Main_OnCommand(1016, 0); // Transport Stop
		}
		else
			GetSet_LoopTimeRange(false, true, &g_loopStart, &g_loopEnd, false); // store orig. loop points
	
		GetSet_LoopTimeRange(true, true, &g_d0, &g_d0, false); // remove loop points
		DoSetLoopPointsToSelectedItems(false);
		SetEditCurPos(g_FirstSelectedItemPos, false, false);

		Main_OnCommand(1007, 0); // Transport Play
		Sleep(100);
		g_PlayItemsOncePlaying = true;

		if (g_PlayItemsOncePlaying)
			plugin_register("timer", (void*)PlayItemsOnceTimer);
	}
}

void DoMoveCurNextTransMinusFade(COMMAND_T*)
{
	const double defFadeLen = *ConfigVar<double>("deffadelen");
	static double prevCurPos = -666.0;
	double CurPos=GetCursorPosition();
	if (CurPos==prevCurPos)
	{
		CurPos=GetCursorPosition()+defFadeLen;
		SetEditCurPos(CurPos,false,false);
	}
	Main_OnCommand(40375,0);
	CurPos=GetCursorPosition()-defFadeLen;
	SetEditCurPos(CurPos,false,false);
	prevCurPos=CurPos;
}

void DoMoveCurPrevTransMinusFade(COMMAND_T*)
{
	Main_OnCommand(40376,0); // move to next transient
}

#define ALEXPIXELS 12

void DoMoveCursor10pixRight(COMMAND_T*) { for (int i = 0; i < ALEXPIXELS; i++) Main_OnCommand(40105,0); }
void DoMoveCursor10pixLeft(COMMAND_T*)  { for (int i = 0; i < ALEXPIXELS; i++) Main_OnCommand(40104,0); }
void DoMoveCursor10pixLeftCreateSel(COMMAND_T*)  { for (int i = 0; i < ALEXPIXELS; i++) Main_OnCommand(40102,0); }
void DoMoveCursor10pixRightCreateSel(COMMAND_T*) { for (int i = 0; i < ALEXPIXELS; i++) Main_OnCommand(40103,0); }

static preview_register_t g_ItemPreview = { {}, 0, };
static bool g_itemPreviewPlaying = false;
static bool g_itemPreviewPaused = false;
static bool g_itemPreviewSendCC123 = false;
static ReaProject *g_itemPreviewProject = NULL;

void ItemPreviewTimer()
{
	if (g_itemPreviewPlaying)
	{
		#ifdef _WIN32
			EnterCriticalSection(&g_ItemPreview.cs);
		#else
			pthread_mutex_lock(&g_ItemPreview.mutex);
		#endif

		// Have we reached the end?
		if (g_ItemPreview.curpos >= g_ItemPreview.src->GetLength())
		{
			#ifdef _WIN32
				LeaveCriticalSection(&g_ItemPreview.cs);
			#else
				pthread_mutex_unlock(&g_ItemPreview.mutex);
			#endif
			g_itemPreviewSendCC123 = false; // item played out completely, no need to send all-notes-off
			ItemPreview (0, NULL, NULL, 0, 0, 0, false);
			plugin_register("-timer",(void*)ItemPreviewTimer);
		}
		else
		{
			#ifdef _WIN32
				LeaveCriticalSection(&g_ItemPreview.cs);
			#else
				pthread_mutex_unlock(&g_ItemPreview.mutex);
			#endif
		}
	}
	else
	{
		plugin_register("-timer",(void*)ItemPreviewTimer);
	}
}

void ItemPreviewPlayState(bool play, bool rec)
{
	if (g_itemPreviewPlaying && (!play || rec || (g_itemPreviewPaused && play)))
		ItemPreview(0, NULL, NULL, 0, 0, 0, false);
}

// mode:
// 0: stop
// 1: start
// 2: toggle
void ItemPreview(int mode, MediaItem* item, MediaTrack* track, double volume, double startOffset, double measureSync, bool pauseDuringPrev)
{
	if (IsRecording(NULL)) // Reaper won't preview anything during recording but extension will still think preview is in progress (could disrupt toggle states and send unneeded CC123)
		return;

	// Preview called while preview in progress, stopping previous preview...
	if (g_itemPreviewPlaying)
	{
		if (g_ItemPreview.preview_track)
		{
			StopTrackPreview2(g_itemPreviewProject, &g_ItemPreview);
			if (g_itemPreviewSendCC123)
				SendAllNotesOff((MediaTrack*)g_ItemPreview.preview_track);
		}
		else
		{
			StopPreview(&g_ItemPreview);
		}

		g_itemPreviewPlaying = false;
		g_itemPreviewSendCC123 = false;
		delete g_ItemPreview.src;

		if (g_itemPreviewPaused && mode != 1) // requesting new preview while old one is still playing shouldn't unpause playback
		{
			if (GetPlayStateEx(NULL)&2)
				OnPauseButton();
			g_itemPreviewPaused = false;
		}

		if (mode == 2)
			return;
	}
	if (mode == 0)
		return;

	if (CountTakes(item))
	{
		MediaItem_Take* take = GetActiveTake(item);
		bool itemMuteState = *(bool*)GetSetMediaItemInfo(item, "B_MUTE", NULL);
		bool isMidi = IsMidi(take);
		double effectiveMidiTakeLen = EffectiveMidiTakeEnd(take, true, true, true) - GetMediaItemInfo_Value(item, "D_POSITION");
		if (isMidi && effectiveMidiTakeLen <= 0)
			return;

		GetSetMediaItemInfo(item, "B_MUTE", &g_bFalse); // needs to be set before getting the source

		PCM_source* src = DuplicateSource((PCM_source*)item); // Casting from MediaItem* to PCM_source works!  Who would have known?
		if (src && (!isMidi || (effectiveMidiTakeLen > 0 && effectiveMidiTakeLen > startOffset)))
		{
			GetSetMediaItemInfo((MediaItem*)src, "D_POSITION", &g_d0);
			if (isMidi) GetSetMediaItemInfo((MediaItem*)src, "D_LENGTH", &effectiveMidiTakeLen);

			if (!g_ItemPreview.src) // src == 0 means need to initialize structure
			{
				#ifdef _WIN32
					InitializeCriticalSection(&g_ItemPreview.cs);
				#else
					pthread_mutex_init(&g_ItemPreview.mutex, NULL);
				#endif
				g_ItemPreview.loop = false;
			}

			g_ItemPreview.src = src;
			g_ItemPreview.m_out_chan = (track) ? (-1) : (0);
			g_ItemPreview.curpos = startOffset;
			g_ItemPreview.volume = volume;
			g_ItemPreview.preview_track = track;


			// Pause before preview otherwise ItemPreviewPlayState will stop it
			g_itemPreviewPaused = pauseDuringPrev;
			if (g_itemPreviewPaused && GetPlayState() & 1)
				OnPauseButton();

			if (g_ItemPreview.preview_track)
			{
				if (isMidi)
					g_itemPreviewSendCC123 = true;
				g_itemPreviewProject = EnumProjects(-1, NULL, 0);
				g_itemPreviewPlaying = !!PlayTrackPreview2Ex(g_itemPreviewProject, &g_ItemPreview, !!measureSync, measureSync);
			}
			else
				g_itemPreviewPlaying = !!PlayPreviewEx(&g_ItemPreview, !!measureSync, measureSync);

			if (g_itemPreviewPlaying)
				plugin_register("timer",(void*)ItemPreviewTimer);
			else
				delete g_ItemPreview.src;
		}

		GetSetMediaItemInfo(item, "B_MUTE", &itemMuteState);
	}
}

void DoPreviewSelectedItem (COMMAND_T* ct)
{
	if (MediaItem* item = GetSelectedMediaItem(NULL, 0))
	{
		int mode = 0;
		if ((int)ct->user == 0)     // zero is stop
			mode = 0;
		else if ((int)ct->user & 1) // odd numbers are start
			mode = 1;
		else                        // even numbers are toggle
			mode = 2;

		double volume = 1;
		if ((int)ct->user == 3 || (int)ct->user == 4)
			volume = GetMediaTrackInfo_Value(GetMediaItem_Track(item), "D_VOL");

		MediaTrack* track = NULL;
		if ((int)ct->user == 5 || (int)ct->user == 6)
			track = GetMediaItem_Track(item);

		ItemPreview(mode, item, track, volume, 0, 0, false);
	}
}

void DoScrollTVPageDown(COMMAND_T*)
{
	HWND hTrackView=GetTrackWnd();
	if (hTrackView)
	{
		SendMessage(hTrackView,WM_VSCROLL,SB_PAGEDOWN,0);
	}
}

void DoScrollTVPageUp(COMMAND_T*)
{
	HWND hTrackView=GetTrackWnd();
	if (hTrackView)
	{
		SendMessage(hTrackView,WM_VSCROLL,SB_PAGEUP,0);
	}
}

void DoScrollTVHome(COMMAND_T*)
{
	HWND hTrackView=GetTrackWnd();
	if (hTrackView)
	{
		SendMessage(hTrackView,WM_VSCROLL,SB_TOP,0);
	}
}

void DoScrollTVEnd(COMMAND_T*)
{
	HWND hTrackView=GetTrackWnd();
	if (hTrackView)
	{
		SendMessage(hTrackView,WM_VSCROLL,SB_BOTTOM,0);
	}
}

void DoRenameMarkersWithAscendingNumbers(COMMAND_T* ct)
{
	int x=0;

	bool isrgn = false;
	double pos, rgnend = 0.0;
	int number, color = 0;
	char newmarkname[100];
	int j=1;
	while ((x = EnumProjectMarkers3(NULL, x, &isrgn, &pos, &rgnend, NULL, &number, &color)))
	{
		if (!isrgn)
		{
			sprintf(newmarkname,"%.3d",j);
			SetProjectMarkerByIndex(NULL, x-1, isrgn, pos, rgnend, number, newmarkname, color);
			j++;
		}
	}
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(ct),8,-1);
}

void DoSetStopAtEndOfTimeSel(int enabled) // -1 toggle 0 unset 1 set
{
	if (ConfigVar<int> stopatend = "stopendofloop")
	{
		if (enabled==-1)
		{
			if (*stopatend==0)
				*stopatend=1;
			else
				*stopatend=0;
		}
		else
			*stopatend=enabled;
	}
}

int IsStopAtEndOfTimeSel(COMMAND_T*) { return *ConfigVar<int>("stopendofloop") ? true : false; }

void DoToggleSTopAtEndOfTimeSel(COMMAND_T*)
{
	DoSetStopAtEndOfTimeSel(-1);
}

WDL_String g_XenIniFilename;

#ifdef _WIN32
#define XEN_INIFILE_OLD "%s\\Plugins\\Xenakios_Commands.ini"
#define XEN_INIFILE_NEW "%s\\Xenakios_Commands.ini"
#else
#define XEN_INIFILE_OLD "%s/Plugins/Xenakios_Commands.ini"
#define XEN_INIFILE_NEW "%s/Xenakios_Commands.ini"
#endif

void XenakiosExit()
{
	plugin_register("-projectconfig",&xen_reftrack_pcreg);
	plugin_register("-timer", (void*)PlayItemsOnceTimer);
	plugin_register("-timer",(void*)ItemPreviewTimer);
	RemoveUndoKeyUpHandler01();

	if (g_hItemInspector) 
		DestroyWindow(g_hItemInspector);
}

int XenakiosInit()
{
	if(!plugin_register("projectconfig",&xen_reftrack_pcreg))
		return 0;
	// Move Xenakios_commands.ini to a new location
	char oldIniFilename[BUFFER_SIZE], iniFilename[BUFFER_SIZE];
	snprintf(oldIniFilename, BUFFER_SIZE, XEN_INIFILE_OLD, GetExePath()); // old location
	snprintf(iniFilename, BUFFER_SIZE, XEN_INIFILE_NEW, GetResourcePath());
	if (FileExists(oldIniFilename))
		MoveFile(oldIniFilename, iniFilename);
	g_XenIniFilename.Set(iniFilename);

	g_ShuffledNumbers.resize(1024);

	SWSRegisterCommands(g_XenCommandTable);

	InitCommandParams();

	InitUndoKeyUpHandler01();
	g_hItemInspector = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_ITEM_INSPECTOR), g_hwndParent, (DLGPROC)MyItemInspectorDlgProc);
	if (g_hItemInspector != NULL)
		ShowWindow(g_hItemInspector, SW_HIDE);

	srand ((unsigned int)time(NULL));

	// Add track template actions
	char cPath[BUFFER_SIZE];
	snprintf(cPath, BUFFER_SIZE, "%s%cTrackTemplates", GetResourcePath(), PATH_SLASH_CHAR);
	vector<string> templates;
	SearchDirectory(templates, cPath, "RTRACKTEMPLATE", true);
	for (int i = 0; i < (int)templates.size(); i++)
	{
		const char* pFilename = strrchr(templates[i].c_str(), PATH_SLASH_CHAR);
		if (pFilename && pFilename[1])
		{
			int iNum = atol(pFilename+1);
			if (iNum && !SWSGetCommandID(DoOpenTrackTemplate, iNum))
			{
				char cDesc[BUFFER_SIZE];
				char cID[BUFFER_SIZE];
				snprintf(cID, BUFFER_SIZE, "XENAKIOS_LOADTRACKTEMPLATE%d", iNum);
				snprintf(cDesc, BUFFER_SIZE, __LOCALIZE_VERFMT("Xenakios/SWS: [Deprecated] Load track template %d","sws_actions"), iNum);
				SWSRegisterCommandExt(DoOpenTrackTemplate, cID, cDesc, iNum, false);
			}
		}
	}

	// Add project template actions
	snprintf(cPath, BUFFER_SIZE, "%s%cProjectTemplates", GetResourcePath(), PATH_SLASH_CHAR);
	templates.clear();
	SearchDirectory(templates, cPath, "RPP", true);
	for (int i = 0; i < (int)templates.size(); i++)
	{
		const char* pFilename = strrchr(templates[i].c_str(), PATH_SLASH_CHAR);
		if (pFilename && pFilename[1])
		{
			int iNum = atol(pFilename+1);
			if (iNum && !SWSGetCommandID(DoOpenProjectTemplate, iNum))
			{
				char cID[BUFFER_SIZE];
				char cDesc[BUFFER_SIZE];
				snprintf(cID, BUFFER_SIZE, "XENAKIOS_LOADPROJTEMPL%d", iNum);
				snprintf(cDesc, BUFFER_SIZE, __LOCALIZE_VERFMT("Xenakios/SWS: [Deprecated] Load project template %d","sws_actions"), iNum);
				SWSRegisterCommandExt(DoOpenProjectTemplate, cID, cDesc, iNum, false);
			}
		}
	}

	return 1;
}
