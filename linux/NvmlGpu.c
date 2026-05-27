/*
htop - linux/NvmlGpu.c
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

Per-process GPU memory accounting via NVIDIA Management Library (NVML).
The proprietary NVIDIA driver does not populate drm-memory-* in fdinfo, so
linux/GPU.c sees nothing for NVIDIA processes. We bridge that gap by
querying NVML directly.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/NvmlGpu.h"

#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "XUtils.h"


/* Minimal subset of NVML decls we need; matches libnvidia-ml ABI.
 * Avoids a hard dependency on the nvml.h header from the CUDA SDK. */
typedef struct nvmlDevice_st* nvmlDevice_t;
typedef int nvmlReturn_t;
#define NVML_SUCCESS 0

/* Layout used by the *_v3 process query symbols (NVML 11.0+). */
typedef struct {
   unsigned int       pid;
   unsigned long long usedGpuMemory;
   unsigned int       gpuInstanceId;
   unsigned int       computeInstanceId;
} nvmlProcessInfo_t;

typedef struct {
   unsigned long long total;
   unsigned long long free;
   unsigned long long used;
} nvmlMemory_t;

extern nvmlReturn_t nvmlInit_v2(void);
extern nvmlReturn_t nvmlShutdown(void);
extern nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int* deviceCount);
extern nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(unsigned int index, nvmlDevice_t* device);
extern nvmlReturn_t nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t* memory);
extern nvmlReturn_t nvmlDeviceGetComputeRunningProcesses_v3(nvmlDevice_t device, unsigned int* infoCount, nvmlProcessInfo_t* infos);
extern nvmlReturn_t nvmlDeviceGetGraphicsRunningProcesses_v3(nvmlDevice_t device, unsigned int* infoCount, nvmlProcessInfo_t* infos);


static bool initialized = false;
static unsigned int deviceCount = 0;
static unsigned long long int totalMem = 0;
static Hashtable* pidMem = NULL;  /* pid -> unsigned long long* (owned) */


void NvmlGpu_init(void) {
   if (initialized)
      return;

   if (nvmlInit_v2() != NVML_SUCCESS)
      return;

   if (nvmlDeviceGetCount_v2(&deviceCount) != NVML_SUCCESS) {
      nvmlShutdown();
      return;
   }

   for (unsigned int i = 0; i < deviceCount; i++) {
      nvmlDevice_t dev;
      if (nvmlDeviceGetHandleByIndex_v2(i, &dev) != NVML_SUCCESS)
         continue;

      nvmlMemory_t mem;
      if (nvmlDeviceGetMemoryInfo(dev, &mem) == NVML_SUCCESS)
         totalMem += mem.total;
   }

   pidMem = Hashtable_new(64, /* owner */ true);
   initialized = true;
}

void NvmlGpu_done(void) {
   if (!initialized)
      return;

   Hashtable_delete(pidMem);
   pidMem = NULL;
   nvmlShutdown();
   deviceCount = 0;
   totalMem = 0;
   initialized = false;
}

static void accumulate(const nvmlProcessInfo_t* infos, unsigned int count) {
   for (unsigned int i = 0; i < count; i++) {
      /* NVML reports NVML_VALUE_NOT_AVAILABLE (~0ULL) when accounting is off
       * for a process; skip those instead of poisoning the total. */
      if (infos[i].usedGpuMemory == (unsigned long long)-1)
         continue;

      ht_key_t key = (ht_key_t)infos[i].pid;
      unsigned long long* slot = Hashtable_get(pidMem, key);
      if (slot) {
         *slot += infos[i].usedGpuMemory;
      } else {
         slot = xMalloc(sizeof(*slot));
         *slot = infos[i].usedGpuMemory;
         Hashtable_put(pidMem, key, slot);
      }
   }
}

static void queryDevice(nvmlDevice_t dev,
                        nvmlReturn_t (*fn)(nvmlDevice_t, unsigned int*, nvmlProcessInfo_t*)) {
   unsigned int count = 0;
   nvmlReturn_t r = fn(dev, &count, NULL);
   /* NVML returns INSUFFICIENT_SIZE (15) when probing with NULL; count is set. */
   if (r == NVML_SUCCESS && count == 0)
      return;
   if (count == 0)
      return;

   nvmlProcessInfo_t* infos = xMallocArray(count, sizeof(*infos));
   if (fn(dev, &count, infos) == NVML_SUCCESS)
      accumulate(infos, count);

   free(infos);
}

void NvmlGpu_refresh(void) {
   if (!initialized)
      return;

   Hashtable_clear(pidMem);

   for (unsigned int i = 0; i < deviceCount; i++) {
      nvmlDevice_t dev;
      if (nvmlDeviceGetHandleByIndex_v2(i, &dev) != NVML_SUCCESS)
         continue;

      queryDevice(dev, nvmlDeviceGetComputeRunningProcesses_v3);
      queryDevice(dev, nvmlDeviceGetGraphicsRunningProcesses_v3);
   }
}

unsigned long long int NvmlGpu_getProcessMem(pid_t pid) {
   if (!initialized)
      return 0;

   unsigned long long* slot = Hashtable_get(pidMem, (ht_key_t)pid);
   return slot ? *slot : 0;
}

unsigned long long int NvmlGpu_getTotalMem(void) {
   return totalMem;
}
