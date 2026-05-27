/*
htop - linux/DockerNames.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/DockerNames.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "XUtils.h"


#define DOCKER_REFRESH_MS 5000


typedef struct DockerName_ {
   char id[13];   /* 12-char container id prefix + NUL */
   char* name;
   struct DockerName_* next;
} DockerName;

static DockerName* cache = NULL;
static uint64_t lastRefreshMs = 0;
static bool firstRefresh = true;
static bool dockerAvailable = true;

static void DockerNames_clear(void) {
   while (cache) {
      DockerName* next = cache->next;
      free(cache->name);
      free(cache);
      cache = next;
   }
}

void DockerNames_refresh(uint64_t monotonicMs) {
   if (!dockerAvailable)
      return;
   if (!firstRefresh && monotonicMs - lastRefreshMs < DOCKER_REFRESH_MS)
      return;

   lastRefreshMs = monotonicMs;
   firstRefresh = false;

   FILE* fp = popen("docker ps --no-trunc --format '{{.ID}}\t{{.Names}}' 2>/dev/null", "r");
   if (!fp) {
      dockerAvailable = false;
      return;
   }

   DockerName* fresh = NULL;
   char line[1024];
   while (fgets(line, sizeof(line), fp)) {
      char* tab = strchr(line, '\t');
      if (!tab)
         continue;

      size_t idLen = (size_t)(tab - line);
      if (idLen < 12)
         continue;

      *tab = '\0';
      char* name = tab + 1;
      char* nl = strchr(name, '\n');
      if (nl)
         *nl = '\0';
      if (!*name)
         continue;

      DockerName* dn = xMalloc(sizeof(DockerName));
      memcpy(dn->id, line, 12);
      dn->id[12] = '\0';
      dn->name = xStrdup(name);
      dn->next = fresh;
      fresh = dn;
   }

   int rc = pclose(fp);
   if (rc == -1 || (WIFEXITED(rc) && WEXITSTATUS(rc) == 127)) {
      /* docker binary not found or pclose failed - stop trying */
      dockerAvailable = false;
      while (fresh) {
         DockerName* next = fresh->next;
         free(fresh->name);
         free(fresh);
         fresh = next;
      }
      return;
   }

   DockerNames_clear();
   cache = fresh;
}

const char* DockerNames_lookup(const char* containerId) {
   if (!containerId)
      return NULL;
   if (strlen(containerId) < 12)
      return NULL;

   for (const DockerName* dn = cache; dn; dn = dn->next) {
      if (memcmp(dn->id, containerId, 12) == 0)
         return dn->name;
   }
   return NULL;
}

void DockerNames_done(void) {
   DockerNames_clear();
   lastRefreshMs = 0;
   firstRefresh = true;
   dockerAvailable = true;
}
