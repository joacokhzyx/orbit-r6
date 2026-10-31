#ifndef R6_TARGET_H
#define R6_TARGET_H

/* The host selects the only target R6 builds: Win64 objects on Windows and
 * System V x86-64 objects elsewhere. There is no cross-target flag. */
#if defined(_WIN32)
#define R6_TARGET_WIN64 1
#else
#define R6_TARGET_WIN64 0
#endif

#endif
