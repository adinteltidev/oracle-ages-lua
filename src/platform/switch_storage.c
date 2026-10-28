#include "platform/switch_storage.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef ORACLES_STORAGE_RENAME
#define ORACLES_STORAGE_RENAME rename
#else
// Tests use real files with a non-overwriting rename and injected failures.
extern int ORACLES_STORAGE_RENAME(const char *, const char *);
#endif
static bool suffix(char *out, size_t size, const char *path, const char *ext) {
  int n = snprintf(out, size, "%s%s", path, ext);
  if (n < 0 || (size_t)n >= size) { errno = ENAMETOOLONG; return false; }
  return true;
}
static int regular_file(const char *path) {
  struct stat st;
  if (!stat(path, &st)) {
    if (!S_ISREG(st.st_mode)) { errno = EISDIR; return -1; }
    return 1;
  }
  return errno == ENOENT ? 0 : -1;
}
bool switch_storage_recover(const char *path) {
  int exists = regular_file(path);
  if (exists) return exists == 1;
  char backup[512];
  if (!suffix(backup, sizeof backup, path, ".bak")) return false;
  exists = regular_file(backup);
  if (exists < 0) return false;
  return !exists || ORACLES_STORAGE_RENAME(backup, path) == 0;
}
bool switch_storage_commit(const char *temporary, const char *path) {
  char backup[512];
  if (!strcmp(temporary, path)) { errno = EINVAL; return false; }
  if (!suffix(backup, sizeof backup, path, ".bak")) return false;
  if (regular_file(temporary) != 1) { if (!errno) errno = ENOENT; return false; }
  if (!switch_storage_recover(path)) return false;
  int exists = regular_file(path);
  if (exists < 0) return false;
  if (exists) {
    int have_backup = regular_file(backup);
    if (have_backup < 0) return false;
    if (have_backup && remove(backup)) return false;
    if (ORACLES_STORAGE_RENAME(path, backup)) return false;
  }
  if (ORACLES_STORAGE_RENAME(temporary, path)) {
    int error = errno;
    // If rollback also fails, keep .bak for recovery at the next load/start.
    if (exists) ORACLES_STORAGE_RENAME(backup, path);
    errno = error;
    return false;
  }
  // Keep one previous copy. The next successful write rotates it safely.
  return true;
}
bool switch_storage_sync(const char *path) {
  FILE *f = fopen(path, "r+b");
  if (!f) return false;
  bool ok = fsync(fileno(f)) == 0;
  int error = errno;
  if (fclose(f) && ok) { ok = false; error = errno; }
  if (!ok) errno = error;
  return ok;
}
bool switch_storage_write(const char *path, const void *data, size_t size) {
  char tmp[512];
  if (!suffix(tmp, sizeof tmp, path, ".tmp")) return false;
  FILE *f = fopen(tmp, "wb");
  if (!f) return false;
  bool ok = fwrite(data, 1, size, f) == size;
  int error = ok ? 0 : (errno ? errno : EIO);
  if (fflush(f)) { ok = false; error = errno; }
  if (ok && fsync(fileno(f))) { ok = false; error = errno; }
  if (fclose(f)) { ok = false; error = errno; }
  if (ok) { ok = switch_storage_commit(tmp, path); if (!ok) error = errno; }
  if (!ok) { remove(tmp); errno = error; }
  return ok;
}
