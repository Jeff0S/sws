/******************************************************************************
/ RazorEditArea.cpp
/
/ Copyright (c) 2021 ReaTeam
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

#include "RazorEditArea.hpp"

razor::RazorEditArea::RazorEditArea() :
m_track(nullptr),
m_areaStart(-1.0),
m_areaEnd(-1.0),
m_envelopeGUID("")
{
}

std::vector<razor::RazorEditArea> razor::RazorEditArea::GetRazorEditAreas()
{
	std::vector<razor::RazorEditArea> razorEditAreas;
	if (atof(GetAppVersion()) < 6.24) // Razor edit area API was added in v6.24
		return razorEditAreas;

	char* razorEditAreasStr;
	for (int i = 1; i <= GetNumTracks(); i++)
	{
		// API returns space-separated triples of start time, end time, and envelope GUID string
		razorEditAreasStr = reinterpret_cast<char*> (GetSetMediaTrackInfo(CSurf_TrackFromID(i, false), "P_RAZOREDITS", nullptr));
		if (razorEditAreasStr && razorEditAreasStr[0])
		{
			RazorEditArea rea;
			// parse razorEditAreasStr
			char* token = strtok(razorEditAreasStr, " "); // start time
			while (token != nullptr)
			{
				rea.m_track = (CSurf_TrackFromID(i, false));
				rea.m_areaStart = (atof(token));

				token = strtok(nullptr, " "); // end time
				rea.m_areaEnd = (atof(token));

				token = strtok(nullptr, " "); // envelope GUID string
				if (token[1] == '{')
					rea.m_envelopeGUID = token;
				else
					rea.m_envelopeGUID = "";
				
				razorEditAreas.push_back(rea);
				token = strtok(nullptr, " "); // next triple
			}
		}
	}
	return razorEditAreas;
}

std::vector<MediaItem*> razor::RazorEditArea::GetMediaItemsCrossingRazorEditAreas(bool includeEnclosedItems)
{
	std::vector<MediaItem*> itemsCrossingRazorEditAreas;

	std::vector<razor::RazorEditArea> reas = razor::RazorEditArea::GetRazorEditAreas();
	for (size_t i = 0; i < reas.size(); i++)
	{
		if (reas[i].m_envelopeGUID == "") // only check razor edit areas which aren't envelopes
		{
			MediaTrack* track = reas[i].m_track;
			int itemCount = CountTrackMediaItems(track);
			for (int j = 0; j < itemCount; j++)
			{
				MediaItem* item = GetTrackMediaItem(track, j);
				int itemPos = GetMediaItemInfo_Value(item, "D_POSITION");
				int itemEnd = itemPos + GetMediaItemInfo_Value(item, "D_LENGTH");

				// check if item is in razor edit area bounds
				if (includeEnclosedItems == false && reas[i].m_areaStart <= itemPos && reas[i].m_areaEnd >= itemEnd)// rea encloses item
					continue;
				if ((itemEnd > reas[i].m_areaStart && itemEnd <= reas[i].m_areaEnd) // item end within rea
					|| (itemPos >= reas[i].m_areaStart && itemPos < reas[i].m_areaEnd) // item start within rea
					|| (itemPos <= reas[i].m_areaStart && itemEnd >= reas[i].m_areaEnd)) // item encloses rae
						itemsCrossingRazorEditAreas.push_back(item);
			}
		}
	}
	return itemsCrossingRazorEditAreas;
}

MediaTrack* razor::RazorEditArea::GetMediaTrack()
{
	return m_track;
}

double razor::RazorEditArea::GetAreaStart()
{
	return m_areaStart;
}

double razor::RazorEditArea::GetAreaEnd()
{
	return m_areaEnd;
}

std::string razor::RazorEditArea::GetEnvelopeGUID()
{
	return m_envelopeGUID;
}
