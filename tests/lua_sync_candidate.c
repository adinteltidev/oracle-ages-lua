#include "lua_sync_candidate.h"
#include "script/gameplay.h"
#include <stdio.h>
#include <stdlib.h>

extern const unsigned char ages_sync_candidate_lua[];
extern const unsigned int ages_sync_candidate_lua_size;

bool sync_candidate_load(void) {
  size_t capacity = ages_sync_candidate_lua_size + 256;
  char *source = malloc(capacity);
  if (!source) return false;
  // Test-only adapter to the existing bounded host, with two boolean results.
  int length = snprintf(source, capacity,
    "local candidate=(function()\n%.*s\nend)(); return {"
    "changed=candidate.awake,alternate=candidate.eligible_tile}",
    (int)ages_sync_candidate_lua_size, (const char *)ages_sync_candidate_lua);
  bool ok = length >= 0 && (size_t)length < capacity && gameplay_load(source, (size_t)length);
  free(source); gameplay_enable(true);
  return ok;
}

bool sync_candidate_query(SyncCandidateRule rule, uint8_t value, bool *out) {
  return gameplay_decide((int)rule, value, 0, out);
}
const char *sync_candidate_revision(void) { return ORACLES_SYNC_CANDIDATE_REVISION; }
