#include "common.h"



u32 option_clock_mhz = PSP_CLOCK_MHZ_DEFAULT;



typedef void (*SctrlHENSetSpeedFunc)(int cpu, int bus);

typedef u32 (*SctrlHENGetSpeedFunc)(void);



static SctrlHENSetSpeedFunc sctrlHENSetSpeed_ptr = NULL;

static SctrlHENGetSpeedFunc sctrlHENGetSpeed_ptr = NULL;

static int (*scePowerSetClockFrequency_ptr)(int pllfreq, int cpufreq, int busfreq) = NULL;

static u32 cpu_clock_max_mhz = PSP_CLOCK_MHZ_DEFAULT;



static const u32 standard_clock_steps[] =

{

  222, 266, 300, 333

};



extern int sctrlHENGetVersion(void);

extern void sctrlHENSetSpeed(int cpu, int bus);

extern u32 sctrlHENGetSpeed(void);



static int bus_clock_for_cpu(int cpu)

{

  if (cpu >= 333)

    return cpu / 2;



  if (cpu >= 300)

    return 150;



  if (cpu >= 266)

    return 133;



  if (cpu >= 222)

    return 111;



  return cpu / 2;

}



static int set_standard_cpu_clock(int cpu, int bus)

{

  if (scePowerSetClockFrequency_ptr == NULL)

    return -1;



  return scePowerSetClockFrequency_ptr(cpu, cpu, bus);

}



static u32 snap_standard_cpu_mhz(u32 mhz)

{

  if (mhz >= 317)

    return 333;



  if (mhz >= 283)

    return 300;



  if (mhz >= 244)

    return 266;



  return 222;

}



static u32 nearest_clock_step(const u32 *steps, u32 count, u32 mhz)

{

  u32 best = steps[0];

  u32 best_dist = (mhz > best) ? (mhz - best) : (best - mhz);

  u32 i;



  for (i = 1; i < count; i++)

  {

    u32 dist = (mhz > steps[i]) ? (mhz - steps[i]) : (steps[i] - mhz);



    if (dist < best_dist)

    {

      best = steps[i];

      best_dist = dist;

    }

  }



  return best;

}



static u32 find_clock_step_index(const u32 *steps, u32 count, u32 mhz)

{

  u32 i;



  for (i = 0; i < count; i++)

  {

    if (steps[i] == mhz)

      return i;

  }



  return 0;

}



static void resolve_cfw_clock_exports(void)

{

  if (sctrlHENGetVersion() < 0)

    return;



  sctrlHENSetSpeed_ptr = sctrlHENSetSpeed;

  sctrlHENGetSpeed_ptr = sctrlHENGetSpeed;

  cpu_clock_max_mhz = PSP_CLOCK_MHZ_MAX;

}



u32 clamp_cpu_clock_mhz(u32 mhz)

{

  if (mhz < PSP_CLOCK_MHZ_MIN)

    return PSP_CLOCK_MHZ_MIN;



  if (mhz > cpu_clock_max_mhz)

    return cpu_clock_max_mhz;



  return mhz;

}



u32 get_cpu_clock_max_mhz(void)

{

  return cpu_clock_max_mhz;

}



u32 snap_cpu_clock_mhz_to_step(u32 mhz)

{

  mhz = clamp_cpu_clock_mhz(mhz);



  if (sctrlHENSetSpeed_ptr != NULL)

  {

    static const u32 all_steps[] =

    {

      222, 266, 300, 333, 352, 370, 389, 407, 426, 444, 463

    };



    return nearest_clock_step(all_steps,

                              sizeof(all_steps) / sizeof(all_steps[0]),

                              mhz);

  }



  return snap_standard_cpu_mhz(mhz);

}



u32 step_cpu_clock_mhz(u32 mhz, int delta)

{

  u32 count;

  const u32 *steps;

  u32 index;



  mhz = snap_cpu_clock_mhz_to_step(mhz);



  if (sctrlHENSetSpeed_ptr != NULL)

  {

    static const u32 all_steps[] =

    {

      222, 266, 300, 333, 352, 370, 389, 407, 426, 444, 463

    };



    steps = all_steps;

    count = sizeof(all_steps) / sizeof(all_steps[0]);

  }

  else

  {

    steps = standard_clock_steps;

    count = sizeof(standard_clock_steps) / sizeof(standard_clock_steps[0]);

  }



  index = find_clock_step_index(steps, count, mhz);



  if (delta > 0)

    index = (index + 1) % count;

  else if (delta < 0)

  {

    if (index == 0)

      index = count - 1;

    else

      index--;

  }



  return steps[index];

}



u32 get_cpu_clock_mhz(void)

{

  if (sctrlHENGetSpeed_ptr != NULL)

  {

    u32 speed = sctrlHENGetSpeed_ptr();



    if (speed >= PSP_CLOCK_MHZ_MIN && speed <= PSP_CLOCK_MHZ_MAX)

      return snap_cpu_clock_mhz_to_step(speed);

  }



  return snap_cpu_clock_mhz_to_step((u32)scePowerGetCpuClockFrequencyInt());

}



void sync_cpu_clock_option_from_system(void)

{

  u32 actual = get_cpu_clock_mhz();



  if (actual > option_clock_mhz)

    option_clock_mhz = actual;

}



void init_cpu_clock(void)

{

  int devkit_version = sceKernelDevkitVersion();



  if (devkit_version < 0x05000010)

    scePowerSetClockFrequency_ptr = scePowerSetClockFrequency;

  else

    scePowerSetClockFrequency_ptr = scePower_EBD177D6;



  resolve_cfw_clock_exports();

  option_clock_mhz = snap_cpu_clock_mhz_to_step(option_clock_mhz);

}



int apply_cpu_clock_mhz(u32 mhz)

{

  int bus;

  int ret = -1;

  u32 target = snap_cpu_clock_mhz_to_step(mhz);



  bus = bus_clock_for_cpu((int)target);



  if (sctrlHENSetSpeed_ptr != NULL)

  {

    sctrlHENSetSpeed_ptr((int)target, bus);

    ret = 0;

  }

  else

  {

    switch (target)

    {

      case 333:

        ret = set_standard_cpu_clock(333, 166);

        break;



      case 300:

        ret = set_standard_cpu_clock(300, 150);

        break;



      case 266:

        ret = set_standard_cpu_clock(266, 133);

        break;



      default:

      case 222:

        ret = set_standard_cpu_clock(222, 111);

        break;

    }

  }



  return ret;

}



int set_cpu_clock_mhz(u32 mhz)

{

  u32 target = snap_cpu_clock_mhz_to_step(mhz);

  int ret = apply_cpu_clock_mhz(target);



  if (ret == 0)

    option_clock_mhz = snap_cpu_clock_mhz_to_step(get_cpu_clock_mhz());



  return ret;

}


