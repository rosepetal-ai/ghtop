#ifndef HEADER_TemperatureMeter
#define HEADER_TemperatureMeter
/*
htop - linux/TemperatureMeter.h
(C) 2026 htop dev team
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"


/* Single-line text meter showing the hottest hwmon temperature in each of
   five categories: CPU (coretemp Package id), PCH (acpitz), RAM (spd5118),
   NVMe and NIC (enp/eth/atlantic/igc). Mirrors the readings emitted by
   the rosepetal `thermal` script. */
extern const MeterClass TemperatureMeter_class;

#endif /* HEADER_TemperatureMeter */
