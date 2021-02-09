/******************************************************************************
/ sws_util_generic.cpp
/
/ Copyright (c) 2010 ans later Tim Payne (SWS), Jeffos, Dominik Martin Drzic

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

#include <gdk/gdk.h>

void SWS_GetDateString(int time, char* buf, int bufsize)
{
  time_t a = time;
  struct tm *tm = a >= 0 && a < WDL_INT64_CONST(0x793406fff) ? localtime(&a) : NULL;
  if (!tm) lstrcpyn_safe(buf,"?",bufsize);
  else strftime(buf,bufsize,"%x",tm);
}

void SWS_GetTimeString(int time, char* buf, int bufsize)
{
  time_t a = time;
  struct tm *tm = a >= 0 && a < WDL_INT64_CONST(0x793406fff) ? localtime(&a) : NULL;
  if (!tm) lstrcpyn_safe(buf,"?",bufsize);
  else strftime(buf,bufsize,"%X",tm);
}

HCURSOR SWS_Cursor::makeFromData()
{
  if (inst)
    return inst;

  unsigned char rgba[32*32*4];
  for (size_t i = 0; i < 32*32; ++i) {
    rgba[4*i+0] = rgba[4*i+1] = rgba[4*i+2] = (data[i]&0xF0) | (data[i]>>4);
    rgba[4*i+3] = (data[i]<<4) | (data[i]&0xF);
  }

  GdkPixbuf *pb = gdk_pixbuf_new_from_data(rgba, GDK_COLORSPACE_RGB, true, 8, 32, 32, 32*4, nullptr, nullptr);
  if (!pb) return nullptr;

  GdkCursor *cur = gdk_cursor_new_from_pixbuf(gdk_display_get_default(), pb, hotspot_x, hotspot_y);
  g_object_unref(pb);

  return inst = reinterpret_cast<HCURSOR>(cur);
}

void mouse_event(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData, ULONG_PTR dwExtraInfo)
{
}
