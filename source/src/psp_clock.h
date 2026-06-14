#ifndef PSP_CLOCK_H
#define PSP_CLOCK_H

#define PSP_CLOCK_MHZ_MIN      222
#define PSP_CLOCK_MHZ_MAX      463
#define PSP_CLOCK_MHZ_DEFAULT  333

extern u32 option_clock_mhz;
extern u32 option_clock_step;

void init_cpu_clock(void);
u32 get_cpu_clock_mhz(void);
u32 get_cpu_clock_max_mhz(void);
u32 get_cpu_clock_step_count(void);
u32 cpu_clock_mhz_from_step(u32 step);
u32 cpu_clock_step_from_mhz(u32 mhz);
u32 clamp_cpu_clock_mhz(u32 mhz);
u32 snap_cpu_clock_mhz_to_step(u32 mhz);
u32 step_cpu_clock_mhz(u32 mhz, int delta);
void option_clock_sync_step_from_mhz(void);
void option_clock_finish_config_load(void);
void sync_cpu_clock_option_from_system(void);
int apply_cpu_clock_mhz(u32 mhz);
int set_cpu_clock_mhz(u32 mhz);

#endif /* PSP_CLOCK_H */
