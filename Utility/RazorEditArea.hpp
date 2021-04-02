/******************************************************************************
/ RazorEditArea.hpp
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
#pragma once

/********************************************************************************
* Wrapper class for razor edit area API                                         *
* functional (detects razor edit areas) with REAPER v6.24+,                     * 
* but can be used in lower REAPER versions also (no razor edit areas detection) * 
********************************************************************************/
namespace razor
{
class RazorEditArea {
public:
	static std::vector<RazorEditArea> GetRazorEditAreas();
	// includeEnclosedItems := include items which are fully enclosed by a razor edit area
	static std::vector<MediaItem*> GetMediaItemsCrossingRazorEditAreas(bool includeEnclosedItems);

	MediaTrack* GetMediaTrack();
	double GetAreaStart();
	double GetAreaEnd();
	// empty string := razor edit area isn't an envelope
	std::string GetEnvelopeGUID(); 
private:
	RazorEditArea();
	
	MediaTrack* m_track;
	double m_areaStart;
	double m_areaEnd;
	std::string m_envelopeGUID; 
};
}
