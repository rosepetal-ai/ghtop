/*
htop - linux/GPUMemMeter.c
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.

VRM meter: shows aggregate NVIDIA GPU memory usage (sum across all
devices) as a bar, using NvmlGpu as the data source.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/GPUMemMeter.h"

#include "CRT.h"
#include "Macros.h"
#include "Object.h"
#include "RichString.h"

#include "linux/NvmlGpu.h"


static const int GPUMemMeter_attributes[] = {
   MEMORY_1,
};

static void GPUMemMeter_updateValues(Meter* this) {
   /* NvmlGpu reports bytes; Meter_humanUnit expects KiB. */
   double total = (double)NvmlGpu_getTotalMem() / 1024.0;
   double used  = (double)NvmlGpu_getUsedMem()  / 1024.0;

   this->total = total > 0.0 ? total : 1.0;
   this->values[0] = used;
   this->curItems = 1;

   char* buffer = this->txtBuffer;
   size_t size = sizeof(this->txtBuffer);
   int written = Meter_humanUnit(buffer, used, size);
   METER_BUFFER_CHECK(buffer, size, written);
   METER_BUFFER_APPEND_CHR(buffer, size, '/');
   Meter_humanUnit(buffer, total, size);
}

static void GPUMemMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[50];

   RichString_writeAscii(out, CRT_colors[METER_TEXT], ":");
   Meter_humanUnit(buffer, this->total, sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[METER_VALUE], buffer);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], " used:");
   Meter_humanUnit(buffer, this->values[0], sizeof(buffer));
   RichString_appendAscii(out, CRT_colors[MEMORY_1], buffer);
}

const MeterClass GPUMemMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = GPUMemMeter_display,
   },
   .updateValues = GPUMemMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 1,
   .isPercentChart = true,
   .total = 100.0,
   .attributes = GPUMemMeter_attributes,
   .name = "VRM",
   .uiName = "VRM",
   .description = "GPU memory used across all NVIDIA devices",
   .caption = "VRM"
};
