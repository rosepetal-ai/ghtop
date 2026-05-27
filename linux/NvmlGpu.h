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

#endif /* HEADER_NvmlGpu */
