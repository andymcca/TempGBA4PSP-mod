/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2007 takka <takka@tfact.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef GUI_H
#define GUI_H


extern char dir_roms[MAX_PATH];
extern char dir_save[MAX_PATH];
extern char dir_state[MAX_PATH];
extern char dir_cfg[MAX_PATH];
extern char dir_snap[MAX_PATH];
extern char dir_cheat[MAX_PATH];

s32 load_file(const char **wildcards, char *result, char *default_dir_name, u32 show_recent);

s32 load_game_config_file(void);
s32 load_config_file(void);
s32 save_config_file(void);

s32 save_theme_config(void);
s32 load_theme_config(void);

s32 load_dir_cfg(char *file_name);

u32 menu(void);

extern u32 savestate_slot;

u32 action_loadstate(void);
u32 action_savestate(void);

/* Theme colors — defined in gui.c, accessible everywhere via common.h */
extern u16 color_bg;
extern u16 color_rom_info;
extern u16 color_active_item;
extern u16 color_inactive_item;
extern u16 color_tooltip_text;
extern u16 color_help_text;
extern u16 color_inactive_dir;
extern u16 color_scroll_bar;
extern u16 color_batt_normal;
extern u16 color_batt_low;
extern u16 color_batt_charg;

#endif /* GUI_H */