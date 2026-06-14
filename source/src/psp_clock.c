/*
 * PLL overclock support adapted from mcidclan's psp-beyond-444mhz (MIT License).
 * https://github.com/mcidclan/psp-beyond-444mhz
 */

#include "common.h"

u32 option_clock_index = 0;
u32 option_clock_display_mhz = 333;

static int ku_bridge_ready = 0;

static int ensure_ku_bridge(void)
{
  SceUID mod;
  char prx_path[MAX_PATH];
  int devkit_version;

  if (ku_bridge_ready)
    return 0;

  sprintf(prx_path, "%sku_bridge.prx", main_path);
  mod = kuKernelLoadModule(prx_path, 0, NULL);

  if (mod < 0)
    return -1;

  if (sceKernelStartModule(mod, 0, NULL, NULL, NULL) < 0)
    return -1;

  devkit_version = sceKernelDevkitVersion();
  init_ku_bridge(devkit_version);

  if (kuInitCpuClock() < 0)
    return -1;

  ku_bridge_ready = 1;
  return 0;
}

u32 get_cpu_clock_nominal_mhz(u32 index)
{
  index = clamp_cpu_clock_index(index);

  if (ku_bridge_ready)
    return kuGetCpuClockNominalMhz(index);

  return 333;
}

u32 clamp_cpu_clock_index(u32 index)
{
  if (index >= CPU_CLOCK_COUNT)
    return CPU_CLOCK_COUNT - 1;

  return index;
}

u32 get_cpu_clock_mhz(void)
{
  if (!ku_bridge_ready)
    return get_cpu_clock_nominal_mhz(option_clock_index);

  return (u32)kuGetCpuClockMhz();
}

void refresh_cpu_clock_display_from_hardware(void)
{
  option_clock_display_mhz = get_cpu_clock_mhz();
}

void init_cpu_clock(void)
{
  option_clock_display_mhz = get_cpu_clock_nominal_mhz(option_clock_index);
  ensure_ku_bridge();
  refresh_cpu_clock_display_from_hardware();
}

int apply_cpu_clock_index(u32 index)
{
  index = clamp_cpu_clock_index(index);

  if (ensure_ku_bridge() < 0)
    return -1;

  if (kuSetCpuClockIndex(index) < 0)
    return -1;

  option_clock_index = index;
  option_clock_display_mhz = get_cpu_clock_mhz();
  return 0;
}

int set_cpu_clock_index(u32 index)
{
  return apply_cpu_clock_index(index);
}

u32 config_value_to_clock_index(u32 stored)
{
  u32 i;
  u32 best = 0;
  u32 best_diff = 0xffffffff;

  if (stored < CPU_CLOCK_COUNT)
    return clamp_cpu_clock_index(stored);

  for (i = 0; i < CPU_CLOCK_COUNT; i++)
  {
    u32 nominal = get_cpu_clock_nominal_mhz(i);
    u32 diff = (stored > nominal) ? (stored - nominal) : (nominal - stored);

    if (diff < best_diff)
    {
      best_diff = diff;
      best = i;
    }
  }

  return best;
}
