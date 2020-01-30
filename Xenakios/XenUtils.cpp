/******************************************************************************
/ XenUtils.cpp
/
/ Copyright (c) 2010 Tim Payne (SWS), original code by Xenakios
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
#include "../SnM/SnM_Util.h"

using namespace std;

// Globals
std::vector<std::string> g_filenames;

int GetActiveTakes(WDL_PtrList<MediaItem_Take> *MediaTakes)
{
	MediaTrack* CurTrack;
	MediaItem* CurItem;
	MediaItem_Take* CurTake;
	int TakeCount=0;
	int trackID;
	int itemID;
	for (trackID=0;trackID<GetNumTracks();trackID++)
	{
		CurTrack=CSurf_TrackFromID(trackID+1,false);
		int numItems=GetTrackNumMediaItems(CurTrack);
		for (itemID=0;itemID<numItems;itemID++)
		{
			CurItem = GetTrackMediaItem(CurTrack,itemID);
			bool ItemSelected=*(bool*)GetSetMediaItemInfo(CurItem,"B_UISEL",NULL);
			if (ItemSelected) 
			{ 
				if (GetMediaItemNumTakes(CurItem))
				{
					int currentTakeIndex=*(int*)GetSetMediaItemInfo(CurItem,"I_CURTAKE",NULL);
					if ((CurTake=GetMediaItemTake(CurItem,currentTakeIndex)))
					{
						MediaTakes->Add(CurTake);
						TakeCount++;
					}
				}
			}
		}
	}
	return TakeCount;	
}

// Returns number of files found with desired extension
// if extension is == NULL, looks for Media Files with Reaper's builtin IsMediaExtension
char g_CurrentScanFile[1024] = "";
bool g_bAbortScan = false;
int SearchDirectory(vector<string> &refvecFiles, const char* cDir, const char* cExt, bool bSubdirs)
{
	WDL_DirScan ds;
	int iRet = ds.First(cDir);
	int found = 0;
	g_bAbortScan = false;
	if (!iRet)
	{
		do
		{
			if (strcmp(ds.GetCurrentFN(), ".") == 0 || strcmp(ds.GetCurrentFN(), "..") == 0)
				continue;
			WDL_String foundFile;
			ds.GetCurrentFullFN(&foundFile);
			lstrcpyn(g_CurrentScanFile, foundFile.Get(), 1024);
			if (bSubdirs && ds.GetCurrentIsDirectory())
			{
				found += SearchDirectory(refvecFiles, foundFile.Get(), cExt, true);
			}
			else
			{
				char* cFoundExt  = strrchr(foundFile.Get(), '.');
				if (cFoundExt)
				{
					cFoundExt++;
					if ((!cExt && IsMediaExtension(cFoundExt, false)) || (cExt && _stricmp(cFoundExt, cExt) == 0))
					{
						refvecFiles.push_back(foundFile.Get());
						found++;
					}
				}
			}
		}
		while(!ds.Next() && !g_bAbortScan);
	}
	return found;
}

#ifdef _WIN32

char *BrowseForFiles(const char *text, const char *initialdir, const char *initialfile, bool allowmul, const char *extlist)
{
	int rv = 0;
	int temp_size = allowmul ? (MAX_PATH * 512) : MAX_PATH * 2;
	char *temp=(char *)malloc(temp_size);
	if (!temp)
		return NULL;

	memset(temp, 0, temp_size);

	OPENFILENAME l = {sizeof(l),};
	l.hwndOwner = g_hwndParent;
	l.lpstrFilter = extlist;
	l.lpstrFile = temp;
	l.nMaxFile = temp_size;
	l.lpstrTitle = text;
	l.lpstrDefExt = NULL;
	l.lpstrInitialDir = initialdir;
	l.Flags = OFN_DONTADDTORECENT | OFN_EXPLORER | (allowmul ? OFN_ALLOWMULTISELECT : 0) | OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&l))
	{
		if (temp[strlen(temp)+1]) // Check for more than one file
		{	// More than one file is returned in the format PATH.FILE1.FILE2..
			// We want PATH\FILE1.PATH\FILE2.. (period means NULL here)
			int pathLength = (int)strlen(temp);
			char* pFile = temp + pathLength + 1;
			int newLength = 1; // double NULL terminate
			while (pFile[0])
			{
				newLength += pathLength + (int)strlen(pFile) + 2; // 2 extra for \ + NULL
				pFile += strlen(pFile) + 1;
			}
			char* fullFilenames = (char*)malloc(newLength);
			pFile = temp + pathLength + 1;
			char* pFull = fullFilenames;
			while (pFile[0])
			{
				sprintf(pFull, "%s\\%s", temp, pFile);
				pFull += strlen(pFull) + 1;
				pFile += strlen(pFile) + 1;
			}
			pFull[0] = 0;
			free(temp);
			return fullFilenames;
		}
		return temp;
	}

	free(temp);
	return NULL;
}

std::vector<char*> BrowseForFilesMultiSelect(const char* text, const char* initialdir, const char* initialfile, bool allowmul, const char* extlist)
{
	std::vector<char*> filelist;

	int rv = 0;
	int temp_size = allowmul ? (MAX_PATH * 512) : MAX_PATH * 2;
	char* temp = (char*)malloc(temp_size);
	if (!temp)
		return filelist;

	memset(temp, 0, temp_size);

	OPENFILENAME l = { sizeof(l), };
	l.hwndOwner = g_hwndParent;
	l.lpstrFilter = extlist;
	l.lpstrFile = temp;
	l.nMaxFile = temp_size;
	l.lpstrTitle = text;
	l.lpstrDefExt = NULL;
	l.lpstrInitialDir = initialdir;
	l.Flags = OFN_DONTADDTORECENT | OFN_EXPLORER | (allowmul ? OFN_ALLOWMULTISELECT : 0) | OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&l))
	{
		if (temp[strlen(temp) + 1]) // Check for more than one file
		{	// More than one file is returned in the format PATH.FILE1.FILE2..
			// We want PATH\FILE1.PATH\FILE2.. (period means NULL here)
			int pathLength = (int)strlen(temp);
			char* pFile = temp + pathLength + 1;
			int newLength = 1; // double NULL terminate
			while (pFile[0])
			{
				newLength += pathLength + (int)strlen(pFile) + 2; // 2 extra for \ + NULL
				pFile += strlen(pFile) + 1;
			}
			char* fullFilenames = (char*)malloc(newLength);
			pFile = temp + pathLength + 1;
			char* pFull = fullFilenames;
			while (pFile[0])
			{
				sprintf(pFull, "%s\\%s", temp, pFile);
				filelist.push_back(pFull);
				pFull += strlen(pFull) + 1;
				pFile += strlen(pFile) + 1;
			}
			pFull[0] = 0;
			free(temp);
			//return fullFilenames;
			return filelist;
		}

		if (filelist.empty()) {
			filelist.push_back(temp);
		}

		return filelist;
	}

	return filelist;
}

bool BrowseForSaveFile(const char *text, const char *initialdir, const char *initialfile, const char *extlist, char *fn, int fnsize)
{
	if (initialfile)
		lstrcpyn(fn, initialfile, fnsize);
	else
		memset(fn,0,fnsize);
	
	// Find the first extension in the extlist and use that as the default
	char defExt[64] = { 0, };
	if (extlist)
	{
		const char* pExt = extlist + strlen(extlist)+1;
		// Ignore *.*
		if (pExt[2] != '*')
		{
			lstrcpyn(defExt, pExt+2, 64);
			// Chop at ';' to select the first in the list
			char* pNextExt = strchr(defExt, ';');
			if (pNextExt)
				*pNextExt = 0;
		}
	}

	OPENFILENAME l={sizeof(l),};
	l.hwndOwner = g_hwndParent;
	l.lpstrFilter = extlist;
	l.lpstrFile = fn;
	l.nMaxFile = fnsize-1;
	l.lpstrTitle = text;
	l.lpstrDefExt = defExt;
	l.lpstrInitialDir = initialdir;
	
	l.Flags = OFN_DONTADDTORECENT | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_OVERWRITEPROMPT;

	if (GetSaveFileName(&l))
		return true;
	else
		return false;	
}

static int CALLBACK BrowseForDirectoryCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
			if (lpData && ((wchar_t *)lpData)[0]) 
				SendMessage(hwnd,BFFM_SETSELECTIONW,1,(LPARAM)lpData);
		break;
	}
	return 0;
}

bool BrowseForDirectory(const char *text, const char *initialdir, char *fn, int fnsize)
{
  wchar_t widename[4096];
  wchar_t widetext[4096];
  MultiByteToWideChar(CP_UTF8, 0, initialdir, -1, widename, 4096);
  MultiByteToWideChar(CP_UTF8, 0, text, -1, widetext, 4096);

  // explicitly call unicode versions and then convert filename back to utf8
  BROWSEINFOW bi={g_hwndParent, NULL, widename, widetext, BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE, BrowseForDirectoryCallbackProc, (LPARAM)widename};
  LPITEMIDLIST idlist = SHBrowseForFolderW( &bi );
  if (idlist) 
  {
    SHGetPathFromIDListW( idlist, widename);

    IMalloc *m;
    SHGetMalloc(&m);
    m->Free(idlist);
    
    WideCharToMultiByte(CP_UTF8, 0, widename, -1, fn, fnsize, NULL, NULL);
    return true;
  }
  return false;
}
#endif

bool FileExists(const char* file) {
	return FileOrDirExists(file);
}
