#ifndef PSP_CLOCK_H
#define PSP_CLOCK_H

#define CPU_CLOCK_LADDER_ARK5   0
#define CPU_CLOCK_LADDER_OTHER  1

#define CPU_CLOCK_COUNT_MAX     14
#define CPU_CLOCK_BASELINE_INDEX 3
#define CPU_CLOCK_LEGACY_OC_COUNT 11
#define CPU_CLOCK_MENU_INDEX PSP_CLOCK_222

extern u32 startup_clock_index;
extern u32 option_clock_index;
extern u32 option_clock_display_mhz;
extern u32 option_clock_ladder;

void init_cpu_clock(void);
void clock_sync_option_from_gameplay(void);
void clock_prepare_menu_entry(int from_gameplay);
void clock_menu_enter(void);
void clock_after_game_config_load(void);
void clock_menu_resume(void);
void clock_quit_cleanup(void);

u32 get_cpu_clock_count(void);
u32 get_cpu_clock_ladder(void);
void apply_cpu_clock_ladder_setting(u32 ladder);
void set_cpu_clock_ladder(u32 ladder);
void set_cpu_clock_ladder_from_menu(u32 ladder);

u32 detect_cpu_clock_index_from_hardware(void);
u32 get_cpu_clock_mhz(void);
u32 get_cpu_clock_nominal_mhz(u32 index);
u32 clamp_cpu_clock_index(u32 index);
void refresh_cpu_clock_display_from_hardware(void);
void refresh_cpu_clock_display_from_option(void);
int apply_cpu_clock_hardware(u32 index);
u32 get_current_cpu_clock_index(void);
int set_cpu_clock_index(u32 index);
int apply_cpu_clock_index(u32 index);
int set_cpu_clock(u32 psp_clock);
void quit_cpu_clock_cleanup(void);
u32 clock_mhz_for_game_config(u32 index);
u32 clock_ladder_from_game_config(u32 stored);
u32 clock_index_from_game_config(u32 stored);
u32 config_value_to_clock_index(u32 stored);

#endif
