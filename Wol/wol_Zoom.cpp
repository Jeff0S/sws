/******************************************************************************
/ wol_Zoom.cpp
/
/ Copyright (c) 2014 and later wol
/ http://forum.cockos.com/member.php?u=70153
/ http://github.com/reaper-oss/sws
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
#include "wol_Zoom.h"
#include "wol_Util.h"
#include "../Breeder/BR_MouseUtil.h"
#include "../Breeder/BR_Util.h"
#include "../Breeder/BR_EnvelopeUtil.h"
#include "../SnM/SnM_Util.h"



bool g_EnvelopesExtendedZoom = false;
int g_SavedEnvelopeOverlapSettings = 0;



void AdjustSelectedEnvelopeOrTrackHeight(COMMAND_T* ct, int val, int valhw, int relmode, HWND hwnd)
{
	if (relmode > 0)
	{
		PreventUIRefresh(1);

		VerticalZoomCenter scrollCenter = static_cast<VerticalZoomCenter>((int)ct->user > 2 ? (int)ct->user - 3 : (int)ct->user);
		int height = AdjustRelative(relmode, (valhw == -1) ? BOUNDED(val, 0, 127) : (int)BOUNDED(16384.0 - (valhw | val << 7), 0.0, 16383.0));
		if (TrackEnvelope* env = GetSelectedEnvelope(NULL))
		{
			BR_Envelope brEnv(env);
			if (brEnv.IsInLane())
			{
				if (brEnv.IsTakeEnvelope())
				{
					SetTrackHeight(brEnv.GetParent(), SetToBounds(height + GetTrackHeight(brEnv.GetParent(), NULL), GetTcpTrackMinHeight(), GetCurrentTcpMaxHeight()));
					SetArrangeScrollTo(brEnv.GetParent(), scrollCenter);
				}
				else
				{
					brEnv.SetLaneHeight(SetToBounds(height + GetTrackEnvHeight(env, NULL, false), GetTcpEnvMinHeight(), GetCurrentTcpMaxHeight()));
					brEnv.Commit();
					SetArrangeScrollTo(env, false, scrollCenter);
				}
			}
			else
			{
				int trackGapTop, trackGapBottom, mul, currentHeight, laneCount, envCount;
				currentHeight = GetTrackHeight(brEnv.GetParent(), NULL, &trackGapTop, &trackGapBottom);
				GetEnvelopeOverlapState(env, &laneCount, &envCount);
				mul = g_EnvelopesExtendedZoom ? laneCount : 1;
				SetTrackHeight(brEnv.GetParent(), SetToBounds(height * mul + currentHeight, GetTcpTrackMinHeight(), (g_EnvelopesExtendedZoom ? GetCurrentTcpMaxHeight() * envCount + trackGapTop + trackGapBottom : GetCurrentTcpMaxHeight())));
				if (mul == 1)
					SetArrangeScrollTo(brEnv.GetParent(), scrollCenter);
				else
					SetArrangeScrollTo(env, false, scrollCenter);
			}
		}
		else if (MediaTrack* tr = (int)ct->user > 2 ? GetLastTouchedTrack() : NULL)
		{
			SetTrackHeight(tr, SetToBounds(height + GetTrackHeight(tr, NULL), GetTcpTrackMinHeight(), GetCurrentTcpMaxHeight()));
			SetArrangeScrollTo(tr, scrollCenter);
		}

		PreventUIRefresh(-1);
	}
}

void AdjustEnvelopeOrTrackHeightUnderMouse(COMMAND_T* ct, int val, int valhw, int relmode, HWND hwnd)
{
	if (relmode > 0)
	{
		PreventUIRefresh(1);

		int height = AdjustRelative(relmode, (valhw == -1) ? BOUNDED(val, 0, 127) : (int)BOUNDED(16384.0 - (valhw | val << 7), 0.0, 16383.0));

		TrackEnvelope* env = NULL;
		MediaTrack* tr = NULL;
		BR_MouseInfo mouseInfo(BR_MouseInfo::MODE_MCP_TCP | BR_MouseInfo::MODE_ARRANGE | BR_MouseInfo::MODE_IGNORE_ENVELOPE_LANE_SEGMENT | BR_MouseInfo::MODE_IGNORE_ALL_TRACK_LANE_ELEMENTS_BUT_ITEMS);
		if ((!strcmp(mouseInfo.GetWindow(), "tcp") && !strcmp(mouseInfo.GetSegment(), "envelope")) ||
			(!strcmp(mouseInfo.GetWindow(), "arrange") && !strcmp(mouseInfo.GetSegment(), "envelope"))
			)
		{
			if (mouseInfo.GetEnvelope())
				env = mouseInfo.GetEnvelope();
		}

		if (!env && ((!strcmp(mouseInfo.GetWindow(), "tcp") && !strcmp(mouseInfo.GetSegment(), "track")) ||
			(!strcmp(mouseInfo.GetWindow(), "arrange") && !strcmp(mouseInfo.GetSegment(), "track")))
			)
		{
			if (mouseInfo.GetTrack())
				tr = mouseInfo.GetTrack();
		}

		TrackEnvelope* sEnv = GetSelectedEnvelope(NULL);

		if (env)
		{
			BR_Envelope brEnv(env);
			if (brEnv.IsInLane())
			{
				if (brEnv.IsTakeEnvelope())
				{
					SetTrackHeight(brEnv.GetParent(), SetToBounds(height + GetTrackHeight(brEnv.GetParent(), NULL), GetTcpTrackMinHeight(), GetCurrentTcpMaxHeight()), true);
					SetArrangeScrollTo(brEnv.GetParent(), static_cast<VerticalZoomCenter>((int)ct->user));
				}
				else
				{
					brEnv.SetLaneHeight(SetToBounds(height + GetTrackEnvHeight(env, NULL, false), GetTcpEnvMinHeight(), GetCurrentTcpMaxHeight()));
					brEnv.Commit();
					SetArrangeScrollTo(env, false, static_cast<VerticalZoomCenter>((int)ct->user));
				}
			}
			else
			{
				SetTrackHeight(brEnv.GetParent(), SetToBounds(height + GetTrackHeight(brEnv.GetParent(), NULL), GetTcpTrackMinHeight(), GetCurrentTcpMaxHeight()), true);
				SetArrangeScrollTo(brEnv.GetParent(), static_cast<VerticalZoomCenter>((int)ct->user));
			}
		}
		else if (tr)
		{
			SetTrackHeight(tr, SetToBounds(height + GetTrackHeight(tr, NULL), GetTcpTrackMinHeight(), GetCurrentTcpMaxHeight()), true);
			SetArrangeScrollTo(tr, static_cast<VerticalZoomCenter>((int)ct->user));
		}

		if (sEnv != GetSelectedEnvelope(NULL))
			SetCursorContext(2, sEnv);

		PreventUIRefresh(-1);
	}
}

void SetVerticalZoomSelectedEnvelope(COMMAND_T* ct)
{
	if (TrackEnvelope* env = GetSelectedEnvelope(NULL))
	{
		BR_Envelope brEnv(env);
		if (brEnv.IsInLane())
		{
			if (brEnv.IsTakeEnvelope())
			{
				SetTrackHeight(brEnv.GetParent(), ((int)ct->user == 0) ? 0 : ((int)ct->user == 1) ? GetTcpTrackMinHeight() : GetCurrentTcpMaxHeight());
				SetArrangeScrollTo(brEnv.GetParent());
			}
			else
			{
				brEnv.SetLaneHeight(((int)ct->user == 0) ? 0 : ((int)ct->user == 1) ? GetTcpEnvMinHeight() : GetCurrentTcpMaxHeight());
				brEnv.Commit();
				SetArrangeScrollTo(env, false);
			}
		}
		else
		{
			bool ScrollToEnv = true;
			int height, trackGapTop, trackGapBottom;
			int overlapMinHeight = *ConfigVar<int>("env_ol_minh");
			int mul = CountVisibleTrackEnvelopesInTrackLane(brEnv.GetParent());
			GetTrackHeight(brEnv.GetParent(), NULL, &trackGapTop, &trackGapBottom);
			if ((int)ct->user == 2)
			{
				if (g_EnvelopesExtendedZoom)
				{
					if (overlapMinHeight >= 0)
					{
						if (GetCurrentTcpMaxHeight() > overlapMinHeight)
						{
							height = GetCurrentTcpMaxHeight()  * mul + trackGapTop + trackGapBottom;
						}
						else
						{
							height = GetCurrentTcpMaxHeight();
							ScrollToEnv = false;
						}
					}
					else
					{
						if (mul == 1)
						{
							height = GetCurrentTcpMaxHeight();
							ScrollToEnv = false;
						}
						else
						{
							height = GetCurrentTcpMaxHeight()  * mul + trackGapTop + trackGapBottom;
						}
					}
				}
				else
				{
					height = GetCurrentTcpMaxHeight();
					ScrollToEnv = false;
				}
			}
			else
			{
				height = 0;
				ScrollToEnv = false;
			}
			SetTrackHeight(brEnv.GetParent(), height);
			if (ScrollToEnv)
				SetArrangeScrollTo(env, false);
			else
				SetArrangeScrollTo(brEnv.GetParent());
		}
	}
}

void ToggleEnableEnvelopesExtendedZoom(COMMAND_T*)
{
	g_EnvelopesExtendedZoom = !g_EnvelopesExtendedZoom;
	char str[32];
	sprintf(str, "%d", g_EnvelopesExtendedZoom);
	WritePrivateProfileString(SWS_INI, "WOLExtZoomEnvInTrLane", str, get_ini_file());
}

int IsEnvelopesExtendedZoomEnabled(COMMAND_T*)
{
	return g_EnvelopesExtendedZoom;
}

void ToggleEnableEnvelopeOverlap(COMMAND_T*)
{
	ConfigVar<int> env_ol_minh("env_ol_minh");
	*env_ol_minh = -((*env_ol_minh) + 1);
	TrackList_AdjustWindows(false);
	UpdateTimeline();
}

int IsEnvelopeOverlapEnabled(COMMAND_T*)
{
	return (*ConfigVar<int>("env_ol_minh") >= 0);
}

void ForceEnvelopeOverlap(COMMAND_T* ct)
{
	ConfigVar<int> env_ol_minh("env_ol_minh");

	if ((int)ct->user == 0)
	{
		if (TrackEnvelope* env = GetSelectedTrackEnvelope(NULL))
		{
			g_SavedEnvelopeOverlapSettings = *env_ol_minh;
			bool lane; EnvVis(env, &lane);
			if ((CountVisibleTrackEnvelopesInTrackLane(GetEnvParent(env)) > 1) && !lane)
			{
				int envHeight = GetTrackEnvHeight(env, NULL, false);
				*env_ol_minh = envHeight + 1;
				TrackList_AdjustWindows(false);
				UpdateTimeline();
				RefreshToolbar(SWSGetCommandID(ToggleEnableEnvelopeOverlap));
			}
		}
	}
	else
	{
		*env_ol_minh = g_SavedEnvelopeOverlapSettings;
		TrackList_AdjustWindows(false);
		UpdateTimeline();
		RefreshToolbar(SWSGetCommandID(ToggleEnableEnvelopeOverlap));
	}
}

//---------//

void ZoomSelectedEnvelopeTimeSelection(COMMAND_T* ct)
{
	double d1, d2;
	GetSet_LoopTimeRange(false, false, &d1, &d2, false);
	TrackEnvelope* env = GetSelectedEnvelope(NULL);
	if (env && (d1 != d2))
	{
		if ((int)ct->user == 1)
			Main_OnCommand(NamedCommandLookup("_WOL_SETSELENVHMAX"), 0);
		else if (IsEnvelopeInMediaLane())
		{
			if ((int)ct->user == 2)
				Main_OnCommand(NamedCommandLookup("_WOL_VZOOMSELENVUPH"), 0);
			else if ((int)ct->user == 3)
				Main_OnCommand(NamedCommandLookup("_WOL_VZOOMSELENVLOH"), 0);
		}
		else
			return;

		Main_OnCommand(40031, 0);
	}
}

void VerticalZoomSelectedEnvelopeLoUpHalf(COMMAND_T* ct)
{
	if (TrackEnvelope* env = GetSelectedEnvelope(NULL))
	{
		VerticalZoomCenter zoomCenter = (int)ct->user == 0 ? UPPER_HALF : LOWER_HALF;
		BR_Envelope brEnv(env);
		if (brEnv.IsInLane())
		{
			//if (brEnv.IsTakeEnvelope())		// Currently not supported...
			//{
			//	return;
			//}
			//else                   // Since take envelopes are not supported and envelopes in their lane cannot be higher than TCP, return
			//{
			//	brEnv.SetLaneHeight(2 * GetCurrentTcpMaxHeight());		// Actually envelopes in their lane cannot be higher than TCP...
			//	brEnv.Commit();
			//	SetArrangeScrollTo(env, false, zoomCenter);
			//}
			return;
		}
		else
		{
			int height, trackGapTop, trackGapBottom;
			int mul = CountVisibleTrackEnvelopesInTrackLane(brEnv.GetParent());
			GetTrackHeight(brEnv.GetParent(), NULL, &trackGapTop, &trackGapBottom);

			height = 2 * GetCurrentTcpMaxHeight() * mul - 15;

			SetTrackHeight(brEnv.GetParent(), height);
			SetArrangeScrollTo(env, false, zoomCenter);
		}
	}
}

//---------//

void SetVerticalZoomCenter(COMMAND_T* ct)
{
	ConfigVar<int> vzoommode{"vzoommode"};
	*vzoommode = (int)ct->user;
	vzoommode.save();
}

void SetHorizontalZoomCenter(COMMAND_T* ct)
{
	ConfigVar<int> zoommode{"zoommode"};
	*zoommode = (int)ct->user;
	zoommode.save();
}

//---------//

static int EnvH[8];
void SaveApplyHeightSelectedEnvelopeSlot(COMMAND_T* ct)
{
	if (TrackEnvelope* env = GetSelectedEnvelope(NULL))
	{
		if ((int)ct->user < 8)
		{
			int s = (int)ct->user;
			BR_Envelope brEnv(env);
			if (brEnv.IsInLane())
				//if (brEnv.IsTakeEnvelope())
				//	EnvH[s] = GetTrackHeight(brEnv.GetParent(), 0);
				//else
				EnvH[s] = brEnv.GetLaneHeight();
			else
			{
				int trackGapTop, trackGapBottom, mul, laneCount, envCount;
				EnvH[s] = GetTrackHeight(brEnv.GetParent(), NULL, &trackGapTop, &trackGapBottom);
				EnvH[s] -= (trackGapTop + trackGapBottom);
				GetEnvelopeOverlapState(env, &laneCount, &envCount);
				mul = g_EnvelopesExtendedZoom ? laneCount : 1;
				EnvH[s] /= mul;
			}
			WDL_FastString key;
			WDL_FastString val;
			key.SetFormatted(256, "WOLEnvHSlot%d", s);
			val.SetFormatted(16, "%d", EnvH[s]);
			WritePrivateProfileString(SWS_INI, key.Get(), val.Get(), get_ini_file());
		}
		else if (((int)ct->user - 8) < 8)
		{
			int s = (int)ct->user - 8;
			if (EnvH[s] == 0)
				return;
			BR_Envelope brEnv(env);
			if (brEnv.IsInLane())
			{
				//if (brEnv.IsTakeEnvelope())
				//{
				//	SetTrackHeight(brEnv.GetParent(), EnvH[s]);
				//	ScrollTo = ScrollToTrackIfNotInArrange;
				//}
				//else
				//{
				brEnv.SetLaneHeight(EnvH[s]);
				brEnv.Commit();
				SetArrangeScrollTo(env);
				//}
			}
			else
			{
				int trackGapTop, trackGapBottom, mul, laneCount, envCount;
				GetTrackHeight(brEnv.GetParent(), NULL, &trackGapTop, &trackGapBottom);
				GetEnvelopeOverlapState(env, &laneCount, &envCount);
				mul = g_EnvelopesExtendedZoom ? laneCount : 1;
				SetTrackHeight(brEnv.GetParent(), SetToBounds(EnvH[s] * mul, GetTcpTrackMinHeight(), (g_EnvelopesExtendedZoom ? GetCurrentTcpMaxHeight() * envCount + trackGapTop + trackGapBottom : GetCurrentTcpMaxHeight())));
				if (mul == 1)
					SetArrangeScrollTo(brEnv.GetParent());
				else
					SetArrangeScrollTo(env, false);
			}
		}
	}
}

//---------//

void wol_ZoomInit()
{
	g_EnvelopesExtendedZoom = GetPrivateProfileInt(SWS_INI, "WOLExtZoomEnvInTrLane", 0, get_ini_file()) ? true : false;
	g_SavedEnvelopeOverlapSettings = *ConfigVar<int>("env_ol_minh");

	WDL_FastString key;
	for (int i = 0; i < 7; ++i)
	{
		key.SetFormatted(256, "WOLEnvHSlot%d", i);
		EnvH[i] = GetPrivateProfileInt(SWS_INI, key.Get(), 0, get_ini_file());
	}
}
