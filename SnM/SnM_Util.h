/******************************************************************************
/ SnM_Util.h
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

#ifndef _SNM_UTIL_H_
#define _SNM_UTIL_H_


const char* GetFileRelativePath(const char* _fn);
const char* GetFileExtension(const char* _fn, bool _wantdot = false);
bool HasFileExtension(const char* _fn, const char* _expectedExt);
void GetFilenameNoExt(const char* _fullFn, char* _fn, int _fnSz);
const char* GetFilenameWithExt(const char* _fullFn);
bool Filenamize(char* _fnInOut, bool _checkOnly = false);
bool IsValidFilenameErrMsg(const char* _fn, bool _errMsg);
bool FileOrDirExists(const char* _fn);
bool FileOrDirExistsErrMsg(const char* _fn, bool _errMsg = true);
bool SNM_DeleteFile(const char* _filename, bool _recycleBin);
bool SNM_DeletePeakFile(const char* _fn, bool _recycleBin);
bool SNM_CopyFile(const char* _destFn, const char* _srcFn);
void RevealFile(const char* _fn, bool _errMsg = true);
bool BrowseResourcePath(const char* _title, const char* _dir, const char* _fileFilters, char* _fn, int _fnSize, bool _wantFullPath = false);
const char* GetShortResourcePath(const char* _resSubDir, const char* _fullFn);
void GetFullResourcePath(const char* _resSubDir, const char* _shortFn, char* _fullFn, int _fullFnSize);
bool LoadChunk(const char* _fn, WDL_FastString* _chunkOut, bool _trim = true, int _approxMaxlen = 0);
bool SaveChunk(const char* _fn, WDL_FastString* _chunk, bool _indent);
WDL_HeapBuf* LoadBin(const char* _fn);
bool SaveBin(const char* _fn, const WDL_HeapBuf* _hb);
#ifdef _SNM_MISC
bool TranscodeFileToFile64(const char* _outFn, const char* _inFn);
#endif
WDL_HeapBuf* TranscodeStr64ToHeapBuf(const char* _str64);
bool GenerateFilename(const char* _dir, const char* _name, const char* _ext, char* _updatedFn, int _updatedSz);
void ScanFiles(WDL_PtrList<WDL_String>* _files, const char* _initDir, const char* _filterList, bool _subdirs);
void StringToExtensionConfig(WDL_FastString* _str, ProjectStateContext* _ctx);
void ExtensionConfigToString(WDL_FastString* _str, ProjectStateContext* _ctx);

void SaveIniSection(const char* _iniSectionName, WDL_FastString* _iniSection, const char* _iniFn);
void UpdatePrivateProfileSection(const char* _oldAppName, const char* _newAppName, const char* _iniFn, const char* _newIniFn = NULL);
void UpdatePrivateProfileString(const char* _appName, const char* _oldKey, const char* _newKey, const char* _iniFn, const char* _newIniFn = NULL);
void SNM_UpgradeIniFiles(int _iniVersion);

int snprintfStrict(char* _buf, size_t _n, const char* _fmt, ...);
bool GetStringWithRN(const char* _bufSrc, char* _buf, int _bufSize);
std::string GetStringWithRN(const std::string &);
const char* FindFirstRN(const char* _str, bool _anyOrder = false);
char* ShortenStringToFirstRN(char* _str, bool _anyOrder = false);
bool ReplaceWithChar(char* _strInOut, const char* _str, const char _replaceCh);
void SplitStrings(const char* _list, WDL_PtrList<WDL_FastString>* _outItems, const char* _sep = ",");

int SNM_SnapToMeasure(double _pos);
void TranslatePos(double _pos, int* _h, int* _m = NULL, int* _s = NULL, int* _ms = NULL);
double SeekPlay(double _pos, bool _moveView = false);

int SNM_NamedCommandLookup(const char* _custId, KbdSectionInfo* _section = NULL, bool _hardCheck = false);
const char* SNM_GetTextFromCmd(int _cmdId, KbdSectionInfo* _section);
bool LoadKbIni(WDL_PtrList<WDL_FastString>* _out);
int GetMacroOrScript(const char* _customId, int _sectionUniqueId, WDL_PtrList<WDL_FastString>* _inMacroScripts, WDL_PtrList<WDL_FastString>* _outCmds, WDL_FastString* _outName = NULL);
enum class ActionType { Unknown, Custom, ReaScript };
ActionType GetActionType(const char* _cmd, bool _cmdIsName = true);
bool IsMacroOrScript(const char* _cmd, bool _cmdIsName = true);
int CheckSwsMacroScriptNumCustomId(const char* _custId, int _secIdx = 0);

int SNM_GetActionSectionUniqueId(int _idx);
int SNM_GetActionSectionIndex(int _uniqueId);
const char* SNM_GetActionSectionName(int _idx);
KbdSectionInfo* SNM_GetActionSection(int _idx);

bool LearnAction(KbdSectionInfo* _section, int _cmdId);
bool GetSectionURL(bool _alr, KbdSectionInfo* _section, char* _sectionURL, int _sectionURLSize);

// Get/SetMediaItemTakeInfo_Value(*,"D_VOL") uses negative value (sign flip) if take polarity is flipped
bool IsTakePolarityFlipped(MediaItem_Take* take);

#ifdef _SNM_MISC
WDL_UINT64 FNV64(WDL_UINT64 h, const unsigned char* data, int sz);
bool FNV64(const char* _strIn, char* _strOut);
#endif


#endif
