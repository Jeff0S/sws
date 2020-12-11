/******************************************************************************
/ cfillion.cpp
/
/ Copyright (c) 2017-2019 Christian Fillion
/ https://cfillion.ca
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
#include "cfillion.hpp"

#include "SnM/SnM_FX.h"
#include "SnM/SnM_Window.h"
#include "version.h"

#include <WDL/localize/localize.h>

#ifdef _WIN32
  constexpr unsigned int CLIPBOARD_FORMAT = CF_UNICODETEXT;
#else
// using RegisterClipboardFormat instead of CF_TEXT for compatibility with REAPER v5
// (prior to WDL commit 0f77b72adf1cdbe98fd56feb41eb097a8fac5681)
#  define CLIPBOARD_FORMAT RegisterClipboardFormat("SWELL__CF_TEXT")
#endif

extern WDL_PtrList_DOD<WDL_FastString> g_script_strs;

void CF_SetClipboard(const char *buf)
{
#ifdef _WIN32
  const int length = MultiByteToWideChar(CP_UTF8, 0, buf, -1, nullptr, 0);
  const size_t size = length * sizeof(wchar_t);
#else
  const size_t size = strlen(buf) + 1;
#endif

  HANDLE mem = GlobalAlloc(GMEM_MOVEABLE, size);
#ifdef _WIN32
  MultiByteToWideChar(CP_UTF8, 0, buf, -1, (wchar_t *)GlobalLock(mem), length);
#else
  memcpy(GlobalLock(mem), buf, size);
#endif
  GlobalUnlock(mem);

  OpenClipboard(GetMainHwnd());
  EmptyClipboard();
  SetClipboardData(CLIPBOARD_FORMAT, mem);
  CloseClipboard();
}

class ClipboardReader {
public:
  ClipboardReader()
  {
    OpenClipboard(GetMainHwnd());
    m_mem = GetClipboardData(CLIPBOARD_FORMAT);
    m_data = reinterpret_cast<decltype(m_data)>(GlobalLock(m_mem));

    if(!m_data) {
      m_size = 0;
      return;
    }

#ifdef _WIN32
    m_size = WideCharToMultiByte(CP_UTF8, 0, m_data, -1, nullptr, 0, nullptr, nullptr) - 1;
#else
    m_size = strlen(m_data);
#endif
  }

  // does not null-terminate the output buffer unless its bigger than the
  // clipboard data
  void read(char *buf, int bufSize)
  {
    if(bufSize > m_size) {
      bufSize = m_size;
      buf[m_size] = 0;
    }

    if(bufSize <= 0)
      return;

#ifdef _WIN32
    WideCharToMultiByte(CP_UTF8, 0, m_data, -1, buf, bufSize, nullptr, nullptr);
#else
    std::copy(m_data, m_data + bufSize, buf);
#endif
  }

  ~ClipboardReader()
  {
    GlobalUnlock(m_mem);
    CloseClipboard();
  }

  int size() const { return m_size; }
  operator bool() const { return m_data != nullptr; }

private:
  HANDLE m_mem;
#ifdef _WIN32
  const wchar_t *m_data;
#else
  const char *m_data;
#endif
  int m_size;
};

void CF_GetClipboard(char *buf, int bufSize)
{
  if(ClipboardReader clipboard{}) {
    if(realloc_cmd_ptr(&buf, &bufSize, clipboard.size()))
      clipboard.read(buf, bufSize);
  }
}

const char *CF_GetClipboardBig(WDL_FastString *output)
{
  ClipboardReader clipboard;

  if(!clipboard || g_script_strs.Find(output) == -1)
    return nullptr;

  output->SetLen(clipboard.size());
  clipboard.read(const_cast<char *>(output->Get()), output->GetLength());

  return output->Get();
}

bool CF_ShellExecute(const char *file, const char *args)
{
  // Windows's implementation of ShellExecute returns a fake HINSTANCE (greater
  // than 32 on success) while SWELL's implementation returns a BOOL.

#ifdef _WIN32
  static_assert(&ShellExecute == &ShellExecuteUTF8,
    "ShellExecute is not aliased to ShellExecuteUTF8");
  HINSTANCE ret = ShellExecute(nullptr, "open", file, args, nullptr, SW_SHOW);
  return ret > (HINSTANCE)32;
#else
  return ShellExecute(nullptr, "open", file, args, nullptr, SW_SHOW);
#endif
}

bool CF_LocateInExplorer(const char *file)
{
  // Quotes inside the filename must not be escaped for the SWELL implementation
  WDL_FastString arg;
  arg.SetFormatted(strlen(file) + 10, R"(/select,"%s")", file);

  return CF_ShellExecute("explorer.exe", arg.Get());
}

void CF_GetSWSVersion(char *buf, const int bufSize)
{
  snprintf(buf, bufSize, "%d.%d.%d.%d", SWS_VERSION);
}

int CF_EnumerateActions(const int section, const int idx, char *nameBuf, const int nameBufSize)
{
  const char *name = "";
  const int cmdId = kbd_enumerateActions(SectionFromUniqueID(section), idx, &name);
  snprintf(nameBuf, nameBufSize, "%s", name);
  return cmdId;
}

const char *CF_GetCommandText(const int section, const int command)
{
  return kbd_getTextFromCmd(command, SectionFromUniqueID(section));
}

static void CF_RefreshTrackFXChainTitle(MediaTrack *track)
{
  // The FX chain is normally updated at the next global timer tick.
  // GetSetMediaTrackInfo updates it immediately when setting I_FXEN.
  //
  // TrackList_AdjustWindows also does the trick (on Windows only since v6)
  // however it is disabled when PreventUIRefresh is used.

  const bool enabled = GetMediaTrackInfo_Value(track, "I_FXEN");
  SetMediaTrackInfo_Value(track, "I_FXEN", enabled);
}

HWND CF_GetTrackFXChain(MediaTrack *track)
{
  if(!track)
    return nullptr;

  char chainTitle[128];

  if(track == GetMasterTrack(nullptr)) {
    snprintf(chainTitle, sizeof(chainTitle), "%s%s",
      __LOCALIZE("FX: ", "fx"), __LOCALIZE("Master Track", "fx"));

    return FindWindowEx(nullptr, nullptr, nullptr, chainTitle);
  }

  const int trackNumber =
    static_cast<int>(GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER"));
  const std::string trackName =
    static_cast<char *>(GetSetMediaTrackInfo(track, "P_NAME", nullptr));
  const bool isFolder = GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH") > 0;

  // HACK: rename the track to uniquely identify its FX chain window across all
  // opened projects
  char guid[64];
  GetSetMediaTrackInfo_String(track, "GUID", guid, false);
  GetSetMediaTrackInfo_String(track, "P_NAME", guid, true);
  CF_RefreshTrackFXChainTitle(track);

  snprintf(chainTitle, sizeof(chainTitle), R"(%s%s %d "%s"%s)",
    __LOCALIZE("FX: ", "fx"), __LOCALIZE("Track", "fx"), trackNumber, guid,
    isFolder ? __LOCALIZE(" (folder)", "fx") : "");

  HWND match = GetReaHwndByTitle(chainTitle);

  // restore the original track name
  GetSetMediaTrackInfo_String(track, "P_NAME",
    const_cast<char *>(trackName.c_str()), true);
  CF_RefreshTrackFXChainTitle(track);

  return match;
}

HWND CF_GetTakeFXChain(MediaItem_Take *take)
{
  if(!take)
    return nullptr;

  const std::string takeName = GetTakeName(take);

  // HACK: Using the GUID to uniquely identify the take's FX chain window. The
  // FX chain window does have some user data attached to it but it does not
  // seem to hint back to the take in any obvious way.
  char guid[64];
  GetSetMediaItemTakeInfo_String(take, "GUID", guid, false);
  GetSetMediaItemTakeInfo_String(take, "P_NAME", guid, true);

  char chainTitle[128];
  snprintf(chainTitle, sizeof(chainTitle), R"(%s%s "%s")",
    __LOCALIZE("FX: ", "fx"), __LOCALIZE("Item", "fx"), guid);

  HWND match = GetReaHwndByTitle(chainTitle);
  GetSetMediaItemTakeInfo_String(take, "P_NAME",
    const_cast<char *>(takeName.c_str()), true);

  return match;
}

HWND CF_GetFocusedFXChain()
{
  // Original idea by amagalma
  // https://forum.cockos.com/showthread.php?t=207220

  MediaTrack *track;
  int trackIndex, itemIndex, fxIndex;
  switch(GetFocusedFX(&trackIndex, &itemIndex, &fxIndex)) {
  case 1:
    if(trackIndex < 1)
      track = GetMasterTrack(nullptr);
    else
      track = GetTrack(nullptr, trackIndex - 1);
    return CF_GetTrackFXChain(track);
  case 2: {
    track = GetTrack(nullptr, trackIndex - 1);
    MediaItem *item = GetTrackMediaItem(track, itemIndex);
    MediaItem_Take *take = GetMediaItemTake(item, HIWORD(fxIndex));
    return CF_GetTakeFXChain(take);
  }
  default:
    return nullptr;
  }
}

int CF_EnumSelectedFX(HWND fxChain, const int index)
{
  if(!fxChain)
    return -1;

  const HWND list = GetDlgItem(fxChain, 1076);
  return ListView_GetNextItem(list, index, LVNI_SELECTED);
}

bool CF_SelectTrackFX(MediaTrack *track, const int index)
{
  // track and index are validated in SelectTrackFX
  return SelectTrackFX(track, index);
}

int CF_GetMediaSourceBitDepth(PCM_source *source)
{
  // parameter validation is already done on the REAPER side, so we're sure
  // source is a valid PCM_source (unless it's null)
  return source ? source->GetBitsPerSample() : 0;
}

double CF_GetMediaSourceBitRate(PCM_source *source)
{
  if(!source)
    return 0;

  double brate{};
  if(source->Extended(PCM_SOURCE_EXT_GETBITRATE, &brate, nullptr, nullptr))
    return brate;

  if(strcmp(source->GetType(), "WAVE"))
    return 0;

  // constant bit rate
  const double chans  = source->GetNumChannels(),
               bdepth = source->GetBitsPerSample(),
               srate  = source->GetSampleRate();

  return srate * bdepth * chans;
}

bool CF_GetMediaSourceOnline(PCM_source *source)
{
  return source && source->IsAvailable();
}

void CF_SetMediaSourceOnline(PCM_source *source, const bool online)
{
  if(source)
    source->SetAvailable(online);
}

bool CF_GetMediaSourceMetadata(PCM_source *source, const char *name,
  char *buf, const int bufSize)
{
  if(!source)
    return false;

  return source->Extended(PCM_SOURCE_EXT_GETMETADATA, const_cast<char *>(name),
    buf, reinterpret_cast<void *>(static_cast<intptr_t>(bufSize))) > 0;
}

bool CF_GetMediaSourceRPP(PCM_source *source, char *buf, const int bufSize)
{
  char *rpp = nullptr;

  if(source)
    source->Extended(PCM_SOURCE_EXT_GETASSOCIATED_RPP, &rpp, nullptr, nullptr);
  if(!rpp)
    return false;

  snprintf(buf, bufSize, "%s", rpp);
  return true;
}

int CF_EnumMediaSourceCues(PCM_source *source, const int index, double *time,
  double *endTime, bool *isRegion, char *name, const int nameSize)
{
  if(!source)
    return 0;

  REAPER_cue cue{};
  const int add = source->Extended(PCM_SOURCE_EXT_ENUMCUES_EX,
    reinterpret_cast<void *>(static_cast<intptr_t>(index)), &cue, nullptr);

  if(time)
    *time = cue.m_time;
  if(endTime)
    *endTime = cue.m_endtime;
  if(isRegion)
    *isRegion = cue.m_isregion;

  if(name && cue.m_name)
    snprintf(name, nameSize, "%s", cue.m_name);

  return add ? index + add : 0;
}

bool CF_ExportMediaSource(PCM_source *source, const char *file)
{
  if(!source)
    return false;

  return source->Extended(PCM_SOURCE_EXT_EXPORTTOFILE,
    const_cast<char *>(file), nullptr, nullptr) > 0;
}
