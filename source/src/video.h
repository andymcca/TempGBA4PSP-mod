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

#ifndef VIDEO_H
#define VIDEO_H

#include <psptypes.h>


#define GBA_SCREEN_WIDTH  (240)
#define GBA_SCREEN_HEIGHT (160)
#define GBA_LINE_SIZE     (256)
#define GBA_SCREEN_SIZE   (GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * 2)

#define PSP_SCREEN_WIDTH  (480)
#define PSP_SCREEN_HEIGHT (272)
#define PSP_LINE_SIZE     (512)
#define PSP_FRAME_SIZE    (PSP_LINE_SIZE * PSP_SCREEN_HEIGHT * 2)


#define COLOR15(red, green, blue)  (((blue) << 10) | ((green) << 5) | (red))

#define COLOR15_WHITE   COLOR15(31, 31, 31)
#define COLOR15_BLACK   COLOR15( 0,  0,  0)
#define COLOR15_RED     COLOR15(31,  5,  5)
#define COLOR15_GREEN   COLOR15( 5, 31,  5)
#define COLOR15_BLUE    COLOR15( 5,  5, 31)
#define COLOR15_YELLOW  COLOR15(31, 31,  5)
#define COLOR15_PURPLE  COLOR15(31,  5, 31)
#define COLOR15_CYAN    COLOR15( 5, 31, 31)

#define COL15_GET_R5(col15) ((col15) & 0x1F)
#define COL15_GET_G5(col15) (((col15) >> 5) & 0x1F)
#define COL15_GET_B5(col15) (((col15) >>10) & 0x1F)

#define COL15_GET_R8(col15) ((((col15) << 3) & 0xF8) | (((col15) >>  2) & 0x07))
#define COL15_GET_G8(col15) ((((col15) >> 2) & 0xF8) | (((col15) >>  7) & 0x07))
#define COL15_GET_B8(col15) ((((col15) >> 7) & 0xF8) | (((col15) >> 12) & 0x07))

#define COLOR32(red, green, blue)  (((blue) << 16) | ((green) << 8) | (red) | 0xFF000000)

#define COLOR32_WHITE  COLOR32(255, 255, 255)
#define COLOR32_BLACK  COLOR32(  0,   0,   0)

#define COL32_GET_R8(col32) ((col32) & 0xFF)
#define COL32_GET_G8(col32) (((col32) >>  8) & 0xFF)
#define COL32_GET_B8(col32) (((col32) >> 16) & 0xFF)

#define COLOR15_TO_32(col15)  COLOR32(COL15_GET_R8(col15), COL15_GET_G8(col15), COL15_GET_B8(col15))

#define X_POS_CENTER  (-1)


void update_scanline(void);
extern void (*update_screen)(void);

extern s32 affine_reference_x[2];
extern s32 affine_reference_y[2];

typedef void (*tile_render_function)(u32 layer_number, u32 start, u32 end, void *dest_ptr);
typedef void (*bitmap_render_function)(u32 start, u32 end, void *dest_ptr);

typedef struct
{
  tile_render_function normal_render_base;
  tile_render_function normal_render_transparent;
  tile_render_function alpha_render_base;
  tile_render_function alpha_render_transparent;
  tile_render_function color16_render_base;
  tile_render_function color16_render_transparent;
  tile_render_function color32_render_base;
  tile_render_function color32_render_transparent;
} TileLayerRenderStruct;

typedef struct
{
  bitmap_render_function normal_render;
} BitmapLayerRenderStruct;


void video_write_mem_savestate(SceUID savestate_file);
void video_read_savestate(SceUID savestate_file);


#endif /* VIDEO_H */
