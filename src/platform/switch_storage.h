#pragma once
#include <stdbool.h>
#include <stddef.h>

// Switch SD rename need not replace an existing file. Rotate a previous copy
// to .bak, install a flushed temporary, and roll back on installation failure.
// This is recoverable replacement, not a power-loss-atomic filesystem operation.
bool switch_storage_recover(const char *path);
bool switch_storage_commit(const char *temporary, const char *path);
bool switch_storage_sync(const char *path);
bool switch_storage_write(const char *path, const void *data, size_t size);
