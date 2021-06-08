/******************************************************************************
/ SnM_Project.h
/
/ Copyright (c) 2012 and later Jeffos
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

//#pragma once

#ifndef _SNM_PROJECT_H_
#define _SNM_PROJECT_H_

#include "SnM.h"


bool IsActiveProjectInLoadSave(char* _projfn = NULL, int _projfnSz = 0, bool _ensureRPP = false);
void TieFileToProject(const char* _fn, ReaProject* _prj = NULL, bool _tie = true);
void UntieFileFromProject(const char* _fn, ReaProject* _prj = NULL);
double SNM_GetProjectLength(int _flags=0xFFFF);
bool InsertSilence(const char* _undoTitle, double _pos, double _len);

void LoadOrSelectProject(const char* _fn, bool _newTab);

class SelectProjectJob : public MidiOscActionJob
{
public:
	SelectProjectJob(int _approxMs, int _val, int _valhw, int _relmode) 
		: MidiOscActionJob(SNM_SCHEDJOB_SEL_PRJ,_approxMs,_val,_valhw,_relmode) {}
protected:
	void Perform();
	double GetCurrentValue();
	double GetMinValue() { return 0.0; }
	double GetMaxValue();
};

void SelectProject(COMMAND_T* _ct, int _val, int _valhw, int _relmode, HWND _hwnd);

void GlobalStartupActionTimer();
void SetStartupAction(COMMAND_T*);
void ClearStartupAction(COMMAND_T*);
void ShowStartupActions(COMMAND_T*);
WDL_FastString* GetGlobalStartupAction(); // for ReaScript export
SWSProjConfig<WDL_FastString>* GetProjectLoadAction(); // for ReaScript export
int SNM_ProjectInit();
void SNM_ProjectExit();

void InsertSilence(COMMAND_T*);
void OpenProjectPathInExplorerFinder(COMMAND_T*);

#endif

