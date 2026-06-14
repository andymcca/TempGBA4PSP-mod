#ifndef PSP_CLOCK_H
#define PSP_CLOCK_H

#define PSP_CLOCK_MHZ_MIN      222
#define PSP_CLOCK_MHZ_MAX      463
#define PSP_CLOCK_MHZ_DEFAULT  333

extern u32 option_clock_mhz;

void init_cpu_clock(void);
u32 get_cpu_clock_mhz(void);
u32 get_cpu_clock_max_mhz(void);
u32 clamp_cpu_clock_mhz(u32 mhz);
u32 snap_cpu_clock_mhz_to_step(u32 mhz);
u32 step_cpu_clock_mhz(u32 mhz, int delta);
void sync_cpu_clock_option_from_system(void);
int apply_cpu_clock_mhz(u32 mhz);
int set_cpu_clock_mhz(u32 mhz);

#endif /* PSP_CLOCK_H */
