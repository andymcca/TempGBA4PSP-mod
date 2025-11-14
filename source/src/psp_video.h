#ifndef PSP_VIDEO_H
#define PSP_VIDEO_H

#include <psptypes.h>

extern u16 *screen_texture;
extern int (*__draw_volume_status)(int draw);

void init_video(int devkit_version);
void video_term(void);

void flip_screen(u32 vsync);

void video_resolution_large(void);
void video_resolution_small(void);

void clear_screen(u32 color);
void clear_texture(u16 color);

u16 *copy_screen(void);
void blit_to_screen(u16 *src, u16 w, u16 h, u16 dest_x, u16 dest_y);

void print_string(const char *str, s16 x, u16 y, u16 fg_color, s16 bg_color);
void print_string_ext(const char *str, s16 x, u16 y, u16 fg_color, s16 bg_color, void *_dest_ptr, u16 pitch);
void print_string_gbk(const char *str, s16 x, u16 y, u16 fg_color, s16 bg_color);
void print_string_ext_gbk(const char *str, s16 x, u16 y, u16 fg_color, s16 bg_color, void *_dest_ptr, u16 pitch);

void draw_box_line(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void draw_box_fill(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void draw_box_alpha(u16 x1, u16 y1, u16 x2, u16 y2, u32 color);
void draw_hline(u16 sx, u16 ex, u16 y, u16 color);
void draw_vline(u16 x, u16 sy, u16 ey, u16 color);

int draw_volume_status(int draw);
int draw_volume_status_null(int draw);

#endif /* PSP_VIDEO_H */

