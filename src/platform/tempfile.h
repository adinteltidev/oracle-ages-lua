#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// Reserve a distinct file in the OS temporary directory; callers remove it after use.
static inline bool oracles_temp_path(char *path, size_t capacity) {
#ifdef _WIN32
  wchar_t dir[MAX_PATH], file[MAX_PATH];
  DWORD n = GetTempPathW(MAX_PATH, dir);
  if (!n || n >= MAX_PATH || !GetTempFileNameW(dir, L"ora", 0, file)) return false;
  if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, file, -1, path, (int)capacity, NULL, NULL)) {
    DeleteFileW(file); return false;
  }
  return true;
#else
  const char *dir = getenv("TMPDIR");
  if (!dir || !*dir) dir = "/tmp";
  int n = snprintf(path, capacity, "%s/oracles-native-XXXXXX", dir);
  if (n < 0 || (size_t)n >= capacity) return false;
  int fd = mkstemp(path);
  if (fd < 0) return false;
  close(fd); return true;
#endif
}
