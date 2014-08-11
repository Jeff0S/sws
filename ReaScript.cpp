/******************************************************************************
/ ReaScript.cpp
/
/ Copyright (c) 2012 Jeffos
/ https://code.google.com/p/sws-extension
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
#include "SnM/SnM_FX.h"
#include "SnM/SnM_Marker.h"
#include "SnM/SnM_Misc.h"
#include "SnM/SnM_Resources.h"
#include "SnM/SnM_Routing.h"
#include "SnM/SnM_Track.h"
#include "Fingers/RprMidiTake.h"
#include "Breeder/BR_ReaScript.h"


// Important:
// keep APIFUNC() and the 6 first fields of the struct "APIdef" as they are defined:
// the script reascript_python.pl needs those to parse the exported functions and generate
// python wrappers (sws_python.py) at compile-time.
// "Yes, that is a Perl script reading C++ to generate Python." - schwa
// See http://code.google.com/p/sws-extension/issues/detail?id=432


#define APIFUNC(x) (void*)x,#x,"API_" #x "","APIdef_" #x ""

typedef struct APIdef
{
	void* func;
	const char* func_name;
	const char* regkey_func;
	const char* regkey_def;
	const char* ret_val;
	const char* parm_types;

	// if additionnal data are needed, add them below (see top remark)
	const char* parm_names;
	const char* help;
	char* dyn_def; // used for dynamic allocations/cleanups

} APIdef;


///////////////////////////////////////////////////////////////////////////////
// Add the functions you want to export here (+ related #include on top)
// Make sure function names have a prefix like "SWS_", "FNG_", etc..
///////////////////////////////////////////////////////////////////////////////

APIdef g_apidefs[] =
{
	{ APIFUNC(SNM_CreateFastString), "WDL_FastString*", "const char*", "str", "[S&M] Instantiates a new \"fast string\". You must delete this string, see SNM_DeleteFastString.", },
	{ APIFUNC(SNM_DeleteFastString), "void", "WDL_FastString*", "str", "[S&M] Deletes a \"fast string\" instance.", },
	{ APIFUNC(SNM_GetFastString), "const char*", "WDL_FastString*", "str", "[S&M] Gets the \"fast string\" content.", },
	{ APIFUNC(SNM_GetFastStringLength), "int", "WDL_FastString*", "str", "[S&M] Gets the \"fast string\" length.", },
	{ APIFUNC(SNM_SetFastString), "WDL_FastString*", "WDL_FastString*,const char*", "str,newstr", "[S&M] Sets the \"fast string\" content. Returns str for facility.", },
	{ APIFUNC(SNM_GetMediaItemTakeByGUID), "MediaItem_Take*", "ReaProject*,const char*", "project,guid", "[S&M] Gets a take by GUID as string. The GUID must be enclosed in braces {}.", },
	{ APIFUNC(SNM_GetSourceType), "bool","MediaItem_Take*,WDL_FastString*", "take,type", "[S&M] Gets the source type of a take. Returns false if failed (e.g. take with empty source, etc..)", },
	{ APIFUNC(SNM_GetSetSourceState), "bool", "MediaItem*,int,WDL_FastString*,bool", "item,takeidx,state,setnewvalue", "[S&M] Gets or sets a take source state. Returns false if failed. Use takeidx=-1 to get/alter the active take.\nNote: this function does not use a MediaItem_Take* param in order to manage empty takes (i.e. takes with MediaItem_Take*==NULL), see SNM_GetSetSourceState2.", },
	{ APIFUNC(SNM_GetSetSourceState2), "bool", "MediaItem_Take*,WDL_FastString*,bool", "take,state,setnewvalue", "[S&M] Gets or sets a take source state. Returns false if failed.\nNote: this function cannot deal with empty takes, see SNM_GetSetSourceState.", },
	{ APIFUNC(SNM_GetSetObjectState), "bool", "void*,WDL_FastString*,bool,bool", "obj,state,setnewvalue,wantminimalstate", "[S&M] Gets or sets the state of a track, an item or an envelope. The state chunk size is unlimited. Returns false if failed.\nWhen getting a track state (and when you are not interested in FX data), you can use wantminimalstate=true to radically reduce the length of the state. Do not set such minimal states back though, this is for read-only applications!\nNote: unlike the native GetSetObjectState, calling to FreeHeapPtr() is not required.", },
	{ APIFUNC(SNM_AddReceive), "bool", "MediaTrack*,MediaTrack*,int", "src,dest,type", "[S&M] Adds a receive. Returns false if nothing updated.\ntype -1=Default type (user preferences), 0=Post-Fader (Post-Pan), 1=Pre-FX, 2=deprecated, 3=Pre-Fader (Post-FX).\nNote: obeys default sends preferences, supports frozen tracks, etc..", },
	{ APIFUNC(SNM_RemoveReceive), "bool", "MediaTrack*,int", "tr,rcvidx", "[S&M] Removes a receive. Returns false if nothing updated.", },
	{ APIFUNC(SNM_RemoveReceivesFrom), "bool", "MediaTrack*,MediaTrack*", "tr,srctr", "[S&M] Removes all receives from srctr. Returns false if nothing updated.", },
	{ APIFUNC(SNM_GetIntConfigVar), "int", "const char*,int", "varname,errvalue", "[S&M] Returns an integer preference (look in project prefs first, then in general prefs). Returns errvalue if failed (e.g. varname not found).", },
	{ APIFUNC(SNM_SetIntConfigVar), "bool", "const char*,int", "varname,newvalue", "[S&M] Sets an integer preference (look in project prefs first, then in general prefs). Returns false if failed (e.g. varname not found).", },
	{ APIFUNC(SNM_GetDoubleConfigVar), "double", "const char*,double", "varname,errvalue", "[S&M] Returns a double preference (look in project prefs first, then in general prefs). Returns errvalue if failed (e.g. varname not found).", },
	{ APIFUNC(SNM_SetDoubleConfigVar), "bool", "const char*,double", "varname,newvalue", "[S&M] Sets a double preference (look in project prefs first, then in general prefs). Returns false if failed (e.g. varname not found).", },
	{ APIFUNC(SNM_MoveOrRemoveTrackFX), "bool", "MediaTrack*,int,int", "tr,fxId,what", "[S&M] Move or removes a track FX. Returns true if tr has been updated.\nfxId: fx index in chain or -1 for the selected fx. what: 0 to remove, -1 to move fx up in chain, 1 to move fx down in chain.", },
	{ APIFUNC(SNM_GetProjectMarkerName), "bool", "ReaProject*,int,bool,WDL_FastString*", "proj,num,isrgn,name", "[S&M] Gets a marker/region name. Returns true if marker/region found.", },
	{ APIFUNC(SNM_SetProjectMarker), "bool", "ReaProject*,int,bool,double,double,const char*,int", "proj,num,isrgn,pos,rgnend,name,color", "[S&M] See SetProjectMarker3, it is the same function but this one can set empty names, i.e. \"\".", },
	{ APIFUNC(SNM_SelectResourceBookmark), "int", "const char*", "name", "[S&M] Select a bookmark of the Resources window. Returns the related bookmark id (or -1 if failed).", },
	{ APIFUNC(SNM_TieResourceSlotActions), "void", "int", "bookmarkId", "[S&M] Attach Resources slot actions to a given bookmark.", },
	{ APIFUNC(SNM_AddTCPFXParm), "bool", "MediaTrack*,int,int", "tr,fxId,prmId", "[S&M] Add an FX parameter knob in the TCP. Returns false if nothing updated (invalid parameters, knob already present, etc..)", },

	{ APIFUNC(FNG_AllocMidiTake), "RprMidiTake*", "MediaItem_Take*", "take", "[FNG] Allocate a RprMidiTake from a take pointer. Returns a NULL pointer if the take is not an in-project MIDI take", },
	{ APIFUNC(FNG_FreeMidiTake), "void", "RprMidiTake*", "midiTake", "[FNG] Commit changes to MIDI take and free allocated memory", },
	{ APIFUNC(FNG_CountMidiNotes), "int", "RprMidiTake*", "midiTake", "[FNG] Count of how many MIDI notes are in the MIDI take", },
	{ APIFUNC(FNG_GetMidiNote), "RprMidiNote*", "RprMidiTake*,int", "midiTake,index", "[FNG] Get a MIDI note from a MIDI take at specified index", },
	{ APIFUNC(FNG_GetMidiNoteIntProperty), "int", "RprMidiNote*,const char*", "midiNote,property", "[FNG] Get MIDI note property", },
	{ APIFUNC(FNG_SetMidiNoteIntProperty), "void", "RprMidiNote*,const char*,int", "midiNote,property,value", "[FNG] Set MIDI note property", },
	{ APIFUNC(FNG_AddMidiNote), "RprMidiNote*", "RprMidiTake*", "midiTake", "[FNG] Add MIDI note to MIDI take", },

	{ APIFUNC(BR_EnvAlloc), "BR_Envelope*", "TrackEnvelope*,bool", "envelope,takeEnvelopesUseProjectTime", "Allocate envelope object from track or take envelope pointer. Always call <a href=\"#BR_EnvFree\">BR_EnvFree</a> when done to release the object and commit changes if needed.\n takeEnvelopesUseProjectTime: take envelope points' positions are counted from take position, not project start time. If you want to work with project time instead, pass this as true.\n\nFor further manipulation see BR_EnvCountPoints, BR_EnvDeletePoint, BR_EnvFind, BR_EnvFindNext, BR_EnvFindPrevious, BR_EnvGetParentTake, BR_EnvGetParentTrack, BR_EnvGetPoint, BR_EnvGetProperties, BR_EnvSetPoint, BR_EnvSetProperties, BR_EnvValueAtPos.", },
	{ APIFUNC(BR_EnvCountPoints), "int", "BR_Envelope*", "envelope", "Count envelope points in the envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>.", },
	{ APIFUNC(BR_EnvDeletePoint), "bool", "BR_Envelope*,int", "envelope,id", "Delete envelope point by index (zero-based) in the envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>. Returns true on success.", },
	{ APIFUNC(BR_EnvFind), "int", "BR_Envelope*,double,double", "envelope,position,delta", "Find envelope point at time position in the envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>. Pass delta > 0 to search surrounding range - in that case the closest point to position within delta will be searched for. Returns envelope point id (zero-based) on success or -1 on failure.", },
	{ APIFUNC(BR_EnvFindNext), "int", "BR_Envelope*,double", "envelope,position", "Find next envelope point after time position in the envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>. Returns envelope point id (zero-based) on success or -1 on failure.", },
	{ APIFUNC(BR_EnvFindPrevious), "int", "BR_Envelope*,double", "envelope,position", "Find previous envelope point before time position in the envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>. Returns envelope point id (zero-based) on success or -1 on failure.", },
	{ APIFUNC(BR_EnvFree), "bool", "BR_Envelope*,bool", "envelope,commit", "Free envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a> and commit changes if needed. Returns true if changes were committed successfully. Note that when envelope object wasn't modified nothing will get committed even if commit = true - in that case function returns false.", },
	{ APIFUNC(BR_EnvGetParentTake), "MediaItem_Take*", "BR_Envelope*", "envelope", "If envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a> is take envelope, returns parent media item take, otherwise NULL.", },
	{ APIFUNC(BR_EnvGetParentTrack), "MediaItem*", "BR_Envelope*", "envelope", "Get parent track of envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>. If take envelope, returns NULL.", },
	{ APIFUNC(BR_EnvGetPoint), "bool", "BR_Envelope*,int,double*,double*,int*,bool*,double*", "envelope,id,position,value,shape,selected,bezier", "Get envelope point by id (zero-based) from the envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>. Returns true on success.", },
	{ APIFUNC(BR_EnvGetProperties), "void", "BR_Envelope*,bool*,bool*,bool*,bool*,int*,int*,double*,double*,double*,int*", "envelope,active,visible,armed,inLane,laneHeight,defaultShape,minValue,maxValue,centerValue,type", "Get envelope properties for the envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>.\n\nactive: true if envelope is active\nvisible: true if envelope is visible\narmed: true if envelope is armed\ninLane: true if envelope has it's own envelope lane\nlaneHeight: envelope lane override height. 0 for none, otherwise size in pixels\ndefaultShape: default point shape: 0->Linear, 1->Square, 2->Slow start/end, 3->Fast start, 4->Fast end, 5->Bezier\nminValue: minimum envelope value\nmaxValue: maximum envelope value\ntype: envelope type: 0->Volume, 1->Volume (Pre-FX), 2->Pan, 3->Pan (Pre-FX), 4->Width, 5->Width (Pre-FX), 6->Mute, 7->Pitch, 8->Playrate, 9->Tempo map, 10->Parameter", },
	{ APIFUNC(BR_EnvSetPoint), "bool", "BR_Envelope*,int,double,double,int,bool,double", "envelope,id,position,value,shape,selected,bezier", "Set envelope point by id (zero-based) in the envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>. Returns true on sucess.", },
	{ APIFUNC(BR_EnvSetProperties), "void", "BR_Envelope*,bool,bool,bool,bool,int,int", "envelope,active,visible,armed,inLane,laneHeight,defaultShape", "Set envelope properties for the envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>. For parameter description see BR_EnvGetProperties.", },
	{ APIFUNC(BR_EnvValueAtPos), "double", "BR_Envelope*,double", "envelope,position", "Get envelope value at time position for the envelope object allocated with <a href=\"#BR_EnvAlloc\">BR_EnvAlloc</a>.", },
	{ APIFUNC(BR_GetMediaSourceProperties), "bool", "MediaItem_Take*,bool*,double*,double*,double*,bool*", "take,section,start,length,fade,reverse", "Get take media source properties as they appear in <i>Item properties</i>. Returns false if take can't have them (MIDI items etc.).\nTo set source properties, see BR_SetMediaSourceProperties." },
	{ APIFUNC(BR_GetMouseCursorContext), "void", "char*,char*,char*,int", "window,segment,details,char_sz", BR_MOUSE_REASCRIPT_DESC, },
	{ APIFUNC(BR_GetMouseCursorContext_Envelope), "TrackEnvelope*", "bool*", "takeEnvelope", "[BR] Returns envelope that was captured with the last call to <a href=\"#BR_GetMouseCursorContext\">BR_GetMouseCursorContext</a>. In case the envelope belongs to take, takeEnvelope will be true.", },
	{ APIFUNC(BR_GetMouseCursorContext_Item), "MediaItem*", "", "", "[BR] Returns item under mouse cursor that was captured with the last call to <a href=\"#BR_GetMouseCursorContext\">BR_GetMouseCursorContext</a>."},
	{ APIFUNC(BR_GetMouseCursorContext_MIDI), "void*", "bool*,int*,int*,int*,int*", "inlineEditor,noteRow,ccLane,ccLaneVal,ccLaneId", "[BR] Returns midi editor under mouse cursor that was captured with the last call to <a href=\"#BR_GetMouseCursorContext\">BR_GetMouseCursorContext</a>.\n\ninlineEditor: if mouse was captured in inline MIDI editor, this will be true (consequentially, returned MIDI editor will be NULL)\nnoteRow: note row or piano key under mouse cursor (0-127)\nccLane: CC lane under mouse cursor (CC0-127=CC, 0x100|(0-31)=14-bit CC, 0x200=velocity, 0x201=pitch, 0x202=program, 0x203=channel pressure, 0x204=bank/program select, 0x205=text, 0x206=sysex)\nccLaneVal: value in CC lane under mouse cursor (0-127 or 0-16383)\nccLaneId: lane position, counting from the top (0 based)\n\nNote: due to API limitations, if mouse is over inline MIDI editor with some note rows hidden, noteRow will be -1"},
	{ APIFUNC(BR_GetMouseCursorContext_Position), "double", "", "", "[BR] Returns project time position in arrange/ruler/midi editor that was captured with the last call to <a href=\"#BR_GetMouseCursorContext\">BR_GetMouseCursorContext</a>."},
	{ APIFUNC(BR_GetMouseCursorContext_Take), "MediaItem_Take*", "", "", "[BR] Returns take under mouse cursor that was captured with the last call to <a href=\"#BR_GetMouseCursorContext\">BR_GetMouseCursorContext</a>."},
	{ APIFUNC(BR_GetMouseCursorContext_Track), "MediaTrack*", "", "", "[BR] Returns track under mouse cursor that was captured with the last call to <a href=\"#BR_GetMouseCursorContext\">BR_GetMouseCursorContext</a>."},
	{ APIFUNC(BR_ItemAtMouseCursor), "MediaItem*", "double*", "position", "[BR] Get media item under mouse cursor. Position is mouse cursor position in arrange.", },
	{ APIFUNC(BR_MIDI_CCLaneRemove), "bool", "void*,int", "midiEditor,laneId", "[BR] Remove CC lane in midi editor. Returns true on success", },
	{ APIFUNC(BR_MIDI_CCLaneReplace), "bool", "void*,int,int", "midiEditor,laneId,newCC", "[BR] Replace CC lane in midi editor. Returns true on success.\nValid CC lanes: CC0-127=CC, 0x100|(0-31)=14-bit CC, 0x200=velocity, 0x201=pitch, 0x202=program, 0x203=channel pressure, 0x204=bank/program select, 0x205=text, 0x206=sysex", },
	{ APIFUNC(BR_PositionAtMouseCursor), "double", "bool", "checkRuler", "[BR] Get position at mouse cursor. To check ruler along with arrange, pass checkRuler=true. Returns -1 if cursor is not over arrange/ruler.", },
	{ APIFUNC(BR_SetMediaSourceProperties), "bool", "MediaItem_Take*,bool,double,double,double,bool", "take,section,start,length,fade,reverse", "Set take media source properties. Returns false if take can't have them (MIDI items etc.). Section parameters have to be valid only when passing section=true.\nTo get source properties, see BR_GetMediaSourceProperties." },
	{ APIFUNC(BR_SetTakeSourceFromFile), "bool", "MediaItem_Take*,const char*,bool", "take,filename,inProjectData", "[BR] Set new take source from file. To import MIDI file as in-project source data pass inProjectData=true. Returns false if failed.\nAny take source properties from the previous source will be lost - to preserve them, see BR_SetTakeSourceFromFile2.\nNote: To set source from existing take, see <a href=\"#SNM_GetSetSourceState2\">SNM_GetSetSourceState2</a>.", },
	{ APIFUNC(BR_SetTakeSourceFromFile2), "bool", "MediaItem_Take*,const char*,bool,bool", "take,filename,inProjectData,keepSourceProperties", "[BR] Differs from <a href=\"#BR_SetTakeSourceFromFile\">BR_SetTakeSourceFromFile</a> only by preserving existing take media source properties.", },
	{ APIFUNC(BR_TakeAtMouseCursor), "MediaItem_Take*", "double*", "position", "[BR] Get take under mouse cursor. Position is mouse cursor position in arrange.", },
	{ APIFUNC(BR_TrackAtMouseCursor), "MediaTrack*", "int*,double*", "context,position", "[BR] Get track under mouse cursor.\nContext signifies where the track was found: 0 = TCP, 1 = MCP, 2 = Arrange.\nPosition will hold mouse cursor position in arrange if applicable.", },

	{ APIFUNC(ULT_GetMediaItemNote), "const char*", "MediaItem*", "item", "[ULT] Get item notes.", },
	{ APIFUNC(ULT_SetMediaItemNote), "void", "MediaItem*,char*", "item,note", "[ULT] Set item notes.", },

	{ NULL, } // denote end of table
};

///////////////////////////////////////////////////////////////////////////////

bool RegisterExportedFuncs(reaper_plugin_info_t* _rec)
{
	bool ok = (_rec!=NULL);
	int i=-1;
	while (ok && g_apidefs[++i].func)
		ok &= (_rec->Register(g_apidefs[i].regkey_func, g_apidefs[i].func) != 0);
	return ok;
}

bool RegisterExportedAPI(reaper_plugin_info_t* _rec)
{
	bool ok = (_rec!=NULL);
	int i=-1;
	char tmp[8*1024];
	while (ok && g_apidefs[++i].func)
	{
		memset(tmp, 0, sizeof(tmp));
		_snprintf(tmp, sizeof(tmp), "%s\r%s\r%s\r%s", g_apidefs[i].ret_val, g_apidefs[i].parm_types, g_apidefs[i].parm_names, g_apidefs[i].help);
		char* p = g_apidefs[i].dyn_def = _strdup(tmp);
		while (*p) { if (*p=='\r') *p='\0'; p++; }
		ok &= (_rec->Register(g_apidefs[i].regkey_def, g_apidefs[i].dyn_def) != 0);
	}
	return ok;
}