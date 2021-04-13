/******************************************************************************
/ SnM.cpp
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

#include "stdafx.h"
#include "SnM.h"
#include "SnM_CueBuss.h"
#include "SnM_Cyclactions.h"
#include "SnM_Dlg.h"
#include "SnM_Find.h"
#include "SnM_FX.h"
#include "SnM_FXChain.h"
#include "SnM_Item.h"
#include "SnM_LiveConfigs.h"
#include "SnM_Marker.h"
#include "SnM_ME.h"
#include "SnM_Misc.h"
#include "SnM_Notes.h"
#include "SnM_Project.h"
#include "SnM_RegionPlaylist.h"
#include "SnM_Resources.h"
#include "SnM_Routing.h"
#include "SnM_Track.h"
#include "SnM_Util.h"
#include "SnM_Window.h"
#include "Misc/TrackSel.h"
#include "Snapshots/Snapshots.h"
#include "version.h"

#include <WDL/localize/localize.h>

void Noop(COMMAND_T* _ct)
{
}

void Noop2(COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd)
{
	WDL_FastString str;
	str.SetFormatted(512, "Noop2: _val=%d, _valhw=%d, _relmode=%d, val14=%d\r\n", _val, _valhw, _relmode, _valhw|(_val<<7));
	ShowConsoleMsg(str.Get());
}

///////////////////////////////////////////////////////////////////////////////
// S&M actions (main section)
///////////////////////////////////////////////////////////////////////////////

static COMMAND_T s_cmdTable[] =
{

//	{ { DEFACCEL, "SWS/S&M: [Internal] QuickTest1" }, "S&M_QUICKTEST1", Noop, NULL, },
//	{ { DEFACCEL, "SWS/S&M: [Internal] QuickTest2" }, "S&M_QUICKTEST2", NULL, NULL, 0, NULL, 0, Noop2, },


//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions


	// Routing & cue buss -----------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Create cue buss from track selection (use last settings)" }, "S&M_CUEBUS", CueBuss, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Open/close Cue Buss generator" }, "S&M_SENDS4", OpenCueBussDlg, NULL, 0, IsCueBussDlgDisplayed},

	{ { DEFACCEL, "SWS/S&M: Remove receives from selected tracks" }, "S&M_SENDS5", RemoveReceives, NULL, },
	{ { DEFACCEL, "SWS/S&M: Remove sends from selected tracks" }, "S&M_SENDS6", RemoveSends, NULL, },
	{ { DEFACCEL, "SWS/S&M: Remove routing from selected tracks" }, "S&M_SENDS7", RemoveRoutings, NULL, },

	{ { DEFACCEL, "SWS/S&M: Copy selected tracks (with routing)" }, "S&M_COPYSNDRCV1", CopyWithIOs, NULL, },
	{ { DEFACCEL, "SWS/S&M: Paste tracks (with routing) or items" }, "S&M_PASTSNDRCV1", PasteWithIOs, NULL, },
	{ { DEFACCEL, "SWS/S&M: Cut selected tracks (with routing)" }, "S&M_CUTSNDRCV1", CutWithIOs, NULL, },

	{ { DEFACCEL, "SWS/S&M: Copy selected tracks routings" }, "S&M_COPYSNDRCV2", CopyRoutings, NULL, },
	{ { DEFACCEL, "SWS/S&M: Paste routings to selected tracks" }, "S&M_PASTSNDRCV2", PasteRoutings, NULL, },
	{ { DEFACCEL, "SWS/S&M: Cut selected tracks routings" }, "S&M_CUTSNDRCV2", CutRoutings, NULL, },

	{ { DEFACCEL, "SWS/S&M: Copy selected tracks sends" }, "S&M_COPYSNDRCV3", CopySends, NULL, },
	{ { DEFACCEL, "SWS/S&M: Paste sends to selected tracks" }, "S&M_PASTSNDRCV3", PasteSends, NULL, },
	{ { DEFACCEL, "SWS/S&M: Cut selected tracks sends" }, "S&M_CUTSNDRCV3", CutSends, NULL, },

	{ { DEFACCEL, "SWS/S&M: Copy selected tracks receives" }, "S&M_COPYSNDRCV4", CopyReceives, NULL, },
	{ { DEFACCEL, "SWS/S&M: Paste receives to selected tracks" }, "S&M_PASTSNDRCV4", PasteReceives, NULL, },
	{ { DEFACCEL, "SWS/S&M: Cut selected tracks receives" }, "S&M_CUTSNDRCV4", CutReceives, NULL, },

	{ { DEFACCEL, "SWS/S&M: Save default track send preferences" }, "S&M_SAVE_DEFSNDFLAGS", SaveDefaultTrackSendPrefs, NULL, },
	{ { DEFACCEL, "SWS/S&M: Recall default track send preferences" }, "S&M_RECALL_DEFSNDFLAGS", RecallDefaultTrackSendPrefs, NULL, },
	{ { DEFACCEL, "SWS/S&M: Set default track sends to audio and MIDI" }, "S&M_DEFSNDFLAGS_BOTH", SetDefaultTrackSendPrefs, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Set default track sends to audio only" }, "S&M_DEFSNDFLAGS_AUDIO", SetDefaultTrackSendPrefs, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Set default track sends to MIDI only" }, "S&M_DEFSNDFLAGS_MIDI", SetDefaultTrackSendPrefs, NULL, 2},

	// Windows ----------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Close all floating FX windows" }, "S&M_WNCLS3", CloseAllFXWindows, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Close all FX chain windows" }, "S&M_WNCLS4", CloseAllFXChainsWindows, NULL, },
	{ { DEFACCEL, "SWS/S&M: Close all floating FX windows for selected tracks" }, "S&M_WNCLS5", CloseAllFXWindows, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Close all floating FX windows, except focused one" }, "S&M_WNCLS6", CloseAllFXWindowsExceptFocused, NULL, },
	{ { DEFACCEL, "SWS/S&M: Show all floating FX windows (!)" }, "S&M_WNTSHW1", ShowAllFXWindows, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Show all FX chain windows (!)" }, "S&M_WNTSHW2", ShowAllFXChainsWindows, NULL, },
	{ { DEFACCEL, "SWS/S&M: Show all floating FX windows for selected tracks" }, "S&M_WNTSHW3", ShowAllFXWindows, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Toggle show all floating FX (!)" }, "S&M_WNTGL3", ToggleAllFXWindows, NULL, 0, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle show all FX chain windows (!)" }, "S&M_WNTGL4", ToggleAllFXChainsWindows, NULL, -666, GetFakeToggleState},	
	{ { DEFACCEL, "SWS/S&M: Toggle show all floating FX for selected tracks" }, "S&M_WNTGL5", ToggleAllFXWindows, NULL, 1, GetFakeToggleState},

	{ { DEFACCEL, "SWS/S&M: Float previous FX (and close others) for selected tracks" }, "S&M_WNONLY1", CycleFloatFXWndSelTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Float next FX (and close others) for selected tracks" }, "S&M_WNONLY2", CycleFloatFXWndSelTracks, NULL, 1},

	{ { DEFACCEL, "SWS/S&M: Focus previous floating FX for selected tracks (cycle)" }, "S&M_WNFOCUS5", CycleFocusFXMainWndSelTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Focus next floating FX for selected tracks (cycle)" }, "S&M_WNFOCUS6", CycleFocusFXMainWndSelTracks, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Focus previous floating FX (cycle)" }, "S&M_WNFOCUS7", CycleFocusFXMainWndAllTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Focus next floating FX (cycle)" }, "S&M_WNFOCUS8", CycleFocusFXMainWndAllTracks, NULL, 1},

	{ { DEFACCEL, "SWS/S&M: Focus main window (only valid within custom actions)" }, "S&M_WNMAIN", FocusMainWindow, NULL, },
#ifdef _WIN32
	{ { DEFACCEL, "SWS/S&M: Focus main window (close others)" }, "S&M_WNMAIN_HIDE_OTHERS", FocusMainWindowCloseOthers, NULL, },
	{ { DEFACCEL, "SWS/S&M: Cycle focused window" }, "S&M_WNFOCUS9", CycleFocusWnd, NULL, },
	{ { DEFACCEL, "SWS/S&M: Focus next window (cycle, hide/unhide others)" }, "S&M_WNFOCUS_NEXT", CycleFocusHideOthersWnd, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Focus previous window (cycle, hide/unhide others)" }, "S&M_WNFOCUS_PREV", CycleFocusHideOthersWnd, NULL, -1},
#endif
	{ { DEFACCEL, "SWS/S&M: Show FX chain for selected tracks (selected FX)" }, "S&M_SHOWFXCHAINSEL", ShowFXChain, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Hide FX chain windows for selected tracks" }, "S&M_HIDEFXCHAIN", HideFXChain, NULL, },
	{ { DEFACCEL, "SWS/S&M: Toggle show FX chain windows for selected tracks" }, "S&M_TOGLFXCHAIN", ToggleFXChain, NULL, -666, IsToggleFXChain},

	{ { DEFACCEL, "SWS/S&M: Float selected FX for selected tracks" }, "S&M_FLOATFXEL", FloatFX, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Unfloat selected FX for selected tracks" }, "S&M_UNFLOATFXEL", UnfloatFX, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Toggle float selected FX for selected tracks" }, "S&M_TOGLFLOATFXEL", ToggleFloatFX, NULL, -1, GetFakeToggleState},

	// Track FX: selection, remove fx, move fx up/down in chain ------------------
	{ { DEFACCEL, "SWS/S&M: Select last FX for selected tracks" }, "S&M_SEL_LAST_FX", SelectTrackFX, NULL, -3},
	{ { DEFACCEL, "SWS/S&M: Select previous FX (cycling) for selected tracks" }, "S&M_SELFXPREV", SelectTrackFX, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Select next FX (cycling) for selected tracks" }, "S&M_SELFXNEXT", SelectTrackFX, NULL, -1},

	{ { DEFACCEL, "SWS/S&M: Move selected FX up in chain for selected tracks" }, "S&M_MOVE_FX_UP", MoveOrRemoveTrackFX, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Move selected FX down in chain for selected tracks" }, "S&M_MOVE_FX_DOWN", MoveOrRemoveTrackFX, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Remove selected FX for selected tracks" }, "S&M_REMOVE_FX", MoveOrRemoveTrackFX, NULL, 0},

	// Track FX online/offline & bypass/unbypass ------------------------------
	{ { DEFACCEL, "SWS/S&M: Toggle last FX online/offline for selected tracks" }, "S&M_FXOFFLAST", ToggleFXOfflineSelTracks, NULL, -2, IsFXOfflineSelTracks},
	{ { DEFACCEL, "SWS/S&M: Toggle selected FX online/offline for selected tracks" }, "S&M_FXOFFSEL", ToggleFXOfflineSelTracks, NULL, -1, IsFXOfflineSelTracks},
	{ { DEFACCEL, "SWS/S&M: Set last FX online for selected tracks" }, "S&M_FXOFF_SETONLAST", SetFXOnlineSelTracks, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Set selected FX online for selected tracks" }, "S&M_FXOFF_SETONSEL", SetFXOnlineSelTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Set last FX offline for selected tracks" }, "S&M_FXOFF_SETOFFLAST", SetFXOfflineSelTracks, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Set selected FX offline for selected tracks" }, "S&M_FXOFF_SETOFFSEL", SetFXOfflineSelTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Toggle all FX online/offline for selected tracks" }, "S&M_FXOFFALL", ToggleAllFXsOfflineSelTracks, NULL, -666, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle last FX bypass for selected tracks" }, "S&M_FXBYPLAST", ToggleFXBypassSelTracks, NULL, -2, IsFXBypassedSelTracks},
	{ { DEFACCEL, "SWS/S&M: Toggle selected FX bypass for selected tracks" }, "S&M_FXBYPSEL", ToggleFXBypassSelTracks, NULL, -1, IsFXBypassedSelTracks},
	{ { DEFACCEL, "SWS/S&M: Bypass last FX for selected tracks" }, "S&M_FXBYP_SETONLAST", BypassFXSelTracks, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Bypass selected FX for selected tracks" }, "S&M_FXBYP_SETONSEL", BypassFXSelTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Unbypass last FX for selected tracks" }, "S&M_FXBYP_SETOFFLAST", UnbypassFXSelTracks, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Unbypass selected FX for selected tracks" }, "S&M_FXBYP_SETOFFSEL", UnbypassFXSelTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Toggle all FX bypass for selected tracks" }, "S&M_FXBYPALL", ToggleAllFXsBypassSelTracks, NULL, -666, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Bypass all FX for selected tracks" }, "S&M_FXBYPALL2", UpdateAllFXsBypassSelTracks, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Unbypass all FX for selected tracks" }, "S&M_FXBYPALL3", UpdateAllFXsBypassSelTracks, NULL, 1},
	// note: set all fx online/offline actions exist natively ^^
	{ { DEFACCEL, "SWS/S&M: Toggle all FX (except selected) online/offline for selected tracks" }, "S&M_FXOFFEXCPTSEL", ToggleExceptFXOfflineSelTracks, NULL, -1, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle all FX (except selected) bypass for selected tracks" }, "S&M_FXBYPEXCPTSEL", ToggleExceptFXBypassSelTracks, NULL, -1, GetFakeToggleState},

	// Take FX online/offline & bypass/unbypass ------------------------------
	{ { DEFACCEL, "SWS/S&M: Toggle all take FX online/offline for selected items" }, "S&M_TGL_TAKEFX_ONLINE", ToggleAllFXsOfflineSelItems, NULL, -666, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Set all take FX offline for selected items" }, "S&M_TAKEFX_OFFLINE", UpdateAllFXsOfflineSelItems, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Set all take FX online for selected items" }, "S&M_TAKEFX_ONLINE", UpdateAllFXsOfflineSelItems, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Toggle all take FX bypass for selected items" }, "S&M_TGL_TAKEFX_BYP", ToggleAllFXsBypassSelItems, NULL, -666, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Bypass all take FX for selected items" }, "S&M_TAKEFX_BYPASS", UpdateAllFXsBypassSelItems, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Unbypass all take FX for selected items" }, "S&M_TAKEFX_UNBYPASS", UpdateAllFXsBypassSelItems, NULL, 0},

	// FX presets -------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Trigger preset for selected FX of selected track (MIDI/OSC only)" }, "S&M_SELFX_PRESET", NULL, NULL, -1, NULL, 0, TriggerFXPresetSelTrack, },

	// FX Chains (items & tracks) ---------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Copy FX chain from selected item" }, "S&M_COPYFXCHAIN1", CopyTakeFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Cut FX chain from selected items" }, "S&M_COPYFXCHAIN2", CutTakeFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Paste (replace) FX chain to selected items" }, "S&M_COPYFXCHAIN3", SetTakeFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Paste (replace) FX chain to selected items, all takes" }, "S&M_COPYFXCHAIN4", SetAllTakesFXChain, NULL, }, 

	{ { DEFACCEL, "SWS/S&M: Copy FX chain from selected track" }, "S&M_COPYFXCHAIN5", CopyTrackFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Cut FX chain from selected tracks" }, "S&M_COPYFXCHAIN6", CutTrackFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Paste (replace) FX chain to selected tracks" }, "S&M_COPYFXCHAIN7", SetTrackFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Paste FX chain to selected items" }, "S&M_COPYFXCHAIN8", PasteTakeFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Paste FX chain to selected items, all takes" }, "S&M_COPYFXCHAIN9", PasteAllTakesFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Paste FX chain to selected tracks" }, "S&M_COPYFXCHAIN10", PasteTrackFXChain, NULL, }, 

	{ { DEFACCEL, "SWS/S&M: Copy input FX chain from selected track" }, "S&M_COPY_INFXCHAIN", CopyTrackInputFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Cut input FX chain from selected tracks" }, "S&M_CUT_INFXCHAIN", CutTrackInputFXChain, NULL, },
	{ { DEFACCEL, "SWS/S&M: Paste (replace) input FX chain to selected tracks" }, "S&M_PASTE_REPLACE_INFXCHAIN", SetTrackInputFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Paste input FX chain to selected tracks" }, "S&M_PASTE_INFXCHAIN", PasteTrackInputFXChain, NULL, }, 

	{ { DEFACCEL, "SWS/S&M: Clear FX chain for selected items" },  "S&M_CLRFXCHAIN1", ClearActiveTakeFXChain, NULL, },
	{ { DEFACCEL, "SWS/S&M: Clear FX chain for selected items, all takes" }, "S&M_CLRFXCHAIN2", ClearAllTakesFXChain, NULL, },
	{ { DEFACCEL, "SWS/S&M: Clear FX chain for selected tracks" }, "S&M_CLRFXCHAIN3", ClearTrackFXChain, NULL, },
	{ { DEFACCEL, "SWS/S&M: Clear input FX chain for selected tracks" }, "S&M_CLR_INFXCHAIN", ClearTrackInputFXChain, NULL, },

	{ { DEFACCEL, "SWS/S&M: Copy FX chain (depending on focus)" }, "S&M_SMART_CPY_FXCHAIN", SmartCopyFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Paste FX chain (depending on focus)" }, "S&M_SMART_PST_FXCHAIN", SmartPasteFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Paste (replace) FX chain (depending on focus)" }, "S&M_SMART_SET_FXCHAIN", SmartPasteReplaceFXChain, NULL, }, 
	{ { DEFACCEL, "SWS/S&M: Cut FX chain (depending on focus)" }, "S&M_SMART_CUT_FXCHAIN", SmartCutFXChain, NULL, }, 

	// Projects --------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Open project path in explorer/finder" }, "S&M_OPEN_PRJ_PATH", OpenProjectPathInExplorerFinder, NULL, },
	{ { DEFACCEL, "SWS/S&M: Select project (MIDI/OSC only)" }, "S&M_SELECT_PROJECT", NULL, NULL, 0, NULL, 0, SelectProject, },

	{ { DEFACCEL, "SWS/S&M: Insert silence (seconds)" }, "S&M_INSERT_SILENCE_S", InsertSilence, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Insert silence (measures.beats)" }, "S&M_INSERT_SILENCE_MB", InsertSilence, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Insert silence (samples)" }, "S&M_INSERT_SILENCE_SMP", InsertSilence, NULL, 2},

	{ { DEFACCEL, "SWS/S&M: Set project startup action" }, "S&M_SET_PRJ_ACTION", SetStartupAction, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Clear project startup action" }, "S&M_CLR_PRJ_ACTION", ClearStartupAction, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Show project/global startup actions" }, "S&M_SHOW_PRJ_ACTION", ShowStartupActions, NULL, },
	// not project actions, but shared code
	{ { DEFACCEL, "SWS/S&M: Set global startup action" }, "S&M_SET_STARTUP_ACTION", SetStartupAction, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Clear global startup action" }, "S&M_CLR_STARTUP_ACTION", ClearStartupAction, NULL, 1},

	// Images ------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Open/close image window" }, "S&M_OPEN_IMAGEVIEW", OpenImageWnd, NULL, 0, IsImageWndDisplayed},
	{ { DEFACCEL, "SWS/S&M: Clear image window" }, "S&M_CLR_IMAGEVIEW", ClearImageWnd, NULL, },

	// Resources --------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Open/close Resources window" }, "S&M_SHOW_RESOURCES_VIEW", OpenResources, NULL, -1, IsResourcesDisplayed},

	{ { DEFACCEL, "SWS/S&M: Open/close Resources window (FX chains)" }, "S&M_SHOWFXCHAINSLOTS", OpenResources, NULL, SNM_SLOT_FXC, IsResourcesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete last FX chain slot/file" }, "S&M_DEL_LAST_FXCHAINSLOT", ResourcesDeleteLastSlot, NULL, SNM_SLOT_FXC},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete all FX chain slots" }, "S&M_DEL_ALL_FXCHAINSLOT", ResourcesDeleteAllSlots, NULL, SNM_SLOT_FXC},
	//JFB!! auto-save actions are poorly identified: should have been S&M_SAVE_FXCHAIN1, S&M_SAVE_FXCHAIN2, etc..
	{ { DEFACCEL, "SWS/S&M: Resources - Auto-save FX chains for selected tracks" }, "S&M_SAVE_FXCHAIN_SLOT1", ResourcesAutoSaveFXChain, NULL, FXC_AUTOSAVE_PREF_TRACK},
	{ { DEFACCEL, "SWS/S&M: Resources - Auto-save FX chains for selected items" }, "S&M_SAVE_FXCHAIN_SLOT2", ResourcesAutoSaveFXChain, NULL, FXC_AUTOSAVE_PREF_ITEM},
	{ { DEFACCEL, "SWS/S&M: Resources - Auto-save input FX chains for selected tracks" }, "S&M_SAVE_FXCHAIN_SLOT3", ResourcesAutoSaveFXChain, NULL, FXC_AUTOSAVE_PREF_INPUT_FX},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste (replace) FX chain to selected items, last slot" }, "S&M_TAKEFXCHAINl1", LoadSetTakeFXChainSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste (replace) FX chain to selected items, all takes, last slot" }, "S&M_TAKEFXCHAINl2", LoadSetAllTakesFXChainSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste FX chain to selected items, last slot" }, "S&M_PASTE_TAKEFXCHAINl1", LoadPasteTakeFXChainSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste FX chain to selected items, all takes, last slot" }, "S&M_PASTE_TAKEFXCHAINl2", LoadPasteAllTakesFXChainSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste (replace) FX chain to selected tracks, last slot" }, "S&M_TRACKFXCHAINl1", LoadSetTrackFXChainSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste FX chain to selected tracks, last slot" }, "S&M_PASTE_TRACKFXCHAINl1", LoadPasteTrackFXChainSlot, NULL, -2},

	{ { DEFACCEL, "SWS/S&M: Open/close Resources window (track templates)" }, "S&M_SHOW_RESVIEW_TR_TEMPLATES", OpenResources, NULL, SNM_SLOT_TR, IsResourcesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete last track template slot/file" }, "S&M_DEL_LAST_TRTEMPLATE_SLOT", ResourcesDeleteLastSlot, NULL, SNM_SLOT_TR},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete all track template slots" }, "S&M_DEL_ALL_TRTEMPLATE_SLOT", ResourcesDeleteAllSlots, NULL, SNM_SLOT_TR},
	//JFB!! auto-save actions are poorly identified: should have been S&M_SAVE_TRTEMPLATE1, S&M_SAVE_TRTEMPLATE2, etc..
	{ { DEFACCEL, "SWS/S&M: Resources - Auto-save track template" }, "S&M_SAVE_TRTEMPLATE_SLOT1", ResourcesAutoSaveTrTemplate, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Resources - Auto-save track template (with items, envelopes) " }, "S&M_SAVE_TRTEMPLATE_SLOT2", ResourcesAutoSaveTrTemplate, NULL, 3},
	{ { DEFACCEL, "SWS/S&M: Resources - Auto-save track template (with envelopes) " }, "S&M_SAVE_TRTEMPLATE_SLOT3", ResourcesAutoSaveTrTemplate, NULL, 2},
	{ { DEFACCEL, "SWS/S&M: Resources - Auto-save track template (with items)" }, "S&M_SAVE_TRTEMPLATE_SLOT4", ResourcesAutoSaveTrTemplate, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Resources - Import tracks from track template, last slot" }, "S&M_ADD_TRTEMPLATEl", LoadImportTrackTemplateSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Apply track template to selected tracks, last slot" }, "S&M_APPLY_TRTEMPLATEl", LoadApplyTrackTemplateSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Apply track template (+envelopes/items) to selected tracks, last slot" }, "S&M_APPLY_TRTEMPLATE_ITEMSENVSl", LoadApplyTrackTemplateSlotWithItemsEnvs, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste (replace) template items to selected tracks, last slot" }, "S&M_REPLACE_TEMPLATE_ITEMSl", ReplaceItemsTrackTemplateSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste template items to selected tracks, last slot" }, "S&M_PASTE_TEMPLATE_ITEMSl", PasteItemsTrackTemplateSlot, NULL, -2},

	{ { DEFACCEL, "SWS/S&M: Open/close Resources window (projects)" }, "S&M_SHOW_RESVIEW_PRJ_TEMPLATES", OpenResources, NULL, SNM_SLOT_PRJ, IsResourcesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete last project slot/file" }, "S&M_DEL_LAST_PRJTEMPLATE_SLOT", ResourcesDeleteLastSlot, NULL, SNM_SLOT_PRJ},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete all project slots" }, "S&M_DEL_ALL_PRJTEMPLATE_SLOT", ResourcesDeleteAllSlots, NULL, SNM_SLOT_PRJ},
	{ { DEFACCEL, "SWS/S&M: Resources - Auto-save project" }, "S&M_SAVE_PRJTEMPLATE_SLOT", ResourcesAutoSave, NULL, SNM_SLOT_PRJ},
	{ { DEFACCEL, "SWS/S&M: Resources - Open project, last slot" }, "S&M_APPLY_PRJTEMPLATEl", LoadOrSelectProjectSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Open project, last slot (new tab)" }, "S&M_NEWTAB_PRJTEMPLATEl", LoadOrSelectProjectTabSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Open project, next slot (cycle)" }, "S&M_PRJ_NEXT_SLOT", LoadOrSelectNextPreviousProjectSlot, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Resources - Open project, previous slot (cycle)" }, "S&M_PRJ_PREV_SLOT", LoadOrSelectNextPreviousProjectSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Open project, next slot (new tab, cycle)" }, "S&M_PRJ_NEXT_TAB_SLOT", LoadOrSelectNextPreviousProjectTabSlot, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Resources - Open project, previous slot (new tab, cycle)" }, "S&M_PRJ_PREV_TAB_SLOT", LoadOrSelectNextPreviousProjectTabSlot, NULL, -1},

	{ { DEFACCEL, "SWS/S&M: Open/close Resources window (media files)" }, "S&M_SHOW_RESVIEW_MEDIA", OpenResources, NULL, SNM_SLOT_MEDIA, IsResourcesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Resources - Stop all playing media files" }, "S&M_STOPMEDIA_ALLTRACK", StopTrackPreviews, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Resources - Stop all playing media files in selected tracks" }, "S&M_STOPMEDIA_SELTRACK", StopTrackPreviews, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete last media file slot/file" }, "S&M_DEL_LAST_MEDIA_SLOT", ResourcesDeleteLastSlot, NULL, SNM_SLOT_MEDIA},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete all media file slots" }, "S&M_DEL_ALL_MEDIA_SLOT", ResourcesDeleteAllSlots, NULL, SNM_SLOT_MEDIA},
	{ { DEFACCEL, "SWS/S&M: Resources - Auto-save media files for selected items" }, "S&M_SAVE_MEDIA_SLOT", ResourcesAutoSave, NULL, SNM_SLOT_MEDIA},
	{ { DEFACCEL, "SWS/S&M: Resources - Play media file in selected tracks, last slot" }, "S&M_PLAYMEDIA_SELTRACKl", PlaySelTrackMediaSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Loop media file in selected tracks, last slot" }, "S&M_LOOPMEDIA_SELTRACKl", LoopSelTrackMediaSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Play media file in selected tracks (toggle), last slot" }, "S&M_TGL_PLAYMEDIA_SELTRACKl", TogglePlaySelTrackMediaSlot, NULL, -2, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Resources - Loop media file in selected tracks (toggle), last slot" }, "S&M_TGL_LOOPMEDIA_SELTRACKl", ToggleLoopSelTrackMediaSlot, NULL, -2, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Resources - Play media file in selected tracks (toggle pause), last slot" }, "S&M_TGLPAUSE_PLAYMEDIA_SELTRACKl", TogglePauseSelTrackMediaSlot, NULL, -2, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Resources - Loop media file in selected tracks (toggle pause), last slot" }, "S&M_TGLPAUSE_LOOPMEDIA_SELTRACKl", ToggleLoopPauseSelTrackMediaSlot, NULL, -2, GetFakeToggleState},

	{ { DEFACCEL, "SWS/S&M: Resources - Add media file to current track, last slot" }, "S&M_ADDMEDIA_CURTRACKl", InsertMediaSlotCurTr, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Add media file to new track, last slot" }, "S&M_ADDMEDIA_NEWTRACKl", InsertMediaSlotNewTr, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Add media file to selected items as takes, last slot" }, "S&M_ADDMEDIA_SELITEMl", InsertMediaSlotTakes, NULL, -2},

	{ { DEFACCEL, "SWS/S&M: Resources - Set \"Add media file\" option to \"Default\"" }, "S&M_ADDMEDIA_OPT0", SetMediaOption, NULL, 0, IsMediaOption},
	{ { DEFACCEL, "SWS/S&M: Resources - Set \"Add media file\" option to \"Stretch/loop to fit time sel\"" }, "S&M_ADDMEDIA_OPT1", SetMediaOption, NULL, 1, IsMediaOption},
	{ { DEFACCEL, "SWS/S&M: Resources - Set \"Add media file\" option to \"Try to match tempo 0.5x\"" }, "S&M_ADDMEDIA_OPT2", SetMediaOption, NULL, 2, IsMediaOption},
	{ { DEFACCEL, "SWS/S&M: Resources - Set \"Add media file\" option to \"Try to match tempo 1x\"" }, "S&M_ADDMEDIA_OPT3", SetMediaOption, NULL, 3, IsMediaOption},
	{ { DEFACCEL, "SWS/S&M: Resources - Set \"Add media file\" option to \"Try to match tempo 2x\"" }, "S&M_ADDMEDIA_OPT4", SetMediaOption, NULL, 4, IsMediaOption},

	{ { DEFACCEL, "SWS/S&M: Open/close Resources window (images)" }, "S&M_SHOW_RESVIEW_IMAGE", OpenResources, NULL, SNM_SLOT_IMG, IsResourcesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Resources - Show next image slot" }, "S&M_SHOW_NEXT_IMG", ShowNextPreviousImageSlot, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Resources - Show previous image slot" }, "S&M_SHOW_PREV_IMG", ShowNextPreviousImageSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete last image slot/file" }, "S&M_DEL_LAST_IMAGE_SLOT", ResourcesDeleteLastSlot, NULL, SNM_SLOT_IMG},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete all image slots" }, "S&M_DEL_ALL_IMAGE_SLOT", ResourcesDeleteAllSlots, NULL, SNM_SLOT_IMG},
	{ { DEFACCEL, "SWS/S&M: Resources - Show image, last slot" }, "S&M_SHOW_IMAGEl", ShowImageSlot, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Resources - Set track icon for selected tracks, last slot" }, "S&M_SET_TRACK_ICONl", SetSelTrackIconSlot, NULL, -2},

	{ { DEFACCEL, "SWS/S&M: Open/close Resources window (themes)" }, "S&M_SHOW_RESVIEW_THEME", OpenResources, NULL, SNM_SLOT_THM, IsResourcesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete last theme slot/file" }, "S&M_DEL_LAST_THEME_SLOT", ResourcesDeleteLastSlot, NULL, SNM_SLOT_THM},
	{ { DEFACCEL, "SWS/S&M: Resources - Delete all theme slots" }, "S&M_DEL_ALL_THEME_SLOT", ResourcesDeleteAllSlots, NULL, SNM_SLOT_THM},
	{ { DEFACCEL, "SWS/S&M: Resources - Load theme, last slot" }, "S&M_LOAD_THEMEl", LoadThemeSlot, NULL, -2},

	// FX presets -------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Trigger next preset for selected FX of selected tracks" }, "S&M_NEXT_SELFX_PRESET", NextPresetSelTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Trigger previous preset for selected FX of selected tracks" }, "S&M_PREVIOUS_SELFX_PRESET", PrevPresetSelTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Trigger next preset for last touched FX" }, "S&M_NEXT_FOCFX_PRESET", NextPrevPresetLastTouchedFX, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Trigger previous preset for last touched FX" }, "S&M_PREV_FOCFX_PRESET", NextPrevPresetLastTouchedFX, NULL, -1},
	
	// MIDI learn -------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Reassign MIDI learned channels of all FX for selected tracks (prompt)" }, "S&M_ALL_FX_LEARN_CHp", ReassignLearntMIDICh, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Reassign MIDI learned channels of selected FX for selected tracks (prompt)" }, "S&M_SEL_FX_LEARN_CHp", ReassignLearntMIDICh, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Reassign MIDI learned channels of all FX to input channel for selected tracks" }, "S&M_ALLIN_FX_LEARN_CHp", ReassignLearntMIDICh, NULL, -3},
	
	// Items ------------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Scroll to selected item (no undo)" }, "S&M_SCROLL_ITEM", ScrollToSelItem, NULL, },
	{ { DEFACCEL, "SWS/S&M: Open selected item path in explorer/finder" }, "S&M_OPEN_ITEM_PATH", OpenMediaPathInExplorerFinder, NULL, },

	// Takes ------------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Pan active takes of selected items to 100% left" }, "S&M_PAN_TAKES_100L", SetPan, NULL, -100},
	{ { DEFACCEL, "SWS/S&M: Pan active takes of selected items to 75% left" }, "S&M_PAN_TAKES_75L", SetPan, NULL, -75},
	{ { DEFACCEL, "SWS/S&M: Pan active takes of selected items to 50% left" }, "S&M_PAN_TAKES_50L", SetPan, NULL, -50},
	{ { DEFACCEL, "SWS/S&M: Pan active takes of selected items to 25% left" }, "S&M_PAN_TAKES_25L", SetPan, NULL, -25},
	{ { DEFACCEL, "SWS/S&M: Pan active takes of selected items to center" }, "S&M_PAN_TAKES_CENTER", SetPan, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Pan active takes of selected items to 25% right" }, "S&M_PAN_TAKES_25R", SetPan, NULL, 25},
	{ { DEFACCEL, "SWS/S&M: Pan active takes of selected items to 50% right" }, "S&M_PAN_TAKES_50R", SetPan, NULL, 50},
	{ { DEFACCEL, "SWS/S&M: Pan active takes of selected items to 75% right" }, "S&M_PAN_TAKES_75R", SetPan, NULL, 75},
	{ { DEFACCEL, "SWS/S&M: Pan active takes of selected items to 100% right" }, "S&M_PAN_TAKES_100R", SetPan, NULL, 100},

	{ { DEFACCEL, "SWS/S&M: Copy active takes" }, "S&M_COPY_TAKE", CopyCutTakes, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Cut active takes" }, "S&M_CUT_TAKE", CopyCutTakes, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Paste takes" }, "S&M_PASTE_TAKE", PasteTakes, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Paste takes (after active takes)" }, "S&M_PASTE_TAKE_AFTER", PasteTakes, NULL, 1},

	{ { DEFACCEL, "SWS/S&M: Takes - Clear active takes/items" }, "S&M_CLRTAKE1", ClearTake, NULL, },
	{ { DEFACCEL, "SWS/S&M: Takes - Activate lanes from selected items" }, "S&M_LANETAKE2", ActivateLaneFromSelItem, NULL, },
	{ { DEFACCEL, "SWS/S&M: Takes - Activate lane under mouse cursor" }, "S&M_LANETAKE4", ActivateLaneUnderMouse, NULL, },
	{ { DEFACCEL, "SWS/S&M: Takes - Remove empty takes/items among selected items" }, "S&M_DELEMPTYTAKE", RemoveAllEmptyTakes /*RemoveEmptyTakes*/, NULL, },
	{ { DEFACCEL, "SWS/S&M: Takes - Remove empty MIDI takes/items among selected items" }, "S&M_DELEMPTYTAKE2", RemoveEmptyMidiTakes, NULL, },
	{ { DEFACCEL, "SWS/S&M: Takes - Move active up (cycling) in selected items" }, "S&M_MOVETAKE3", MoveActiveTake, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Takes - Move active down (cycling) in selected items" }, "S&M_MOVETAKE4", MoveActiveTake, NULL, 1},

	{ { DEFACCEL, "SWS/S&M: Delete selected items' takes and source files (prompt, no undo)" }, "S&M_DELTAKEANDFILE1", DeleteTakeAndMedia, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Delete selected items' takes and source files (no undo)" }, "S&M_DELTAKEANDFILE2", DeleteTakeAndMedia, NULL, 2},
	{ { DEFACCEL, "SWS/S&M: Delete active take and source file in selected items (prompt, no undo)" }, "S&M_DELTAKEANDFILE3", DeleteTakeAndMedia, NULL, 3},
	{ { DEFACCEL, "SWS/S&M: Delete active take and source file in selected items (no undo)" }, "S&M_DELTAKEANDFILE4", DeleteTakeAndMedia, NULL, 4},

	// Notes ------------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window" }, "S&M_SHOW_NOTES_VIEW", OpenNotes, NULL, -1, IsNotesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (project notes)" }, "S&M_SHOWNOTESHELP", OpenNotes, NULL, SNM_NOTES_PROJECT, IsNotesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (extra project notes)" }, "S&M_EXTRAPROJECTNOTES", OpenNotes, NULL, SNM_NOTES_PROJECT_EXTRA, IsNotesDisplayed},
	// #647
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (global notes)" }, "S&M_GLOBALNOTES", OpenNotes, NULL, SNM_NOTES_GLOBAL, IsNotesDisplayed },
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (item notes)" }, "S&M_ITEMNOTES", OpenNotes, NULL, SNM_NOTES_ITEM, IsNotesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (track notes)" }, "S&M_TRACKNOTES", OpenNotes, NULL, SNM_NOTES_TRACK, IsNotesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (marker names)" }, "S&M_MKR_NAMES", OpenNotes, NULL, SNM_NOTES_MKR_NAME, IsNotesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (region names)" }, "S&M_RGN_NAMES", OpenNotes, NULL, SNM_NOTES_RGN_NAME, IsNotesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (marker/region names)" }, "S&M_MARKERNAMES", OpenNotes, NULL, SNM_NOTES_MKRRGN_NAME, IsNotesDisplayed}, // custom id poorly named for historical reasons...
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (marker subtitles)" }, "S&M_MKR_SUBTITLES", OpenNotes, NULL, SNM_NOTES_MKR_SUB, IsNotesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (region subtitles)" }, "S&M_RGN_SUBTITLES", OpenNotes, NULL, SNM_NOTES_RGN_SUB, IsNotesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (marker/region subtitles)" }, "S&M_MARKERSUBTITLES", OpenNotes, NULL, SNM_NOTES_MKRRGN_SUB, IsNotesDisplayed}, // custom id poorly named for historical reasons...
#if defined(_WIN32) && defined(WANT_ACTION_HELP)
	{ { DEFACCEL, "SWS/S&M: Open/close Notes window (action help)" }, "S&M_ACTIONHELP", OpenNotes, NULL, SNM_NOTES_ACTION_HELP, IsNotesDisplayed},
	{ { DEFACCEL, "SWS/S&M: Notes - Set action help file..." }, "S&M_ACTIONHELPPATH", SetActionHelpFilename, NULL, },
#endif
	{ { DEFACCEL, "SWS/S&M: Notes - Toggle lock" }, "S&M_ACTIONHELPTGLOCK", ToggleNotesLock, NULL, 0, IsNotesLocked},
	{ { DEFACCEL, "SWS/S&M: Notes - Import subtitle file..." }, "S&M_IMPORT_SUBTITLE", ImportSubTitleFile, NULL, },
	{ { DEFACCEL, "SWS/S&M: Notes - Export subtitle file..." }, "S&M_EXPORT_SUBTITLE", ExportSubTitleFile, NULL, },

	// Split ------------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Split selected items at edit cursor (MIDI) or prior zero crossing (audio)" }, "S&M_SPLIT1", SplitMidiAudio, NULL, },
	{ { DEFACCEL, "SWS/S&M: Split selected items at time selection, edit cursor (MIDI) or prior zero crossing (audio)" }, "S&M_SPLIT2", SmartSplitMidiAudio, NULL, },
	{ { DEFACCEL, "SWS/gofer: Split selected items at mouse cursor (obey snapping)" }, "S&M_SPLIT10", GoferSplitSelectedItems, NULL, },
	{ { DEFACCEL, "SWS/S&M: Split and select items in region near cursor" }, "S&M_SPLIT11", SplitSelectAllItemsInRegion, NULL, },

	// ME ---------------------------------------------------------------------
	// outdated, kept for asc. compatibility: those versions are registered in the main section
	{ { DEFACCEL, "SWS/S&M: Active MIDI Editor - Hide all CC lanes" }, "S&M_MEHIDECCLANES", MainHideCCLanes, NULL, },
	{ { DEFACCEL, "SWS/S&M: Active MIDI Editor - Create CC lane" }, "S&M_MECREATECCLANE", MainCreateCCLane, NULL, },
	// those versions are registered in the ME section
	{ { DEFACCEL, "SWS/S&M: Hide all CC lanes" }, "S&M_HIDECCLANES_ME", NULL, NULL, 0, NULL, 32060, MEHideCCLanes, },
	{ { DEFACCEL, "SWS/S&M: Create CC lane" }, "S&M_CREATECCLANE_ME", NULL, NULL, 0, NULL, 32060, MECreateCCLane, },

	// Tracks -----------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Copy selected track grouping" }, "S&M_COPY_TR_GRP", CopyCutTrackGrouping, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Cut selected tracks grouping" }, "S&M_CUT_TR_GRP", CopyCutTrackGrouping, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Paste grouping to selected tracks" }, "S&M_PASTE_TR_GRP", PasteTrackGrouping, NULL, },
	{ { DEFACCEL, "SWS/S&M: Remove track grouping for selected tracks" }, "S&M_REMOVE_TR_GRP", RemoveTrackGrouping, NULL, },
	{ { DEFACCEL, "SWS/S&M: Set selected tracks to first unused group (default flags)" }, "S&M_SET_TRACK_UNUSEDGROUP", SetTrackToFirstUnusedGroup, NULL, },

	{ { DEFACCEL, "SWS/S&M: Save selected tracks folder states" }, "S&M_SAVEFOLDERSTATE1", SaveTracksFolderStates, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Restore selected tracks folder states" }, "S&M_RESTOREFOLDERSTATE1", RestoreTracksFolderStates, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Set selected tracks folder states to last of all folders" }, "S&M_FOLDER_LAST_ALL", SetTracksFolderState, NULL, -2},
	{ { DEFACCEL, "SWS/S&M: Set selected tracks folder states to last in folder" }, "S&M_FOLDER_LAST", SetTracksFolderState, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Set selected tracks folder states to parent" }, "S&M_FOLDERON", SetTracksFolderState, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Set selected tracks folder states to normal" }, "S&M_FOLDEROFF", SetTracksFolderState, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Save selected tracks folder compact states" }, "S&M_SAVEFOLDERSTATE2", SaveTracksFolderStates, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Restore selected tracks folder compact states" }, "S&M_RESTOREFOLDERSTATE2", RestoreTracksFolderStates, NULL, 1},

	// Take envelopes ---------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Show and unbypass take volume envelope" }, "S&M_TAKEENV1", BypassUnbypassShowHideTakeVolEnvelope, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Show and unbypass take pan envelope" }, "S&M_TAKEENV2", BypassUnbypassShowHideTakePanEnvelope, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Show and unbypass take mute envelope" }, "S&M_TAKEENV3", BypassUnbypassShowHideTakeMuteEnvelope, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Hide and bypass take volume envelope" }, "S&M_TAKEENV4", BypassUnbypassShowHideTakeVolEnvelope, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Hide and bypass take pan envelope" }, "S&M_TAKEENV5", BypassUnbypassShowHideTakePanEnvelope, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Hide and bypass take mute envelope" }, "S&M_TAKEENV6", BypassUnbypassShowHideTakeMuteEnvelope, NULL, 0},

	{ { DEFACCEL, "SWS/S&M: Show take volume envelope" }, "S&M_TAKEENVSHOW1", ShowHideTakeVolEnvelope, NULL, 1 },
	{ { DEFACCEL, "SWS/S&M: Show take pan envelope" }, "S&M_TAKEENVSHOW2", ShowHideTakePanEnvelope, NULL, 1 },
	{ { DEFACCEL, "SWS/S&M: Show take mute envelope" }, "S&M_TAKEENVSHOW3", ShowHideTakeMuteEnvelope, NULL, 1 },
	{ { DEFACCEL, "SWS/S&M: Hide take volume envelope" }, "S&M_TAKEENVSHOW4", ShowHideTakeVolEnvelope, NULL, 0 },
	{ { DEFACCEL, "SWS/S&M: Hide take pan envelope" }, "S&M_TAKEENVSHOW5", ShowHideTakePanEnvelope, NULL, 0 },
	{ { DEFACCEL, "SWS/S&M: Hide take mute envelope" }, "S&M_TAKEENVSHOW6", ShowHideTakeMuteEnvelope, NULL, 0 },


	{ { DEFACCEL, "SWS/S&M: Set active take pan envelope to 100% right" }, "S&M_TAKEENV_100R", PanTakeEnvelope, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Set active take pan envelope to 100% left" }, "S&M_TAKEENV_100L", PanTakeEnvelope, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Set active take pan envelope to center" }, "S&M_TAKEENV_CENTER", PanTakeEnvelope, NULL, 0},

	{ { DEFACCEL, "SWS/S&M: Show and unbypass take pitch envelope" }, "S&M_TAKEENV10", BypassUnbypassShowHideTakePitchEnvelope, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Hide and bypass take pitch envelope" }, "S&M_TAKEENV11", BypassUnbypassShowHideTakePitchEnvelope, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Show take pitch envelope" }, "S&M_TAKEENVSHOW7", ShowHideTakePitchEnvelope, NULL, 1 },
	{ { DEFACCEL, "SWS/S&M: Hide take pitch envelope" }, "S&M_TAKEENVSHOW8", ShowHideTakePitchEnvelope, NULL, 0 },

	// NF: unlike the deactivated "SWS/S&M: Toggle show take ... envelope" actions below
	// these *only* toggle visibility (no bypass/unbypass) and aren't available natively
	{ { DEFACCEL, "SWS/S&M: Toggle show take volume envelope" }, "S&M_TAKEENVSHOW9", ShowHideTakeVolEnvelope, NULL, -1, GetFakeToggleState },
	{ { DEFACCEL, "SWS/S&M: Toggle show take pan envelope" }, "S&M_TAKEENVSHOW10", ShowHideTakePanEnvelope, NULL, -1, GetFakeToggleState },
	{ { DEFACCEL, "SWS/S&M: Toggle show take mute envelope" }, "S&M_TAKEENVSHOW11", ShowHideTakeMuteEnvelope, NULL, -1, GetFakeToggleState },
	{ { DEFACCEL, "SWS/S&M: Toggle show take pitch envelope" }, "S&M_TAKEENVSHOW12", ShowHideTakePitchEnvelope, NULL, -1, GetFakeToggleState },

	// Track envelopes --------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Remove all envelopes for selected tracks" }, "S&M_REMOVE_ALLENVS", RemoveAllEnvsSelTracksNoChunk, NULL, },
	{ { DEFACCEL, "SWS/S&M: Toggle arming of all active envelopes for selected tracks" }, "S&M_TGLARMALLENVS", ToggleArmTrackEnv, NULL, 0, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Arm all active envelopes for selected tracks" }, "S&M_ARMALLENVS", ToggleArmTrackEnv, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Disarm all active envelopes for selected tracks" }, "S&M_DISARMALLENVS", ToggleArmTrackEnv, NULL, 2},
	{ { DEFACCEL, "SWS/S&M: Toggle arming of volume envelope for selected tracks" }, "S&M_TGLARMVOLENV", ToggleArmTrackEnv, NULL, 3, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle arming of pan envelope for selected tracks" }, "S&M_TGLARMPANENV", ToggleArmTrackEnv, NULL, 4, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle arming of mute envelope for selected tracks" }, "S&M_TGLARMMUTEENV", ToggleArmTrackEnv, NULL, 5, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle arming of all receive volume envelopes for selected tracks" }, "S&M_TGLARMAUXVOLENV", ToggleArmTrackEnv, NULL, 6, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle arming of all receive pan envelopes for selected tracks" }, "S&M_TGLARMAUXPANENV", ToggleArmTrackEnv, NULL, 7, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle arming of all receive mute envelopes for selected tracks" }, "S&M_TGLARMAUXMUTEENV", ToggleArmTrackEnv, NULL, 8, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle arming of all plugin envelopes for selected tracks" }, "S&M_TGLARMPLUGENV", ToggleArmTrackEnv, NULL, 9, GetFakeToggleState},

	{ { DEFACCEL, "SWS/S&M: Select only track with selected envelope" }, "S&M_SELTR_SELENV", SelOnlyTrackWithSelEnv, NULL, },

	// Toolbar ----------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Toggle toolbars auto refresh enable" },	"S&M_TOOLBAR_REFRESH_ENABLE", EnableToolbarsAutoRefesh, NULL, 0, IsToolbarsAutoRefeshEnabled},
	{ { DEFACCEL, "SWS/S&M: Toolbar - Toggle track envelopes in touch/latch/latch preview/write" }, "S&M_TOOLBAR_WRITE_ENV", ToggleWriteEnvExists, NULL, 0, WriteEnvExists},
	{ { DEFACCEL, "SWS/S&M: Toolbar - Toggle offscreen item selection (left)" }, "S&M_TOOLBAR_ITEM_SEL0", ToggleOffscreenSelItems, NULL, SNM_ITEM_SEL_LEFT, HasOffscreenSelItems},
	{ { DEFACCEL, "SWS/S&M: Toolbar - Toggle offscreen item selection (right)" },"S&M_TOOLBAR_ITEM_SEL1", ToggleOffscreenSelItems, NULL, SNM_ITEM_SEL_RIGHT, HasOffscreenSelItems},
	{ { DEFACCEL, "SWS/S&M: Toolbar - Toggle offscreen item selection (top)" }, "S&M_TOOLBAR_ITEM_SEL2", ToggleOffscreenSelItems, NULL, SNM_ITEM_SEL_UP, HasOffscreenSelItems},
	{ { DEFACCEL, "SWS/S&M: Toolbar - Toggle offscreen item selection (bottom)" }, "S&M_TOOLBAR_ITEM_SEL3", ToggleOffscreenSelItems, NULL, SNM_ITEM_SEL_DOWN, HasOffscreenSelItems},
	{ { DEFACCEL, "SWS/S&M: Toolbar - Toggle offscreen item selection" }, "S&M_TGL_OFFSCREEN_ITEMS", ToggleOffscreenSelItems, NULL, -1, HasOffscreenSelItems},
	{ { DEFACCEL, "SWS/S&M: Unselect offscreen items" }, "S&M_UNSEL_OFFSCREEN_ITEMS", UnselectOffscreenItems, NULL, -1, HasOffscreenSelItems}, // -1: trick to share HasOffscreenSelItems() w/ above actions

	// Find -------------------------------------------------------------------
	//JFB removed default shortcut ctrl+F: more future proof..
	{ {/*{FCONTROL|FVIRTKEY,'F',0}*/ DEFACCEL, "SWS/S&M: Find" }, "S&M_SHOWFIND", OpenFind, NULL, 0, IsFindDisplayed},
	{ { DEFACCEL, "SWS/S&M: Find next" }, "S&M_FIND_NEXT", FindNextPrev, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Find previous" }, "S&M_FIND_PREVIOUS", FindNextPrev, NULL, -1},

	// Live Configs -----------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Open/close Live Configs window" }, "S&M_SHOWMIDILIVE", OpenLiveConfig, NULL, 0, IsLiveConfigDisplayed},

	// Cyclactions ---------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Open/close Cycle Action editor" }, "S&M_CYCLEDITOR", OpenCyclaction, NULL, 0, IsCyclactionDisplayed},
	{ { DEFACCEL, "SWS/S&M: Open/close Cycle Action editor (event list)" }, "S&M_CYCLEDITOR_ME_LIST", OpenCyclaction, NULL, 4, IsCyclactionDisplayed},
	{ { DEFACCEL, "SWS/S&M: Open/close Cycle Action editor (piano roll)" }, "S&M_CYCLEDITOR_ME_PIANO", OpenCyclaction, NULL, 3, IsCyclactionDisplayed},

	// REC inputs -------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Set selected tracks MIDI input to all channels" }, "S&M_MIDI_INPUT_ALL_CH", SetMIDIInputChannel, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Map selected tracks MIDI input to source channel" }, "S&M_MAP_MIDI_INPUT_CH_SRC", RemapMIDIInputChannel, NULL, -1},

	// Region playlist --------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Open/close Region Playlist window" }, "S&M_SHOW_RGN_PLAYLIST", OpenRegionPlaylist, NULL, 0, IsRegionPlaylistDisplayed},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Toggle monitoring/edition mode" }, "S&M_TGL_RGN_PLAYLIST_MODE", ToggleRegionPlaylistLock, NULL, 0, IsRegionPlaylistMonitoring},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Play" }, "S&M_PLAY_RGN_PLAYLIST", PlaylistPlay, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Play previous region (smooth seek)" }, "S&M_PLAY_PREV_RGN_PLAYLIST", PlaylistSeekPrevNext, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Play next region (smooth seek)" }, "S&M_PLAY_NEXT_RGN_PLAYLIST", PlaylistSeekPrevNext, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Play previous region (based on current playing region)" }, "S&M_PLAY_PREV_CUR_BASED_RGN_PLAYLIST", PlaylistSeekPrevNextCurBased, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Play next region (based on current playing region)" }, "S&M_PLAY_NEXT_CUR_BASED_RGN_PLAYLIST", PlaylistSeekPrevNextCurBased, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Crop project to playlist" }, "S&M_CROP_RGN_PLAYLIST1", AppendPasteCropPlaylist, NULL, CROP_PROJECT},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Crop project to playlist (new project tab)" }, "S&M_CROP_RGN_PLAYLIST2", AppendPasteCropPlaylist, NULL, CROP_PROJECT_TAB},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Append playlist to project" }, "S&M_APPEND_RGN_PLAYLIST", AppendPasteCropPlaylist, NULL, PASTE_PROJECT},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Paste playlist at edit cursor" }, "S&M_PASTE_RGN_PLAYLIST", AppendPasteCropPlaylist, NULL, PASTE_CURSOR},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Set repeat off" }, "S&M_PLAYLIST_REPEAT_ON", SetPlaylistRepeat, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Set repeat on" }, "S&M_PLAYLIST_REPEAT_OFF", SetPlaylistRepeat, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Toggle repeat" }, "S&M_PLAYLIST_TGL_REPEAT", SetPlaylistRepeat, NULL, -1, IsPlaylistRepeat},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Options/Enable smooth seek (only in Region Playlist)" }, "S&M_PLAYLIST_OPT_SMOOTHSEEK_ON", SetPlaylistOptionSmoothSeek, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Options/Disable smooth seek (only in Region Playlist)" }, "S&M_PLAYLIST_OPT_SMOOTHSEEK_OFF", SetPlaylistOptionSmoothSeek, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Options/Toggle smooth seek (only in Region Playlist)" }, "S&M_PLAYLIST_OPT_TGL_SMOOTHSEEK", SetPlaylistOptionSmoothSeek, NULL, -1, IsPlaylistOptionSmoothSeek},
	{{ DEFACCEL, "SWS/S&M: Region Playlist - Options/Enable shuffle (only in Region Playlist)" }, "S&M_PLAYLIST_OPT_SHUFFLE_ON", SetPlaylistOptionShuffle, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Options/Disable shuffle (only in Region Playlist)" }, "S&M_PLAYLIST_OPT_SHUFFLE_OFF", SetPlaylistOptionShuffle, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Region Playlist - Options/Toggle shuffle (only in Region Playlist)" }, "S&M_PLAYLIST_OPT_TGL_SHUFFLE", SetPlaylistOptionShuffle, NULL, -1, IsPlaylistOptionShuffle},

	// Markers & regions ------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Insert marker at edit cursor" }, "S&M_INS_MARKER_EDIT", InsertMarker, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Insert marker at play cursor" }, "S&M_INS_MARKER_PLAY", InsertMarker, NULL, 1},

	// Other, misc ------------------------------------------------------------
	{ { DEFACCEL, "SWS/S&M: Send all notes off to selected tracks" }, "S&M_CC123_SEL_TRACKS", SendAllNotesOff, NULL, 1|2},
	{ { DEFACCEL, "SWS/S&M: Send all sounds off to selected tracks" }, "S&M_CC120_SEL_TRACKS", SendAllNotesOff, NULL, 4},
	{ { DEFACCEL, "SWS/S&M: Increase metronome volume" }, "S&M_METRO_VOL_UP", ChangeMetronomeVolume, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Decrease metronome volume" }, "S&M_METRO_VOL_DOWN", ChangeMetronomeVolume, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Show theme helper (all tracks)" }, "S&M_THEME_HELPER_ALL", ShowThemeHelper, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Show theme helper (selected tracks)" }, "S&M_THEME_HELPER_SEL", ShowThemeHelper, NULL, 1},
#ifdef _WIN32
	{ { DEFACCEL, "SWS/S&M: Left mouse click at cursor position (use w/o modifier)" }, "S&M_MOUSE_L_CLICK", SimulateMouseClick, NULL, 0},
#endif
	{ { DEFACCEL, "SWS/S&M: Dump ALR Wiki summary (native actions only)" }, "S&M_ALRSUMMARY1", DumpWikiActionList, NULL, 4},
	{ { DEFACCEL, "SWS/S&M: Dump ALR Wiki summary (SWS actions only)" }, "S&M_ALRSUMMARY2", DumpWikiActionList, NULL, 8},
	{ { DEFACCEL, "SWS/S&M: Dump action list (native actions only)" }, "S&M_DUMP_ACTION_LIST", DumpActionList, NULL, 4},
	{ { DEFACCEL, "SWS/S&M: Dump action list (SWS actions only)" }, "S&M_DUMP_SWS_ACTION_LIST", DumpActionList, NULL, 8},
	{ { DEFACCEL, "SWS/S&M: Dump action list (custom actions only)" }, "S&M_DUMP_CUST_ACTION_LIST", DumpActionList, NULL, 16},
	{ { DEFACCEL, "SWS/S&M: Dump action list (all but custom actions)" }, "S&M_DUMP_NOT_CUST_ACTION_LIST", DumpActionList, NULL, 4|8},
	{ { DEFACCEL, "SWS/S&M: Dump action list (all actions)" }, "S&M_DUMP_ALL_ACTION_LIST", DumpActionList, NULL, 4|8|16},

	{ { DEFACCEL, "SWS/S&M: Resources - Clear FX chain slot, prompt for slot" }, "S&M_CLRFXCHAINSLOT", ResourcesClearSlotPrompt, NULL, SNM_SLOT_FXC},
	{ { DEFACCEL, "SWS/S&M: Resources - Clear track template slot, prompt for slot" }, "S&M_CLR_TRTEMPLATE_SLOT", ResourcesClearSlotPrompt, NULL, SNM_SLOT_TR},
	{ { DEFACCEL, "SWS/S&M: Resources - Clear project template slot, prompt for slot" }, "S&M_CLR_PRJTEMPLATE_SLOT", ResourcesClearSlotPrompt, NULL, SNM_SLOT_PRJ},
	{ { DEFACCEL, "SWS/S&M: Resources - Clear media file slot, prompt for slot" }, "S&M_CLR_MEDIA_SLOT", ResourcesClearSlotPrompt, NULL, SNM_SLOT_MEDIA},
	{ { DEFACCEL, "SWS/S&M: Resources - Clear image slot, prompt for slot" }, "S&M_CLR_IMAGE_SLOT", ResourcesClearSlotPrompt, NULL, SNM_SLOT_IMG},
	{ { DEFACCEL, "SWS/S&M: Resources - Clear theme slot, prompt for slot" }, "S&M_CLR_THEME_SLOT", ResourcesClearSlotPrompt, NULL, SNM_SLOT_THM},

	{ { DEFACCEL, "SWS/S&M: Resources - Paste (replace) FX chain to selected items, prompt for slot" }, "S&M_TAKEFXCHAINp1", LoadSetTakeFXChainSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste (replace) FX chain to selected items, all takes, prompt for slot" }, "S&M_TAKEFXCHAINp2", LoadSetAllTakesFXChainSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste FX chain to selected items, prompt for slot" }, "S&M_PASTE_TAKEFXCHAINp1", LoadPasteTakeFXChainSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste FX chain to selected items, all takes, prompt for slot" }, "S&M_PASTE_TAKEFXCHAINp2", LoadPasteAllTakesFXChainSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste (replace) FX chain to selected tracks, prompt for slot" }, "S&M_TRACKFXCHAINp1", LoadSetTrackFXChainSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste FX chain to selected tracks, prompt for slot" }, "S&M_PASTE_TRACKFXCHAINp1", LoadPasteTrackFXChainSlot, NULL, -1},

	{ { DEFACCEL, "SWS/S&M: Resources - Import tracks from track template, prompt for slot" }, "S&M_ADD_TRTEMPLATEp", LoadImportTrackTemplateSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Apply track template to selected tracks, prompt for slot" }, "S&M_APPLY_TRTEMPLATEp", LoadApplyTrackTemplateSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Apply track template (+envelopes/items) to selected tracks, prompt for slot" }, "S&M_APPLY_TRTEMPLATE_ITEMSENVSp", LoadApplyTrackTemplateSlotWithItemsEnvs, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste (replace) template items to selected tracks, last slot" }, "S&M_REPLACE_TEMPLATE_ITEMSp", ReplaceItemsTrackTemplateSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Paste template items to selected tracks, last slot" }, "S&M_PASTE_TEMPLATE_ITEMSp", PasteItemsTrackTemplateSlot, NULL, -1},

	{ { DEFACCEL, "SWS/S&M: Resources - Open project, prompt for slot" }, "S&M_APPLY_PRJTEMPLATEp", LoadOrSelectProjectSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Open project, prompt for slot (new tab)" }, "S&M_NEWTAB_PRJTEMPLATEp", LoadOrSelectProjectTabSlot, NULL, -1},


  //!WANT_LOCALIZE_1ST_STRING_END


  // Deprecated, unreleased, etc... -----------------------------------------
#ifdef _SNM_MISC
	{ { DEFACCEL, "SWS/S&M: Resources - Play media file in selected tracks, prompt for slot" }, "S&M_PLAYMEDIA_SELTRACKp", PlaySelTrackMediaSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Loop media file in selected tracks, prompt for slot" }, "S&M_LOOPMEDIA_SELTRACKp", LoopSelTrackMediaSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Play media file in selected tracks (toggle), prompt for slot" }, "S&M_TGL_PLAYMEDIA_SELTRACKp", TogglePlaySelTrackMediaSlot, NULL, -1, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Resources - Loop media file in selected tracks (toggle), prompt for slot" }, "S&M_TGL_LOOPMEDIA_SELTRACKp", ToggleLoopSelTrackMediaSlot, NULL, -1, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Resources - Play media file in selected tracks (toggle pause), prompt for slot" }, "S&M_TGLPAUSE_PLAYMEDIA_SELTRACKp", TogglePauseSelTrackMediaSlot, NULL, -1, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Resources - Loop media file in selected tracks (toggle pause), prompt for slot" }, "S&M_TGLPAUSE_LOOPMEDIA_SELTRACKp", ToggleLoopPauseSelTrackMediaSlot, NULL, -1, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Resources - Add media file to current track, prompt for slot" }, "S&M_ADDMEDIA_CURTRACKp", InsertMediaSlotCurTr, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Add media file to new track, prompt for slot" }, "S&M_ADDMEDIA_NEWTRACKp", InsertMediaSlotNewTr, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Add media file to selected items as takes, prompt for slot" }, "S&M_ADDMEDIA_SELITEMp", InsertMediaSlotTakes, NULL, -1},

	{ { DEFACCEL, "SWS/S&M: Resources - Show image, prompt for slot" }, "S&M_SHOW_IMAGEp", ShowImageSlot, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Resources - Set track icon for selected tracks, prompt for slot" }, "S&M_SET_TRACK_ICONp", SetSelTrackIconSlot, NULL, -1},

	{ { DEFACCEL, "SWS/S&M: Resources - Load theme, prompt for slot" }, "S&M_LOAD_THEMEp", LoadThemeSlot, NULL, -1},

	// those actions do not focus the main window on cycle
	{ { DEFACCEL, "SWS/S&M: Focus previous floating FX for selected tracks (cycle)" }, "S&M_WNFOCUS1", CycleFocusFXWndSelTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Focus next floating FX for selected tracks (cycle)" }, "S&M_WNFOCUS2", CycleFocusFXWndSelTracks, NULL, 1},
	{ { DEFACCEL, "SWS/S&M: Focus previous floating FX (cycle)" }, "S&M_WNFOCUS3", CycleFocusFXWndAllTracks, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Focus next floating FX (cycle)" }, "S&M_WNFOCUS4", CycleFocusFXWndAllTracks, NULL, 1},

	// seems confusing, "SWS/S&M: Takes - Remove empty MIDI takes/items among selected items" is fine..
	{ { DEFACCEL, "SWS/S&M: Takes - Remove all empty takes/items among selected items" }, "S&M_DELEMPTYTAKE3", RemoveAllEmptyTakes, NULL, },

	// native "rotate take lanes forward/backward" actions added in REAPER v3.67
	{ { DEFACCEL, "SWS/S&M: Takes - Move all up (cycling) in selected items" }, "S&M_MOVETAKE1", MoveTakes, NULL, -1},
	{ { DEFACCEL, "SWS/S&M: Takes - Move all down (cycling) in selected items" }, "S&M_MOVETAKE2", MoveTakes, NULL, 1},

	// native take alignment since v4
	{ { DEFACCEL, "SWS/S&M: Takes - Build lanes for selected tracks" }, "S&M_LANETAKE1", BuildLanes, NULL, 0},
	{ { DEFACCEL, "SWS/S&M: Takes - Build lanes for selected items" }, "S&M_LANETAKE3", BuildLanes, NULL, 1},

	// contrary to their native versions, the following actions were spliting selected items *and only them*
	// (http://forum.cockos.com/showthread.php?t=51547) => removed because of REAPER v3.67's new native pref 
	// "if no items are selected, some split/trim/delete actions affect all items at the edit cursor", those 
	// actions are less useful: they would still split only selected items, even if that native pref is ticked. 
	{ { DEFACCEL, "SWS/S&M: Split selected items at play cursor" }, "S&M_SPLIT3", SplitSelectedItems, NULL, 40196},
	{ { DEFACCEL, "SWS/S&M: Split selected items at time selection" }, "S&M_SPLIT4", SplitSelectedItems, NULL, 40061},
	{ { DEFACCEL, "SWS/S&M: Split selected items at edit cursor (no change selection)" }, "S&M_SPLIT5", SplitSelectedItems, NULL, 40757},
	{ { DEFACCEL, "SWS/S&M: Split selected items at time selection (select left)" }, "S&M_SPLIT6", SplitSelectedItems, NULL, 40758},
	{ { DEFACCEL, "SWS/S&M: Split selected items at time selection (select right)" }, "S&M_SPLIT7", SplitSelectedItems, NULL, 40759},
	{ { DEFACCEL, "SWS/S&M: Split selected items at edit or play cursor" }, "S&M_SPLIT8", SplitSelectedItems, NULL, 40012},
	{ { DEFACCEL, "SWS/S&M: Split selected items at edit or play cursor (ignoring grouping)" }, "S&M_SPLIT9", SplitSelectedItems, NULL, 40186},

	// exist natively..
	{ { DEFACCEL, "SWS/S&M: Toggle show take volume envelope" }, "S&M_TAKEENV7", BypassUnbypassShowHideTakeVolEnvelope, NULL, -1, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle show take pan envelope" }, "S&M_TAKEENV8", BypassUnbypassShowHideTakePanEnvelope, NULL, -1, GetFakeToggleState},
	{ { DEFACCEL, "SWS/S&M: Toggle show take mute envelope" }, "S&M_TAKEENV9", BypassUnbypassShowHideTakeMuteEnvelope, NULL, -1, GetFakeToggleState},
#endif

	{ {}, LAST_COMMAND, } // denote end of table
};


///////////////////////////////////////////////////////////////////////////////
// S&M dynamic actions: the number of instances of these actions can be 
// customized in the S&M.ini file, section [NbOfActions].
// In the following table:
// - a function doCommand(COMMAND_T*) or getEnabled(COMMAND_T*) will be 
//   triggered with 0-based COMMAND_T.user
// - action names are formatted strings, they *must* contain "%d"
// - custom command ids are not formated strings, but final ids will end with
//   slot numbers (1-based for display reasons)
// Example: 
//   { "Do stuff %d", "DO_STUFF", doStuff, 2, SNM_MAX_DYN_ACTIONS, NULL }
//   if not overrided in the S&M.ini file (e.g. "DO_STUFF=32"), 2 actions will 
//   be created: "Do stuff 1" and "Do stuff 2" both calling doStuff(c) with 
//   c->user=0 and c->user=1, respectively. custom ids will be "_DO_STUFF1"  
//   and "_DO_STUFF2", repectively.
///////////////////////////////////////////////////////////////////////////////

void ExclusiveToggle(COMMAND_T*);

static DYN_COMMAND_T s_dynCmdTable[]
{

//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions

	{ "SWS/S&M: Create cue buss from track selection, settings %d", "S&M_CUEBUS", CueBuss, SNM_MAX_CUE_BUSS_CONFS, SNM_MAX_CUE_BUSS_CONFS, NULL},

	{ "SWS/S&M: Set FX %d online for selected tracks", "S&M_FXOFF_SETON", SetFXOnlineSelTracks, 8, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Set FX %d offline for selected tracks", "S&M_FXOFF_SETOFF", SetFXOfflineSelTracks, 8, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Toggle FX %d online/offline for selected tracks", "S&M_FXOFF", ToggleFXOfflineSelTracks, 8, SNM_MAX_DYN_ACTIONS, IsFXOfflineSelTracks},
	{ "SWS/S&M: Bypass FX %d for selected tracks", "S&M_FXBYP_SETON", BypassFXSelTracks, 8, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Unbypass FX %d for selected tracks", "S&M_FXBYP_SETOFF", UnbypassFXSelTracks, 8, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Toggle FX %d bypass for selected tracks", "S&M_FXBYP", ToggleFXBypassSelTracks, 8, SNM_MAX_DYN_ACTIONS, IsFXBypassedSelTracks},
	{ "SWS/S&M: Toggle all FX (except %d) online/offline for selected tracks", "S&M_FXOFFEXCPT", ToggleExceptFXOfflineSelTracks, 0, SNM_MAX_DYN_ACTIONS, GetFakeToggleState}, // default: none
	{ "SWS/S&M: Toggle all FX (except %d) bypass for selected tracks", "S&M_FXBYPEXCPT", ToggleExceptFXBypassSelTracks, 0, SNM_MAX_DYN_ACTIONS, GetFakeToggleState}, // default: none
	{ "SWS/S&M: Set all FX (except %d) online for selected tracks", "S&M_FXOFF_ALL_ON_EXCPT", SetAllFXsOnlineExceptSelTracks, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Set all FX (except %d) offline for selected tracks", "S&M_FXOFF_ALL_OFF_EXCPT", SetAllFXsOfflineExceptSelTracks, 8, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Bypass all FX (except %d) for selected tracks", "S&M_FXBYP_ALL_ON_EXCPT", BypassAllFXsExceptSelTracks, 8, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Unbypass all FX (except %d) for selected tracks", "S&M_FXBYP_ALL_OFF_EXCPT", UnypassAllFXsExceptSelTracks, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
 
	{ "SWS/S&M: Resources - Clear FX chain slot %d", "S&M_CLRFXCHAINSLOT", ResourcesClearFXChainSlot, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Resources - Paste (replace) FX chain to selected items, slot %d", "S&M_TAKEFXCHAIN", LoadSetTakeFXChainSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Paste FX chain to selected items, slot %d", "S&M_PASTE_TAKEFXCHAIN", LoadPasteTakeFXChainSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Paste (replace) FX chain to selected items, all takes, slot %d", "S&M_FXCHAIN_ALLTAKES", LoadSetAllTakesFXChainSlot, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Resources - Paste FX chain to selected items, all takes, slot %d", "S&M_PASTE_FXCHAIN_ALLTAKES", LoadPasteAllTakesFXChainSlot, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Resources - Paste (replace) FX chain to selected tracks, slot %d", "S&M_TRACKFXCHAIN", LoadSetTrackFXChainSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Paste FX chain to selected tracks, slot %d", "S&M_PASTE_TRACKFXCHAIN", LoadPasteTrackFXChainSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Paste (replace) input FX chain to selected tracks, slot %d", "S&M_INFXCHAIN", LoadSetTrackInFXChainSlot, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Resources - Paste input FX chain to selected tracks, slot %d", "S&M_PASTE_INFXCHAIN", LoadPasteTrackInFXChainSlot, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Resources - Paste (replace) template items to selected tracks, slot %d", "S&M_REPLACE_TEMPLATE_ITEMS", ReplaceItemsTrackTemplateSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Paste template items to selected tracks, slot %d", "S&M_PASTE_TEMPLATE_ITEMS", PasteItemsTrackTemplateSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},

	{ "SWS/S&M: Resources - Clear track template slot %d", "S&M_CLR_TRTEMPLATE_SLOT", ResourcesClearTrTemplateSlot, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Resources - Apply track template to selected tracks, slot %d", "S&M_APPLY_TRTEMPLATE", LoadApplyTrackTemplateSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Apply track template (+envelopes/items) to selected tracks, slot %d", "S&M_APPLY_TRTEMPLATE_ITEMSENVS", LoadApplyTrackTemplateSlotWithItemsEnvs, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Import tracks from track template, slot %d", "S&M_ADD_TRTEMPLATE", LoadImportTrackTemplateSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},

	{ "SWS/S&M: Resources - Clear project slot %d", "S&M_CLR_PRJTEMPLATE_SLOT", ResourcesClearPrjTemplateSlot, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Resources - Open project, slot %d", "S&M_APPLY_PRJTEMPLATE", LoadOrSelectProjectSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Open project, slot %d (new tab)", "S&M_NEWTAB_PRJTEMPLATE", LoadOrSelectProjectTabSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},

	{ "SWS/S&M: Resources - Clear media file slot %d", "S&M_CLR_MEDIA_SLOT", ResourcesClearMediaSlot, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Resources - Play media file in selected tracks, slot %d", "S&M_PLAYMEDIA_SELTRACK", PlaySelTrackMediaSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Loop media file in selected tracks, slot %d", "S&M_LOOPMEDIA_SELTRACK", LoopSelTrackMediaSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Play media file in selected tracks (sync with next measure), slot %d", "S&M_PLAYMEDIA_SELTRACK_SYNC", SyncPlaySelTrackMediaSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Loop media file in selected tracks (sync with next measure), slot %d", "S&M_LOOPMEDIA_SELTRACK_SYNC", SyncLoopSelTrackMediaSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Play media file in selected tracks (toggle), slot %d", "S&M_TGL_PLAYMEDIA_SELTRACK", TogglePlaySelTrackMediaSlot, 4, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},
	{ "SWS/S&M: Resources - Loop media file in selected tracks (toggle), slot %d", "S&M_TGL_LOOPMEDIA_SELTRACK", ToggleLoopSelTrackMediaSlot, 4, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},
	{ "SWS/S&M: Resources - Play media file in selected tracks (toggle pause), slot %d", "S&M_TGLPAUSE_PLAYMEDIA_SELTR", TogglePauseSelTrackMediaSlot, 4, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},
	{ "SWS/S&M: Resources - Loop media file in selected tracks (toggle pause), slot %d", "S&M_TGLPAUSE_LOOPMEDIA_SELTR", ToggleLoopPauseSelTrackMediaSlot, 0, SNM_MAX_DYN_ACTIONS, GetFakeToggleState}, // default: none

	{ "SWS/S&M: Resources - Add media file to current track, slot %d", "S&M_ADDMEDIA_CURTRACK", InsertMediaSlotCurTr, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Add media file to new track, slot %d", "S&M_ADDMEDIA_NEWTRACK", InsertMediaSlotNewTr, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Add media file to selected items as takes, slot %d", "S&M_ADDMEDIA_SELITEM", InsertMediaSlotTakes, 4, SNM_MAX_DYN_ACTIONS, NULL},

	{ "SWS/S&M: Resources - Clear image slot %d", "S&M_CLR_IMAGE_SLOT", ResourcesClearImageSlot, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Resources - Show image, slot %d", "S&M_SHOW_IMG", ShowImageSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Resources - Set track icon for selected tracks, slot %d", "S&M_SET_TRACK_ICON", SetSelTrackIconSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},

	{ "SWS/S&M: Resources - Clear theme slot %d", "S&M_CLR_THEME_SLOT", ResourcesClearThemeSlot, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Resources - Load theme, slot %d", "S&M_LOAD_THEME", LoadThemeSlot, 4, SNM_MAX_DYN_ACTIONS, NULL},

	{ "SWS/S&M: Trigger next preset for FX %d of selected tracks", "S&M_NEXT_PRESET_FX", NextPresetSelTracks, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Trigger previous preset for FX %d of selected tracks", "S&M_PREVIOUS_PRESET_FX", PrevPresetSelTracks, 4, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Trigger preset for FX %d of selected track (MIDI/OSC only)", "S&M_PRESET_FX", NULL, SNM_PRESETS_NB_FX, SNM_PRESETS_NB_FX, NULL, 0, TriggerFXPresetSelTrack},

	{ "SWS/S&M: Select FX %d for selected tracks", "S&M_SELFX", SelectTrackFX, 8, SNM_MAX_DYN_ACTIONS, NULL},

	{ "SWS/S&M: Show FX chain for selected tracks, FX %d", "S&M_SHOWFXCHAIN", ShowFXChain, 8, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Float FX %d for selected tracks", "S&M_FLOATFX", FloatFX, 8, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Unfloat FX %d for selected tracks", "S&M_UNFLOATFX", UnfloatFX, 8, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Toggle float FX %d for selected tracks", "S&M_TOGLFLOATFX", ToggleFloatFX, 8, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},

	// outdated, kept for asc. compatibility: those versions are registered in the main section
	{ "SWS/S&M: Active MIDI Editor - Restore displayed CC lanes, slot %d", "S&M_MESETCCLANES", MainSetCCLanes, 8, SNM_MAX_DYN_ACTIONS, NULL},
	{ "SWS/S&M: Active MIDI Editor - Save displayed CC lanes, slot %d", "S&M_MESAVECCLANES", MainSaveCCLanes, 8, SNM_MAX_DYN_ACTIONS, NULL},
	// those versions are registered in the ME section
	{ "SWS/S&M: Restore displayed CC lanes, slot %d", "S&M_SETCCLANES_ME", NULL, 8, SNM_MAX_DYN_ACTIONS, NULL, 32060, MESetCCLanes, },
	{ "SWS/S&M: Save displayed CC lanes, slot %d", "S&M_SAVECCLANES_ME", NULL, 8, SNM_MAX_DYN_ACTIONS, NULL, 32060, MESaveCCLanes, },

	{ "SWS/S&M: Set selected tracks to group %d (default flags)", "S&M_SET_TRACK_GROUP", SetTrackGroup, 8, SNM_MAX_TRACK_GROUPS, NULL}, // not all the 32 groups by default!

	{ "SWS/S&M: Live Config #%d - Open/close monitoring window", "S&M_OPEN_LIVECFG_MONITOR", OpenLiveConfigMonitorWnd, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, IsLiveConfigMonitorWndDisplayed},
	{ "SWS/S&M: Live Config #%d - Apply config (MIDI/OSC only)", "S&M_LIVECFG_APPLY", NULL, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, NULL, 0, ApplyLiveConfig },
	{ "SWS/S&M: Live Config #%d - Preload config (MIDI/OSC only)", "S&M_LIVECFG_PRELOAD", NULL, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, NULL, 0, PreloadLiveConfig },
	{ "SWS/S&M: Live Config #%d - Apply next config", "S&M_NEXT_LIVE_CFG", ApplyNextLiveConfig, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, NULL},
	{ "SWS/S&M: Live Config #%d - Apply previous config", "S&M_PREVIOUS_LIVE_CFG", ApplyPreviousLiveConfig, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, NULL},
	{ "SWS/S&M: Live Config #%d - Preload next config", "S&M_PRELOAD_NEXT_LIVE_CFG", PreloadNextLiveConfig, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, NULL},
	{ "SWS/S&M: Live Config #%d - Preload previous config", "S&M_PRELOAD_PREVIOUS_LIVE_CFG", PreloadPreviousLiveConfig, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, NULL},
	{ "SWS/S&M: Live Config #%d - Apply preloaded config (swap preload/current)", "S&M_PRELOAD_LIVE_CFG", SwapCurrentPreloadLiveConfigs, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, NULL},

	{ "SWS/S&M: Live Config #%d - Enable", "S&M_LIVECFG_ON", EnableLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Disable", "S&M_LIVECFG_OFF", DisableLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Toggle enable", "S&M_LIVECFG_TGL", ToggleEnableLiveConfig, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, IsLiveConfigEnabled},

	{ "SWS/S&M: Live Config #%d - Enable option 'Mute all but active track'", "S&M_LIVECFG_MUTEBUT_ON", EnableMuteOthersLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Disable option 'Mute all but active track'", "S&M_LIVECFG_MUTEBUT_OFF", DisableMuteOthersLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Toggle option 'Mute all but active track'", "S&M_LIVECFG_MUTEBUT_TGL", ToggleMuteOthersLiveConfig, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, IsMuteOthersLiveConfigEnabled},

	{ "SWS/S&M: Live Config #%d - Enable option 'Offline all but active/preloaded tracks'", "S&M_LIVECFG_OFFLINEBUT_ON", EnableOfflineOthersLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Disable option 'Offline all but active/preloaded tracks'", "S&M_LIVECFG_OFFLINEBUT_OFF", DisableOfflineOthersLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Toggle option 'Offline all but active/preloaded tracks'", "S&M_LIVECFG_OFFLINEBUT_TGL", ToggleOfflineOthersLiveConfig, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, IsOfflineOthersLiveConfigEnabled},

	{ "SWS/S&M: Live Config #%d - Enable option 'Disarm all but active track'", "S&M_LIVECFG_DISARMBUT_ON", EnableDisarmOthersLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Disable option 'Disarm all but active track'", "S&M_LIVECFG_DISARMBUT_OFF", DisableDisarmOthersLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Toggle option 'Disarm all but active track'", "S&M_LIVECFG_DISARMBUT_TGL", ToggleDisarmOthersLiveConfig, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, NULL},

	{ "SWS/S&M: Live Config #%d - Enable option 'Send all notes off when switching configs'", "S&M_LIVECFG_CC123_ON", EnableAllNotesOffLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Disable option 'Send all notes off when switching configs'", "S&M_LIVECFG_CC123_OFF", DisableAllNotesOffLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Toggle option 'Send all notes off when switching configs'", "S&M_LIVECFG_CC123_TGL", ToggleAllNotesOffLiveConfig, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, IsAllNotesOffLiveConfigEnabled},

	{ "SWS/S&M: Live Config #%d - Enable tiny fades", "S&M_LIVECFG_FADES_ON", EnableTinyFadesLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Disable tiny fades", "S&M_LIVECFG_FADES_OFF", DisableTinyFadesLiveConfig, 0, SNM_LIVECFG_NB_CONFIGS, NULL}, // default: none
	{ "SWS/S&M: Live Config #%d - Toggle enable tiny fades", "S&M_LIVECFG_FADES_TGL", ToggleTinyFadesLiveConfig, SNM_LIVECFG_NB_CONFIGS, SNM_LIVECFG_NB_CONFIGS, IsTinyFadesLiveConfigEnabled},

	{ "SWS/S&M: Region Playlist #%d - Play", "S&M_PLAY_RGN_PLAYLIST", PlaylistPlay, 4, SNM_MAX_DYN_ACTIONS, NULL},

	{ "SWS/S&M: Go to marker %d (obeys smooth seek)", "S&M_GOTO_MARKER", GotoMarker, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Go to region %d (obeys smooth seek)", "S&M_GOTO_REGION", GotoRegion, 0, SNM_MAX_DYN_ACTIONS, NULL}, // default: none
	{ "SWS/S&M: Go to/select region %d (obeys smooth seek)", "S&M_GOTO_SEL_REGION", GotoAnsSelectRegion, 4, SNM_MAX_DYN_ACTIONS, NULL},

	{ "SWS/S&M: Dummy toggle %d", "S&M_DUMMY_TGL", Noop, 8, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},
	{ "SWS/S&M: Exclusive toggle A%d", "S&M_EXCL_TGL", ExclusiveToggle, 4, SNM_MAX_DYN_ACTIONS, GetFakeToggleState}, // not "S&M_EXCL_TGL_A" for historical reasons...
	{ "SWS/S&M: Exclusive toggle B%d", "S&M_EXCL_TGL_B", ExclusiveToggle, 4, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},
	{ "SWS/S&M: Exclusive toggle C%d", "S&M_EXCL_TGL_C", ExclusiveToggle, 4, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},
	{ "SWS/S&M: Exclusive toggle D%d", "S&M_EXCL_TGL_D", ExclusiveToggle, 4, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},
	{ "SWS/S&M: Exclusive toggle E%d", "S&M_EXCL_TGL_E", ExclusiveToggle, 0, SNM_MAX_DYN_ACTIONS, GetFakeToggleState}, // default: none
	{ "SWS/S&M: Exclusive toggle F%d", "S&M_EXCL_TGL_F", ExclusiveToggle, 0, SNM_MAX_DYN_ACTIONS, GetFakeToggleState}, // default: none
	{ "SWS/S&M: Exclusive toggle G%d", "S&M_EXCL_TGL_G", ExclusiveToggle, 0, SNM_MAX_DYN_ACTIONS, GetFakeToggleState}, // default: none
	{ "SWS/S&M: Exclusive toggle H%d", "S&M_EXCL_TGL_H", ExclusiveToggle, 0, SNM_MAX_DYN_ACTIONS, GetFakeToggleState}, // default: none

	{ "SWS/S&M: Set selected tracks MIDI input to channel %d", "S&M_MIDI_INPUT_CH", SetMIDIInputChannel, 16, 16, },
	{ "SWS/S&M: Map selected tracks MIDI input to channel %d", "S&M_MAP_MIDI_INPUT_CH", RemapMIDIInputChannel, 16, 16, },

	{ "SWS: Recall snapshot %d", "SWSSNAPSHOT_GET", GetSnapshot, 12, SNM_MAX_DYN_ACTIONS, },
	{ "SWS: Save as snapshot %d", "SWSSNAPSHOT_SAVE", SaveSnapshot, 12, SNM_MAX_DYN_ACTIONS, },
	{ "SWS: Select only track %d", "SWS_SEL", SelectTrack, 32, SNM_MAX_DYN_ACTIONS, },

//!WANT_LOCALIZE_1ST_STRING_END

#ifdef _SNM_MISC
	{ "SWS/S&M: Play media file in selected tracks (toggle, sync with next measure), slot %d", "S&M_TGL_PLAYMEDIA_SELTRACK_SYNC", SyncTogglePlaySelTrackMediaSlot, 4, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},
	{ "SWS/S&M: Loop media file in selected tracks (toggle, sync with next measure), slot %d", "S&M_TGL_LOOPMEDIA_SELTRACK_SYNC", SyncToggleLoopSelTrackMediaSlot, 4, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},
	{ "SWS/S&M: Play media file in selected tracks (toggle pause, sync with next measure), slot %d", "S&M_TGLPAUSE_PLAYMEDIA_SELTR_SYNC", SyncTogglePauseSelTrackMediaSlot, 4, SNM_MAX_DYN_ACTIONS, GetFakeToggleState},
	{ "SWS/S&M: Loop media file in selected tracks (toggle pause, sync with next measure), slot %d", "S&M_TGLPAUSE_LOOPMEDIA_SELTR_SYNC", SyncToggleLoopPauseSelTrackMediaSlot, 0, SNM_MAX_DYN_ACTIONS, GetFakeToggleState}, // default: none
#endif

};


bool RegisterDynamicActions()
{
	for (DYN_COMMAND_T &ct : s_dynCmdTable)
	{
		int n = GetPrivateProfileInt("NbOfActions", ct.id, -1, g_SNM_IniFn.Get());
		if (n < 0) n = ct.count;
		ct.count = BOUNDED(n, 0, ct.max<=0 ? SNM_MAX_DYN_ACTIONS : ct.max);

		for (int j = 0; j < ct.count; ++j)
			ct.Register(j);
	}

	return true;
}

bool DYN_COMMAND_T::Register(const int slot) const
{
	char actionName[SNM_MAX_ACTION_NAME_LEN], custId[SNM_MAX_ACTION_CUSTID_LEN];

	if (snprintfStrict(actionName, sizeof(actionName), GetLocalizedActionName(desc, LOCALIZE_FLAG_VERIFY_FMTS), slot+1) <= 0
			|| snprintfStrict(custId, sizeof(custId), "%s%d", id, slot+1) <= 0)
		return false;

	if (SWSCreateRegisterDynamicCmd(uniqueSectionId, 0, doCommand, onAction, getEnabled, custId, actionName, "", slot, __FILE__, false)) // already localized
	{
#ifdef _SNM_DEBUG
		OutputDebugString("DYN_COMMAND_T::Register() - Registered: ");
		OutputDebugString(actionName);
		OutputDebugString("\n");
#endif
	}

	return true;
}

void SaveDynamicActions()
{
	// no localization here, intentional
	std::ostringstream iniSection;
	iniSection << "; Set the number of actions you want below. Quit REAPER first! ===" << '\0';
	iniSection << "; Unless specified, the maximum number of actions is " << SNM_MAX_DYN_ACTIONS << " (0 will hide actions). ===" << '\0';

	WDL_String nameStr, str; // no fast string here: mangled buffer
	for (const DYN_COMMAND_T &ct : s_dynCmdTable)
	{
		KbdSectionInfo* sec = SectionFromUniqueID(ct.uniqueSectionId);
		if (sec) nameStr.SetFormatted(512, "[%s] ",  __localizeFunc(sec->name,"accel_sec",0));
		else nameStr.Set("");

		nameStr.Append(GetLocalizedActionName(ct.desc) + IsSwsAction(ct.desc));
		ReplaceWithChar(nameStr.Get(), "%d", 'n');
		if (ct.max>0 && ct.max!=SNM_MAX_DYN_ACTIONS) // is a specific max value defined?
			nameStr.AppendFormatted(128, " -- Max. = %d!", ct.max);

		// indent things (a \t solution would suck here)
		str.SetFormatted(256, "%s=%d", ct.id, ct.count);
		while (str.GetLength() < 40) str.Append(" ");
		str.Append(" ; ");
		iniSection << str.Get() << nameStr.Get() << '\0';
	}
	SaveIniSection("NbOfActions", iniSection.str(), g_SNM_IniFn.Get());
}

DYN_COMMAND_T *FindDynamicAction(void (*doCommand)(COMMAND_T*))
{
	for (DYN_COMMAND_T &ct : s_dynCmdTable)
	{
		if (ct.doCommand == doCommand)
			return &ct;
	}

	return nullptr;
}

bool SNM_GetActionName(const char* _custId, WDL_FastString* _nameOut, int _slot)
{
	if (!_nameOut) return false;

	int i=-1;
	if (_slot<0)
	{
		while(s_cmdTable[++i].id != LAST_COMMAND)
		{
			COMMAND_T* ct = &s_cmdTable[i];
			if (ct && !strcmp(ct->id, _custId)) {
				_nameOut->Set(GetLocalizedActionName(ct->accel.desc) + IsSwsAction(ct->accel.desc));
				return true;
			}
		}
	}

	// 2nd try: search as dynamic cmd
	i=-1;
	WDL_String nameStr; // no fast string here: mangled buffer
	for (const DYN_COMMAND_T &ct : s_dynCmdTable)
	{
		if (!strcmp(ct.id, _custId))
		{
			nameStr.Set(GetLocalizedActionName(ct.desc) + IsSwsAction(ct.desc));

			if (_slot>=0 && strstr(nameStr.Get(), "%d"))
			{
				_nameOut->SetFormatted(SNM_MAX_ACTION_NAME_LEN, nameStr.Get(), _slot+1);
				return true;
			}

			// fall back
			ReplaceWithChar(nameStr.Get(), "%d", 'x');
			_nameOut->Set(nameStr.Get());
			return true;
		}
	}
	_nameOut->Set("");
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// Action sections info, keep SNM_SEC_IDX_* enums in sync!
///////////////////////////////////////////////////////////////////////////////

SECTION_INFO_T *SNM_GetActionSectionInfo(int _idx)
{
  static SECTION_INFO_T sectionInfos[] = {
    {0,		"S&M_CYCLACTION_",			"Main_Cyclactions"},
    {100,	"S&M_MAIN_ALT_CYCLACTION",	"MainAlt_Cyclactions"},
    {32063,	"S&M_MEDIAEX_CYCLACTION",	"MediaEx_Cyclactions"},
    {32060,	"S&M_ME_PIANO_CYCLACTION",	"ME_Piano_Cyclactions"},
    {32061,	"S&M_ME_LIST_CYCLACTION",	"ME_List_Cyclactions"},
    {32062,	"S&M_ME_INLINE_CYCLACTION", "ME_Inline_Cyclactions"}
  };  
  return (_idx>=SNM_SEC_IDX_MAIN && _idx<SNM_NUM_MANAGED_SECTIONS ? &sectionInfos[_idx] : NULL);
}


///////////////////////////////////////////////////////////////////////////////
// Fake toggle states
// Those fake states are automatically toggled when actions are performed, 
// see hookCommandProc()
///////////////////////////////////////////////////////////////////////////////

void FakeToggle(COMMAND_T* _ct)
{
	if (_ct) {
		if (strstr(_ct->id, "S&M_EXCL_TGL")) ExclusiveToggle(_ct);
		else _ct->fakeToggle = !_ct->fakeToggle;
	}
}

int GetFakeToggleState(COMMAND_T* _ct) {
	return (_ct && _ct->fakeToggle);
}

void ExclusiveToggle(COMMAND_T* _ct)
{
/*JFB commented: the proper solution should look like this + related funcs
   like ExclusiveToggleA(COMMAND_T*), ExclusiveToggleB, etc but such func 
   pointers are messed-up (inlined) in release builds
   anyway, replaced with code below: faster but it first aims at avoiding
   function pointers

	if (_ct && _ct->fakeToggle)
		for (INT_PTR i=0; i<SNM_MAX_DYN_ACTIONS; i++)
			if (COMMAND_T* ct = SWSGetCommand(_ct->doCommand, i))
			{
				if (ct->accel.accel.cmd != _ct->accel.accel.cmd) {
					ct->fakeToggle = false;
					RefreshToolbar(ct->accel.accel.cmd);
				}
			}
			else
				break;
*/
	//JFB! enough ATM but relies on *ordered* cmds, cmd ids, etc
	int id = _ct->accel.accel.cmd - (int)_ct->user;
	for (int i=0; i<SNM_MAX_DYN_ACTIONS; i++)
	{
		if (COMMAND_T* ct = SWSGetCommandByID(id+i))
		{
			if ((int)ct->user == i) // real break condition
			{
				if (ct->accel.accel.cmd != _ct->accel.accel.cmd)
					ct->fakeToggle = false;
			}
			else
				break;
		}
		else
			break;
	}
	RefreshToolbar(0); // 0 for tied CAs
}


///////////////////////////////////////////////////////////////////////////////
// ScheduledJob
///////////////////////////////////////////////////////////////////////////////

WDL_PtrList_DOD<ScheduledJob> g_jobs;

void ScheduledJob::Schedule(ScheduledJob* _job)
{
	if (!_job)
		return;

	// perform?
	if (_job->IsImmediate())
	{
		_job->PerformSafe();
#ifdef _SNM_DEBUG
		char dbg[256]="";
		snprintf(dbg, sizeof(dbg), "ScheduledJob::Schedule() - Performed job #%d\n", _job->m_id);
		OutputDebugString(dbg);
#endif
		DELETE_NULL(_job);
		return;
	}

	// replace?
	for (int i=0; i<g_jobs.GetSize(); i++)
	{
		if (ScheduledJob* job = g_jobs.Get(i))
		{
			if (job->m_id == _job->m_id)
			{
				_job->InitSafe(job);
				g_jobs.Set(i, _job);
				DELETE_NULL(job);
#ifdef _SNM_DEBUG
				char dbg[256]="";
				snprintf(dbg, sizeof(dbg), "ScheduledJob::Schedule() - Replaced job #%d\n", _job->m_id);
				OutputDebugString(dbg);
#endif
				return;
			}
		}
	}

	// add (exclusive with the above)
	_job->InitSafe();
	g_jobs.Add(_job);

#ifdef _SNM_DEBUG
	char dbg[256]="";
	snprintf(dbg, sizeof(dbg), "ScheduledJob::Schedule() - Added job #%d\n", _job->m_id);
	OutputDebugString(dbg);
#endif
}

// perform (and auto-delete) scheduled jobs, if needed
// polled from the main thread via SNM_CSurfRun()
void ScheduledJob::Run()
{
	for (int i=g_jobs.GetSize()-1; i>=0; i--)
	{
		if (ScheduledJob* job = g_jobs.Get(i))
		{
			if (GetTickCount() > job->m_time)
			{
				g_jobs.Delete(i, false);
				job->PerformSafe();
#ifdef _SNM_DEBUG
				char dbg[256]="";
				snprintf(dbg, sizeof(dbg), "ScheduledJob::Run() - Performed job %d\n", job->m_id);
				OutputDebugString(dbg);
#endif
				DELETE_NULL(job);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// MidiOscActionJob
// Unless g_SNM_LearnPitchAndNormOSC is enabled, MIDI pitch is not supported,
// see http://forum.cockos.com/showthread.php?t=116377
///////////////////////////////////////////////////////////////////////////////

void MidiOscActionJob::Init(ScheduledJob* _replacedJob)
{
	double min=GetMinValue(), max=GetMaxValue();

	// 14-bit resolution (osc & midi pitch ATM)
	if (m_valhw>=0)
	{
		if (g_SNM_LearnPitchAndNormOSC)
			m_absval = min + (max-min) * (BOUNDED(m_valhw|m_val<<7, 0.0, 16383.0)/16383.0);
		else
			m_absval = m_valhw||m_val ? BOUNDED(16384.0-(m_valhw|m_val<<7), 0.0, 16383.0) : 0.0; // see top remark!
	}
	// midi
	else if (m_valhw==-1 && m_val>=0 && m_val<128)
	{
		// absolute mode
		if (!m_relmode) m_absval = m_val;
		// relative modes
		else m_absval = (_replacedJob ? 
			((MidiOscActionJob*)_replacedJob)->m_absval : 
			GetCurrentValue()) + AdjustRelative(m_relmode, m_val);
	}

	// clamp if needed
	m_absval = BOUNDED(m_absval, min, max);

#ifdef _SNM_DEBUG
	char dbg[256]="";
	snprintf(dbg, sizeof(dbg), 
		"MidiOscActionJob::Init() - val: %d, valhw: %d, relmode: %d ===> value: %f\n", 
		m_val, m_valhw, m_relmode, m_absval);
	OutputDebugString(dbg);
#endif
}

// http://forum.cockos.com/project.php?issueid=4576
int MidiOscActionJob::AdjustRelative(int _adjmode, int _reladj)
{
  if (_adjmode==1) { if (_reladj >= 0x40) _reladj|=~0x3f; } // sign extend if 0x40 set
  else if (_adjmode==2) { _reladj-=0x40; } // offset by 0x40
  else if (_adjmode==3) { if (_reladj&0x40) _reladj = -(_reladj&0x3f); } // 0x40 is sign bit
  else _reladj=0;
  return _reladj;
}


///////////////////////////////////////////////////////////////////////////////
// S&M.ini
///////////////////////////////////////////////////////////////////////////////

WDL_FastString g_SNM_IniFn, g_SNM_CyclIniFn, g_SNM_DiffToolFn;
int g_SNM_Beta=0, g_SNM_LearnPitchAndNormOSC=0;
int g_SNM_MediaFlags=0, g_SNM_ToolbarRefreshFreq=SNM_DEF_TOOLBAR_RFRSH_FREQ;
bool g_SNM_ToolbarRefresh = false;


void IniFileInit()
{
	g_SNM_IniFn.SetFormatted(SNM_MAX_PATH, SNM_FORMATED_INI_FILE, GetResourcePath());
	g_SNM_CyclIniFn.SetFormatted(SNM_MAX_PATH, SNM_CYCLACTION_INI_FILE, GetResourcePath());

	int iniVersion = GetPrivateProfileInt("General", "IniFileUpgrade", 0, g_SNM_IniFn.Get());
	SNM_UpgradeIniFiles(iniVersion);

	g_SNM_MediaFlags |= (GetPrivateProfileInt("General", "MediaFileLockAudio", 0, g_SNM_IniFn.Get()) ? 1:0);
	g_SNM_ToolbarRefresh = (GetPrivateProfileInt("General", "ToolbarsAutoRefresh", 1, g_SNM_IniFn.Get()) == 1);
	g_SNM_ToolbarRefreshFreq = BOUNDED(GetPrivateProfileInt("General", "ToolbarsAutoRefreshFreq", SNM_DEF_TOOLBAR_RFRSH_FREQ, g_SNM_IniFn.Get()), 100, 5000);
	g_SNM_SupportBuggyPlug = GetPrivateProfileInt("General", "BuggyPlugsSupport", 0, g_SNM_IniFn.Get());

	// #1175, prompt by default, may be overridden
	if (GetPrivateProfileInt("Misc", "RemoveAllEnvsSelTracksPrompt", -666, g_SNM_IniFn.Get()) == -666)
		WritePrivateProfileString("Misc", "RemoveAllEnvsSelTracksPrompt", "1", g_SNM_IniFn.Get());
		
#ifdef _WIN32
	g_SNM_ClearType = (GetPrivateProfileInt("General", "ClearTypeFont", 1, g_SNM_IniFn.Get()) == 1);

	char buf[SNM_MAX_PATH];
	GetPrivateProfileString("General", "DiffTool", "", buf, sizeof(buf), g_SNM_IniFn.Get());
	g_SNM_DiffToolFn.Set(buf);
#endif
	g_SNM_LearnPitchAndNormOSC = GetPrivateProfileInt("General", "LearnPitchAndNormOSC", 0, g_SNM_IniFn.Get());
	g_SNM_Beta = GetPrivateProfileInt("General", "Beta", 0, g_SNM_IniFn.Get());
}

void IniFileExit()
{
	std::ostringstream iniSection;

	char version[64];
	snprintf(version, sizeof(version), "v%d.%d.%d Build %d", SWS_VERSION);

	// debug info
	// note: the character '=' is needed, SWELL's WritePrivateProfileSection() would drop these comments otherwise
	iniSection
		<< "; REAPER v" << GetAppVersion() << " ===" << '\0'
		<< "; SWS/S&M Extension " << version << " ===" << '\0'
		<< "; " << g_SNM_IniFn.Get() << " ===" << '\0'

	// general prefs
		<< "IniFileUpgrade=" << SNM_INI_FILE_VERSION << '\0'
		<< "MediaFileLockAudio=" << (g_SNM_MediaFlags&1 ? 1 : 0) << '\0'
		<< "ToolbarsAutoRefresh=" << (g_SNM_ToolbarRefresh ? 1 : 0) << '\0'
		<< "ToolbarsAutoRefreshFreq=" << g_SNM_ToolbarRefreshFreq << " ; in ms (min: 100, max: 5000)" << '\0'
		<< "BuggyPlugsSupport=" << (g_SNM_SupportBuggyPlug ? 1 : 0) << '\0'
#ifdef _WIN32
		<< "ClearTypeFont=" << (g_SNM_ClearType ? 1 : 0) << '\0'
		<< "DiffTool=\"" << g_SNM_DiffToolFn.Get() << '"' << '\0'
#endif
		<< "LearnPitchAndNormOSC=" << g_SNM_LearnPitchAndNormOSC << '\0'
		<< "Beta=" << g_SNM_Beta << '\0'
	;

	SaveIniSection("General", iniSection.str(), g_SNM_IniFn.Get());

	// save dynamic actions, if needed
	SaveDynamicActions();

#ifdef _WIN32
	// force ini file's cache flush, see http://support.microsoft.com/kb/68827
	WritePrivateProfileString(NULL, NULL, NULL, g_SNM_IniFn.Get());
#endif
}


///////////////////////////////////////////////////////////////////////////////
// S&M core stuff
///////////////////////////////////////////////////////////////////////////////

#ifdef _SNM_MISC
static void SNM_Menuhook(const char* _menustr, HMENU _hMenu, int _flag) {
	if (!strcmp(_menustr, "Main extensions") && !_flag) SWSCreateMenuFromCommandTable(s_cmdTable, _hMenu);
	//else if (!strcmp(_menustr, "Media item context") && !_flag) {}
	//else if (!strcmp(_menustr, "Track control panel context") && !_flag) {}
	//else if (_flag == 1) {}
}
#endif

void OnInitTimer()
{
	plugin_register("-timer",(void*)OnInitTimer); // unregister timer (single call)
	CAsInit();
	GlobalStartupActionTimer();
}

int SNM_Init(reaper_plugin_info_t* _rec)
{
	if (!_rec)
		return 0;

	IniFileInit();

#ifdef _SNM_MISC
	if (!plugin_register("hookcustommenu", (void*)SNM_Menuhook))
		return 0;
#endif

	// actions must be registered before views and cycle actions
	if (!SWSRegisterCommands(s_cmdTable) || !RegisterDynamicActions())
	{
		return 0;
	}

	SNM_UIInit();
	CueBussInit();
	LiveConfigInit();
	ResourcesInit();
	NotesInit();
	FindInit();
	ImageInit();
	RegionPlaylistInit();
	SNM_ProjectInit();
	CyclactionInit();

	// callback when REAPER is fully initialized 
	plugin_register("timer",(void*)OnInitTimer);

	return 1;
}

void SNM_Exit()
{
	LiveConfigExit();
	ResourcesExit();
	NotesExit();
	FindExit();
	ImageExit();
	RegionPlaylistExit();
	SNM_ProjectExit();
	CyclactionExit();
	SNM_UIExit();
	IniFileExit();
#ifdef _SNM_MISC
	plugin_register("-hookcustommenu", (void*)SNM_Menuhook);
#endif
	plugin_register("-timer",(void*)OnInitTimer);
}
