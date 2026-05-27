#ifndef HEADER_NvmlGpu
/*
htop - linux/NvmlGpu.h
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/
#define HEADER_NvmlGpu

#include <sys/types.h>


void NvmlGpu_init(void);

void NvmlGpu_done(void);

/* Refreshes the cached PID -> bytes mapping by querying every NVIDIA device
 * for its compute and graphics running processes. Cheap to call once per
 * sampling cycle; safe no-op if NVML failed to initialize. */
void NvmlGpu_refresh(void);

/* Returns total GPU memory (bytes) attributed to the given PID across all
 * NVIDIA devices, or 0 if the PID has no NVML-tracked usage. */
unsigned long long int NvmlGpu_getProcessMem(pid_t pid);

/* Returns the sum of total physical memory (bytes) across every NVIDIA
 * device discovered at init, or 0 if NVML is unavailable. Cached: the
 * value does not change at runtime. */
unsigned long long int NvmlGpu_getTotalMem(void);

/* Returns the highest GPU compute-engine utilization (0..100) observed
 * across all NVIDIA devices in the last NvmlGpu_refresh() cycle, or 0
 * if NVML is unavailable or no device responded. */
unsigned int NvmlGpu_getUtilization(void);

/* Returns the highest GPU die temperature in °C across all NVIDIA
 * devices in the last NvmlGpu_refresh() cycle, or 0 if NVML is
 * unavailable or no device responded. */
unsigned int NvmlGpu_getTemperature(void);

/* Returns the sum of currently-used physical GPU memory (bytes) across
 * every NVIDIA device, refreshed each NvmlGpu_refresh() cycle. 0 if
 * NVML is unavailable. */
unsigned long long int NvmlGpu_getUsedMem(void);

#endif /* HEADER_NvmlGpu */
