/*
 * PLL overclock support adapted from mcidclan's psp-beyond-444mhz (MIT License).
 * https://github.com/mcidclan/psp-beyond-444mhz
 *
 * Clock flow (see plan):
 *   init_cpu_clock()              - cold start: read HW, set startup + option indices
 *   clock_menu_enter()            - pause/menu open: ramp HW to 222, keep option
 *   clock_after_game_config_load()- after load_game_config_file: ramp HW to option
 *   clock_menu_resume()           - menu close: ramp HW to option
 *   clock_quit_cleanup()          - quit: ramp HW to startup_clock_index
 *
 * Every ramp: power unlock, sync PLL index from hardware, compare actual MHz, then step.
 */

#include "common.h"

#define CPU_CLOCK_CFG_VERSION_V1      1
#define CPU_CLOCK_CFG_VERSION         2
#define CPU_CLOCK_CFG_VERSION_SHIFT   24
#define CPU_CLOCK_CFG_LADDER_SHIFT    16
#define CPU_CLOCK_MHZ_MIN           220
#define CPU_CLOCK_MHZ_MAX           520
#define CPU_CLOCK_MHZ_MATCH         2

u32 startup_clock_index = CPU_CLOCK_BASELINE_INDEX;
u32 option_clock_index = CPU_CLOCK_BASELINE_INDEX;
u32 option_clock_display_mhz = 333;
u32 option_clock_ladder = CPU_CLOCK_LADDER_ARK5;

static int ku_bridge_ready = 0;
static u32 ladder_generation = 0;

static u32 clock_index_from_nominal_mhz(u32 mhz);
static int ramp_cpu_clock_to_index(u32 index);

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

  kuSetCpuClockLadder(option_clock_ladder);

  ku_bridge_ready = 1;
  return 0;
}

void apply_cpu_clock_ladder_setting(u32 ladder)
{
  u32 prev_ladder = option_clock_ladder;

  if (ladder > CPU_CLOCK_LADDER_OTHER)
    ladder = CPU_CLOCK_LADDER_OTHER;

  option_clock_ladder = ladder;

  if (ku_bridge_ready)
    kuSetCpuClockLadder(ladder);

  if (prev_ladder != ladder)
  {
    ladder_generation++;
    option_clock_index = clamp_cpu_clock_index(option_clock_index);
    refresh_cpu_clock_display_from_option();
  }
}

void set_cpu_clock_ladder(u32 ladder)
{
  apply_cpu_clock_ladder_setting(ladder);
  option_clock_index = CPU_CLOCK_BASELINE_INDEX;
  apply_cpu_clock_index(CPU_CLOCK_BASELINE_INDEX);
}

void set_cpu_clock_ladder_from_menu(u32 ladder)
{
  apply_cpu_clock_ladder_setting(ladder);
  option_clock_index = CPU_CLOCK_BASELINE_INDEX;
  refresh_cpu_clock_display_from_option();
  ramp_cpu_clock_to_index(CPU_CLOCK_MENU_INDEX);
}

u32 get_cpu_clock_count(void)
{
  if (ku_bridge_ready)
    return kuGetCpuClockCount();

  if (option_clock_ladder == CPU_CLOCK_LADDER_OTHER)
    return CPU_CLOCK_COUNT_MAX;

  return 11;
}

u32 get_cpu_clock_ladder(void)
{
  return option_clock_ladder;
}

void prepare_for_clock_change(void)
{
  scePowerUnlock(0);

  if (ensure_ku_bridge() >= 0)
    kuSyncCpuClockFromHardware();
}

u32 clamp_cpu_clock_index(u32 index)
{
  u32 count = get_cpu_clock_count();

  if (index >= count)
    return count - 1;

  return index;
}

u32 get_cpu_clock_nominal_mhz(u32 index)
{
  index = clamp_cpu_clock_index(index);

  if (ku_bridge_ready)
    return kuGetCpuClockNominalMhz(index);

  return 333;
}

u32 get_cpu_clock_mhz(void)
{
  if (!ku_bridge_ready)
    return get_cpu_clock_nominal_mhz(option_clock_index);

  return (u32)kuGetCpuClockMhz();
}

u32 get_current_cpu_clock_index(void)
{
  if (ensure_ku_bridge() < 0)
    return option_clock_index;

  return clamp_cpu_clock_index((u32)kuSyncCpuClockFromHardware());
}

u32 detect_cpu_clock_index_from_hardware(void)
{
  u32 pll_index;
  int api_mhz;

  prepare_for_clock_change();
  pll_index = get_current_cpu_clock_index();

  api_mhz = scePowerGetCpuClockFrequencyInt();
  if (api_mhz <= 0)
    return pll_index;

  {
    u32 api_index = clock_index_from_nominal_mhz((u32)api_mhz);
    u32 pll_mhz = get_cpu_clock_nominal_mhz(pll_index);
    u32 api_nom = get_cpu_clock_nominal_mhz(api_index);
    u32 pll_diff = ((u32)api_mhz > pll_mhz) ? ((u32)api_mhz - pll_mhz) : (pll_mhz - (u32)api_mhz);
    u32 api_diff = ((u32)api_mhz > api_nom) ? ((u32)api_mhz - api_nom) : (api_nom - (u32)api_mhz);

    if (api_diff < pll_diff)
      return api_index;
  }

  return pll_index;
}

void clock_sync_option_from_gameplay(void)
{
  u32 actual_mhz;
  u32 menu_mhz;

  prepare_for_clock_change();

  if (ensure_ku_bridge() < 0)
    return;

  actual_mhz = get_cpu_clock_mhz();
  menu_mhz = get_cpu_clock_nominal_mhz(CPU_CLOCK_MENU_INDEX);

  if (actual_mhz > menu_mhz + CPU_CLOCK_MHZ_MATCH)
  {
    option_clock_index = clock_index_from_nominal_mhz(actual_mhz);
    refresh_cpu_clock_display_from_option();
  }
}

void refresh_cpu_clock_display_from_hardware(void)
{
  option_clock_display_mhz = get_cpu_clock_mhz();
}

void refresh_cpu_clock_display_from_option(void)
{
  option_clock_display_mhz = get_cpu_clock_nominal_mhz(option_clock_index);
}

static u32 clock_index_from_nominal_mhz(u32 mhz)
{
  u32 i;
  u32 best = CPU_CLOCK_BASELINE_INDEX;
  u32 best_diff = 0xffffffff;

  for (i = 0; i < get_cpu_clock_count(); i++)
  {
    u32 nominal = get_cpu_clock_nominal_mhz(i);
    u32 diff = (mhz > nominal) ? (mhz - nominal) : (nominal - mhz);

    if (diff < best_diff)
    {
      best_diff = diff;
      best = i;
    }
  }

  return best;
}

static int clock_hardware_at_index(u32 index)
{
  u32 actual_mhz;
  u32 target_mhz;
  u32 diff;

  index = clamp_cpu_clock_index(index);

  if (!ku_bridge_ready)
    return 0;

  actual_mhz = get_cpu_clock_mhz();
  target_mhz = get_cpu_clock_nominal_mhz(index);
  diff = (actual_mhz > target_mhz) ? (actual_mhz - target_mhz) : (target_mhz - actual_mhz);

  return diff <= CPU_CLOCK_MHZ_MATCH;
}

static int ramp_cpu_clock_to_index(u32 index)
{
  static u32 last_ramp_generation = 0;
  int force_ramp = (last_ramp_generation != ladder_generation);

  index = clamp_cpu_clock_index(index);

  prepare_for_clock_change();

  if (ensure_ku_bridge() < 0)
    return -1;

  last_ramp_generation = ladder_generation;

  if (!force_ramp && clock_hardware_at_index(index))
    return 0;

  return kuSetCpuClockIndex(index);
}

int apply_cpu_clock_hardware(u32 index)
{
  return ramp_cpu_clock_to_index(index);
}

void init_cpu_clock(void)
{
  startup_clock_index = detect_cpu_clock_index_from_hardware();
  option_clock_index = startup_clock_index;
  refresh_cpu_clock_display_from_option();
}

void clock_prepare_menu_entry(int from_gameplay)
{
  if (from_gameplay)
    clock_sync_option_from_gameplay();

  refresh_cpu_clock_display_from_option();
  ramp_cpu_clock_to_index(CPU_CLOCK_MENU_INDEX);
  refresh_cpu_clock_display_from_option();
}

void clock_menu_enter(void)
{
  ramp_cpu_clock_to_index(CPU_CLOCK_MENU_INDEX);
  refresh_cpu_clock_display_from_option();
}

void clock_after_game_config_load(void)
{
  ramp_cpu_clock_to_index(option_clock_index);
  refresh_cpu_clock_display_from_option();
}

void clock_menu_resume(void)
{
  ramp_cpu_clock_to_index(option_clock_index);
  refresh_cpu_clock_display_from_option();
}

void clock_quit_cleanup(void)
{
  ramp_cpu_clock_to_index(startup_clock_index);
}

int apply_cpu_clock_index(u32 index)
{
  index = clamp_cpu_clock_index(index);

  if (ramp_cpu_clock_to_index(index) < 0)
    return -1;

  option_clock_index = index;
  refresh_cpu_clock_display_from_option();
  return 0;
}

int set_cpu_clock_index(u32 index)
{
  return apply_cpu_clock_index(index);
}

int set_cpu_clock(u32 psp_clock)
{
  return apply_cpu_clock_index(clamp_cpu_clock_index(psp_clock));
}

void quit_cpu_clock_cleanup(void)
{
  clock_quit_cleanup();
}

u32 clock_mhz_for_game_config(u32 index)
{
  return (CPU_CLOCK_CFG_VERSION << CPU_CLOCK_CFG_VERSION_SHIFT) |
         ((option_clock_ladder & 1) << CPU_CLOCK_CFG_LADDER_SHIFT) |
         get_cpu_clock_nominal_mhz(index);
}

u32 clock_ladder_from_game_config(u32 stored)
{
  u32 version = stored >> CPU_CLOCK_CFG_VERSION_SHIFT;

  if (version >= CPU_CLOCK_CFG_VERSION)
    return (stored >> CPU_CLOCK_CFG_LADDER_SHIFT) & 1;

  return option_clock_ladder;
}

u32 clock_index_from_game_config(u32 stored)
{
  u32 version = stored >> CPU_CLOCK_CFG_VERSION_SHIFT;

  if (version == CPU_CLOCK_CFG_VERSION_V1 || version == CPU_CLOCK_CFG_VERSION)
    return clock_index_from_nominal_mhz(stored & 0xFFFF);

  if (stored >= CPU_CLOCK_MHZ_MIN && stored <= CPU_CLOCK_MHZ_MAX)
    return clock_index_from_nominal_mhz(stored);

  if (stored < CPU_CLOCK_LEGACY_OC_COUNT)
    return clamp_cpu_clock_index(stored + CPU_CLOCK_BASELINE_INDEX);

  return CPU_CLOCK_BASELINE_INDEX;
}

u32 config_value_to_clock_index(u32 stored)
{
  return clock_index_from_game_config(stored);
}
