#ifndef HEADER_DockerNames
#define HEADER_DockerNames
/*
htop - linux/DockerNames.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdint.h>


/* Refreshes the docker container id->name cache by invoking `docker ps`.
   No-op if the previous refresh was less than 5s ago or docker is unavailable. */
void DockerNames_refresh(uint64_t monotonicMs);

/* Looks up the container name for the given full or 12-char-prefix container id.
   Returns NULL if not found. The returned pointer is owned by the cache and
   becomes invalid on the next refresh - callers must xStrdup if they need to keep it. */
const char* DockerNames_lookup(const char* containerId);

/* Releases all memory held by the cache. */
void DockerNames_done(void);

#endif /* HEADER_DockerNames */
