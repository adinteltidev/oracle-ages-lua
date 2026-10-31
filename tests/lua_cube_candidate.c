#include "lua_cube_candidate.h"
#include "script/gameplay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char ages_cube_candidate_lua[];
extern const unsigned int ages_cube_candidate_lua_size;

bool cube_candidate_load(CubeCandidateRule rule, uint8_t position) {
  const char *expression;
  char bound[80];
  switch (rule) {
    case CUBE_NO_CUBE: expression = "candidate.no_cube(a)"; break;
    case CUBE_COLOR_ACTIVE: expression = "candidate.color_active(a)"; break;
    case CUBE_TILE_MATCHES: expression = "candidate.tile_matches(a,b)"; break;
    case CUBE_ALLOWS:
      snprintf(bound, sizeof bound, "candidate.allows(%u,a,b)", (unsigned)position);
      expression = bound; break;
    default: return false;
  }
  size_t capacity = ages_cube_candidate_lua_size + 512;
  char *source = malloc(capacity);
  if (!source) return false;
  // Reuse the existing protected/limited host through its test seam. Binding to
  // changed is only an adapter for tests, not a new production switch rule.
  int length = snprintf(source, capacity,
    "local candidate=(function()\n%.*s\nend)(); return {"
    "changed=function(a,b) return %s end,"
    "alternate=function(a,b) return (a&b)~=0 end}",
    (int)ages_cube_candidate_lua_size, ages_cube_candidate_lua, expression);
  bool ok = length >= 0 && (size_t)length < capacity && gameplay_load(source, (size_t)length);
  free(source); gameplay_enable(true);
  return ok;
}

bool cube_candidate_query(uint8_t a, uint8_t b, bool *out) {
  return gameplay_decide(0, a, b, out);
}

const char *cube_candidate_revision(void) { return ORACLES_CUBE_CANDIDATE_REVISION; }
