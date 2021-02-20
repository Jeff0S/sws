/******************************************************************************
/ SnM_Util.cpp
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

#include "stdafx.h"

#include "SnM.h"
#include "SnM_Chunk.h"
#include "SnM_Util.h"
#include "../cfillion/cfillion.hpp" // CF_LocateInExplorer

#include <WDL/localize/localize.h>
#include <WDL/sha.h>
#include <WDL/projectcontext.h>

///////////////////////////////////////////////////////////////////////////////
// File util
///////////////////////////////////////////////////////////////////////////////

const char* GetFileRelativePath(const char* _fn)
{
	return WDL_get_filepart(_fn);
}

const char* GetFileExtension(const char* _fn, bool _wantdot)
{
	const char* p = WDL_get_fileext(_fn);
	if (*p=='.' && !_wantdot) return p+1;
	return p;
}

bool HasFileExtension(const char* _fn, const char* _expectedExt) {
	return _stricmp(GetFileExtension(_fn), _expectedExt) == 0;
}

void GetFilenameNoExt(const char* _fullFn, char* _fn, int _fnSz)
{
	lstrcpyn(_fn, GetFileRelativePath(_fullFn), _fnSz);
	char* p =  (char*)WDL_get_fileext(_fn);
	if (*p=='.') *p = '\0';
}

const char* GetFilenameWithExt(const char* _fullFn)
{
	return GetFileRelativePath(_fullFn);
}

bool IsFullFileName(const char *p)
{
#ifdef _WIN32
	return (p[0] && p[1] == ':' && (p[2] == '\\' || p[2] == '/')) || (p[0] == '\\' && p[1] == '\\');
#else
	return p[0]=='/';
#endif
}

// check the most restrictive OS forbidden chars (so that filenames are cross-platform)
bool Filenamize(char* _fnInOut, bool _checkOnly)
{
	if (_fnInOut && *_fnInOut)
	{
		int i=-1, j=0;
		while (_fnInOut[++i])
		{
			if (_fnInOut[i] != ':' && _fnInOut[i] != '/' && _fnInOut[i] != '\\' &&
				_fnInOut[i] != '*' && _fnInOut[i] != '?' && _fnInOut[i] != '\"' &&
				_fnInOut[i] != '>' && _fnInOut[i] != '<' && _fnInOut[i] != '\'' && 
				_fnInOut[i] != '|' && _fnInOut[i] != '^') // ^ is forbidden on FAT
			{
				if (!_checkOnly)
					_fnInOut[j] = _fnInOut[i];
				j++;
			}
			else if (_checkOnly)
				return false;
		}
		if (!_checkOnly)
			_fnInOut[j] = '\0';
		return i==j;
	}
	return false;
}

bool IsValidFilenameErrMsg(const char* _fn, bool _errMsg)
{
	bool ko = !Filenamize((char*)_fn, true);
	if (ko && _errMsg)
	{
		char buf[SNM_MAX_PATH];
		if (_fn && *_fn)
			snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("Invalid filename: %s\nFilenames cannot contain any of the following characters: / \\ * ? \" < > ' | ^ :","sws_mbox"), _fn);
		else
			lstrcpyn(buf, __LOCALIZE("Empty filename!","sws_mbox"), sizeof(buf));
		MessageBox(GetMainHwnd(), buf, __LOCALIZE("S&M - Error","sws_mbox"), MB_OK);
	}
	return !ko;
}

// the API function file_exists() is different, it returns false for folders
bool FileOrDirExists(const char* _fn)
{
	if (_fn && *_fn && *_fn!='.')
	{
		WDL_FastString fn(_fn);
		fn.remove_trailing_dirchars(); // bug fix for directories

		struct stat s;
#ifdef _WIN32
		return (statUTF8(fn.Get(), &s) == 0);
#else
		return (stat(fn.Get(), &s) == 0);
#endif
	}
	return false;
}

// FileOrDirExists() and FileOrDirExistsErrMsg() are intentionally not merged
// (would impact other project members' code...)
bool FileOrDirExistsErrMsg(const char* _fn, bool _errMsg)
{
	bool exists = FileOrDirExists(_fn);
	if (!exists && _errMsg)
	{
		char buf[SNM_MAX_PATH];
		if (_fn && *_fn)
			snprintf(buf, sizeof(buf), __LOCALIZE_VERFMT("File or directory not found:\n%s","sws_mbox"), _fn);
		else
			lstrcpyn(buf, __LOCALIZE("Empty filename!","sws_mbox"), sizeof(buf));
		MessageBox(GetMainHwnd(), buf, __LOCALIZE("S&M - Error","sws_mbox"), MB_OK);
	}
	return exists;
}

bool SNM_DeleteFile(const char* _filename, bool _recycleBin)
{
	bool ok = false;
	if (_filename && *_filename) 
	{
#ifdef _WIN32
		if (_recycleBin)
			return (SendFileToRecycleBin(_filename) == 0);
#endif
		ok = !!DeleteFile(_filename);
	}
	return ok;
}

bool SNM_DeletePeakFile(const char* _fn, bool _recycleBin)
{
	char peakFn[SNM_MAX_PATH] = "";
	GetPeakFileName(_fn, peakFn, sizeof(peakFn));
	if (*peakFn != '\0')
		return SNM_DeleteFile(peakFn, _recycleBin);

	return false;	
}

bool SNM_MovePeakFile(const char* oldMediaFn, const char* newMediaFn)
{
	char oldPeakFn[SNM_MAX_PATH]{}, newPeakFn[SNM_MAX_PATH]{};
	GetPeakFileName(oldMediaFn, oldPeakFn, sizeof(oldPeakFn));
	GetPeakFileNameEx(newMediaFn, newPeakFn, sizeof(newPeakFn), true);

	if (const char dirSep{ WDL_remove_filepart(newPeakFn) }) {
		// create the last directory (eg. the first two characters of the hashed filename)
		// assumes all parent directories already exists
		CreateDirectory(newPeakFn, nullptr);
		newPeakFn[strlen(newPeakFn)] = dirSep;
	}

	if (!MoveFile(oldPeakFn, newPeakFn)) {
		if (!SNM_CopyFile(newPeakFn, oldPeakFn))
			return false;

		SNM_DeleteFile(oldPeakFn, false);
	}

	return true;
}

bool SNM_CopyFile(const char* _destFn, const char* _srcFn)
{
	bool ok = false;
	if (_destFn && _srcFn) {
		if (WDL_HeapBuf* hb = LoadBin(_srcFn)) {
			ok = SaveBin(_destFn, hb);
			DELETE_NULL(hb);
		}
	}
	return ok;
}

// for files only - on Win & OSX, directories can be browsed with:
// ShellExecute(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
void RevealFile(const char* _fn, bool _errMsg)
{
	if (FileOrDirExistsErrMsg(_fn, _errMsg))
		CF_LocateInExplorer(_fn);
}

// browse + return short resource filename (if possible and if _wantFullPath == false)
// returns false if cancelled
bool BrowseResourcePath(const char* _title, const char* _resSubDir, const char* _fileFilters, char* _fn, int _fnSize, bool _wantFullPath)
{
	bool ok = false;
	char defaultPath[SNM_MAX_PATH];
	snprintf(defaultPath, sizeof(defaultPath), "%s%c%s", GetResourcePath(), PATH_SLASH_CHAR, _resSubDir);
	if (char* fn = BrowseForFiles(_title, defaultPath, NULL, false, _fileFilters)) 
	{
		lstrcpyn(_fn, _wantFullPath ? fn : GetShortResourcePath(_resSubDir, fn), _fnSize);
		free(fn);
		ok = true;
	}
	return ok;
}

// get a short filename from a full resource path
// ex: C:\Documents and Settings\<user>\Application Data\REAPER\FXChains\EQ\JS\test.RfxChain -> EQ\JS\test.RfxChain
// notes: 
// - *must* work with non existing files (just some string processing here)
// - *must* be no-op for non resource paths (c:\temp\test.RfxChain -> c:\temp\test.RfxChain)
// - *must* be no-op for short resource paths 
const char* GetShortResourcePath(const char* _resSubDir, const char* _fullFn)
{
	if (_resSubDir && *_resSubDir && _fullFn && *_fullFn)
	{
		static int res_path_len=strlen(GetResourcePath());
		if (!_strnicmp(_fullFn, GetResourcePath(), res_path_len) && WDL_IS_DIRCHAR(_fullFn[res_path_len]))
		{
			const int subdir_sz = strlen(_resSubDir);
			if (!_strnicmp(_fullFn+res_path_len+1, _resSubDir, subdir_sz) && WDL_IS_DIRCHAR(_fullFn[res_path_len+1+subdir_sz]))
			{
				return _fullFn+res_path_len+1+subdir_sz+1;
			}
		}
	}
	return _fullFn;
}

// get a full resource path from a short filename
// ex: EQ\JS\test.RfxChain -> C:\Documents and Settings\<user>\Application Data\REAPER\FXChains\EQ\JS\test.RfxChain
// notes: 
// - work with non existing files
// - no-op for non resource paths (c:\temp\test.RfxChain -> c:\temp\test.RfxChain)
// - no-op for full resource paths 
void GetFullResourcePath(const char* _resSubDir, const char* _shortFn, char* _fullFn, int _fullFnSize)
{
	if (_fullFn && _fullFnSize>0) *_fullFn = '\0';
	else return;

	if (_shortFn && *_shortFn) 
	{
		if (IsFullFileName(_shortFn))
		{
			lstrcpyn(_fullFn, _shortFn, _fullFnSize);
		}
		else
		{
			snprintf(_fullFn, _fullFnSize, "%s%c%s%c%s", GetResourcePath(), PATH_SLASH_CHAR, _resSubDir, PATH_SLASH_CHAR, _shortFn);
		}
	}
}

// _trim: if true, remove empty lines + left trim
// _approxMaxlen: if >0, truncate arround _approxMaxlen bytes
// note: WDL's ProjectCreateFileRead() seems very slow..
bool LoadChunk(const char* _fn, WDL_FastString* _chunkOut, bool _trim, int _approxMaxlen)
{
	if (_chunkOut && _fn && *_fn)
	{
		_chunkOut->Set("");
		if (FILE* f = fopenUTF8(_fn, "r"))
		{
			char line[SNM_MAX_CHUNK_LINE_LENGTH]="";
			while(fgets(line, sizeof(line), f) && *line)
			{
				if (_trim) //JFB!! useless with SNM_ChunkParserPatcher v2, TBC: v2 supports "\r\n"?
				{
					char* p = line;
					while(*p && (*p == ' ' || *p == '\t')) p++;
					if (*p && *p!='\n' && *p!='\r')
					{
						// "\r\n" -> "\n"
						if (const char* eol = FindFirstRN(p)) {
							_chunkOut->Append(p, (int)(eol-p));
							_chunkOut->Append("\n");
						}
					}
				}
				else
					_chunkOut->Append(line);

				if (_approxMaxlen && _chunkOut->GetLength() > _approxMaxlen)
					break;
			}
			fclose(f);
			return true;
		}
	}
	return false;
}

// note: WDL's ProjectCreateFileWrite() seems very slow..
bool SaveChunk(const char* _fn, WDL_FastString* _chunk, bool _indent)
{
	if (_fn && *_fn && _chunk)
	{
		SNM_ChunkIndenter p(_chunk, false); // no auto-commit, see trick below
		if (_indent) p.Indent();
		if (FILE* f = fopenUTF8(_fn, "w"))
		{
			fputs(p.GetUpdates() ? p.GetChunk()->Get() : _chunk->Get(), f); // avoids p.Commit(), faster
			fclose(f);
			return true;
		}
	}
	return false;
}

// returns NULL if failed, otherwise it's up to the caller to free the returned buffer
WDL_HeapBuf* LoadBin(const char* _fn)
{
	WDL_HeapBuf* hb = NULL;
	if (FILE* f = fopenUTF8(_fn , "rb"))
	{
		long lSize;
		fseek(f, 0, SEEK_END);
		lSize = ftell(f);
		rewind(f);

		hb = new WDL_HeapBuf();
		void* p = hb->Resize(lSize,false);
		if (p && hb->GetSize() == lSize) {
			size_t result = fread(p,1,lSize,f);
			if (result != lSize)
				DELETE_NULL(hb);
		}
		else
			DELETE_NULL(hb);
		fclose(f);
	}
	return hb;
}

bool SaveBin(const char* _fn, const WDL_HeapBuf* _hb)
{
	bool ok = false;
	if (_hb && _hb->GetSize())
		if (FILE* f = fopenUTF8(_fn , "wb")) {
			ok = (fwrite(_hb->Get(), 1, _hb->GetSize(), f) == _hb->GetSize());
			fclose(f);
		}
	return ok;
}

#ifdef _SNM_MISC
bool TranscodeFileToFile64(const char* _outFn, const char* _inFn)
{
	bool ok = false;
	WDL_HeapBuf* hb = LoadBin(_inFn); 
	if (hb && hb->GetSize())
	{
		ProjectStateContext* ctx = ProjectCreateFileWrite(_outFn);
		cfg_encode_binary(ctx, hb->Get(), hb->GetSize());
		delete ctx;
		ok = FileOrDirExists(_outFn);
	}
	delete hb;
	return ok;
}
#endif

// returns NULL if failed, otherwise it's up to the caller to free the returned buffer
WDL_HeapBuf* TranscodeStr64ToHeapBuf(const char* _str64)
{
	WDL_HeapBuf* hb = NULL;
	long len = strlen(_str64);
	WDL_HeapBuf hbTmp;
	void* p = hbTmp.Resize(len, false);
	if (p && hbTmp.GetSize() == len)
	{
		memcpy(p, _str64, len);
		ProjectStateContext* ctx = ProjectCreateMemCtx(&hbTmp);
		hb = new WDL_HeapBuf();
		cfg_decode_binary(ctx, hb);
		delete ctx;
	}
	return hb;
}

// use Filenamize() first!
bool GenerateFilename(const char* _dir, const char* _name, const char* _ext, char* _outFn, int _outSz)
{
	if (_dir && _name && _ext && _outFn && *_dir)
	{
		char fn[SNM_MAX_PATH] = "";
		bool slash = _dir[strlen(_dir)-1] == PATH_SLASH_CHAR;
		if (slash) {
			if (snprintfStrict(fn, sizeof(fn), "%s%s.%s", _dir, _name, _ext) <= 0)
				return false;
		} else {
			if (snprintfStrict(fn, sizeof(fn), "%s%c%s.%s", _dir, PATH_SLASH_CHAR, _name, _ext) <= 0)
				return false;
		}

		int i=0;
		while(FileOrDirExists(fn))
		{
			if (slash) {
				if (snprintfStrict(fn, sizeof(fn), "%s%s_%03d.%s", _dir, _name, ++i, _ext) <= 0)
					return false;
			} else {
				if (snprintfStrict(fn, sizeof(fn), "%s%c%s_%03d.%s", _dir, PATH_SLASH_CHAR, _name, ++i, _ext) <= 0)
					return false;
			}
		}
		lstrcpyn(_outFn, fn, _outSz);
		return true;
	}
	return false;
}

// fills a list of filenames matching extensions defined in _filterList
// _filterList: file extensions without null separators, ex: "*.ext1 *.ext2" ("*" == all files)
// note: it is up to the caller to free _files (use WDL_PtrList_DeleteOnDestroy)
void ScanFiles(WDL_PtrList<WDL_String>* _files, const char* _initDir, const char* _filterList, bool _subdirs)
{
	WDL_DirScan ds;
	if (_files && _initDir && !ds.First(_initDir))
	{
		const char* curFn;
		const char* curfnExt;
		WDL_FastString fn, ext;
		do 
		{
			curFn = ds.GetCurrentFN();
			if (!strcmp(curFn, ".") || !strcmp(curFn, "..")) 
				continue;

			if (ds.GetCurrentIsDirectory())
			{
				if (_subdirs) {
					ds.GetCurrentFullFN(&fn);
					ScanFiles(_files, fn.Get(), _filterList, true);
				}
			}
			else
			{
				if (!strcmp("*", _filterList)) // || !strcmp("*.*", _filterList))
				{
					ds.GetCurrentFullFN(&fn);
					_files->Add(new WDL_String(fn.Get()));
				}
				else
				{
					curfnExt = GetFileExtension(curFn);
					if (*curfnExt)
					{
						ext.SetFormatted(64, "*.%s", curfnExt);
						if (stristr(_filterList, ext.Get())) {
							ds.GetCurrentFullFN(&fn);
							_files->Add(new WDL_String(fn.Get()));
						}
					}
				}
			}
		}
		while(!ds.Next());
	}
}

void StringToExtensionConfig(WDL_FastString* _str, ProjectStateContext* _ctx)
{
	if (_str && _ctx)
	{
		const char* pEOL = _str->Get()-1;
		char curLine[SNM_MAX_CHUNK_LINE_LENGTH] = "";
		for(;;) 
		{
			const char* pLine = pEOL+1;
			pEOL = strchr(pLine, '\n');
			if (!pEOL)
				break;
			int curLineLen = (int)(pEOL-pLine);
			if (curLineLen >= SNM_MAX_CHUNK_LINE_LENGTH)
				curLineLen = SNM_MAX_CHUNK_LINE_LENGTH-1; // trim long lines
			memcpy(curLine, pLine, curLineLen);
			curLine[curLineLen] = '\0';
			_ctx->AddLine("%s", curLine); // "%s" needed, see http://github.com/reaper-oss/sws/issues/358
		}
	}
}

void ExtensionConfigToString(WDL_FastString* _str, ProjectStateContext* _ctx)
{
	if (_str && _ctx)
	{
		char* p;
		char linebuf[SNM_MAX_CHUNK_LINE_LENGTH] = "";
		for(;;) 
		{
			if (!_ctx->GetLine(linebuf, sizeof(linebuf)))
			{
				p=linebuf;
				while (*p == ' ' || *p == '\t') p++;
				if (!*p || *p == '>') break;
				_str->Append(p);
				_str->Append("\n");
			}
			else
				break;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// Ini file helpers
///////////////////////////////////////////////////////////////////////////////

// write a full ini file section in one go
// "the data in the buffer pointed to by the lpString parameter consists 
// of one or more null-terminated strings, followed by a final null character"
void SaveIniSection(const char* _iniSectionName, WDL_FastString* _iniSection, const char* _iniFn)
{
	if (_iniSectionName && _iniSection && _iniFn)
	{
		if (char* buf = (char*)calloc(_iniSection->GetLength()+1, sizeof(char)))  // +1 for dbl-null termination
		{
			memcpy(buf, _iniSection->Get(), _iniSection->GetLength());
			for (int j=0; j < _iniSection->GetLength(); j++)
				if (buf[j] == '\n') buf[j] = '\0';
			WritePrivateProfileStruct(_iniSectionName, NULL, NULL, 0, _iniFn); // flush section
			WritePrivateProfileSection(_iniSectionName, buf, _iniFn);
			free(buf);
		}
	}
}

void UpdatePrivateProfileSection(const char* _oldAppName, const char* _newAppName, const char* _iniFn, const char* _newIniFn)
{
	char buf[SNM_MAX_INI_SECTION]="";
	int sectionSz = GetPrivateProfileSection(_oldAppName, buf, SNM_MAX_INI_SECTION, _iniFn);
	WritePrivateProfileStruct(_oldAppName, NULL, NULL, 0, _iniFn); // flush section
	if (sectionSz)
		WritePrivateProfileSection(_newAppName, buf, _newIniFn ? _newIniFn : _iniFn);
}

void UpdatePrivateProfileString(const char* _appName, const char* _oldKey, const char* _newKey, const char* _iniFn, const char* _newIniFn)
{
	char buf[SNM_MAX_PATH] = "";
	GetPrivateProfileString(_appName, _oldKey, "", buf, sizeof(buf), _iniFn);
	WritePrivateProfileString(_appName, _oldKey, NULL, _iniFn); // remove key
	if (*buf)
		WritePrivateProfileString(_appName, _newKey, buf, _newIniFn ? _newIniFn : _iniFn);
}

void SNM_UpgradeIniFiles(int _iniVersion)
{
	if (_iniVersion < 1) // < v2.1.0 #18
	{
		// move the old S&M.ini if needed/possible
		WDL_FastString fn;
		fn.SetFormatted(SNM_MAX_PATH, SNM_OLD_FORMATED_INI_FILE, GetExePath());
		if (FileOrDirExists(fn.Get()))
			MoveFile(fn.Get(), g_SNM_IniFn.Get()); // no check, best effort
    
		if (!FileOrDirExists(g_SNM_IniFn.Get()))
			return; // no upgrade needed, e.g. SWS installed for the very first time
    
		// upgrade deprecated section names 
		UpdatePrivateProfileSection("FXCHAIN", "FXChains", g_SNM_IniFn.Get());
		UpdatePrivateProfileSection("FXCHAIN_VIEW", "RESOURCE_VIEW", g_SNM_IniFn.Get());
		// upgrade deprecated key names (automatically generated now)
		UpdatePrivateProfileString("RESOURCE_VIEW", "DblClick_Type", "DblClickFXChains", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "DblClick_Type_Tr_Template", "DblClickTrackTemplates", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "DblClick_Type_Prj_Template", "DblClickProjectTemplates", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "AutoSaveDirFXChain", "AutoSaveDirFXChains", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "AutoFillDirFXChain", "AutoFillDirFXChains", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "AutoSaveDirTrTemplate", "AutoSaveDirTrackTemplates", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "AutoFillDirTrTemplate", "AutoFillDirTrackTemplates", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "AutoSaveDirPrjTemplate", "AutoSaveDirProjectTemplates", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "AutoFillDirPrjTemplate", "AutoFillDirProjectTemplates", g_SNM_IniFn.Get());
	}
	if (_iniVersion < 2) // < v2.1.0 #21
	{
		// move cycle actions to a new dedicated file (+ make backup if it already exists)
		WDL_FastString fn;
		fn.SetFormatted(SNM_MAX_PATH, SNM_CYCLACTION_BAK_FILE, GetResourcePath());
		if (FileOrDirExists(g_SNM_CyclIniFn.Get()))
			MoveFile(g_SNM_CyclIniFn.Get(), fn.Get());
		UpdatePrivateProfileSection("MAIN_CYCLACTIONS", "Main_Cyclactions", g_SNM_IniFn.Get(), g_SNM_CyclIniFn.Get());
		UpdatePrivateProfileSection("ME_LIST_CYCLACTIONS", "ME_List_Cyclactions", g_SNM_IniFn.Get(), g_SNM_CyclIniFn.Get());
		UpdatePrivateProfileSection("ME_PIANO_CYCLACTIONS", "ME_Piano_Cyclactions", g_SNM_IniFn.Get(), g_SNM_CyclIniFn.Get());
	}
	if (_iniVersion < 3) // < v2.1.0 #22
	{
		WritePrivateProfileString("RESOURCE_VIEW", "DblClick_To", NULL, g_SNM_IniFn.Get()); // remove key
		UpdatePrivateProfileString("RESOURCE_VIEW", "FilterByPath", "Filter", g_SNM_IniFn.Get());
	}
	if (_iniVersion < 4) // < v2.2.0
	{
		UpdatePrivateProfileString("RESOURCE_VIEW", "AutoSaveTrTemplateWithItems", "AutoSaveTrTemplateFlags", g_SNM_IniFn.Get());
#ifdef _WIN32
		UpdatePrivateProfileSection("data\\track_icons", "Track_icons", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "AutoFillDirdata\\track_icons", "AutoFillDirTrack_icons", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "TiedActionsdata\\track_icons", "TiedActionsTrack_icons", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "DblClickdata\\track_icons", "DblClickTrack_icons", g_SNM_IniFn.Get());
#else
		UpdatePrivateProfileSection("data/track_icons", "Track_icons", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "AutoFillDirdata/track_icons", "AutoFillDirTrack_icons", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "TiedActionsdata/track_icons", "TiedActionsTrack_icons", g_SNM_IniFn.Get());
		UpdatePrivateProfileString("RESOURCE_VIEW", "DblClickdata/track_icons", "DblClickTrack_icons", g_SNM_IniFn.Get());
#endif
	}
	if (_iniVersion < 5) // < v2.2.0 #3
		UpdatePrivateProfileSection("LAST_CUEBUS", "CueBuss1", g_SNM_IniFn.Get());
	if (_iniVersion < 6) // < v2.2.0 #6
		WritePrivateProfileStruct("RegionPlaylist", NULL, NULL, 0, g_SNM_IniFn.Get()); // flush section
	if (_iniVersion < 7) // < v2.2.0 #16
	{
		UpdatePrivateProfileSection("RESOURCE_VIEW", "Resources", g_SNM_IniFn.Get());
		UpdatePrivateProfileSection("NOTES_HELP_VIEW", "Notes", g_SNM_IniFn.Get());
		UpdatePrivateProfileSection("FIND_VIEW", "Find", g_SNM_IniFn.Get());
		UpdatePrivateProfileSection("MIDI_EDITOR", "MidiEditor", g_SNM_IniFn.Get());
		WritePrivateProfileStruct("LIVE_CONFIGS", NULL, NULL, 0, g_SNM_IniFn.Get()); // flush section, everything moved to rpp

		// update nb of default actions (SNM_LIVECFG_NB_CONFIGS changed)
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_TGL", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_NEXT_LIVE_CFG", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_PREVIOUS_LIVE_CFG", NULL, g_SNM_IniFn.Get());
	}
	if (_iniVersion < 8) // < v2.4.0 #4
	{
		// deprecated
		WritePrivateProfileString("Resources", "ProjectLoaderStartSlot", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("Resources", "ProjectLoaderStartSlot", NULL, g_SNM_IniFn.Get());

		// show some actions by default, if hidden (do not scratch user tweaks)
		if (!GetPrivateProfileInt("NbOfActions", "S&M_DUMMY_TGL", 0, g_SNM_IniFn.Get()))
			WritePrivateProfileString("NbOfActions", "S&M_DUMMY_TGL", NULL, g_SNM_IniFn.Get());
		if (!GetPrivateProfileInt("NbOfActions", "S&M_EXCL_TGL", 0, g_SNM_IniFn.Get()))
			WritePrivateProfileString("NbOfActions", "S&M_EXCL_TGL", NULL, g_SNM_IniFn.Get());
	}
	if (_iniVersion < 9) // < v2.8.7 #0
	{
		// changed SNM_LIVECFG_NB_CONFIGS (4 -> 8)
		WritePrivateProfileString("NbOfActions", "S&M_OPEN_LIVECFG_MONITOR", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_APPLY", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_PRELOAD", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_NEXT_LIVE_CFG", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_PREVIOUS_LIVE_CFG", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_PRELOAD_NEXT_LIVE_CFG", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_PRELOAD_PREVIOUS_LIVE_CFG", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_PRELOAD_LIVE_CFG", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_TGL", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_ON", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_OFF", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_MUTEBUT_TGL", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_MUTEBUT_ON", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_MUTEBUT_OFF", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_OFFLINEBUT_TGL", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_OFFLINEBUT_ON", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_OFFLINEBUT_OFF", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_DISARMBUT_TGL", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_DISARMBUT_ON", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_DISARMBUT_OFF", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_CC123_TGL", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_CC123_ON", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_CC123_OFF", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_FADES_TGL", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_FADES_ON", NULL, g_SNM_IniFn.Get());
		WritePrivateProfileString("NbOfActions", "S&M_LIVECFG_FADES_OFF", NULL, g_SNM_IniFn.Get());
    
		// reaper_sws_whatsnew.txt is not deployed anymore (replaced with online help)
		// note: on Win, this file is removed by NSIS installers
#ifndef _WIN32
		WDL_FastString fn;
		fn.SetFormatted(SNM_MAX_PATH, "%s/UserPlugins/reaper_sws_whatsnew.txt", GetResourcePath());
		SNM_DeleteFile(fn.Get(), false); // lazy cleanup
#endif    
	}
}


///////////////////////////////////////////////////////////////////////////////
// String helpers
///////////////////////////////////////////////////////////////////////////////

// a snprintf that returns >=0 when the string is null terminated and not truncated
// => callers must check the returned value 
int snprintfStrict(char* _buf, size_t _n, const char* _fmt, ...)
{
	va_list va;
	va_start(va, _fmt);
	int l = vsnprintf(_buf, _n, _fmt, va);
	if (l>=(int)_n)
		l = -1;
	va_end(va);
	return l;
}

bool GetStringWithRN(const char* _bufIn, char* _bufOut, int _bufOutSz)
{
	if (!_bufOut || !_bufIn)
		return false;

	int i=0, j=0;
	while (_bufIn[i] && j < _bufOutSz)
	{
		if (_bufIn[i] == '\n') {
			_bufOut[j++] = '\r';
			_bufOut[j++] = '\n';
		}
		else if (_bufIn[i] != '\r')
			_bufOut[j++] = _bufIn[i];
		i++;
	}

	if (j < _bufOutSz)
		_bufOut[j] = '\0';
	else
		_bufOut[_bufOutSz-1] = '\0'; // truncates, best effort..

	return (j < _bufOutSz);
}

const char* FindFirstRN(const char* _str, bool _anyOrder)
{
	const char* p = NULL;
	if (_str)
	{
		if (_anyOrder)
		{
			p = strchr(_str, '\r');
			const char* p2 = strchr(_str, '\n');
			p = !p ? p2 : !p2 ? p :  p<p2 ? p : p2;
		}
		else 
		{
			p = strchr(_str, '\r');
			p = !p ? strchr(_str, '\n') : p;
		}
	}
	return p;
}

char* ShortenStringToFirstRN(char* _str, bool _anyOrder)
{
	char* p = (char*)FindFirstRN(_str, _anyOrder);
	if (p) *p = '\0'; 
	return p;
}

// replace the first _str with _replaceCh in _strInOut
bool ReplaceWithChar(char* _strInOut, const char* _str, const char _replaceCh)
{
	if (_strInOut && *_strInOut)
	{
		if (char* p = strstr(_strInOut, _str))
		{
			const int str_len = strlen(_str);
			p[0] = _replaceCh;
			if (p[str_len]) memmove((char*)(p+1), p+str_len, strlen(p+str_len)+1);
			else p[1] = '\0';
			return true;
		}
	}
	return false;
}

// _outItems: it's up to the caller to unalloc things
void SplitStrings(const char* _list, WDL_PtrList<WDL_FastString>* _outItems, const char* _sep)
{
	char tmp[512]="";
	lstrcpyn(tmp, _list, sizeof(tmp));
	char* tok = strtok(tmp, _sep);
	while (tok) {
		_outItems->Add(new WDL_FastString(tok));
		tok = strtok(NULL, _sep);
	}
}


///////////////////////////////////////////////////////////////////////////////
// Time/play helpers
///////////////////////////////////////////////////////////////////////////////

// note: the API's SnapToGrid() requires snap options to be enabled..
int SNM_SnapToMeasure(double _pos)
{
	int meas;
	if (TimeMap2_timeToBeats(NULL, _pos, &meas, NULL, NULL, NULL) > SNM_FUDGE_FACTOR) {
		double measPosA = TimeMap2_beatsToTime(NULL, 0.0, &meas); meas++;
		double measPosB = TimeMap2_beatsToTime(NULL, 0.0, &meas);
		return ((_pos-measPosA) > (measPosB-_pos) ? meas : meas-1);
	}
	return meas;
}

void TranslatePos(double _pos, int* _h, int* _m, int* _s, int* _ms)
{
	if (_h) *_h = int(_pos/3600);
	if (_m) *_m = int((_pos - 3600*int(_pos/3600)) / 60);
	if (_s) *_s = int(_pos - 3600*int(_pos/3600) - 60*int((_pos-3600*int(_pos/3600))/60));
	if (_ms) *_ms = int(1000*(_pos-int(_pos)) + 0.5); // rounding
}

double SeekPlay(double _pos, bool _moveView)
{
	double cursorpos = GetCursorPositionEx(NULL);
	PreventUIRefresh(1);
	SetEditCurPos2(NULL, _pos, _moveView, true);
	if ((GetPlayStateEx(NULL)&1) != 1) OnPlayButton();
	SetEditCurPos2(NULL, cursorpos, false, false);
	PreventUIRefresh(-1);
#ifdef _SNM_DEBUG
	char dbg[256] = "";
	snprintf(dbg, sizeof(dbg), "SeekPlay() - Pos: %f\n", _pos);
	OutputDebugString(dbg);
#endif
	return cursorpos;
}


///////////////////////////////////////////////////////////////////////////////
// Action helpers
///////////////////////////////////////////////////////////////////////////////

// "fixes" the API's NamedCommandLookup, e.g. NamedCommandLookup("65534")
// returns "65534" although this action doesn't exist
// _hardCheck: if true, do more tests on the returned command id because the 
//             API's NamedCommandLookup can return an id although the related 
//             action is not registered yet, ex: at init time, when an action
//             is attached to a mouse modifier
// _section: section or NULL for the main section
// note: calling this func with default param values like SNM_NamedCommandLookup("_bla") 
//       is the same as the native NamedCommandLookup("_bla")
int SNM_NamedCommandLookup(const char* _custId, KbdSectionInfo* _section, bool _hardCheck)
{
	int cmdId = 0;
	if (_custId && *_custId)
	{
		// NamedCommandLookup() works for all sections (unique command ids accross sections)
		if (*_custId == '_')
			cmdId = NamedCommandLookup(_custId);
		else
			cmdId = atoi(_custId);

		// make sure things like -666 won't match
		if (cmdId)
		{
			bool found = false;
			KbdSectionInfo* section = _section ? _section : SNM_GetActionSection(SNM_SEC_IDX_MAIN);
			for (int i=0; i<section->action_list_cnt; i++) {
				if (section->action_list[i].cmd == cmdId) {
					found = true;
					break;
				}
			}
			if (!found)
				cmdId = 0;
		}
	}

	if (_hardCheck && cmdId)
		if (const char* desc = kbd_getTextFromCmd(cmdId, _section))
			if (!desc || !*desc)
				cmdId = 0;
	return cmdId;
}

const char* SNM_GetTextFromCmd(int _cmdId, KbdSectionInfo* _section)
{
	if (!_section || _section==SNM_GetActionSection(SNM_SEC_IDX_MAIN)) {
		const char* name = SWS_CMD_SHORTNAME(SWSGetCommandByID(_cmdId));
		if (*name) return name;
	}
	return kbd_getTextFromCmd(_cmdId, _section);
}

bool LoadKbIni(WDL_PtrList<WDL_FastString>* _out)
{
	char buf[SNM_MAX_PATH] = "";
	if (_out && snprintfStrict(buf, sizeof(buf), SNM_KB_INI_FILE, GetResourcePath()) > 0)
	{
		_out->Empty(true);
		if (FILE* f = fopenUTF8(buf, "r"))
		{
			while(fgets(buf, sizeof(buf), f) && *buf)
				if (!_strnicmp(buf,"ACT",3) || !_strnicmp(buf,"SCR",3))
					_out->Add(new WDL_FastString(buf));
			fclose(f);
			return true;
		}
	}
	return false;
}

// returns 1 for a macro, 2 for a script, 0 if not found
// _custId: custom id (both formats are allowed: "bla" and "_bla")
// _inMacroScripts: to optimize accesses to reaper-kb.ini, required
// _outCmds: optionnal, if any it is up to the caller to unalloc items
// note: no cache here (the user can create new macros...)
int GetMacroOrScript(const char* _custId, int _sectionUniqueId, WDL_PtrList<WDL_FastString>* _inMacroScripts, WDL_PtrList<WDL_FastString>* _outCmds, WDL_FastString* _outName)
{
	if (!_custId || !_inMacroScripts)
		return 0;

	if (*_custId == '_')
		_custId++; // custom ids in reaper-kb.ini do not start with '_'

	if (_outCmds)
		_outCmds->Empty(true);

	int found = 0;
	for (int i=0; i<_inMacroScripts->GetSize(); i++)
	{
		if (const char* cmd = _inMacroScripts->Get(i) ? _inMacroScripts->Get(i)->Get() : "")
		{
			LineParser lp(false);
			if (!lp.parse(cmd) && 
				lp.getnumtokens()>=5 &&
				!_stricmp(lp.gettoken_str(3), _custId))
			{
				int success, iniSecId = lp.gettoken_int(2, &success);
				if (success && iniSecId==_sectionUniqueId)
				{
					found = !_stricmp(lp.gettoken_str(0), "ACT") ? 1 : !_stricmp(lp.gettoken_str(0), "SCR") ? 2 : 0;
					if (found)
					{
						if (_outName)
							_outName->Set(lp.gettoken_str(4));
						if (_outCmds)
						{
							for (int i=5; i<lp.getnumtokens(); i++)
							{
								WDL_FastString* cmd = new WDL_FastString;
								const char* p = FindFirstRN(lp.gettoken_str(i)); // there are some "\r\n" sometimes
								cmd->Set(lp.gettoken_str(i), p ? (int)(p-lp.gettoken_str(i)) : 0);
								_outCmds->Add(new WDL_FastString(cmd));
							}
						}
					}
					break;
				}
			}
		}
	}
	return found;
}

// test if an action name or a custom id is a macro/script one
// *not* based on NamedCommandLookup() so that it works even if _cmd is not registered yet
// note: a bit hacky but we definitely need this..
ActionType GetActionType(const char* _cmd, bool _cmdIsName)
{
	if (_cmd && *_cmd)
	{
		// _cmd is an action name?
		if (_cmdIsName)
		{
			const char* tag = __localizeFunc("Custom","actions",0);
			int len = strlen(tag);
			if (!_strnicmp(_cmd, tag, len) && _cmd[len] == ':') return ActionType::Custom;
			tag = __localizeFunc("Script","actions",0);
			len = strlen(tag);
			if (!_strnicmp(_cmd, tag, len) && _cmd[len] == ':') return ActionType::ReaScript;
			return ActionType::Unknown;
		}
		// _cmd is a custom id (both formats are allowed: "bla" and "_bla")
		else
		{
			int len=0;
			if (*_cmd=='_') _cmd++;

			bool probably_new_script_id=false; // REAPER v5.05pre1+
			if (!strncmp(_cmd, "RS", 2))
			{
				probably_new_script_id=true;
				_cmd+=2;
        
				// v5.11pre14+: scripts registered in non-main sections have an extra "UniqueSectionIdInHexa_" tag, skip it
				if (strchr(_cmd, '_')) while (*_cmd)
				{
					if ((*_cmd>='0' && *_cmd<='9') ||  (*_cmd>='a' && *_cmd<='f')) _cmd++;
					else if (*_cmd=='_') { _cmd++; break; }
					else return ActionType::Unknown;
				}
			}

			while (*_cmd)
			{
				if ((*_cmd>='0' && *_cmd<='9') ||  (*_cmd>='a' && *_cmd<='f')) { _cmd++; len++; }
				else return ActionType::Unknown;
			}

			if(len == (probably_new_script_id ? WDL_SHA1SIZE*2 : SNM_MAX_MACRO_CUSTID_LEN))
				return ActionType::ReaScript;
		}
	}
	return ActionType::Unknown;
}

bool IsMacroOrScript(const char* cmd, const bool cmdIsName)
{
	const ActionType type = GetActionType(cmd, cmdIsName);
	return type == ActionType::Custom || type == ActionType::ReaScript;
}

// check numeric ids
// returns 0=not a numeric custom id (does not ensure _custId is valid!), -1=bad sws custom id, -2=bad script/macro custom id
int CheckSwsMacroScriptNumCustomId(const char* _custId, int _secIdx)
{
	if (_custId && *_custId!='_')
	{
		if (int numCmdId = atoi(_custId))
		{
			// sws check
			if (SWSGetCommandByID(numCmdId)) // unique cmd ids acrross sections
				return -1;
			// macro check
			if (IsMacroOrScript(kbd_getTextFromCmd(numCmdId, SNM_GetActionSection(_secIdx)), true))
				return -2;
		}
	}
	return 0;
}

int SNM_GetActionSectionUniqueId(int _sectionIdx)
{
	SECTION_INFO_T *info = SNM_GetActionSectionInfo(_sectionIdx);
	if (info) return info->unique_id;
	return -1;
}

int SNM_GetActionSectionIndex(int _uniqueId)
{
	for (int i=0; i<SNM_NUM_MANAGED_SECTIONS; i++)
		if (SNM_GetActionSectionUniqueId(i) == _uniqueId)
			return i;
	return -1;
}

const char* SNM_GetActionSectionName(int _sectionIdx)
{
	if (KbdSectionInfo* sec = SNM_GetActionSection(_sectionIdx))
		return __localizeFunc(sec->name,"accel_sec",0);
	return "";
}

// returns NULL on error (must not be considered as the "Main" section here!)
KbdSectionInfo* SNM_GetActionSection(int _idx) {
	int id = SNM_GetActionSectionUniqueId(_idx);
	return id>=0 ? SectionFromUniqueID(id) : NULL;
}

bool LearnAction(KbdSectionInfo* _section, int _cmdId)
{
	if (!_section)
		_section = SNM_GetActionSection(SNM_SEC_IDX_MAIN);

	int nbShortcuts = CountActionShortcuts(_section, _cmdId);
	if (nbShortcuts>0)
	{
		char buf[128] = "";
		WDL_FastString shortcuts;
		for (int i=0; i<nbShortcuts; i++)
		{
			if (GetActionShortcutDesc(_section, _cmdId, i, buf, sizeof(buf)) && *buf)
			{
				if (shortcuts.GetLength())
					shortcuts.Append(",");
				shortcuts.Append(" ");
				shortcuts.Append(buf);
			}
		}
		shortcuts.Ellipsize(32, 32);

		WDL_FastString msg;
		msg.SetFormatted(256, __LOCALIZE_VERFMT("The action \"%s\" is already bound to:","sws_mbox"), kbd_getTextFromCmd(_cmdId, _section));
		msg.Append(shortcuts.GetLength() ? shortcuts.Get() : __LOCALIZE("<unknown bindings>","sws_mbox"));
		msg.Append("\n");
		msg.Append(nbShortcuts>1 || !shortcuts.GetLength() ?
			__LOCALIZE("Do you want to replace those bindings?","sws_mbox") : 
			__LOCALIZE("Do you want to replace this binding?","sws_mbox"));
		msg.Append("\n");
		msg.Append(__LOCALIZE("If you select No, a new binding will be added.","sws_mbox"));

		switch (MessageBox(GetMainHwnd(), msg.Get(), __LOCALIZE("S&M - Confirmation","sws_mbox"), MB_YESNOCANCEL))
		{
			case IDYES:
				while (DeleteActionShortcut(_section, _cmdId, --nbShortcuts));
				break;
			case IDNO:
				break;
			default:
				return false;
		}
	}
	return DoActionShortcutDialog(GetMainHwnd(), _section, _cmdId, nbShortcuts);
}

bool GetSectionURL(bool _alr, KbdSectionInfo* _section, char* _sectionURL, int _sectionURLSize)
{
	if (!_sectionURL || _sectionURLSize<=0)
		return false;
  
	if (!_section)
		_section = SNM_GetActionSection(SNM_SEC_IDX_MAIN);

	if (_alr)
	{
		switch (_section->uniqueID)
		{
			case 0:
			case 100:
				lstrcpyn(_sectionURL, "ALR_Main", _sectionURLSize);
				break;
			case 32060:
				lstrcpyn(_sectionURL, "ALR_MIDIEditor", _sectionURLSize);
				break;
			case 32061:
				lstrcpyn(_sectionURL, "ALR_MIDIEvtList", _sectionURLSize);
				break;
			case 32062:
				lstrcpyn(_sectionURL, "ALR_MIDIInline", _sectionURLSize);
				break;
			case 32063:
				lstrcpyn(_sectionURL, "ALR_MediaExplorer", _sectionURLSize);
				break;
			default:
				return false;
		}
	}
	else
	{
		lstrcpyn(_sectionURL, __localizeFunc(_section->name,"accel_sec",0), _sectionURLSize);
	}
	return true;
}

bool IsTakePolarityFlipped(MediaItem_Take* take)
{
	if (*(double*)GetSetMediaItemTakeInfo(take, "D_VOL", NULL) < 0.0)
		return true;
	else
		return false;
}


///////////////////////////////////////////////////////////////////////////////
// Other util funcs
///////////////////////////////////////////////////////////////////////////////

#ifdef _SNM_MISC

#ifdef _WIN32
#define FNV64_IV ((WDL_UINT64)(0xCBF29CE484222325i64))
#else
#define FNV64_IV ((WDL_UINT64)(0xCBF29CE484222325LL))
#endif

WDL_UINT64 FNV64(WDL_UINT64 h, const unsigned char* data, int sz)
{
	int i;
	for (i=0; i < sz; ++i)
	{
#ifdef _WIN32
		h *= (WDL_UINT64)0x00000100000001B3i64;
#else
		h *= (WDL_UINT64)0x00000100000001B3LL;
#endif
		h ^= data[i];
	}
	return h;
}

// _strOut[65] by definition..
bool FNV64(const char* _strIn, char* _strOut)
{
	WDL_UINT64 h = FNV64_IV;
	const char *p=_strIn;
	while (*p)
	{
		char c = *p++;
		if (c == '\\')
		{
			if (*p == '\\'||*p == '"' || *p == '\'') h=FNV64(h,(unsigned char *)p,1);
			else if (*p == 'n') h=FNV64(h,(unsigned char *)"\n",1);
			else if (*p == 'r') h=FNV64(h,(unsigned char *)"\r",1);
			else if (*p == 't') h=FNV64(h,(unsigned char *)"\t",1);
			else if (*p == '0') h=FNV64(h,(unsigned char *)"",1);
			else return false;
			p++;
		}
		else
			h=FNV64(h,(unsigned char *)&c,1);
	}
	h=FNV64(h,(unsigned char *)"",1);
	return (snprintfStrict(_strOut, 65, "%08X%08X",(int)(h>>32),(int)(h&0xffffffff)) > 0);
}

#endif

