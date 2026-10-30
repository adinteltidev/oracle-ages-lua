#include "unit.h"
#include "lua_sync_candidate.h"
#include "script/gameplay.h"

int main(void) {
  ASSERT(sync_candidate_load());
  for (int value = 0; value < 256; ++value) {
    bool awake, eligible;
    ASSERT(sync_candidate_query(SYNC_AWAKE, value, &awake));
    ASSERT(sync_candidate_query(SYNC_ELIGIBLE_TILE, value, &eligible));
    ASSERT_EQ(awake, value != 0);
    ASSERT_EQ(eligible, value != 0xda);
  }
  ASSERT_EQ(gameplay_calls(0), 256); ASSERT_EQ(gameplay_calls(1), 256);
  // Repeat in reverse order on the same VM: rules do not retain puzzle state.
  for (int value = 255; value >= 0; --value) {
    bool awake, eligible;
    ASSERT(sync_candidate_query(SYNC_AWAKE, value, &awake));
    ASSERT(sync_candidate_query(SYNC_ELIGIBLE_TILE, value, &eligible));
    ASSERT_EQ(awake, value != 0); ASSERT_EQ(eligible, value != 0xda);
  }
  ASSERT_EQ(gameplay_calls(0), 512); ASSERT_EQ(gameplay_calls(1), 512);
  ASSERT(gameplay_memory() < 256*1024);
  printf("ok lua_sync_candidate: 1024 bounded decisions; SHA256 %s\n", sync_candidate_revision());
  gameplay_close(); return 0;
}
