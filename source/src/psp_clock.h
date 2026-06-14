#ifndef PSP_CLOCK_H
#define PSP_CLOCK_H

#define CPU_CLOCK_COUNT 11

extern u32 option_clock_index;
extern u32 option_clock_display_mhz;

void init_cpu_clock(void);
u32 get_cpu_clock_mhz(void);
u32 get_cpu_clock_nominal_mhz(u32 index);
u32 clamp_cpu_clock_index(u32 index);
void refresh_cpu_clock_display_from_hardware(void);
int set_cpu_clock_index(u32 index);
int apply_cpu_clock_index(u32 index);
u32 config_value_to_clock_index(u32 stored);

#endif
