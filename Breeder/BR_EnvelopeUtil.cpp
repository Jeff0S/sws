/******************************************************************************
/ BR_EnvelopeUtil.cpp
/
/ Copyright (c) 2013-2015 Dominik Martin Drzic
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

#include "BR_EnvelopeUtil.h"
#include "BR_Util.h"

#include <WDL/lice/lice_bezier.h>
#include <WDL/localize/localize.h>

/******************************************************************************
* BR_Envelope                                                                 *
******************************************************************************/
BR_Envelope::BR_Envelope () :
m_envelope      (NULL),
m_parent        (NULL),
m_take          (NULL),
m_tempoMap      (false),
m_update        (false),
m_sorted        (true),
m_pointsEdited  (false),
m_takeEnvOffset (0),
m_sampleRate    (-1),
m_rebuildConseq (true),
m_height        (-1),
m_yOffset       (-1),
m_takeEnvType   (UNKNOWN),
m_data          (NULL)
{
}

BR_Envelope::BR_Envelope (TrackEnvelope* envelope, bool takeEnvelopesUseProjectTime /*=true*/) :
m_envelope      (envelope),
m_parent        (GetEnvParent(m_envelope)),
m_take          (NULL),
m_tempoMap      (envelope == GetTempoEnv()),
m_update        (false),
m_sorted        (true),
m_pointsEdited  (false),
m_takeEnvOffset (0),
m_sampleRate    (-1),
m_rebuildConseq (true),
m_height        (-1),
m_yOffset       (-1),
m_takeEnvType   (UNKNOWN),
m_data          (NULL)
{
	if (!m_parent)
		m_take = GetTakeEnvParent(m_envelope, &m_takeEnvType);
	this->Build(takeEnvelopesUseProjectTime);
}

BR_Envelope::BR_Envelope (MediaTrack* track, int envelopeId, bool takeEnvelopesUseProjectTime /*=true*/) :
m_envelope      (GetTrackEnvelope(track, envelopeId)),
m_parent        (track),
m_take          (NULL),
m_tempoMap      (m_envelope == GetTempoEnv()),
m_update        (false),
m_sorted        (true),
m_pointsEdited  (false),
m_takeEnvOffset (0),
m_sampleRate    (-1),
m_rebuildConseq (true),
m_height        (-1),
m_yOffset       (-1),
m_takeEnvType   (UNKNOWN),
m_data          (NULL)
{
	this->Build(takeEnvelopesUseProjectTime);
}

BR_Envelope::BR_Envelope (MediaItem_Take* take, BR_EnvType envType, bool takeEnvelopesUseProjectTime /*=true*/) :
m_envelope      (GetTakeEnv(take, envType)),
m_parent        (NULL),
m_take          (take),
m_tempoMap      (false),
m_update        (false),
m_sorted        (true),
m_pointsEdited  (false),
m_takeEnvOffset (0),
m_sampleRate    (-1),
m_rebuildConseq (true),
m_height        (-1),
m_yOffset       (-1),
m_takeEnvType   (m_envelope ? envType : UNKNOWN),
m_data          (NULL)
{
	this->Build(takeEnvelopesUseProjectTime);
}

BR_Envelope::BR_Envelope (const BR_Envelope& envelope) :
m_envelope        (envelope.m_envelope),
m_parent          (envelope.m_parent),
m_take            (envelope.m_take),
m_tempoMap        (envelope.m_tempoMap),
m_update          (envelope.m_update),
m_sorted          (envelope.m_sorted),
m_pointsEdited    (envelope.m_pointsEdited),
m_takeEnvOffset   (envelope.m_takeEnvOffset),
m_sampleRate      (envelope.m_sampleRate),
m_rebuildConseq   (true),
m_height          (envelope.m_height),
m_yOffset         (envelope.m_yOffset),
m_takeEnvType     (envelope.m_takeEnvType),
m_data            (envelope.m_data),
m_points          (envelope.m_points),
m_pointsSel       (envelope.m_pointsSel),
m_pointsConseq    (envelope.m_pointsConseq),
m_properties      (envelope.m_properties),
m_chunkProperties (envelope.m_chunkProperties),
m_envName         (envelope.m_envName)
{
}

BR_Envelope::~BR_Envelope ()
{
}

BR_Envelope& BR_Envelope::operator= (const BR_Envelope& envelope)
{
	if (this == &envelope)
		return *this;

	m_envelope      = envelope.m_envelope;
	m_parent        = envelope.m_parent;
	m_take          = envelope.m_take;
	m_tempoMap      = envelope.m_tempoMap;
	m_update        = envelope.m_update;
	m_sorted        = envelope.m_sorted;
	m_pointsEdited  = envelope.m_pointsEdited;
	m_takeEnvOffset = envelope.m_takeEnvOffset;
	m_sampleRate    = envelope.m_sampleRate;
	m_rebuildConseq = envelope.m_rebuildConseq;
	m_height        = envelope.m_height;
	m_yOffset       = envelope.m_yOffset;
	m_takeEnvType   = envelope.m_takeEnvType;
	m_data          = envelope.m_data;
	m_points        = envelope.m_points;
	m_pointsSel     = envelope.m_pointsSel;
	m_pointsConseq  = envelope.m_pointsConseq;
	m_properties    = envelope.m_properties;

	m_chunkProperties.Set(&envelope.m_chunkProperties);
	m_envName.Set(&envelope.m_envName);

	return *this;
}

bool BR_Envelope::operator== (const BR_Envelope& envelope) const
{
	if (this->m_tempoMap  != envelope.m_tempoMap)  return false;
	if (this->m_points    != envelope.m_points)    return false;
	if (this->m_pointsSel != envelope.m_pointsSel) return false;

	if (!this->m_properties.filled)    this->FillProperties();
	if (!envelope.m_properties.filled) envelope.FillProperties();
	if (this->m_properties.active != envelope.m_properties.active) return false;

	return true;
}

bool BR_Envelope::operator!= (const BR_Envelope& envelope) const
{
	return !(*this == envelope);
}

bool BR_Envelope::GetPoint (int id, double* position, double* value, int* shape, double* bezier)
{
	if (this->ValidateId(id))
	{
		WritePtr(position, m_points[id].position + m_takeEnvOffset);
		WritePtr(value,    m_points[id].value   );
		WritePtr(shape,    m_points[id].shape   );
		WritePtr(bezier,   m_points[id].bezier  );
		return true;
	}
	else
	{
		WritePtr(position, 0.0);
		WritePtr(value,    0.0);
		WritePtr(shape,    0  );
		WritePtr(bezier,   0.0);
		return false;
	}
}

bool BR_Envelope::SetPoint (int id, double* position, double* value, int* shape, double* bezier, bool checkPosition /*= false*/, bool snapValue /*= false*/)
{
	if (this->ValidateId(id))
	{
		if (this->IsTakeEnvelope() && position && checkPosition && !CheckBounds(*position - m_takeEnvOffset, 0.0, GetMediaItemInfo_Value(GetMediaItemTake_Item(m_take), "D_LENGTH")))
			return false;

		if (snapValue && value)
			WritePtr(value, this->SnapValue(*value));

		if (position) m_points[id].position = *position - m_takeEnvOffset;
		ReadPtr(value,  m_points[id].value);
		ReadPtr(shape,  m_points[id].shape);
		ReadPtr(bezier, m_points[id].bezier);

		m_update = true;
		if (position) m_sorted = false;
		m_pointsEdited = true;
		return true;
	}
	else
		return false;
}

bool BR_Envelope::GetSelection (int id)
{
	if (this->ValidateId(id))
		return (m_points[id].selected) ? true : false;
	else
		return false;
}

bool BR_Envelope::SetSelection (int id, bool selected)
{
	if (this->ValidateId(id))
	{
		if (m_points[id].selected != selected)
		{
			m_points[id].selected = selected;
			m_update = true;
		}
		return true;
	}
	else
		return false;
}

bool BR_Envelope::CreatePoint (int id, double position, double value, int shape, double bezier, bool selected, bool checkPosition /*=false*/, bool snapValue /*= false*/)
{
	if (id >= 0)
	{
		if (id >= (int)m_points.size())
			id = m_points.size();

		position -= m_takeEnvOffset;

		if (this->IsTakeEnvelope() && checkPosition && !CheckBounds(position, 0.0, GetMediaItemInfo_Value(GetMediaItemTake_Item(m_take), "D_LENGTH")))
			return false;

		BR_Envelope::EnvPoint newPoint(position, (snapValue) ? (this->SnapValue(value)) : (value), shape, 0, selected, 0, bezier);
		m_points.insert(m_points.begin() + id, newPoint);

		m_update       = true;
		m_sorted       = false;
		m_pointsEdited = true;
		return true;
	}
	else
		return false;
}

bool BR_Envelope::DeletePoint (int id)
{
	if (this->ValidateId(id))
	{
		m_points.erase(m_points.begin() + id);

		m_update       = true;
		m_pointsEdited = true;
		return true;
	}
	else
		return false;
}

bool BR_Envelope::GetTimeSig (int id, bool* sig, bool* partial, int* num, int* den)
{
	if (this->ValidateId(id) && m_tempoMap)
	{
		WritePtr(sig,     (m_points[id].sig)                ? (true) : (false));
		WritePtr(partial, (GetBit(m_points[id].partial, 2)) ? (true) : (false));

		if (num || den)
		{
			int effectiveTimeSig = 0;
			for (;id >= 0; --id)
			{
				if (m_points[id].sig != 0)
				{
					effectiveTimeSig = m_points[id].sig;
					break;
				}
			}

			int effNum, effDen;
			if (!effectiveTimeSig)
				TimeMap_GetTimeSigAtTime(NULL, -1, &effNum, &effDen, NULL);
			else
			{
				effNum = effectiveTimeSig & 0xFF; // num is low 16 bits
				effDen = effectiveTimeSig >> 16;  // den is high 16 bits
			}

			WritePtr(num, effNum);
			WritePtr(den, effDen);
		}
		return true;
	}
	else
	{
		WritePtr(sig, false);
		WritePtr(sig, false);
		WritePtr(num, 0);
		WritePtr(den, 0);
		return false;
	}
}

bool BR_Envelope::SetTimeSig (int id, bool sig, bool partial, int num, int den)
{
	if (this->ValidateId(id) && m_tempoMap)
	{
		if (sig && (!CheckBounds(num, MIN_SIG, MAX_SIG) || !CheckBounds(den, MIN_SIG, MAX_SIG)))
				return false;

		m_points[id].sig = (sig) ? ((den << 16) + num) : (0);
		m_points[id].partial = SetBit(m_points[id].partial, 0, sig);
		m_points[id].partial = SetBit(m_points[id].partial, 2, partial);

		m_update       = true;
		m_pointsEdited = true;
		return true;
	}
	else
		return false;
}

bool BR_Envelope::SetCreatePoint (int id, double position, double value, int shape, double bezier, bool selected)
{
	position -= m_takeEnvOffset;

	if (id == -1)
	{
		m_update       = true;
		m_pointsEdited = true;

		if (m_sorted && !m_points.empty() && position < m_points.back().position)
			m_sorted = false;

		BR_Envelope::EnvPoint newPoint(position, value, (shape < MIN_SHAPE || shape > MAX_SHAPE) ? this->GetDefaultShape() : shape, 0, selected, 0, (shape == 5) ? bezier : 0);
		m_points.push_back(newPoint);

		return true;
	}
	else if (this->ValidateId(id))
	{
		if (m_sorted)
		{
			if ((this->ValidateId(id-1) && position < m_points[id-1].position) || (this->ValidateId(id+1) && position > m_points[id+1].position))
				m_sorted = false;
		}

		if (shape >= MIN_SHAPE && shape <= MAX_SHAPE)
			m_points[id].shape = shape;

		m_points[id].position = position;
		m_points[id].value    = value;
		m_points[id].bezier   = (m_points[id].shape == BEZIER) ? bezier : 0;
		m_points[id].selected = selected;

		m_update       = true;
		m_pointsEdited = true;
		return true;
	}
	else
	{
		return false;
	}
}

int BR_Envelope::DeletePoints (int startId, int endId)
{
	if (endId < startId)
		swap(startId, endId);

	if (startId < 0)        startId = 0;
	if (endId >= (int)m_points.size()) endId = m_points.size() - 1;
	if (!this->ValidateId(startId) || !this->ValidateId(endId))
		return 0;

	m_points.erase(m_points.begin() + startId, m_points.begin() + endId+1);

	m_update       = true;
	m_pointsEdited = true;
	return (endId - startId + 1);
}

int BR_Envelope::DeletePointsInRange (double start, double end)
{
	start -= m_takeEnvOffset;
	end   -= m_takeEnvOffset;

	int pointsErased = 0;
	if (m_sorted)
	{
		int startId = FindPrevious(start, 0);
		while (startId < (int)m_points.size())
		{
			if (this->ValidateId(startId) && m_points[startId].position >= start)
				break;
			else
				++startId;
		}
		if (!this->ValidateId(startId))
			return 0;

		int endId = FindNext(end, 0);
		while (endId >= 0)
		{
			if (this->ValidateId(endId) && m_points[endId].position <= end)
				break;
			else
				--endId;
		}
		if (endId < startId)
			return 0;

		if (!this->ValidateId(endId))
			endId = m_points.size() - 1;
		pointsErased = this->DeletePoints(startId, endId);
	}
	else
	{
		for (vector<BR_Envelope::EnvPoint>::iterator i = m_points.begin(); i != m_points.end();)
		{
			if (i->position >= start && i->position <= end)
			{
				i = m_points.erase(i);
				m_update       = true;
				m_pointsEdited = true;
				++pointsErased;
			}
			else
				++i;
		}
	}

	return pointsErased;
}

void BR_Envelope::UnselectAll ()
{
	for (size_t i = 0; i < m_points.size(); ++i)
		m_points[i].selected = 0;
	m_update = true;
}

void BR_Envelope::UpdateSelected ()
{
	m_pointsSel.clear();
	m_pointsSel.reserve(m_points.size());

	for (size_t i = 0; i < m_points.size(); ++i)
		if (m_points[i].selected)
			m_pointsSel.push_back(i);

	m_rebuildConseq = true;
}

int BR_Envelope::CountSelected ()
{
	return m_pointsSel.size();
}

int BR_Envelope::GetSelected (int id)
{
	return m_pointsSel[id];
}

int BR_Envelope::CountConseq ()
{
	if (m_rebuildConseq)
		this->UpdateConsequential();
	return m_pointsConseq.size();
}

bool BR_Envelope::GetConseq (int idx, int* startId, int* endId)
{
	if (idx < 0 || idx >= this->CountConseq())
		return false;
	else
	{
		if (startId != NULL)
			*startId = m_pointsConseq[idx].first;
		if (endId != NULL)
			*endId = m_pointsConseq[idx].second;
		return true;
	}
}

bool BR_Envelope::ValidateId (int id)
{
	if (id < 0 || id >= (int)m_points.size())
		return false;
	else
		return true;
}

void BR_Envelope::DeleteAllPoints ()
{
	m_points.clear();
	m_sorted = true;
	m_update = true;
}

void BR_Envelope::Sort ()
{
	if (!m_sorted)
	{
		stable_sort(m_points.begin(), m_points.end(), BR_Envelope::EnvPoint::ComparePoints());
		m_sorted = true;
	}
}

int BR_Envelope::CountPoints ()
{
	return m_points.size();
}

int BR_Envelope::Find (double position, double surroundingRange /*=0*/)
{
	position -= m_takeEnvOffset;
	int id = -1;

	if (m_sorted)
	{
		int prevId = this->FindPrevious(position, 0);
		int nextId = prevId + 1;
		double distanceFromPrev = (this->ValidateId(prevId)) ? (position - m_points[prevId].position) : (abs(surroundingRange) + 1);
		double distanceFromNext = (this->ValidateId(nextId)) ? (m_points[nextId].position - position) : (abs(surroundingRange) + 1);

		if (distanceFromPrev <= distanceFromNext)
		{
			if (distanceFromPrev <= surroundingRange)
				id = prevId;
		}
		else
		{
			if (distanceFromNext <= surroundingRange)
				id = nextId;
		}
	}
	else
	{
		int prevId = (m_sorted) ? (this->FindPrevious(position, 0)) : (0);

		for (vector<BR_Envelope::EnvPoint>::iterator i = m_points.begin(); i != m_points.end() ; ++i)
		{
			if (i->position == position)
			{
				id = (int)(i - m_points.begin());
				break;
			}
		}

		// Search in range only if nothing has been found at the exact position
		if (id == -1 && surroundingRange != 0)
		{
			int nextId = this->FindNext(position, 0);
			double distanceFromPrev = (this->ValidateId(prevId)) ? (position - m_points[prevId].position) : (abs(surroundingRange) + 1);
			double distanceFromNext = (this->ValidateId(nextId)) ? (m_points[nextId].position - position) : (abs(surroundingRange) + 1);

			if (distanceFromPrev <= distanceFromNext)
			{
				if (distanceFromPrev <= surroundingRange)
					id = prevId;
			}
			else
			{
				if (distanceFromNext <= surroundingRange)
					id = nextId;
			}
		}
	}

	return id;
}

int BR_Envelope::FindNext (double position)
{
	return this->FindNext(position, m_takeEnvOffset);
}

int BR_Envelope::FindPrevious (double position)
{
	return this->FindPrevious(position, m_takeEnvOffset);
}

int BR_Envelope::FindClosest (double position)
{
	int prevId = this->FindPrevious(position, m_takeEnvOffset);
	if (!this->ValidateId(prevId))
	{
		int nextId = this->FindNext(position, m_takeEnvOffset);
		if (this->ValidateId(nextId))
			return nextId;
		else
			return -1;
	}
	else
	{
		int nextId = (m_sorted) ? prevId + 1 : this->FindNext(position, m_takeEnvOffset);
		if (!this->ValidateId(nextId))
			return prevId;
		else
		{
			double prevPos = m_points[prevId].position + m_takeEnvOffset;
			double nextPos = m_points[nextId].position + m_takeEnvOffset;
			return (GetClosestVal(position, prevPos, nextPos) == prevPos) ? prevId : nextId;
		}
	}
}

double BR_Envelope::ValueAtPosition (double position, bool fastMode /*= false*/)
{
	position -= m_takeEnvOffset;

	const int id = FindPrevious(position, 0);
	const bool faderMode = IsScaledToFader();
	const double playRate = m_take ? GetMediaItemTakeInfo_Value(m_take, "D_PLAYRATE") : 1;

	if (!m_pointsEdited && !fastMode)
	{
		if (m_sampleRate == -1)
			m_sampleRate = ConfigVar<int>("projsrate").value_or(-1);

		double value;
		Envelope_Evaluate(m_envelope, position * playRate, m_sampleRate, 1, &value, NULL, NULL, NULL); // slower than our way with high point count (probably because we use binary search while Cockos uses linear) but more accurate
		return ScaleFromEnvelopeMode(GetEnvelopeScalingMode(m_envelope), value);
	}
	else
	{
		// No previous point?
		if (!this->ValidateId(id))
		{
			int nextId = this->FindFirstPoint();
			if (this->ValidateId(nextId))
				return m_points[nextId].value;
			else
				return this->LaneCenterValue();
		}

		// No next point?
		int nextId = (m_sorted) ? (id + 1) : this->FindNext(m_points[id].position, 0);
		if (!this->ValidateId(nextId))
			return m_points[id].value;

		// Position at the end of transition ?
		if (m_points[nextId].position == position)
			return m_points[this->LastPointAtPos(nextId)].value;

		// Everything else
		double t1 = m_points[id].position;
		double t2 = m_points[nextId].position;
		double v1 = m_points[id].value;
		double v2 = m_points[nextId].value;
		if (faderMode)
		{
			v1 = this->NormalizedDisplayValue(v1);
			v2 = this->NormalizedDisplayValue(v2);
		}

		double returnValue = 0;
		switch (m_points[id].shape)
		{
			case SQUARE:
			{
				returnValue = v1;
			}
			break;

			case LINEAR:
			{
				double t = (position - t1) / (t2 - t1);
				returnValue = (!m_tempoMap) ? (v1 + (v2 - v1) * t) : CalculateTempoAtPosition(v1, v2, t1, t2, position);
			}
			break;

			case FAST_END:                                 // f(x) = x^3
			{
				double t = (position - t1) / (t2 - t1);
				returnValue =  v1 + (v2 - v1) * pow(t, 3);
			}
			break;

			case FAST_START:                               // f(x) = 1 - (1 - x)^3
			{
				double t = (position - t1) / (t2 - t1);
				returnValue =  v1 + (v2 - v1) * (1 - pow(1-t, 3));
			}
			break;

			case SLOW_START_END:                           // f(x) = x^2 * (3-2x)
			{
				double t = (position - t1) / (t2 - t1);
				returnValue =  v1 + (v2 - v1) * (pow(t, 2) * (3 - 2*t));
			}
			break;

			case BEZIER:
			{
				int id0 = (m_sorted) ? (id-1)     : (this->FindPrevious(t1, 0));
				int id3 = (m_sorted) ? (nextId+1) : (this->FindNext(t2, 0));
				double t0 = (!this->ValidateId(id0)) ? (t1) : (m_points[id0].position);
				double v0 = (!this->ValidateId(id0)) ? (v1) : (m_points[id0].value);
				double t3 = (!this->ValidateId(id3)) ? (t2) : (m_points[id3].position);
				double v3 = (!this->ValidateId(id3)) ? (v2) : (m_points[id3].value);
				if (faderMode)
				{
					v0 = this->NormalizedDisplayValue(v0);
					v3 = this->NormalizedDisplayValue(v3);
				}

				double x1, x2, y1, y2, empty;
				LICE_Bezier_FindCardinalCtlPts(0.25, t0, t1, t2, v0, v1, v2, &empty, &x1, &empty, &y1);
				LICE_Bezier_FindCardinalCtlPts(0.25, t1, t2, t3, v1, v2, v3, &x2, &empty, &y2, &empty);

				double tension = m_points[id].bezier;
				x1 += tension * ((tension > 0) ? (t2-x1) : (x1-t1));
				x2 += tension * ((tension > 0) ? (t2-x2) : (x2-t1));
				y1 -= tension * ((tension > 0) ? (y1-v1) : (v2-y1));
				y2 -= tension * ((tension > 0) ? (y2-v1) : (v2-y2));

				x1 = SetToBounds(x1, t1, t2);
				x2 = SetToBounds(x2, t1, t2);
				y1 = SetToBounds(y1, this->MinValueAbs(), this->MaxValueAbs());
				y2 = SetToBounds(y2, this->MinValueAbs(), this->MaxValueAbs());
				returnValue = LICE_CBezier_GetY(t1, x1, x2, t2, v1, y1, y2, v2, position);
			}
			break;
		}

		if (faderMode)
			returnValue = this->RealValue(returnValue);
		return returnValue;
	}
}

double BR_Envelope::NormalizedDisplayValue (double value)
{
	double min = this->LaneMinValue();
	double max = this->LaneMaxValue();
	value = SetToBounds(value, min, max);

	double displayValue = 0;
	if ((this->Type() == VOLUME || this->Type() == VOLUME_PREFX) && this->IsScaledToFader())
	{
		displayValue = SetToBounds(ScaleToEnvelopeMode(1, value) / ScaleToEnvelopeMode(1, max), 0.0, 1.0);
	}
	else if (this->Type() == TEMPO)
	{
		displayValue = (value - min) / (max - min);
	}
	else
	{
		double center = this->LaneCenterValue();
		if (value > center) displayValue = ((value - center) / (max - center) + 1) / 2;
		else                displayValue = ((value - min)    / (center - min)    ) * 0.5;
	}

	return displayValue;
}

double BR_Envelope::RealValue (double normalizedDisplayValue)
{
	double min = this->LaneMinValue();
	double max = this->LaneMaxValue();
	normalizedDisplayValue = SetToBounds(normalizedDisplayValue, 0.0, 1.0);

	double realValue = 0;
	if ((this->Type() == VOLUME || this->Type() == VOLUME_PREFX) && this->IsScaledToFader())
	{
		realValue = SetToBounds(ScaleFromEnvelopeMode(1, normalizedDisplayValue * ScaleToEnvelopeMode(1, max)), min, max);
	}
	else if (this->Type() == TEMPO)
	{
		realValue = min + normalizedDisplayValue * (max - min);
	}
	else
	{
		double center = this->LaneCenterValue();
		if (normalizedDisplayValue > 0.5) realValue = center + (2 * normalizedDisplayValue - 1) * (max - center);
		else                              realValue = min    + (normalizedDisplayValue / 0.5)   * (center - min);
	}

	return realValue;
}

double BR_Envelope::SnapValue (double value)
{
	if (this->Type() == PITCH)
	{
		const int pitchenvrange = ConfigVar<int>("pitchenvrange").value_or(0);
		int mode = pitchenvrange >> 8; // range is in high 8 bits

		if      (mode == 0) return value;
		else if (mode == 1) return Round(value);             // 1 semitone
		else if (mode == 2) return Round(value * 2)   / 2;   // 50 cent
		else if (mode == 3) return Round(value * 4)   / 4;   // 25 cent
		else if (mode == 4) return Round(value * 10)  / 10;  // 10 cent
		else if (mode == 5) return Round(value * 20)  / 20;  // 5  cent
		else                return Round(value * 100) / 100; // 1 cents
	}
	else
	{
		return value;
	}
}

void BR_Envelope::GetSelectedPointsExtrema (double* minimum, double* maximum)
{
	double minVal = 0;
	double maxVal = 0;

	if (m_update)
	{
		bool found = false;
		for (size_t i = 0; i < m_points.size(); ++i)
		{
			if (m_points[i].selected)
			{
				if (!found)
				{
					found = true;
					maxVal = m_points[i].value;
					minVal = m_points[i].value;
				}
				else
				{
					if (m_points[i].value > maxVal) maxVal = m_points[i].value;
					if (m_points[i].value < minVal) minVal = m_points[i].value;
				}
			}
		}
	}
	else
	{
		bool found = false;
		for (size_t i = 0; i < m_pointsSel.size(); ++i)
		{
			int id = m_pointsSel[i];
			if (!found)
			{
				found = true;
				maxVal = m_points[id].value;
				minVal = m_points[id].value;
			}
			else
			{
				if (m_points[id].value > maxVal) maxVal = m_points[id].value;
				if (m_points[id].value < minVal) minVal = m_points[id].value;
			}
		}
	}

	WritePtr(minimum, minVal);
	WritePtr(maximum, maxVal);
}

bool BR_Envelope::GetPointsInTimeSelection (int* startId, int* endId, double* tStart /*=NULL*/, double* tEnd /*=NULL*/)
{
	double start, end;
	GetSet_LoopTimeRange2(NULL, false, false, &start, &end, false);
	WritePtr(tStart, start);
	WritePtr(tEnd, end);

	if (start != end)
	{
		double offset = (this->IsTakeEnvelope()) ? GetMediaItemInfo_Value(GetMediaItemTake_Item(m_take), "D_POSITION") : 0;

		if (startId)
		{
			int id = this->FindPrevious(start, offset) + 1;
			while (this->ValidateId(id))
			{
				if (m_points[id].position >= start)
					break;
				++id;
			}

			*startId = (this->ValidateId(id)) ? id : -1;
			if (*startId != -1 && !CheckBounds(m_points[*startId].position, start, end))
				*startId = -1;
		}

		if (endId)
		{
			int id = this->FindNext(end, offset) - 1;
			while (this->ValidateId(id))
			{
				if (m_points[id].position <= end)
					break;
				--id;
			}

			*endId = (this->ValidateId(id)) ? id : -1;
			if (*endId != -1 && !CheckBounds(m_points[*endId].position, start, end))
				*endId = -1;
		}

		return true;
	}
	else
	{
		WritePtr(startId, -1);
		WritePtr(endId,   -1);
		return false;
	}
}

bool BR_Envelope::VisibleInArrange (int* envHeight, int* yOffset, bool cacheValues /*=false*/)
{
	if (!this->IsVisible())
		return false;

	HWND hwnd = GetArrangeWnd();
	SCROLLINFO si = { sizeof(SCROLLINFO), };
	si.fMask = SIF_ALL;
	CoolSB_GetScrollInfo(hwnd, SB_VERT, &si);

	if (this->IsTakeEnvelope())
	{
		if (!cacheValues || m_height == -1)
			m_height = GetTakeEnvHeight(m_take, &m_yOffset);

		WritePtr(envHeight, m_height);
		WritePtr(yOffset,   m_yOffset);

		int envelopeEnd = m_yOffset + m_height;
		int pageEnd = si.nPos + (int)si.nPage + SCROLLBAR_W;

		if (AreOverlappedEx(m_yOffset, envelopeEnd, si.nPos, pageEnd))
		{
			double arrangeStart, arrangeEnd;
			RECT r; GetWindowRect(hwnd, &r);
			GetSetArrangeView(NULL, false, &arrangeStart, &arrangeEnd);

			double itemStart = GetMediaItemInfo_Value(GetMediaItemTake_Item(m_take), "D_POSITION");
			double itemEnd = itemStart + GetMediaItemInfo_Value(GetMediaItemTake_Item(m_take), "D_LENGTH");

			if (AreOverlappedEx(itemStart, itemEnd, arrangeStart, arrangeEnd))
				return true;
		}
	}
	else
	{
		if (!cacheValues || m_height == -1)
			m_height = GetTrackEnvHeight(m_envelope, &m_yOffset, true, this->GetParent());

		WritePtr(envHeight, m_height);
		WritePtr(yOffset,   m_yOffset);

		if (m_height > 0)
		{
			int pageEnd = si.nPos + (int)si.nPage + SCROLLBAR_W;
			int envelopeEnd = m_yOffset + m_height;

			if (AreOverlappedEx(m_yOffset, envelopeEnd, si.nPos, pageEnd))
				return true;
		}
	}

	return false;
}

void BR_Envelope::MoveArrangeToPoint (int id, int referenceId)
{
	if (this->ValidateId(id))
	{
		double takePosOffset = (!this->IsTakeEnvelope()) ? (0) : GetMediaItemInfo_Value(GetMediaItemTake_Item(m_take), "D_POSITION");

		double pos = m_points[id].position + takePosOffset;
		if (this->ValidateId(referenceId))
			MoveArrangeToTarget(pos, m_points[referenceId].position + takePosOffset);
		else
			CenterArrange(pos);
	}
}

void BR_Envelope::SetTakeEnvelopeTimebase (bool useProjectTime)
{
	m_takeEnvOffset = (useProjectTime && this->IsTakeEnvelope()) ? (GetMediaItemInfo_Value(GetMediaItemTake_Item(m_take), "D_POSITION")) : (0);
}

WDL_FastString BR_Envelope::FormatValue (double value)
{
	WDL_FastString formatedValue;

	if (this->Type() == VOLUME || this->Type() == VOLUME_PREFX)
	{
		static const char* s_unit = __localizeFunc("dB", "common", 0);

		value = VAL2DB(value);
		if (value == NEGATIVE_INF)
		{
			static const char* s_negativeInf = __localizeFunc("-inf", "vol", 0);
			formatedValue.AppendFormatted(256, "%s %s", s_negativeInf, s_unit);
		}
		else if (value == 0)
			formatedValue.AppendFormatted(256, "%#.2lg%s", value, s_unit);
		else if (value > -1 && value < 1)
			formatedValue.AppendFormatted(256, "%#+.2lg%s", value, s_unit);
		else
			formatedValue.AppendFormatted(256, "%#+.3lg%s", value, s_unit);
	}

	else if (this->Type() == PAN || this->Type() == PAN_PREFX)
	{
		static const char* s_unit   = __localizeFunc("%", "common", 0);
		if (value == 0)
		{
			static const char* s_center = __localizeFunc("center", "pan", 0);
			formatedValue.AppendFormatted(256, "%s", s_center);
		}
		else if (value > 0)
		{
			static const char* s_left   = __localizeFunc("L", "pan", 0);
			formatedValue.AppendFormatted(256, "%d%s%s", (int)(value*100), s_unit, s_left);
		}
		else
		{
			static const char* s_right  = __localizeFunc("R", "pan", 0);
			formatedValue.AppendFormatted(256, "%d%s%s", (int)(-value*100), s_unit, s_right);
		}
	}

	else if (this->Type() == WIDTH || this->Type() == WIDTH_PREFX)
	{
		static const char* s_unit = __localizeFunc("%", "common", 0);

		formatedValue.AppendFormatted(256, "%.1lf%s", value*100, s_unit);
	}

	else if (this->Type() == MUTE)
	{
		static const char* s_mute   = __localizeFunc("MUTE", "env", 0);
		static const char* s_unmute = __localizeFunc("UNMUTE", "env", 0);

		formatedValue.AppendFormatted(256, "%s", (value < 0.25) ? s_mute : s_unmute);
	}

	else if (this->Type() == PITCH)
	{
		static const char* s_unit = __localizeFunc("semitones", "env", 0);

		formatedValue.AppendFormatted(256, "%+.4lf %s", value, s_unit);
	}

	else if (this->Type() == PLAYRATE)
	{
		formatedValue.AppendFormatted(256, "%.2lf%s", value, "x");
	}

	else if (this->Type() == TEMPO)
	{
		static const char* s_unit = __localizeFunc(" bpm", "env", 0);
		formatedValue.AppendFormatted(256, "%.3lf%s", value, s_unit);
	}

	else if (this->Type() == PARAMETER)
	{
		if (m_take)
		{
			formatedValue.AppendFormatted(256, "%.2lf", value);
		}
		else
		{
			char tmp[256];
			TrackFX_FormatParamValue(this->GetParent(), this->GetFxId(), this->GetParamId(), value, tmp, sizeof(tmp));

			if (strlen(tmp) == 0) formatedValue.AppendFormatted(256, this->IsTakeEnvelope() ? "%.4lf": "%.2lf", value); // because TrackFX_FormatParamValue() only works with FX that support Cockos VST extensions.
			else                  formatedValue.AppendFormatted(256, "%s", tmp);
		}
	}

	return formatedValue;
}

WDL_FastString BR_Envelope::GetName ()
{
	if (!m_envName.GetLength())
	{
		char envName[64];
		GetEnvelopeName(m_envelope, envName, sizeof(envName));
		m_envName.AppendFormatted(sizeof(envName), "%s", envName);
	}
	return m_envName;
}

MediaItem_Take* BR_Envelope::GetTake ()
{
	return m_take;
}

MediaTrack* BR_Envelope::GetParent ()
{
	if (!m_parent)
	{
		if (this->IsTakeEnvelope())
			m_parent = GetMediaItemTake_Track(m_take);
		else
			m_parent = GetEnvParent(m_envelope);
	}
	return m_parent;
}

TrackEnvelope* BR_Envelope::GetPointer ()
{
	return m_envelope;
}

void BR_Envelope::SetData (void* data)
{
	m_data = data;
}

void* BR_Envelope::GetData ()
{
	return m_data;
}

BR_EnvType BR_Envelope::Type ()
{
	this->FillProperties();
	return m_properties.type;
}

bool BR_Envelope::IsTempo ()
{
	return m_tempoMap;
}

bool BR_Envelope::IsTakeEnvelope ()
{
	if (m_take)
		return true;
	else
		return false;
}

bool BR_Envelope::IsLocked ()
{
	bool locked = false;
	if (IsLockingActive())
	{
		if      (m_take)     locked = ::IsLocked(TAKE_ENV);
		else if (m_tempoMap) locked = ::IsLocked(this->IsVisible() ? TRACK_ENV : TEMPO_MARKERS);
		else                 locked = ::IsLocked(TRACK_ENV);
	}
	return locked;
}

bool BR_Envelope::IsActive ()
{
	this->FillProperties();
	return !!m_properties.active;
}

bool BR_Envelope::IsVisible ()
{
	this->FillProperties();
	return !!m_properties.visible;
}

bool BR_Envelope::IsInLane ()
{
	this->FillProperties();
	return !!m_properties.lane;
}

bool BR_Envelope::IsArmed ()
{
	this->FillProperties();
	return !!m_properties.armed;
}

bool BR_Envelope::IsScaledToFader ()
{
	// no this->FillProperties() because we read it with API when constructing the object
	return (m_properties.faderMode == 1);
}

int BR_Envelope::GetAIoptions()
{
	this->FillProperties();
	return m_properties.AIoptions;
}

int BR_Envelope::GetLaneHeight ()
{
	this->FillProperties();
	return m_properties.height;
}

int BR_Envelope::GetDefaultShape ()
{
	this->FillProperties();
	return m_properties.shape;
}

int BR_Envelope::GetFxId ()
{
	this->FillFxInfo();
	return m_properties.fxId;
}

int BR_Envelope::GetParamId ()
{
	this->FillFxInfo();
	return m_properties.paramId;
}

int BR_Envelope::GetSendId ()
{
	int id = -1;

	if (!this->IsTakeEnvelope())
	{
		MediaTrack* track    = this->GetParent();
		const char* sendType = (this->Type() == VOLUME) ? ("<VOLENV") : ((this->Type() == PAN) ? ("<PANENV") : ("<MUTEENV"));

		// Check normal sends first
		for (int i = 0; i < GetTrackNumSends(track, 0); ++i)
		{
			if (m_envelope == (TrackEnvelope*)GetSetTrackSendInfo(track, 0, i, "P_ENV", (void*)sendType))
			{
				id = i + GetTrackNumSends(track, 1);
				break;
			}
		}

		// Hardware sends if nothing has been found
		if (id == -1)
		{
			for (int i = 0; i < GetTrackNumSends(track, 1); ++i)
			{
				if (m_envelope == (TrackEnvelope*)GetSetTrackSendInfo(track, 1, i, "P_ENV", (void*)sendType))
				{
					id = i;
					break;
				}
			}
		}
	}
	return id;
}

double BR_Envelope::MinValueAbs ()
{
	this->FillProperties();
	return m_properties.minValue;
}

double BR_Envelope::MaxValueAbs ()
{
	this->FillProperties();
	if (this->Type() == VOLUME || this->Type() == VOLUME_PREFX)
		return this->LaneMaxValue();
	return m_properties.maxValue;
}

double BR_Envelope::LaneCenterValue ()
{
	this->FillProperties();
	if ((this->Type() == VOLUME || this->Type() == VOLUME_PREFX) && this->LaneMaxValue() == 1)
		return 0.5;
	else if (this->Type() == TEMPO)
		return (this->LaneMaxValue() + this->LaneMinValue()) / 2;
	else
		return m_properties.centerValue;
}

double BR_Envelope::LaneMinValue ()
{
	if (m_tempoMap)
	{
		const int min = ConfigVar<int>("tempoenvmin").value_or(0);
		return (double)min;
	}
	else if (this->Type() == PITCH)
	{
		const int min = ConfigVar<int>("pitchenvrange").value_or(0);
		return -(min & 0x0F); // range is in low 8 bits
	}
	return this->MinValueAbs();
}

double BR_Envelope::LaneMaxValue ()
{
	if (m_tempoMap)
	{
		const int max = ConfigVar<int>("tempoenvmax").value_or(0);
		return (double)max;
	}
	else if (this->Type() == VOLUME || this->Type() == VOLUME_PREFX)
	{
		static int s_max = -666;
		static int s_val = -666;
		const int max = ConfigVar<int>("volenvrange").value_or(0);

		// LaneMaxValue tends to get called a lot so instead of doing all comparisons each time, rather cache value and do one comparison only
		if (max != s_max)
		{
			if      (max == 3 || max == 1) s_val = 1; // different max value depends on default volume envelope scaling (3 or 1 for 0db etc...)
			else if (max == 2 || max == 0) s_val = 2;
			else if (max == 6 || max == 4) s_val = 4;
			else if (max == 7 || max == 5) s_val = 16;
			else                           s_val = 2;
		}
		return s_val;
	}
	else if (this->Type() == PITCH)
	{
		const int max = ConfigVar<int>("pitchenvrange").value_or(0);
		return max & 0x0F; // range is in low 8 bits
	}
	return this->MaxValueAbs();
}

void BR_Envelope::SetActive (bool active)
{
	if (this->FillProperties() && !!m_properties.active != active)
	{
		m_properties.active  = active;
		m_properties.changed = true;
		m_update             = true;
	}
}

void BR_Envelope::SetAIoptions(int AIoptions)
{
	if (this->FillProperties() && m_properties.AIoptions != AIoptions)
	{
		m_properties.AIoptions = AIoptions;
		m_properties.changed = true;
		m_update = true;
	}
}

void BR_Envelope::SetVisible (bool visible)
{
	if (this->FillProperties() && !!m_properties.visible != visible)
	{
		m_properties.visible = visible;
		m_properties.changed = true;
		m_update             = true;
	}
}

void BR_Envelope::SetInLane (bool lane)
{
	if (!this->IsTakeEnvelope())
	{
		if (this->FillProperties() && !!m_properties.lane != lane)
		{
			m_properties.lane    = lane;
			m_properties.changed = true;
			m_update             = true;
		}
	}
}

void BR_Envelope::SetArmed (bool armed)
{
	if (this->FillProperties() && !!m_properties.armed != armed)
	{
		m_properties.armed   = armed;
		m_properties.changed = true;
		m_update             = true;
	}
}

void BR_Envelope::SetLaneHeight (int height)
{
	if (this->FillProperties() && m_properties.height != height)
	{
		m_properties.height  = height;
		m_properties.changed = true;
		m_update             = true;
	}
}

void BR_Envelope::SetDefaultShape (int shape)
{
	if (this->FillProperties() && m_properties.shape != shape)
	{
		m_properties.shape   = shape;
		m_properties.changed = true;
		m_update             = true;
	}
}

void BR_Envelope::SetScalingToFader (bool faderScaling)
{
	if (this->FillProperties() && (m_properties.type == VOLUME || m_properties.type == VOLUME_PREFX))
	{
		if (!!m_properties.faderMode != faderScaling)
		{
			m_properties.faderMode = faderScaling ? 1 : 0;
			m_properties.changed   = true;
			m_pointsEdited         = true;
			m_update               = true;
		}
	}
}

bool BR_Envelope::Commit (bool force /*=false*/)
{
	if ((force || (m_update && !this->IsLocked())) && m_envelope)
	{
		// Prevents reselection of points in time selection
		const ConfigVar<int> envClickSegMode("envclicksegmode");
		ConfigVarOverride<int> tempEnvClickSegMode(envClickSegMode,
			ClearBit(envClickSegMode.value_or(0), 6));

		const ConfigVar<int> pooledenvs("pooledenvs");
		ConfigVarOverride<int> tempPooledEnvs(pooledenvs, pooledenvs.value_or(0) & (~12));

		// Need to commit whole chunk
		if (m_tempoMap)
		{
			WDL_FastString chunkStart = this->GetProperties();
			for (vector<BR_Envelope::EnvPoint>::iterator i = m_points.begin(); i != m_points.end(); ++i)
				i->Append(chunkStart, true);
			chunkStart.Append(">");
			GetSetObjectState(m_envelope, chunkStart.Get());
			UpdateTempoTimeline();
		}
		// We can update through API (faster)
		else
		{
			PreventUIRefresh(1);

			// If properties were changed, first commit chunk with properties only and one point (one point prevents REAPER
			// from removing envelope completely) (creating points later using API instead of supplying full chunk is faster)
			bool firstPointDone = false;
			if (m_properties.changed || force)
			{
				WDL_FastString chunkStart = this->GetProperties();
				if (!m_points.empty())
				{
					m_points[0].Append(chunkStart, false);
					firstPointDone = true;
				}
				chunkStart.Append(">");
				GetSetObjectState(m_envelope, chunkStart.Get());
			}

			// Delete excess points
			size_t currentCount = CountEnvelopePoints(m_envelope);
			if (currentCount > m_points.size())
			{
				double startTime, endTime;
				if (m_points.size() > 0) GetEnvelopePoint(m_envelope, m_points.size() - 1, &startTime, NULL, NULL, NULL, NULL);
				else                  startTime = 0;
				if (currentCount    > 0) GetEnvelopePoint(m_envelope, currentCount - 1, &endTime,   NULL, NULL, NULL, NULL);
				else                  endTime = 0;

				startTime -= 1;
				endTime   += 1;
				DeleteEnvelopePointRange(m_envelope, startTime, endTime);
			}

			// Edit/insert cached points
			currentCount = CountEnvelopePoints(m_envelope);
			const double playrate = m_take ? GetMediaItemTakeInfo_Value(m_take, "D_PLAYRATE") : 1;
			for (size_t i = firstPointDone; i < currentCount; ++i)
			{
				double value = (m_properties.faderMode != 0) ? ScaleToEnvelopeMode(m_properties.faderMode, m_points[i].value) : m_points[i].value;
				double position = m_points[i].position * playrate;
				SetEnvelopePoint(m_envelope, i, &position, &value, &m_points[i].shape, &m_points[i].bezier, &m_points[i].selected, &g_bTrue);
			}
			for (size_t i = currentCount; i < m_points.size(); ++i)
			{
				double value = (m_properties.faderMode != 0) ? ScaleToEnvelopeMode(m_properties.faderMode, m_points[i].value) : m_points[i].value;
				double position = m_points[i].position * playrate;
				InsertEnvelopePoint(m_envelope, position, value, m_points[i].shape, m_points[i].bezier, m_points[i].selected, &g_bTrue);
			}
			Envelope_SortPoints(m_envelope);

			PreventUIRefresh(-1);
		}

		UpdateArrange();
		m_update       = false;
		m_pointsEdited = false;
		return true;
	}
	return false;
}

int BR_Envelope::FindFirstPoint ()
{
	if (m_points.empty())
		return -1;

	if (m_sorted)
		return 0;
	else
	{
		int id = 0;
		double first = m_points[id].position;
		for (vector<BR_Envelope::EnvPoint>::iterator i = m_points.begin(); i != m_points.end() ; ++i)
			if (i->position < first)
				id = (int)(i - m_points.begin());
		return id;
	}
}

int BR_Envelope::LastPointAtPos (int id)
{
	/* no bounds checking - internal function so caller handles before calling */
	double position = m_points[id].position;
	int lastId = id;

	if (m_sorted)
	{
		for (vector<BR_Envelope::EnvPoint>::iterator i = id + m_points.begin(); i != m_points.end() ; ++i)
		{
			if (i->position == position)
				lastId = (int)(i - m_points.begin());
			else
				break;
		}
	}
	else
	{
		for (vector<BR_Envelope::EnvPoint>::iterator i = m_points.begin(); i != m_points.end() ; ++i)
			if (i->position == position)
				lastId = (int)(i - m_points.begin());
	}

	return lastId;
}

int BR_Envelope::FindNext (double position, double offset)
{
	position -= offset;

	if (m_sorted)
	{
		BR_Envelope::EnvPoint val(position);
		return (int)(upper_bound(m_points.begin(), m_points.end(), val, BR_Envelope::EnvPoint::ComparePoints()) - m_points.begin());
	}
	else
	{
		int id = -1;
		double nextPos = 0;
		bool foundFirst = false;
		for (vector<BR_Envelope::EnvPoint>::iterator i = m_points.begin(); i != m_points.end() ; ++i)
		{
			double currentPos = i->position;
			if (currentPos > position)
			{
				if (foundFirst)
				{
					if (currentPos < nextPos)
					{
						nextPos = currentPos;
						id = (int)(i - m_points.begin());
					}
				}
				else
				{
					nextPos = currentPos;
					id = (int)(i - m_points.begin());
					foundFirst = true;
				}
			}
		}
		return id;
	}
}

int BR_Envelope::FindPrevious (double position, double offset)
{
	position -= offset;

	if (m_sorted)
	{
		BR_Envelope::EnvPoint val(position);
		return (int)(lower_bound(m_points.begin(), m_points.end(), val, BR_Envelope::EnvPoint::ComparePoints()) - m_points.begin())-1;
	}
	else
	{
		int id = -1;
		double prevPos = 0;
		bool foundFirst = false;
		for (vector<BR_Envelope::EnvPoint>::iterator i = m_points.begin(); i != m_points.end() ; ++i)
		{
			double currentPos = i->position;
			if (currentPos < position)
			{
				if (foundFirst)
				{
					if (currentPos >= prevPos)
					{
						prevPos = currentPos;
						id = (int)(i - m_points.begin());
					}
				}
				else
				{
					prevPos = currentPos;
					id = (int)(i - m_points.begin());
					foundFirst = true;
				}
			}
		}
		return id;
	}
}

void BR_Envelope::Build (bool takeEnvelopesUseProjectTime)
{
	if (m_envelope)
	{
		int count = CountEnvelopePoints(m_envelope);
		m_properties.faderMode = (GetEnvelopeScalingMode(m_envelope) == 1) ? 1 : 0;
		m_points.reserve(count);
		m_pointsSel.reserve(count);

		// Since information on partial measures is missing from the API, we need to parse the chunk for tempo map
		if (m_tempoMap)
		{
			char* envState = GetSetObjectState(m_envelope, "");
			char* token = strtok(envState, "\n");
			LineParser lp(false);
			bool start = false;
			int id = -1;
			while (token != NULL)
			{
				lp.parse(token);
				BR_Envelope::EnvPoint point;
				if (point.ReadLine(lp))
				{
					++id;
					start = true;
					m_points.push_back(point);
					if (point.selected == 1)
						m_pointsSel.push_back(id);
				}
				else if (!start)
					AppendLine(m_chunkProperties, token);
				token = strtok(NULL, "\n");
			}
			FreeHeapPtr(envState);
		}
		else
		{
			double playrate = (m_take) ? (GetMediaItemTakeInfo_Value(m_take, "D_PLAYRATE")) : 1;
			for (int i = 0; i < count; ++i)
			{
				BR_Envelope::EnvPoint point;
				GetEnvelopePoint(m_envelope, i, &point.position, &point.value, &point.shape, &point.bezier, &point.selected);
				point.position /= playrate;

				if (m_properties.faderMode != 0)
					point.value = ScaleFromEnvelopeMode(m_properties.faderMode, point.value);

				m_points.push_back(point);
				if (point.selected) m_pointsSel.push_back(i);
			}
		}
	}

	if (takeEnvelopesUseProjectTime && m_take)
		m_takeEnvOffset = GetMediaItemInfo_Value(GetMediaItemTake_Item(m_take), "D_POSITION");
}

void BR_Envelope::UpdateConsequential ()
{
	for (size_t i = 0; i < m_pointsSel.size(); ++i)
	{
		int startId = m_pointsSel[i];
		if (this->GetSelection(startId))
		{
			for (size_t j = i; j < m_pointsSel.size(); ++j)
			{
				int endId = m_pointsSel[j];
				if (this->GetSelection(endId) && !this->GetSelection(endId+1))
				{
					BR_Envelope::IdPair selection = {startId, endId};
					m_pointsConseq.push_back(selection);
					i += j-i;
					break;
				}
			}
		}
	}

	m_rebuildConseq = false;
}

void BR_Envelope::FillFxInfo ()
{
	if (m_properties.paramId == -2)
	{
		MediaTrack* track = this->GetParent();
		bool found = false;

		int fxCount = TrackFX_GetCount(track);
		for (int i = 0; i < fxCount; ++i)
		{
			int paramCount = TrackFX_GetNumParams(track, i);
			for (int j = 0; j < paramCount; ++j)
			{
				if (m_envelope == GetFXEnvelope(track, i, j, false))
				{
					m_properties.paramId = j;
					m_properties.fxId    = i;
					found                = true;
					break;
				}
			}
		}

		if (!found)
		{
			m_properties.paramId = -1;
			m_properties.fxId    = -1;
		}
	}
}

bool BR_Envelope::FillProperties () const
{
	if (!m_properties.filled)
	{
		char* chunk    = NULL;
		bool freeChunk = false;
		if (m_chunkProperties.GetLength())
		{
			size_t size = m_chunkProperties.GetLength()+1;
			if ((chunk = new (nothrow) char[size]))
				memcpy(chunk, m_chunkProperties.Get(), size);
		}
		else
		{
			chunk = GetSetObjectState(m_envelope, "");
			freeChunk = true;
		}

		if (chunk)
		{
			LineParser lp(false);
			char* token = strtok(chunk, "\n");
			while (token != NULL)
			{
				if (!strncmp(token, "PT ", sizeof("PT ")-1))
				{
					break;
				}
				else if (!strncmp(token, "ACT ", sizeof("ACT ")-1))
				{
					lp.parse(token);
					m_properties.active    = lp.gettoken_int(1);
					m_properties.AIoptions = lp.gettoken_int(2);
				}
				else if (!strncmp(token, "VIS ", sizeof("VIS ")-1))
				{
					lp.parse(token);
					m_properties.visible    = lp.gettoken_int(1);
					m_properties.lane       = lp.gettoken_int(2);
					// 3rd field is a deprecated value, always 1.0, see p=2198426
				}
				else if (!strncmp(token, "LANEHEIGHT ", sizeof("LANEHEIGHT ")-1))
				{
					lp.parse(token);
					m_properties.height        = lp.gettoken_int(1);
					m_properties.heightUnknown = lp.gettoken_int(2);
				}
				else if (!strncmp(token, "ARM ", sizeof("ARM ")-1))
				{
					lp.parse(token);
					m_properties.armed = lp.gettoken_int(1);
				}
				else if (!strncmp(token, "DEFSHAPE ", sizeof("DEFSHAPE ")-1))
				{
					lp.parse(token);
					m_properties.shape         = lp.gettoken_int(1);
					// 2nd and 3rd fields are pitch env range/snap settings, -1 = global default, 
					// otherwise these are the LSBand MSB of the "pitchenvrange" config variable, see p=2198426
					m_properties.pitchEnvRange = lp.gettoken_int(2);
					m_properties.pitchEnvSnap  = lp.gettoken_int(3);
				}
				else if (!strncmp(token, "VOLTYPE ", sizeof("VOLTYPE ")-1))
				{
					lp.parse(token);
					m_properties.faderMode = lp.gettoken_int(1);
				}
				else if (strstr(token, "PARMENV"))
				{
					lp.parse(token);
					m_properties.minValue    = lp.gettoken_float(2);
					m_properties.maxValue    = lp.gettoken_float(3);
					m_properties.centerValue = lp.gettoken_float(4);
					m_properties.type = PARAMETER;
					m_properties.paramType.Set(token);
				}
				else if (strstr(token, "VOLENV"))
				{
					m_properties.minValue = 0;
					m_properties.maxValue = 16;
					m_properties.centerValue = 1;
					m_properties.type = (strstr(token, "AUXVOLENV") || strstr(token, "VOLENV2") || strstr(token, "HWVOLENV")) ? VOLUME : VOLUME_PREFX;
					m_properties.paramType.Set(token);
				}
				else if (strstr(token, "PANENV"))
				{
					m_properties.minValue = -1;
					m_properties.maxValue = 1;
					m_properties.centerValue = 0;
					m_properties.type = (strstr(token, "AUXPANENV") || strstr(token, "PANENV2") || strstr(token, "HWPANENV")) ? PAN : PAN_PREFX;
					m_properties.paramType.Set(token);
				}
				else if (strstr(token, "WIDTHENV"))
				{
					m_properties.minValue = -1;
					m_properties.maxValue = 1;
					m_properties.centerValue = 0;
					m_properties.type = (strstr(token, "WIDTHENV2")) ? WIDTH : WIDTH_PREFX;
					m_properties.paramType.Set(token);
				}
				else if (strstr(token, "MUTEENV"))
				{
					m_properties.minValue = 0;
					m_properties.maxValue = 1;
					m_properties.centerValue = 0.5;
					m_properties.type = MUTE;
					m_properties.paramType.Set(token);
				}
				else if (strstr(token, "SPEEDENV"))
				{
					m_properties.minValue = 0.1;
					m_properties.maxValue = 4;
					m_properties.centerValue = 1;
					m_properties.type = PLAYRATE;
					m_properties.paramType.Set(token);
				}
				else if (strstr(token, "PITCHENV"))
				{
					m_properties.minValue = -255;
					m_properties.maxValue = 255;
					m_properties.centerValue = 0;
					m_properties.type = PITCH;
					m_properties.paramType.Set(token);
				}
				else if (strstr(token, "TEMPOENV"))
				{
					m_properties.minValue = MIN_BPM;
					m_properties.maxValue = MAX_BPM;
					m_properties.centerValue = (m_properties.maxValue + m_properties.minValue) / 2;
					m_properties.type = TEMPO;
					m_properties.paramType.Set(token);
				}
				else if (strstr(token, "POOLEDENVINST"))
				{
					m_properties.automationItems.push_back(WDL_FastString(token));
				}
				else if (strstr(token, "<EXT"))
				{
					int subchunks = 0; // <BIN, https://github.com/reaper-oss/sws/pull/1387#issuecomment-705651041

					do
					{
						if (*token == '<')
							++subchunks;
						else if (!strcmp(token, ">"))
							--subchunks;

						AppendLine(m_properties.EXT, token);
						token = strtok(NULL, "\n");
					} while (token && subchunks > 0);
				}

				token = strtok(NULL, "\n");
			}

			if (freeChunk) FreeHeapPtr(chunk);
			else           delete [] chunk;
			m_properties.filled = true;
		}
	}

	return m_properties.filled;
}

WDL_FastString BR_Envelope::GetProperties ()
{
	if (m_properties.filled || !m_envelope)
	{
		WDL_FastString properties;
		properties.Append(m_properties.paramType.Get());
		properties.Append("\n");
		properties.AppendFormatted(256, "ACT %d %d\n", m_properties.active, m_properties.AIoptions);
		properties.AppendFormatted(256, "VIS %d %d 1\n", m_properties.visible, m_properties.lane);
		properties.AppendFormatted(256, "LANEHEIGHT %d %d\n", m_properties.height, m_properties.heightUnknown);
		properties.AppendFormatted(256, "ARM %d\n", m_properties.armed);
		properties.AppendFormatted(256, "DEFSHAPE %d %d %d\n", m_properties.shape, m_properties.pitchEnvRange, m_properties.pitchEnvSnap);
		for (int i = 0; i < (int)m_properties.automationItems.size(); ++i)
		{
			properties.Append(m_properties.automationItems[i].Get());
			properties.Append("\n");
		}
		if (m_properties.faderMode != 0) properties.AppendFormatted(256, "VOLTYPE %d\n", 1);
		if (m_properties.EXT.GetLength())
			properties.Append(m_properties.EXT.Get());
		return properties;
	}
	else
	{
		if (m_chunkProperties.GetLength() == 0)
		{
			this->FillProperties();
			return this->GetProperties();
		}
		else
		{
			return m_chunkProperties;
		}
	}
}

BR_Envelope::EnvProperties::EnvProperties () :
active        (0),
AIoptions     (-1),
visible       (0),
lane          (0),
height        (0),
heightUnknown (0),
armed         (0),
shape         (0),
pitchEnvRange (0),
pitchEnvSnap  (0),
faderMode     (0),
type          (UNKNOWN),
minValue      (0),
maxValue      (0),
centerValue   (0),
paramId       (-2),
fxId          (-2),
filled        (false),
changed       (false)
{
}

BR_Envelope::EnvProperties::EnvProperties (const EnvProperties& properties) :
active          (properties.active),
AIoptions       (-1),
visible         (properties.visible),
lane            (properties.lane),
height          (properties.height),
heightUnknown   (properties.heightUnknown),
armed           (properties.armed),
shape           (properties.shape),
pitchEnvRange   (properties.pitchEnvRange),
pitchEnvSnap    (properties.pitchEnvSnap),
faderMode       (properties.faderMode),
type            (properties.type),
minValue        (properties.minValue),
maxValue        (properties.maxValue),
centerValue     (properties.centerValue),
paramId         (properties.paramId),
fxId            (properties.fxId),
filled          (properties.filled),
changed         (properties.changed),
paramType       (properties.paramType),
automationItems (properties.automationItems)
{
}

BR_Envelope::EnvProperties& BR_Envelope::EnvProperties::operator= (const EnvProperties& properties)
{
	if (this == &properties)
		return *this;

	active          = properties.active;
	AIoptions       = properties.AIoptions;
	visible         = properties.visible;

	lane            = properties.lane;
	height          = properties.height;
	heightUnknown   = properties.heightUnknown;
	armed           = properties.armed;
	shape           = properties.shape;
	pitchEnvRange   = properties.pitchEnvRange;
	pitchEnvSnap    = properties.pitchEnvSnap;
	faderMode       = properties.faderMode;
	type            = properties.type;
	minValue        = properties.minValue;
	maxValue        = properties.maxValue;
	centerValue     = properties.centerValue;
	paramId         = properties.paramId;
	fxId            = properties.fxId;
	filled          = properties.filled;
	changed         = properties.changed;
	automationItems = properties.automationItems;
	paramType.Set(&properties.paramType);

	return *this;
}

BR_Envelope::EnvPoint::EnvPoint () :
position   (0),
value      (0),
bezier     (0),
selected   (false),
shape      (0),
sig        (0),
partial    (0),
metronome1 (0),
metronome2 (0)
{
}

BR_Envelope::EnvPoint::EnvPoint (double position, double value, int shape, int sig, bool selected, int partial, double bezier) :
position   (position),
value      (value),
bezier     (bezier),
selected   (selected),
shape      (shape),
sig        (sig),
partial    (partial),
metronome1 (0),
metronome2 (0)
{
}

BR_Envelope::EnvPoint::EnvPoint (double position) :
position   (position),
value      (0),
bezier     (0),
selected   (false),
shape      (0),
sig        (0),
partial    (0),
metronome1 (0),
metronome2 (0)
{
}

bool BR_Envelope::EnvPoint::operator==(const EnvPoint &point) const
{
	return
		this->position == point.position &&
		this->value == point.value &&
		this->bezier == point.bezier &&
		this->shape == point.shape &&
		this->sig == point.sig &&
		this->selected == point.selected &&
		this->partial == point.partial
		;
}

bool BR_Envelope::EnvPoint::ReadLine (const LineParser& lp)
{
	if (strcmp(lp.gettoken_str(0), "PT"))
		return false;
	else
	{
		this->position   = lp.gettoken_float(1);
		this->value      = lp.gettoken_float(2);
		this->shape      = lp.gettoken_int(3);
		this->sig        = lp.gettoken_int(4);
		this->selected   = (lp.gettoken_int(5)&1)==1;
		this->partial    = lp.gettoken_int(6);
		this->bezier     = lp.gettoken_float(7);
		this->metronome1 = lp.gettoken_uint(9);
		this->metronome2 = lp.gettoken_uint(10);
		tempoStr.Append(lp.gettoken_str(8));

		return true;
	}
}

void BR_Envelope::EnvPoint::Append (WDL_FastString& string, bool tempoPoint)
{
	if (tempoPoint)
	{
		string.AppendFormatted
		(
			256,
			"PT %.12lf %.10lf %d %d %d %d %.8lf \"%s\" %u %u\n",
			this->position,
			this->value,
			this->shape,
			this->sig,
			this->selected ? 1 : 0,
			this->partial,
			this->bezier,
			this->tempoStr.Get(),
			this->metronome1,
			this->metronome2
		);
	}
	else
	{
		string.AppendFormatted
		(
			256,
			"PT %.12lf %.10lf %d %d %d %d %.8lf\n",
			this->position,
			this->value,
			this->shape,
			this->sig,
			this->selected ? 1 : 0,
			this->partial,
			this->bezier
		);
	}
}

/******************************************************************************
* Miscellaneous                                                               *
******************************************************************************/
TrackEnvelope* GetTempoEnv ()
{
	return GetTrackEnvelopeByName(CSurf_TrackFromID(0, false),  __localizeFunc("Tempo map", "env", 0));
}

TrackEnvelope* GetVolEnv (MediaTrack* track)
{
	return SWS_GetTrackEnvelopeByName(track, "Volume");
}

TrackEnvelope* GetVolEnvPreFX (MediaTrack* track)
{
	return SWS_GetTrackEnvelopeByName(track, "Volume (Pre-FX)");
}

TrackEnvelope* GetTakeEnv (MediaItem_Take* take, BR_EnvType envelope)
{
	const char* envName = NULL;
	if      (envelope == VOLUME) envName = "Volume";
	else if (envelope == PAN)    envName = "Pan";
	else if (envelope == MUTE)   envName = "Mute";
	else if (envelope == PITCH)  envName = "Pitch";

	return (envName) ? SWS_GetTakeEnvelopeByName(take, envName) : NULL;
}

MediaItem_Take* GetTakeEnvParent (TrackEnvelope* envelope, BR_EnvType* type)
{
	if (envelope)
	{
		const int itemCount = CountMediaItems(NULL);
		for (int i = 0; i < itemCount; ++i)
		{
			MediaItem* item = GetMediaItem(NULL, i);
			const int takeCount = CountTakes(item);
			for (int j = 0; j < takeCount; ++j)
			{
				MediaItem_Take* take = GetTake(item, j);
				BR_EnvType returnType = UNKNOWN;

				if      (GetTakeEnv(take, VOLUME) == envelope) returnType = VOLUME;
				else if (GetTakeEnv(take, PAN)    == envelope) returnType = PAN;
				else if (GetTakeEnv(take, MUTE)   == envelope) returnType = MUTE;
				else if (GetTakeEnv(take, PITCH)  == envelope) returnType = PITCH;
				else
				{
					const int envelopeCount = CountTakeEnvelopes(take);
					for (int k = 0; k < envelopeCount; ++k)
					{
						if (GetTakeEnvelope(take, k) == envelope)
						{
							returnType = PARAMETER;
							break;
						}
					}
				}

				if (returnType != UNKNOWN)
				{
					WritePtr(type, returnType);
					return take;
				}
			}
		}
	}

	WritePtr(type, UNKNOWN);
	return NULL;
}

MediaTrack* GetEnvParent (TrackEnvelope* envelope)
{
	int count = CountTracks(NULL);
	for (int i = -1; i < count; ++i)
	{
		MediaTrack* track = (i == -1) ? (GetMasterTrack(NULL)) : (GetTrack(NULL, i));

		int count = CountTrackEnvelopes(track);
		for (int i = 0; i < count; ++i)
			if (envelope == GetTrackEnvelope(track, i))
				return track;
	}
	return NULL;
}

vector<int> GetSelPoints (TrackEnvelope* envelope)
{
	char* envState = GetSetObjectState(envelope, "");
	char* token = strtok(envState, "\n");

	vector<int> selectedPoints;
	LineParser lp(false);
	int id = -1;
	while (token != NULL)
	{
		lp.parse(token);
		if (!strcmp(lp.gettoken_str(0), "PT"))
		{
			++id;
			if ((lp.gettoken_int(5)&1)==1)
				selectedPoints.push_back(id);
		}
		token = strtok(NULL, "\n");
	}
	FreeHeapPtr(envState);
	return selectedPoints;
}

WDL_FastString ConstructReceiveEnv (BR_EnvType type, double firstPointValue, bool hardwareSend)
{
	WDL_FastString envelope;

	if (type != VOLUME && type != PAN && type != MUTE)
		return envelope;

	const int defAutoMode = ConfigVar<int>("defautomode").value_or(0);
	const int envLanes = ConfigVar<int>("envlanes").value_or(0);

	BR_EnvShape defShape = (type == MUTE) ? SQUARE : GetDefaultPointShape();

	if      (type == VOLUME) (hardwareSend) ? AppendLine(envelope, "<HWVOLENV")  : AppendLine(envelope, "<AUXVOLENV");
	else if (type == PAN)    (hardwareSend) ? AppendLine(envelope, "<HWPANENV")  : AppendLine(envelope, "<AUXPANENV");
	else if (type == MUTE)   (hardwareSend) ? AppendLine(envelope, "<HWMUTEENV") : AppendLine(envelope, "<AUXMUTEENV");

	const int volenvrange = ConfigVar<int>("volenvrange").value_or(0);
	envelope.AppendFormatted(128, "%s %d\n",             "ACT", 1);
	envelope.AppendFormatted(128, "%s %d %d %d\n",       "VIS", 1, GetBit(envLanes, 0), 1);
	envelope.AppendFormatted(128, "%s %d %d\n",          "LANEHEIGHT", 0, 0);
	envelope.AppendFormatted(128, "%s %d\n",             "ARM", !GetBit(defAutoMode, 9));
	envelope.AppendFormatted(128, "%s %d %d %d\n",       "DEFSHAPE", defShape, -1, -1);
	if (GetBit(volenvrange, 1))
		envelope.AppendFormatted(128, "%s %d\n", "VOLTYPE", 1);

	envelope.AppendFormatted(128, "%s %.8lf %.8lf %d\n", "PT", 0.0, firstPointValue, defShape);
	AppendLine(envelope, ">");

	return envelope;
}

bool ToggleShowSendEnvelope (MediaTrack* track, int sendId, BR_EnvType type)
{
	bool hwSend = (sendId < GetTrackNumSends(track, 1));
	MediaTrack* receiveTrack = hwSend ? track : (MediaTrack*)GetSetTrackSendInfo(track, 0, sendId, "P_DESTTRACK", NULL);

	bool update = false;
	if (receiveTrack && (type == VOLUME || type == PAN || type == MUTE))
	{
		int sendTrackId = CSurf_TrackToID(track, false) - 1; // -1 so it's the same id receives use

		// In case there are multiple sends to same receive track (or multiple hardware sends from the same track)
		int sendNum = 0;

		if (hwSend)
		{
			sendNum = sendId + 1;
		}
		else
		{
			for (int i = 0; i < GetTrackNumSends(track, 0); ++i)
			{
				if (i > sendId)
					break;
				if ((MediaTrack*)GetSetTrackSendInfo(track, 0, i, "P_DESTTRACK", NULL) == receiveTrack)
					++sendNum;
			}
		}

		WDL_FastString newState;
		char* trackState = GetSetObjectState(receiveTrack, "");
		bool stateUpdated = false;
		if (trackState)
		{
			LineParser lp(false);
			int blockCount = 0;
			int currentSendNum = 0;
			char* token = strtok(trackState, "\n");
			while (token != NULL)
			{
				lp.parse(token);
				if      (lp.gettoken_str(0)[0] == '<')  ++blockCount;
				else if (lp.gettoken_str(0)[0] == '>')  --blockCount;

				if (blockCount == 1 && ((!hwSend && !strcmp(lp.gettoken_str(0), "AUXRECV") && lp.gettoken_int(1) == sendTrackId) || (hwSend && !strcmp(lp.gettoken_str(0), "HWOUT"))))
				{
					++currentSendNum;
					if (currentSendNum == sendNum)
					{
						int sendTrackId   = lp.gettoken_int(1);
						double sendVolume = lp.gettoken_float(3);
						double sendPan    = -lp.gettoken_float(4);
						double sendMute   = (double)!lp.gettoken_float(5);

						WDL_FastString receiveLine, volEnv, panEnv, muteEnv;
						receiveLine.Set(token);

						int    pointCount     = 0;
						double firstPointVal  = 0;
						bool   envelopeHidden = false;

						token = strtok(NULL, "\n");
						while (token != NULL)
						{
							lp.parse(token);
							if ((!hwSend && (!strcmp(lp.gettoken_str(0), "<AUXVOLENV") || !strcmp(lp.gettoken_str(0), "<AUXPANENV") || !strcmp(lp.gettoken_str(0), "<AUXMUTEENV"))) ||
							    ( hwSend && (!strcmp(lp.gettoken_str(0), "<HWVOLENV")  || !strcmp(lp.gettoken_str(0), "<HWPANENV")  || !strcmp(lp.gettoken_str(0), "<HWMUTEENV"))))
							{
								WDL_FastString* currentEnvState = NULL;
								BR_EnvType currentEnv = UNKNOWN;

								if      (!strcmp(lp.gettoken_str(0), "<AUXVOLENV")  || !strcmp(lp.gettoken_str(0), "<HWVOLENV"))  {currentEnv = VOLUME; currentEnvState = &volEnv;}
								else if (!strcmp(lp.gettoken_str(0), "<AUXPANENV")  || !strcmp(lp.gettoken_str(0), "<HWPANENV"))  {currentEnv = PAN;    currentEnvState = &panEnv;}
								else if (!strcmp(lp.gettoken_str(0), "<AUXMUTEENV") || !strcmp(lp.gettoken_str(0), "<HWMUTEENV")) {currentEnv = MUTE;   currentEnvState = &muteEnv;}

								// Save current send envelope
								while (token != NULL)
								{
									lp.parse(token);

									if (!strcmp(lp.gettoken_str(0), "VIS") && (type & currentEnv))
									{
										for (int i = 0; i < lp.getnumtokens(); ++i)
										{
											if (i == 1)
											{
												if (lp.gettoken_int(1) == 0)
												{
													currentEnvState->Append("1");
												}
												else
												{
													currentEnvState->Append("0");
													envelopeHidden = true;
												}
												stateUpdated = true;
											}
											else
												currentEnvState->Append(lp.gettoken_str(i));

											currentEnvState->Append(" ");
										}
										currentEnvState->Append("\n");
									}
									else
									{
										if (!strcmp(lp.gettoken_str(0), "PT") && (type & currentEnv))
										{
											++pointCount;
											if (pointCount == 1)
												firstPointVal = lp.gettoken_float(2);
										}
										AppendLine(*currentEnvState, token);
									}

									if (lp.gettoken_str(0)[0] == '>')
										break;
									else
										token = strtok(NULL, "\n");
								}
							}
							else
								break;
							token = strtok(NULL, "\n");
						}

						bool trim = false;
						const int trimMode = ConfigVar<int>("envtrimadjmode").value_or(0);
						if (trimMode == 0 || (trimMode == 1 && GetEffectiveAutomationMode(hwSend ? track : CSurf_TrackFromID(sendTrackId + 1, false)) != 0))
						{
							trim = true;
							WDL_FastString newReceiveLine;
							lp.parse(receiveLine.Get());
							for (int i = 0; i < lp.getnumtokens(); ++i)
							{
								if (i == 3 && (type & VOLUME))
								{
									if (pointCount <= 1 && envelopeHidden)
										newReceiveLine.AppendFormatted(256, "%lf", firstPointVal);
									else if (!volEnv.GetLength())
										newReceiveLine.Append("1");
									else
										newReceiveLine.Append(lp.gettoken_str(i));
								}
								else if (i == 4 && (type & PAN))
								{
									if (pointCount <= 1 && envelopeHidden)
										newReceiveLine.AppendFormatted(256, "%lf", -firstPointVal);
									else if (!panEnv.GetLength())
										newReceiveLine.Append("0");
									else
										newReceiveLine.Append(lp.gettoken_str(i));
								}
								else
								{
									newReceiveLine.Append(lp.gettoken_str(i));
								}

								newReceiveLine.Append(" ");
							}

							AppendLine(newState, newReceiveLine.Get());
						}
						else
							AppendLine(newState, receiveLine.Get());

						if ((type & VOLUME) && !volEnv.GetLength())  {volEnv  = ConstructReceiveEnv(VOLUME, trim ? sendVolume : 1, hwSend); stateUpdated = true;}
						if ((type & PAN)    && !panEnv.GetLength())  {panEnv  = ConstructReceiveEnv(PAN,    trim ? sendPan : 0,    hwSend); stateUpdated = true;}
						if ((type & MUTE)   && !muteEnv.GetLength()) {muteEnv = ConstructReceiveEnv(MUTE,   sendMute,              hwSend); stateUpdated = true;}

						if (volEnv.GetLength()  && (!(type & VOLUME) || (!envelopeHidden || pointCount >= 2)))
							newState.Append(volEnv.Get());
						if (panEnv.GetLength()  && (!(type & PAN)    || (!envelopeHidden || pointCount >= 2)))
							newState.Append(panEnv.Get());
						if (muteEnv.GetLength() && (!(type & MUTE)   || (!envelopeHidden || pointCount >= 2)))
							newState.Append(muteEnv.Get());
					}
					else
					{
						AppendLine(newState, token);
						token = strtok(NULL, "\n");
					}
				}
				else
				{
					AppendLine(newState, token);
					token = strtok(NULL, "\n");
				}
			}
		}

		if (stateUpdated)
		{
			GetSetObjectState(receiveTrack, newState.Get());
			update = true;
		}
		FreeHeapPtr(trackState);
	}

	return update;
}

bool ShowSendEnvelopes (vector<MediaTrack*>& tracks, BR_EnvType envelopeTypes)
{
	if (!(envelopeTypes & MUTE) && !(envelopeTypes & PAN) && !(envelopeTypes & VOLUME))
		return false;

	set<int> trackIds;
	vector<MediaTrack*> receiveTracks;
	for (size_t i = 0; i < tracks.size(); ++i)
	{
		MediaTrack* track = tracks[i];
		int id = CSurf_TrackToID(track, false);
		if (id > 0)
		{
			trackIds.insert(id - 1);  // -1 so it's the same id receives use
			for (int j = 0; j < GetTrackNumSends(track, 0); ++j)
				receiveTracks.push_back((MediaTrack*)GetSetTrackSendInfo(track, 0, j, "P_DESTTRACK", NULL));
		}

	}

	// For hardware sends
	size_t receivesStop = receiveTracks.size() - 1;
	for (size_t i = 0; i < tracks.size(); ++i)
		receiveTracks.push_back(tracks[i]);

	bool update = false;
	for (size_t i = 0; i < receiveTracks.size(); i++)
	{
		if (MediaTrack* track = receiveTracks[i])
		{
			bool hwSend = (i > receivesStop);
			bool stateUpdated = false;
			WDL_FastString newState;

			// Parse chunk and rewrite it in newState
			char* trackState = GetSetObjectState(track, "");
			if (trackState)
			{
				LineParser lp(false);
				int blockCount = 0;

				char* token = strtok(trackState, "\n");
				while (token != NULL)
				{
					lp.parse(token);
					if      (lp.gettoken_str(0)[0] == '<')  ++blockCount;
					else if (lp.gettoken_str(0)[0] == '>')  --blockCount;

					if (blockCount == 1 && ((!strcmp(lp.gettoken_str(0), "HWOUT") && hwSend) || (!strcmp(lp.gettoken_str(0), "AUXRECV") && !hwSend && trackIds.find(lp.gettoken_int(1)) != trackIds.end())))
					{
						int sendTrackId   = lp.gettoken_int(1);
						double sendVolume = lp.gettoken_float(3);
						double sendPan    = -lp.gettoken_float(4);
						double sendMute   = (double)!lp.gettoken_float(5);

						WDL_FastString receiveLine, volEnv, panEnv, muteEnv;
						receiveLine.Set(token);

						token = strtok(NULL, "\n");
						while (token != NULL)
						{
							lp.parse(token);
							if ((!hwSend && (!strcmp(lp.gettoken_str(0), "<AUXVOLENV") || !strcmp(lp.gettoken_str(0), "<AUXPANENV") || !strcmp(lp.gettoken_str(0), "<AUXMUTEENV"))) ||
							    ( hwSend && (!strcmp(lp.gettoken_str(0), "<HWVOLENV")  || !strcmp(lp.gettoken_str(0), "<HWPANENV")  || !strcmp(lp.gettoken_str(0), "<HWMUTEENV"))))
							{
								WDL_FastString* currentEnvState = NULL;
								BR_EnvType currentEnv = UNKNOWN;

								if      (!strcmp(lp.gettoken_str(0), "<AUXVOLENV")  || !strcmp(lp.gettoken_str(0), "<HWVOLENV"))  {currentEnv = VOLUME; currentEnvState = &volEnv;}
								else if (!strcmp(lp.gettoken_str(0), "<AUXPANENV")  || !strcmp(lp.gettoken_str(0), "<HWPANENV"))  {currentEnv = PAN;    currentEnvState = &panEnv;}
								else if (!strcmp(lp.gettoken_str(0), "<AUXMUTEENV") || !strcmp(lp.gettoken_str(0), "<HWMUTEENV")) {currentEnv = MUTE;   currentEnvState = &muteEnv;}

								// Save current send envelope
								while (token != NULL)
								{
									lp.parse(token);

									if (!strcmp(lp.gettoken_str(0), "VIS") && (envelopeTypes & currentEnv))
									{
										for (int i = 0; i < lp.getnumtokens(); ++i)
										{
											if (i == 1)
											{
												if (lp.gettoken_int(1) == 0)
													stateUpdated = true;
												currentEnvState->Append("1");
											}
											else
												currentEnvState->Append(lp.gettoken_str(i));

											currentEnvState->Append(" ");
										}
										currentEnvState->Append("\n");
									}
									else
										AppendLine(*currentEnvState, token);

									if (lp.gettoken_str(0)[0] == '>')
										break;
									else
										token = strtok(NULL, "\n");
								}
							}
							else
								break;
							token = strtok(NULL, "\n");
						}

						bool trim = false;
						const int trimMode = ConfigVar<int>("envtrimadjmode").value_or(0);
						if (trimMode == 0 || (trimMode == 1 && GetEffectiveAutomationMode(hwSend ? track : CSurf_TrackFromID(sendTrackId + 1, false)) != 0))
						{
							trim = true;
							WDL_FastString newReceiveLine;
							lp.parse(receiveLine.Get());
							for (int i = 0; i < lp.getnumtokens(); ++i)
							{
								if      (i == 3 && (envelopeTypes & VOLUME) && !volEnv.GetLength()) newReceiveLine.Append("1");
								else if (i == 4 && (envelopeTypes & PAN)    && !panEnv.GetLength()) newReceiveLine.Append("0");
								else                                                                newReceiveLine.Append(lp.gettoken_str(i));

								newReceiveLine.Append(" ");
							}

							AppendLine(newState, newReceiveLine.Get());
						}
						else
							AppendLine(newState, receiveLine.Get());

						if ((envelopeTypes & VOLUME) && !volEnv.GetLength())  {volEnv  = ConstructReceiveEnv(VOLUME, trim ? sendVolume : 1, hwSend); stateUpdated = true;}
						if ((envelopeTypes & PAN)    && !panEnv.GetLength())  {panEnv  = ConstructReceiveEnv(PAN,    trim ? sendPan : 0,    hwSend); stateUpdated = true;}
						if ((envelopeTypes & MUTE)   && !muteEnv.GetLength()) {muteEnv = ConstructReceiveEnv(MUTE,   sendMute,              hwSend); stateUpdated = true;}

						if (volEnv.GetLength())  newState.Append(volEnv.Get());
						if (panEnv.GetLength())  newState.Append(panEnv.Get());
						if (muteEnv.GetLength()) newState.Append(muteEnv.Get());
					}
					else
					{
						AppendLine(newState, token);
						token = strtok(NULL, "\n");
					}
				}
			}

			if (stateUpdated)
			{
				GetSetObjectState(track, newState.Get());
				update = true;
			}
			FreeHeapPtr(trackState);
		}
	}

	return update;
}

bool EnvVis (TrackEnvelope* envelope, bool* lane)
{
	if (char* tmp = new (nothrow) char[128])
	{
		tmp[0] = 0;
		GetSetEnvelopeState(envelope, tmp, 128);
		LineParser lp(false);

		char* token = strtok(tmp, "\n");
		while (token != NULL)
		{
			lp.parse(token);
			if (!strcmp(lp.gettoken_str(0), "VIS"))
			{
				delete[] tmp;
				WritePtr(lane, !!lp.gettoken_int(2));
				return !!lp.gettoken_int(1);
			}
			token = strtok(NULL, "\n");
		}
		delete[] tmp;
	}

	WritePtr(lane, false);
	return false;
}

int GetEnvId (TrackEnvelope* envelope, MediaTrack* parent /*= NULL*/)
{
	MediaTrack* track = (parent) ? (parent) : (GetEnvParent(envelope));

	int count = CountTrackEnvelopes(track);
	for (int i = 0; i < count; ++i)
	{
		if (envelope == GetTrackEnvelope(track, i))
			return i;
	}
	return -1;
}

int GetEffectiveAutomationMode (MediaTrack* track)
{
	int override = GetGlobalAutomationOverride();
	return (override == -1 || override == 5) ? (int)GetMediaTrackInfo_Value(track, "I_AUTOMODE") : override;
}

int CountTrackEnvelopePanels (MediaTrack* track)
{
	const int tcp_h_no_envs = (int)GetMediaTrackInfo_Value(track, "I_TCPH");
	const int tcp_h = (int)GetMediaTrackInfo_Value(track, "I_WNDH");
	if (tcp_h <= tcp_h_no_envs) return 0;

	const int fullEnvCount = CountTrackEnvelopes(track);
	int count = 0;
	for (int i = 0; i < fullEnvCount; ++i)
	{
		TrackEnvelope *env = GetTrackEnvelope(track,i);
		if (GetEnvelopeInfo_Value(env,"I_TCPY") >= tcp_h_no_envs)
		{
			if (GetEnvelopeInfo_Value(env,"I_TCPH") > 0)
				count++;
		}
	}

	return count;
}

BR_EnvShape GetDefaultPointShape ()
{
	const int defEnvs = ConfigVar<int>("defenvs").value_or(0);
	return static_cast<BR_EnvShape>(defEnvs >> 16);
}

BR_EnvType GetEnvType (TrackEnvelope* envelope, bool* isSend, bool* isHwSend)
{
	static const char* volumePreFX     = __localizeFunc("Volume (Pre-FX)", "envname", 0);
	static const char* panPreFX        = __localizeFunc("Pan (Pre-FX)", "envname", 0);
	static const char* widthPreFX      = __localizeFunc("Width (Pre-FX)", "envname", 0);
	static const char* volume          = __localizeFunc("Volume", "envname", 0);
	static const char* pan             = __localizeFunc("Pan", "envname", 0);
	static const char* width           = __localizeFunc("Width", "envname", 0);
	static const char* mute            = __localizeFunc("Mute", "envname", 0);
	static const char* sendHardVolume  = __localizeFunc("Audio Hardware Output: Volume", "envname", 0);
	static const char* sendHardPan     = __localizeFunc("Audio Hardware Output: Pan", "envname", 0);
	static const char* sendHardMute    = __localizeFunc("Audio Hardware Output: Mute", "envname", 0);
	static const char* sendVolume      = __localizeFunc("Send Volume", "envname", 0);
	static const char* sendPan         = __localizeFunc("Send Pan", "envname", 0);
	static const char* sendMute        = __localizeFunc("Send Mute", "envname", 0);
	static const char* takePitch       = __localizeFunc("Pitch", "item", 0);
	static const char* takeVolume      = __localizeFunc("Volume", "item", 0);
	static const char* takePan         = __localizeFunc("Pan", "item", 0);
	static const char* takeMute        = __localizeFunc("Mute", "item", 0);
	static const char* playrate        = __localizeFunc("Playrate", "env", 0);
	static const char* tempo           = __localizeFunc("Tempo map", "env", 0);

	char name[512];
	GetEnvelopeName(envelope, name, sizeof(name));

	BR_EnvType type  = PARAMETER;
	bool send   = false;
	bool hwSend = false;
	if      (!strcmp(name, volumePreFX))    {type = VOLUME_PREFX;         }
	else if (!strcmp(name, panPreFX))       {type = PAN_PREFX;            }
	else if (!strcmp(name, widthPreFX))     {type = WIDTH_PREFX;          }
	else if (!strcmp(name, volume))         {type = VOLUME;               }
	else if (!strcmp(name, pan))            {type = PAN;                  }
	else if (!strcmp(name, width))          {type = WIDTH;                }
	else if (!strcmp(name, mute))           {type = MUTE;                 }
	else if (!strcmp(name, sendVolume))     {type = VOLUME; send = true;  }
	else if (!strcmp(name, sendPan))        {type = PAN;    send = true;  }
	else if (!strcmp(name, sendMute))       {type = MUTE;   send = true;  }
	else if (!strcmp(name, sendHardVolume)) {type = VOLUME; hwSend = true;}
	else if (!strcmp(name, sendHardPan))    {type = PAN;    hwSend = true;}
	else if (!strcmp(name, sendHardMute))   {type = MUTE;   hwSend = true;}
	else if (!strcmp(name, takePitch))      {type = PITCH;                }
	else if (!strcmp(name, takeVolume))     {type = VOLUME;               }
	else if (!strcmp(name, takePan))        {type = PAN;                  }
	else if (!strcmp(name, takeMute))       {type = MUTE;                 }
	else if (!strcmp(name, playrate))       {type = PLAYRATE;             }
	else if (!strcmp(name, tempo))          {type = TEMPO;                }
	else                                    {type = PARAMETER;            }

	WritePtr(isSend,   send);
	WritePtr(isHwSend, hwSend);
	return type;
}

/******************************************************************************
* Tempo                                                                       *
******************************************************************************/
int FindPreviousTempoMarker (double position)
{
	int first = 0;
	int last = CountTempoTimeSigMarkers(NULL);

	while (first != last)
	{
		int mid = (first + last) / 2;
		double currentPos; GetTempoTimeSigMarker(NULL, mid, &currentPos, NULL, NULL, NULL, NULL, NULL, NULL);

		if (currentPos < position) first = mid + 1;
		else                       last  = mid;

	}
	return first - 1;
}

int FindNextTempoMarker (double position)
{
	int first = 0;
	int last = CountTempoTimeSigMarkers(NULL);
	int count = last;

	while (first != last)
	{
		int mid = (first + last) /2;
		double currentPos; GetTempoTimeSigMarker(NULL, mid, &currentPos, NULL, NULL, NULL, NULL, NULL, NULL);

		if (position >= currentPos) first = mid + 1;
		else                        last  = mid;

	}
	return (first < count) ? first: -1;
}

int FindClosestTempoMarker (double position)
{
	int prevId = FindPreviousTempoMarker(position);
	int nextId = prevId + 1;

	int count = CountTempoTimeSigMarkers(NULL);
	if (prevId == -1)
	{
		return (nextId < count) ? nextId : -1;
	}
	else
	{
		if (nextId < count)
		{

			double prevPos, nextPos;
			GetTempoTimeSigMarker(NULL, prevId, &prevPos, NULL, NULL, NULL, NULL, NULL, NULL);
			GetTempoTimeSigMarker(NULL, nextId, &nextPos, NULL, NULL, NULL, NULL, NULL, NULL);

			return (GetClosestVal(position, prevPos, nextPos) == prevPos) ? prevId : nextId;
		}
		else
		{
			return prevId;
		}
	}
}

int FindTempoMarker (double position, double surroundingRange /*= 0*/)
{
	int count = CountTempoTimeSigMarkers(NULL);
	if (count == 0)
		return -1;

	int prevId = FindPreviousTempoMarker(position);
	int nextId = prevId + 1;

	double prevPos, nextPos;
	GetTempoTimeSigMarker(NULL, prevId, &prevPos, NULL, NULL, NULL, NULL, NULL, NULL);
	GetTempoTimeSigMarker(NULL, nextId, &nextPos, NULL, NULL, NULL, NULL, NULL, NULL);

	double distanceFromPrev = (CheckBounds(prevId, 0, count-1)) ? (position - prevPos) : (abs(surroundingRange) + 1);
	double distanceFromNext = (CheckBounds(nextId, 0, count-1)) ? (nextPos - position) : (abs(surroundingRange) + 1);

	int id = -1;
	if (distanceFromPrev <= distanceFromNext)
	{
		if (distanceFromPrev <= surroundingRange)
			id = prevId;
	}
	else
	{
		if (distanceFromNext <= surroundingRange)
			id = nextId;
	}
	return (id < count) ? id : -1;
}

double TempoAtPosition (double position)
{
	double bpm;
	TimeMap_GetTimeSigAtTime(NULL, position, NULL, NULL, &bpm);
	return bpm;
}

double AverageProjTempo ()
{
	if (int count = CountTempoTimeSigMarkers(NULL))
	{
		double t, b; int lastPoint = count - 1;
		GetTempoTimeSigMarker(NULL, lastPoint, &t, NULL, NULL, &b, NULL, NULL, NULL);
		double len = EndOfProject(false, false);
		if (t > len)
			len = t;

		if (len != 0)
			return TimeMap_timeToQN(len) * 60 / len;
		else
			return b;
	}
	else
	{
		return TempoAtPosition(0);
	}
}

double GetProjectSettingsTempo (int* num, int* den)
{
	int _den;
	TimeMap_GetTimeSigAtTime(NULL, -1, num, &_den, NULL);
	double bpm;
	GetProjectTimeSignature2(NULL, &bpm, NULL);

	WritePtr(den, _den);
	return bpm/_den*4;
}

double CalculateTempoAtPosition (double startBpm, double endBpm, double startTime, double endTime, double targetTime)
{
	return startBpm + (endBpm-startBpm) / (endTime-startTime) * (targetTime-startTime);
}

double CalculateMeasureAtPosition (double startBpm, double endBpm, double timeLen, double targetTime)
{
	// Return number of measures counted from the musical position of startBpm
	return targetTime * (targetTime * (endBpm-startBpm) + 2*timeLen*startBpm) / (480*timeLen);
}

double CalculatePositionAtMeasure (double startBpm, double endBpm, double timeLen, double targetMeasure)
{
	// Return number of seconds counted from the time position of startBpm
	double a = startBpm - endBpm;
	double b = timeLen * startBpm;
	double c = 480 * timeLen * targetMeasure;
	return c / (b + sqrt(pow(b,2) - a*c));  // more stable solution for quadratic equation
}

void CalculateMiddlePoint (double* middleTime, double* middleBpm, double measure, double startTime, double endTime, double startBpm, double endBpm)
{
	double f = 480 * (measure/2);
	double a = startBpm - endBpm;
	double b = a*(startTime+endTime) / 2 + f;
	double c = a*(startTime*endTime) + f*(startTime+endTime);

	double pos = c / (b + sqrt(pow(b,2) - a*c));
	double bpm = f / (pos-startTime) - startBpm; // when editing tempo we usually move forward so previous point
	WritePtr(middleTime, pos);                   // remains the same and calculating the middle point in relation
	WritePtr(middleBpm, bpm);                    // to it will make middle point land on the correct musical position
}

void CalculateSplitMiddlePoints (double* time1, double* time2, double* bpm1, double* bpm2, double splitRatio, double measure, double startTime, double middleTime, double endTime, double startBpm, double middleBpm, double endBpm)
{
	// First point
	double temp = measure * (1-splitRatio) / 2;
	double pos1 = CalculatePositionAtMeasure(startBpm, middleBpm, (middleTime-startTime), temp) + startTime;
	double val1 = 480*temp / (pos1-startTime) - startBpm;
	WritePtr(time1, pos1);
	WritePtr(bpm1, val1);

	// Second point
	double f1 = 480 * measure*splitRatio;
	double f2 = 480 * temp;
	double a = val1-endBpm;
	double b = (a*(pos1+endTime) + f1+f2) / 2;
	double c = a*(pos1*endTime) + f1*endTime + f2*pos1;

	double pos2 = c / (b + sqrt(pow(b,2) - a*c));
	double val2 = f1 / (pos2-pos1) - val1;
	WritePtr(time2, pos2);
	WritePtr(bpm2, val2);
}

void InitTempoMap ()
{
	if (!CountTempoTimeSigMarkers(NULL))
		SetTempoTimeSigMarker(NULL, -1, 0, -1, -1, GetProjectSettingsTempo(NULL, NULL), 0, 0, false);
}

void RemoveTempoMap ()
{
	// Using native API DeleteTempoTimeSigMarker() or DeleteEnvelopePointRange() won't remove first point (but BR_Envelope can do it since it uses chunks for tempo map)
	BR_Envelope tempoMap(GetTempoEnv());
	tempoMap.DeleteAllPoints();
	tempoMap.SetVisible(false);
	tempoMap.Commit(true);
}

void UpdateTempoTimeline ()
{
	double t, b; int n, d; bool s;
	GetTempoTimeSigMarker(NULL, 0, &t, NULL, NULL, &b, &n, &d, &s);
	SetTempoTimeSigMarker(NULL, 0, t, -1, -1, b, n, d, s);
	UpdateTimeline();
}

void UnselectAllTempoMarkers ()
{
	TrackEnvelope* tempoEnv = GetTempoEnv();
	for (int i = 0; i < CountEnvelopePoints(tempoEnv); ++i)
		SetEnvelopePoint(tempoEnv, i, NULL, NULL, NULL, NULL, &g_bFalse, &g_bFalse);
}
