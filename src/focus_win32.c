#include "focus_win32.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void basename_w(const wchar_t* full, wchar_t* out, int out_len) {
  const wchar_t* last = full;
  for (const wchar_t* p = full; *p; ++p) {
    if (*p == L'\\' || *p == L'/') last = p + 1;
  }
  wcsncpy_s(out, (size_t)out_len, last, _TRUNCATE);
}

bool focus_get_foreground_exe(wchar_t* out_exe, int out_len) {
  if (!out_exe || out_len <= 0) return false;
  out_exe[0] = 0;

  HWND hwnd = GetForegroundWindow();
  if (!hwnd) return false;

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (!pid) return false;

  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) return false;

  wchar_t fullpath[1024];
  DWORD size = (DWORD)(sizeof(fullpath) / sizeof(fullpath[0]));
  BOOL ok = QueryFullProcessImageNameW(h, 0, fullpath, &size);
  CloseHandle(h);
  if (!ok) return false;

  basename_w(fullpath, out_exe, out_len);
  return out_exe[0] != 0;
}
