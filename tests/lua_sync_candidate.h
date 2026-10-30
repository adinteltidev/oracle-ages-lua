#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { SYNC_AWAKE, SYNC_ELIGIBLE_TILE } SyncCandidateRule;
bool sync_candidate_load(void);
bool sync_candidate_query(SyncCandidateRule rule, uint8_t value, bool *out);
const char *sync_candidate_revision(void);
