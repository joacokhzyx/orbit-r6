#ifndef R6_PASS_CACHE_H
#define R6_PASS_CACHE_H

#include "../codegen-x64/win64.h"

typedef bool (*R6Passes)(TextBuf *text, void *context);
typedef struct { size_t hits, misses; } R6CacheStats;

/* A local, disposable cache. Unreadable or invalid entries are misses. */
bool r6_cached_passes(TextBuf *text, const R6Module *module, const R6FunctionSpan *spans,
                      const char *path, const char *version, R6Passes passes,
                      void *context, R6CacheStats *stats);

#endif
