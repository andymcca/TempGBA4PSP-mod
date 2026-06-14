/*
 * PLL overclock support adapted from mcidclan's psp-beyond-444mhz (MIT License).
 * https://github.com/mcidclan/psp-beyond-444mhz
 */

#include <pspsdk.h>
#include <pspkernel.h>
#include <psppower.h>

#define CPU_CLOCK_COUNT 14
#define CPU_CLOCK_BASELINE_INDEX 3
#define PLL_DEFAULT_DEN 0x12
#define PLL_BASE_FREQ 37.0f
#define PLL_DEFAULT_RATIO 1.0f
#define PLL_DEFAULT_FREQUENCY 333
#define DELAY_AFTER_CLOCK_CHANGE 300000
#define DELAY_PLL_RAMP_STEP 50000

static const u8 cpu_clock_multipliers[CPU_CLOCK_COUNT] =
{
  0x6c, 0x81, 0x92, 0xa2,
  0xab, 0xb4, 0xbd, 0xc6, 0xcf, 0xd8, 0xe1, 0xea, 0xf3, 0xfc
};

static u32 cpu_clock_nominal_mhz[CPU_CLOCK_COUNT];
static u32 applied_clock_index = CPU_CLOCK_BASELINE_INDEX;
static int memory_unlocked = 0;

#define hw(addr) (*((volatile unsigned int *)(addr)))

#define sync() asm volatile("sync \n")

#define delayPipeline() asm volatile("nop; nop; nop; nop; nop; nop; nop \n")

#define suspendCpuIntr(var) \
  asm volatile( \
    ".set push \n" ".set noreorder \n" ".set volatile \n" ".set noat \n" \
    "mfc0  %0, $12 \n" "sync \n" "li    $t0, 0xfffffffe \n" \
    "and   $t0, %0, $t0 \n" "mtc0  $t0, $12 \n" "sync \n" \
    "nop \n" "nop \n" "nop \n" ".set pop \n" \
    : "=r"(var) : : "$t0", "memory")

#define resumeCpuIntr(var) \
  asm volatile( \
    ".set push \n" ".set noreorder \n" ".set volatile \n" ".set noat \n" \
    "mtc0  %0, $12 \n" "sync \n" "nop \n" "nop \n" "nop \n" ".set pop \n" \
    : : "r"(var) : "memory")

#define settle() \
  asm volatile( \
    ".set push \n" ".set noreorder \n" ".set nomacro \n" ".set volatile \n" ".set noat \n" \
    "sync \n" "lui  $t0, 0x02 \n" "ori  $t0, $t0, 0xffff \n" \
    "1: \n" "  nop \n" "  nop \n" "  nop \n" "  nop \n" "  nop \n" "  nop \n" "  nop \n" \
    "  addiu $t0, $t0, -1 \n" "  bnez  $t0, 1b \n" "  nop \n" \
    ".set pop \n" : : : "$t0", "memory")

static void pll_ready(void)
{
  do
  {
    delayPipeline();
  }
  while (hw(0xbc100068) & 0x80);

  sync();
}

static u32 nominal_mhz_from_multiplier(u8 mul)
{
  float mhz = PLL_BASE_FREQ * (((float)mul) / (float)PLL_DEFAULT_DEN) * PLL_DEFAULT_RATIO;
  return (u32)(mhz + 0.5f);
}

static void build_nominal_table(void)
{
  u32 i;

  for (i = 0; i < CPU_CLOCK_COUNT; i++)
    cpu_clock_nominal_mhz[i] = nominal_mhz_from_multiplier(cpu_clock_multipliers[i]);
}

static void unlock_memory(void)
{
  const u32 start = 0xbc000000;
  const u32 end = 0xbc00002c;
  u32 reg;

  for (reg = start; reg <= end; reg += 4)
    hw(reg) = -1;

  sync();
  memory_unlocked = 1;
}

static void adjust_domain_ratios(void)
{
  int intr, state;
  u32 cpuDen, cpuNum, busDen, busNum;
  u32 cpu, bus;
  const int step = 18;

  state = sceKernelSuspendDispatchThread();
  suspendCpuIntr(intr);

  cpu = hw(0xbc200000);
  bus = hw(0xBC200004);
  sync();

  cpuDen = cpu & 0x1ff;
  cpuNum = (cpu >> 16) & 0x1ff;
  busDen = bus & 0x1ff;
  busNum = (bus >> 16) & 0x1ff;

  hw(0xbc200000) = (cpuNum << 16) | cpuDen;
  hw(0xBC200004) = (busNum << 16) | busDen;
  settle();

  while ((cpuNum & cpuDen & busNum & busDen) != 0x1ff)
  {
    const u32 nextCpuNum = cpuNum + step;
    const u32 nextCpuDen = cpuDen + step;
    const u32 nextBusNum = busNum + step;
    const u32 nextBusDen = busDen + step;

    cpuNum = (nextCpuNum > 0x1ff) ? 0x1ff : nextCpuNum;
    cpuDen = (nextCpuDen > 0x1ff) ? 0x1ff : nextCpuDen;
    busNum = (nextBusNum > 0x1ff) ? 0x1ff : nextBusNum;
    busDen = (nextBusDen > 0x1ff) ? 0x1ff : nextBusDen;

    hw(0xbc200000) = (cpuNum << 16) | cpuDen;
    hw(0xBC200004) = (busNum << 16) | busDen;
    settle();
  }

  resumeCpuIntr(intr);
  sceKernelResumeDispatchThread(state);
}

static u32 multiplier_to_table_index(u8 mul)
{
  u32 i;
  u32 best = CPU_CLOCK_BASELINE_INDEX;
  u32 best_diff = 0xff;

  for (i = 0; i < CPU_CLOCK_COUNT; i++)
  {
    u32 diff;

    if (mul >= cpu_clock_multipliers[i])
      diff = mul - cpu_clock_multipliers[i];
    else
      diff = cpu_clock_multipliers[i] - mul;

    if (diff < best_diff)
    {
      best_diff = diff;
      best = i;
    }
  }

  return best;
}

static u32 read_current_table_index(void)
{
  const u8 current_mul = (u8)((hw(0xbc1000fc) & 0xff00) >> 8);

  sync();
  return multiplier_to_table_index(current_mul);
}

static void write_pll_multiplier(u8 mul)
{
  int intr, state;

  state = sceKernelSuspendDispatchThread();
  suspendCpuIntr(intr);

  hw(0xbc100068) = 0x85;
  sync();
  pll_ready();
  settle();

  hw(0xbc1000fc) = (hw(0xbc1000fc) & 0xffff0000) | ((u32)mul << 8) | PLL_DEFAULT_DEN;
  sync();
  settle();

  resumeCpuIntr(intr);
  sceKernelResumeDispatchThread(state);
}

static int apply_clock_index(u32 index)
{
  u32 current_index;
  s32 step;

  if (!memory_unlocked)
    unlock_memory();

  if (index == applied_clock_index)
    return 0;

  scePowerUnlock(0);
  scePowerSetClockFrequency(PLL_DEFAULT_FREQUENCY, PLL_DEFAULT_FREQUENCY, PLL_DEFAULT_FREQUENCY / 2);
  sceKernelDelayThread(DELAY_AFTER_CLOCK_CHANGE);

  adjust_domain_ratios();

  current_index = read_current_table_index();

  if (index == current_index)
  {
    applied_clock_index = index;
    scePowerTick(0);
    return 0;
  }

  step = (index > current_index) ? 1 : -1;

  while (current_index != index)
  {
    current_index += step;
    write_pll_multiplier(cpu_clock_multipliers[current_index]);
    sceKernelDelayThread(DELAY_PLL_RAMP_STEP);
  }

  applied_clock_index = index;
  scePowerTick(0);
  return 0;
}

static int read_pll_mhz(void)
{
  const u32 pllMul = hw(0xbc1000fc) & 0xffff;
  const float n = (float)((pllMul & 0xff00) >> 8);
  const float d = (float)(pllMul & 0x00ff);
  float mhz;

  sync();

  if (d <= 0.0f)
    return PLL_DEFAULT_FREQUENCY;

  mhz = PLL_BASE_FREQ * (n / d) * PLL_DEFAULT_RATIO;
  return (int)(mhz + 0.5f);
}

int kuInitCpuClock(void)
{
  build_nominal_table();

  if (!memory_unlocked)
    unlock_memory();

  return 0;
}

int kuSetCpuClockIndex(u32 index)
{
  if (index >= CPU_CLOCK_COUNT)
    index = CPU_CLOCK_COUNT - 1;

  return apply_clock_index(index);
}

int kuGetCpuClockMhz(void)
{
  if (!memory_unlocked)
    return (int)cpu_clock_nominal_mhz[applied_clock_index];

  return read_pll_mhz();
}

u32 kuGetCpuClockNominalMhz(u32 index)
{
  if (index >= CPU_CLOCK_COUNT)
    index = CPU_CLOCK_COUNT - 1;

  return cpu_clock_nominal_mhz[index];
}
