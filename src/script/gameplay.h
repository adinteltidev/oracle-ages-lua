#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Stateless pilot; no VM pointer or new persistent data is added to GB.
bool gameplay_init(void);
void gameplay_enable(bool enabled);
bool gameplay_decide(int rule, uint8_t a, uint8_t b, bool *out);
const char *gameplay_error(void);
uint64_t gameplay_calls(int rule);
size_t gameplay_memory(void);
void gameplay_close(void);
// Used for bounded-runtime regression tests; production always loads embedded source.
bool gameplay_load(const char *source, size_t length);
