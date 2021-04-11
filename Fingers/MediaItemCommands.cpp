#include "stdafx.h"

#include "MediaItemCommands.h"
#include "CommandHandler.h"
#include "TimeMap.h"

#include "RprItem.h"
#include "RprTake.h"
#include "RprTrack.h"
#include "RprMidiTake.h"
#include "../SnM/SnM_Misc.h" // #865, for SNM_GetIntConfigVar()

#include <WDL/localize/localize.h>

void CmdCleanItemLengths(int flag, void *data);
void CmdLegatoItemLengths(int flag, void *data);
void CmdMoveItemsToEditCursor(int flag, void *data);
void CmdDeselectIfNotStartInTimeSelection(int flag, void *data);
static void CreateMidiItem();

//!WANT_LOCALIZE_1ST_STRING_BEGIN:sws_actions
void MediaItemCommands::Init()
{
    RprCommand::registerCommand("SWS/FNG: Expand selected media items", "FNG_EXPAND", new CmdExpandItems(0.005), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Contract selected media items", "FNG_CONTRACT", new CmdExpandItems(-0.005), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Expand selected media items (fine)", "FNG_EXPAND_F", new CmdExpandItems(0.0001), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Contract selected media items (fine)", "FNG_CONTRACT_F", new CmdExpandItems(-0.0001), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Expand/contract selected media items to bar", "FNG_EXPAND_BAR1", new CmdExpandItemsToBar(1), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Expand selected media items by 2", "FNG_EXPAND_BY2", new CmdExpandItems(1.0), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Contract selected media items by 1/2", "FNG_CONTRACT_BY_HALF", new CmdExpandItems(-0.5), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Clean selected overlapping media items on same track", "FNG_CLEAN_OVERLAP", &CmdCleanItemLengths,UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Legato selected media items on same track", "FNG_LEGATO_LENGTH", &CmdLegatoItemLengths, 0, UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Legato selected media items on same track (change rate)", "FNG_LEGATO_RATE", &CmdLegatoItemLengths, 1, UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Rotate selected media items positions", "FNG_ROTATE_POS", new CmdRotateItems(false, false), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Rotate selected media items positions and lengths", "FNG_ROTATE_POSLEN", new CmdRotateItems(true, false), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Rotate selected media items positions (reverse)", "FNG_ROTATE_POS_REV", new CmdRotateItems(false, true), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Rotate selected media items positions and lengths (reverse)", "FNG_ROTATE_POSLEN_REV", new CmdRotateItems(true, true),UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Insert MIDI item with note C4 of size 32nd", "FNG_MIDI_BASIC", new CmdInsertMidiNote(), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Transpose selected MIDI items up a semitone", "FNG_MIDI_UP_SEMI", new CmdPitchUpMidi(1), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Transpose selected MIDI items down a semitone", "FNG_MIDI_DN_SEMI", new CmdPitchUpMidi(-1), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Transpose selected MIDI items up an octave", "FNG_MIDI_UP_OCT", new CmdPitchUpMidi(12), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Transpose selected MIDI items down an octave", "FNG_MIDI_DN_OCT", new CmdPitchUpMidi(-12), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Set selected MIDI items name to first note", "FNG_MIDI_NAME", new CmdSetItemNameMidi(true), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Increase selected MIDI items velocity by 1", "FNG_MIDI_UP_VEL1", new CmdVelChangeMidi(1), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Decrease selected MIDI items velocity by 1", "FNG_MIDI_UP_VELM1", new CmdVelChangeMidi(-1), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Increase selected MIDI items velocity by 10", "FNG_MIDI_UP_VEL10", new CmdVelChangeMidi(10), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Decrease selected MIDI items velocity by 10", "FNG_MIDI_UP_VELM10", new CmdVelChangeMidi(-10), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Time stretch selected items by 2", "FNG_RATE_1_2", new CmdIncreaseItemRate(1.0 / 2.0), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Time compress selected items by 1/2", "FNG_RATE_2", new CmdIncreaseItemRate(2.0), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Time stretch selected items (fine)", "FNG_RATE_1_101", new CmdIncreaseItemRate(1.0/ 1.01), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Time compress selected items (fine)", "FNG_RATE_101", new CmdIncreaseItemRate(1.01), UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Move selected items to edit cursor", "FNG_MOVE_TO_EDIT", &CmdMoveItemsToEditCursor, UNDO_STATE_ITEMS);
    RprCommand::registerCommand("SWS/FNG: Unselect items that do not start in time selection", "FNG_TIME_SEL_NOT_START", &CmdDeselectIfNotStartInTimeSelection,UNDO_STATE_ITEMS);
}
//!WANT_LOCALIZE_1ST_STRING_END

void CmdRotateItems::doCommand(int flag)
{
    RprItemCtrPtr ctr = RprItemCollec::getSelected();

    if(ctr->size() <= 1)
        return;

    std::list<double> itemLengths;
    std::list<double> itemPositions;
    std::list<RprTrack> itemTracks;

    for(int i = 0; i < ctr->size(); i++) {
        itemLengths.push_back(ctr->getAt(i).getLength());
        itemPositions.push_back(ctr->getAt(i).getPosition());
        itemTracks.push_back(ctr->getAt(i).getTrack());
    }
    if(!m_bReverse) {
        double lastLen = itemLengths.back();
        itemLengths.pop_back();
        itemLengths.push_front(lastLen);

        double lastPos = itemPositions.back();
        itemPositions.pop_back();
        itemPositions.push_front(lastPos);

        RprTrack track = itemTracks.back();
        itemTracks.pop_back();
        itemTracks.push_front(track);
    } else {
        double lastLen = itemLengths.front();
        itemLengths.pop_front();
        itemLengths.push_back(lastLen);

        double lastPos = itemPositions.front();
        itemPositions.pop_front();
        itemPositions.push_back(lastPos);

        RprTrack track = itemTracks.front();
        itemTracks.pop_front();
        itemTracks.push_back(track);
    }

    for(int i = 0; i < ctr->size(); i++) {
        ctr->getAt(i).setPosition(itemPositions.front());
        itemPositions.pop_front();

        ctr->getAt(i).setTrack(itemTracks.front());
        itemTracks.pop_front();
        if(m_bRotateLengths) {
            ctr->getAt(i).setLength(itemLengths.front());
            itemLengths.pop_front();
        }
    }
}

void CmdExpandItems::doCommand(int flag)
{
    RprItemCtrPtr ctr = RprItemCollec::getSelected();

    if(ctr->size() <= 1)
        return;

    double startPos = ctr->first().getPosition();

    for(int i = 0; i < ctr->size(); i++) {
        RprItem item = ctr->getAt(i);
        item.setPosition(item.getPosition() + (item.getPosition() - startPos) * m_dAmount);
    }
}

void CmdExpandItemsToBar::doCommand(int flag)
{
    RprItemCtrPtr ctr = RprItemCollec::getSelected();
    if(ctr->size() <= 1)
        return;
    if(m_nBars == 0)
        return;

    double startPos = ctr->first().getPosition();
    double expandPoint = BeatToTime(TimeToBeat(startPos) + BeatsInMeasure(TimeToMeasure(startPos)) * m_nBars);

    for(int i = 0; i < 5; i++)
    {
        RprItem last = ctr->last();
        double lastPos = last.getPosition();
        double finalPoint = lastPos + last.getLength();
        if( abs(finalPoint - expandPoint) < 0.0001)
            break;

        double dFactor = lastPos * (expandPoint / finalPoint - 1) / (lastPos - startPos);

        for(int i = 0; i < ctr->size(); i++) {
            RprItem item = ctr->getAt(i);
            double pos = item.getPosition();
            pos = pos + dFactor * ( pos - startPos);
            item.setPosition(pos);
        }
    }
}

void CmdCleanItemLengths(int flag, void *data)
{
    RprItemCtrPtr ctr = RprItemCollec::getSelected();
    if(ctr->size() == 0)
        return;

    ctr->sort();

    for(int i = 0; i < ctr->size() - 1; i++)
    {
        RprTrack track1 = ctr->getAt(i).getTrack();
        double len = ctr->getAt(i).getLength();
        double lhsPosition = ctr->getAt(i).getPosition();
        for(int j = i + 1; j < ctr->size(); j++) {
            RprTrack track2 = ctr->getAt(j).getTrack();
            if(track1 == track2) {
                double rhsPosition = ctr->getAt(j).getPosition();
                if(lhsPosition + len > rhsPosition) {
                    len = rhsPosition - lhsPosition;
                }
                break;
            }
        }
        ctr->getAt(i).setLength(len);
    }
}

void CmdLegatoItemLengths(int flag, void *data)
{
    bool doRate = *(int*)data > 0;

    RprItemCtrPtr ctr = RprItemCollec::getSelected();
    if(ctr->size() == 0)
        return;

    ctr->sort();

    for(int i = 0; i < ctr->size() - 1; i++)
    {
        RprTrack track1 = ctr->getAt(i).getTrack();
        double len = ctr->getAt(i).getLength();
        double lhsPosition = ctr->getAt(i).getPosition();
        for(int j = i + 1; j < ctr->size(); j++) {
            RprTrack track2 = ctr->getAt(j).getTrack();
            if(track1 == track2) {
                double rhsPosition = ctr->getAt(j).getPosition();
                if(lhsPosition + len < rhsPosition) {
                    len = rhsPosition - lhsPosition;
                }
                break;
            }
        }
        if(!doRate || ctr->getAt(i).takeCount() == 0) { /* In case of an empty item always change length, not rate */
            ctr->getAt(i).setLength(len);
        } else {
            RprTake take = ctr->getAt(i).getActiveTake();
            double playRate = take.getPlayRate();
            playRate *= ctr->getAt(i).getLength() / len;
            take.setPlayRate(playRate);
            ctr->getAt(i).setLength(len);
        }
    }
}

static
void CreateMidiItem()
{
    Main_OnCommandEx(40214, 0, 0);
    RprItemCtrPtr ctr = RprItemCollec::getSelected();

    if(ctr->size() != 1)
        return;

    RprItem item = ctr->getAt(0);
    RprMidiTake midiTake(item.getActiveTake());
    RprMidiNote *note = midiTake.addNoteAt(midiTake.countNotes());

    note->setPosition(item.getPosition());
    note->setChannel(1);
    note->setVelocity(127);
    note->setPitch(72);
    note->setLength(item.getLength());
}

void CmdInsertMidiNote::doCommand(int flag)
{
    double start, end;
    GetSet_LoopTimeRange(false, true, &start, &end, false);
    if(CountSelectedTracks(0) == 0)
        return;
    double pos = GetCursorPosition();
    double qn = 60.0 / TimeMap2_GetDividedBpmAtTime(0, pos);
    double newend = pos + qn / 8;
    GetSet_LoopTimeRange(true, true, &pos, &newend, false);
    CreateMidiItem();
    GetSet_LoopTimeRange(true, true, &start, &end, false);
}

static bool convertToInProjectMidi(RprItemCtrPtr &ctr)
{
    bool hasMidiFile = false;
    for(int i = 0; i < ctr->size(); i++) {
        RprTake take(ctr->getAt(i).getActiveTake());
        if(!take.isMIDI())
            continue;
        if(take.isFile()) {
            hasMidiFile = true;
            break;
        }
    }
    if(hasMidiFile) {
        if(MessageBox(GetMainHwnd(),
            __LOCALIZE("Current selection has takes with MIDI files.\r\nTo apply this action these takes must be converted to in-project takes.\r\nDo you want to continue?","sws_mbox"),
            __LOCALIZE("FNG - Warning","sws_mbox"), MB_YESNO) == IDNO) {
            return false;
        }
        Main_OnCommandEx(40684, 0 , 0);
    }
    return true;
}

void CmdPitchUpMidi::doCommand(int flag)
{
    std::set<std::string> pooledGuids;
    RprItemCtrPtr ctr = RprItemCollec::getSelected();

    if(!convertToInProjectMidi(ctr))
        return;

    for(int i = 0; i < ctr->size(); i++) {
        if (!ctr->getAt(i).getActiveTake().isMIDI())
            continue;
        RprMidiTake midiItem(ctr->getAt(i).getActiveTake());
        if(!pooledGuids.insert(midiItem.poolGuid()).second)
            continue;
        for(int j = 0; j < midiItem.countNotes(); j++) {
            RprMidiNote *note = midiItem.getNoteAt(j);
            note->setPitch(note->getPitch() + m_pitchAmt);
        }
    }
}

void CmdSetItemNameMidi::doCommand(int flag)
{
    RprItemCtrPtr ctr = RprItemCollec::getSelected();

    if (!convertToInProjectMidi(ctr))
    {
        return;
    }

	int octOffset = SNM_GetIntConfigVar("midioctoffs", -666); // #865
	
    for (int i = 0; i < ctr->size(); i++)
    {
        if (!ctr->getAt(i).getActiveTake().isMIDI())
        {
            continue;
        }

        RprMidiTake midiItem(ctr->getAt(i).getActiveTake(), true);
        if (midiItem.countNotes() > 0)
        {
            int pitch = midiItem.getNoteAt(0)->getPitch(), pitchOffsetted = pitch;
            // in reaper.ini: midioctoffs = 0 => offset set in REAPER prefs = -1
            if (octOffset != -666 && octOffset != 1) 
                pitchOffsetted = pitchOffsetted + (12 * (octOffset - 1));

            static const char* noteNames[] = {"C", "C#", "D", "D#",
                "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            int nameIndex = pitch % 12;

            char octave[12];
            snprintf(octave, sizeof(octave), "%d", pitchOffsetted / 12 - 1);

            char noteName[2 + sizeof(octave)];
            strcpy(noteName, noteNames[nameIndex]);
            strcat(noteName, octave);

            ctr->getAt(i).getActiveTake().setName(noteName);
        }
    }
}

void CmdIncreaseItemRate::doCommand(int flag)
{
    RprItemCtrPtr ctr = RprItemCollec::getSelected();
    for(int i = 0; i < ctr->size(); i++) {
    	if (ctr->getAt(i).takeCount() > 0) {
    		double playRate = ctr->getAt(i).getActiveTake().getPlayRate();
    		playRate *= m_amt;
    		if(playRate <= 0.1)
    			continue;
    		ctr->getAt(i).setLength(ctr->getAt(i).getLength() / m_amt);
    		ctr->getAt(i).getActiveTake().setPlayRate(playRate);
    	}
    	else /* In case of an empty item always change length, not rate */
    		ctr->getAt(i).setLength(ctr->getAt(i).getLength()/m_amt);
    }
}

void CmdVelChangeMidi::doCommand(int flag)
{
    std::set<std::string> pooledGuids;
    RprItemCtrPtr ctr = RprItemCollec::getSelected();

    if(!convertToInProjectMidi(ctr))
        return;

    for(int i = 0; i < ctr->size(); i++) {
        if (!ctr->getAt(i).getActiveTake().isMIDI())
            continue;
        RprMidiTake midiItem(ctr->getAt(i).getActiveTake());
        if(!pooledGuids.insert(midiItem.poolGuid()).second)
            continue;
        for(int j = 0; j < midiItem.countNotes(); j++) {
            RprMidiNote *note = midiItem.getNoteAt(j);
            note->setVelocity(note->getVelocity() + m_velAmt);
        }
    }
}

void CmdMoveItemsToEditCursor(int flag, void *data)
{
    RprItemCtrPtr ctr = RprItemCollec::getSelected();
    if(ctr->size() == 0)
        return;
    ctr->sort();
    double diff = GetCursorPosition() - ctr->getAt(0).getPosition();

    for(int i = 0; i < ctr->size(); i++)
        ctr->getAt(i).setPosition(ctr->getAt(i).getPosition() + diff);
}

void CmdDeselectIfNotStartInTimeSelection(int flag, void *data)
{
    RprItemCtrPtr ctr = RprItemCollec::getSelected();
    if(ctr->size() == 0)
        return;
    ctr->sort();

    double startLoop, endLoop;
    GetSet_LoopTimeRange(false, false, &startLoop, &endLoop, false);

    PreventUIRefresh(1);
    for(int i = 0; i < ctr->size(); i++) {
        if( ctr->getAt(i).getPosition() < startLoop || ctr->getAt(i).getPosition() > endLoop)
            ctr->getAt(i).setSelected(false);
    }
    PreventUIRefresh(-1);
    UpdateArrange();
}
