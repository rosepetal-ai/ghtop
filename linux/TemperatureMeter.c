/*
htop - linux/TemperatureMeter.c
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "linux/TemperatureMeter.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Object.h"
#include "RichString.h"
#include "XUtils.h"

#include "linux/NvmlGpu.h"


#define HWMON_BASE "/sys/class/hwmon"

enum {
   TEMP_CPU = 0,
   TEMP_GPU,
   TEMP_PCH,
   TEMP_RAM,
   TEMP_NVME,
   TEMP_NIC,
   TEMP_COUNT
};

static const char* const TemperatureMeter_labels[TEMP_COUNT] = {
   "CPU", "GPU", "PCH", "RAM", "NVMe", "NIC"
};

static const int TemperatureMeter_attributes[TEMP_COUNT] = {
   METER_VALUE_ERROR,
   METER_VALUE_ERROR,
   METER_VALUE_WARN,
   METER_VALUE_NOTICE,
   METER_VALUE_OK,
   METER_VALUE,
};

static char* readTrim(const char* path) {
   FILE* fp = fopen(path, "r");
   if (!fp)
      return NULL;

   char buf[128];
   if (!fgets(buf, sizeof(buf), fp)) {
      fclose(fp);
      return NULL;
   }
   fclose(fp);

   size_t len = strlen(buf);
   while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == ' ' || buf[len - 1] == '\t'))
      buf[--len] = '\0';

   return xStrdup(buf);
}

static double readMilliC(const char* path) {
   char* s = readTrim(path);
   if (!s)
      return NAN;
   char* end;
   double v = strtod(s, &end);
   double r = (end != s) ? v / 1000.0 : NAN;
   free(s);
   return r;
}

/* Reads all `tempN_input` files inside `hwmon` and returns the max value
   (in °C) that falls within [min, max]. Returns NAN if no valid reading. */
static double maxHwmonTemp(const char* hwmon, double min, double max) {
   DIR* dir = opendir(hwmon);
   if (!dir)
      return NAN;

   double best = NAN;
   struct dirent* ent;
   while ((ent = readdir(dir)) != NULL) {
      const char* n = ent->d_name;
      if (strncmp(n, "temp", 4) != 0)
         continue;
      const char* suffix = strrchr(n, '_');
      if (!suffix || strcmp(suffix, "_input") != 0)
         continue;

      char path[512];
      xSnprintf(path, sizeof(path), "%s/%s", hwmon, n);
      double t = readMilliC(path);
      if (isnan(t) || t < min || t > max)
         continue;
      if (isnan(best) || t > best)
         best = t;
   }
   closedir(dir);
   return best;
}

/* CPU: read coretemp hwmon, find the entry whose `tempN_label` starts with
   "Package id" - that is the CPU package temperature reported by Intel/AMD
   coretemp. Falls back to the hottest `Core N` reading if no package entry. */
static double readCpuTemp(const char* hwmon) {
   DIR* dir = opendir(hwmon);
   if (!dir)
      return NAN;

   double pkg = NAN;
   double coreMax = NAN;
   struct dirent* ent;
   while ((ent = readdir(dir)) != NULL) {
      const char* n = ent->d_name;
      if (strncmp(n, "temp", 4) != 0)
         continue;
      const char* suffix = strrchr(n, '_');
      if (!suffix || strcmp(suffix, "_label") != 0)
         continue;

      char labelPath[512];
      xSnprintf(labelPath, sizeof(labelPath), "%s/%s", hwmon, n);
      char* label = readTrim(labelPath);
      if (!label)
         continue;

      char inputPath[512];
      size_t baseLen = (size_t)(suffix - n);
      xSnprintf(inputPath, sizeof(inputPath), "%s/%.*s_input", hwmon, (int)baseLen, n);
      double t = readMilliC(inputPath);

      if (!isnan(t)) {
         if (strstr(label, "Package id")) {
            pkg = t;
         } else if (strncmp(label, "Core", 4) == 0) {
            if (isnan(coreMax) || t > coreMax)
               coreMax = t;
         }
      }
      free(label);
   }
   closedir(dir);
   return !isnan(pkg) ? pkg : coreMax;
}

/* Returns whether the hwmon `name` matches a NIC driver we expect to expose
   temperature readings on. enp/eth covers Aquantia/Marvell-style names; the
   atlantic and igc names match upstream Intel/Aquantia drivers. */
static bool isNicHwmonName(const char* name) {
   if (!name)
      return false;
   if (strncmp(name, "enp", 3) == 0)
      return true;
   if (strncmp(name, "eth", 3) == 0)
      return true;
   if (strcmp(name, "atlantic") == 0)
      return true;
   if (strcmp(name, "igc") == 0)
      return true;
   return false;
}

static void TemperatureMeter_readAll(double out[TEMP_COUNT]) {
   for (size_t i = 0; i < TEMP_COUNT; i++)
      out[i] = NAN;

   DIR* base = opendir(HWMON_BASE);
   if (!base)
      return;

   struct dirent* ent;
   while ((ent = readdir(base)) != NULL) {
      if (ent->d_name[0] == '.')
         continue;

      char hwmon[512];
      xSnprintf(hwmon, sizeof(hwmon), "%s/%s", HWMON_BASE, ent->d_name);

      char namePath[512];
      xSnprintf(namePath, sizeof(namePath), "%s/name", hwmon);
      char* name = readTrim(namePath);
      if (!name)
         continue;

      if (strcmp(name, "coretemp") == 0) {
         double v = readCpuTemp(hwmon);
         if (!isnan(v) && (isnan(out[TEMP_CPU]) || v > out[TEMP_CPU]))
            out[TEMP_CPU] = v;
      } else if (strcmp(name, "acpitz") == 0) {
         double v = maxHwmonTemp(hwmon, -INFINITY, INFINITY);
         if (!isnan(v) && (isnan(out[TEMP_PCH]) || v > out[TEMP_PCH]))
            out[TEMP_PCH] = v;
      } else if (strcmp(name, "spd5118") == 0) {
         double v = maxHwmonTemp(hwmon, -INFINITY, INFINITY);
         if (!isnan(v) && (isnan(out[TEMP_RAM]) || v > out[TEMP_RAM]))
            out[TEMP_RAM] = v;
      } else if (strcmp(name, "nvme") == 0) {
         double v = maxHwmonTemp(hwmon, -20.0, 150.0);
         if (!isnan(v) && (isnan(out[TEMP_NVME]) || v > out[TEMP_NVME]))
            out[TEMP_NVME] = v;
      } else if (isNicHwmonName(name)) {
         double v = maxHwmonTemp(hwmon, -20.0, 150.0);
         if (!isnan(v) && (isnan(out[TEMP_NIC]) || v > out[TEMP_NIC]))
            out[TEMP_NIC] = v;
      }

      free(name);
   }
   closedir(base);

   unsigned int gpuTemp = NvmlGpu_getTemperature();
   if (gpuTemp > 0)
      out[TEMP_GPU] = (double)gpuTemp;
}

static void TemperatureMeter_updateValues(Meter* this) {
   double temps[TEMP_COUNT];
   TemperatureMeter_readAll(temps);

   double maxKnown = 0.0;
   for (size_t i = 0; i < TEMP_COUNT; i++) {
      this->values[i] = isnan(temps[i]) ? 0.0 : temps[i];
      if (!isnan(temps[i]) && temps[i] > maxKnown)
         maxKnown = temps[i];
   }
   if (this->total < maxKnown + 10.0)
      this->total = maxKnown + 10.0;

   /* Plain-text fallback used when the meter is rendered in TEXT mode by a
      caption-less consumer (e.g. legacy themes). */
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer),
             "CPU:%.0f GPU:%.0f PCH:%.0f RAM:%.0f NVMe:%.0f NIC:%.0f",
             this->values[TEMP_CPU], this->values[TEMP_GPU],
             this->values[TEMP_PCH], this->values[TEMP_RAM],
             this->values[TEMP_NVME], this->values[TEMP_NIC]);
}

static void TemperatureMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*) cast;

   for (size_t i = 0; i < TEMP_COUNT; i++) {
      char buf[32];
      int len;

      if (i > 0)
         RichString_appendAscii(out, CRT_colors[METER_TEXT], " ");

      len = xSnprintf(buf, sizeof(buf), "%s:", TemperatureMeter_labels[i]);
      RichString_appendnAscii(out, CRT_colors[METER_TEXT], buf, len);

      double v = this->values[i];
      if (v <= 0.0) {
         RichString_appendAscii(out, CRT_colors[METER_VALUE], "N/A");
         continue;
      }

      int attr = CRT_colors[METER_VALUE];
      /* Highlight values that exceed the comfort thresholds the rosepetal
         thermal script flags - matches the BAD/WARN categories there.
         GPU threshold is conservative; NVIDIA consumer GPUs throttle at
         ~83-90°C depending on model. */
      if ((i == TEMP_CPU && v >= 90.0) ||
          (i == TEMP_GPU && v >= 85.0) ||
          (i == TEMP_PCH && v >= 90.0) ||
          (i == TEMP_RAM && v >= 75.0) ||
          (i == TEMP_NVME && v >= 75.0)) {
         attr = CRT_colors[METER_VALUE_ERROR];
      }

      len = xSnprintf(buf, sizeof(buf), "%.0f", v);
      RichString_appendnAscii(out, attr, buf, len);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "C");
   }
}

const MeterClass TemperatureMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = TemperatureMeter_display,
   },
   .updateValues = TemperatureMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = TEMP_COUNT,
   .isPercentChart = false,
   .total = 100.0,
   .attributes = TemperatureMeter_attributes,
   .name = "Temperatures",
   .uiName = "Temperatures",
   .description = "Hottest hwmon temperature per category (CPU/PCH/RAM/NVMe/NIC)",
   .caption = "Temps: "
};
