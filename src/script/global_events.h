#pragma once
#include "script/npcs_events.h"
// Global cutscenes are not owned by an interaction/fiber-local actor.
#undef EVENT_ACTIVE
#define EVENT_ACTIVE NPC_ACTIVE
#ifdef ORACLES_LUA_GAMEPLAY
#undef STORY_NOTE
#define STORY_NOTE() ((void)0)
#undef NPC_CALL_C
#define NPC_CALL_C(at,fn,target,ret) do { \
  if(NPC_ACTIVE && ((fn)==showText_hook || (fn)==showTextNonExitable_hook)) {EVENT_DIALOGUE();} \
  CALL_C(at,fn,target,ret);NPC_FAILED_RETURN(); \
}while(0)
#endif
