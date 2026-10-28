#include "unit.h"
#include "platform/switch_storage.h"
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
static bool fail_install, fail_rollback;
// Simulate the SD service's non-overwriting rename, using actual disk files.
int storage_test_rename(const char *old, const char *new) {
  if ((fail_install && strstr(old, ".tmp")) || (fail_rollback && strstr(old, ".bak"))) {
    errno = EIO; return -1;
  }
  struct stat st;
  if (!stat(new, &st)) { errno = EEXIST; return -1; }
  return rename(old, new);
}
static void contents(const char *path, const char *want) {
  size_t n; unsigned char *got = read_file(path, &n);
  ASSERT(got); ASSERT_EQ(n, strlen(want)); ASSERT(!memcmp(got, want, n)); free(got);
}
int main(void) {
  char dir[] = "/tmp/oracles-switch-storage-XXXXXX"; ASSERT(mkdtemp(dir));
  char path[512], bak[512], tmp[512];
  snprintf(path, sizeof path, "%s/quick.state", dir);
  snprintf(bak, sizeof bak, "%s.bak", path);
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  ASSERT(switch_storage_write(path, "first", 5));
  // The old frontend's rename(tmp,path) cannot replace this existing file.
  FILE *f = fopen(tmp, "wb"); ASSERT(f); ASSERT_EQ(fclose(f), 0);
  ASSERT_EQ(storage_test_rename(tmp, path), -1); ASSERT_EQ(errno, EEXIST); ASSERT_EQ(remove(tmp), 0);
  ASSERT(switch_storage_write(path, "second", 6)); contents(path, "second"); contents(bak, "first");
  ASSERT(switch_storage_write(path, "third", 5)); contents(path, "third"); contents(bak, "second");
  fail_install = true;
  ASSERT(!switch_storage_write(path, "broken", 6)); ASSERT_EQ(errno, EIO); contents(path, "third");
  fail_rollback = true;
  ASSERT(!switch_storage_write(path, "broken", 6)); ASSERT(access(path, F_OK)); contents(bak, "third");
  fail_install = fail_rollback = false;
  ASSERT(switch_storage_recover(path)); contents(path, "third");
  ASSERT(switch_storage_write(path, "fourth", 6)); contents(path, "fourth"); contents(bak, "third");
  // Failure before committing a temporary must preserve the active save too.
  ASSERT_EQ(mkdir(tmp, 0700), 0);
  ASSERT(!switch_storage_write(path, "broken", 6)); contents(path, "fourth");
  ASSERT_EQ(rmdir(tmp), 0);
  ASSERT_EQ(remove(path), 0); ASSERT_EQ(remove(bak), 0); ASSERT_EQ(rmdir(dir), 0);
  printf("ok switch_storage: first save, repeated overwrite, rollback, backup recovery, write failure\n");
  return 0;
}
