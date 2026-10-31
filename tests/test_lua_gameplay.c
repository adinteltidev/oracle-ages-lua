#include "unit.h"
#include "script/gameplay.h"
#include <time.h>
static double now(void) { return (double)clock()/CLOCKS_PER_SEC; }

static void exhaustive_decisions(void) {
  ASSERT(gameplay_init());
  double start = now();
  for (int a = 0; a < 256; ++a) for (int b = 0; b < 256; ++b) {
    bool result;
    ASSERT(gameplay_decide(0, a, b, &result)); ASSERT_EQ(result, a != b);
    ASSERT(gameplay_decide(1, a, b, &result)); ASSERT_EQ(result, (a & b) != 0);
    GameplayBlockOutput out;
    GameplayBlockInput release = {.state = GAMEPLAY_BLOCK_WAIT_RELEASE,
      .input.release = {.threshold = a, .enemies = b}};
    ASSERT(gameplay_block_step(&release, &out)); ASSERT_EQ(out.next, a >= b ? 2 : 1);
    GameplayBlockInput moved = {.state = GAMEPLAY_BLOCK_WAIT_MOVED,
      .input.tile = {.previous = a, .current = b}};
    ASSERT(gameplay_block_step(&moved, &out)); ASSERT_EQ(out.next, a != b ? 3 : 2);
  }
  GameplayBlockInput in = {.state = GAMEPLAY_BLOCK_INITIALIZE};
  GameplayBlockOutput out;
  ASSERT(gameplay_block_step(&in, &out)); ASSERT_EQ(out.next, GAMEPLAY_BLOCK_WAIT_RELEASE);
  in.state = GAMEPLAY_BLOCK_WAIT_DELAY;
  for (int elapsed = 0; elapsed < 2; ++elapsed) {
    in.input.delay_elapsed = elapsed != 0;
    ASSERT(gameplay_block_step(&in, &out)); ASSERT_EQ(out.next, elapsed ? 4 : 3);
  }
  ASSERT_EQ(gameplay_calls(0), 65536); ASSERT_EQ(gameplay_calls(1), 65536);
  ASSERT_EQ(gameplay_block_calls(0), 1); ASSERT_EQ(gameplay_block_calls(1), 65536);
  ASSERT_EQ(gameplay_block_calls(2), 65536); ASSERT_EQ(gameplay_block_calls(3), 2);
  printf("Lua: 262147 decisions, CPU %.6fs, heap %zu bytes\n", now()-start, gameplay_memory());
  ASSERT(gameplay_memory() < 256*1024);
  GameplayMemoryStats stats = gameplay_memory_stats();
  ASSERT_EQ(stats.current, gameplay_memory());
  ASSERT(stats.allocations > 0 && stats.peak >= stats.current && stats.peak <= 256*1024);
  gameplay_close();
  stats = gameplay_memory_stats();
  ASSERT_EQ(stats.current, 0); ASSERT_EQ(stats.peak, 0); ASSERT_EQ(stats.allocations, 0);
}

static void invalid_block_results(void) {
  const char *bodies[] = {"return true", "return 2.0", "return -1", "return 99",
    "return 9223372036854775807", "return {}", "return nil", "return", "return 1,2",
    "while true do end", "local x='x'; for i=1,30 do x=x..x end; return 1"};
  for (size_t i = 0; i < sizeof bodies/sizeof *bodies; ++i) {
    char source[512];
    snprintf(source, sizeof source, "return {next_state=function() %s end}", bodies[i]);
    GameplaySources sources = {.block_puzzles = source, .block_puzzles_size = strlen(source)};
    ASSERT(gameplay_load_sources(&sources));
    GameplayBlockInput in = {.state = GAMEPLAY_BLOCK_INITIALIZE};
    GameplayBlockOutput out = {.next = GAMEPLAY_BLOCK_FINISH};
    ASSERT(!gameplay_block_step(&in, &out));
    ASSERT_EQ(out.next, GAMEPLAY_BLOCK_FINISH); // no partial output on failure
    ASSERT(strstr(gameplay_error(), "block_puzzles.next_state"));
    ASSERT_EQ(gameplay_block_calls(0), 0);
    bool result = false;
    ASSERT(!gameplay_decide(0, 1, 2, &result)); ASSERT(!result); // poisoned VM stays stopped
  }
  const char *initializers[] = {"return {}", "return true", "this is not Lua",
    "while true do end", "local x='x'; for i=1,30 do x=x..x end; return {}"};
  for (size_t i = 0; i < sizeof initializers/sizeof *initializers; ++i) {
    GameplaySources sources = {.block_puzzles = initializers[i], .block_puzzles_size = strlen(initializers[i])};
    ASSERT(!gameplay_load_sources(&sources)); ASSERT(!gameplay_init());
    ASSERT(gameplay_error()[0]);
  }
  gameplay_close(); ASSERT(gameplay_init());
  GameplayBlockInput in = {.state = GAMEPLAY_BLOCK_FINISH};
  GameplayBlockOutput out = {.next = GAMEPLAY_BLOCK_FINISH};
  ASSERT(!gameplay_block_step(&in, &out)); ASSERT_EQ(out.next, GAMEPLAY_BLOCK_FINISH);
  gameplay_close(); ASSERT(gameplay_init());
  in.state = (GameplayBlockState)-1;
  ASSERT(!gameplay_block_step(&in, &out));
  gameplay_close(); ASSERT(gameplay_init());
  ASSERT(!gameplay_block_step(NULL, &out));
  gameplay_close(); ASSERT(gameplay_init());
  in.state = GAMEPLAY_BLOCK_INITIALIZE;
  ASSERT(!gameplay_block_step(&in, NULL));
  gameplay_close();
}

static void module_isolation(void) {
  const char sw[] = "shared=42; return {changed=function(a,b) return a~=b end, alternate=function(a,b) return (a&b)~=0 end}";
  const char block[] = "return {next_state=function() if shared~=nil or io~=nil or os~=nil or math~=nil or _G~=nil then return 99 end return 1 end}";
  GameplaySources sources = {.switch_tiles = sw, .switch_tiles_size = sizeof sw-1,
    .block_puzzles = block, .block_puzzles_size = sizeof block-1};
  ASSERT(gameplay_load_sources(&sources));
  GameplayBlockInput in = {.state = GAMEPLAY_BLOCK_INITIALIZE};
  GameplayBlockOutput out;
  ASSERT(gameplay_block_step(&in, &out)); ASSERT_EQ(out.next, 1);
  bool result;
  ASSERT(gameplay_decide(0, 1, 2, &result)); ASSERT(result);
  ASSERT_EQ(gameplay_block_calls(0), 1); ASSERT_EQ(gameplay_calls(0), 1);
  gameplay_close();
}

static void existing_switch_contract(void) {
  bool result;
  const char bad[] = "return {changed=function() while true do end end, alternate=function() return true end}";
  ASSERT(gameplay_load(bad, sizeof bad-1)); ASSERT(!gameplay_decide(0, 0, 0, &result));
  ASSERT(strstr(gameplay_error(), "limit"));
  const char invalid[] = "return {changed=function() return 1 end, alternate=function() return true end}";
  ASSERT(gameplay_load(invalid, sizeof invalid-1)); ASSERT(!gameplay_decide(0, 0, 0, &result));
  gameplay_close(); ASSERT(gameplay_init());
  gameplay_enable(false); ASSERT(gameplay_decide(0, 1, 2, &result)); ASSERT(result);
  GameplayBlockInput in = {.state = GAMEPLAY_BLOCK_WAIT_RELEASE, .input.release = {255, 255}};
  GameplayBlockOutput out;
  ASSERT(gameplay_block_step(&in, &out)); ASSERT_EQ(out.next, 2);
  ASSERT_EQ(gameplay_block_calls(1), 0);
  gameplay_enable(true); ASSERT(gameplay_decide(1, 1, 2, &result)); ASSERT(!result);
  gameplay_close();
}

int main(void) {
  printf("Lua scripts SHA256: %s\n", gameplay_revision());
  RUN(exhaustive_decisions);
  RUN(invalid_block_results);
  RUN(module_isolation);
  RUN(existing_switch_contract);
  puts("ok lua_gameplay: exhaustive byte inputs, typed states, bounds, memory, module isolation, C control");
}
