/******************************************************************************
/ BR_Loudness.cpp
/
/ Copyright (c) 2014-2015 Dominik Martin Drzic
/ http://forum.cockos.com/member.php?u=27094
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
#include "BR_Loudness.h"
#include "BR_EnvelopeUtil.h"
#include "BR_Util.h"
#include "../SnM/SnM_Dlg.h"
#include "../SnM/SnM_Util.h"
#include "../SnM/SnM.h"
#include "../libebur128/ebur128.h"

#include <WDL/localize/localize.h>

/******************************************************************************
* Constants                                                                   *
******************************************************************************/
const char* const PROJ_SETTINGS_KEY          = "<BR_LOUDNESS";
const char* const PROJ_SETTINGS_KEY_LU       = "LU";
const char* const PROJ_SETTINGS_KEY_GRAPH    = "GRAPH";

const char* const PROJ_OBJECT_KEY              = "<BR_ANALYZED_LOUDNESS_OBJECT";
const char* const PROJ_OBJECT_KEY_TARGET       = "TARGET";
const char* const PROJ_OBJECT_KEY_MEASUREMENTS = "MEASUREMENTS";
const char* const PROJ_OBJECT_KEY_STATUS       = "STATUS";
const char* const PROJ_OBJECT_KEY_SHORT_TERM   = "PT_SHORT_TERM";
const char* const PROJ_OBJECT_KEY_MOMENTARY    = "PT_MOMENTARY";

const char* const LOUDNESS_KEY         = "BR - AnalyzeLoudness";
const char* const LOUDNESS_WND         = "BR - AnalyzeLoudness WndPos" ;
const char* const LOUDNESS_VIEW_WND    = "BR - AnalyzeLoudnessView WndPos";
const char* const NORMALIZE_KEY        = "BR - NormalizeLoudness";
const char* const NORMALIZE_WND        = "BR - NormalizeLoudness WndPos";
const char* const PREF_KEY             = "BR - LoudnessPref";
const char* const PREF_WND             = "BR - LoudnessPref WndPos";
const char* const EXPORT_FORMAT_KEY    = "BR - LoudnessExportFormat";
const char* const EXPORT_FORMAT_WND    = "BR - LoudnessExportFormat WndPos";
const char* const EXPORT_FORMAT_RECENT = "BR - LoudnessExportFormat_Pattern_";

const int EXPORT_FORMAT_RECENT_MAX      = 10;
const int VERSION                       = 1;

// Export format wildcards
static const struct
{
	const char* wildcard;
	int osxTabs;
	const char* desc;
} g_wildcards[] = {
	// !WANT_LOCALIZE_STRINGS_BEGIN:sws_DLG_180
	{"$id",                  5, "list view id in analyze loudness dialog"},
	{"$integrated",          3, "integrated loudness"},
	{"$range",               4, "loudness range"},
	{"$truepeak",            4, "true peak"},
	{"$maxshort",            4, "maximum short-term loudness"},
	{"$maxmomentary",        3, "maximum momentary loudness"},
	{"$target",              4, "in case the track is analyzed, set track name, otherwise analyzed item name"},
	{"$item",                5, "name of the analyzed item"},
	{"$track",               4, "name of the analyzed track or parent track of the analyzed item"},
	{"$targetnumber",        3, "in case the track is analyzed, set track number, otherwise media item number in track"},
	{"$itemnumber",          3, "media item number in track - 1 for the first item on a track, 2 for the second..."},
	{"$tracknumber",         3, "track number - 1 for the first track, 2 for the second..."},
	{"$start",               4, "project start time position of the analyzed audio"},
	{"$end",                 5, "project end time position of the analyzed audio"},
	{"$length",              4, "time length of the analyzed audio"},
	{"$truepeakpos",         3, "true peak time position (counting from audio start)"},
	{"$maxshortpos",         3, "time position of maximum short-term loudness (counting from audio start)"},
	{"$maxmomentarypos",     2, "time position of maximum momentary loudness (counting from audio start)"},
	{"$truepeakposproj",     2, "true peak project time position"},
	{"$maxshortposproj",     2, "project time position of maximum short-term loudness"},
	{"$maxmomentaryposproj", 1, "project time position of maximum momentary loudness"},
	{"$lureference",         3, "0 LU reference value set in global preferences"},
	{"$luformat",            4, "LU unit format set in global preferences"},
	{"$t",                   5, "inserts tab character (displayed as normal space in preview)"},
	{"$n",                   5, "inserts newline (displayed as normal space in preview)"},
	{"$$",                   5, "inserts $ character (normal $ works too, this is to prevent wildcard from being treated as wildcard)"},
	{NULL,                   0, NULL},
	// !WANT_LOCALIZE_STRINGS_END
};

// !WANT_LOCALIZE_STRINGS_BEGIN:sws_DLG_174
static SWS_LVColumn g_cols[] =
{
	{25,  0, "#"},
	{110, 0, "Track"},
	{110, 0, "Item" },
	{70,  0, "Integrated" },
	{70,  0, "Range" },
	{70,  0, "True peak" },
	{115, 0, "Maximum short-term" },
	{115, 0, "Maximum momentary" },
};
// !WANT_LOCALIZE_STRINGS_END

enum
{
	COL_ID = 0,
	COL_TRACK,
	COL_TAKE,
	COL_INTEGRATED,
	COL_RANGE,
	COL_TRUEPEAK,
	COL_SHORTTERM,
	COL_MOMENTARY,
	COL_COUNT
};

// Normalize loudness window messages
const int UPDATE_FROM_PROJDATA = 0xF001;

// Analyze loudness window messages and timers
const int REANALYZE_ITEMS             = 0xF001;
const int NORMALIZE                   = 0xF002;
const int NORMALIZE_TO_23             = 0xF003;
const int NORMALIZE_TO_0LU            = 0xF004;
const int DRAW_SHORTTERM              = 0xF005;
const int DRAW_MOMENTARY              = 0xF006;
const int DELETE_ITEM                 = 0xF007;
const int SET_ANALYZE_TARGET_ITEM     = 0xF008;
const int SET_ANALYZE_TARGET_TRACK    = 0xF009;
const int SET_ANALYZE_ON_NORMALIZE    = 0xF00A;
const int SET_MIRROR_SELECTION        = 0xF00B;
const int SET_DOUBLECLICK_GOTO_TARGET = 0xF00C;
const int SET_TIMESEL_OVER_MAX        = 0xF00D;
const int SET_CLEAR_ENVELOPE          = 0xF00E;
const int SET_CLEAR_ON_ANALYZE        = 0xF00F;
const int SET_DO_TRUE_PEAK            = 0xF010;
const int SET_UNIT_LUFS               = 0xF011;
const int SET_UNIT_LU                 = 0xF012;
const int OPEN_GLOBAL_PREFERENCES     = 0xF013;
const int OPEN_EXPORT_FORMAT          = 0xF014;
const int OPEN_WIKI_HELP              = 0xF015;
const int EXPORT_TO_CLIPBOARD         = 0xF016;
const int EXPORT_TO_FILE              = 0xF017;
const int GO_TO_SHORTTERM             = 0xF018;
const int GO_TO_MOMENTARY             = 0xF019;
const int GO_TO_TRUE_PEAK             = 0xF01A;
const int SET_DO_HIGH_PRECISION_MODE  = 0xF01B;
const int SET_DO_DUAL_MONO_MODE       = 0xF01C;

const int ANALYZE_TIMER     = 1;
const int REANALYZE_TIMER   = 2;
const int UPDATE_TIMER      = 3;
const int ANALYZE_TIMER_FREQ = 50;
const int UPDATE_TIMER_FREQ  = 200;

/******************************************************************************
* Macros                                                                      *
******************************************************************************/
#define g_pref BR_LoudnessPref::Get() // not exactly global object, but it behaves as such, heh...

/******************************************************************************
* Globals                                                                     *
******************************************************************************/
SNM_WindowManager<BR_AnalyzeLoudnessWnd>                              g_loudnessWndManager(LOUDNESS_WND);
static SWSProjConfig<WDL_PtrList_DeleteOnDestroy<BR_LoudnessObject> > g_analyzedObjects; // no WDL_PtrList_DOD here (abort analysis)
static HWND                                                           g_normalizeWnd = NULL;

/******************************************************************************
* Loudness object                                                             *
******************************************************************************/
BR_LoudnessObject::BR_LoudnessObject () :
m_track               (NULL),
m_take                (NULL),
m_guid                (GUID_NULL),
m_integrated          (NEGATIVE_INF),
m_truePeak            (NEGATIVE_INF),
m_truePeakPos         (-1),
m_shortTermMax        (NEGATIVE_INF),
m_momentaryMax        (NEGATIVE_INF),
m_range               (0),
m_progress            (0),
m_running             (false),
m_analyzed            (false),
m_killFlag            (false),
m_integratedOnly      (false),
m_doTruePeak          (true),
m_truePeakAnalyzed    (false),
m_process             (NULL),
m_doHighPrecisionMode (true),
m_doDualMonoMode      (true)
{
}

BR_LoudnessObject::BR_LoudnessObject (MediaTrack* track) :
m_track               (track),
m_take                (NULL),
m_guid                (*(GUID*)GetSetMediaTrackInfo(track, "GUID", NULL)),
m_integrated          (NEGATIVE_INF),
m_truePeak            (NEGATIVE_INF),
m_truePeakPos         (-1),
m_shortTermMax        (NEGATIVE_INF),
m_momentaryMax        (NEGATIVE_INF),
m_range               (0),
m_progress            (0),
m_running             (false),
m_analyzed            (false),
m_killFlag            (false),
m_integratedOnly      (false),
m_doTruePeak          (true),
m_truePeakAnalyzed    (false),
m_process             (NULL),
m_doHighPrecisionMode (true),
m_doDualMonoMode      (true)
{
	this->CheckSetAudioData();
}

BR_LoudnessObject::BR_LoudnessObject (MediaItem_Take* take) :
m_track               (NULL),
m_take                (take),
m_guid                (*(GUID*)GetSetMediaItemTakeInfo(take, "GUID", NULL)),
m_integrated          (NEGATIVE_INF),
m_truePeak            (NEGATIVE_INF),
m_truePeakPos         (-1),
m_shortTermMax        (NEGATIVE_INF),
m_momentaryMax        (NEGATIVE_INF),
m_range               (0),
m_progress            (0),
m_running             (false),
m_analyzed            (false),
m_killFlag            (false),
m_integratedOnly      (false),
m_doTruePeak          (true),
m_truePeakAnalyzed    (false),
m_process             (NULL),
m_doHighPrecisionMode (true),
m_doDualMonoMode      (true)
{
	this->CheckSetAudioData();
}

BR_LoudnessObject::~BR_LoudnessObject ()
{
	this->AbortAnalyze();
	if (EnumProjects(0, NULL, 0)) // prevent destroying accessor on reaper exit (otherwise we get access violation)
		DestroyAudioAccessor(this->GetAudioData().audio);
}

bool BR_LoudnessObject::Analyze (bool integratedOnly, bool doTruePeak, bool doHighPrecisionMode, bool doDualMonoMode)
{
	this->AbortAnalyze();
	this->SetIntegratedOnly(integratedOnly);
	this->SetDoTruePeak(doTruePeak);
	this->SetDoHighPrecisionMode(doHighPrecisionMode);
	this->SetDoDualMonoMode(doDualMonoMode);

	// Is audio data still valid?
	if (this->CheckSetAudioData())
	{
		bool analyzed = this->GetAnalyzedStatus();
		if (analyzed && doTruePeak && !this->GetTruePeakAnalyzeStatus())
			analyzed = false;

		if (!analyzed)
		{
			this->SetRunning(true);
			this->SetProgress(0);
			this->SetProcess((HANDLE)_beginthreadex(NULL, 0, this->AnalyzeData, (void*)this, 0, NULL));
		}
		return true;
	}

	// Audio isn't valid and can't be retrieved (related track/take is no more)
	else
	{
		this->SetRunning(false);
		this->SetProgress(0);
		return false;
	}
}

void BR_LoudnessObject::AbortAnalyze ()
{
	if (this->GetProcess())
	{
		this->SetKillFlag(true);
		WaitForSingleObject(this->GetProcess(), INFINITE);
		this->SetKillFlag(false);
		CloseHandle(this->GetProcess());

		this->SetProcess(NULL);
		this->SetRunning(false);
		this->SetProgress(0);
	}
}

bool BR_LoudnessObject::IsRunning ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_running;
}

double BR_LoudnessObject::GetProgress ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_progress;
}

double BR_LoudnessObject::GetColumnVal (int column, int mode)
{
	SWS_SectionLock lock(&m_mutex);

	double returnVal = 0;
	switch (column)
	{
		case COL_INTEGRATED:
			this->GetAnalyzeData(&returnVal, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		break;

		case COL_RANGE:
			this->GetAnalyzeData(NULL, &returnVal, NULL, NULL, NULL, NULL, NULL, NULL);
		break;

		case COL_TRUEPEAK:
			this->GetAnalyzeData(NULL, NULL, &returnVal, NULL, NULL, NULL, NULL, NULL);
		break;

		case COL_SHORTTERM:
			this->GetAnalyzeData(NULL, NULL, NULL, NULL, &returnVal, NULL, NULL, NULL);
		break;

		case COL_MOMENTARY:
			this->GetAnalyzeData(NULL, NULL, NULL, NULL, NULL, &returnVal, NULL, NULL);
		break;
	}

	if (mode == 1 && column != COL_RANGE && column != COL_TRUEPEAK)
		returnVal = g_pref.LUFStoLU(returnVal);

	return RoundToN(returnVal, 1);
}

void BR_LoudnessObject::GetColumnStr (int column, char* str, int strSz, int mode)
{
	SWS_SectionLock lock(&m_mutex);

	switch (column)
	{
		case COL_ID:
		{
			snprintf(str, strSz, "%d", g_analyzedObjects.Get()->Find(this) + 1);
		}
		break;

		case COL_TRACK:
		{
			snprintf(str, strSz, "%s", (this->GetTrackName()).Get());
		}
		break;

		case COL_TAKE:
		{
			snprintf(str, strSz, "%s", (this->GetTakeName()).Get());
		}
		break;

		case COL_RANGE:
		{
			double range;
			this->GetAnalyzeData(NULL, &range, NULL, NULL, NULL, NULL, NULL, NULL);

			if (range <= NEGATIVE_INF)
				snprintf(str, strSz, "%s", __localizeFunc("-inf", "vol", 0));
			else
				snprintf(str, strSz, "%.1lf %s", RoundToN(range, 1) , __LOCALIZE("LU", "sws_loudness"));
		}
		break;

		case COL_TRUEPEAK:
		{
			double truePeak;
			this->GetAnalyzeData(NULL, NULL, &truePeak, NULL, NULL, NULL, NULL, NULL);

			if (truePeak <= NEGATIVE_INF)
				snprintf(str, strSz, "%s", __localizeFunc("-inf", "vol", 0));
			else
				snprintf(str, strSz, "%.1lf %s", RoundToN(truePeak, 1), __LOCALIZE("dBTP", "sws_loudness"));
		}
		break;

		case COL_INTEGRATED:
		case COL_SHORTTERM:
		case COL_MOMENTARY:
		{
			double value;
			if      (column == COL_INTEGRATED) this->GetAnalyzeData(&value, NULL, NULL, NULL, NULL,   NULL,   NULL, NULL);
			else if (column == COL_SHORTTERM)  this->GetAnalyzeData(NULL,   NULL, NULL, NULL, &value, NULL,   NULL, NULL);
			else                               this->GetAnalyzeData(NULL,   NULL, NULL, NULL, NULL,   &value, NULL, NULL);

			WDL_FastString unitLU = g_pref.GetFormatedLUString();
			const char* unit = (mode == 1) ? unitLU.Get() : __LOCALIZE("LUFS", "sws_loudness");

			if (value <= NEGATIVE_INF)
				snprintf(str, strSz, "%s", __localizeFunc("-inf", "vol", 0));
			else
				snprintf(str, strSz, "%.1lf %s", RoundToN((mode == 1) ? g_pref.LUFStoLU(value) : value, 1), unit);
		}
		break;
	}
}

void BR_LoudnessObject::SaveObject (ProjectStateContext* ctx)
{
	if (this->CheckSetAudioData())
	{
		GUID guid = this->GetGuid();
		char tmp[128]; guidToString(&guid, tmp);

		double integrated, range, truePeak, truePeakPos, shortTermMax, momentaryMax;
		vector<double> shortTermValues, momentaryValues;
		this->GetAnalyzeData(&integrated, &range, &truePeak, &truePeakPos, &shortTermMax, &momentaryMax, &shortTermValues, &momentaryValues);

		ctx->AddLine(PROJ_OBJECT_KEY);
		ctx->AddLine("%s %d %s", PROJ_OBJECT_KEY_TARGET, (this->GetTrack() ? 1 : 0), tmp);
		ctx->AddLine("%s %lf %lf %lf %lf %lf %lf", PROJ_OBJECT_KEY_MEASUREMENTS, integrated, range, truePeak, truePeakPos, shortTermMax, momentaryMax);
		ctx->AddLine("%s %d %d %d %d %d %d %d", PROJ_OBJECT_KEY_STATUS, this->GetDoTruePeak(), this->GetTruePeakAnalyzeStatus(), this->GetAnalyzedStatus(), this->GetIntegratedOnly(), VERSION, this->GetDoHighPrecisionMode(), this->GetDoDualMonoMode());

		int count = 0;
		WDL_FastString string;

		for (size_t i = 0; i < shortTermValues.size(); ++i)
		{
			string.AppendFormatted(512, " %lf", shortTermValues[i]);
			++count;

			if (count == 10 || i == shortTermValues.size() - 1)
			{
				ctx->AddLine("%s %s", PROJ_OBJECT_KEY_SHORT_TERM, string.Get());
				string.DeleteSub(0, string.GetLength());
				count = 0;
			}
		}

		count = 0;
		string.DeleteSub(0, string.GetLength());
		for (size_t i = 0; i < momentaryValues.size(); ++i)
		{
			string.AppendFormatted(512, " %lf", momentaryValues[i]);
			++count;

			if (count == 10 || i == momentaryValues.size() - 1)
			{
				ctx->AddLine("%s %s", PROJ_OBJECT_KEY_MOMENTARY, string.Get());
				string.DeleteSub(0, string.GetLength());
				count = 0;
			}
		}
		ctx->AddLine(">");
	}
}

bool BR_LoudnessObject::RestoreObject (ProjectStateContext* ctx)
{
	bool doTruePeak = false, truePeakAnalyzed = false, analyzed = false, integratedOnly = false, doHighPrecision = false, doDualMono = false;
	int version = VERSION;

	double integrated   = NEGATIVE_INF;
	double range        = 0;
	double truePeak     = NEGATIVE_INF;
	double truePeakPos  = NEGATIVE_INF;
	double shortTermMax = NEGATIVE_INF;
	double momentaryMax = NEGATIVE_INF;
	vector<double> shortTermValues;
	vector<double> momentaryValues;

	char line[256];
	LineParser lp(false);
	while(!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
	{
		if (!strcmp(lp.gettoken_str(0), ">"))
			break;

		if (!strcmp(lp.gettoken_str(0), PROJ_OBJECT_KEY_TARGET))
		{
			GUID guid; stringToGuid(lp.gettoken_str(2), &guid);

			this->SetGuid(guid);
			if (lp.gettoken_int(1) == 1) this->SetTrack(GuidToTrack(&guid));
			else                         this->SetTake(GetMediaItemTakeByGUID(NULL, &guid));
		}
		else if (!strcmp(lp.gettoken_str(0), PROJ_OBJECT_KEY_MEASUREMENTS))
		{
			integrated   = (lp.getnumtokens() > 1)  ? lp.gettoken_float(1) : NEGATIVE_INF;
			range        = (lp.getnumtokens() > 2)  ? lp.gettoken_float(2) : 0;
			truePeak     = (lp.getnumtokens() > 3)  ? lp.gettoken_float(3) : NEGATIVE_INF;
			truePeakPos  = (lp.getnumtokens() > 4)  ? lp.gettoken_float(4) : NEGATIVE_INF;
			shortTermMax = (lp.getnumtokens() > 5)  ? lp.gettoken_float(5) : NEGATIVE_INF;
			momentaryMax = (lp.getnumtokens() > 6)  ? lp.gettoken_float(6) : NEGATIVE_INF;
		}
		else if (!strcmp(lp.gettoken_str(0), PROJ_OBJECT_KEY_STATUS))
		{
			doTruePeak       = !!lp.gettoken_int(1);
			truePeakAnalyzed = !!lp.gettoken_int(2);
			analyzed         = !!lp.gettoken_int(3);
			integratedOnly   = !!lp.gettoken_int(4);
			version          = lp.gettoken_int(5);
			doHighPrecision  = !!lp.gettoken_int(6);
			doDualMono       = !!lp.gettoken_int(7);
		}
		else if (!strcmp(lp.gettoken_str(0), PROJ_OBJECT_KEY_SHORT_TERM))
		{
			if (lp.getnumtokens() > 1)  shortTermValues.push_back(lp.gettoken_float(1));
			if (lp.getnumtokens() > 2)  shortTermValues.push_back(lp.gettoken_float(2));
			if (lp.getnumtokens() > 3)  shortTermValues.push_back(lp.gettoken_float(3));
			if (lp.getnumtokens() > 4)  shortTermValues.push_back(lp.gettoken_float(4));
			if (lp.getnumtokens() > 5)  shortTermValues.push_back(lp.gettoken_float(5));
			if (lp.getnumtokens() > 6)  shortTermValues.push_back(lp.gettoken_float(6));
			if (lp.getnumtokens() > 7)  shortTermValues.push_back(lp.gettoken_float(7));
			if (lp.getnumtokens() > 8)  shortTermValues.push_back(lp.gettoken_float(8));
			if (lp.getnumtokens() > 9)  shortTermValues.push_back(lp.gettoken_float(9));
			if (lp.getnumtokens() > 10) shortTermValues.push_back(lp.gettoken_float(10));
		}
		else if (!strcmp(lp.gettoken_str(0), PROJ_OBJECT_KEY_MOMENTARY))
		{
			if (lp.getnumtokens() > 1)  momentaryValues.push_back(lp.gettoken_float(1));
			if (lp.getnumtokens() > 2)  momentaryValues.push_back(lp.gettoken_float(2));
			if (lp.getnumtokens() > 3)  momentaryValues.push_back(lp.gettoken_float(3));
			if (lp.getnumtokens() > 4)  momentaryValues.push_back(lp.gettoken_float(4));
			if (lp.getnumtokens() > 5)  momentaryValues.push_back(lp.gettoken_float(5));
			if (lp.getnumtokens() > 6)  momentaryValues.push_back(lp.gettoken_float(6));
			if (lp.getnumtokens() > 7)  momentaryValues.push_back(lp.gettoken_float(7));
			if (lp.getnumtokens() > 8)  momentaryValues.push_back(lp.gettoken_float(8));
			if (lp.getnumtokens() > 9)  momentaryValues.push_back(lp.gettoken_float(9));
			if (lp.getnumtokens() > 10) momentaryValues.push_back(lp.gettoken_float(10));
		}
	}
	this->SetAnalyzeData(integrated, range, truePeak, truePeakPos, shortTermMax, momentaryMax, shortTermValues, momentaryValues);

	if (this->IsTargetValid())
	{
		this->CheckSetAudioData(); // call this before setting analyze status data (the function changes it)

		this->SetDoTruePeak(doTruePeak);
		this->SetTruePeakAnalyzed(truePeakAnalyzed);
		this->SetAnalyzedStatus(((version == VERSION) ? analyzed : false));
		this->SetIntegratedOnly(integratedOnly);
		this->SetDoHighPrecisionMode(doHighPrecision);
		this->SetDoDualMonoMode(doDualMono);

		return true;
	}
	else
	{
		return false;
	}
}

double BR_LoudnessObject::GetAudioLength ()
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return -1;

	return this->GetAudioData().audioEnd - this->GetAudioData().audioStart;
}

double BR_LoudnessObject::GetAudioStart ()
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return -1;

	double start = this->GetAudioData().audioStart;
	if (this->GetTrack())
		return start;
	else
		return GetMediaItemInfo_Value(this->GetItem(), "D_POSITION") + start;
}

double BR_LoudnessObject::GetAudioEnd ()
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return -1;

	double end = this->GetAudioData().audioEnd;
	if (this->GetTrack())
		return end;
	else
		return GetMediaItemInfo_Value(this->GetItem(), "D_POSITION") + end;
}

double BR_LoudnessObject::GetMaxMomentaryPos (bool projectTime)
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return -1;

	double position = 0;
	if (projectTime)
		position = (this->GetTrack()) ? (this->GetAudioData().audioStart) : (GetMediaItemInfo_Value(this->GetItem(), "D_POSITION"));

	vector<double> momentaryValues;
	double momentaryMax;
	this->GetAnalyzeData(NULL, NULL, NULL, NULL, NULL, &momentaryMax, NULL, &momentaryValues);
	for (size_t i = 0; i < momentaryValues.size(); ++i)
	{
		if (momentaryValues[i] == momentaryMax)
			break;
		else
			position += 0.4;
	}

	return position;
}

double BR_LoudnessObject::GetMaxShorttermPos (bool projectTime)
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return -1;

	double position = 0;
	if (projectTime)
		position = (this->GetTrack()) ? (this->GetAudioData().audioStart) : (GetMediaItemInfo_Value(this->GetItem(), "D_POSITION"));

	vector<double> shortTermValues;
	double shortTermMax;
	this->GetAnalyzeData(NULL, NULL, NULL, NULL, &shortTermMax, NULL, &shortTermValues, NULL);
	for (size_t i = 0; i < shortTermValues.size(); ++i)
	{
		if (shortTermValues[i] == shortTermMax)
			break;
		else
			position += 3;
	}

	return position;
}

double BR_LoudnessObject::GetTruePeakPos (bool projectTime)
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return -1;

	double position;
	this->GetAnalyzeData(NULL, NULL, NULL, &position, NULL, NULL, NULL, NULL);

	if (position >= 0)
	{
		if (projectTime)
			position += (this->GetTrack()) ? (this->GetAudioData().audioStart) : (GetMediaItemInfo_Value(this->GetItem(), "D_POSITION"));
	}
	else
		position = -1;

	return position;
}

bool BR_LoudnessObject::IsTrack ()
{
	SWS_SectionLock lock(&m_mutex);
	if (this->GetTrack()) return true;
	else                  return false;
}

bool BR_LoudnessObject::IsTargetValid ()
{
	SWS_SectionLock lock(&m_mutex);

	GUID guid = this->GetGuid();

	if (this->GetTrack())
	{
		if (ValidatePtr(this->GetTrack(), "MediaTrack*") && GuidsEqual(&guid, (GUID*)GetSetMediaTrackInfo(this->GetTrack(), "GUID", NULL)))
			return true;
		else
		{
			// Deleting and undoing track will naturally change item's pointer so find new one if possible
			if (MediaTrack* newTrack = GuidToTrack(&guid))
			{
				if (GuidsEqual(&guid, (GUID*)GetSetMediaTrackInfo(newTrack, "GUID", NULL)))
					this->SetTrack(newTrack);
				return true;
			}
		}
	}
	else
	{	
		if (ValidatePtr(this->GetTake(), "MediaItem_Take*") && GuidsEqual(&guid, (GUID*)GetSetMediaItemTakeInfo(this->GetTake(), "GUID", NULL)))
		{
			// NF: prevent items going offline during analysis
			// fix for when "Prefs->Set media items offline when app. is inactive" is enabled
			// and user clicks outside REAPER during analysis, resulting in items going offline and giving wrong calculation results
			PCM_source* source = GetMediaItemTake_Source(this->GetTake());
			if (!source->IsAvailable())
				source->SetAvailable(true);

			return true;
		}
			
		else
		{
			if (MediaItem_Take* newTake = GetMediaItemTakeByGUID(NULL, &guid))
			{
				if (GuidsEqual(&guid, (GUID*)GetSetMediaItemTakeInfo(newTake, "GUID", NULL)))
				{
					this->SetTake(newTake);

					PCM_source* source = GetMediaItemTake_Source(this->GetTake());
					if (!source->IsAvailable())
						source->SetAvailable(true);
				}
				return true;
			}
		}
	}

	return false;
}

bool BR_LoudnessObject::CheckTarget (MediaTrack* track)
{
	SWS_SectionLock lock(&m_mutex);
	if (this->IsTargetValid() && this->GetTrack() && this->GetTrack() == track)
		return true;
	else
		return false;
}

bool BR_LoudnessObject::CheckTarget (MediaItem_Take* take)
{
	SWS_SectionLock lock(&m_mutex);
	if (this->IsTargetValid() && this->GetTake() && this->GetTake() == take)
		return true;
	else
		return false;
}

bool BR_LoudnessObject::IsSelectedInProject ()
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return false;

	if (this->GetTrack())
		return !!(*(int*)GetSetMediaTrackInfo(this->GetTrack(), "I_SELECTED", NULL));
	else
		return *(bool*)GetSetMediaItemInfo(this->GetItem(), "B_UISEL", NULL);
}

bool BR_LoudnessObject::CreateGraph (BR_Envelope& envelope, double minLUFS, double maxLUFS, bool momentary, bool highPrecisionMode, HWND warningHwnd /*=g_hwndParent*/)
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid() || envelope.IsTempo())
	{
		if (envelope.IsTempo())
			MessageBox(warningHwnd, __LOCALIZE("Can't create loudness graph in tempo map.","sws_mbox"), __LOCALIZE("SWS/BR - Error","sws_mbox"), 0);
		return false;
	}

	if (highPrecisionMode)
	{
		MessageBox(warningHwnd, __LOCALIZE("Creating graph in high precision mode\nis currently not implemented.", "sws_mbox"), __LOCALIZE("SWS/BR - Error", "sws_mbox"), 0);
		return false;
	}

	envelope.Sort();
	double start = this->GetAudioStart();
	double end   = this->GetAudioEnd();

	vector<double> values;
	this->GetAnalyzeData(NULL, NULL, NULL, NULL, NULL, NULL, ((momentary) ? NULL : &values), ((momentary) ? &values : NULL));
	envelope.DeletePointsInRange(start, end);

	double position = start;
	envelope.CreatePoint(envelope.CountPoints(), position, envelope.LaneMinValue(), LINEAR, 0, false);
	position += (momentary) ? 0.4 : 3;

	size_t size = values.size();
	for (size_t i = 0; i < size; ++i)
	{
		double value = envelope.RealValue(TranslateRange(values[i], minLUFS, maxLUFS, 0.0, 1.0));

		if (i != size-1)
		{
			envelope.CreatePoint(envelope.CountPoints(), position, value, LINEAR, 0, false);
			position += (momentary) ? 0.4 : 3;
		}
		else
		{
			// Reached the end, last point has to be square and end right where the audio ends
			if (position < end)
			{
				envelope.CreatePoint(envelope.CountPoints(), position, value, LINEAR, 0, false);
				envelope.CreatePoint(envelope.CountPoints(), end, value, LINEAR, 0, false);
				if (value > envelope.LaneMinValue())
					envelope.CreatePoint(envelope.CountPoints(), end, envelope.LaneMinValue(), SQUARE, 0, false);
			}
			else
			{
				if (position > end)
					position = end;

				if (value > envelope.LaneMinValue())
				{
					envelope.CreatePoint(envelope.CountPoints(), position, value, LINEAR, 0, false);
					envelope.CreatePoint(envelope.CountPoints(), end, envelope.LaneMinValue(), SQUARE, 0, false);
				}
				else
				{
					envelope.CreatePoint(envelope.CountPoints(), position, value, SQUARE, 0, false);
				}
			}
			break;
		}
	}

	// In case there are no values (item too short) make sure graph ends with minimum
	if (size == 0)
		envelope.CreatePoint(envelope.CountPoints(), end, envelope.LaneMinValue(), SQUARE, 0, false);

	return true;
}

bool BR_LoudnessObject::NormalizeIntegrated (double targetLUFS)
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return false;

	double integrated;
	this->GetAnalyzeData(&integrated, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	bool update = false;
	if (integrated > NEGATIVE_INF)
	{
		if (this->GetTrack())
		{
			double volume = *(double*)GetSetMediaTrackInfo(this->GetTrack(), "D_VOL", NULL);
			volume *= DB2VAL(targetLUFS) / DB2VAL(integrated);
			GetSetMediaTrackInfo(this->GetTrack(), "D_VOL", &volume);

			update = true;
		}
		else
		{
			if (!IsItemLocked(this->GetItem()) && !IsLocked(ITEM_FULL))
			{
				double volume = *(double*)GetSetMediaItemTakeInfo(this->GetTake(), "D_VOL", NULL);
				volume *= DB2VAL(targetLUFS) / DB2VAL(integrated);
				GetSetMediaItemTakeInfo(this->GetTake(), "D_VOL", &volume);

				update = true;
			}
		}
	}

	return update;
}

void BR_LoudnessObject::SetSelectedInProject (bool selected)
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return;

	if (this->GetTrack())
	{
		int sel = (selected) ? (1) : (0);
		GetSetMediaTrackInfo(this->GetTrack(), "I_SELECTED", &sel);
	}
	else
	{
		MediaItem* item = this->GetItem();
		GetSetMediaItemInfo(item, "B_UISEL", &selected);
		UpdateItemInProject(item);
	}
}

void BR_LoudnessObject::GoToTarget ()
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return;

	MediaTrack* track = NULL;
	if (this->GetTrack())
		track = this->GetTrack();
	else
	{
		track = GetMediaItemTake_Track(this->GetTake());
		SetArrangeStart(GetMediaItemInfo_Value(this->GetItem(), "D_POSITION"));
	}
	ScrollToTrackIfNotInArrange(track);
}

void BR_LoudnessObject::GoToMomentaryMax (bool timeSelection)
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return;

	if (g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::DO_HIGH_PRECISION_MODE))
	{
		MessageBox(g_loudnessWndManager.GetMsgHWND(), __LOCALIZE("Going to maximum momentary in high precision mode\nis currently not implemented.", "sws_mbox"), __LOCALIZE("SWS/BR - Error", "sws_mbox"), 0);
		return;
	}
		
	PreventUIRefresh(1);

	double position = this->GetMaxMomentaryPos(true);
	SetEditCurPos2(NULL, position, true, false);
	ScrollToTrackIfNotInArrange((this->GetTrack()) ? this->GetTrack() : GetMediaItemTake_Track(this->GetTake()));

	if (timeSelection)
	{
		double timeselEnd = position + 0.4;
		GetSet_LoopTimeRange2(NULL, true, false, &position, &timeselEnd, false);
	}
	PreventUIRefresh(-1);
}

void BR_LoudnessObject::GoToShortTermMax (bool timeSelection)
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return;

	if (g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::DO_HIGH_PRECISION_MODE))
	{
		MessageBox(g_loudnessWndManager.GetMsgHWND(), __LOCALIZE("Going to maximum short-term in high precision mode\nis currently not implemented.", "sws_mbox"), __LOCALIZE("SWS/BR - Error", "sws_mbox"), 0);
		return;
	}

	PreventUIRefresh(1);

	double position = this->GetMaxShorttermPos(true);
	SetEditCurPos2(NULL, position, true, false);
	ScrollToTrackIfNotInArrange((this->GetTrack()) ? this->GetTrack() : GetMediaItemTake_Track(this->GetTake()));

	if (timeSelection)
	{
		double timeselEnd = position + 3;
		GetSet_LoopTimeRange2(NULL, true, false, &position, &timeselEnd, false);
	}

	PreventUIRefresh(-1);
}

void BR_LoudnessObject::GoToTruePeak ()
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return;

	double position = this->GetTruePeakPos(true);
	if (position >= 0)
	{
		PreventUIRefresh(1);
		SetEditCurPos2(NULL, position, true, false);
		ScrollToTrackIfNotInArrange((this->GetTrack()) ? this->GetTrack() : GetMediaItemTake_Track(this->GetTake()));
		PreventUIRefresh(-1);
	}
}

int BR_LoudnessObject::GetTrackNumber ()
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return -1;
	int id = (int)GetMediaTrackInfo_Value(this->GetTrack() ? this->GetTrack() : GetMediaItemTake_Track(this->GetTake()), "IP_TRACKNUMBER");
	if (id == -1) return  0; // master
	if (id == 0)  return -1; // nothing found
	return id;
}

int BR_LoudnessObject::GetItemNumber ()
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return -1;

	if (!this->GetTrack())
		return 1 + (int)GetMediaItemInfo_Value(this->GetItem(), "IP_ITEMNUMBER");
	else
		return -1;
}

unsigned WINAPI BR_LoudnessObject::AnalyzeData (void* loudnessObject)
{
	// Analyze results that get saved at the end
	double integrated   = NEGATIVE_INF;
	double truePeak     = NEGATIVE_INF;
	double truePeakPos  = -1;
	double momentaryMax = NEGATIVE_INF;
	double shortTermMax = NEGATIVE_INF;
	double range        = 0;
	vector<double> momentaryValues;
	vector<double> shortTermValues;

	// Get take/track info
	BR_LoudnessObject* _this = (BR_LoudnessObject*)loudnessObject;
	BR_LoudnessObject::AudioData data = _this->GetAudioData();

	const bool doPan               = data.channels > 1              && data.pan != 0; // tracks will always get false here (see CheckSetAudioData())
	const bool doVolEnv            = data.volEnv.CountPoints()      && data.volEnv.IsActive();
	const bool doVolPreFXEnv       = data.volEnvPreFX.CountPoints() && data.volEnvPreFX.IsActive();
	const bool integratedOnly      = _this->GetIntegratedOnly();
	const bool doTruePeak          = _this->GetDoTruePeak();
	const bool doHighPrecisionMode = _this->GetDoHighPrecisionMode() && !integratedOnly;
	const bool doDualMonoMode      = _this->GetDoDualMonoMode();

	/*
	NF: fix for wrong results when analyzing item, item is not at pos 0.0 and contains take vol. env.
	https://github.com/reaper-oss/sws/issues/957#issuecomment-371233030
	TakeAudioAccesor, unlike TrackAudioAccessor returns relative start/end times
	https://forum.cockos.com/showthread.php?t=204397
	so in the correction for take volume env. we must add item pos. to get correct env. evaluation
	*/
	const double itemPos = _this->m_take ?
		GetMediaItemInfo_Value(_this->GetItem(), "D_POSITION") : 0.0;

	// Prepare ebur123_state
	ebur128_state* loudnessState = NULL;
	int mode = integratedOnly ? EBUR128_MODE_I : EBUR128_MODE_M | EBUR128_MODE_S | EBUR128_MODE_I | EBUR128_MODE_LRA;
	if (!integratedOnly && doTruePeak)
		mode |= EBUR128_MODE_TRUE_PEAK;
	loudnessState = ebur128_init((size_t)data.channels, (size_t)data.samplerate, mode);

	// Ignore channels according to channel mode. Note: we can't partially request samples, i.e. channel mode is mono, but take is stereo...asking for
	// 1 channel only won't work. We must always request the real channel count even though reaper interleaves active channels starting from 0
	if (data.channelMode > 1)
	{
		// Mono channel modes
		if (data.channelMode <= 66)
		{	
			if (doDualMonoMode) {
				// Actually there's a check in ebur128_set_channel() for dual mono mode:
				// if ebur128_init() is called with channels > 1 than dual mono mode is disabled.
				// But it's ok because if source channels > 1 and set to mono channel modes,
				// AudioAccessor returns dual mono anyway (what we want)
				// so we can use stereo channel map in this case
				ebur128_set_channel(loudnessState, 0, EBUR128_DUAL_MONO);
				ebur128_set_channel(loudnessState, 1, EBUR128_RIGHT);
				for (int i = 2; i <= data.channels; ++i)
					ebur128_set_channel(loudnessState, i, EBUR128_UNUSED);
			}
			else {
				ebur128_set_channel(loudnessState, 0, EBUR128_LEFT);
				for (int i = 1; i <= data.channels; ++i)
					ebur128_set_channel(loudnessState, i, EBUR128_UNUSED);
			}
		}
		// Stereo channel modes
		else
		{
			ebur128_set_channel(loudnessState, 0, EBUR128_LEFT);
			ebur128_set_channel(loudnessState, 1, EBUR128_RIGHT);
			for (int i = 2; i <= data.channels; ++i)
				ebur128_set_channel(loudnessState, i, EBUR128_UNUSED);
		}
	}
	// Dual mono mode for mono takes
	else if (doDualMonoMode && _this->m_take && data.channels == 1)
	{
		ebur128_set_channel(loudnessState, 0, EBUR128_DUAL_MONO);
		for (int i = 1; i <= data.channels; ++i)
			ebur128_set_channel(loudnessState, i, EBUR128_UNUSED);
	}
	// Normal channel mode
	else
	{
		ebur128_set_channel(loudnessState, 0, EBUR128_LEFT);
		ebur128_set_channel(loudnessState, 1, EBUR128_RIGHT);
		ebur128_set_channel(loudnessState, 2, EBUR128_CENTER);
		ebur128_set_channel(loudnessState, 3, EBUR128_LEFT_SURROUND);
		ebur128_set_channel(loudnessState, 4, EBUR128_RIGHT_SURROUND);
	}

	bool doShortTerm = true;
	bool doMomentary = true;

	// This lets us get integrated reading even if the target is too short
	const double effectiveEndTime = data.audioEnd;
	if (data.audioEnd - data.audioStart < 3)
	{
		doShortTerm = false; // doMomentary is set during calculation
		data.audioEnd = data.audioStart + 3;
	}

	// Fill loudnessState with samples and get momentary/short-term measurements
	// standard mode       =   5 Hz refresh rate (200 ms buffer)
	// high precision mode = 100 Hz refresh rate ( 10 ms buffer)
	const int refreshRateInHz = doHighPrecisionMode ? 100 : 5;
	const double bufferTime = 1.0 / refreshRateInHz; // how many seconds in a buffer
	const double sampleTimeLen = 1.0 / data.samplerate;
	const double audioLength = data.audioEnd - data.audioStart;

	int sampleCount = data.samplerate / refreshRateInHz;
	int bufSz = sampleCount * data.channels;
	double currentTime = data.audioStart;
	bool momentaryFilled = true;
	int processedSamples = 0;
	int i = 0;

	while (currentTime < data.audioEnd && !_this->GetKillFlag())
	{
		// Make sure we always fill our buffer exactly to audio end (and skip momentary/short-term intervals if not enough new samples)
		bool skipIntervals = false;
		const double remainingTime = data.audioEnd - currentTime; // how many seconds until the end of the audio source
		if (remainingTime < bufferTime + numeric_limits<double>::epsilon())
		{
			sampleCount = static_cast<int>(data.samplerate * remainingTime);
			bufSz = sampleCount * data.channels;
			skipIntervals = true;
		}

		// Get new 200 ms (or 10 ms in high precision mode) of samples
		// GetAudioAccessorSamples() stops writing to the buffer once it reaches the item's end, everything from that point to sampleCount is garbage
		std::vector<double> samples(bufSz);
		GetAudioAccessorSamples(data.audio, data.samplerate, data.channels, currentTime, sampleCount, &samples[0]);

		// Correct for volume and pan/volume envelopes
		int currentChannel = 1;
		double sampleTime = currentTime;

		for (double &sample : samples)
		{
			double adjust = 1;

			// Volume envelopes
			if (doVolPreFXEnv)
				adjust *= data.volEnvPreFX.ValueAtPosition(sampleTime, true);
			if (doVolEnv) 
				adjust *= data.volEnv.ValueAtPosition(sampleTime + itemPos, true);

			// Volume fader
			adjust *= data.volume;

			// Pan fader (takes only)
			if (doPan)
			{
				if (data.pan > 0 && (currentChannel % 2 == 1))
					adjust *= 1 - data.pan; // takes have no pan law!
				else if (data.pan < 0 && (currentChannel % 2 == 0))
					adjust *= 1 + data.pan;
			}

			sample *= adjust;

			if (++currentChannel > data.channels)
				currentChannel = 1;

			if (currentChannel + 1 > data.channels)
				sampleTime = sampleTime + sampleTimeLen;
		}

		ebur128_add_frames_double(loudnessState, &samples[0], sampleCount);

		if (!integratedOnly && !skipIntervals)
		{
			if (!doShortTerm && doMomentary)
			{
				if (currentTime + bufferTime >= effectiveEndTime + numeric_limits<double>::epsilon())
					doMomentary = false;
			}

			// Momentary buffer (400 ms) filled
			if ((i % 2 > 0) == momentaryFilled && doMomentary)
			{
				double momentary;
				ebur128_loudness_momentary(loudnessState, &momentary);
				if (momentary == -HUGE_VAL)
					momentary = NEGATIVE_INF;
				if (momentary > momentaryMax)
					momentaryMax = momentary;
				momentaryValues.push_back(momentary);
			}

			// Short-term buffer (3000 ms) filled
			if (i == 14 && doShortTerm)
			{
				double shortTerm;
				ebur128_loudness_shortterm(loudnessState, &shortTerm);
				if (shortTerm == -HUGE_VAL)
					shortTerm = NEGATIVE_INF;
				if (shortTerm > shortTermMax)
					shortTermMax = shortTerm;
				shortTermValues.push_back(shortTerm);
			}
		}

		// This is definitely more accurate than adding 0.2 seconds every time
		processedSamples += sampleCount;
		currentTime = data.audioStart + ((double)processedSamples / (double)data.samplerate);

		_this->SetProgress ((currentTime - data.audioStart) / audioLength * 0.95); // loudness_global and loudness_range seem rather fast and since we currently
		if (++i == 15)                                                             // can't monitor their progress, leave last 10% of progress for them
		{
			i = 0;
			momentaryFilled = !momentaryFilled;
		}

		// We reached the end of the file, break without checking currentTime against endTime (rounding errors could make us go through loop one more time)
		if (skipIntervals)
			break;
	}

	// Get integrated and loudness range
	if (!_this->GetKillFlag())
	{
		ebur128_loudness_global(loudnessState, &integrated);
		if (!integratedOnly)
		{
			ebur128_loudness_range(loudnessState, &range);
			if (doTruePeak)
			{
				for (int i = 0; i < data.channels; ++i)
				{
					double channelTruePeak, channelTruePeakPos;
					ebur128_true_peak(loudnessState, i, &channelTruePeak, &channelTruePeakPos);
					if (channelTruePeak > truePeak)
					{
						truePeak    = channelTruePeak;
						truePeakPos = channelTruePeakPos;
					}
				}
				truePeak = VAL2DB(truePeak);
				_this->SetTruePeakAnalyzed(true);
			}

		}
	}
	ebur128_destroy(&loudnessState);

	// Write analyze data
	if (!_this->GetKillFlag())
	{
		_this->SetAnalyzeData(integrated, range, truePeak, truePeakPos, shortTermMax, momentaryMax, shortTermValues, momentaryValues);
		_this->SetProgress(1);
		_this->SetRunning(false);
		if (!integratedOnly)
			_this->SetAnalyzedStatus(true);
	}

	return 0;
}

int BR_LoudnessObject::CheckSetAudioData ()
{
	SWS_SectionLock lock(&m_mutex);
	if (!this->IsTargetValid())
		return 0;

	BR_LoudnessObject::AudioData audioData = this->GetAudioData();

	char newHash[128]; memset(newHash, 0, 128);
	GetAudioAccessorHash(audioData.audio, newHash);

	double audioStart = GetAudioAccessorStartTime(audioData.audio);
	double audioEnd   = GetAudioAccessorEndTime(audioData.audio);
	int channels    = (this->GetTrack()) ? ((int)GetMediaTrackInfo_Value(this->GetTrack(), "I_NCHAN")) : ((GetMediaItemTake_Source(this->GetTake()))->GetNumChannels());
	int channelMode = (this->GetTrack()) ? (0) : (*(int*)GetSetMediaItemTakeInfo(this->GetTake(), "I_CHANMODE", NULL));
	int samplerate = (int)(1 / parse_timestr_len("1", 0, 4)); // NF: https://forum.cockos.com/showpost.php?p=2060657&postcount=15

	BR_Envelope volEnv, volEnvPreFX;
	double volume = 0, pan = 0;
	if (this->GetTrack())
	{
		volume = *(double*)GetSetMediaTrackInfo(this->GetTrack(), "D_VOL", NULL);
		pan = 0; // ignore track pan
		volEnv = BR_Envelope(GetVolEnv(this->GetTrack()));
		volEnvPreFX = BR_Envelope(GetVolEnvPreFX(this->GetTrack()));
	}
	else
	{
		volume = (*(double*)GetSetMediaItemTakeInfo(this->GetTake(), "D_VOL", NULL)) * (*(double*)GetSetMediaItemInfo(this->GetItem(), "D_VOL", NULL));
		pan = *(double*)GetSetMediaItemTakeInfo(this->GetTake(), "D_PAN", NULL);
		volEnv = BR_Envelope(this->GetTake(), VOLUME);
	}

	if (!this->GetAnalyzedStatus()                       ||
	    AudioAccessorValidateState(audioData.audio)      ||
	    strcmp(newHash, audioData.audioHash)             ||
	    audioStart   != audioData.audioStart             ||
	    audioEnd     != audioData.audioEnd               ||
	    channels     != audioData.channels               ||
	    channelMode  != audioData.channelMode            ||
	    samplerate   != audioData.samplerate             ||
	    fabs(volume - audioData.volume) >= VOLUME_DELTA  ||
	    fabs(pan    - audioData.pan)    >= PAN_DELTA     ||
	    volEnv       != audioData.volEnv                 ||
	    volEnvPreFX  != audioData.volEnvPreFX
	)
	{
		DestroyAudioAccessor(audioData.audio);
		audioData.audio = (this->GetTrack()) ? (CreateTrackAudioAccessor(this->GetTrack())) : (CreateTakeAudioAccessor(this->GetTake()));
		memset(audioData.audioHash, 0, 128);
		GetAudioAccessorHash(audioData.audio, audioData.audioHash);

		audioData.audioStart   = GetAudioAccessorStartTime(audioData.audio);
		audioData.audioEnd     = GetAudioAccessorEndTime(audioData.audio);
		audioData.channels     = channels;
		audioData.channelMode  = channelMode;
		audioData.samplerate   = samplerate;
		audioData.volume       = volume;
		audioData.pan          = pan;
		audioData.volEnv       = volEnv;
		audioData.volEnvPreFX  = volEnvPreFX;

		this->SetAudioData(audioData);

		this->SetAnalyzedStatus(false);
		this->SetTruePeakAnalyzed(false);
		return 2;
	}
	else
		return 1;
}

void BR_LoudnessObject::SetAudioData (const BR_LoudnessObject::AudioData& audioData)
{
	SWS_SectionLock lock(&m_mutex);
	m_audioData = audioData;
}

BR_LoudnessObject::AudioData BR_LoudnessObject::GetAudioData ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_audioData;
}

void BR_LoudnessObject::SetRunning (bool running)
{
	SWS_SectionLock lock(&m_mutex);
	m_running = running;
}

void BR_LoudnessObject::SetProgress (double progress)
{
	SWS_SectionLock lock(&m_mutex);
	m_progress = progress;
}

void BR_LoudnessObject::SetAnalyzeData (double integrated, double range, double truePeak, double truePeakPos, double shortTermMax, double momentaryMax, const vector<double>& shortTermValues, const vector<double>& momentaryValues)
{
	SWS_SectionLock lock(&m_mutex);
	m_integrated      = integrated;
	m_range           = range;
	m_truePeak        = truePeak;
	m_truePeakPos     = truePeakPos;
	m_shortTermMax    = shortTermMax;
	m_momentaryMax    = momentaryMax;
	m_shortTermValues = shortTermValues;
	m_momentaryValues = momentaryValues;
}

void BR_LoudnessObject::GetAnalyzeData (double* integrated, double* range, double* truePeak, double* truePeakPos, double* shortTermMax, double* momentaryMax, vector<double>* shortTermValues, vector<double>* momentaryValues)
{
	SWS_SectionLock lock(&m_mutex);
	WritePtr(integrated,      m_integrated);
	WritePtr(range,           m_range);
	WritePtr(truePeak,        m_truePeak);
	WritePtr(truePeakPos,     m_truePeakPos);
	WritePtr(shortTermMax,    m_shortTermMax);
	WritePtr(momentaryMax,    m_momentaryMax);
	WritePtr(shortTermValues, m_shortTermValues);
	WritePtr(momentaryValues, m_momentaryValues);
}

void BR_LoudnessObject::SetAnalyzedStatus (bool analyzed)
{
	SWS_SectionLock lock(&m_mutex);
	m_analyzed = analyzed;
}

bool BR_LoudnessObject::GetAnalyzedStatus ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_analyzed;
}

void BR_LoudnessObject::SetIntegratedOnly (bool integratedOnly)
{
	SWS_SectionLock lock(&m_mutex);
	m_integratedOnly = integratedOnly;
}

bool BR_LoudnessObject::GetIntegratedOnly ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_integratedOnly;
}

void BR_LoudnessObject::SetDoTruePeak (bool doTruePeak)
{
	SWS_SectionLock lock(&m_mutex);
	m_doTruePeak = doTruePeak;
}

bool BR_LoudnessObject::GetDoTruePeak ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_doTruePeak;
}

void BR_LoudnessObject::SetDoHighPrecisionMode(bool doHighPrecionMode)
{
	SWS_SectionLock lock(&m_mutex);
	m_doHighPrecisionMode = doHighPrecionMode;
}

bool BR_LoudnessObject::GetDoHighPrecisionMode()
{
	SWS_SectionLock lock(&m_mutex);
	return m_doHighPrecisionMode;
}

void BR_LoudnessObject::SetDoDualMonoMode(bool doDualMonoMode)
{
	SWS_SectionLock lock(&m_mutex);
	m_doDualMonoMode = doDualMonoMode;
	
}

bool BR_LoudnessObject::GetDoDualMonoMode()
{
	SWS_SectionLock lock(&m_mutex);
	return m_doDualMonoMode;
}

void BR_LoudnessObject::SetTruePeakAnalyzed (bool analyzed)
{
	SWS_SectionLock lock(&m_mutex);
	m_truePeakAnalyzed = analyzed;
}

bool BR_LoudnessObject::GetTruePeakAnalyzeStatus ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_truePeakAnalyzed;
}

void BR_LoudnessObject::SetKillFlag (bool killFlag)
{
	SWS_SectionLock lock(&m_mutex);
	m_killFlag = killFlag;
}

bool BR_LoudnessObject::GetKillFlag ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_killFlag;
}

void BR_LoudnessObject::SetProcess (HANDLE process)
{
	SWS_SectionLock lock(&m_mutex);
	m_process = process;
}

HANDLE BR_LoudnessObject::GetProcess ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_process;
}

WDL_FastString BR_LoudnessObject::GetTakeName ()
{
	SWS_SectionLock lock(&m_mutex);

	WDL_FastString takeName;
	if (m_track)
	{
		takeName.Set("-");
	}
	else
	{
		if (this->IsTargetValid())
			takeName.Set((char*)GetSetMediaItemTakeInfo(this->GetTake(), "P_NAME", NULL));
		else
			takeName.Set(__LOCALIZE("--NO TAKE FOUND--", "sws_DLG_174")); // Analyze loudness window should remove invalid objects, but leave this anyway
	}

	return takeName;
}

WDL_FastString BR_LoudnessObject::GetTrackName ()
{
	SWS_SectionLock lock(&m_mutex);

	WDL_FastString trackName;
	if (this->GetTrack())
	{
		if (this->IsTargetValid())
		{
			if (this->GetTrack() == GetMasterTrack(NULL))
				trackName.Set(__localizeFunc("MASTER", "track", 0));
			else
				trackName.Set((char*)GetSetMediaTrackInfo(this->GetTrack(), "P_NAME", NULL));
		}
		else
			trackName.Set(__LOCALIZE("--NO TRACK FOUND--", "sws_DLG_174")); // Analyze loudness window should remove invalid objects, but leave this anyway
	}
	else
	{
		if (this->IsTargetValid())
			trackName.Set((char*)GetSetMediaTrackInfo(GetMediaItemTake_Track(this->GetTake()), "P_NAME", NULL));
		else
			trackName.Set(__LOCALIZE("--NO TAKE FOUND--", "sws_DLG_174")); // Analyze loudness window should remove invalid objects, but leave this anyway
	}

	return trackName;
}

MediaItem* BR_LoudnessObject::GetItem ()
{
	SWS_SectionLock lock(&m_mutex);
	MediaItem_Take* take = this->GetTake();
	if (take)
		return GetMediaItemTake_Item(take);
	else
		return NULL;
}

MediaTrack* BR_LoudnessObject::GetTrack ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_track;
}

MediaItem_Take* BR_LoudnessObject::GetTake ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_take;
}

GUID BR_LoudnessObject::GetGuid ()
{
	SWS_SectionLock lock(&m_mutex);
	return m_guid;
}

void BR_LoudnessObject::SetTrack (MediaTrack* track)
{
	SWS_SectionLock lock(&m_mutex);
	m_track = track;
}

void BR_LoudnessObject::SetTake (MediaItem_Take* take)
{
	SWS_SectionLock lock(&m_mutex);
	m_take = take;
}

void BR_LoudnessObject::SetGuid (GUID guid)
{
	SWS_SectionLock lock(&m_mutex);
	m_guid = guid;
}

BR_LoudnessObject::AudioData::AudioData () :
audio        (NULL),
samplerate   (0),
channels     (0),
channelMode  (0),
audioStart   (0),
audioEnd     (0),
volume       (0),
pan          (0)
{
	memset(audioHash, 0, 128);
}

/******************************************************************************
* Loudness preferences                                                        *
******************************************************************************/
static bool ProcessExtensionLine (const char *line, ProjectStateContext *ctx, bool isUndo, project_config_extension_t *reg)
{
	if (isUndo)
		return false;

	LineParser lp(false);
	if (lp.parse(line) || lp.getnumtokens() < 1)
		return false;

	if (!strcmp(lp.gettoken_str(0), PROJ_SETTINGS_KEY))
	{
		g_pref.LoadProjPref(ctx);
		return true;
	}

	if (!strcmp(lp.gettoken_str(0), PROJ_OBJECT_KEY))
	{
		if (BR_LoudnessObject* object = new (nothrow) BR_LoudnessObject())
		{
			if (object->RestoreObject(ctx))
				g_analyzedObjects.Get()->Add(object);
			else
				delete object;
		}
		return true;
	}

	return false;
}

static void SaveExtensionConfig (ProjectStateContext *ctx, bool isUndo, project_config_extension_t *reg)
{
	if (isUndo)
		return;

	g_pref.SaveProjPref(ctx);

	for (int i = 0; i < g_analyzedObjects.Get()->GetSize(); ++i)
	{
		if (BR_LoudnessObject* object = g_analyzedObjects.Get()->Get(i))
		{
			if (object->IsTargetValid())
				object->SaveObject(ctx);
		}
	}
}

static void BeginLoadProjectState (bool isUndo, project_config_extension_t *reg)
{
	if (isUndo)
		return;

	g_analyzedObjects.Get()->Empty(true);
	g_analyzedObjects.Cleanup();
	g_pref.CleanProjPref();
}

BR_LoudnessPref& BR_LoudnessPref::Get ()
{
	static BR_LoudnessPref s_instance;
	return s_instance;
}

double BR_LoudnessPref::LUtoLUFS (double lu)
{
	return lu + this->GetReferenceLU();
}

double BR_LoudnessPref::LUFStoLU (double lufs)
{
	return lufs - this->GetReferenceLU();
}

double BR_LoudnessPref::GetGraphMin ()
{
	return m_projData.Get()->useProjGraph ? m_projData.Get()->graphMin : m_graphMin;
}

double BR_LoudnessPref::GetGraphMax ()
{
	return m_projData.Get()->useProjGraph ? m_projData.Get()->graphMax : m_graphMax;
}

double BR_LoudnessPref::GetReferenceLU ()
{
	return (m_projData.Get()->useProjLU) ? (m_projData.Get()->valueLU) : (m_valueLU);
}

WDL_FastString BR_LoudnessPref::GetFormatedLUString ()
{
	return m_projData.Get()->stringLU;
}

void BR_LoudnessPref::ShowPreferenceDlg (bool show)
{
	if (show)
	{
		if (!m_prefWnd)
			m_prefWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_BR_LOUDNESS_PREF), g_hwndParent, this->GlobalLoudnessPrefProc);
		else
		{
			ShowWindow(m_prefWnd, SW_SHOW);
			SetFocus(m_prefWnd);
		}
	}
	else
	{
		if (m_prefWnd)
		{
			DestroyWindow(m_prefWnd);
			m_prefWnd = NULL;
		}
	}
}

void BR_LoudnessPref::UpdatePreferenceDlg ()
{
	if (m_prefWnd)
		SendMessage(m_prefWnd, WM_COMMAND, BR_LoudnessPref::READ_PROJDATA, 0);
}

bool BR_LoudnessPref::IsPreferenceDlgVisible ()
{
	return m_prefWnd ? true : false;
}

void BR_LoudnessPref::SaveGlobalPref ()
{
	int luFormat;  // don't rely on enum values
	if      (m_globalLUFormat == BR_LoudnessPref::LU)      luFormat = 0;
	else if (m_globalLUFormat == BR_LoudnessPref::LU_AT_K) luFormat = 1;
	else if (m_globalLUFormat == BR_LoudnessPref::LU_K)    luFormat = 2;
	else if (m_globalLUFormat == BR_LoudnessPref::K)       luFormat = 3;
	else                                                   luFormat = 0;

	char tmp[966];
	snprintf(tmp, sizeof(tmp), "%lf %d %lf %lf", m_valueLU, luFormat, m_graphMin, m_graphMax);
	WritePrivateProfileString("SWS", PREF_KEY, tmp, get_ini_file());
}

void BR_LoudnessPref::LoadGlobalPref ()
{
	char tmp[256];
	GetPrivateProfileString("SWS", PREF_KEY, "", tmp, sizeof(tmp), get_ini_file());

	LineParser lp(false);
	lp.parse(tmp);
	m_valueLU         = (lp.getnumtokens() > 0) ? lp.gettoken_float(0) : -23;
	m_globalLUFormat  = (lp.getnumtokens() > 1) ? lp.gettoken_int(1)   : 0;
	m_graphMin        = (lp.getnumtokens() > 2) ? lp.gettoken_float(2) : -41;
	m_graphMax        = (lp.getnumtokens() > 3) ? lp.gettoken_float(3) : -14;

	if      (m_globalLUFormat == 0) m_globalLUFormat = BR_LoudnessPref::LU;  // don't rely on enum values
	else if (m_globalLUFormat == 1) m_globalLUFormat = BR_LoudnessPref::LU_AT_K;
	else if (m_globalLUFormat == 2) m_globalLUFormat = BR_LoudnessPref::LU_K;
	else if (m_globalLUFormat == 3) m_globalLUFormat = BR_LoudnessPref::K;
	else                            m_globalLUFormat = BR_LoudnessPref::LU;
}

void BR_LoudnessPref::SaveProjPref (ProjectStateContext *ctx)
{
	if (m_projData.Get()->useProjLU || m_projData.Get()->useProjGraph)
	{
		ctx->AddLine("%s", PROJ_SETTINGS_KEY);
		if (m_projData.Get()->useProjLU)    ctx->AddLine("%s %lf",     PROJ_SETTINGS_KEY_LU,    m_projData.Get()->valueLU);
		if (m_projData.Get()->useProjGraph) ctx->AddLine("%s %lf %lf", PROJ_SETTINGS_KEY_GRAPH, m_projData.Get()->graphMin, m_projData.Get()->graphMax);
		ctx->AddLine(">");
	}
}

void BR_LoudnessPref::LoadProjPref (ProjectStateContext *ctx)
{
	char line[256];
	LineParser lp(false);
	while(!ctx->GetLine(line, sizeof(line)) && !lp.parse(line))
	{
		if (!strcmp(lp.gettoken_str(0), ">"))
			break;

		if (!strcmp(lp.gettoken_str(0), PROJ_SETTINGS_KEY_LU))
		{
			m_projData.Get()->valueLU = lp.gettoken_float(1);
			m_projData.Get()->useProjLU = true;
		}
		else if (!strcmp(lp.gettoken_str(0), PROJ_SETTINGS_KEY_GRAPH))
		{
			m_projData.Get()->graphMin     = lp.gettoken_float(1);
			m_projData.Get()->graphMax     = lp.gettoken_float(2);
			m_projData.Get()->useProjGraph = true;
		}
	}

	m_projData.Get()->stringLU = this->GetFormatedLUString(g_pref.m_globalLUFormat);
}

void BR_LoudnessPref::CleanProjPref ()
{
	m_projData.Cleanup();
}

WDL_FastString BR_LoudnessPref::GetFormatedLUString (int mode, double* valueLU /*=NULL*/)
{
	WDL_FastString string;
	if (mode == BR_LoudnessPref::LU_AT_K)
		string.AppendFormatted(256, "%s%g", __LOCALIZE("LU at K", "sws_loudness"), valueLU ? *valueLU : this->GetReferenceLU());
	else if (mode == BR_LoudnessPref::LU_K)
		string.AppendFormatted(256, "%s%g", __LOCALIZE("LU K", "sws_loudness"), valueLU ? *valueLU : this->GetReferenceLU());
	else if (mode == BR_LoudnessPref::K)
		string.AppendFormatted(256, "%s%g", __LOCALIZE("K", "sws_loudness"), valueLU ? *valueLU : this->GetReferenceLU());
	else
		string.AppendFormatted(256, "%s", __LOCALIZE("LU", "sws_loudness"));

	return string;
}

WDL_DLGRET BR_LoudnessPref::GlobalLoudnessPrefProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (INT_PTR r = SNM_HookThemeColorsMessage(hwnd, uMsg, wParam, lParam))
		return r;

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			WDL_UTF8_HookComboBox(GetDlgItem(hwnd, IDC_LU_FORMAT));
			WDL_UTF8_HookComboBox(GetDlgItem(hwnd, IDC_PROJ_LU));
			WDL_UTF8_HookComboBox(GetDlgItem(hwnd, IDC_GLOBAL_LU));

			SendDlgItemMessage(hwnd, IDC_PROJ_LU, CB_ADDSTRING, 0, (LPARAM)"-12");
			SendDlgItemMessage(hwnd, IDC_PROJ_LU, CB_ADDSTRING, 0, (LPARAM)"-14");
			SendDlgItemMessage(hwnd, IDC_PROJ_LU, CB_ADDSTRING, 0, (LPARAM)"-16");
			SendDlgItemMessage(hwnd, IDC_PROJ_LU, CB_ADDSTRING, 0, (LPARAM)"-20");
			SendDlgItemMessage(hwnd, IDC_PROJ_LU, CB_ADDSTRING, 0, (LPARAM)"-23");
			SendDlgItemMessage(hwnd, IDC_GLOBAL_LU, CB_ADDSTRING, 0, (LPARAM)"-12");
			SendDlgItemMessage(hwnd, IDC_GLOBAL_LU, CB_ADDSTRING, 0, (LPARAM)"-14");
			SendDlgItemMessage(hwnd, IDC_GLOBAL_LU, CB_ADDSTRING, 0, (LPARAM)"-16");
			SendDlgItemMessage(hwnd, IDC_GLOBAL_LU, CB_ADDSTRING, 0, (LPARAM)"-20");
			SendDlgItemMessage(hwnd, IDC_GLOBAL_LU, CB_ADDSTRING, 0, (LPARAM)"-23");

			SendMessage(hwnd, WM_COMMAND, BR_LoudnessPref::READ_PROJDATA, 0);

			SetFocus(GetDlgItem(hwnd, g_pref.m_projData.Get()->useProjLU ? IDC_PROJ_LU : IDC_GLOBAL_LU));
			SendMessage(GetDlgItem(hwnd, g_pref.m_projData.Get()->useProjLU ? IDC_PROJ_LU : IDC_GLOBAL_LU), EM_SETSEL, 0, -1);
			RestoreWindowPos(hwnd, PREF_WND, false);
			ShowWindow(hwnd, SW_SHOW);
		}
		break;

		case WM_COMMAND :
		{
			switch (LOWORD(wParam))
			{
				case IDC_ENB_PROJ_LU:
				case IDC_ENB_PROJ_GRAPH:
				{
					EnableWindow(GetDlgItem(hwnd, IDC_PROJ_LU),  IsDlgButtonChecked(hwnd, IDC_ENB_PROJ_LU));
					EnableWindow(GetDlgItem(hwnd, IDC_MIN_PROJ), IsDlgButtonChecked(hwnd, IDC_ENB_PROJ_GRAPH));
					EnableWindow(GetDlgItem(hwnd, IDC_MAX_PROJ), IsDlgButtonChecked(hwnd, IDC_ENB_PROJ_GRAPH));

					SendMessage(hwnd, WM_COMMAND, BR_LoudnessPref::SAVE_PROJDATA, 0);
					MarkProjectDirty(NULL);
				}
				break;

				case IDC_GLOBAL_LU:
				case IDC_PROJ_LU:
				{
					// CBN_SELCHANGE is sent before selected string is set, so set it manually before saving project data
					if (HIWORD(wParam) == CBN_SELCHANGE)
					{
						char tmp[256];
						SendDlgItemMessage(hwnd, LOWORD(wParam), CB_GETLBTEXT, SendDlgItemMessage(hwnd, LOWORD(wParam), CB_GETCURSEL, 0, 0), (LPARAM)&tmp);
						SetDlgItemText(hwnd, LOWORD(wParam), tmp);

						SendMessage(hwnd, WM_COMMAND, BR_LoudnessPref::SAVE_PROJDATA, 0);
						if (LOWORD(wParam) == IDC_PROJ_LU)
							MarkProjectDirty(NULL);
					}

					if (HIWORD(wParam) == CBN_EDITCHANGE)
					{
						SendMessage(hwnd, WM_COMMAND, BR_LoudnessPref::SAVE_PROJDATA, 0);
						if (LOWORD(wParam) == IDC_PROJ_LU)
							MarkProjectDirty(NULL);
					}
				}
				break;

				case IDC_MIN:
				case IDC_MAX:
				case IDC_MIN_PROJ:
				case IDC_MAX_PROJ:
				{
					if (HIWORD(wParam) == EN_CHANGE && GetFocus() == GetDlgItem(hwnd, LOWORD(wParam)))
					{
						SendMessage(hwnd, WM_COMMAND, BR_LoudnessPref::SAVE_PROJDATA, 0);
						if (LOWORD(wParam) == IDC_MIN_PROJ || LOWORD(wParam) == IDC_MAX_PROJ)
							MarkProjectDirty(NULL);
					}
				}

				case IDC_LU_FORMAT:
				{
					if (HIWORD(wParam) == CBN_SELCHANGE)
						SendMessage(hwnd, WM_COMMAND, BR_LoudnessPref::SAVE_PROJDATA, 0);
				}
				break;

				case BR_LoudnessPref::READ_PROJDATA:
				{
					char tmp[256];
					snprintf(tmp, sizeof(tmp), "%g", g_pref.m_valueLU);                  SetDlgItemText(hwnd, IDC_GLOBAL_LU, tmp);
					snprintf(tmp, sizeof(tmp), "%g", g_pref.m_graphMin);                 SetDlgItemText(hwnd, IDC_MIN, tmp);
					snprintf(tmp, sizeof(tmp), "%g", g_pref.m_graphMax);                 SetDlgItemText(hwnd, IDC_MAX, tmp);
					snprintf(tmp, sizeof(tmp), "%g", g_pref.m_projData.Get()->valueLU);  SetDlgItemText(hwnd, IDC_PROJ_LU, tmp);
					snprintf(tmp, sizeof(tmp), "%g", g_pref.m_projData.Get()->graphMin); SetDlgItemText(hwnd, IDC_MIN_PROJ, tmp);
					snprintf(tmp, sizeof(tmp), "%g", g_pref.m_projData.Get()->graphMax); SetDlgItemText(hwnd, IDC_MAX_PROJ, tmp);

					CheckDlgButton(hwnd, IDC_ENB_PROJ_LU,    g_pref.m_projData.Get()->useProjLU);
					CheckDlgButton(hwnd, IDC_ENB_PROJ_GRAPH, g_pref.m_projData.Get()->useProjGraph);

					EnableWindow(GetDlgItem(hwnd, IDC_PROJ_LU),  g_pref.m_projData.Get()->useProjLU);
					EnableWindow(GetDlgItem(hwnd, IDC_MIN_PROJ), g_pref.m_projData.Get()->useProjGraph);
					EnableWindow(GetDlgItem(hwnd, IDC_MAX_PROJ), g_pref.m_projData.Get()->useProjGraph);

					SendMessage(hwnd, WM_COMMAND, BR_LoudnessPref::UPDATE_LU_FORMAT_PREVIEW, 0);
				}
				break;

				case BR_LoudnessPref::SAVE_PROJDATA:
				{
					char tmp[256];
					GetDlgItemText(hwnd, IDC_GLOBAL_LU, tmp, sizeof(tmp)); g_pref.m_valueLU  = AltAtof(tmp);
					GetDlgItemText(hwnd, IDC_MIN,       tmp, sizeof(tmp)); g_pref.m_graphMin = AltAtof(tmp);
					GetDlgItemText(hwnd, IDC_MAX,       tmp, sizeof(tmp)); g_pref.m_graphMax = AltAtof(tmp);
					GetDlgItemText(hwnd, IDC_PROJ_LU,   tmp, sizeof(tmp)); g_pref.m_projData.Get()->valueLU  = AltAtof(tmp);
					GetDlgItemText(hwnd, IDC_MIN_PROJ,  tmp, sizeof(tmp)); g_pref.m_projData.Get()->graphMin = AltAtof(tmp);
					GetDlgItemText(hwnd, IDC_MAX_PROJ,  tmp, sizeof(tmp)); g_pref.m_projData.Get()->graphMax = AltAtof(tmp);

					g_pref.m_projData.Get()->useProjLU    = !!IsDlgButtonChecked(hwnd, IDC_ENB_PROJ_LU);
					g_pref.m_projData.Get()->useProjGraph = !!IsDlgButtonChecked(hwnd, IDC_ENB_PROJ_GRAPH);

					g_pref.m_globalLUFormat = (int)SendDlgItemMessage(hwnd, IDC_LU_FORMAT, CB_GETCURSEL, 0, 0);

					SendMessage(hwnd, WM_COMMAND, BR_LoudnessPref::UPDATE_LU_FORMAT_PREVIEW, 0);
					LoudnessUpdate(false);
				}
				break;

				case BR_LoudnessPref::UPDATE_LU_FORMAT_PREVIEW:
				{
					for (int i = 0; i < BR_LoudnessPref::LU_FORMAT_COUNT; ++i)
						SendDlgItemMessage(hwnd, IDC_LU_FORMAT, CB_DELETESTRING, 0, 0);

					for (int i = 0; i < BR_LoudnessPref::LU_FORMAT_COUNT; ++i)
						SendDlgItemMessage(hwnd, IDC_LU_FORMAT, CB_ADDSTRING, 0, (LPARAM)g_pref.GetFormatedLUString(i).Get());

					g_pref.m_projData.Get()->stringLU = g_pref.GetFormatedLUString(g_pref.m_globalLUFormat);
					SendDlgItemMessage(hwnd, IDC_LU_FORMAT, CB_SETCURSEL, g_pref.m_globalLUFormat, 0);
				}
				break;

				case IDOK:
				case IDCANCEL:
				{
					SendMessage(hwnd, WM_COMMAND, BR_LoudnessPref::SAVE_PROJDATA, 0);
					DestroyWindow(hwnd);
					g_pref.m_prefWnd = NULL;
				}
				break;
			}
		}
		break;

		case WM_DESTROY:
		{
			SaveWindowPos(hwnd, PREF_WND);
		}
		break;
	}

	return 0;
}

BR_LoudnessPref::BR_LoudnessPref () :
m_prefWnd        (NULL),
m_valueLU        (-23),
m_graphMin       (-41),
m_graphMax       (-14),
m_globalLUFormat (BR_LoudnessPref::LU_K)
{
}

BR_LoudnessPref::ProjData::ProjData () :
useProjLU    (false),
useProjGraph (false),
valueLU      (-23),
graphMin     (-41),
graphMax     (-14)
{
	valueLU  = g_pref.m_valueLU;
	graphMin = g_pref.m_graphMin;
	graphMax = g_pref.m_graphMax;
	stringLU = g_pref.GetFormatedLUString(g_pref.m_globalLUFormat, &g_pref.m_valueLU); // need to supply the value otherwise the object calls itself in constructor
}

/******************************************************************************
* Normalize loudness                                                          *
******************************************************************************/
static WDL_DLGRET NormalizeProgressProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (INT_PTR r = SNM_HookThemeColorsMessage(hwnd, uMsg, wParam, lParam))
		return r;

	static BR_NormalizeData* s_normalizeData = NULL;
	static BR_LoudnessObject* s_currentItem  = NULL;

	static int  s_currentItemId      = 0;
	static bool s_analyzeInProgress  = false;
	static double s_itemsLen         = 0;
	static double s_currentItemLen   = 0;
	static double s_finishedItemsLen = 0;

	#ifndef _WIN32
		static bool s_positionSet = false;
	#endif

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			// Reset variables
			s_normalizeData = (BR_NormalizeData*)lParam;
			if (!s_normalizeData || !s_normalizeData->items)
			{
				EndDialog(hwnd, 0);
				return 0;
			}

			s_currentItem = NULL;
			s_currentItemId = 0;
			s_analyzeInProgress = false;
			s_itemsLen = 0;
			s_currentItemLen = 0;
			s_finishedItemsLen = 0;

			// Get progress data
			for (int i = 0; i < s_normalizeData->items->GetSize(); ++i)
			{
				if (BR_LoudnessObject* item = s_normalizeData->items->Get(i))
					s_itemsLen += item->GetAudioLength();
			}
			if (s_itemsLen == 0) s_itemsLen = 1; // to prevent division by zero


			#ifdef _WIN32
				CenterDialog(hwnd, g_hwndParent, HWND_TOPMOST);
			#else
				s_positionSet = false;
			#endif

			// Start normalizing
			SetTimer(hwnd, ANALYZE_TIMER_FREQ, 100, NULL);
		}
		break;

		#ifndef _WIN32
			case WM_ACTIVATE:
			{
				// SetWindowPos doesn't seem to work in WM_INITDIALOG on OSX
				// when creating a dialog with DialogBox so call here
				if (!s_positionSet)
					CenterDialog(hwnd, GetParent(hwnd), HWND_TOPMOST);
				s_positionSet = true;
			}
			break;
		#endif

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDCANCEL:
				{
					KillTimer(hwnd, 1);
					s_normalizeData = NULL;
					if (s_currentItem)
						s_currentItem->AbortAnalyze();
					EndDialog(hwnd, 0);
				}
				break;
			}
		}
		break;

		case WM_TIMER:
		{
			if (!s_normalizeData)
				return 0;

			if (!s_analyzeInProgress)
			{
				// No more objects to analyze, normalize them
				if (s_currentItemId >= s_normalizeData->items->GetSize())
				{
					bool undoTrack = false;
					bool undoItem  = false;
					for (int i = 0; i < s_normalizeData->items->GetSize(); ++i)
					{
						if (BR_LoudnessObject* item = s_normalizeData->items->Get(i))
						{
							if (item->NormalizeIntegrated(s_normalizeData->targetLufs))
							{
								if (!undoTrack && item->IsTrack()) undoTrack = true;
								if (!undoItem && !item->IsTrack()) undoItem = true;
							}
						}
					}

					if (undoTrack || undoItem)
					{
						if (undoTrack && !undoItem)
							Undo_OnStateChangeEx2(NULL, __LOCALIZE("Normalize track loudness", "sws_undo"), UNDO_STATE_TRACKCFG, -1);
						else if (!undoTrack && undoItem)
							Undo_OnStateChangeEx2(NULL, __LOCALIZE("Normalize item loudness", "sws_undo"), UNDO_STATE_ITEMS, -1);
						else
							Undo_OnStateChangeEx2(NULL, __LOCALIZE("Normalize item and track loudness", "sws_undo"), UNDO_STATE_TRACKCFG | UNDO_STATE_ITEMS, -1);
					}

					s_normalizeData->normalized = true;
					UpdateTimeline();
					EndDialog(hwnd, 0);
					return 0;
				}

				// Start analysis of the next item
				if ((s_currentItem = s_normalizeData->items->Get(s_currentItemId)))
				{
					s_currentItemLen = s_currentItem->GetAudioLength();

					// check if user set high precision mode 
					bool doHighPrecisionMode = false;
					if (!s_normalizeData->quickMode) 
					{
						doHighPrecisionMode = !!IsHighPrecisionOptionEnabled(NULL);

					}

					bool doDualMonoMode = !!IsDualMonoOptionEnabled(NULL);
					s_currentItem->Analyze(s_normalizeData->quickMode, false, doHighPrecisionMode, doDualMonoMode);
					s_analyzeInProgress = true;
				}
				else
					++s_currentItemId;
			}
			else
			{
				if (s_currentItem->IsRunning())
				{
					double progress = (s_finishedItemsLen + s_currentItemLen * s_currentItem->GetProgress()) / s_itemsLen;
					SendMessage(GetDlgItem(hwnd, IDC_PROGRESS), PBM_SETPOS, (int)(progress*100), 0);
				}
				else
				{
					s_finishedItemsLen += s_currentItemLen;
					double progress = s_finishedItemsLen / s_itemsLen;
					SendMessage(GetDlgItem(hwnd, IDC_PROGRESS), PBM_SETPOS, (int)(progress*100), 0);

					s_analyzeInProgress = false;
					++s_currentItemId;
				}
			}
		}
		break;

		case WM_DESTROY:
		{
			KillTimer(hwnd, 1);
			s_normalizeData = NULL;
			if (s_currentItem)
				s_currentItem->AbortAnalyze();
			s_analyzeInProgress = false;
		}
		break;
	}
	return 0;
}

void NormalizeAndShowProgress (BR_NormalizeData* normalizeData)
{
	static bool s_normalizeInProgress = false;

	if (!s_normalizeInProgress)
	{
		// Kill all normalize dialogs prior to normalizing
		if (BR_AnalyzeLoudnessWnd* dialog = g_loudnessWndManager.Get())
			dialog->KillNormalizeDlg();
		if (g_normalizeWnd)
		{
			DestroyWindow(g_normalizeWnd);
			g_normalizeWnd = NULL;
			RefreshToolbar(NamedCommandLookup("_BR_NORMALIZE_LOUDNESS_ITEMS"));
		}

		s_normalizeInProgress = true;
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_BR_LOUDNESS_NORM_PROGRESS), g_hwndParent, NormalizeProgressProc, (LPARAM)normalizeData);
		s_normalizeInProgress = false;
	}
}

/******************************************************************************
* Analyze loudness list view                                                  *
******************************************************************************/
BR_AnalyzeLoudnessView::BR_AnalyzeLoudnessView (HWND hwndList, HWND hwndEdit)
:SWS_ListView(hwndList, hwndEdit, COL_COUNT, g_cols, LOUDNESS_VIEW_WND, false, "sws_DLG_174")
{
}

void BR_AnalyzeLoudnessView::GetItemText (SWS_ListItem* item, int iCol, char* str, int iStrMax)
{
	WritePtr(str, '\0');
	if (BR_LoudnessObject* listItem = (BR_LoudnessObject*)item)
		listItem->GetColumnStr(iCol, str, iStrMax, g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::USING_LU));
}

void BR_AnalyzeLoudnessView::GetItemList (SWS_ListItemList* pList)
{
	for (int i = 0; i < g_analyzedObjects.Get()->GetSize(); ++i)
		pList->Add((SWS_ListItem*)g_analyzedObjects.Get()->Get(i));
}

void BR_AnalyzeLoudnessView::OnItemSelChanged (SWS_ListItem* item, int iState)
{
	if (g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::MIRROR_PROJ_SELECTION) && item)
	{
		BR_LoudnessObject* listItem = (BR_LoudnessObject*)item;
		listItem->SetSelectedInProject(!!iState);
	}
	g_loudnessWndManager.Get()->Update(false);
}

void BR_AnalyzeLoudnessView::OnItemDblClk (SWS_ListItem* item, int iCol)
{
	if (BR_LoudnessObject* listItem = (BR_LoudnessObject*)item)
	{
		if (iCol == COL_SHORTTERM)
			listItem->GoToShortTermMax(g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::TIME_SEL_OVER_MAX));
		else if (iCol == COL_MOMENTARY)
			listItem->GoToMomentaryMax(g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::TIME_SEL_OVER_MAX));
		else if (iCol == COL_TRUEPEAK)
			listItem->GoToTruePeak();
		else if (g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::DOUBLECLICK_GOTO_TARGET))
			listItem->GoToTarget();
	}
}

int BR_AnalyzeLoudnessView::OnItemSort (SWS_ListItem* item1, SWS_ListItem* item2)
{
	int column = abs(m_iSortCol)-1;

	if ((item1 && item2) && (column == COL_INTEGRATED || column == COL_RANGE || column == COL_TRUEPEAK || column == COL_SHORTTERM || column == COL_MOMENTARY))
	{
		double i1 = ((BR_LoudnessObject*)item1)->GetColumnVal(column, g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::USING_LU));
		double i2 = ((BR_LoudnessObject*)item2)->GetColumnVal(column, g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::USING_LU));

		int iRet = 0;
		if (i1 > i2)      iRet = 1;
		else if (i1 < i2) iRet = -1;

		if (m_iSortCol < 0) return -iRet;
		else                return  iRet;
	}
	else
		return SWS_ListView::OnItemSort(item1, item2);
}

void BR_AnalyzeLoudnessView::OnItemSortEnd ()
{
	if (g_loudnessWndManager.Get()) // prevent crash on reaper startup (calling object still in construction)
		g_loudnessWndManager.Get()->Update(false);
}

/******************************************************************************
* Analyze loudness window                                                     *
******************************************************************************/
BR_AnalyzeLoudnessWnd::BR_AnalyzeLoudnessWnd () :
SWS_DockWnd(IDD_BR_LOUDNESS_ANALYZER, __LOCALIZE("Loudness", "sws_DLG_174"), ""),
m_objectsLen        (0),
m_currentObjectId   (0),
m_analyzeInProgress (false),
m_list              (NULL),
m_normalizeWnd      (NULL),
m_exportFormatWnd   (NULL)
{
	m_id.Set(LOUDNESS_WND);
	Init(); // Must call SWS_DockWnd::Init() to restore parameters and open the window if necessary
}

BR_AnalyzeLoudnessWnd::~BR_AnalyzeLoudnessWnd ()
{
}

void BR_AnalyzeLoudnessWnd::Update (bool updateList /*=true*/)
{
	if (this->IsValidWindow())
	{
		if (updateList)
			m_list->Update();

		if (m_properties.analyzeTracks)
			SetDlgItemText(m_hwnd, IDC_ANALYZE, __LOCALIZE("Analyze selected tracks", "sws_DLG_174"));
		else
			SetDlgItemText(m_hwnd, IDC_ANALYZE, __LOCALIZE("Analyze selected items", "sws_DLG_174"));

		if (m_normalizeWnd)
			SendMessage(m_normalizeWnd, WM_COMMAND, BR_AnalyzeLoudnessWnd::READ_PROJDATA, 0);
		if (m_exportFormatWnd)
			SendMessage(m_exportFormatWnd, WM_COMMAND, BR_AnalyzeLoudnessWnd::UPDATE_FORMAT_AND_PREVIEW, 0);
	}
}

void BR_AnalyzeLoudnessWnd::KillNormalizeDlg ()
{
	this->ShowNormalizeDialog(false);
}

bool BR_AnalyzeLoudnessWnd::GetProperty (int propertySpecifier)
{
	switch (propertySpecifier)
	{
		case TIME_SEL_OVER_MAX:       return m_properties.timeSelOverMax;
		case DOUBLECLICK_GOTO_TARGET: return m_properties.doubleClickGoToTarget;
		case USING_LU:                return m_properties.usingLU;
		case MIRROR_PROJ_SELECTION:   return m_properties.mirrorProjSelection;
		case DO_HIGH_PRECISION_MODE:  return m_properties.doHighPrecisionMode;
		case DO_DUAL_MONO_MODE:       return m_properties.doDualMonoMode;
	}
	return false;
}

bool BR_AnalyzeLoudnessWnd::SetProperty(int propertySpecifier, bool newVal)
{
	switch (propertySpecifier)
	{
		// case TIME_SEL_OVER_MAX:       m_properties.timeSelOverMax = newVal; return true;
		// case DOUBLECLICK_GOTO_TARGET: m_properties.doubleClickGoToTarget = newVal; return true;
		// case USING_LU:                m_properties.usingLU = newVal; return true;
		// case MIRROR_PROJ_SELECTION:   m_properties.mirrorProjSelection = newVal; return true;
		case DO_HIGH_PRECISION_MODE:     m_properties.doHighPrecisionMode = newVal; return true;
		case DO_DUAL_MONO_MODE:          m_properties.doDualMonoMode      = newVal; return true;
	}
	return false;
}

BR_LoudnessObject* BR_AnalyzeLoudnessWnd::IsObjectInList (MediaTrack* track)
{
	for (int i = 0; i < g_analyzedObjects.Get()->GetSize(); ++i)
	{
		if (BR_LoudnessObject* object = g_analyzedObjects.Get()->Get(i))
		{
			if (object->CheckTarget(track))
				return object;
		}
	}
	return NULL;
}

BR_LoudnessObject* BR_AnalyzeLoudnessWnd::IsObjectInList (MediaItem_Take* take)
{
	for (int i = 0; i < g_analyzedObjects.Get()->GetSize(); ++i)
	{
		if (BR_LoudnessObject* object = g_analyzedObjects.Get()->Get(i))
		{
			if (object->CheckTarget(take))
				return object;
		}
	}
	return NULL;
}

void BR_AnalyzeLoudnessWnd::AbortAnalyze ()
{
	SetAnalyzing(false, false);

	// Make sure objects already in the list are NOT destroyed
	for (int i = 0; i < m_analyzeQueue.GetSize(); ++i)
	{
		if (g_analyzedObjects.Get()->Find(m_analyzeQueue.Get(i)) != -1)
			m_analyzeQueue.Delete(i--, false);
	}
	m_analyzeQueue.Empty(true);
	m_objectsLen      = 0;
	m_currentObjectId = 0;
}

void BR_AnalyzeLoudnessWnd::AbortReanalyze ()
{
	SetAnalyzing(false, true);

	m_reanalyzeQueue.Empty(false);
	m_objectsLen      = 0;
	m_currentObjectId = 0;
}

void BR_AnalyzeLoudnessWnd::SetAnalyzing (const bool analyzing, const bool reanalyze)
{
	ShowWindow(GetDlgItem(m_hwnd, IDC_PROGRESS), analyzing ? SW_SHOW : SW_HIDE);
	SendMessage(GetDlgItem(m_hwnd, IDC_PROGRESS), PBM_SETPOS, 0, 0);
	EnableWindow(GetDlgItem(m_hwnd, IDC_ANALYZE), !analyzing);
	EnableWindow(GetDlgItem(m_hwnd, IDC_CANCEL), analyzing);

	const int timer = reanalyze ? REANALYZE_TIMER : ANALYZE_TIMER;

	if (analyzing)
		SetTimer(m_hwnd, timer, ANALYZE_TIMER_FREQ, NULL);
	else {
		KillTimer(m_hwnd, timer);

		m_analyzeInProgress = false;
	}
}

void BR_AnalyzeLoudnessWnd::ClearList ()
{
	// Make sure objects in analyze queue are not destroyed (prior to clearing the list, existing objects are put there to make analysis faster)
	for (int i = 0; i < m_analyzeQueue.GetSize(); ++i)
	{
		int id = g_analyzedObjects.Get()->Find(m_analyzeQueue.Get(i));
		if (id != -1)
			 g_analyzedObjects.Get()->Delete(id, false);
	}
	g_analyzedObjects.Get()->Empty(true);
	this->Update();
}

void BR_AnalyzeLoudnessWnd::ShowExportFormatDialog (bool show)
{
	if (show)
	{
		if (!m_exportFormatWnd)
		{
			m_exportFormatWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_BR_LOUDNESS_EXPORT_FORMAT), m_hwnd, this->ExportFormatDialogProc);
			#ifndef _WIN32
				SWELL_SetWindowLevel(m_exportFormatWnd, 3); // NSFloatingWindowLevel
			#endif
		}
		else
		{
			ShowWindow(m_exportFormatWnd, SW_SHOW);
			SetFocus(m_exportFormatWnd);
		}
	}
	else
	{
		if (m_exportFormatWnd)
		{
			DestroyWindow(m_exportFormatWnd);
			m_exportFormatWnd = NULL;
		}
	}
}

void BR_AnalyzeLoudnessWnd::ShowNormalizeDialog (bool show)
{
	if (show)
	{
		if (!m_normalizeWnd)
		{
			m_normalizeWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_BR_LOUDNESS_ANALYZER_NORM), m_hwnd, this->NormalizeDialogProc);
			#ifndef _WIN32
				SWELL_SetWindowLevel(m_normalizeWnd, 3); // NSFloatingWindowLevel
			#endif
		}
		else
		{
			ShowWindow(m_normalizeWnd, SW_SHOW);
			SetFocus(m_normalizeWnd);
		}
	}
	else
	{
		if (m_normalizeWnd)
		{
			DestroyWindow(m_normalizeWnd);
			m_normalizeWnd = NULL;
		}
	}
}

void BR_AnalyzeLoudnessWnd::LoadProperties()
{
	if (g_loudnessWndManager.Create())
	{
		m_properties.Load();
	}
}

void BR_AnalyzeLoudnessWnd::SaveProperties()
{
	if (g_loudnessWndManager.Create())
	{
		m_properties.Save();
	}
}

void BR_AnalyzeLoudnessWnd::SaveRecentFormatPattern (WDL_FastString pattern)
{
	bool patternNeedsSaving = false;

	int x = 0;
	while (const char* wildcard = g_wildcards[x++].wildcard)
	{
		if (strstr(pattern.Get(), wildcard))
		{
			patternNeedsSaving = true;
			break;
		}
	}

	if (patternNeedsSaving)
	{
		WDL_PtrList_DeleteOnDestroy<WDL_FastString> patternsToSave;

		int i = 0;
		while (true)
		{
			WDL_FastString currentPattern = this->GetRecentFormatPattern(i++);
			if (currentPattern.GetLength())
			{
				// If pattern already exists don't save it - it will get appended on the top
				if (strcmp(currentPattern.Get(), pattern.Get()))
					patternsToSave.Add(new WDL_FastString(currentPattern));
			}
			else
				break;

			if (patternsToSave.GetSize() >= EXPORT_FORMAT_RECENT_MAX)
			{
				patternsToSave.Delete(0, true);
				break;
			}
		}
		patternsToSave.Insert(0, new WDL_FastString(pattern));

		for (int j = 0 ; j < patternsToSave.GetSize(); ++j)
			WritePrivateProfileString("SWS", this->GetRecentFormatPatternKey(j).Get(), patternsToSave.Get(j)->Get(), get_ini_file());
	}
}

WDL_FastString BR_AnalyzeLoudnessWnd::GetRecentFormatPattern (int id)
{
	char tmp[256];
	GetPrivateProfileString("SWS", this->GetRecentFormatPatternKey(id).Get(), "", tmp, sizeof(tmp), get_ini_file());

	WDL_FastString pattern;
	pattern.Set(tmp);
	return pattern;
}

WDL_FastString BR_AnalyzeLoudnessWnd::GetRecentFormatPatternKey (int id)
{
	WDL_FastString key;
	key.AppendFormatted(256, "%s%.2d", EXPORT_FORMAT_RECENT, id);
	return key;
}

WDL_FastString BR_AnalyzeLoudnessWnd::CreateExportString (bool previewOnly)
{
	WDL_PtrList<BR_LoudnessObject> objects;
	WDL_PtrList_DeleteOnDestroy<WDL_FastString> strings;
	if (previewOnly)
	{
		int x = 0;
		if (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->EnumSelected(&x))
			objects.Add(listItem);
		else if (m_list->GetListItemCount())
		{
			if (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->GetListItem(0))
				objects.Add(listItem);
		}
		strings.Add(new WDL_FastString());
	}
	else
	{
		int x = 0;
		while (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->EnumSelected(&x))
		{
			objects.Add(listItem);
			strings.Add(new WDL_FastString());
		}
	}

	if (objects.GetSize())
	{
		int luMode = m_properties.usingLU ? 1 : 0;
		const char* format = m_properties.exportFormat.Get();
		while (*format != '\0')
		{
			bool foundWildcard = false;
			int  step = 1;

			// Possible wildcard...
			if (!strncmp(format, "$", 1))
			{
				// keep alphabetically sorted with longer names first (itemnumber before item)
				if ((foundWildcard = !strncmp(format, "$$", strlen("$$"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->Append("$");
					step = strlen("$$");
				}
				else if ((foundWildcard = !strncmp(format, "$end", strlen("$end"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(128, "%.3lf", objects.Get(i)->GetAudioEnd());
					step = strlen("$end");
				}
				else if ((foundWildcard = !strncmp(format, "$id", strlen("$id"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						char tmp[512];
						objects.Get(i)->GetColumnStr(COL_ID, tmp, sizeof(tmp), luMode);
						strings.Get(i)->Append(tmp);
					}
					step = strlen("$id");
				}
				else if ((foundWildcard = !strncmp(format, "$integrated", strlen("$integrated"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						char tmp[512];
						objects.Get(i)->GetColumnStr(COL_INTEGRATED, tmp, sizeof(tmp), luMode);
						strings.Get(i)->Append(tmp);
					}
					step = strlen("$integrated");
				}
				else if ((foundWildcard = !strncmp(format, "$itemnumber", strlen("$itemnumber"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(128, "%d", objects.Get(i)->GetItemNumber());
					step = strlen("$itemnumber");
				}
				else if ((foundWildcard = !strncmp(format, "$item", strlen("$item"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						char tmp[512];
						objects.Get(i)->GetColumnStr(COL_TAKE, tmp, sizeof(tmp), luMode);
						strings.Get(i)->Append(tmp);
					}
					step = strlen("$item");
				}
				else if ((foundWildcard = !strncmp(format, "$length", strlen("$length"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
							strings.Get(i)->AppendFormatted(128, "%.3lf", objects.Get(i)->GetAudioLength());
					step = strlen("$length");
				}
				else if ((foundWildcard = !strncmp(format, "$luformat", strlen("$luformat"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->Append(g_pref.GetFormatedLUString().Get());
					step = strlen("$luformat");
				}
				else if ((foundWildcard = !strncmp(format, "$lureference", strlen("$lureference"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(128, "%g %s", g_pref.GetReferenceLU(), __LOCALIZE("LUFS", "sws_loudness"));
					step = strlen("$lureference");
				}
				else if ((foundWildcard = !strncmp(format, "$maxmomentaryposproj", strlen("$maxmomentaryposproj"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(256, "%.3lf", objects.Get(i)->GetMaxMomentaryPos(true));
					step = strlen("$maxmomentaryposproj");
				}
				else if ((foundWildcard = !strncmp(format, "$maxmomentarypos", strlen("$maxmomentarypos"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(256, "%.3lf", objects.Get(i)->GetMaxMomentaryPos(false));
					step = strlen("$maxmomentarypos");
				}
				else if ((foundWildcard = !strncmp(format, "$maxmomentary", strlen("$maxmomentary"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						char tmp[512];
						objects.Get(i)->GetColumnStr(COL_MOMENTARY, tmp, sizeof(tmp), luMode);
						strings.Get(i)->Append(tmp);
					}
					step = strlen("$maxmomentary");
				}
				else if ((foundWildcard = !strncmp(format, "$maxshortposproj", strlen("$maxshortposproj"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(256, "%.3lf", objects.Get(i)->GetMaxShorttermPos(true));
					step = strlen("$maxshortposproj");
				}
				else if ((foundWildcard = !strncmp(format, "$maxshortpos", strlen("$maxshortpos"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(256, "%.3lf", objects.Get(i)->GetMaxShorttermPos(false));
					step = strlen("$maxshortpos");
				}
				else if ((foundWildcard = !strncmp(format, "$maxshort", strlen("$maxshort"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						char tmp[512];
						objects.Get(i)->GetColumnStr(COL_SHORTTERM, tmp, sizeof(tmp), luMode);
						strings.Get(i)->Append(tmp);
					}
					step = strlen("$maxshort");
				}
				else if ((foundWildcard = !strncmp(format, "$n", strlen("$n"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						if (previewOnly)
							strings.Get(i)->Append(" ");
						else
							#ifdef _WIN32
								strings.Get(i)->Append("\r\n");
							#else
								strings.Get(i)->Append("\n");
							#endif
					}
					step = strlen("$n");
				}
				else if ((foundWildcard = !strncmp(format, "$range", strlen("$range"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						char tmp[512];
						objects.Get(i)->GetColumnStr(COL_RANGE, tmp, sizeof(tmp), luMode);
						strings.Get(i)->Append(tmp);
					}
					step = strlen("$range");
				}
				else if ((foundWildcard = !strncmp(format, "$start", strlen("$start"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(128, "%.3lf", objects.Get(i)->GetAudioStart());
					step = strlen("$start");
				}
				else if ((foundWildcard = !strncmp(format, "$targetnumber", strlen("$targetnumber"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(128, "%d", objects.Get(i)->IsTrack() ? objects.Get(i)->GetTrackNumber() : objects.Get(i)->GetItemNumber());
					step = strlen("$targetnumber");
				}
				else if ((foundWildcard = !strncmp(format, "$target", strlen("$target"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						char tmp[512];
						objects.Get(i)->GetColumnStr(objects.Get(i)->IsTrack() ? COL_TRACK : COL_TAKE, tmp, sizeof(tmp), luMode);
						strings.Get(i)->Append(tmp);
					}
					step = strlen("$target");
				}
				else if ((foundWildcard = !strncmp(format, "$tracknumber", strlen("$tracknumber"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(128, "%d", objects.Get(i)->GetTrackNumber());
					step = strlen("$tracknumber");
				}
				else if ((foundWildcard = !strncmp(format, "$track", strlen("$track"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						char tmp[512];
						objects.Get(i)->GetColumnStr(COL_TRACK, tmp, sizeof(tmp), luMode);
						strings.Get(i)->Append(tmp);
					}
					step = strlen("$track");
				}
				else if ((foundWildcard = !strncmp(format, "$truepeakposproj", strlen("$truepeakposproj"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(256, "%.3lf", objects.Get(i)->GetTruePeakPos(true));
					step = strlen("$truepeakposproj");
				}
				else if ((foundWildcard = !strncmp(format, "$truepeakpos", strlen("$truepeakpos"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
						strings.Get(i)->AppendFormatted(256, "%.3lf", objects.Get(i)->GetTruePeakPos(false));
					step = strlen("$truepeakpos");
				}
				else if ((foundWildcard = !strncmp(format, "$truepeak", strlen("$truepeak"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						char tmp[512];
						objects.Get(i)->GetColumnStr(COL_TRUEPEAK, tmp, sizeof(tmp), luMode);
						strings.Get(i)->Append(tmp);
					}
					step = strlen("$truepeak");
				}
				else if ((foundWildcard = !strncmp(format, "$t", strlen("$t"))))
				{
					for (int i = 0; i < strings.GetSize(); ++i)
					{
						if (previewOnly)
							strings.Get(i)->Append(" ");
						else
							strings.Get(i)->Append("\t");
					}
					step = strlen("$t");
				}
			}

			if (!foundWildcard)
			{
				for (int i = 0; i < strings.GetSize(); ++i)
					strings.Get(i)->Append(format, 1);
			}

			format += step;
		}
	}

	WDL_FastString returnString;
	for (int i = 0; i < strings.GetSize(); ++i)
		AppendLine(returnString, strings.Get(i)->Get());
	return returnString;
}

WDL_DLGRET BR_AnalyzeLoudnessWnd::ExportFormatDialogProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (INT_PTR r = SNM_HookThemeColorsMessage(hwnd, uMsg, wParam, lParam))
		return r;

	static WDL_WndSizer s_resize;
	static WDL_FastString s_originalFormat;

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			s_originalFormat.Set(g_loudnessWndManager.Get()->m_properties.exportFormat.Get());

			AttachWindowResizeGrip(hwnd);
			s_resize.init(hwnd);
			s_resize.init_item(IDC_EDIT,           0.0, 0.0, 1.0, 1.0);
			s_resize.init_item(IDC_FORMAT_STATIC,  0.0, 1.0, 0.0, 1.0);
			s_resize.init_item(IDC_PREVIEW_STATIC, 0.0, 1.0, 0.0, 1.0);
			s_resize.init_item(IDC_FORMAT,         0.0, 1.0, 1.0, 1.0);
			s_resize.init_item(IDC_PREVIEW,        0.0, 1.0, 1.0, 1.0);
			s_resize.init_item(IDC_WILDCARDS,      1.0, 1.0, 1.0, 1.0);
			s_resize.init_item(IDOK,               1.0, 1.0, 1.0, 1.0);
			s_resize.init_item(IDCANCEL,           1.0, 1.0, 1.0, 1.0);

			WDL_FastString helpString;
			helpString.Append(__LOCALIZE("All wildcards related to loudness measurements will follow list view formating (if integrated loudness is displayed as -23 LUFS that's exactly what $integrated will create).\r\n\r\nWildcards description:\r\n", "sws_DLG_180"));

			#ifdef _WIN32
				LICE_CachedFont font;
				font.SetFromHFont((HFONT)SendMessage(GetDlgItem(hwnd, IDC_EDIT), WM_GETFONT, 0, 0), LICE_FONT_FLAG_FORCE_NATIVE);

				// Get the length of the longest wildcard
				int i = -1;
				int maxLen = 0;
				while (g_wildcards[++i].wildcard)
				{
					RECT r = {0, 0, 0, 0};
					font.DrawText(NULL, g_wildcards[i].wildcard, -1, &r, DT_CALCRECT | DT_NOPREFIX | DT_EDITCONTROL);
					if (maxLen < r.right) maxLen = r.right;
				}

				// Format help string so wildcards and their descriptions are vertically aligned
				i = -1;
				while (g_wildcards[++i].wildcard)
				{
					WDL_FastString string;
					string.Append(g_wildcards[i].wildcard);

					int currentLen = 0;
					while (currentLen <= maxLen)
					{
						string.Append("\t");

						RECT r = {0, 0, 0, 0};
						font.DrawText(NULL, string.Get(), -1, &r, DT_CALCRECT | DT_NOPREFIX | DT_EDITCONTROL | DT_EXPANDTABS);
						if (maxLen < r.right) currentLen = r.right;
					}

					helpString.Append(string.Get());
					helpString.Append(__localizeFunc(g_wildcards[i].desc, "sws_DLG_180", 0));
					helpString.Append("\r\n");
				}
			#else
				int i = -1;
				while (g_wildcards[++i].wildcard)
				{
					helpString.Append(g_wildcards[i].wildcard);
					for (int j = 0; j < g_wildcards[i].osxTabs; ++j)
						helpString.Append("\t");
					helpString.Append(__localizeFunc(g_wildcards[i].desc, "sws_DLG_180", 0));
					helpString.Append("\r\n");
				}
			#endif

			SetWindowText(GetDlgItem(hwnd, IDC_EDIT), helpString.Get());
			SetWindowText(GetDlgItem(hwnd, IDC_FORMAT), g_loudnessWndManager.Get()->m_properties.exportFormat.Get());
			SendMessage(hwnd, WM_COMMAND, BR_AnalyzeLoudnessWnd::UPDATE_FORMAT_AND_PREVIEW, 0);

			RestoreWindowPos(hwnd, EXPORT_FORMAT_WND, true);
			ShowWindow(hwnd, SW_SHOW);
			SetFocus(GetDlgItem(hwnd, IDC_FORMAT));
		}
		break;

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case BR_AnalyzeLoudnessWnd::UPDATE_FORMAT_AND_PREVIEW:
				{
					char format[2048];
					GetDlgItemText(hwnd, IDC_FORMAT, format, sizeof(format));
					g_loudnessWndManager.Get()->m_properties.exportFormat.Set(format);

					WDL_FastString previewString = g_loudnessWndManager.Get()->CreateExportString(true);
					SetDlgItemText(hwnd, IDC_PREVIEW, previewString.Get());
				}
				break;

				case IDC_WILDCARDS:
				{
					HWND buttonHwnd = GetDlgItem(hwnd, IDC_WILDCARDS);
					RECT r;  GetClientRect(buttonHwnd, &r);
					ClientToScreen(buttonHwnd, (LPPOINT)&r);
					ClientToScreen(buttonHwnd, ((LPPOINT)&r)+1);
					SendMessage(hwnd, WM_CONTEXTMENU, 0, MAKELPARAM((UINT)(r.left), (UINT)(r.bottom+SNM_1PIXEL_Y)));
				}
				break;

				case IDC_FORMAT:
				{
					if (HIWORD(wParam) == EN_CHANGE && GetFocus() == GetDlgItem(hwnd, IDC_FORMAT))
						SendMessage(hwnd, WM_COMMAND, BR_AnalyzeLoudnessWnd::UPDATE_FORMAT_AND_PREVIEW, 0);
				}
				break;

				case IDOK:
				{
					char format[2048];
					GetDlgItemText(hwnd, IDC_FORMAT, format, sizeof(format));
					s_originalFormat.Set(format);
					g_loudnessWndManager.Get()->SaveRecentFormatPattern(s_originalFormat);

					g_loudnessWndManager.Get()->ShowExportFormatDialog(false);
				}
				break;

				case IDCANCEL:
				{
					g_loudnessWndManager.Get()->m_properties.exportFormat.Set(s_originalFormat.Get());
					g_loudnessWndManager.Get()->ShowExportFormatDialog(false);
				}
				break;
			}
		}
		break;

		case WM_SIZE:
		{
			if (wParam != SIZE_MINIMIZED)
				s_resize.onResize();
		}
		break;

		case WM_GETMINMAXINFO:
		{
			if (lParam)
			{
				LPMINMAXINFO l = (LPMINMAXINFO)lParam;
				l->ptMinTrackSize.x = 254;
				l->ptMinTrackSize.y = 147;
			}
		}
		break;

		case WM_CONTEXTMENU:
		{
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);
			RECT r;	GetWindowRect(GetDlgItem(hwnd, IDC_WILDCARDS), &r);
			POINT pt = {x, y + 3*(SNM_1PIXEL_Y*(-1))};

			if (PtInRect(&r, pt))
			{
				HMENU menu = CreatePopupMenu();
				int i = 0;

				// Add wildcards
				while (const char* wildcard = g_wildcards[i++].wildcard)
					AddToMenu(menu, wildcard, i);
				AddToMenu(menu, SWS_SEPARATOR, 0);

				// Create recent wildcards submenu
				int firstRecent = i;
				HMENU menuRecent = CreatePopupMenu();
				while (true)
				{
					WDL_FastString pattern = g_loudnessWndManager.Get()->GetRecentFormatPattern(i - firstRecent);
					if (pattern.GetLength())
						AddToMenu(menuRecent, pattern.Get(), i, -1, false);
					else
						break;
					++i;
				}
				AddSubMenu(menu, menuRecent, __LOCALIZE("Recent patterns", "sws_DLG_180"), -1, i == firstRecent ? MF_DISABLED : MF_ENABLED);

				int wildcard = TrackPopupMenu(menu, TPM_RETURNCMD, x, y, 0, hwnd, NULL);
				if (wildcard > 0)
				{
					if (wildcard < firstRecent)
					{
						char format[2048];
						GetDlgItemText(hwnd, IDC_FORMAT, format, sizeof(format));
						DWORD start = 0, end = 0;
						SendMessage(GetDlgItem(hwnd, IDC_FORMAT), EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

						WDL_FastString newFormat;
						newFormat.AppendFormatted(start, "%s", format);
						newFormat.Append(g_wildcards[wildcard - 1].wildcard);
						newFormat.Append(format + end);

						end = start + strlen(g_wildcards[wildcard - 1].wildcard);
						SetDlgItemText(hwnd, IDC_FORMAT, newFormat.Get());
						SendMessage(GetDlgItem(hwnd, IDC_FORMAT), EM_SETSEL, end, end);
					}
					else
					{
						WDL_FastString newFormat = g_loudnessWndManager.Get()->GetRecentFormatPattern(wildcard - firstRecent);
						SetDlgItemText(hwnd, IDC_FORMAT, newFormat.Get());
						SendMessage(GetDlgItem(hwnd, IDC_FORMAT), EM_SETSEL, newFormat.GetLength(), newFormat.GetLength());
					}

					SendMessage(hwnd, WM_COMMAND, BR_AnalyzeLoudnessWnd::UPDATE_FORMAT_AND_PREVIEW, 0);
					SetFocus(GetDlgItem(hwnd, IDC_FORMAT));
				}

				DestroyMenu(menu);
			}
		}
		break;

		case WM_DESTROY:
		{
			g_loudnessWndManager.Get()->m_properties.exportFormat.Set(s_originalFormat.Get());

			s_resize.init(NULL);
			s_originalFormat.SetLen(0, true);
			SaveWindowPos(hwnd, EXPORT_FORMAT_WND);
		}
		break;
	}
	return 0;
}

WDL_DLGRET BR_AnalyzeLoudnessWnd::NormalizeDialogProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (INT_PTR r = SNM_HookThemeColorsMessage(hwnd, uMsg, wParam, lParam))
		return r;

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			// Get data
			char tmp[256];
			GetPrivateProfileString("SWS", NORMALIZE_KEY, "", tmp, sizeof(tmp), get_ini_file());

			LineParser lp(false);
			lp.parse(tmp);
			double value = (lp.getnumtokens() > 0) ? lp.gettoken_float(0) : -23;
			int unit     = (lp.getnumtokens() > 1) ? lp.gettoken_int(1)   : 0;

			// Set controls
			WDL_UTF8_HookComboBox(GetDlgItem(hwnd, IDC_UNIT));
			SendDlgItemMessage(hwnd, IDC_UNIT, CB_ADDSTRING, 0, (LPARAM)__LOCALIZE("LUFS", "sws_loudness"));
			SendDlgItemMessage(hwnd, IDC_UNIT, CB_ADDSTRING, 0, (LPARAM)g_pref.GetFormatedLUString().Get());
			SendDlgItemMessage(hwnd, IDC_UNIT, CB_SETCURSEL, unit, 0);
			snprintf(tmp, sizeof(tmp), "%.6g", value); SetDlgItemText(hwnd, IDC_VALUE, tmp);
			SetFocus(GetDlgItem(hwnd, IDC_VALUE));
			SendMessage(GetDlgItem(hwnd, IDC_VALUE), EM_SETSEL, 0, -1);

			RestoreWindowPos(hwnd, NORMALIZE_WND, false);
			ShowWindow(hwnd, SW_SHOW);
			SetFocus(hwnd);
		}
		break;

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case BR_AnalyzeLoudnessWnd::READ_PROJDATA:
				{
					int unit = (int)SendDlgItemMessage(hwnd, IDC_UNIT, CB_GETCURSEL, 0, 0);
					SendDlgItemMessage(hwnd, IDC_UNIT, CB_DELETESTRING, 1, 0);
					SendDlgItemMessage(hwnd, IDC_UNIT, CB_ADDSTRING, 0, (LPARAM)g_pref.GetFormatedLUString().Get());
					SendDlgItemMessage(hwnd, IDC_UNIT, CB_SETCURSEL, unit, 0);
				}
				break;

				case IDC_UNIT:
				{
					if (HIWORD(wParam) == CBN_SELCHANGE)
					{
						int unit = (int)SendDlgItemMessage(hwnd, IDC_UNIT, CB_GETCURSEL, 0, 0);
						char value[256]; GetDlgItemText(hwnd, IDC_VALUE, value, sizeof(value));

						snprintf(value, sizeof(value), "%.6g", (unit == 1) ? g_pref.LUFStoLU(AltAtof(value)) : g_pref.LUtoLUFS(AltAtof(value)));
						SetDlgItemText(hwnd, IDC_VALUE, value);
					}
				}
				break;

				case IDOK:
				{
					BR_AnalyzeLoudnessWnd* dialog = g_loudnessWndManager.Get();

					WDL_PtrList<BR_LoudnessObject> itemsToNormalize;
					int x = 0;
					while (BR_LoudnessObject* listItem = (BR_LoudnessObject*)dialog->m_list->EnumSelected(&x))
						itemsToNormalize.Add(listItem);
					BR_NormalizeData normalizeData = {&itemsToNormalize, 0, false, false};

					int unit = (int)SendDlgItemMessage(hwnd, IDC_UNIT, CB_GETCURSEL, 0, 0);
					char value[256]; GetDlgItemText(hwnd, IDC_VALUE, value, sizeof(value));
					normalizeData.targetLufs = (unit == 0) ? AltAtof(value) : g_pref.LUtoLUFS(AltAtof(value));

					if (itemsToNormalize.GetSize())
					{
						dialog->ShowNormalizeDialog(false);
						dialog->AbortAnalyze();
						dialog->AbortReanalyze();
						dialog->Update();

						NormalizeAndShowProgress(&normalizeData);
						dialog->Update();
						if (dialog->m_properties.analyzeOnNormalize && normalizeData.normalized)
							dialog->OnCommand(REANALYZE_ITEMS, 0);
					}
				}
				break;

				case IDCANCEL:
				{
					g_loudnessWndManager.Get()->ShowNormalizeDialog(false);
				}
				break;
			}
		}
		break;

		case WM_DESTROY:
		{
			int unit = (int)SendDlgItemMessage(hwnd, IDC_UNIT, CB_GETCURSEL, 0, 0);
			char value[256]; GetDlgItemText(hwnd, IDC_VALUE, value, sizeof(value));

			// Third parameter is used by dialog invoked by normalize command, get it before storing new data
			char tmp[342];
			GetPrivateProfileString("SWS", NORMALIZE_KEY, "", tmp, sizeof(tmp), get_ini_file());

			LineParser lp(false);
			lp.parse(tmp);
			int target = (lp.getnumtokens() > 2) ? lp.gettoken_int(2) : 0;

			snprintf(tmp, sizeof(tmp), "%lf %d %d", AltAtof(value), unit, target);
			WritePrivateProfileString("SWS", NORMALIZE_KEY, tmp, get_ini_file());

			SaveWindowPos(hwnd, NORMALIZE_WND);
		}
		break;
	}
	return 0;
}

void BR_AnalyzeLoudnessWnd::OnInitDlg ()
{
	m_parentVwnd.SetRealParent(m_hwnd);
	m_vwnd_painter.SetGSC(WDL_STYLE_GetSysColor);

	m_properties.Load();

	m_resize.init_item(IDC_LIST, 0.0, 0.0, 1.0, 1.0);
	m_resize.init_item(IDC_PROGRESS, 0.0, 1.0, 1.0, 1.0);
	m_resize.init_item(IDC_ANALYZE, 0.0, 1.0, 0.0, 1.0);
	m_resize.init_item(IDC_CANCEL, 0.0, 1.0, 0.0, 1.0);
	m_resize.init_item(IDC_OPTIONS, 1.0, 1.0, 1.0, 1.0);
	ShowWindow(GetDlgItem(m_hwnd, IDC_PROGRESS), SW_HIDE);

	m_list = new BR_AnalyzeLoudnessView(GetDlgItem(m_hwnd, IDC_LIST), GetDlgItem(m_hwnd, IDC_EDIT));
	m_pLists.Add(m_list);
	SetTimer(m_hwnd, UPDATE_TIMER, UPDATE_TIMER_FREQ, NULL);

	EnableWindow(GetDlgItem(m_hwnd, IDC_CANCEL), false);

	this->Update();
}

void BR_AnalyzeLoudnessWnd::OnCommand (WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam))
	{
		case IDC_OPTIONS:
		{
			HWND hwnd = GetDlgItem(m_hwnd, IDC_OPTIONS);
			RECT r;  GetClientRect(hwnd, &r);
			ClientToScreen(hwnd, (LPPOINT)&r);
			ClientToScreen(hwnd, ((LPPOINT)&r)+1);
			SendMessage(m_hwnd, WM_CONTEXTMENU, 0, MAKELPARAM((UINT)(r.left), (UINT)(r.bottom+SNM_1PIXEL_Y)));
		}
		break;

		case IDC_ANALYZE:
		{
			this->AbortAnalyze();
			this->AbortReanalyze();

			// Add currently selected tracks/takes to queue
			if (m_properties.analyzeTracks)
			{
				// Can't analyze master for now (accessor doesn't take receives into account) but leave in case this changes
				if (*(int*)GetSetMediaTrackInfo(GetMasterTrack(NULL), "I_SELECTED", NULL))
				{
					if (BR_LoudnessObject* object = this->IsObjectInList(GetMasterTrack(NULL)))
						m_analyzeQueue.Add(object);
					if (m_properties.clearAnalyzed)
						m_analyzeQueue.Add(new BR_LoudnessObject(GetMasterTrack(NULL)));
				}

				const int cnt=CountSelectedTracks(NULL);
				for (int i = 0; i < cnt; ++i)
				{
					if (BR_LoudnessObject* object = this->IsObjectInList(GetSelectedTrack(NULL, i)))
						m_analyzeQueue.Add(object);
					else
						m_analyzeQueue.Add(new BR_LoudnessObject(GetSelectedTrack(NULL, i)));
				}
			}
			else
			{
				const int cnt=CountSelectedMediaItems(NULL);
				for (int i = 0; i < cnt; ++i)
				{
					if (MediaItem_Take* take = GetActiveTake(GetSelectedMediaItem(NULL, i)))
					{
						if (BR_LoudnessObject* object = this->IsObjectInList(take))
							m_analyzeQueue.Add(object);
						else
							m_analyzeQueue.Add(new BR_LoudnessObject(take));
					}
				}
			}

			if (m_properties.clearAnalyzed)
				this->ClearList();

			if (m_analyzeQueue.GetSize())
			{
				for (int i = 0; i < m_analyzeQueue.GetSize(); ++i)
					m_objectsLen += m_analyzeQueue.Get(i)->GetAudioLength();

				// Prevent division by zero when calculating progress
				if (m_objectsLen == 0) m_objectsLen = 1;

				// Start timer which will analyze each object and finally update the list view
				SetAnalyzing(true, false);
			}
		}
		break;

		case REANALYZE_ITEMS:
		{
			this->AbortReanalyze();
			this->AbortAnalyze();
			this->Update();

			int x = 0;
			while (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->EnumSelected(&x))
				m_reanalyzeQueue.Add(listItem);

			if (m_reanalyzeQueue.GetSize())
			{
				for (int i = 0; i < m_reanalyzeQueue.GetSize(); ++i)
					m_objectsLen += m_reanalyzeQueue.Get(i)->GetAudioLength();

				// Prevent division by zero when calculating progress
				if (m_objectsLen == 0) m_objectsLen = 1;

				// Start timer which will analyze each object and finally update the list view
				SetAnalyzing(true, true);
			}
		}
		break;

		case NORMALIZE:
		{
			this->ShowNormalizeDialog(true);
		}
		break;

		case NORMALIZE_TO_23:
		case NORMALIZE_TO_0LU:
		{
			WDL_PtrList<BR_LoudnessObject> itemsToNormalize;
			int x = 0;
			while (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->EnumSelected(&x))
				itemsToNormalize.Add(listItem);

			BR_NormalizeData normalizeData = {&itemsToNormalize, 0, false, false};
			normalizeData.targetLufs = (wParam == NORMALIZE_TO_23) ? -23 : g_pref.LUtoLUFS(0);

			if (itemsToNormalize.GetSize())
			{
				this->AbortAnalyze();
				this->AbortReanalyze();
				this->Update();

				NormalizeAndShowProgress(&normalizeData);
				this->Update();
				if (m_properties.analyzeOnNormalize && normalizeData.normalized)
					this->OnCommand(REANALYZE_ITEMS, 0);
			}
		}
		break;

		case DRAW_SHORTTERM:
		case DRAW_MOMENTARY:
		{
			if (TrackEnvelope* env = GetSelectedEnvelope(NULL))
			{
				BR_Envelope envelope(env);
				if (m_properties.clearEnvelope)
				{
					envelope.DeletePoints(1, envelope.CountPoints()-1);
					envelope.UnselectAll();
					double position = 0;
					double value = envelope.LaneMinValue();
					double bezier = 0;
					int shape = SQUARE;
					envelope.SetPoint(0, &position, &value, &shape, &bezier);
				}

				bool update = false;
				int x = 0;
				while (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->EnumSelected(&x))
				{
					if (listItem->CreateGraph(envelope, g_pref.GetGraphMin(), g_pref.GetGraphMax(), (wParam == DRAW_MOMENTARY) ? (true) : (false), this->GetProperty(DO_HIGH_PRECISION_MODE), m_hwnd))
						update = true;
				}

				if (update && envelope.Commit())
					Undo_OnStateChangeEx2(NULL, __LOCALIZE("Draw loudness graph in active envelope", "sws_undo"), UNDO_STATE_TRACKCFG, -1);
			}
		}
		break;

		case DELETE_ITEM:
		{
			int x = 0;
			while (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->EnumSelected(&x))
			{
				m_reanalyzeQueue.Delete(m_reanalyzeQueue.Find(listItem), false);
				m_analyzeQueue.Delete(m_analyzeQueue.Find(listItem), true);

				int id = g_analyzedObjects.Get()->Find(listItem);
				if (id >= 0)
					g_analyzedObjects.Get()->Delete(id, true);
			}
			this->Update();
		}
		break;

		case SET_ANALYZE_TARGET_ITEM:
		{
			m_properties.analyzeTracks = false;
			this->Update();
		}
		break;

		case SET_ANALYZE_TARGET_TRACK:
		{
			m_properties.analyzeTracks = true;
			this->Update();
		}
		break;

		case SET_ANALYZE_ON_NORMALIZE:
		{
			m_properties.analyzeOnNormalize = !m_properties.analyzeOnNormalize;
		}
		break;

		case SET_MIRROR_SELECTION:
		{
			m_properties.mirrorProjSelection = !m_properties.mirrorProjSelection;
		}
		break;

		case SET_DOUBLECLICK_GOTO_TARGET:
		{
			m_properties.doubleClickGoToTarget = !m_properties.doubleClickGoToTarget;
		}
		break;

		case SET_TIMESEL_OVER_MAX:
		{
			m_properties.timeSelOverMax = !m_properties.timeSelOverMax;
		}
		break;

		case SET_CLEAR_ENVELOPE:
		{
			m_properties.clearEnvelope = !m_properties.clearEnvelope;
		}
		break;

		case SET_CLEAR_ON_ANALYZE:
		{
			m_properties.clearAnalyzed = !m_properties.clearAnalyzed;
		}
		break;

		case SET_DO_TRUE_PEAK:
		{
			m_properties.doTruePeak = !m_properties.doTruePeak;
		}
		break;

		case SET_DO_HIGH_PRECISION_MODE:
		{
			m_properties.doHighPrecisionMode = !m_properties.doHighPrecisionMode;
			this->ClearList(); // NF: force reanalyzing, as max. momentary values potentially change
			RefreshToolbar(NamedCommandLookup("_BR_NF_TOGGLE_LOUDNESS_HIGH_PREC"));
		}
		break;

		case SET_DO_DUAL_MONO_MODE:
		{
			m_properties.doDualMonoMode = !m_properties.doDualMonoMode;
			this->ClearList();
			RefreshToolbar(NamedCommandLookup("_BR_NF_TOGGLE_LOUDNESS_DUAL_MONO"));
		}
		break;

		case SET_UNIT_LUFS:
		{
			m_properties.usingLU = false;
			this->Update();
		}
		break;

		case SET_UNIT_LU:
		{
			m_properties.usingLU = true;
			this->Update();
		}
		break;

		case OPEN_GLOBAL_PREFERENCES:
		{
			if (!g_pref.IsPreferenceDlgVisible())
				ToggleLoudnessPref(NULL); // do it like this to refresh toolbar buttons
		}
		break;

		case OPEN_EXPORT_FORMAT:
		{
			this->ShowExportFormatDialog(true);
		}
		break;

		case OPEN_WIKI_HELP:
		{
			ShellExecute(NULL, "open", "http://wiki.cockos.com/wiki/index.php/Measure_and_normalize_loudness_with_SWS", NULL, NULL, SW_SHOWNORMAL);
		}
		break;

		case EXPORT_TO_CLIPBOARD:
		{
			int x = 0;
			if (m_list->EnumSelected(&x))
			{
				if (OpenClipboard(g_hwndParent))
				{
					WDL_FastString exportString = this->CreateExportString(false);

					EmptyClipboard();
					#ifdef _WIN32
						#if !defined(WDL_NO_SUPPORT_UTF8)
						if (WDL_HasUTF8(exportString.Get()))
						{
							DWORD size;
							WCHAR* wc = WDL_UTF8ToWC(exportString.Get(), false, 0, &size);

							if (HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, size*sizeof(WCHAR)))
							{
								if (LPVOID cp = GlobalLock(hglbCopy))
									memcpy(cp, wc, size*sizeof(WCHAR));
								GlobalUnlock(hglbCopy);
								SetClipboardData(CF_UNICODETEXT, hglbCopy);
							}
							free(wc);
						}
						else
						#endif
					#endif
					{
						if (HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, exportString.GetLength() + 1))
						{
							if (LPVOID cp = GlobalLock(hglbCopy))
								memcpy(cp, exportString.Get(), exportString.GetLength() + 1);
							GlobalUnlock(hglbCopy);
							SetClipboardData(CF_TEXT, hglbCopy);
						}
					}
					CloseClipboard();
				}
			}
		}
		break;

		case EXPORT_TO_FILE:
		{
			int x = 0;
			if (m_list->EnumSelected(&x))
			{
				static WDL_FastString s_lastPath;
				static bool           s_noNameSet = true;
				if (!s_lastPath.GetLength())
				{
					char projPath[SNM_MAX_PATH];
					GetProjectPath(projPath, sizeof(projPath));
					s_lastPath.Set(projPath);
				}

				char fn[SNM_MAX_PATH] = "";
				if (BrowseForSaveFile(__LOCALIZE("SWS/BR - Export formated list of analyzed items and tracks", "sws_DLG_174"),
					s_lastPath.Get(),
					s_noNameSet ? NULL : (strrchr(s_lastPath.Get(), '.') ? s_lastPath.Get() : NULL),
					SNM_TXT_EXT_LIST,
					fn,
					sizeof(fn))
				   )
				{
					if (FILE* f = fopenUTF8(fn, "wt"))
					{
						s_lastPath.Set(fn);
						s_noNameSet = false;

						WDL_FastString exportString = this->CreateExportString(false);
						if (exportString.GetLength())
						{
							fputs(exportString.Get(), f);
							fclose(f);
						}
					}
				}
			}
		}
		break;

		case GO_TO_SHORTTERM:
		{
			if (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->EnumSelected(NULL))
				listItem->GoToShortTermMax(this->m_properties.timeSelOverMax);
		}
		break;

		case GO_TO_MOMENTARY:
		{
			if (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->EnumSelected(NULL))
				listItem->GoToMomentaryMax(this->m_properties.timeSelOverMax);
		}
		break;

		case GO_TO_TRUE_PEAK:
		{
			if (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->EnumSelected(NULL))
				listItem->GoToTruePeak();
		}

		case IDC_CANCEL:
		{
			if (m_analyzeQueue.GetSize())   this->AbortAnalyze();
			if (m_reanalyzeQueue.GetSize())	this->AbortReanalyze();
			this->Update();
		}
		break;
	}
}

void BR_AnalyzeLoudnessWnd::OnTimer (WPARAM wParam)
{
	static BR_LoudnessObject* s_currentObject      = NULL;
	static double             s_currentObjectLen   = 0;
	static double             s_finishedObjectsLen = 0;

	if (wParam == ANALYZE_TIMER)
	{
		if (!m_analyzeInProgress)
		{
			// New analyze task began, reset variables
			if (m_currentObjectId == 0)
				s_finishedObjectsLen = 0;

			if (!m_analyzeQueue.GetSize())
			{
				// Make sure list view isn't populated with invalid items (i.e. user could have deleted them during analysis)
				for (int i = 0; i < g_analyzedObjects.Get()->GetSize(); ++i)
				{
					if (BR_LoudnessObject* object = g_analyzedObjects.Get()->Get(i))
					{
						if (!object->IsTargetValid())
							g_analyzedObjects.Get()->Delete(i--, true);
					}
				}

				this->Update();
				SetAnalyzing(false, false);

				return;
			}
			else
			{
				if ((s_currentObject = m_analyzeQueue.Get(0)))
				{
					s_currentObjectLen = s_currentObject->GetAudioLength();
					s_currentObject->Analyze(false, m_properties.doTruePeak, m_properties.doHighPrecisionMode, m_properties.doDualMonoMode);
					m_analyzeInProgress = true;
				}
				else
					m_analyzeQueue.Delete(0, true);
				++m_currentObjectId;
			}
		}
		else
		{
			// Make sure our object is still here
			double progress = 1;
			if (m_analyzeQueue.Find(s_currentObject) != -1 && s_currentObject->IsRunning())
				progress = (s_finishedObjectsLen + s_currentObjectLen * s_currentObject->GetProgress()) / m_objectsLen;
			else
			{
				if (m_analyzeQueue.Find(s_currentObject) != -1)
				{
					// Sometimes the analyzed object can already be in the list (if option to clear list upon analyzing is disabled)
					if (g_analyzedObjects.Get()->Find(s_currentObject) == -1)
						g_analyzedObjects.Get()->Add(s_currentObject);
					m_analyzeQueue.Delete(m_analyzeQueue.Find(s_currentObject), false);
					this->Update();
				}

				s_finishedObjectsLen += s_currentObjectLen;
				progress = s_finishedObjectsLen / m_objectsLen;

				m_analyzeInProgress = false;
			}
			SendMessage(GetDlgItem(m_hwnd, IDC_PROGRESS), PBM_SETPOS, (int)(progress*100), 0);
		}
	}
	else if (wParam == REANALYZE_TIMER)
	{
		if (!m_analyzeInProgress)
		{
			// New analyze task began, reset variables
			if (m_currentObjectId == 0)
				s_finishedObjectsLen = 0;

			if (!m_reanalyzeQueue.GetSize())
			{
				this->Update();
				SetAnalyzing(false, true);
				return;
			}
			else
			{
				if ((s_currentObject = m_reanalyzeQueue.Get(0)))
				{
					s_currentObjectLen = s_currentObject->GetAudioLength();
					s_currentObject->Analyze(false, m_properties.doTruePeak, m_properties.doHighPrecisionMode, m_properties.doDualMonoMode);
					m_analyzeInProgress = true;
				}
				else
					m_reanalyzeQueue.Delete(0, false);
				++m_currentObjectId;
			}
		}
		else
		{
			// Make sure our object is still here (user could have deleted it)
			if (m_reanalyzeQueue.Find(s_currentObject) != -1 && s_currentObject->IsRunning())
			{
				double progress = (s_finishedObjectsLen + s_currentObjectLen * s_currentObject->GetProgress()) / m_objectsLen;
				SendMessage(GetDlgItem(m_hwnd, IDC_PROGRESS), PBM_SETPOS, (int)(progress*100), 0);
			}
			else
			{
				if (m_reanalyzeQueue.Find(s_currentObject) != -1)
					m_reanalyzeQueue.Delete(m_reanalyzeQueue.Find(s_currentObject), false);

				s_finishedObjectsLen += s_currentObjectLen;
				double progress = s_finishedObjectsLen / m_objectsLen;
				SendMessage(GetDlgItem(m_hwnd, IDC_PROGRESS), PBM_SETPOS, (int)(progress*100), 0);
				m_analyzeInProgress = false;
			}
		}
	}
	else if (wParam == UPDATE_TIMER)
	{
		// Check for take/track name, selection, deletion updates
		if (!m_analyzeQueue.GetSize())
		{
			HWND hwnd = m_list->GetHWND();
			bool update = false;
			for (int i = 0; i < ListView_GetItemCount(hwnd); ++i)
			{
				if (BR_LoudnessObject* listItem = (BR_LoudnessObject*)m_list->GetListItem(i))
				{
					if (listItem->IsTargetValid())
					{
						char listName[CELL_MAX_LEN] = "";
						char realName[CELL_MAX_LEN] = "";

						ListView_GetItemText(hwnd, i, COL_TRACK, listName, sizeof(listName));
						listItem->GetColumnStr(COL_TRACK, realName, sizeof(realName), 0);
						if (strcmp(listName, realName))
							update = true;

						ListView_GetItemText(hwnd, i, COL_TAKE, listName, sizeof(listName));
						listItem->GetColumnStr(COL_TAKE, realName, sizeof(realName), 0);
						if (strcmp(listName, realName))
							update = true;

						if (m_properties.mirrorProjSelection)
						{
							if (listItem->IsSelectedInProject())
							{
								ListView_SetItemState(hwnd, i, LVIS_SELECTED, LVIS_SELECTED);
							}
							else
							{
								ListView_SetItemState(hwnd, i, 0, LVIS_SELECTED);
							}
						}
					}
					else
					{
						// Remove from reanalyze and analyze queues first!
						m_reanalyzeQueue.Delete(m_reanalyzeQueue.Find(listItem), false);
						m_analyzeQueue.Delete(m_analyzeQueue.Find(listItem), true);

						int id = g_analyzedObjects.Get()->Find(listItem);
						g_analyzedObjects.Get()->Delete(id, true);
						update = true;
					}
				}
			}
			if (update)
				this->Update();
		}
	}
}

void BR_AnalyzeLoudnessWnd::OnDestroy ()
{
	this->AbortAnalyze();
	this->AbortReanalyze();
	m_properties.Save();
	KillTimer(m_hwnd, UPDATE_TIMER);
}

void BR_AnalyzeLoudnessWnd::GetMinSize (int* w, int* h)
{
	WritePtr(w, 327);
	WritePtr(h, 160);
}

int BR_AnalyzeLoudnessWnd::OnKey (MSG* msg, int iKeyState)
{
	if (msg->message == WM_KEYDOWN && msg->wParam == VK_DELETE && !iKeyState)
	{
		this->OnCommand(DELETE_ITEM, 0);
		return 1;
	}
	return 0;
}

HMENU BR_AnalyzeLoudnessWnd::OnContextMenu (int x, int y, bool* wantDefaultItems)
{
	HMENU menu = CreatePopupMenu();

	int column;
	if ((BR_LoudnessObject*)m_list->GetHitItem(x, y, &column))
	{
		WritePtr(wantDefaultItems, false);
		if (m_properties.usingLU)
		{
			char menuEntry[512];
			WDL_FastString unit = g_pref.GetFormatedLUString();
			if (!strcmp(unit.Get(), __LOCALIZE("LU", "sws_loudness")))
				snprintf(menuEntry, sizeof(menuEntry), __LOCALIZE_VERFMT("Normalize to 0 %s (%g LUFS)", "sws_DLG_174"), unit.Get(), g_pref.LUtoLUFS(0));
			else
				snprintf(menuEntry, sizeof(menuEntry), __LOCALIZE_VERFMT("Normalize to 0 %s", "sws_DLG_174"), unit.Get());
			AddToMenu(menu, menuEntry, NORMALIZE_TO_0LU, -1, false);
		}
		else
			AddToMenu(menu, __LOCALIZE("Normalize to -23 LUFS", "sws_DLG_174"), NORMALIZE_TO_23, -1, false);
		AddToMenu(menu, __LOCALIZE("Normalize...", "sws_DLG_174"), NORMALIZE, -1, false);
		AddToMenu(menu, __LOCALIZE("Analyze", "sws_DLG_174"), REANALYZE_ITEMS, -1, false);

		AddToMenu(menu, SWS_SEPARATOR, 0);
		AddToMenu(menu, __LOCALIZE("Go to maximum short-term", "sws_DLG_174"), GO_TO_SHORTTERM, -1, false);
		AddToMenu(menu, __LOCALIZE("Go to maximum momentary", "sws_DLG_174"), GO_TO_MOMENTARY, -1, false);
		AddToMenu(menu, __LOCALIZE("Go to true peak", "sws_DLG_174"), GO_TO_TRUE_PEAK, -1, false);

		AddToMenu(menu, SWS_SEPARATOR, 0);
		WDL_FastString shortTermGraph, momentaryGraph;
		shortTermGraph.AppendFormatted(256, __LOCALIZE_VERFMT("Create short-term graph in selected envelope (%g to %g LUFS)", "sws_DLG_174"), g_pref.GetGraphMin(), g_pref.GetGraphMax());
		momentaryGraph.AppendFormatted(256, __LOCALIZE_VERFMT("Create momentary graph in selected envelope  (%g to %g LUFS)", "sws_DLG_174"), g_pref.GetGraphMin(), g_pref.GetGraphMax());
		AddToMenu(menu, shortTermGraph.Get(), DRAW_SHORTTERM, -1, false);
		AddToMenu(menu, momentaryGraph.Get(), DRAW_MOMENTARY, -1, false);

		AddToMenu(menu, SWS_SEPARATOR, 0);
		AddToMenu(menu, __LOCALIZE("Export formated list to clipboard", "sws_DLG_174"), EXPORT_TO_CLIPBOARD, -1, false);
		AddToMenu(menu, __LOCALIZE("Export formated list to file", "sws_DLG_174"), EXPORT_TO_FILE, -1, false);

		AddToMenu(menu, SWS_SEPARATOR, 0);
		AddToMenu(menu, __LOCALIZE("Remove", "sws_DLG_174"), DELETE_ITEM, -1, false);
	}
	else
	{
		bool button = false;
		RECT r;	GetWindowRect(GetDlgItem(m_hwnd, IDC_OPTIONS), &r);
		POINT pt = {x, y + 3*(SNM_1PIXEL_Y*(-1))};    // +/- 3 trick by Jeffos, see OnCommand()
		if (PtInRect(&r, pt))
		{
			WritePtr(wantDefaultItems, false);
			button = true;
		}
		AddToMenu(menu, __LOCALIZE("Analyze selected items", "sws_DLG_174"), SET_ANALYZE_TARGET_ITEM, -1, false, !m_properties.analyzeTracks ?  MF_CHECKED : MF_UNCHECKED);
		AddToMenu(menu, __LOCALIZE("Analyze selected tracks", "sws_DLG_174"), SET_ANALYZE_TARGET_TRACK, -1, false, m_properties.analyzeTracks ?  MF_CHECKED : MF_UNCHECKED);

		AddToMenu(menu, SWS_SEPARATOR, 0);
		HMENU optionsMenu = button ? NULL : CreatePopupMenu();
		HMENU unitMenu = CreatePopupMenu();
		AddToMenu(unitMenu, __LOCALIZE("LUFS", "sws_loudness"), SET_UNIT_LUFS, -1, false, !m_properties.usingLU ?  MF_CHECKED : MF_UNCHECKED);
		AddToMenu(unitMenu, g_pref.GetFormatedLUString().Get(), SET_UNIT_LU, -1, false, m_properties.usingLU ?  MF_CHECKED : MF_UNCHECKED);

		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Measure true peak (slower)", "sws_DLG_174"), SET_DO_TRUE_PEAK, -1, false, m_properties.doTruePeak ?  MF_CHECKED : MF_UNCHECKED);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Use high precision mode (slower)", "sws_DLG_174"), SET_DO_HIGH_PRECISION_MODE, -1, false, m_properties.doHighPrecisionMode ? MF_CHECKED : MF_UNCHECKED);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Use dual mono mode for mono takes/channel modes", "sws_DLG_174"), SET_DO_DUAL_MONO_MODE, -1, false, m_properties.doDualMonoMode ? MF_CHECKED : MF_UNCHECKED);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Analyze after normalizing", "sws_DLG_174"), SET_ANALYZE_ON_NORMALIZE, -1, false, m_properties.analyzeOnNormalize ?  MF_CHECKED : MF_UNCHECKED);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Clear list when analyzing", "sws_DLG_174"), SET_CLEAR_ON_ANALYZE, -1, false, m_properties.clearAnalyzed ?  MF_CHECKED : MF_UNCHECKED);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Clear envelope when creating loudness graph", "sws_DLG_174"), SET_CLEAR_ENVELOPE, -1, false, m_properties.clearEnvelope ?  MF_CHECKED : MF_UNCHECKED);

		AddToMenu((button ? menu : optionsMenu), SWS_SEPARATOR, 0);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Mirror project selection", "sws_DLG_174"), SET_MIRROR_SELECTION, -1, false, m_properties.mirrorProjSelection ?  MF_CHECKED : MF_UNCHECKED);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Double-click moves arrange to track/item", "sws_DLG_174"), SET_DOUBLECLICK_GOTO_TARGET, -1, false, m_properties.doubleClickGoToTarget ?  MF_CHECKED : MF_UNCHECKED);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Navigating to maximum short-term/momentary creates time selection", "sws_DLG_174"), SET_TIMESEL_OVER_MAX, -1, false, m_properties.timeSelOverMax ?  MF_CHECKED : MF_UNCHECKED);

		AddToMenu((button ? menu : optionsMenu), SWS_SEPARATOR, 0);
		AddSubMenu((button ? menu : optionsMenu), unitMenu, __LOCALIZE("Unit", "sws_DLG_174"), -1);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Export format...", "sws_DLG_174"), OPEN_EXPORT_FORMAT, -1, false);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Global preferences...", "sws_DLG_174"), OPEN_GLOBAL_PREFERENCES, -1, false);

		AddToMenu((button ? menu : optionsMenu), SWS_SEPARATOR, 0);
		AddToMenu((button ? menu : optionsMenu), __LOCALIZE("Help...", "sws_DLG_174"), OPEN_WIKI_HELP, -1, false);

		if (!button)
			AddSubMenu(menu, optionsMenu, __LOCALIZE("Options", "sws_DLG_174"), -1);
	}
	return menu;
}

bool BR_AnalyzeLoudnessWnd::ReprocessContextMenu ()
{
	return false;
}

INT_PTR BR_AnalyzeLoudnessWnd::OnUnhandledMsg(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_SHOWWINDOW)
	{
		if (!wParam)
		{
			if (m_normalizeWnd)
				this->ShowNormalizeDialog(false);
			if (m_exportFormatWnd)
				this->ShowExportFormatDialog(false);
		}
	}

	return 0;
}

BR_AnalyzeLoudnessWnd::Properties::Properties () :
analyzeTracks         (false),
analyzeOnNormalize    (true),
mirrorProjSelection   (true),
doubleClickGoToTarget (true),
timeSelOverMax        (false),
clearEnvelope         (true),
clearAnalyzed         (true),
doTruePeak            (true),
usingLU               (false),
doHighPrecisionMode   (true),
doDualMonoMode        (true)
{
}

void BR_AnalyzeLoudnessWnd::Properties::Load ()
{
	char tmp[2048];
	GetPrivateProfileString("SWS", LOUDNESS_KEY, "", tmp, sizeof(tmp), get_ini_file());

	LineParser lp(false);
	lp.parse(tmp);
	analyzeTracks         = (lp.getnumtokens() > 0) ? !!lp.gettoken_int(0) : false;
	analyzeOnNormalize    = (lp.getnumtokens() > 1) ? !!lp.gettoken_int(1) : true;
	mirrorProjSelection   = (lp.getnumtokens() > 2) ? !!lp.gettoken_int(2) : true;
	doubleClickGoToTarget = (lp.getnumtokens() > 3) ? !!lp.gettoken_int(3) : true;
	timeSelOverMax        = (lp.getnumtokens() > 4) ? !!lp.gettoken_int(4) : true;
	clearEnvelope         = (lp.getnumtokens() > 5) ? !!lp.gettoken_int(5) : true;
	clearAnalyzed         = (lp.getnumtokens() > 6) ? !!lp.gettoken_int(6) : true;
	doTruePeak            = (lp.getnumtokens() > 7) ? !!lp.gettoken_int(7) : false;
	usingLU               = (lp.getnumtokens() > 8) ? !!lp.gettoken_int(8) : false;
	doHighPrecisionMode   = (lp.getnumtokens() > 9) ? !!lp.gettoken_int(9) : false;
	doDualMonoMode        = (lp.getnumtokens() > 10) ? !!lp.gettoken_int(10) : false;

	GetPrivateProfileString("SWS", EXPORT_FORMAT_KEY, "$id - $target: $integrated, Range: $range, True peak: $truepeak", tmp, sizeof(tmp), get_ini_file());
	exportFormat.Set(tmp);
}

void BR_AnalyzeLoudnessWnd::Properties::Save ()
{
	int analyzeTracksInt         = analyzeTracks;
	int analyzeOnNormalizeInt    = analyzeOnNormalize;
	int mirrorProjSelectionInt   = mirrorProjSelection;
	int doubleClickGoToTargetInt = doubleClickGoToTarget;
	int timeSelOverMaxInt        = timeSelOverMax;
	int clearEnvelopeInt         = clearEnvelope;
	int clearAnalyzedInt         = clearAnalyzed;
	int doTruePeakInt            = doTruePeak;
	int usingLUInt               = usingLU;
	int doHighPrecisionModeInt   = doHighPrecisionMode;
	int doDualMonoModeInt        = doDualMonoMode;

	char tmp[512];
	snprintf(tmp, sizeof(tmp), "%d %d %d %d %d %d %d %d %d %d %d", analyzeTracksInt, analyzeOnNormalizeInt, mirrorProjSelectionInt, doubleClickGoToTargetInt, timeSelOverMaxInt, clearEnvelopeInt, clearAnalyzedInt, doTruePeakInt, usingLUInt, doHighPrecisionModeInt, doDualMonoModeInt);
	WritePrivateProfileString("SWS", LOUDNESS_KEY, tmp, get_ini_file());

	WritePrivateProfileString("SWS", EXPORT_FORMAT_KEY, exportFormat.Get(), get_ini_file());
}

/******************************************************************************
* Loudness init/exit                                                          *
******************************************************************************/
int LoudnessInitExit (bool init)
{
	static project_config_extension_t s_projectconfig = {ProcessExtensionLine, SaveExtensionConfig, BeginLoadProjectState, NULL};

	if (init)
	{
		g_pref.LoadGlobalPref();
		g_loudnessWndManager.Init();
		return plugin_register("projectconfig", &s_projectconfig);
	}
	else
	{
		g_pref.SaveGlobalPref();
		g_loudnessWndManager.Delete();
		plugin_register("-projectconfig", &s_projectconfig);
		return 1;
	}
}

void LoudnessUpdate (bool updatePreferencesDlg /*true*/)
{
	if (BR_AnalyzeLoudnessWnd* dialog = g_loudnessWndManager.Get())
		dialog->Update();
	if (g_normalizeWnd)
		SendMessage(g_normalizeWnd, WM_COMMAND, UPDATE_FROM_PROJDATA, 0);

	if (updatePreferencesDlg)
		g_pref.UpdatePreferenceDlg();
}

/******************************************************************************
* Commands                                                                    *
******************************************************************************/
static WDL_DLGRET NormalizeCommandDialogProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (INT_PTR r = SNM_HookThemeColorsMessage(hwnd, uMsg, wParam, lParam))
		return r;

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			// Get data
			char tmp[256];
			GetPrivateProfileString("SWS", NORMALIZE_KEY, "", tmp, sizeof(tmp), get_ini_file());

			LineParser lp(false);
			lp.parse(tmp);
			double value = (lp.getnumtokens() > 0) ? lp.gettoken_float(0) : -23;
			int unit     = (lp.getnumtokens() > 1) ? lp.gettoken_int(1)   : 0;
			int target   = (lp.getnumtokens() > 2) ? lp.gettoken_int(2)   : 0;

			// Set controls
			WDL_UTF8_HookComboBox(GetDlgItem(hwnd, IDC_UNIT));
			SendDlgItemMessage(hwnd, IDC_UNIT, CB_ADDSTRING, 0, (LPARAM)__LOCALIZE("LUFS", "sws_loudness"));
			SendDlgItemMessage(hwnd, IDC_UNIT, CB_ADDSTRING, 0, (LPARAM)g_pref.GetFormatedLUString().Get());
			SendDlgItemMessage(hwnd, IDC_UNIT, CB_SETCURSEL, unit, 0);

			snprintf(tmp, sizeof(tmp), "%.6g", value); SetDlgItemText(hwnd, IDC_VALUE, tmp);
			SetFocus(GetDlgItem(hwnd, IDC_VALUE));
			SendMessage(GetDlgItem(hwnd, IDC_VALUE), EM_SETSEL, 0, -1);

			WDL_UTF8_HookComboBox(GetDlgItem(hwnd, IDC_TARGET));
			SendDlgItemMessage(hwnd, IDC_TARGET, CB_ADDSTRING, 0, (LPARAM)__LOCALIZE("items","sws_DLG_177"));
			SendDlgItemMessage(hwnd, IDC_TARGET, CB_ADDSTRING, 0, (LPARAM)__LOCALIZE("tracks","sws_DLG_177"));
			SendDlgItemMessage(hwnd, IDC_TARGET, CB_SETCURSEL, target, 0);

			// Restore window
			RestoreWindowPos(hwnd, NORMALIZE_WND, false);
			ShowWindow(hwnd, SW_SHOW);
		}
		break;

		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case UPDATE_FROM_PROJDATA:
				{
					int unit = (int)SendDlgItemMessage(hwnd, IDC_UNIT, CB_GETCURSEL, 0, 0);
					SendDlgItemMessage(hwnd, IDC_UNIT, CB_DELETESTRING, 1, 0);
					SendDlgItemMessage(hwnd, IDC_UNIT, CB_ADDSTRING, 0, (LPARAM)g_pref.GetFormatedLUString().Get());
					SendDlgItemMessage(hwnd, IDC_UNIT, CB_SETCURSEL, unit, 0);
				}
				break;

				case IDC_UNIT:
				{
					if (HIWORD(wParam) == CBN_SELCHANGE)
					{
						int unit = (int)SendDlgItemMessage(hwnd, IDC_UNIT, CB_GETCURSEL, 0, 0);
						char value[256]; GetDlgItemText(hwnd, IDC_VALUE, value, sizeof(value));

						snprintf(value, sizeof(value), "%.6g", (unit == 1) ? g_pref.LUFStoLU(AltAtof(value)) : g_pref.LUtoLUFS(AltAtof(value)));
						SetDlgItemText(hwnd, IDC_VALUE, value);
					}
				}
				break;

				case IDOK:
				{
					bool useLU    = ((int)SendDlgItemMessage(hwnd, IDC_UNIT, CB_GETCURSEL, 0, 0) == 1)   ? true : false;
					bool doTracks = ((int)SendDlgItemMessage(hwnd, IDC_TARGET, CB_GETCURSEL, 0, 0) == 1) ? true : false;
					char value[256]; GetDlgItemText(hwnd, IDC_VALUE, value, sizeof(value));

					WDL_PtrList_DeleteOnDestroy<BR_LoudnessObject> objects;
					if (doTracks)
					{
						// Can't analyze master for now (accessor doesn't take receives into account) but leave in case this changes
						if (*(int*)GetSetMediaTrackInfo(GetMasterTrack(NULL), "I_SELECTED", NULL))
							objects.Add(new BR_LoudnessObject(GetMasterTrack(NULL)));

						const int cnt=CountSelectedTracks(NULL);
						for (int i = 0; i < cnt; ++i)
							objects.Add(new BR_LoudnessObject(GetSelectedTrack(NULL, i)));
					}
					else
					{
						const int cnt=CountSelectedMediaItems(NULL);
						for (int i = 0; i < cnt; ++i)
						{
							if (MediaItem_Take* take = GetActiveTake(GetSelectedMediaItem(NULL, i)))
								objects.Add(new BR_LoudnessObject(take));
						}
					}
					BR_NormalizeData normalizeData = {&objects, (useLU) ? g_pref.LUtoLUFS(AltAtof(value)) : AltAtof(value), true, false};

					DestroyWindow(hwnd);
					RefreshToolbar(NamedCommandLookup("_BR_NORMALIZE_LOUDNESS_ITEMS"));

					if (objects.GetSize())
						NormalizeAndShowProgress(&normalizeData);
				}
				break;

				case IDCANCEL:
				{
					DestroyWindow(hwnd);
					RefreshToolbar(NamedCommandLookup("_BR_NORMALIZE_LOUDNESS_ITEMS"));
				}
				break;
			}
		}
		break;

		case WM_DESTROY:
		{
			int unit = (int)SendDlgItemMessage(hwnd, IDC_UNIT, CB_GETCURSEL, 0, 0);
			int target = (int)SendDlgItemMessage(hwnd, IDC_TARGET ,CB_GETCURSEL,0,0);

			char value[256]; GetDlgItemText(hwnd, IDC_VALUE, value, sizeof(value));

			char tmp[342];
			snprintf(tmp, sizeof(tmp), "%lf %d %d", AltAtof(value), unit, target);
			WritePrivateProfileString("SWS", NORMALIZE_KEY, tmp, get_ini_file());

			SaveWindowPos(hwnd, NORMALIZE_WND);
			g_normalizeWnd = NULL;
		}
		break;
	}
	return 0;
}

void NormalizeLoudness (COMMAND_T* ct)
{
	if ((int)ct->user == 0)
	{
		if (!g_normalizeWnd)
			g_normalizeWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_BR_LOUDNESS_NORMALIZE), g_hwndParent, NormalizeCommandDialogProc);
		else
		{
			DestroyWindow(g_normalizeWnd);
			g_normalizeWnd = NULL;
		}
		RefreshToolbar(NamedCommandLookup("_BR_NORMALIZE_LOUDNESS_ITEMS"));
	}
	else
	{
		WDL_PtrList_DeleteOnDestroy<BR_LoudnessObject> objects;
		if (abs((int)ct->user) == 1)
		{
			const int cnt=CountSelectedMediaItems(NULL);
			for (int i = 0; i < cnt; ++i)
			{
				if (MediaItem_Take* take = GetActiveTake(GetSelectedMediaItem(NULL, i)))
					objects.Add(new BR_LoudnessObject(take));
			}
		}
		else
		{
			// Can't analyze master for now (accessor doesn't take receives into account) but leave in case this changes
			if (*(int*)GetSetMediaTrackInfo(GetMasterTrack(NULL), "I_SELECTED", NULL))
				objects.Add(new BR_LoudnessObject(GetMasterTrack(NULL)));

			const int trSelCnt = CountSelectedTracks(NULL);
			for (int i = 0; i < trSelCnt; ++i)
				objects.Add(new BR_LoudnessObject(GetSelectedTrack(NULL, i)));
		}

		BR_NormalizeData normalizeData = {&objects, ((int)ct->user < 0) ? g_pref.LUtoLUFS(0) : -23, true, false};
		NormalizeAndShowProgress(&normalizeData);
	}
}

void AnalyzeLoudness (COMMAND_T* ct)
{
	if (BR_AnalyzeLoudnessWnd* dialog = g_loudnessWndManager.Create())
		dialog->Show(true, true);
}

void ToggleLoudnessPref (COMMAND_T* ct)
{
	g_pref.ShowPreferenceDlg(!g_pref.IsPreferenceDlgVisible());
	RefreshToolbar(NamedCommandLookup("_BR_LOUDNESS_PREF"));
}

void ToggleHighPrecisionOption(COMMAND_T* ct)
{
	bool isHighPrecOptEnabled = !!IsHighPrecisionOptionEnabled(NULL); // creates window if necessary (calls g_loudnessWndManager.Create())

	BR_AnalyzeLoudnessWnd* dialog = g_loudnessWndManager.Get();
	dialog->SetProperty(BR_AnalyzeLoudnessWnd::DO_HIGH_PRECISION_MODE, !isHighPrecOptEnabled);
	dialog->SaveProperties();
	dialog->ClearList(); // force reanalyzing

	RefreshToolbar(NamedCommandLookup("_BR_NF_TOGGLE_LOUDNESS_HIGH_PREC"));
}

void ToggleDualMonoOption(COMMAND_T* ct)
{
	bool isDualMonoOptEnabled = !!IsDualMonoOptionEnabled(NULL);

	BR_AnalyzeLoudnessWnd* dialog = g_loudnessWndManager.Get();
	dialog->SetProperty(BR_AnalyzeLoudnessWnd::DO_DUAL_MONO_MODE, !isDualMonoOptEnabled);
	dialog->SaveProperties();
	dialog->ClearList();

	RefreshToolbar(NamedCommandLookup("_BR_NF_TOGGLE_LOUDNESS_DUAL_MONO"));
}

/******************************************************************************
* Toggle states                                                               *
******************************************************************************/
int IsNormalizeLoudnessVisible (COMMAND_T* ct)
{
	return g_normalizeWnd ? 1 : 0;
}

int IsAnalyzeLoudnessVisible (COMMAND_T* ct)
{
	if (BR_AnalyzeLoudnessWnd* dialog = g_loudnessWndManager.Get())
		return (int)dialog->IsWndVisible();
	return 0;
}

int IsLoudnessPrefVisible (COMMAND_T* ct)
{
	return g_pref.IsPreferenceDlgVisible() ? 1 : 0;
}

int IsHighPrecisionOptionEnabled(COMMAND_T* ct)
{
	bool isHighPrecOptEnabled  = false;
	if (g_loudnessWndManager.Get())
		isHighPrecOptEnabled = g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::DO_HIGH_PRECISION_MODE);
	else
	{
		if (BR_AnalyzeLoudnessWnd* dialog = g_loudnessWndManager.Create()) 
		{
			dialog->LoadProperties();
			isHighPrecOptEnabled = dialog->GetProperty(BR_AnalyzeLoudnessWnd::DO_HIGH_PRECISION_MODE);
		}
	}

	return isHighPrecOptEnabled;
}

int IsDualMonoOptionEnabled(COMMAND_T* ct)
{
	bool isDualMonoOptEnabled = false;
	if (g_loudnessWndManager.Get())
		isDualMonoOptEnabled = g_loudnessWndManager.Get()->GetProperty(BR_AnalyzeLoudnessWnd::DO_DUAL_MONO_MODE);
	else
	{
		if (BR_AnalyzeLoudnessWnd * dialog = g_loudnessWndManager.Create())
		{
			dialog->LoadProperties();
			isDualMonoOptEnabled = dialog->GetProperty(BR_AnalyzeLoudnessWnd::DO_DUAL_MONO_MODE);
		}
	}

	return isDualMonoOptEnabled;
}


//////////////////////////////////////////////////////////////////
//                                                              //
//    #880 Export Loudness analysis to ReaScript                //
//                                                              //
//////////////////////////////////////////////////////////////////
static WDL_DLGRET NFAnalyzeLUFSProgressProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (INT_PTR r = SNM_HookThemeColorsMessage(hwnd, uMsg, wParam, lParam))
		return r;

	static BR_NormalizeData* s_normalizeData = NULL;
	static BR_LoudnessObject* s_currentItem = NULL;

	static int  s_currentItemId = 0;
	static bool s_analyzeInProgress = false;
	static double s_itemsLen = 0;
	static double s_currentItemLen = 0;
	static double s_finishedItemsLen = 0;

#ifndef _WIN32
	static bool s_positionSet = false;
#endif

	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		// Reset variables
		s_normalizeData = (BR_NormalizeData*)lParam;
		if (!s_normalizeData || !s_normalizeData->items)
		{
			EndDialog(hwnd, 0);
			return 0;
		}

		s_currentItemId = 0;
		s_analyzeInProgress = false;
		s_itemsLen = 0;
		s_currentItemLen = 0;
		s_finishedItemsLen = 0;

		// Get progress data
		for (int i = 0; i < s_normalizeData->items->GetSize(); ++i)
		{
			if (BR_LoudnessObject* item = s_normalizeData->items->Get(i))
				s_itemsLen += item->GetAudioLength();
		}
		if (s_itemsLen == 0) s_itemsLen = 1; // to prevent division by zero


#ifdef _WIN32
		CenterDialog(hwnd, g_hwndParent, HWND_TOP);
#else
		s_positionSet = false;
#endif

		// Start normalizing
		SetTimer(hwnd, ANALYZE_TIMER_FREQ, 100, NULL);
	}
	break;

#ifndef _WIN32
	case WM_ACTIVATE:
	{
		// SetWindowPos doesn't seem to work in WM_INITDIALOG on OSX
		// when creating a dialog with DialogBox so call here
		if (!s_positionSet)
			CenterDialog(hwnd, GetParent(hwnd), HWND_TOP);
		s_positionSet = true;
	}
	break;
#endif

	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDCANCEL:
		{
			KillTimer(hwnd, 1);
			s_normalizeData = NULL;
			if (s_currentItem)
				s_currentItem->AbortAnalyze();
			EndDialog(hwnd, 0);
		}
		break;
		}
	}
	break;

	case WM_TIMER:
	{
		if (!s_normalizeData)
			return 0;

		if (!s_analyzeInProgress)
		{
			// No more objects to analyze, normalize them
			if (s_currentItemId >= s_normalizeData->items->GetSize())

			{
				s_normalizeData->normalized = true;
				UpdateTimeline();
				EndDialog(hwnd, 0);
				return 0;
			}


			// Start analysis of the next item
			if ((s_currentItem = s_normalizeData->items->Get(s_currentItemId)))
			{
				s_currentItemLen = s_currentItem->GetAudioLength();

				// NF: only use high prec. mode in full analyzing mode (and user has set it in Options), disable in quick mode
				bool wantHighPrecisionMode = false;
				if (!s_normalizeData->quickMode)
					wantHighPrecisionMode = true;

				s_currentItem->Analyze(s_normalizeData->quickMode, s_normalizeData->items->Get(s_currentItemId)->GetDoTruePeak(), wantHighPrecisionMode ? s_normalizeData->items->Get(s_currentItemId)->GetDoHighPrecisionMode() : false, s_normalizeData->items->Get(s_currentItemId)->GetDoDualMonoMode());

				s_analyzeInProgress = true;
			}
			else
				++s_currentItemId;
		}
		else
		{
			if (s_currentItem->IsRunning())
			{
				double progress = (s_finishedItemsLen + s_currentItemLen * s_currentItem->GetProgress()) / s_itemsLen;
				SendMessage(GetDlgItem(hwnd, IDC_PROGRESS), PBM_SETPOS, (int)(progress * 100), 0);
			}
			else
			{
				s_finishedItemsLen += s_currentItemLen;
				double progress = s_finishedItemsLen / s_itemsLen;
				SendMessage(GetDlgItem(hwnd, IDC_PROGRESS), PBM_SETPOS, (int)(progress * 100), 0);

				s_analyzeInProgress = false;
				++s_currentItemId;
			}
		}
	}
	break;

	case WM_DESTROY:
	{
		KillTimer(hwnd, 1);
		s_normalizeData = NULL;
		if (s_currentItem)
			s_currentItem->AbortAnalyze();
		s_analyzeInProgress = false;
	}
	break;
	}
	return 0;
}

void NFAnalyzeItemsLoudnessAndShowProgress(BR_NormalizeData* analyzeData)
{
	static bool s_normalizeInProgress = false;

	if (!s_normalizeInProgress)
	{
		// Kill all normalize dialogs prior to normalizing
		if (BR_AnalyzeLoudnessWnd* dialog = g_loudnessWndManager.Get())
			dialog->KillNormalizeDlg();
		if (g_normalizeWnd)
		{
			DestroyWindow(g_normalizeWnd);
			g_normalizeWnd = NULL;
			// RefreshToolbar(NamedCommandLookup("_BR_NORMALIZE_LOUDNESS_ITEMS"));
		}

		s_normalizeInProgress = true;
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_NF_LOUDNESS_ANALYZE_PROGRESS), g_hwndParent, NFAnalyzeLUFSProgressProc, (LPARAM)analyzeData);
		s_normalizeInProgress = false;
	}
}

bool NFDoAnalyzeTakeLoudness_IntegratedOnly(MediaItem_Take* take, double* lufsIntegrated)
{
	bool success = false;

	if (take) {
		if (TakeIsMIDI(take)) // check for MIDI takes
			return false;

		WDL_PtrList_DeleteOnDestroy<BR_LoudnessObject> objects;
		objects.Add(new BR_LoudnessObject(take));

		bool isTargetValid = objects.Get(0)->CheckTarget(take);
		if (isTargetValid) {
			BR_NormalizeData analyzeData = { &objects, -23, true, false }; // quick mode, integrated only, targetLUFS isn't used here
			NFAnalyzeItemsLoudnessAndShowProgress(&analyzeData);

			if (analyzeData.normalized) { // here: checks if analysis is completed, returns false if e.g. user cancels analyse process
				double returnLUFSintegrated;

				objects.Get(0)->GetAnalyzeData(&returnLUFSintegrated, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
				WritePtr(lufsIntegrated, returnLUFSintegrated);

				success = true;
			}
		}
	}
	return success;
}

bool NFDoAnalyzeTakeLoudness(MediaItem_Take* take, bool analyzeTruePeak, double* lufsIntegrated, double* range, double* truePeak, double* truePeakPos, double* shortTermMax, double* momentaryMax)
{
	bool success = false;

	if (take)
	{
		if (TakeIsMIDI(take)) // check for MIDI takes
			return false;

		WDL_PtrList_DeleteOnDestroy<BR_LoudnessObject> objects;
		objects.Add(new BR_LoudnessObject(take));

		bool isTargetValid = objects.Get(0)->CheckTarget(take);
		if (isTargetValid)
		{
			objects.Get(0)->SetDoTruePeak(analyzeTruePeak);

			// check if user set high precision mode 
			bool doHighPrecisionMode = !!IsHighPrecisionOptionEnabled(NULL);
			objects.Get(0)->SetDoHighPrecisionMode(doHighPrecisionMode);

			BR_NormalizeData analyzeData = { &objects, -23, false, false }; // full analyze mode
			NFAnalyzeItemsLoudnessAndShowProgress(&analyzeData);

			if (analyzeData.normalized) {
				double returnLUFSintegrated, returnRange, returnTruePeak, returnTruePeakPos, returnShortTermMax, returnMomentaryMax;

				objects.Get(0)->GetAnalyzeData(&returnLUFSintegrated, &returnRange, &returnTruePeak, &returnTruePeakPos, &returnShortTermMax, &returnMomentaryMax, NULL, NULL);

				WritePtr(lufsIntegrated, returnLUFSintegrated);
				WritePtr(range, returnRange);
				WritePtr(truePeak, returnTruePeak);
				WritePtr(truePeakPos, returnTruePeakPos);
				WritePtr(shortTermMax, returnShortTermMax);
				WritePtr(momentaryMax, returnMomentaryMax);

				success = true;
			}
		}
	}
	return success;
}

bool NFDoAnalyzeTakeLoudness2(MediaItem_Take* take, bool analyzeTruePeak, double* lufsIntegrated, double* range, double* truePeak, double* truePeakPos, double* shortTermMax, double* momentaryMax, double* shortTermMaxPos, double* momentaryMaxPos)
{
	bool success = false;

	if (take)
	{
		if (TakeIsMIDI(take)) // check for MIDI takes
			return false;

		WDL_PtrList_DeleteOnDestroy<BR_LoudnessObject> objects;
		objects.Add(new BR_LoudnessObject(take));

		bool isTargetValid = objects.Get(0)->CheckTarget(take);
		if (isTargetValid) {
			objects.Get(0)->SetDoTruePeak(analyzeTruePeak);

			// don't use high precision mode here to ensure correct max. short-term / momentary positions
			objects.Get(0)->SetDoHighPrecisionMode(false);

			BR_NormalizeData analyzeData = { &objects, -23, false, false }; // full analyze mode
			NFAnalyzeItemsLoudnessAndShowProgress(&analyzeData);

			if (analyzeData.normalized) {
				double returnLUFSintegrated, returnRange, returnTruePeak, returnTruePeakPos, returnShortTermMax, returnMomentaryMax, returnShortTermMaxPos, returnMomentaryMaxPos;

				objects.Get(0)->GetAnalyzeData(&returnLUFSintegrated, &returnRange, &returnTruePeak, &returnTruePeakPos, &returnShortTermMax, &returnMomentaryMax, NULL, NULL);

				returnShortTermMaxPos = objects.Get(0)->GetMaxShorttermPos(true); // use project time
				returnMomentaryMaxPos = objects.Get(0)->GetMaxMomentaryPos(true);


				WritePtr(lufsIntegrated, returnLUFSintegrated);
				WritePtr(range, returnRange);
				WritePtr(truePeak, returnTruePeak);
				WritePtr(truePeakPos, returnTruePeakPos);
				WritePtr(shortTermMax, returnShortTermMax);
				WritePtr(momentaryMax, returnMomentaryMax);

				WritePtr(shortTermMaxPos, returnShortTermMaxPos);
				WritePtr(momentaryMaxPos, returnMomentaryMaxPos);

				success = true;
			}
		}
	}
	return success;
}

//////////////////////////////////////////////////////////////////
//    E N D                                                     //
//    #880 Export Loudness analysis to ReaScript                //
//                                                              //
//////////////////////////////////////////////////////////////////
