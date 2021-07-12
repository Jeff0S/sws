/******************************************************************************
/ Adam.cpp
/
/ Copyright (c) 2010 Adam Wathan
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

#include "Adam.h"
#include "Context.h"
#include "TrackSel.h"
#include "ProjPrefs.h"
#include "../Xenakios/XenakiosExts.h"
#include "../SnM/SnM_Dlg.h"
#include "../Breeder/BR_Util.h"

#include <WDL/localize/localize.h>

// Globals for copy and paste

namespace ItemClickMoveCurs { enum {
  MoveOnTimeChange = 4, MoveOnPaste = 8,
  ClearLoopOnClick = 32, ClearTimeOnClick = 64,
}; }

///////////////////////////////////////////////////////////////////////////////
// Fills gaps aka Beat Detective
// for Adam to get started...
///////////////////////////////////////////////////////////////////////////////

void ReadFillGapsIniFile(char* cmdString,
						 char* triggerPad=NULL, char* fadeLength=NULL, char* maxGap=NULL,
						 char* maxStretch=NULL, char* presTrans=NULL, char* transFade=NULL,
						 int* fadeShape=NULL, int* markErrors=NULL, int* stretch=NULL, int* trans=NULL)
{
	char tmp[128] = "";
	WDL_FastString cmd;
	int s,t;

	GetPrivateProfileString("SWS","FillGapsTriggerPad","5",tmp,128,get_ini_file());
	cmd.AppendFormatted(128, "%s,", tmp);
	if (triggerPad)
		strncpy(triggerPad, tmp, 128);

	GetPrivateProfileString("SWS","FillGapsFadeLen","5",tmp,128,get_ini_file());
	cmd.AppendFormatted(128, "%s,", tmp);
	if (fadeLength)
		strncpy(fadeLength, tmp, 128);

	GetPrivateProfileString("SWS","FillGapsMaxGap","15",tmp,128,get_ini_file());
	cmd.AppendFormatted(128, "%s,", tmp);
	if (maxGap)
		strncpy(maxGap, tmp, 128);

	GetPrivateProfileString("SWS","FillGapsStretch","1",tmp,128,get_ini_file());
	s = atoi(tmp);
	if (stretch)
		*stretch = s;

	GetPrivateProfileString("SWS","FillGapsMaxStretch","0.5",tmp,128,get_ini_file());
	cmd.AppendFormatted(128, "%s,", !s ? "1.0" : tmp);
	if (maxStretch)
		strncpy(maxStretch, tmp, 128);

	GetPrivateProfileString("SWS","FillGapsTrans","1",tmp,128,get_ini_file());
	t = atoi(tmp);
	if (trans)
		*trans = t;

	GetPrivateProfileString("SWS","FillGapsPresTrans","35",tmp,128,get_ini_file());
	cmd.AppendFormatted(128, "%s,", (!s || !t) ? "0" : tmp);
	if (presTrans)
		strncpy(presTrans, tmp, 128);

	GetPrivateProfileString("SWS","FillGapsTransFade","5",tmp,128,get_ini_file());
	cmd.AppendFormatted(128, "%s,", tmp);
	if (transFade)
		strncpy(transFade, tmp, 128);

	GetPrivateProfileString("SWS","FillGapsFadeShape","0",tmp,128,get_ini_file());
	cmd.AppendFormatted(128, "%s,", tmp);
	if (fadeShape)
		*fadeShape = atoi(tmp);

	GetPrivateProfileString("SWS","FillGapsMarkErr","1",tmp,128,get_ini_file());
	cmd.AppendFormatted(128, "%s", tmp);
	if (markErrors)
		*markErrors = atoi(tmp);

	if (cmdString)
		strncpy(cmdString, cmd.Get(), 128);
}

void SaveFillGapsIniFile(char* triggerPad, char* fadeLength, char* maxGap,
						 char* maxStretch, char* presTrans, char* transFade,
						 int fadeShape, int markErrors, int stretch, int trans)
{
	char tmp[128] = "";
	WritePrivateProfileString("SWS","FillGapsTriggerPad",triggerPad,get_ini_file());
	WritePrivateProfileString("SWS","FillGapsFadeLen",fadeLength,get_ini_file());
	WritePrivateProfileString("SWS","FillGapsMaxGap",maxGap,get_ini_file());
	sprintf(tmp,"%d",stretch);
	WritePrivateProfileString("SWS","FillGapsStretch",tmp,get_ini_file());
	WritePrivateProfileString("SWS","FillGapsMaxStretch",maxStretch,get_ini_file());
	sprintf(tmp,"%d",trans);
	WritePrivateProfileString("SWS","FillGapsTrans",tmp,get_ini_file());
	WritePrivateProfileString("SWS","FillGapsPresTrans",presTrans,get_ini_file());
	WritePrivateProfileString("SWS","FillGapsTransFade",transFade,get_ini_file());
	sprintf(tmp,"%d",fadeShape);
	WritePrivateProfileString("SWS","FillGapsFadeShape",tmp,get_ini_file());
	sprintf(tmp,"%d",markErrors);
	WritePrivateProfileString("SWS","FillGapsMarkErr",tmp,get_ini_file());
}

HWND g_strtchHFader = 0;
double g_strtchHFaderValue = 0.5f;

WDL_DLGRET AWFillGapsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (INT_PTR r = SNM_HookThemeColorsMessage(hwnd, uMsg, wParam, lParam))
		return r;

	const char cWndPosKey[] = "Fill gaps Window Pos";
	switch(uMsg)
	{
		case WM_INITDIALOG :
		{
			WDL_UTF8_HookComboBox(GetDlgItem(hwnd, IDC_FADE_SHAPE));

			char triggerPad[128], fadeLength[128], maxGap[128], maxStretch[128], presTrans[128], transFade[128];
			int fadeShape, markErrors, stretch, trans;
			ReadFillGapsIniFile(NULL, triggerPad, fadeLength, maxGap, maxStretch, presTrans, transFade, &fadeShape, &markErrors, &stretch, &trans);

			SetDlgItemText(hwnd,IDC_TRIG_PAD,triggerPad);
			SetDlgItemText(hwnd,IDC_XFADE_LEN1,fadeLength);
			SetDlgItemText(hwnd,IDC_MAX_GAP,maxGap);
			SetDlgItemText(hwnd,IDC_TRANS_LEN,presTrans);
			SetDlgItemText(hwnd,IDC_XFADE_LEN2,transFade);
			CheckDlgButton(hwnd, IDC_CHECK1, !!markErrors);
			CheckDlgButton(hwnd, IDC_CHECK2, !!stretch);
			CheckDlgButton(hwnd, IDC_CHECK3, !!trans);

#ifdef _WIN32
			RECT r;
			GetWindowRect(GetDlgItem(hwnd, IDC_SLIDER1), &r);
			ScreenToClient(hwnd, (LPPOINT)&r);
			ScreenToClient(hwnd, ((LPPOINT)&r)+1);
			g_strtchHFader = CreateWindowEx(WS_EX_LEFT, "REAPERhfader", "DLGFADER1",
				WS_CHILD | WS_VISIBLE | TBS_VERT,
				r.left, r.top, r.right-r.left, r.bottom-r.top, hwnd, NULL, g_hInst, NULL);
#else
			g_strtchHFader = GetDlgItem(hwnd, IDC_SLIDER1);
			ShowWindow(g_strtchHFader, SW_SHOW);
#endif
			if (g_strtchHFader) {
				SendMessage(g_strtchHFader,TBM_SETTIC,0,500);
				SendMessage(g_strtchHFader,TBM_SETPOS,1,(LPARAM)(atof(maxStretch)*1000));
			}

			int x = (int)SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Equal Gain","sws_DLG_156"));
			SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_SETITEMDATA,x,0);
			x = (int)SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Equal Power","sws_DLG_156"));
			SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_SETITEMDATA,x,1);

			//experimental starts here
			x = (int)SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Reverse Equal Power","sws_DLG_156"));
			SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_SETITEMDATA,x,2);
			x = (int)SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Steep Curve","sws_DLG_156"));
			SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_SETITEMDATA,x,3);
			x = (int)SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("Reverse Steep Curve","sws_DLG_156"));
			SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_SETITEMDATA,x,4);
			x = (int)SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_ADDSTRING,0,(LPARAM)__LOCALIZE("S-Curve","sws_DLG_156"));
			SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_SETITEMDATA,x,5);
			//experimental ends here

			SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_SETCURSEL,fadeShape,0);

			RestoreWindowPos(hwnd, cWndPosKey, false);
			SetFocus(GetDlgItem(hwnd, IDC_TRIG_PAD));
			if (g_strtchHFader)
				PostMessage(hwnd, WM_HSCROLL, 0, (LPARAM)g_strtchHFader); // indirectly refreshes stretch fader %
			PostMessage(hwnd, WM_COMMAND, IDC_CHECK2, 0); // indirectly refreshes grayed states
			PostMessage(hwnd, WM_COMMAND, IDC_CHECK3, 0);
			return 0;
		}
		break;
		case WM_HSCROLL:
		{
			int pos=(int)SendMessage((HWND)lParam,TBM_GETPOS,0,0);
			if (g_strtchHFader && (HWND)lParam==g_strtchHFader) {
				char txt[128];
				sprintf(txt,"%d%%", (int)floor(pos/10 + 0.5));
				SetDlgItemText(hwnd, IDC_STRTCH_LIMIT, txt);
				g_strtchHFaderValue = ((double)pos)/1000;
			}
		}
		break;
		case WM_COMMAND :
		{
			switch(LOWORD(wParam))
			{
				case IDOK:
				case IDC_SAVE:
				{
					char triggerPad[128], fadeLength[128], maxGap[128], maxStretch[314], presTrans[128], transFade[128];
					int fadeShape, markErrors, stretch, trans;
					GetDlgItemText(hwnd,IDC_TRIG_PAD,triggerPad,sizeof(triggerPad));
					GetDlgItemText(hwnd,IDC_XFADE_LEN1,fadeLength,sizeof(fadeLength));
					GetDlgItemText(hwnd,IDC_MAX_GAP,maxGap,sizeof(maxGap));
					GetDlgItemText(hwnd,IDC_TRANS_LEN,presTrans,sizeof(presTrans));
					GetDlgItemText(hwnd,IDC_XFADE_LEN2,transFade,sizeof(transFade));
					if (g_strtchHFader)
						snprintf(maxStretch, sizeof(maxStretch), "%.2f", g_strtchHFaderValue);
					markErrors = IsDlgButtonChecked(hwnd, IDC_CHECK1);
					stretch = IsDlgButtonChecked(hwnd, IDC_CHECK2);
					trans = IsDlgButtonChecked(hwnd, IDC_CHECK3);
					fadeShape = (int)SendDlgItemMessage(hwnd,IDC_FADE_SHAPE,CB_GETCURSEL,0,0);

					SaveFillGapsIniFile(triggerPad, fadeLength, maxGap, maxStretch, presTrans, transFade, fadeShape, markErrors, stretch, trans);

					if(wParam == IDOK)
					{
						char prms[(128 * 6) + 128];
						snprintf(prms, sizeof(prms), "%s,%s,%s,%s,%s,%s,%d,%d",
							triggerPad, fadeLength, maxGap, !stretch ? "1.0" : maxStretch, (!stretch || !trans) ? "0" : presTrans,
							transFade, fadeShape, markErrors);
						AWFillGapsAdv(__LOCALIZE("Fill gaps between selected items","sws_DLG_156"), prms);
					}
					return 0;
				}
				break;
				case IDCANCEL:
				{
					ShowWindow(hwnd, SW_HIDE);
					return 0;
				}
				break;
				case IDC_CHECK2:
				{
					bool strtchEnable = (IsDlgButtonChecked(hwnd, IDC_CHECK2) == 1);
					bool transEnable = (IsDlgButtonChecked(hwnd, IDC_CHECK3) == 1);
					if (g_strtchHFader)
						EnableWindow(g_strtchHFader, strtchEnable);
					EnableWindow(GetDlgItem(hwnd, IDC_CHECK3), strtchEnable);
					EnableWindow(GetDlgItem(hwnd, IDC_TRANS_LEN), strtchEnable && transEnable);
					EnableWindow(GetDlgItem(hwnd, IDC_XFADE_LEN2), strtchEnable && transEnable);
				}
				break;
				case IDC_CHECK3:
				{
					bool transEnable = (IsDlgButtonChecked(hwnd, IDC_CHECK3) == 1);
					EnableWindow(GetDlgItem(hwnd, IDC_TRANS_LEN), transEnable);
					EnableWindow(GetDlgItem(hwnd, IDC_XFADE_LEN2), transEnable);
				}
				break;
			}
		}
		break;
		case WM_DESTROY:
			SaveWindowPos(hwnd, cWndPosKey);
			break;
	}

	return 0;
}

void AWFillGapsAdv(COMMAND_T* t)
{
	if ((int)t->user)
	{
		char prms[128];
		ReadFillGapsIniFile(prms);
		AWFillGapsAdv(SWS_CMD_SHORTNAME(t), prms);
	}
	else
	{
		static HWND hwnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_AW_ITEM_SMOOTHING), g_hwndParent, AWFillGapsProc);
		ShowWindow(hwnd, SW_SHOW);
		SetFocus(hwnd);
	}
}

int AWCountItemGroups()
{
	int maxGroupId = 0;

	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);
		for (int j = 0; j < GetTrackNumMediaItems(tr); j++)
		{
			MediaItem* item = GetTrackMediaItem(tr, j);
			int id = *(int*)GetSetMediaItemInfo(item, "I_GROUPID", NULL);
			if (id > maxGroupId)
				maxGroupId = id;
		}
	}

	return maxGroupId;
}

void AWFillGapsAdv(const char* title, char* retVals)
{
	// Divided by 1000 to convert to milliseconds except maxStretch
	double triggerPad = (atof(strtok(retVals, ",")))/1000;
	double fadeLength = (atof(strtok(NULL, ",")))/1000;
	double maxGap = (atof(strtok(NULL, ",")))/1000;
	double maxStretch = atof(strtok(NULL, ","));
	double presTrans = (atof(strtok(NULL, ",")))/1000;
	double transFade = (atof(strtok(NULL, ",")))/1000;
	int fadeShape = atoi(strtok(NULL, ","));
	int markErrors = atoi(strtok(NULL, ","));

	if ((triggerPad < 0) || (fadeLength < 0) || (maxGap < 0) || (maxStretch < 0) || (maxStretch > 1) || (presTrans < 0) || (transFade < 0) || (fadeShape < 0) || (fadeShape > 5))
	{
		//ShowMessageBox("Don't use such stupid values, try again.","Invalid Input",0);
		MessageBox(g_hwndParent, __LOCALIZE("All values must be non-negative","sws_DLG_156"), __LOCALIZE("SWS - Error","sws_mbox"), MB_OK);
		return;
	}


	int maxGroupID = 0;

	if (presTrans)
		 maxGroupID = AWCountItemGroups();


	// Run loop for every track in project
	for (int trackIndex = 0; trackIndex < GetNumTracks(); trackIndex++)
	{

		// Gets current track in loop
		MediaTrack* track = GetTrack(0, trackIndex);

		// Run loop for every item on track

		int itemCount = GetTrackNumMediaItems(track);

		for (int itemIndex = 0; itemIndex < (itemCount - 1); itemIndex++)
		{

			MediaItem* item1 = GetTrackMediaItem(track, itemIndex);
			MediaItem* item2 = GetTrackMediaItem(track, itemIndex + 1);
			// errorFlag = 0;


			// BEGIN FIX OVERLAP CODE --------------------------------------------------------------

			// If first item is selected selected, run this loop
			if (GetMediaItemInfo_Value(item1, "B_UISEL"))
			{
				// Get various pieces of info for the 2 items
				double item1Start = GetMediaItemInfo_Value(item1, "D_POSITION");
				double item1Length = GetMediaItemInfo_Value(item1, "D_LENGTH");
				double item1End = item1Start + item1Length;

				double item2Start = GetMediaItemInfo_Value(item2, "D_POSITION");


				// If the first item overlaps the second item
				if (item1End > item2Start)
				{
					item1Length = item2Start - item1Start;
				}

				// If the second item is also selected, trim the trigger pad off of item 1
				if (GetMediaItemInfo_Value(item2, "B_UISEL"))
				{
					item1Length -= triggerPad;
				}

				SetMediaItemInfo_Value(item1, "D_LENGTH", item1Length);
			}

			// END FIX OVERLAP CODE ----------------------------------------------------------------


			// BEGIN TIMESTRETCH CODE---------------------------------------------------------------

			// If stretching is enabled
			if (maxStretch < 1)
			{

				// Only stretch if both items are selected, avoids stretching unselected items or the last selected item
				if (GetMediaItemInfo_Value(item1, "B_UISEL") && GetMediaItemInfo_Value(item2, "B_UISEL"))
				{

					// Get various pieces of info for the 2 items
					double item1Start = GetMediaItemInfo_Value(item1, "D_POSITION");
					double item1Length = GetMediaItemInfo_Value(item1, "D_LENGTH");
					double item1End = item1Start + item1Length;

					double item2Start = GetMediaItemInfo_Value(item2, "D_POSITION");

					// Calculate gap between items
					double gap = item2Start - item1End;

					// If gap is bigger than the max allowable gap
					if (gap > maxGap)
					{

						// If preserve transient is enabled, split item at preserve point
						if (presTrans)
						{

							// Get snap offset for item1
							double item1SnapOffset = GetMediaItemInfo_Value(item1, "D_SNAPOFFSET");

							// Calculate position of item1 transient
							double item1TransPos = item1Start + item1SnapOffset;

							// Split point is (presTrans) after the transient point
							// Subtract fade length because easier to change item end then item start
							// (don't need take loop to adjust all start offsets)
							double splitPoint = item1TransPos + presTrans - transFade;

							MediaItem *item1B;

							{
								// Check for default item fades
								ConfigVarOverride<int> fadeState("splitautoxfade", 12);

								// Split item1 at the split point
								item1B = SplitMediaItem(item1, splitPoint);

								int item1Group = (int)GetMediaItemInfo_Value(item1, "I_GROUPID");

								if (item1Group)
									SetMediaItemInfo_Value(item1B, "I_GROUPID", item1Group + maxGroupID);
							}

							// Get new item1 length after split
							item1Length = GetMediaItemInfo_Value(item1, "D_LENGTH") + transFade;

							// Create overlap of 'transFade'
							SetMediaItemInfo_Value(item1, "D_LENGTH", item1Length);

							// Crossfade the overlap
							SetMediaItemInfo_Value(item1, "D_FADEOUTLEN_AUTO", transFade);
							SetMediaItemInfo_Value(item1B, "D_FADEINLEN_AUTO", transFade);

							// Set Fade Shapes
							SetMediaItemInfo_Value(item1, "C_FADEOUTSHAPE", fadeShape);
							SetMediaItemInfo_Value(item1B, "C_FADEINSHAPE", fadeShape);

							// Set the stretched half to be item 1 so loop continues properly
							item1 = item1B;

							// Get item1 info since item1 is now item1B
							item1Start = GetMediaItemInfo_Value(item1, "D_POSITION");
							item1Length = GetMediaItemInfo_Value(item1, "D_LENGTH");
							item1End = item1Start + item1Length;

							// Increase itemIndex and itemCount since split added item
							itemIndex += 1;
							itemCount += 1;
						}

						// Calculate distance between item1Start and the beginning of the maximum allowable gap
						double item1StartToGap = (item2Start - maxGap) - item1Start;

						// Calculate amount to stretch
						double stretchPercent = (item1Length/(item1StartToGap));

						if (stretchPercent < maxStretch)
						{
							stretchPercent = maxStretch;

						}

						item1Length *= (1/stretchPercent);

						SetMediaItemInfo_Value(item1, "D_LENGTH", item1Length);

						for (int takeIndex = 0; takeIndex < GetMediaItemNumTakes(item1); takeIndex++)
						{
							MediaItem_Take* currentTake = GetMediaItemTake(item1, takeIndex);
							SetMediaItemTakeInfo_Value(currentTake, "D_PLAYRATE", stretchPercent);
						}
					}
				}
			}

			// END TIME STRETCH CODE ---------------------------------------------------------------


			// BEGIN FILL GAPS CODE ----------------------------------------------------------------

			// If both items selected, run this loop
			if (GetMediaItemInfo_Value(item1, "B_UISEL") && GetMediaItemInfo_Value(item2, "B_UISEL"))
			{

				double item1Start = GetMediaItemInfo_Value(item1, "D_POSITION");
				double item1Length = GetMediaItemInfo_Value(item1, "D_LENGTH");
				double item1End = item1Start + item1Length;

				double item2Start = GetMediaItemInfo_Value(item2, "D_POSITION");
				double item2Length = GetMediaItemInfo_Value(item2, "D_LENGTH");
				double item2SnapOffset = GetMediaItemInfo_Value(item2, "D_SNAPOFFSET");



				// If there is a gap, fill it and also add an overlap to the left that is "fadeLength" long
				if (item2Start >= item1End)
				{
					double item2StartDiff = item2Start - item1End + fadeLength;
					item2Length += item2StartDiff;

					// Calculate gap between items
					double gap = item2Start - item1End;

					// Check to see if gap is bigger than maxGap, even after time stretching
					// Make sure to account for triggerPad since it is subtracted before this point and shouldn't count towards the gap
					if (gap > (maxGap + triggerPad))
					{
						// If gap is big and mark errors is enabled, add a marker
						if (markErrors == 1)
						{
							AddProjectMarker(NULL, false, item1End, 0, __LOCALIZE("Possible Artifact","sws_DLG_156"), 0);
						}

					}


					// Adjust start offset for all takes in item2 to account for fill
					for (int takeIndex = 0; takeIndex < GetMediaItemNumTakes(item2); takeIndex++)
					{
						MediaItem_Take* currentTake = GetMediaItemTake(item2, takeIndex);
						double startOffset = GetMediaItemTakeInfo_Value(currentTake, "D_STARTOFFS");
						startOffset -= item2StartDiff;
						SetMediaItemTakeInfo_Value(currentTake, "D_STARTOFFS", startOffset);
					}

					// Finally trim the item to fill the gap and adjust the snap offset
					SetMediaItemInfo_Value(item2, "D_POSITION", (item1End - fadeLength));
					SetMediaItemInfo_Value(item2, "D_LENGTH", item2Length);
					SetMediaItemInfo_Value(item2, "D_SNAPOFFSET", (item2SnapOffset + item2StartDiff));

					// Crossfade the overlap between the two items
					SetMediaItemInfo_Value(item1, "D_FADEOUTLEN_AUTO", fadeLength);
					SetMediaItemInfo_Value(item2, "D_FADEINLEN_AUTO", fadeLength);

					// Set Fade Shapes
					SetMediaItemInfo_Value(item1, "C_FADEOUTSHAPE", fadeShape);
					SetMediaItemInfo_Value(item2, "C_FADEINSHAPE", fadeShape);
				}
			}

			// END FILL GAPS CODE ------------------------------------------------------------------

		}

	}


	UpdateTimeline();
	Undo_OnStateChangeEx(title, UNDO_STATE_ITEMS | UNDO_STATE_MISCCFG, -1);
}


void AWFillGapsQuick(COMMAND_T* t)
{


	// Run loop for every track in project
	for (int trackIndex = 0; trackIndex < GetNumTracks(); trackIndex++)
	{

		// Gets current track in loop
		MediaTrack* track = GetTrack(0, trackIndex);

		// Run loop for every item on track

		int itemCount = GetTrackNumMediaItems(track);

		for (int itemIndex = 0; itemIndex < (itemCount - 1); itemIndex++)
		{

			MediaItem* item1 = GetTrackMediaItem(track, itemIndex);
			MediaItem* item2 = GetTrackMediaItem(track, itemIndex + 1);


			// BEGIN FIX OVERLAP CODE --------------------------------------------------------------

			// If first item is selected, run this loop
			if (GetMediaItemInfo_Value(item1, "B_UISEL"))
			{
				// Get various pieces of info for the 2 items
				double item1Start = GetMediaItemInfo_Value(item1, "D_POSITION");
				double item1Length = GetMediaItemInfo_Value(item1, "D_LENGTH");
				double item1End = item1Start + item1Length;

				double item2Start = GetMediaItemInfo_Value(item2, "D_POSITION");

				// If the first item overlaps the second item, trim the first item
				if (item1End > (item2Start))
				{
					item1Length = item2Start - item1Start;

					SetMediaItemInfo_Value(item1, "D_LENGTH", item1Length);
				}

			}

			// END FIX OVERLAP CODE ----------------------------------------------------------------

			// BEGIN FILL GAPS CODE ----------------------------------------------------------------

			// If both items selected, run this loop
			if (GetMediaItemInfo_Value(item1, "B_UISEL") && GetMediaItemInfo_Value(item2, "B_UISEL"))
			{

				double item1Start = GetMediaItemInfo_Value(item1, "D_POSITION");
				double item1Length = GetMediaItemInfo_Value(item1, "D_LENGTH");
				double item1End = item1Start + item1Length;

				double item2Start = GetMediaItemInfo_Value(item2, "D_POSITION");
				double item2Length = GetMediaItemInfo_Value(item2, "D_LENGTH");
				double item2SnapOffset = GetMediaItemInfo_Value(item2, "D_SNAPOFFSET");

				// If there is a gap, fill it and also add an overlap to the left that is "fadeLength" long
				if (item2Start >= item1End)
				{
					double item2StartDiff = item2Start - item1End;
					item2Length += item2StartDiff;

					// Adjust start offset for all takes in item2 to account for fill
					for (int takeIndex = 0; takeIndex < GetMediaItemNumTakes(item2); takeIndex++)
						{
							MediaItem_Take* currentTake = GetMediaItemTake(item2, takeIndex);
							double startOffset = GetMediaItemTakeInfo_Value(currentTake, "D_STARTOFFS");
							startOffset -= item2StartDiff;
							SetMediaItemTakeInfo_Value(currentTake, "D_STARTOFFS", startOffset);
						}

					// Finally trim the item to fill the gap and adjust the snap offset
					SetMediaItemInfo_Value(item2, "D_POSITION", (item1End));
					SetMediaItemInfo_Value(item2, "D_LENGTH", item2Length);
					SetMediaItemInfo_Value(item2, "D_SNAPOFFSET", (item2SnapOffset + item2StartDiff));

				}
			}

			// END FILL GAPS CODE ------------------------------------------------------------------

		}

	}



	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);
}

void AWFillGapsQuickXFade(COMMAND_T* t)
{
	double fadeLength = fabs(*ConfigVar<double>("deffadelen"));

	// Run loop for every track in project
	for (int trackIndex = 0; trackIndex < GetNumTracks(); trackIndex++)
	{

		// Gets current track in loop
		MediaTrack* track = GetTrack(0, trackIndex);

		// Run loop for every item on track

		int itemCount = GetTrackNumMediaItems(track);

		for (int itemIndex = 0; itemIndex < (itemCount - 1); itemIndex++)
		{

			MediaItem* item1 = GetTrackMediaItem(track, itemIndex);
			MediaItem* item2 = GetTrackMediaItem(track, itemIndex + 1);


			// BEGIN FIX OVERLAP CODE --------------------------------------------------------------

			// If first item is selected, run this loop
			if (GetMediaItemInfo_Value(item1, "B_UISEL"))
			{
				// Get various pieces of info for the 2 items
				double item1Start = GetMediaItemInfo_Value(item1, "D_POSITION");
				double item1Length = GetMediaItemInfo_Value(item1, "D_LENGTH");
				double item1End = item1Start + item1Length;

				double item2Start = GetMediaItemInfo_Value(item2, "D_POSITION");

				// If the first item overlaps the second item, trim the first item
				if (item1End > (item2Start))
				{

					item1Length = item2Start - item1Start;

					SetMediaItemInfo_Value(item1, "D_LENGTH", item1Length);
				}
			}

			// END FIX OVERLAP CODE ----------------------------------------------------------------

			// BEGIN FILL GAPS CODE ----------------------------------------------------------------

			// If both items selected, run this loop
			if (GetMediaItemInfo_Value(item1, "B_UISEL") && GetMediaItemInfo_Value(item2, "B_UISEL"))
			{

				double item1Start = GetMediaItemInfo_Value(item1, "D_POSITION");
				double item1Length = GetMediaItemInfo_Value(item1, "D_LENGTH");
				double item1End = item1Start + item1Length;

				double item2Start = GetMediaItemInfo_Value(item2, "D_POSITION");
				double item2Length = GetMediaItemInfo_Value(item2, "D_LENGTH");
				double item2SnapOffset = GetMediaItemInfo_Value(item2, "D_SNAPOFFSET");

				// If there is a gap, fill it and also add an overlap to the left that is "fadeLength" long
				if (item2Start >= item1End)
				{
					double item2StartDiff = item2Start - item1End + fadeLength;
					item2Length += item2StartDiff;

					// Adjust start offset for all takes in item2 to account for fill
					for (int takeIndex = 0; takeIndex < GetMediaItemNumTakes(item2); takeIndex++)
					{
						MediaItem_Take* currentTake = GetMediaItemTake(item2, takeIndex);
						double startOffset = GetMediaItemTakeInfo_Value(currentTake, "D_STARTOFFS");

						// NF fix: also take Playrate into account
						double dPlayrate = GetMediaItemTakeInfo_Value(currentTake, "D_PLAYRATE");
						startOffset -= (item2StartDiff * dPlayrate);
						SetMediaItemTakeInfo_Value(currentTake, "D_STARTOFFS", startOffset);

						// NF: fix / workaround for setting take start offset doesn't work if containing stretch markers
						UpdateStretchMarkersAfterSetTakeStartOffset(currentTake, item2StartDiff * dPlayrate);

					}

					// Finally trim the item to fill the gap and adjust the snap offset
					SetMediaItemInfo_Value(item2, "D_POSITION", (item1End - fadeLength));
					SetMediaItemInfo_Value(item2, "D_LENGTH", item2Length);
					SetMediaItemInfo_Value(item2, "D_SNAPOFFSET", (item2SnapOffset + item2StartDiff));

					// Crossfade the overlap between the two items
					SetMediaItemInfo_Value(item1, "D_FADEOUTLEN_AUTO", fadeLength);
					SetMediaItemInfo_Value(item2, "D_FADEINLEN_AUTO", fadeLength);

				}
			}

			// END FILL GAPS CODE ------------------------------------------------------------------

		}

	}



	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);
}


void AWFixOverlaps(COMMAND_T* t)
{

	// Run loop for every track in project
	for (int trackIndex = 0; trackIndex < GetNumTracks(); trackIndex++)
	{

		// Gets current track in loop
		MediaTrack* track = GetTrack(0, trackIndex);

		// Run loop for every item on track

		int itemCount = GetTrackNumMediaItems(track);

		for (int itemIndex = 0; itemIndex < (itemCount - 1); itemIndex++)
		{

			MediaItem* item1 = GetTrackMediaItem(track, itemIndex);
			MediaItem* item2 = GetTrackMediaItem(track, itemIndex + 1);


			// BEGIN FIX OVERLAP CODE --------------------------------------------------------------

			// If first item is selected, run this loop
			if (GetMediaItemInfo_Value(item1, "B_UISEL"))
			{
				// Get various pieces of info for the 2 items
				double item1Start = GetMediaItemInfo_Value(item1, "D_POSITION");
				double item1Length = GetMediaItemInfo_Value(item1, "D_LENGTH");
				double item1End = item1Start + item1Length;

				double item2Start = GetMediaItemInfo_Value(item2, "D_POSITION");

				// If the first item overlaps the second item, trim the first item
				if (item1End > (item2Start))
				{

					item1Length = item2Start - item1Start;

					SetMediaItemInfo_Value(item1, "D_LENGTH", item1Length);
				}

			}

			// END FIX OVERLAP CODE ----------------------------------------------------------------
		}

	}



	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);
}









void AWRecordConditional(COMMAND_T* t)
{
	double t1, t2;
	GetSet_LoopTimeRange(false, false, &t1, &t2, false);

	if (*ConfigVar<int>("projrecmode") != 0) // 0 is item punch mode
	{
		if (t1 != t2)
		{
			Main_OnCommand(40076, 0); //Set record mode to time selection auto punch
		}



		else
		{
			Main_OnCommand(40252, 0); //Set record mode to normal
		}
	}

	Main_OnCommand(1013,0); // Transport: Record

	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);
}



void AWRecordConditional2(COMMAND_T* t)
{
	double t1, t2;
	GetSet_LoopTimeRange(false, false, &t1, &t2, false);


	if (t1 != t2)
	{
		Main_OnCommand(40076, 0); //Set record mode to time selection auto punch
	}

	else if (CountSelectedMediaItems(0) != 0)
	{
		Main_OnCommand(40253, 0); //Set record mode to auto punch selected items
	}

	else
	{
		Main_OnCommand(40252, 0); //Set record mode to normal
	}


	Main_OnCommand(1013,0); // Transport: Record

	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);
}



//Auto Group

bool g_AWAutoGroup = false;
bool g_AWAutoGroupRndColor = false;


void AWToggleAutoGroup(COMMAND_T* = NULL)
{
	g_AWAutoGroup = !g_AWAutoGroup;
	char str[32];
	sprintf(str, "%d", g_AWAutoGroup);
	WritePrivateProfileString(SWS_INI, "AWAutoGroup", str, get_ini_file());
}

int AWIsAutoGroupEnabled(COMMAND_T* = NULL)
{
	return g_AWAutoGroup;
}

// option to set auto grouped items to random color
void AWToggleAutoGroupRndColor(COMMAND_T* = NULL)
{
	g_AWAutoGroupRndColor = !g_AWAutoGroupRndColor;
	char str[32];
	sprintf(str, "%d", g_AWAutoGroupRndColor);
	WritePrivateProfileString(SWS_INI, "AWAutoGroupRndColor", str, get_ini_file());
}

int AWIsAutoGroupRndColorEnabled(COMMAND_T* = NULL)
{
	return g_AWAutoGroupRndColor;
}

bool g_AWIsRecording = false; // For auto group, remember if it was just recording

/* autoxfade
&4 &8
0  0, Splits existing items and created new takes (default)
1  0, Trims existing items behind new recording (tape mode)
0  1, Creates new media items in separate lanes (layers)
*/
void AWDoAutoGroup(bool rec)
{
	//If transport changes to recording, set recording flag
	if (rec)
		g_AWIsRecording = true;

	// If a "not playing anymore" message is sent and we were just recording
	if (!rec && g_AWIsRecording)
	{
		if (g_AWAutoGroup)
		{
			WDL_TypedBuf<MediaItem*> selItems;
			SWS_GetSelectedMediaItems(&selItems);
			if (selItems.GetSize() > 1 && ((*ConfigVar<int>("autoxfade") & 4) || (*ConfigVar<int>("autoxfade") & 8))) // (Don't group if not in tape or overlap record mode, takes mode is messy) NF: added new auto group in takes mode functionality below
			{
				// check if we're in 'Autopunch selected items' mode
				// because in this mode sel. items don't get automatically unselected after recording,
				// so 'simple' grouping would screw things up
				if ((*ConfigVar<int>("projrecmode")) == 0) {
					NFDoAutoGroupTakesMode(selItems);
					g_AWIsRecording = false;
					return;
				}

				PreventUIRefresh(1);

				//Find highest current group
				int maxGroupId = AWCountItemGroups();

				//Add one to assign new items to a new group
				maxGroupId++;

				int rndColor;
				if (g_AWAutoGroupRndColor)
					// use WDL Mersenne Twister
					rndColor = ColorToNative(g_MTRand.randInt(255), g_MTRand.randInt(255), g_MTRand.randInt(255)) | 0x1000000;

				for (int i = 0; i < selItems.GetSize(); i++) {
					MediaItem* item = selItems.Get()[i];
					MediaTrack* parentTrack = GetMediaItem_Track(item);

					if (*(int*)GetSetMediaTrackInfo(parentTrack, "I_RECARM", NULL)) { // only group items on rec. armed tracks
						GetSetMediaItemInfo(item, "I_GROUPID", &maxGroupId);

						// assign random colors if "Toggle assign random colors..." is enabled
						if (g_AWAutoGroupRndColor)
							SetMediaItemInfo_Value(item, "I_CUSTOMCOLOR", rndColor);
					}
				}

				PreventUIRefresh(-1);
				UpdateArrange();
			}

			// #587 Auto group in takes mode
			else if (selItems.GetSize() > 1 && !(*ConfigVar<int>("autoxfade") & 4) && !(*ConfigVar<int>("autoxfade") & 8))
			{
				NFDoAutoGroupTakesMode(selItems);
			}

			// No longer recording so reset flag to false
			g_AWIsRecording = false;
		}
	}
}

// replication of X-Raym's "Group selected items according to their order in selection per track.lua"
void NFDoAutoGroupTakesMode(WDL_TypedBuf<MediaItem*> origSelItems)
{
	// only group if more than one track is rec. armed
	// otherwise it would group items to itself on same track
	if (!IsMoreThanOneTrackRecArmed())
		return;

	WDL_TypedBuf<MediaTrack*> origSelTracks;
	SWS_GetSelectedTracks(&origSelTracks);

	PreventUIRefresh(1);

	// unselect cur. sel. tracks
	for (int i = 0; i < origSelTracks.GetSize(); i++) {
		SetMediaTrackInfo_Value(origSelTracks.Get()[i], "I_SELECTED", 0);
	}

	SelectRecArmedTracksOfSelItems(origSelItems);

	WDL_TypedBuf<MediaTrack*> selTracksOfSelItems;
	SWS_GetSelectedTracks(&selTracksOfSelItems);

	// get max. of items selected on a track
	vector<int> selItemsPerTrackCount;
	selItemsPerTrackCount.resize(selTracksOfSelItems.GetSize());

	// loop through sel. tracks
	for (int i = 0; i < selTracksOfSelItems.GetSize(); i++) {
		MediaTrack* track = selTracksOfSelItems.Get()[i];
		WDL_TypedBuf<MediaItem*> selItemsOnTrack;
		SWS_GetSelectedMediaItemsOnTrack(&selItemsOnTrack, track);

		int selItemsOnCurTrackCount = selItemsOnTrack.GetSize();

		selItemsPerTrackCount[i] = selItemsOnCurTrackCount;
	}

	int maxSelItemsOnTrack = GetMaxSelItemsPerTrackCount(selItemsPerTrackCount);

	int rndColor;
	if (g_AWAutoGroupRndColor)
		rndColor = ColorToNative(g_MTRand.randInt(255), g_MTRand.randInt(255), g_MTRand.randInt(255)) | 0x1000000;

	// do column-wise grouping
	// loop through columns of items
	for (int column = 0; column < maxSelItemsOnTrack; column++)
	{
		int maxGroupId = AWCountItemGroups();
		maxGroupId++;

		// loop through sel. tracks
		for (int selTrackIdx = 0; selTrackIdx < selTracksOfSelItems.GetSize(); selTrackIdx++) {

			// get item of cur. track and cur. column
			MediaItem* item = GetSelectedItemOnTrack_byIndex(origSelItems, selItemsPerTrackCount, selTrackIdx, column);

			if (item) {
				MediaTrack* parentTrack = GetMediaItem_Track(item);
				if (GetMediaTrackInfo_Value(parentTrack, "I_RECARM")) { // only group items on rec. armed tracks
					GetSetMediaItemInfo(item, "I_GROUPID", &maxGroupId);

					if (g_AWAutoGroupRndColor) {
						MediaItem_Take* take = GetActiveTake(item);
						SetMediaItemTakeInfo_Value(take, "I_CUSTOMCOLOR", rndColor);
					}
				}
			}
		}
	}

	UnselectAllTracks(); UnselectAllItems();
	RestoreOrigTracksAndItemsSelection(origSelTracks, origSelItems);
	PreventUIRefresh(-1);
	UpdateArrange();
}

// Auto group helper functions
bool IsMoreThanOneTrackRecArmed()
{
	int recArmedTracks = 0;
	for (int i = 1; i <= GetNumTracks(); ++i) { // skip master
		MediaTrack* CurTrack = CSurf_TrackFromID(i, false);
		if (*(int*)GetSetMediaTrackInfo(CurTrack, "I_RECARM", NULL)) {
			recArmedTracks += 1;
			if (recArmedTracks > 1)
				return true;
		}
	}
	return false;
}

MediaItem * GetSelectedItemOnTrack_byIndex(WDL_TypedBuf<MediaItem*> origSelItems, vector<int> selItemsPerTrackCount,
	int selTrackIdx, int column)
{
	MediaItem* selItem = NULL;
	int prevSelItemsPerTrackCount = 0;

	if (column <= selItemsPerTrackCount[selTrackIdx]) {
		int offset = 0;

		for (int i = 0; i <= selTrackIdx; i++) {
			if (i >= 1) {
				prevSelItemsPerTrackCount = selItemsPerTrackCount[i - 1];
			}

			offset += prevSelItemsPerTrackCount;
		}
		if (offset + column < origSelItems.GetSize())
			selItem = origSelItems.Get()[offset + column];
	}
	return selItem;
}

void SelectRecArmedTracksOfSelItems(WDL_TypedBuf<MediaItem*> selItems)
{
	for (int i = 0; i < selItems.GetSize(); i++) {
		MediaTrack* track = GetMediaItem_Track(selItems.Get()[i]);

		if (*(int*)GetSetMediaTrackInfo(track, "I_RECARM", NULL))
			SetMediaTrackInfo_Value(track, "I_SELECTED", 1);
	}
}

int GetMaxSelItemsPerTrackCount(vector <int> selItemsPerTrackCount)
{
	int maxVal = 0;
	for (size_t i = 0; i < selItemsPerTrackCount.size(); i++) {
		int val = selItemsPerTrackCount[i];
		if (val > maxVal)
			maxVal = val;
	}
	return maxVal;
}

void UnselectAllTracks() {
	WDL_TypedBuf<MediaTrack*> selTracks;
	SWS_GetSelectedTracks(&selTracks);

	for (int i = 0; i < selTracks.GetSize(); i++) {
		SetMediaTrackInfo_Value(selTracks.Get()[i], "B_UISEL", 0);
	}
}

void UnselectAllItems()
{
	WDL_TypedBuf<MediaItem*> selItems;
	SWS_GetSelectedMediaItems(&selItems);

	for (int i = 0; i < selItems.GetSize(); i++) {
		SetMediaItemInfo_Value(selItems.Get()[i], "B_UISEL", 0);
	}
}

void RestoreOrigTracksAndItemsSelection(WDL_TypedBuf<MediaTrack*> origSelTracks, WDL_TypedBuf<MediaItem*> origSelItems)
{
	for (int i = 0; i < origSelTracks.GetSize(); i++) {
		SetMediaTrackInfo_Value(origSelTracks.Get()[i], "B_UISEL", 1);
	}

	for (int i = 0; i < origSelItems.GetSize(); i++) {
		SetMediaItemInfo_Value(origSelItems.Get()[i], "B_UISEL", 1);
	}
}
// /#587


// Deprecated actions, now you can use the native record/play/stop actions and they will obey the Auto Grouping toggle
// NF: these are now labeled as deprecated

void AWRecordAutoGroup(COMMAND_T* t)
{
	if (GetPlayState() & 4)
	{
		Main_OnCommand(1013, 0); //If recording, toggle recording and group items
		int numItems = CountSelectedMediaItems(0);

		if (numItems > 1)
		{
			Main_OnCommand(40032, 0); //Group selected items
		}
	}
	else
	{
		Main_OnCommand(1013, 0); //If Transport is playing or stopped, Record
	}

	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);
}

void AWRecordConditionalAutoGroup2(COMMAND_T* t)
{
	double t1, t2;
	GetSet_LoopTimeRange(false, false, &t1, &t2, false);


	if (t1 != t2)
	{
		Main_OnCommand(40076, 0); //Set record mode to time selection auto punch
	}

	else if (CountSelectedMediaItems(0) != 0)
	{
		Main_OnCommand(40253, 0); //Set record mode to auto punch selected items
	}

	else
	{
		Main_OnCommand(40252, 0); //Set record mode to normal
	}


	if (GetPlayState() & 4)
	{
		Main_OnCommand(1013, 0); //If recording, toggle recording and group items
		int numItems = CountSelectedMediaItems(0);

		if (numItems > 1)
		{
			Main_OnCommand(40032, 0); //Group selected items
		}
	}
	else
	{
		Main_OnCommand(1013, 0); //If Transport is playing or stopped, Record
	}

	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);
}

void AWRecordConditionalAutoGroup(COMMAND_T* t)
{
	double t1, t2;
	GetSet_LoopTimeRange(false, false, &t1, &t2, false);

	if (*ConfigVar<int>("projrecmode") != 0) // 0 is item punch mode for projrecmode
	{
		if (t1 != t2)
		{
			Main_OnCommand(40076, 0); //Set record mode to time selection auto punch
		}

		else
		{
			Main_OnCommand(40252, 0); //Set record mode to normal
		}
	}

	if (GetPlayState() & 4)
	{
		Main_OnCommand(1013, 0); //If recording, toggle recording and group items
		int numItems = CountSelectedMediaItems(0);

		if (numItems > 1)
		{
			Main_OnCommand(40032, 0); //Group selected items
		}
	}
	else
	{
		Main_OnCommand(1013, 0); //If Transport is playing or stopped, Record
	}

	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);
}

void AWPlayStopAutoGroup(COMMAND_T* t)
{
	if (GetPlayState() & 4)
	{
		Main_OnCommand(1016, 0); //If recording, Stop
		int numItems = CountSelectedMediaItems(0);

		if ((numItems > 1) && ((*ConfigVar<int>("autoxfade") & 4) || (*ConfigVar<int>("autoxfade") & 8))) // Don't group if not in tape or overlap record mode, takes mode is messy
		{
			Main_OnCommand(40032, 0); //Group selected items
		}
	}
	else
	{
		Main_OnCommand(40044, 0); //If Transport is playing or stopped, Play/Stop
	}

	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);
}






void AWSelectToEnd(COMMAND_T* t)
{
	double projEnd = 0;

	// Find end position of last item in project

	for (int trackIndex = 0; trackIndex < GetNumTracks(); trackIndex++)
	{
		MediaTrack* track = GetTrack(0, trackIndex);

		int itemNum = GetTrackNumMediaItems(track) - 1;

		MediaItem* item = GetTrackMediaItem(track, itemNum);

		double itemEnd = GetMediaItemInfo_Value(item, "D_POSITION") + GetMediaItemInfo_Value(item, "D_LENGTH");

		if (itemEnd > projEnd)
		{
			projEnd = itemEnd;
		}
	}

	double selStart = GetCursorPosition();

	GetSet_LoopTimeRange2(0, 1, 0, &selStart, &projEnd, 0);

	Main_OnCommand(40718, 0); // Select all items in time selection on selected tracks

	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS | UNDO_STATE_MISCCFG, -1);
}


void AWFadeSelection(COMMAND_T* t)
{
	double dFadeLen;
	double dEdgeAdj;
	double dEdgeAdj1;
	double dEdgeAdj2;
	bool leftFlag=false;
	bool rightFlag=false;


	double selStart;
	double selEnd;

	GetSet_LoopTimeRange2(0, 0, 0, &selStart, &selEnd, 0);



	for (int iTrack = 1; iTrack <= GetNumTracks(); iTrack++)
	{
		MediaTrack* tr = CSurf_TrackFromID(iTrack, false);
		for (int iItem1 = 0; iItem1 < GetTrackNumMediaItems(tr); iItem1++)
		{
			MediaItem* item1 = GetTrackMediaItem(tr, iItem1);
			if (*(bool*)GetSetMediaItemInfo(item1, "B_UISEL", NULL))
			{
				double dStart1 = *(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL);
				double dEnd1   = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) + dStart1;

				leftFlag = false;
				rightFlag = false;


				//Crossfade items section

				// Try to match up the end with the start of the other selected media items
				for (int iItem2 = 0; iItem2 < GetTrackNumMediaItems(tr); iItem2++)
				{

					MediaItem* item2 = GetTrackMediaItem(tr, iItem2);

					if (item1 != item2 && *(bool*)GetSetMediaItemInfo(item2, "B_UISEL", NULL))
					{

						double dStart2 = *(double*)GetSetMediaItemInfo(item2, "D_POSITION", NULL);
						double dEnd2   = *(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL) + dStart2;
						//double dTest = fabs(dEnd1 - dStart2);

						// Need a tolerance for "items are adjacent". Also check to see if split is inside time selection
						if ((fabs(dEnd1 - dStart2) < SWS_ADJACENT_ITEM_THRESHOLD))
						{   // Found a matching item

							// If split is within selection and selection is within total item selection
							if ((selStart > dStart1) && (selEnd < dEnd2) && (selStart < dStart2) && (selEnd > dEnd1))
							{
								dFadeLen = selEnd - selStart;
								dEdgeAdj1 = selEnd - dEnd1;
								dEdgeAdj2 = dStart2 - selStart;

								MediaItem_Take* activeTake = GetActiveTake(item2);
								if (activeTake && dEdgeAdj2 > *(double*)GetSetMediaItemTakeInfo(activeTake, "D_STARTOFFS", NULL))
								{
									dFadeLen -= dEdgeAdj2;
									dEdgeAdj2 = *(double*)GetSetMediaItemTakeInfo(activeTake, "D_STARTOFFS", NULL);
									dFadeLen += dEdgeAdj2;
								}

								// Move the edges around and set the crossfades
								double dLen1 = dEnd1 - dStart1 + dEdgeAdj1;
								*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
								GetSetMediaItemInfo(item1, "D_FADEOUTLEN_AUTO", &dFadeLen);

								double dLen2 = *(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL);
								dStart2 -= dEdgeAdj2;
								dLen2   += dEdgeAdj2;
								*(double*)GetSetMediaItemInfo(item2, "D_POSITION", NULL) = dStart2;
								*(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL) = dLen2;
								GetSetMediaItemInfo(item2, "D_FADEINLEN_AUTO", &dFadeLen);
								double dSnapOffset2 = *(double*)GetSetMediaItemInfo(item2, "D_SNAPOFFSET", NULL);
								if (dSnapOffset2)
								{   // Only adjust the snap offset if it's non-zero
									dSnapOffset2 += dEdgeAdj2;
									GetSetMediaItemInfo(item2, "D_SNAPOFFSET", &dSnapOffset2);
								}

								for (int iTake = 0; iTake < GetMediaItemNumTakes(item2); iTake++)
								{
									MediaItem_Take* take = GetMediaItemTake(item2, iTake);
									if (take)
									{
										double dOffset = *(double*)GetSetMediaItemTakeInfo(take, "D_STARTOFFS", NULL);
										double dPlayrate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
										dOffset -= (dEdgeAdj2 * dPlayrate);
										GetSetMediaItemTakeInfo(take, "D_STARTOFFS", &dOffset);

										UpdateStretchMarkersAfterSetTakeStartOffset(take, dEdgeAdj2 * dPlayrate); // NF fix
									}
								}
								rightFlag=true;
								//leftFlag=true;
								break;

							}
							// If selection does not surround split or does not exist


							else
							{
								dFadeLen = fabs(*ConfigVar<double>("defsplitxfadelen")); // Abs because neg value means "not auto"
								dEdgeAdj1 = dFadeLen / 2.0;
								dEdgeAdj2 = dFadeLen / 2.0;

								// Need to ensure that there's "room" to move the start of the second item back
								// Check all of the takes' start offset before doing any "work"
								MediaItem_Take* activeTake = GetActiveTake(item2);
								if (activeTake && dEdgeAdj2 > *(double*)GetSetMediaItemTakeInfo(activeTake, "D_STARTOFFS", NULL))
								{
									dEdgeAdj2 = *(double*)GetSetMediaItemTakeInfo(activeTake, "D_STARTOFFS", NULL);
									dEdgeAdj1 = dFadeLen - dEdgeAdj2;
								}


								// We're all good, move the edges around and set the crossfades
								double dLen1 = dEnd1 - dStart1 + dEdgeAdj1;
								*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
								GetSetMediaItemInfo(item1, "D_FADEOUTLEN_AUTO", &dFadeLen);

								double dLen2 = *(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL);
								dStart2 -= dEdgeAdj2;
								dLen2   += dEdgeAdj2;
								*(double*)GetSetMediaItemInfo(item2, "D_POSITION", NULL) = dStart2;
								*(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL) = dLen2;
								GetSetMediaItemInfo(item2, "D_FADEINLEN_AUTO", &dFadeLen);
								double dSnapOffset2 = *(double*)GetSetMediaItemInfo(item2, "D_SNAPOFFSET", NULL);
								if (dSnapOffset2)
								{   // Only adjust the snap offset if it's non-zero
									dSnapOffset2 += dEdgeAdj2;
									GetSetMediaItemInfo(item2, "D_SNAPOFFSET", &dSnapOffset2);
								}

								for (int iTake = 0; iTake < GetMediaItemNumTakes(item2); iTake++)
								{
									MediaItem_Take* take = GetMediaItemTake(item2, iTake);
									if (take)
									{
										double dOffset = *(double*)GetSetMediaItemTakeInfo(take, "D_STARTOFFS", NULL);
										double dPlayrate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);

										dOffset -= (dEdgeAdj2 * dPlayrate);
										GetSetMediaItemTakeInfo(take, "D_STARTOFFS", &dOffset);

										UpdateStretchMarkersAfterSetTakeStartOffset(take, dEdgeAdj2 * dPlayrate); // NF fix
									}
								}
								rightFlag=true;

								//Added Mar 16/2011, fixes fade in getting created if edit cursor placed in first half of left item, this is technically expected behavior but not in reality
								leftFlag=true;
								break;
							}



						}


						// If items are adjacent in opposite direction, turn flag on. Actual fades handled when situation is found
						// in reverse, but still need to flag it here

						else if ((fabs(dEnd2 - dStart1) < SWS_ADJACENT_ITEM_THRESHOLD))
						{
							leftFlag=true;

							//Added Mar 16/2011, fixes fade out getting created on right item if edit cursor is within last half of it
							rightFlag=true;
						}


						// if items already overlap (item 1 on left and item 2 on right)

						else if ((dStart2 < dEnd1) && (dEnd1 < dEnd2) && (dStart1 < dStart2))
						{


							// If split is within selection and selection is within total item selection
							if ((selStart > dStart1) && (selEnd < dEnd2) && (selEnd > dStart2) && (selStart < dEnd1))
							{
								dFadeLen = selEnd - selStart;
								dEdgeAdj1 = selEnd - dEnd1;

								dEdgeAdj2 = dStart2 - selStart;


								// Need to ensure that there's "room" to move the start of the second item back
								// Check all of the takes' start offset before doing any "work"
								MediaItem_Take* activeTake = GetActiveTake(item2);
								if (activeTake && dEdgeAdj2 > *(double*)GetSetMediaItemTakeInfo(activeTake, "D_STARTOFFS", NULL))
								{
									dFadeLen -= dEdgeAdj2;
									dEdgeAdj2 = *(double*)GetSetMediaItemTakeInfo(activeTake, "D_STARTOFFS", NULL);
									dFadeLen += dEdgeAdj2;
								}


								// We're all good, move the edges around and set the crossfades
								double dLen1 = dEnd1 - dStart1 + dEdgeAdj1;
								*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
								GetSetMediaItemInfo(item1, "D_FADEOUTLEN_AUTO", &dFadeLen);

								double dLen2 = *(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL);
								dStart2 -= dEdgeAdj2;
								dLen2   += dEdgeAdj2;
								*(double*)GetSetMediaItemInfo(item2, "D_POSITION", NULL) = dStart2;
								*(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL) = dLen2;
								GetSetMediaItemInfo(item2, "D_FADEINLEN_AUTO", &dFadeLen);
								double dSnapOffset2 = *(double*)GetSetMediaItemInfo(item2, "D_SNAPOFFSET", NULL);
								if (dSnapOffset2)
								{   // Only adjust the snap offset if it's non-zero
									dSnapOffset2 += dEdgeAdj2;
									GetSetMediaItemInfo(item2, "D_SNAPOFFSET", &dSnapOffset2);
								}

								for (int iTake = 0; iTake < GetMediaItemNumTakes(item2); iTake++)
								{
									MediaItem_Take* take = GetMediaItemTake(item2, iTake);
									if (take)
									{
										double dOffset = *(double*)GetSetMediaItemTakeInfo(take, "D_STARTOFFS", NULL);
										double dPlayrate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
										dOffset -= (dEdgeAdj2 * dPlayrate);
										GetSetMediaItemTakeInfo(take, "D_STARTOFFS", &dOffset);

										UpdateStretchMarkersAfterSetTakeStartOffset(take, dEdgeAdj2 * dPlayrate); // NF fix
									}
								}

								// Set both flags, prevents bad behavior with "trim" fade when selection only overlaps left side of crossfade
								// Normally the leftFlag wouldn't get set so the trim fade code creates a fade in
								// No harm in leftFlag here because it has been verified that the selection does not extend past the left
								// edge anyways.
								leftFlag=true;
								rightFlag=true;
								break;

							}


							// If selection does not surround split or does not exist
							else
							{
								dFadeLen = dEnd1 - dStart2;
								dEdgeAdj = 0;

								// Need to ensure that there's "room" to move the start of the second item back
								// Check all of the takes' start offset before doing any "work"
								int iTake;
								for (iTake = 0; iTake < GetMediaItemNumTakes(item2); iTake++)
									if (dEdgeAdj > *(double*)GetSetMediaItemTakeInfo(GetMediaItemTake(item2, iTake), "D_STARTOFFS", NULL))
										break;
								if (iTake < GetMediaItemNumTakes(item2))
									continue;   // Keep looking

								// We're all good, move the edges around and set the crossfades
								double dLen1 = dEnd1 - dStart1 + dEdgeAdj;
								*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
								GetSetMediaItemInfo(item1, "D_FADEOUTLEN_AUTO", &dFadeLen);

								double dLen2 = *(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL);
								dStart2 -= dEdgeAdj;
								dLen2   += dEdgeAdj;
								*(double*)GetSetMediaItemInfo(item2, "D_POSITION", NULL) = dStart2;
								*(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL) = dLen2;
								GetSetMediaItemInfo(item2, "D_FADEINLEN_AUTO", &dFadeLen);
								double dSnapOffset2 = *(double*)GetSetMediaItemInfo(item2, "D_SNAPOFFSET", NULL);
								if (dSnapOffset2)
								{   // Only adjust the snap offset if it's non-zero
									dSnapOffset2 += dEdgeAdj;
									GetSetMediaItemInfo(item2, "D_SNAPOFFSET", &dSnapOffset2);
								}

								for (iTake = 0; iTake < GetMediaItemNumTakes(item2); iTake++)
								{
									MediaItem_Take* take = GetMediaItemTake(item2, iTake);
									if (take)
									{
										double dOffset = *(double*)GetSetMediaItemTakeInfo(take, "D_STARTOFFS", NULL);
										double dPlayrate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
										dOffset -= (dEdgeAdj * dPlayrate);
										GetSetMediaItemTakeInfo(take, "D_STARTOFFS", &dOffset);

										UpdateStretchMarkersAfterSetTakeStartOffset(take, dEdgeAdj * dPlayrate); // NF fix
									}
								}
								rightFlag=true;

								//Added Mar16/2011, fixes unexpected fade in's from "fade in to cursor" section
								leftFlag=true;
								break;
							}



						}



						// If the items overlap backwards ie item 2 is before item 1, turn on leftFlag
						// Actual fade calculations and stuff will be handled when the overlap gets discovered the opposite way
						// but this prevents other weird behavior


						else if ((dStart1 < dEnd2) && (dStart2 < dStart1) && (dEnd1 > dEnd2))
						{
							leftFlag=true;
							rightFlag=true;
						}



						// If there's a gap between two adjacent items, item 1 before item 2
						else if ((dEnd1 < dStart2))
						{

							// Make sure there's no other items in the gap, otherwise you get a crossfade between two items and an item in the middle that overlaps the crossfade

							bool itemBetweenFlag;

							for (int iItem3 = 0; iItem3 < GetTrackNumMediaItems(tr); iItem3++)
							{

								MediaItem* item3 = GetTrackMediaItem(tr, iItem3);

								if (item3 != item1 && item3 != item2 && *(bool*)GetSetMediaItemInfo(item3, "B_UISEL", NULL))
								{

									double dStart3 = *(double*)GetSetMediaItemInfo(item3, "D_POSITION", NULL);
									double dEnd3   = *(double*)GetSetMediaItemInfo(item3, "D_LENGTH", NULL) + dStart3;

									// Check to see if item3 is between item1 and 2
									if (dStart3 > dEnd1 && dEnd3 < dStart2)
									{
										itemBetweenFlag = TRUE;
									}

								}
							}

							// If selection starts inside item1, crosses gap and ends inside item 2, and there's no items in between
							if ((selStart > dStart1) && (selStart < dEnd1) && (selEnd > dStart2) && (selEnd < dEnd2) && !itemBetweenFlag)
							{
								dFadeLen = selEnd - selStart;
								dEdgeAdj1 = selEnd - dEnd1;
								dEdgeAdj2 = dStart2 - selStart;


								// Need to ensure that there's "room" to move the start of the second item back
								// Check all of the takes' start offset before doing any "work"
								int iTake;


								for (iTake = 0; iTake < GetMediaItemNumTakes(item2); iTake++)
									if (dEdgeAdj2 > *(double*)GetSetMediaItemTakeInfo(GetMediaItemTake(item2, iTake), "D_STARTOFFS", NULL))
									{
										dFadeLen -= dEdgeAdj2;

										dEdgeAdj2 = *(double*)GetSetMediaItemTakeInfo(GetMediaItemTake(item2, iTake), "D_STARTOFFS", NULL);

										dFadeLen += dEdgeAdj2;
									}
								if (iTake < GetMediaItemNumTakes(item2))
									continue;   // Keep looking



								// We're all good, move the edges around and set the crossfades
								double dLen1 = dEnd1 - dStart1 + dEdgeAdj1;
								*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
								GetSetMediaItemInfo(item1, "D_FADEOUTLEN_AUTO", &dFadeLen);

								double dLen2 = *(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL);
								dStart2 -= dEdgeAdj2;
								dLen2   += dEdgeAdj2;

								*(double*)GetSetMediaItemInfo(item2, "D_POSITION", NULL) = dStart2;
								*(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL) = dLen2;
								GetSetMediaItemInfo(item2, "D_FADEINLEN_AUTO", &dFadeLen);
								double dSnapOffset2 = *(double*)GetSetMediaItemInfo(item2, "D_SNAPOFFSET", NULL);
								if (dSnapOffset2)
								{   // Only adjust the snap offset if it's non-zero
									dSnapOffset2 += dEdgeAdj2;
									GetSetMediaItemInfo(item2, "D_SNAPOFFSET", &dSnapOffset2);
								}

								for (iTake = 0; iTake < GetMediaItemNumTakes(item2); iTake++)
								{
									MediaItem_Take* take = GetMediaItemTake(item2, iTake);
									if (take)
									{
										double dOffset = *(double*)GetSetMediaItemTakeInfo(take, "D_STARTOFFS", NULL);
										double dPlayrate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
										dOffset -= (dEdgeAdj2 * dPlayrate);
										GetSetMediaItemTakeInfo(take, "D_STARTOFFS", &dOffset);

										UpdateStretchMarkersAfterSetTakeStartOffset(take, dEdgeAdj2 * dPlayrate); // NF fix
									}
								}
								rightFlag=true;
								break;

							}
						}

						// Else if there's a gap but the items are in reverse order, set the opposite flag but let the fades
						// get taken care of when it finds them "item 1 (gap) item 2"

						else if ((dEnd2 < dStart1))
						{
							if ((selStart > dStart2) && (selStart < dEnd2) && (selEnd > dStart1) && (selEnd < dEnd1))
							{
								leftFlag=true;

								//Check for selected items in between item 1 and 2

								for (int iItem3 = 0; iItem3 < GetTrackNumMediaItems(tr); iItem3++)
								{

									MediaItem* item3 = GetTrackMediaItem(tr, iItem3);

									if (item1 != item2 && item2 != item3 && *(bool*)GetSetMediaItemInfo(item3, "B_UISEL", NULL))
									{

										double dStart3 = *(double*)GetSetMediaItemInfo(item3, "D_POSITION", NULL);
										double dEnd3   = *(double*)GetSetMediaItemInfo(item3, "D_LENGTH", NULL) + dStart3;

										// If end of item 3 is between end of item 2 and start of item 1, reset the flag

										if (dEnd3 > dEnd2 && dEnd3 <= dStart1)
										{
											leftFlag = false;
										}

									}
								}
							}
						}
					}
				}

				if (!(leftFlag))
				{
					double fadeLength;

					if ((selStart <= dStart1) && (selEnd > dStart1) && (selEnd < dEnd1) && (!(rightFlag)))
					{
						fadeLength = selEnd - dStart1;
						SetMediaItemInfo_Value(item1, "D_FADEINLEN", fadeLength);
						SetMediaItemInfo_Value(item1, "D_FADEINLEN_AUTO", 0.0);
					}

					else if (selStart <= dStart1 && selEnd >= dEnd1)
					{
						fadeLength = fabs(*ConfigVar<double>("deffadelen")); // Abs because neg value means "not auto"
						SetMediaItemInfo_Value(item1, "D_FADEINLEN", fadeLength);

					}

					else if (selStart > dStart1 && selEnd < dEnd1)
					{
						double dFadeIn = selStart - dStart1;
						//double dFadeOut = dEnd1 - selEnd;

						SetMediaItemInfo_Value(item1, "D_FADEINLEN", dFadeIn);
						SetMediaItemInfo_Value(item1, "D_FADEINLEN_AUTO", 0.0);
						//SetMediaItemInfo_Value(item1, "D_FADEOUTLEN_AUTO", dFadeOut);
					}

					///* Might break shit, replaces D and G in PT
					else if ((selStart < dStart1 && selEnd < dStart1) || (selStart > dEnd1 && selEnd > dEnd1) || (selStart == selEnd))
					{
						double cursorPos = GetCursorPosition();
						double dLength1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);


						if (cursorPos > dStart1 && cursorPos < (dStart1 + (0.5 * dLength1)))
						{
							fadeLength = cursorPos - dStart1;
							SetMediaItemInfo_Value(item1, "D_FADEINLEN", fadeLength);
							SetMediaItemInfo_Value(item1, "D_FADEINLEN_AUTO", 0.0);
						}

					}
					//*/
				}

				if (!(rightFlag))
				{
					double fadeLength;

					if ((selStart > dStart1) && (selEnd >= dEnd1) && (selStart < dEnd1) && (!(leftFlag)))
					{
						fadeLength = dEnd1 - selStart;
						SetMediaItemInfo_Value(item1, "D_FADEOUTLEN", fadeLength);
						SetMediaItemInfo_Value(item1, "D_FADEOUTLEN_AUTO", 0.0);
					}
					else if (selStart <= dStart1 && selEnd >= dEnd1)
					{
						fadeLength = fabs(*ConfigVar<double>("deffadelen")); // Abs because neg value means "not auto"
						SetMediaItemInfo_Value(item1, "D_FADEOUTLEN", fadeLength);
					}

					else if (selStart > dStart1 && selEnd < dEnd1)
					{
						//double dFadeIn = selStart - dStart1;
						double dFadeOut = dEnd1 - selEnd;

						//SetMediaItemInfo_Value(item1, "D_FADEINLEN_AUTO", dFadeIn);
						SetMediaItemInfo_Value(item1, "D_FADEOUTLEN", dFadeOut);
						SetMediaItemInfo_Value(item1, "D_FADEOUTLEN_AUTO", 0.0);
					}


					///* Might break shit, replaces D and G in PT
					else if ((selStart < dStart1 && selEnd < dStart1) || (selStart > dEnd1 && selEnd > dEnd1) || (selStart == selEnd))
					{
						double cursorPos = GetCursorPosition();
						double dLength1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);


						if (cursorPos > (dStart1 + (0.5 * dLength1)) && cursorPos < dEnd1)
						{
							fadeLength = dEnd1 - cursorPos;
							SetMediaItemInfo_Value(item1, "D_FADEOUTLEN", fadeLength);
							SetMediaItemInfo_Value(item1, "D_FADEOUTLEN_AUTO", 0.0);
						}

					}
					//*/
				}
			}
		}
	}

	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);

}





void AWTrimCrop(COMMAND_T* t)
{

	double selStart;
	double selEnd;

	GetSet_LoopTimeRange2(0, 0, 0, &selStart, &selEnd, 0);

	if (selStart != selEnd)
	{
		Main_OnCommand(40508, 0); // If there's a time selection, just run native Trim action because I am lazy
	}
	else // Otherwise trim to cursor
	{
		double cursorPos = GetCursorPosition();

		for (int iTrack = 1; iTrack <= GetNumTracks(); iTrack++)
		{
			MediaTrack* tr = CSurf_TrackFromID(iTrack, false);
			for (int iItem1 = 0; iItem1 < GetTrackNumMediaItems(tr); iItem1++)
			{
				MediaItem* item1 = GetTrackMediaItem(tr, iItem1);
				if (*(bool*)GetSetMediaItemInfo(item1, "B_UISEL", NULL))
				{
					double dStart1 = *(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL);
					double dLen1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);

					if (cursorPos > dStart1 && cursorPos < (dStart1 + (0.5 * dLen1)))
					{
						Main_OnCommand(40511, 0);
					}
					else if (cursorPos < (dStart1 + dLen1) && cursorPos > (dStart1 + (0.5 * dLen1)))
					{
						Main_OnCommand(40512, 0);
					}

				}
			}
		}
	}

	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);

}







// In Progress



void AWTrimFill(COMMAND_T* t)
{

	double selStart;
	double selEnd;
	bool leftFlag = false;
	bool rightFlag = false;

	GetSet_LoopTimeRange2(0, 0, 0, &selStart, &selEnd, 0);

	double cursorPos = GetCursorPosition();

	for (int iTrack = 1; iTrack <= GetNumTracks(); iTrack++)
	{
		MediaTrack* tr = CSurf_TrackFromID(iTrack, false);
		for (int iItem1 = 0; iItem1 < GetTrackNumMediaItems(tr); iItem1++)
		{
			MediaItem* item1 = GetTrackMediaItem(tr, iItem1);
			if (*(bool*)GetSetMediaItemInfo(item1, "B_UISEL", NULL))
			{
				double dStart1 = *(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL);
				double dEnd1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) + dStart1;

				// Reset flags
				leftFlag = false;
				rightFlag = false;

				// If the item is selected the the time selection crosses either of it's edges
				if ((selStart < dEnd1 && selEnd > dEnd1) || (selStart < dStart1 && selEnd > dStart1))
				{

					// Check for other items that are also crossed by time selection that may be earlier  or later
					for (int iItem2 = 0; iItem2 < GetTrackNumMediaItems(tr); iItem2++)
					{

						MediaItem* item2 = GetTrackMediaItem(tr, iItem2);

						if (item1 != item2 && *(bool*)GetSetMediaItemInfo(item2, "B_UISEL", NULL))
						{

							double dStart2 = *(double*)GetSetMediaItemInfo(item2, "D_POSITION", NULL);
							double dEnd2   = *(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL) + dStart2;

							// If selection crosses right edge
							if (selStart < dEnd1 && selEnd >= dEnd1)
							{
								// If selection ends after item 2 starts
								if (selEnd >= dStart2)
								{
									// Set right flag if item 2 ends after item 1 ends
									if (dEnd2 > dEnd1)
										rightFlag = true;
								}
							}


							// If selection crosses left edge
							if (selStart <= dStart1 && selEnd > dStart1)
							{
								// If selection starts before item 2 ends
								if  (selStart <= dEnd2)
								{
									// Set left flag if item 2 starts before item 1
									if (dStart2 < dStart1)
										leftFlag = true;
								}
							}
						}


					}

					if (selEnd < dEnd1)
						rightFlag = true;

					if (selStart > dStart1)
						leftFlag = true;

					if (!(leftFlag))
					{

						double dLen1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);
						double edgeAdj = dStart1 - selStart;

						dLen1 += edgeAdj;

						//*(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL) = selStart;
						//*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
						GetSetMediaItemInfo(item1, "D_POSITION", &selStart);
						GetSetMediaItemInfo(item1, "D_LENGTH", &dLen1);

						for (int iTake = 0; iTake < GetMediaItemNumTakes(item1); iTake++)
						{
							MediaItem_Take* take = GetMediaItemTake(item1, iTake);
							if (take)
							{
								double dOffset = *(double*)GetSetMediaItemTakeInfo(take, "D_STARTOFFS", NULL);
								double dPlayrate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
								dOffset -= (edgeAdj * dPlayrate);
								GetSetMediaItemTakeInfo(take, "D_STARTOFFS", &dOffset);

								UpdateStretchMarkersAfterSetTakeStartOffset(take, edgeAdj * dPlayrate); // NF fix
							}
						}


					}

					if (!(rightFlag))
					{

						double dLen1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);
						double edgeAdj = selEnd - dEnd1;

						dLen1 += edgeAdj;

						//*(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL) = selStart;
						//*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
						GetSetMediaItemInfo(item1, "D_LENGTH", &dLen1);
					}

				}


				// If there's no time selection
				else //if (selStart = selEnd)
				{


					// Check for other items that are also selected on track
					for (int iItem2 = 0; iItem2 < GetTrackNumMediaItems(tr); iItem2++)
					{

						MediaItem* item2 = GetTrackMediaItem(tr, iItem2);

						if (item1 != item2 && *(bool*)GetSetMediaItemInfo(item2, "B_UISEL", NULL))
						{

							double dStart2 = *(double*)GetSetMediaItemInfo(item2, "D_POSITION", NULL);
							double dEnd2   = *(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL) + dStart2;

							// If cursor is before item 1
							if (cursorPos < dStart1)
							{
								// If cursor is before item 2
								if (cursorPos < dEnd2)
								{
									// Set left flag if item 2 comes first
									if (dStart1 > dStart2)
										leftFlag = true;
								}
							}


							// If cursor is after item 1
							if (cursorPos > dEnd1)
							{
								// If cursor is after item 2
								if  (cursorPos > dStart2)
								{
									// Set right flag if item 2 ends later than item 1
									if (dEnd1 < dEnd2)
										rightFlag = true;
								}
							}
						}


					}


					if (cursorPos >= dStart1)
					{
						leftFlag = true;
					}
					if (cursorPos <= dEnd1)
					{
						rightFlag = true;
					}


					if (!(leftFlag))
					{

						double dLen1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);
						double edgeAdj = dStart1 - cursorPos;

						dLen1 += edgeAdj;

						//*(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL) = selStart;
						//*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
						GetSetMediaItemInfo(item1, "D_POSITION", &cursorPos);
						GetSetMediaItemInfo(item1, "D_LENGTH", &dLen1);

						for (int iTake = 0; iTake < GetMediaItemNumTakes(item1); iTake++)
						{
							MediaItem_Take* take = GetMediaItemTake(item1, iTake);
							if (take)
							{
								double dOffset = *(double*)GetSetMediaItemTakeInfo(take, "D_STARTOFFS", NULL);
								double dPlayrate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
								dOffset -= (edgeAdj * dPlayrate);
								GetSetMediaItemTakeInfo(take, "D_STARTOFFS", &dOffset);

								UpdateStretchMarkersAfterSetTakeStartOffset(take, edgeAdj * dPlayrate); // NF fix
							}
						}


					}

					if (!(rightFlag))
					{

						double dLen1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);
						double edgeAdj = cursorPos - dEnd1;

						dLen1 += edgeAdj;

						//*(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL) = selStart;
						//*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
						GetSetMediaItemInfo(item1, "D_LENGTH", &dLen1);
					}
				}

			}
		}
	}


	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);

}



void AWStretchFill(COMMAND_T* t)
{

	double selStart;
	double selEnd;
	bool leftFlag = false;
	bool rightFlag = false;

	GetSet_LoopTimeRange2(0, 0, 0, &selStart, &selEnd, 0);

	double cursorPos = GetCursorPosition();

	for (int iTrack = 1; iTrack <= GetNumTracks(); iTrack++)
	{
		MediaTrack* tr = CSurf_TrackFromID(iTrack, false);
		for (int iItem1 = 0; iItem1 < GetTrackNumMediaItems(tr); iItem1++)
		{
			MediaItem* item1 = GetTrackMediaItem(tr, iItem1);
			if (*(bool*)GetSetMediaItemInfo(item1, "B_UISEL", NULL))
			{
				double dStart1 = *(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL);
				double dEnd1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) + dStart1;

				// Reset flags
				leftFlag = false;
				rightFlag = false;

				// If the item is selected the the time selection crosses either of it's edges
				if ((selStart < dEnd1 && selEnd > dEnd1) || (selStart < dStart1 && selEnd > dStart1))
				{

					// Check for other items that are also crossed by time selection that may be earlier  or later
					for (int iItem2 = 0; iItem2 < GetTrackNumMediaItems(tr); iItem2++)
					{

						MediaItem* item2 = GetTrackMediaItem(tr, iItem2);

						if (item1 != item2 && *(bool*)GetSetMediaItemInfo(item2, "B_UISEL", NULL))
						{

							double dStart2 = *(double*)GetSetMediaItemInfo(item2, "D_POSITION", NULL);
							double dEnd2   = *(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL) + dStart2;

							// If selection crosses right edge
							if (selStart < dEnd1 && selEnd >= dEnd1)
							{
								// If selection ends after item 2 starts
								if (selEnd >= dStart2)
								{
									// Set right flag if item 2 ends after item 1 ends
									if (dEnd2 > dEnd1)
										rightFlag = true;
								}
							}


							// If selection crosses left edge
							if (selStart <= dStart1 && selEnd > dStart1)
							{
								// If selection starts before item 2 ends
								if  (selStart <= dEnd2)
								{
									// Set left flag if item 2 starts before item 1
									if (dStart2 < dStart1)
										leftFlag = true;
								}
							}
						}


					}

					if (selEnd < dEnd1)
						rightFlag = true;

					if (selStart > dStart1)
						leftFlag = true;

					if (!(leftFlag))
					{

						double dLen1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);
						double edgeAdj = dStart1 - selStart;

						double dNewLen1 = dLen1 + edgeAdj;

						//*(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL) = selStart;
						//*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;

						GetSetMediaItemInfo(item1, "D_POSITION", &selStart);
						GetSetMediaItemInfo(item1, "D_LENGTH", &dNewLen1);


						for (int iTake = 0; iTake < GetMediaItemNumTakes(item1); iTake++)
						{
							MediaItem_Take* take = GetMediaItemTake(item1, iTake);
							if (take)
							{
								double dRate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
								dRate *= (dLen1/dNewLen1);
								GetSetMediaItemTakeInfo(take, "D_PLAYRATE", &dRate);
							}
						}


					}

					if (!(rightFlag))
					{

						double dLen1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);
						double edgeAdj = selEnd - dEnd1;

						double dNewLen1 = dLen1 + edgeAdj;

						//*(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL) = selStart;
						//*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
						GetSetMediaItemInfo(item1, "D_LENGTH", &dNewLen1);

						for (int iTake = 0; iTake < GetMediaItemNumTakes(item1); iTake++)
						{
							MediaItem_Take* take = GetMediaItemTake(item1, iTake);
							if (take)
							{
								double dRate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
								dRate *= (dLen1/dNewLen1);
								GetSetMediaItemTakeInfo(take, "D_PLAYRATE", &dRate);
							}
						}


					}

				}


				else //if (selStart = selEnd)
				{


					// Check for other items that are also selected on track
					for (int iItem2 = 0; iItem2 < GetTrackNumMediaItems(tr); iItem2++)
					{

						MediaItem* item2 = GetTrackMediaItem(tr, iItem2);

						if (item1 != item2 && *(bool*)GetSetMediaItemInfo(item2, "B_UISEL", NULL))
						{

							double dStart2 = *(double*)GetSetMediaItemInfo(item2, "D_POSITION", NULL);
							double dEnd2   = *(double*)GetSetMediaItemInfo(item2, "D_LENGTH", NULL) + dStart2;

							// If cursor is before item 1
							if (cursorPos < dStart1)
							{
								// If cursor is before item 2
								if (cursorPos < dEnd2)
								{
									// Set left flag if item 2 comes first
									if (dStart1 > dStart2)
										leftFlag = true;
								}
							}


							// If cursor is after item 1
							if (cursorPos > dEnd1)
							{
								// If cursor is after item 2
								if  (cursorPos > dStart2)
								{
									// Set right flag if item 2 ends later than item 1
									if (dEnd1 < dEnd2)
										rightFlag = true;
								}
							}
						}


					}


					if (cursorPos >= dStart1)
					{
						leftFlag = true;
					}
					if (cursorPos <= dEnd1)
					{
						rightFlag = true;
					}


					if (!(leftFlag))
					{

						double dLen1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);
						double edgeAdj = dStart1 - cursorPos;

						double dNewLen1 = dLen1 + edgeAdj;

						//*(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL) = selStart;
						//*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;

						GetSetMediaItemInfo(item1, "D_POSITION", &cursorPos);
						GetSetMediaItemInfo(item1, "D_LENGTH", &dNewLen1);


						for (int iTake = 0; iTake < GetMediaItemNumTakes(item1); iTake++)
						{
							MediaItem_Take* take = GetMediaItemTake(item1, iTake);
							if (take)
							{
								double dRate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
								dRate *= (dLen1/dNewLen1);
								GetSetMediaItemTakeInfo(take, "D_PLAYRATE", &dRate);
							}
						}


					}

					if (!(rightFlag))
					{

						double dLen1 = *(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL);
						double edgeAdj = cursorPos - dEnd1;

						double dNewLen1 = dLen1 + edgeAdj;

						//*(double*)GetSetMediaItemInfo(item1, "D_POSITION", NULL) = selStart;
						//*(double*)GetSetMediaItemInfo(item1, "D_LENGTH", NULL) = dLen1;
						GetSetMediaItemInfo(item1, "D_LENGTH", &dNewLen1);

						for (int iTake = 0; iTake < GetMediaItemNumTakes(item1); iTake++)
						{
							MediaItem_Take* take = GetMediaItemTake(item1, iTake);
							if (take)
							{
								double dRate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
								dRate *= (dLen1/dNewLen1);
								GetSetMediaItemTakeInfo(take, "D_PLAYRATE", &dRate);
							}
						}


					}
				}

			}
		}
	}



	UpdateTimeline();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);

}



// Experimental cut, copy and paste that will let you use relative paste since JCS are mean

void AWBusDelete(COMMAND_T* t)
{
	if (GetCursorContext() == 0)
	{
		const int cnt = CountSelectedTracks(NULL);
		for (int i = 0; i < cnt; i++)
		{
			if (GetMediaTrackInfo_Value(GetSelectedTrack(0, i), "I_FOLDERDEPTH") == 1)
				SetMediaTrackInfo_Value(GetSelectedTrack(0, i), "I_FOLDERDEPTH", 0);
		}
	}

	SmartRemove(NULL);
}

void AWPaste(COMMAND_T* t)
{
	Undo_BeginBlock();

	const int pTrimMode = *ConfigVar<int>("autoxfade");

	if (GetCursorContext() == 1 && pTrimMode & 2)
	{
		// Enable "move edit cursor on paste" so that the time selection can be set properly
		const ConfigVar<int> itemclickmovecurs("itemclickmovecurs");
		ConfigVarOverride<int> tmpCursorMode(itemclickmovecurs, *itemclickmovecurs & ~ItemClickMoveCurs::MoveOnPaste);

		double cursorPos = GetCursorPosition();

		Main_OnCommand(40625, 0); // Set time selection start point
		Main_OnCommand(40058, 0); // item paste items tracks
		Main_OnCommand(40626, 0); // time selection set end point

		// Select only tracks with selected items

		SelTracksWItems();

		Main_OnCommand(40718, 0); // item select all item on selected track in time selection
		Main_OnCommand(40312, 0); // item remove selected area of item

		// Go to beginning of selection, this is smoother than using actual action and easier
		SetEditCurPos(cursorPos, 0, 0);

		DoSelectFirstOfSelectedTracks(NULL);

		tmpCursorMode.rollback();

		Main_OnCommand(40058, 0); // Paste items
		Main_OnCommand(40380, 0); // Set item timebase to project default (wish could copy time base from source item but too hard)

		// This would be PT style but probably better to just follow preference in preferences
		//Main_OnCommand(40630, 0); // go to start of time selection
	}

	else
		Main_OnCommand(40058, 0); // Std paste

	UpdateTimeline();

	Undo_EndBlock(SWS_CMD_SHORTNAME(t), UNDO_STATE_ALL);
}





void AWConsolidateSelection(COMMAND_T* t)
{
	Undo_BeginBlock();

	// Set time sel to sel items if no time sel
	double t1, t2;
	GetSet_LoopTimeRange(false, false, &t1, &t2, false);
	if (t1 == t2)
		Main_OnCommand(40290, 0);


	int trackOn = 1;

	// Turn trim mode off
	const ConfigVar<int> autoxfade("autoxfade");
	ConfigVarOverride<int> pTrimMode(autoxfade, *autoxfade & ~2);

	SelTracksWItems();

	SaveSelected();

	for (int iTrack = 1; iTrack <= GetNumTracks(); iTrack++)
	{
		MediaTrack* tr = CSurf_TrackFromID(iTrack, false);

		if (*(int*)GetSetMediaTrackInfo(tr, "I_SELECTED", NULL))
		{
			ClearSelected();

			GetSetMediaTrackInfo(tr, "I_SELECTED", &trackOn); // Select current track

			Main_OnCommand(40142, 0); // Insert empty item
			Main_OnCommand(40718, 0); // Select all items on track in time selection
			Main_OnCommand(40919, 0); // Set per item mix behavior to always mix
			Main_OnCommand(40362, 0); // Glue items

			RestoreSelected();

		}
	}

	RestoreSelected();

	Undo_EndBlock(SWS_CMD_SHORTNAME(t), UNDO_STATE_ALL);
}


void AWSelectStretched(COMMAND_T* t)
{

	WDL_TypedBuf<MediaItem*> items;
	SWS_GetSelectedMediaItems(&items);

	PreventUIRefresh(1);
	for (int i = 0; i < items.GetSize(); i++)
	{
		MediaItem_Take* take = GetMediaItemTake(items.Get()[i], -1);

		if (take && *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL) == 1.0)
			GetSetMediaItemInfo(items.Get()[i], "B_UISEL", &g_i0);
	}
	PreventUIRefresh(-1);

	UpdateArrange();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS, -1);

}

// Metronome actions
void AWMetrPlayOn(COMMAND_T* = NULL)        { *ConfigVar<int>("projmetroen") |= 2;}
void AWMetrPlayOff(COMMAND_T* = NULL)       { *ConfigVar<int>("projmetroen") &= ~2;}
void AWMetrRecOn(COMMAND_T* = NULL)         { *ConfigVar<int>("projmetroen") |= 4;}
void AWMetrRecOff(COMMAND_T* = NULL)        { *ConfigVar<int>("projmetroen") &= ~4;}
void AWCountPlayOn(COMMAND_T* = NULL)       { *ConfigVar<int>("projmetroen") |= 8;}
void AWCountPlayOff(COMMAND_T* = NULL)      { *ConfigVar<int>("projmetroen") &= ~8;}
void AWCountRecOn(COMMAND_T* = NULL)        { *ConfigVar<int>("projmetroen") |= 16;}
void AWCountRecOff(COMMAND_T* = NULL)       { *ConfigVar<int>("projmetroen") &= ~16;}
void AWMetrPlayToggle(COMMAND_T* = NULL)    { *ConfigVar<int>("projmetroen") ^= 2;}
void AWMetrRecToggle(COMMAND_T* = NULL)     { *ConfigVar<int>("projmetroen") ^= 4;}
void AWCountPlayToggle(COMMAND_T* = NULL)   { *ConfigVar<int>("projmetroen") ^= 8;}
void AWCountRecToggle(COMMAND_T* = NULL)    { *ConfigVar<int>("projmetroen") ^= 16;}
int IsMetrPlayOn(COMMAND_T* = NULL)     { return (*ConfigVar<int>("projmetroen") & 2)  != 0; }
int IsMetrRecOn(COMMAND_T* = NULL)          { return (*ConfigVar<int>("projmetroen") & 4)  != 0; }
int IsCountPlayOn(COMMAND_T* = NULL)        { return (*ConfigVar<int>("projmetroen") & 8)  != 0; }
int IsCountRecOn(COMMAND_T* = NULL)     { return (*ConfigVar<int>("projmetroen") & 16) != 0; }

// Editing Preferences
void AWClrTimeSelClkOn(COMMAND_T* = NULL)
{
	using namespace ItemClickMoveCurs;
	ConfigVar<int> itemclickmovecurs{"itemclickmovecurs"};
	*itemclickmovecurs |= ClearTimeOnClick | MoveOnTimeChange;
	itemclickmovecurs.save();
}

void AWClrTimeSelClkOff(COMMAND_T* = NULL)
{
	using namespace ItemClickMoveCurs;
	ConfigVar<int> itemclickmovecurs{"itemclickmovecurs"};
	*itemclickmovecurs &= ~(ClearTimeOnClick | MoveOnTimeChange);
	itemclickmovecurs.save();
}

void AWClrTimeSelClkToggle(COMMAND_T* = NULL)
{
	using namespace ItemClickMoveCurs;

	ConfigVar<int> itemclickmovecurs{"itemclickmovecurs"};
	const int both = MoveOnTimeChange | ClearTimeOnClick;
	const int values = *itemclickmovecurs & both;

	// If "click to clear" and "move cursor on time change" are both ON or both OFF, just toggle
	if (values == both || values == 0)
		*itemclickmovecurs ^= both;
	// If one of them is different than the other, turn them both ON
	else
		*itemclickmovecurs |= both;

	itemclickmovecurs.save();
}

int IsClrTimeSelClkOn(COMMAND_T* = NULL)
{
	using namespace ItemClickMoveCurs;
	return (*ConfigVar<int>("itemclickmovecurs") & (ClearTimeOnClick | MoveOnTimeChange)) != 0;
}

void AWClrLoopClkOn(COMMAND_T* = NULL)
{
	ConfigVar<int> itemclickmovecurs{"itemclickmovecurs"};
	*itemclickmovecurs |= ItemClickMoveCurs::ClearLoopOnClick;
	itemclickmovecurs.save();
}

void AWClrLoopClkOff(COMMAND_T* = NULL)
{
	ConfigVar<int> itemclickmovecurs{"itemclickmovecurs"};
	*itemclickmovecurs &= ~ItemClickMoveCurs::ClearLoopOnClick;
	itemclickmovecurs.save();
}

void AWClrLoopClkToggle(COMMAND_T* = NULL)
{
	ConfigVar<int> itemclickmovecurs{"itemclickmovecurs"};
	*itemclickmovecurs ^= ItemClickMoveCurs::ClearLoopOnClick;
	itemclickmovecurs.save();
}

int IsClrLoopClkOn(COMMAND_T* = NULL) { return (*ConfigVar<int>("itemclickmovecurs") & ItemClickMoveCurs::ClearLoopOnClick) != 0; }

void UpdateTimebaseToolbar()
{
	static int cmds[3] =
	{
		NamedCommandLookup("_SWS_AWTBASETIME"),
		NamedCommandLookup("_SWS_AWTBASEBEATPOS"),
		NamedCommandLookup("_SWS_AWTBASEBEATALL"),
	};
	for (int i = 0; i < 3; ++i)
		RefreshToolbar(cmds[i]);
}

void AWProjectTimebase(COMMAND_T* t)
{
	*ConfigVar<int>("itemtimelock") = (int)t->user;
	UpdateTimebaseToolbar();
	// ?Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_MISCCFG, -1);
}

int IsProjectTimebase(COMMAND_T* t)
{
	return (*ConfigVar<int>("itemtimelock") == (int)t->user);
}


int IsGridTriplet(COMMAND_T* = NULL)
{
	double grid = *ConfigVar<double>("projgriddiv");
	if (grid < 1e-8) return 0;
	double n = 1.0/grid;

	while (n < 3.0) { n *= 2.0; }

	double r = fmod(n, 3.0);
	return r < 0.000001 || r > 2.99999;
}

int IsGridDotted(COMMAND_T* = NULL)
{
	double grid = *ConfigVar<double>("projgriddiv");
	if (grid < 1e-8) return 0;
	double n = 1.0/grid;

	while (n < (2.0/3.0)) { n *= 2.0; }
	while (n > (4.0/3.0)) { n *= 0.5; }

	double r = fmod(n, (2.0/3.0));
	return r < 0.000001 || r > 0.66666;
}

int IsGridSwing(COMMAND_T* = NULL)
{
	return GetToggleCommandStateEx(0, 42304); // Grid: Toggle swing grid
}

int IsAWSetGridPreserveType(COMMAND_T* ct)
{
	auto grid = static_cast<double>(ct->user);
	grid = grid < 0 ? abs(grid) * 4.0 : 4.0 / grid;

	if (IsGridTriplet())
		grid *= 2.0 / 3.0;
	else if (IsGridDotted())
		grid *= 3.0 / 2.0;

	return grid == *ConfigVar<double>("projgriddiv");
}

void AWToggleDotted(COMMAND_T* = NULL);
void AWToggleSwing(COMMAND_T* = NULL);

void AWToggleTriplet(COMMAND_T* ct = NULL)
{
	if (ct) {
		if (IsGridDotted())
			AWToggleDotted();
		if (IsGridSwing())
			AWToggleSwing();
	}

	*ConfigVar<double>("projgriddiv") *= IsGridTriplet() ? 3.0/2.0 : 2.0/3.0;

	if (MIDIEditor_GetActive() && GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41022)) // Grid: Use the same grid division in arrange view and MIDI editor
	{
		if (IsGridTriplet() && !GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41004)) // Grid: Set grid type to triplet
			MIDIEditor_LastFocused_OnCommand(41004, false); // Grid: Set grid type to triplet
		else if (!IsGridTriplet() && !GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41003)) // Grid: Set grid type to straight
			MIDIEditor_LastFocused_OnCommand(41003, false); // Grid: Set grid type to straight
	}

	UpdateGridToolbar();
	UpdateTimeline();
}

void AWToggleDotted(COMMAND_T* ct)
{
	if (ct) {
		if (IsGridTriplet())
			AWToggleTriplet();
		if (IsGridSwing())
			AWToggleSwing();
	}

	*ConfigVar<double>("projgriddiv") *= IsGridDotted() ? 2.0/3.0 : 3.0/2.0;

	if (MIDIEditor_GetActive() && GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41022)) // Grid: Use the same grid division in arrange view and MIDI editor
	{
		if (IsGridDotted() && !GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41005)) // Grid: Set grid type to dotted
			MIDIEditor_LastFocused_OnCommand(41005, false); // Grid: Set grid type to dotted
		else if (!IsGridDotted() && !GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41003)) // Grid: Set grid type to straight
			MIDIEditor_LastFocused_OnCommand(41003, false); // Grid: Set grid type to straight
	}

	UpdateGridToolbar();
	UpdateTimeline();
}

void AWToggleSwing(COMMAND_T* ct)
{
	if (ct) {
		if (IsGridTriplet())
			AWToggleTriplet();
		if (IsGridDotted())
			AWToggleDotted();
	}

	// Grid: Toggle swing grid
	Main_OnCommand(42304, 0);

	if (MIDIEditor_GetActive() && GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41022)) // Grid: Use the same grid division in arrange view and MIDI editor
	{
		// BR: It seems REAPER doesn't report toggle state for swing grid so instead check all other three grid types
		if (GetToggleCommandStateEx(0, 42304)) // Grid: Toggle swing grid
		{
			if (GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41005) == 1 // Grid: Set grid type to dotted
			 || GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41004) == 1 // Grid: Set grid type to triplet
			 || GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41003) == 1 // Grid: Set grid type to straight
			)
				MIDIEditor_LastFocused_OnCommand(41006, false); // Grid: Set grid type to swing
		}
		else
		{
			if (GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41005) == 1 // Grid: Set grid type to dotted
			 || GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41004) == 1 // Grid: Set grid type to triplet
			 || GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41003) != 1 // Grid: Set grid type to straight
			)
				MIDIEditor_LastFocused_OnCommand(41003, false); // Grid: Set grid type to straight
		}
	}
	UpdateGridToolbar();
}

void AWSetGridPreserveType(COMMAND_T* ct)
{
	double normalizedGrid = static_cast<double>(abs((int)ct->user));
	if ((int)ct->user > 0)
		normalizedGrid = 1 / normalizedGrid ;

	int    swingMode = 0;
	double newGrid   = normalizedGrid;
	if (IsGridTriplet())
		newGrid = newGrid * (2.0 / 3.0);
	else if (IsGridDotted())
		newGrid = newGrid * (3.0 / 2.0);
	else if (IsGridSwing())
		swingMode = 1;
	GetSetProjectGrid(NULL, true, &newGrid, &swingMode, NULL);

	if (MIDIEditor_GetActive() && GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41022)) // Grid: Use the same grid division in arrange view and MIDI editor
	{
		// Turn off - Grid: Use the same grid division in arrange view and MIDI editor
		MIDIEditor_LastFocused_OnCommand(41022, false);

		if (normalizedGrid >= 1) // BR: For some reason, whenever setting grid type over 1 measure REAPER acts strange so first set grid as straight and then change grid type
		{
			// BR: Get grid types for later check before setting grid type to straight
			int isTriplet = IsGridTriplet();
			int isDotted  = IsGridDotted();
			int isSwing   = IsGridSwing();

			if (GetToggleCommandStateEx(SECTION_MIDI_EDITOR, 41003) != 1) // Grid: Set grid type to straight
				MIDIEditor_LastFocused_OnCommand(41003, false);           // Grid: Set grid type to straight
			SetMIDIEditorGrid(NULL, normalizedGrid);

			if (isTriplet > 0) MIDIEditor_LastFocused_OnCommand(41004, false); // Grid: Set grid type to triplet
			if (isDotted > 0) MIDIEditor_LastFocused_OnCommand(41005, false); // Grid: Set grid type to dotted
			if (isSwing > 0) MIDIEditor_LastFocused_OnCommand(41006, false); // Grid: Set grid type to swing
		}
		else
			SetMIDIEditorGrid(NULL, newGrid);

		// Turn on - Grid: Use the same grid division in arrange view and MIDI editor
		MIDIEditor_LastFocused_OnCommand(41022, false);

	}

	UpdateGridToolbar();
	UpdateTimeline();
}

void AWToggleClickTrack(COMMAND_T*)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);

		if (_stricmp("click", (char*)GetSetMediaTrackInfo(tr, "P_NAME", NULL)) == 0)
		{
			bool bMute = *(bool*)GetSetMediaTrackInfo(tr, "B_MUTE", NULL);
			bMute = !bMute;
			GetSetMediaTrackInfo(tr, "B_MUTE", &bMute);
			MetronomeOff();
			return;
		}
	}

	Main_OnCommand(40364, 0); // Toggle built in metronome
}

void UpdateGridToolbar()
{
	static const int cmds[] {
		NamedCommandLookup("_SWS_AWTOGGLETRIPLET"),
		NamedCommandLookup("_SWS_AWTOGGLEDOTTED"),
		NamedCommandLookup("_SWS_AWTOGGLESWING"),
		NamedCommandLookup("_SWS_AWTOGGLECLICKTRACK"),
		NamedCommandLookup("_SWS_SETGRID_PRESERVE_TYPE_4"),
		NamedCommandLookup("_SWS_SETGRID_PRESERVE_TYPE_2"),
		NamedCommandLookup("_SWS_SETGRID_PRESERVE_TYPE_1"),
		NamedCommandLookup("_SWS_SETGRID_PRESERVE_TYPE_1_2"),
		NamedCommandLookup("_SWS_SETGRID_PRESERVE_TYPE_1_4"),
		NamedCommandLookup("_SWS_SETGRID_PRESERVE_TYPE_1_8"),
		NamedCommandLookup("_SWS_SETGRID_PRESERVE_TYPE_1_16"),
		NamedCommandLookup("_SWS_SETGRID_PRESERVE_TYPE_1_32"),
		NamedCommandLookup("_SWS_SETGRID_PRESERVE_TYPE_1_64"),
		NamedCommandLookup("_SWS_SETGRID_PRESERVE_TYPE_1_128"),
	};
	for (const int cmd : cmds)
		RefreshToolbar(cmd);

	static const int midiCmds[] {
		NamedCommandLookup("_NF_ME_TOGGLETRIPLET"),
		NamedCommandLookup("_NF_ME_TOGGLEDOTTED"),
		NamedCommandLookup("_NF_ME_TOGGLESWING"),
	};
	for (const int midiCmd : midiCmds)
		RefreshToolbar2(SECTION_MIDI_EDITOR, midiCmd);
}

void AWInsertClickTrack(COMMAND_T* t)
{
	Undo_BeginBlock();

	//Insert track at top of track list
	InsertTrackAtIndex(0,false);
	TrackList_AdjustWindows(false);

	Main_OnCommand(40297, 0); // Unselect all tracks

	MediaTrack* clickTrack = GetTrack(0,0);

	SetMediaTrackInfo_Value(clickTrack, "I_SELECTED", 1);

	char buf[512];
	strcpy(buf,"Click");
	GetSetMediaTrackInfo(clickTrack,"P_NAME",&buf);

	Main_OnCommand(40914, 0); // Set first selected track as last touched


	Main_OnCommand(40013, 0); // Insert Click Source

	MetronomeOff();

	MediaItem* clickSource = GetTrackMediaItem(clickTrack, 0);

	SetMediaItemLength(clickSource, 600, 0);
	SetMediaItemPosition(clickSource, 0, 1);

	UpdateGridToolbar();

	Undo_EndBlock(SWS_CMD_SHORTNAME(t), UNDO_STATE_ALL);
}

int IsClickUnmuted(COMMAND_T* = NULL)
{
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		MediaTrack* tr = CSurf_TrackFromID(i, false);

		if (_stricmp("click", (char*)GetSetMediaTrackInfo(tr, "P_NAME", NULL)) == 0)
		{
			bool bMute = *(bool*)GetSetMediaTrackInfo(tr, "B_MUTE", NULL);
			return (!bMute);
		}

	}
	if (*ConfigVar<int>("projmetroen") & 1)
		return 1;
	return 0;
}



void AWRenderStem_Smart_Mono(COMMAND_T* = NULL)
{
	double timeStart, timeEnd;
	GetSet_LoopTimeRange2(0, 0, 0, &timeStart, &timeEnd, 0);

	if (timeStart != timeEnd)
	{
		Main_OnCommand(41721, 0); //Render mono stem of selected area
	}
	else
	{
		Main_OnCommand(40789, 0); //Render mono stem
	}
}

void AWRenderStem_Smart_Stereo(COMMAND_T* = NULL)
{
	double timeStart, timeEnd;
	GetSet_LoopTimeRange2(0, 0, 0, &timeStart, &timeEnd, 0);

	if (timeStart != timeEnd)
	{
		Main_OnCommand(41719, 0); //Render mono stem of selected area
	}
	else
	{
		Main_OnCommand(40788, 0); //Render mono stem
	}
}


void AWCascadeInputs(COMMAND_T* t)
{
	char returnString[128] = "1";

	if (GetUserInputs(__LOCALIZE("Cascade Selected Track Inputs","sws_mbox"),1,__LOCALIZE("Start at input:","sws_mbox"), returnString, 128))
	{
		MediaTrack* track;
		int inputOffset = atoi(returnString);
		const int cnt = CountSelectedTracks(NULL);
		for (int iTrack = 0; iTrack < cnt; iTrack++)
		{
			track = GetSelectedTrack(0, iTrack);
			SetMediaTrackInfo_Value(track, "I_RECINPUT", iTrack+inputOffset-1);
		}
	}
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_TRACKCFG, -1);

}

void AWSelectGroupIfGrouping(COMMAND_T* = NULL)
{
	const bool groupOverride = *ConfigVar<int>("projgroupover");

	if (!groupOverride)
		Main_OnCommand(40034, 0); //Select all items in groups
}

void AWSplitXFadeLeft(COMMAND_T* t)
{
	Undo_BeginBlock();

	double cursorPos = GetCursorPositionEx(0);

	// Abs because neg value means "not auto"
	const double dFadeLen = fabs(*ConfigVar<double>("defsplitxfadelen"));
	const int fadeShape = *ConfigVar<int>("defxfadeshape");

	//turn OFF autocrossfades on split
	ConfigVarOverride<int> tmpXfade("splitautoxfade", 12);

	WDL_TypedBuf<MediaItem*> items;
	SWS_GetSelectedMediaItems(&items);

	MediaItem* newItem;
	double dItemLength, dItemStart;

	int numGroups = AWCountItemGroups();
	int item1group;
	int lastGroup = -1;
	int groupAdd = 0;

	PreventUIRefresh(1);
	for (int i = 0; i < items.GetSize(); i++)
	{
		dItemLength = GetMediaItemInfo_Value(items.Get()[i], "D_LENGTH");
		dItemStart = GetMediaItemInfo_Value(items.Get()[i], "D_POSITION");
		item1group = (int)GetMediaItemInfo_Value(items.Get()[i], "I_GROUPID");

		if (cursorPos > dItemStart && cursorPos < (dItemStart + dItemLength))
		{
			newItem = SplitMediaItem(items.Get()[i], (cursorPos - dFadeLen));
			dItemLength = GetMediaItemInfo_Value(items.Get()[i], "D_LENGTH");
			SetMediaItemLength(items.Get()[i], (dItemLength+dFadeLen), 0);
			SetMediaItemInfo_Value(items.Get()[i], "D_FADEOUTLEN_AUTO", dFadeLen);
			SetMediaItemInfo_Value(items.Get()[i], "C_FADEOUTSHAPE", fadeShape);
			SetMediaItemInfo_Value(newItem, "D_FADEINLEN_AUTO", dFadeLen);
			SetMediaItemInfo_Value(newItem, "C_FADEINSHAPE", fadeShape);

			if(item1group)
			{
				if (item1group != lastGroup)
				{
					lastGroup = item1group;
					groupAdd++;
				}

				SetMediaItemInfo_Value(newItem, "I_GROUPID", numGroups + groupAdd);
			}
		}

		SetMediaItemInfo_Value(items.Get()[i], "B_UISEL", 0);
	}
	PreventUIRefresh(-1);

	UpdateArrange();
	Undo_EndBlock(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS);
}


// Track timebase actions
void AWSelTracksTimebase(COMMAND_T* t)
{
	WDL_TypedBuf<MediaTrack*> selTracks;
	SWS_GetSelectedTracks(&selTracks, false);
	for (int i = 0; i < selTracks.GetSize(); i++)
		SetMediaTrackInfo_Value(selTracks.Get()[i], "C_BEATATTACHMODE", (double)t->user);
	UpdateTrackTimebaseToolbar();

	if (selTracks.GetSize())
		Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_TRACKCFG, -1);
}

int IsSelTracksTimebase(COMMAND_T* t)
{
	WDL_TypedBuf<MediaTrack*> selTracks;
	SWS_GetSelectedTracks(&selTracks, false);

	if (!selTracks.GetSize())
		return 0;

	for (int i = 0; i < selTracks.GetSize(); i++)
		if ((int)GetMediaTrackInfo_Value(selTracks.Get()[i], "C_BEATATTACHMODE") != t->user)
			return 0;

	return 1;
}

void UpdateTrackTimebaseToolbar()
{
	static int cmds[4] =
	{
		NamedCommandLookup("_SWS_AWTRACKTBASEPROJ"),
		NamedCommandLookup("_SWS_AWTRACKTBASETIME"),
		NamedCommandLookup("_SWS_AWTRACKTBASEBEATPOS"),
		NamedCommandLookup("_SWS_AWTRACKTBASEBEATALL"),
	};
	for (int i = 0; i < 4; ++i)
		RefreshToolbar(cmds[i]);
}

class ItemTimebase {
public:
	enum {
		// C_BEATATTACHMODE
		Default     = -1,
		Time        =  0,
		PosLenRate  =  1,
		Position    =  2,

		// REAPER v6.01+: C_BEATATTACHMODE=1 C_AUTOSTRETCH=1
		AutoStretch = -2,
	};

	ItemTimebase(int timebase) : m_beatAttachMode(timebase), m_autoStretch(false)
	{
		if(m_beatAttachMode == AutoStretch) {
			m_beatAttachMode = PosLenRate;
			m_autoStretch = true;
		}
	}

	bool match(MediaItem *item) const
	{
		return GetMediaItemInfo_Value(item, "C_BEATATTACHMODE") == m_beatAttachMode &&
			(m_beatAttachMode != PosLenRate || (GetMediaItemInfo_Value(item, "C_AUTOSTRETCH") > 0) == m_autoStretch);
	}

	void apply(MediaItem *item) const
	{
		SetMediaItemInfo_Value(item, "C_BEATATTACHMODE", m_beatAttachMode);
		SetMediaItemInfo_Value(item, "C_AUTOSTRETCH", m_autoStretch);
	}

private:
	int m_beatAttachMode;
	bool m_autoStretch;
};

// Item timebase actions
void AWSelItemsTimebase(COMMAND_T* t)
{
	const ItemTimebase timebase{static_cast<int>(t->user)};

	WDL_TypedBuf<MediaItem*> selItems;
	SWS_GetSelectedMediaItems(&selItems);

	for (int i = 0; i < selItems.GetSize(); i++)
		timebase.apply(selItems.Get()[i]);

	UpdateItemTimebaseToolbar();

	if (selItems.GetSize())
		Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_TRACKCFG, -1);
}

int IsSelItemsTimebase(COMMAND_T* t)
{
	const ItemTimebase timebase{static_cast<int>(t->user)};

	WDL_TypedBuf<MediaItem*> selItems;
	SWS_GetSelectedMediaItems(&selItems);

	if (!selItems.GetSize())
		return 0;

	for (int i = 0; i < selItems.GetSize(); i++) {
		if (!timebase.match(selItems.Get()[i]))
			return 0;
	}

	return 1;
}

void UpdateItemTimebaseToolbar()
{
	static int cmds[4] =
	{
		NamedCommandLookup("_SWS_AWITEMTBASEPROJ"),
		NamedCommandLookup("_SWS_AWITEMTBASETIME"),
		NamedCommandLookup("_SWS_AWITEMTBASEBEATPOS"),
		NamedCommandLookup("_SWS_AWITEMTBASEBEATALL"),
	};
	for (int i = 0; i < 4; ++i)
		RefreshToolbar(cmds[i]);
}


void AWSelChilOrSelItems(COMMAND_T* t)
{
	MediaTrack* tr = GetLastTouchedTrack();

	PreventUIRefresh(1);
	if(GetMediaTrackInfo_Value(tr, "I_FOLDERDEPTH") == 1)
		SelChildren();
	else
	{

		//Set Cursor context to items somehow here

		for(int i = 0; i < CountTrackMediaItems(tr); i++)
		{
			MediaItem* item = GetTrackMediaItem(tr, i);
			SetMediaItemInfo_Value(item, "B_UISEL", 1);
		}
	}
	PreventUIRefresh(-1);

	UpdateArrange();
	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_ITEMS | UNDO_STATE_TRACKCFG, -1);
}

void AWSelTracksPanMode(COMMAND_T* t)
{
	WDL_TypedBuf<MediaTrack*> selTracks;
	SWS_GetSelectedTracks(&selTracks, false);

	if (!selTracks.GetSize())
		return;

	for (int i = 0; i < selTracks.GetSize(); i++)
		SetMediaTrackInfo_Value(selTracks.Get()[i], "I_PANMODE", (double)t->user);

	Undo_OnStateChangeEx(SWS_CMD_SHORTNAME(t), UNDO_STATE_TRACKCFG, -1);
	TrackList_AdjustWindows(false);
}



//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions
static COMMAND_T g_commandTable[] =
{
	// Add commands here (copy paste an example from ItemParams.cpp or similar)

	// Fill Gaps Actions
	{ { DEFACCEL, "SWS/AW: Fill gaps between selected items (advanced)" },                                              "SWS_AWFILLGAPSADV",                AWFillGapsAdv, NULL, 0 },
	{ { DEFACCEL, "SWS/AW: Fill gaps between selected items (advanced, use last settings)" },                           "SWS_AWFILLGAPSADVLASTSETTINGS",    AWFillGapsAdv, NULL, 1 },
	{ { DEFACCEL, "SWS/AW: Fill gaps between selected items (quick, no crossfade)" },                                   "SWS_AWFILLGAPSQUICK",              AWFillGapsQuick, },
	{ { DEFACCEL, "SWS/AW: Fill gaps between selected items (quick, crossfade using default fade length)" },            "SWS_AWFILLGAPSQUICKXFADE",         AWFillGapsQuickXFade, },
	{ { DEFACCEL, "SWS/AW: Remove overlaps in selected items preserving item starts" },                                 "SWS_AWFIXOVERLAPS",                AWFixOverlaps, },

	// Transport Actions
	{ { DEFACCEL, "SWS/AW: Record Conditional (normal or time selection only)" },                                                             "SWS_AWRECORDCOND",                 AWRecordConditional, },
	{ { DEFACCEL, "SWS/AW: Record Conditional (normal, time selection or item selection)" },                                                  "SWS_AWRECORDCOND2",                AWRecordConditional2, },
	{ { DEFACCEL, "SWS/AW: Toggle auto group newly recorded items" },                                                                         "SWS_AWAUTOGROUPTOG",               AWToggleAutoGroup, NULL, 0, AWIsAutoGroupEnabled},
	{ { DEFACCEL, "SWS/AW: Record (automatically group simultaneously recorded items) (deprecated)" },                                                     "SWS_AWRECORDGROUP",                AWRecordAutoGroup, },
	{ { DEFACCEL, "SWS/AW: Record Conditional (normal or time selection only, automatically group simultaneously recorded items) (deprecated)" },          "SWS_AWRECORDCONDGROUP",            AWRecordConditionalAutoGroup, },
	{ { DEFACCEL, "SWS/AW: Record Conditional (normal/time selection/item selection, automatically group simultaneously recorded items) (deprecated)" },   "SWS_AWRECORDCONDGROUP2",           AWRecordConditionalAutoGroup2, },
	{ { DEFACCEL, "SWS/AW: Play/Stop (automatically group simultaneously recorded items) (deprecated)" },                                                  "SWS_AWPLAYSTOPGRP",                AWPlayStopAutoGroup, },
	// #587
	{ { DEFACCEL, "SWS/AW/NF: Toggle assign random colors if auto group newly recorded items is enabled" },                                                           "SWS_AWNFAUTOGROUPCOLORSTOG",    AWToggleAutoGroupRndColor, NULL, 0, AWIsAutoGroupRndColorEnabled },


	// Misc Item Actions
	{ { DEFACCEL, "SWS/AW: Select from cursor to end of project (items and time selection)" },          "SWS_AWSELTOEND",                   AWSelectToEnd, },
	{ { DEFACCEL, "SWS/AW: Fade in/out/crossfade selected area of selected items" },                    "SWS_AWFADESEL",                    AWFadeSelection, },
	{ { DEFACCEL, "SWS/AW: Trim selected items to selection or cursor (crop)" },                        "SWS_AWTRIMCROP",                   AWTrimCrop, },
	{ { DEFACCEL, "SWS/AW: Trim selected items to fill selection" },                                    "SWS_AWTRIMFILL",                   AWTrimFill, },
	{ { DEFACCEL, "SWS/AW: Stretch selected items to fill selection" },                                 "SWS_AWSTRETCHFILL",                AWStretchFill, },
	{ { DEFACCEL, "SWS/AW: Consolidate Selection" },                                                    "SWS_AWCONSOLSEL",                  AWConsolidateSelection, },



	// Toggle Actions

	// Metronome Actions
	{ { DEFACCEL, "SWS/AW: Enable metronome during playback" },         "SWS_AWMPLAYON",                    AWMetrPlayOn, },
	{ { DEFACCEL, "SWS/AW: Disable metronome during playback" },        "SWS_AWMPLAYOFF",                   AWMetrPlayOff, },
	{ { DEFACCEL, "SWS/AW: Toggle metronome during playback" },         "SWS_AWMPLAYTOG",                   AWMetrPlayToggle, NULL, 0, IsMetrPlayOn },

	{ { DEFACCEL, "SWS/AW: Enable metronome during recording" },        "SWS_AWMRECON",                     AWMetrRecOn, },
	{ { DEFACCEL, "SWS/AW: Disable metronome during recording" },       "SWS_AWMRECOFF",                    AWMetrRecOff, },
	{ { DEFACCEL, "SWS/AW: Toggle metronome during recording" },        "SWS_AWMRECTOG",                    AWMetrRecToggle, NULL, 0, IsMetrRecOn },


	{ { DEFACCEL, "SWS/AW: Enable count-in before playback" },          "SWS_AWCOUNTPLAYON",                AWCountPlayOn, },
	{ { DEFACCEL, "SWS/AW: Disable count-in before playback" },         "SWS_AWCOUNTPLAYOFF",               AWCountPlayOff, },
	{ { DEFACCEL, "SWS/AW: Toggle count-in before playback" },          "SWS_AWCOUNTPLAYTOG",               AWCountPlayToggle, NULL, 0, IsCountPlayOn },


	{ { DEFACCEL, "SWS/AW: Enable count-in before recording" },         "SWS_AWCOUNTRECON",                 AWCountRecOn, },
	{ { DEFACCEL, "SWS/AW: Disable count-in before recording" },        "SWS_AWCOUNTRECOFF",                AWCountRecOff, },
	{ { DEFACCEL, "SWS/AW: Toggle count-in before recording" },         "SWS_AWCOUNTRECTOG",                AWCountRecToggle, NULL, 0, IsCountRecOn },

	{ { DEFACCEL, "SWS/AW: Enable 'link time selection and edit cursor'" },         "SWS_AWCLRTIMESELCLKON",        AWClrTimeSelClkOn, },
	{ { DEFACCEL, "SWS/AW: Disable 'link time selection and edit cursor'" },        "SWS_AWCLRTIMESELCLKOFF",       AWClrTimeSelClkOff, },
	{ { DEFACCEL, "SWS/AW: Toggle 'link time selection and edit cursor'" },         "SWS_AWCLRTIMESELCLKTOG",       AWClrTimeSelClkToggle, NULL, 0, IsClrTimeSelClkOn },

	{ { DEFACCEL, "SWS/AW: Enable clear loop points on click in ruler" },           "SWS_AWCLRLOOPCLKON",           AWClrLoopClkOn, },
	{ { DEFACCEL, "SWS/AW: Disable clear loop points on click in ruler" },          "SWS_AWCLRLOOPCLKOFF",          AWClrLoopClkOff, },
	{ { DEFACCEL, "SWS/AW: Toggle clear loop points on click in ruler" },           "SWS_AWCLRLOOPCLKTOG",          AWClrLoopClkToggle, NULL, 0, IsClrLoopClkOn },

	{ { DEFACCEL, "SWS/AW: Set project timebase to time" },											"SWS_AWTBASETIME",			AWProjectTimebase, NULL, 0, IsProjectTimebase, },
	{ { DEFACCEL, "SWS/AW: Set project timebase to beats (position only)" },						"SWS_AWTBASEBEATPOS",		AWProjectTimebase, NULL, 2, IsProjectTimebase, },
	{ { DEFACCEL, "SWS/AW: Set project timebase to beats (position, length, rate)" },				"SWS_AWTBASEBEATALL",		AWProjectTimebase, NULL, 1, IsProjectTimebase, },

	{ { DEFACCEL, "SWS/AW: Set selected tracks timebase to project default" },						"SWS_AWTRACKTBASEPROJ",		AWSelTracksTimebase, NULL, -1, IsSelTracksTimebase, },
	{ { DEFACCEL, "SWS/AW: Set selected tracks timebase to time" },									"SWS_AWTRACKTBASETIME",		AWSelTracksTimebase, NULL,  0, IsSelTracksTimebase, },
	{ { DEFACCEL, "SWS/AW: Set selected tracks timebase to beats (position only)" },				"SWS_AWTRACKTBASEBEATPOS",	AWSelTracksTimebase, NULL,  2, IsSelTracksTimebase, },
	{ { DEFACCEL, "SWS/AW: Set selected tracks timebase to beats (position, length, rate)" },		"SWS_AWTRACKTBASEBEATALL",	AWSelTracksTimebase, NULL,  1, IsSelTracksTimebase, },

	{ { DEFACCEL, "SWS/AW: Set selected items timebase to project/track default" },					"SWS_AWITEMTBASEPROJ",		AWSelItemsTimebase, NULL, ItemTimebase::Default, IsSelItemsTimebase, },
	{ { DEFACCEL, "SWS/AW: Set selected items timebase to time" },									"SWS_AWITEMTBASETIME",		AWSelItemsTimebase, NULL,  ItemTimebase::Time, IsSelItemsTimebase, },
	{ { DEFACCEL, "SWS/AW: Set selected items timebase to beats (position only)" },					"SWS_AWITEMTBASEBEATPOS",	AWSelItemsTimebase, NULL,  ItemTimebase::Position, IsSelItemsTimebase, },
	{ { DEFACCEL, "SWS/AW: Set selected items timebase to beats (position, length, rate)" },		"SWS_AWITEMTBASEBEATALL",	AWSelItemsTimebase, NULL,  ItemTimebase::PosLenRate, IsSelItemsTimebase, },

	{ { DEFACCEL, "SWS/AW: Toggle triplet grid" },          "SWS_AWTOGGLETRIPLET",              AWToggleTriplet, NULL, 0, IsGridTriplet},
	{ { DEFACCEL, "SWS/AW: Toggle dotted grid" },           "SWS_AWTOGGLEDOTTED",               AWToggleDotted, NULL, 0, IsGridDotted},
	{ { DEFACCEL, "SWS/AW: Toggle swing grid" },            "SWS_AWTOGGLESWING",                AWToggleSwing, NULL, 0, IsGridSwing},

	{ { DEFACCEL, "SWS/AW: Set grid to 4 preserving grid type" },            "SWS_SETGRID_PRESERVE_TYPE_4",                  AWSetGridPreserveType, NULL, -4, IsAWSetGridPreserveType},
	{ { DEFACCEL, "SWS/AW: Set grid to 2 preserving grid type" },            "SWS_SETGRID_PRESERVE_TYPE_2",                  AWSetGridPreserveType, NULL, -2, IsAWSetGridPreserveType},
	{ { DEFACCEL, "SWS/AW: Set grid to 1 preserving grid type" },            "SWS_SETGRID_PRESERVE_TYPE_1",                  AWSetGridPreserveType, NULL, 1, IsAWSetGridPreserveType},
	{ { DEFACCEL, "SWS/AW: Set grid to 1/2 preserving grid type" },          "SWS_SETGRID_PRESERVE_TYPE_1_2",                AWSetGridPreserveType, NULL, 2, IsAWSetGridPreserveType},
	{ { DEFACCEL, "SWS/AW: Set grid to 1/4 preserving grid type" },          "SWS_SETGRID_PRESERVE_TYPE_1_4",                AWSetGridPreserveType, NULL, 4, IsAWSetGridPreserveType},
	{ { DEFACCEL, "SWS/AW: Set grid to 1/8 preserving grid type" },          "SWS_SETGRID_PRESERVE_TYPE_1_8",                AWSetGridPreserveType, NULL, 8, IsAWSetGridPreserveType},
	{ { DEFACCEL, "SWS/AW: Set grid to 1/16 preserving grid type" },         "SWS_SETGRID_PRESERVE_TYPE_1_16",               AWSetGridPreserveType, NULL, 16, IsAWSetGridPreserveType},
	{ { DEFACCEL, "SWS/AW: Set grid to 1/32 preserving grid type" },         "SWS_SETGRID_PRESERVE_TYPE_1_32",               AWSetGridPreserveType, NULL, 32, IsAWSetGridPreserveType},
	{ { DEFACCEL, "SWS/AW: Set grid to 1/64 preserving grid type" },         "SWS_SETGRID_PRESERVE_TYPE_1_64",               AWSetGridPreserveType, NULL, 64, IsAWSetGridPreserveType},
	{ { DEFACCEL, "SWS/AW: Set grid to 1/128 preserving grid type" },        "SWS_SETGRID_PRESERVE_TYPE_1_128",              AWSetGridPreserveType, NULL, 128, IsAWSetGridPreserveType},

	{ { DEFACCEL, "SWS/AW: Paste" },        "SWS_AWPASTE",                  AWPaste, },
	{ { DEFACCEL, "SWS/AW: Remove tracks/items/env, obeying time selection and leaving children" },     "SWS_AWBUSDELETE",                  AWBusDelete, },

	{ { DEFACCEL, "SWS/AW: Insert click track" },       "SWS_AWINSERTCLICKTRK",                 AWInsertClickTrack, NULL, },
	{ { DEFACCEL, "SWS/AW: Toggle click track mute" },      "SWS_AWTOGGLECLICKTRACK",                   AWToggleClickTrack, NULL, 0, IsClickUnmuted},


	{ { DEFACCEL, "SWS/AW: Render tracks to stereo stem tracks, obeying time selection" },          "SWS_AWRENDERSTEREOSMART",      AWRenderStem_Smart_Stereo, },
	{ { DEFACCEL, "SWS/AW: Render tracks to mono stem tracks, obeying time selection" },            "SWS_AWRENDERMONOSMART",        AWRenderStem_Smart_Mono, },
	{ { DEFACCEL, "SWS/AW: Cascade selected track inputs" },                                        "SWS_AWCSCINP",     AWCascadeInputs, },
	{ { DEFACCEL, "SWS/AW: Select all items in group if grouping is enabled" },                     "SWS_AWSELGROUPIFGROUP",        AWSelectGroupIfGrouping, },
	{ { DEFACCEL, "SWS/AW: Split selected items at edit cursor w/crossfade on left" },              "SWS_AWSPLITXFADELEFT",     AWSplitXFadeLeft, },


	{ { DEFACCEL, "SWS/AW: Set selected tracks pan mode to stereo balance" },						"SWS_AWPANBALANCENEW",		AWSelTracksPanMode, NULL, 3, },
	{ { DEFACCEL, "SWS/AW: Set selected tracks pan mode to 3.x balance" },							"SWS_AWPANBALANCEOLD",		AWSelTracksPanMode, NULL, 0, },
	{ { DEFACCEL, "SWS/AW: Set selected tracks pan mode to stereo pan" },							"SWS_AWPANSTEREOPAN",		AWSelTracksPanMode, NULL, 5, },
	{ { DEFACCEL, "SWS/AW: Set selected tracks pan mode to dual pan" },								"SWS_AWPANDUALPAN",			AWSelTracksPanMode, NULL, 6, },


	{ {}, LAST_COMMAND, }, // Denote end of table
};

static COMMAND_T g_commandTable_REAPER6[] =
{
	{ { DEFACCEL, "SWS/AW: Set selected items timebase to beats (auto-stretch at tempo changes)" },		"SWS_AWITEMTBASEBEATSTRETCH",	AWSelItemsTimebase, NULL,  ItemTimebase::AutoStretch, IsSelItemsTimebase, },

	{ {}, LAST_COMMAND, },
};
//!WANT_LOCALIZE_1ST_STRING_END

int AdamInit()
{
	SWSRegisterCommands(g_commandTable);

	// legacy - v5 compatibility
	if(atof(GetAppVersion()) >= 6.01)
		SWSRegisterCommands(g_commandTable_REAPER6);

	g_AWAutoGroup = GetPrivateProfileInt(SWS_INI, "AWAutoGroup", 0, get_ini_file()) ? true : false;

	// #587
	g_AWAutoGroupRndColor = GetPrivateProfileInt(SWS_INI, "AWAutoGroupRndColor", 0, get_ini_file()) ? true : false;

	return 1;
}
