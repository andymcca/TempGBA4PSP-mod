#include "common.h"
#include "psp_video.h"
#include "volume_icon.c"

#define VOLICON_OFFSET (GBA_SCREEN_HEIGHT + 32)
#define SLICE (64)
#define VERTEX_COUNT (2 * ((u16)(GBA_SCREEN_WIDTH / SLICE) + ((GBA_SCREEN_WIDTH % SLICE) ? 1 : 0)))

static u32 ALIGN_PSPDATA display_list[512];
static u32 ALIGN_PSPDATA display_list_0[256];

static void *disp_frame;
static void *draw_frame;

typedef struct
{
  u16 u, v;
  u16 x, y, z;
} Vertex;

typedef struct
{
  u16 color;
  u16 x, y, z;
} VertexLineCol16;

typedef struct
{
  u32 color;
  u16 x, y, z;
} VertexLineCol32;

static void set_gba_resolution(void);
static void generate_display_list(float scale_x, float scale_y);
static void bitbilt_gu(void);
static void bitbilt_sw(void);
static void *psp_vram_addr(void *frame, u32 x, u32 y);
static void load_volume_icon(int devkit_version);
static void draw_volume(int volume);

int (*__draw_volume_status)(int draw);


void init_video(int devkit_version)
{
  disp_frame = (void *)0;
  draw_frame = (void *)PSP_FRAME_SIZE;

  sceGuDisplay(GU_FALSE);

  sceGuInit();
  sceGuStart(GU_DIRECT, display_list);

  sceGuDrawBuffer(GU_PSM_5551, draw_frame, PSP_LINE_SIZE);
  sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, disp_frame, PSP_LINE_SIZE);

  sceGuOffset(2048 - (PSP_SCREEN_WIDTH / 2), 2048 - (PSP_SCREEN_HEIGHT / 2));
  sceGuViewport(2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);

  sceGuScissor(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
  sceGuEnable(GU_SCISSOR_TEST);

  sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
  sceGuTexMode(GU_PSM_5551, 0, 0, GU_FALSE);
  sceGuTexImage(0, 256, 256, GBA_LINE_SIZE, screen_texture);
  sceGuTexFlush();
  sceGuEnable(GU_TEXTURE_2D);

  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDisable(GU_BLEND);

  sceGuFinish();
  sceGuSync(0, GU_SYNC_FINISH);

  sceDisplayWaitVblankStart();
  sceGuDisplay(GU_TRUE);

  generate_display_list(1.5f, 1.5f);
  update_screen = bitbilt_gu;

  load_volume_icon(devkit_version);
}

void video_term(void)
{
  sceGuDisplay(GU_FALSE);
  sceGuTerm();
}

void flip_screen(u32 vsync)
{
  if (vsync != 0) sceDisplayWaitVblankStart();

  disp_frame = draw_frame;
  draw_frame = sceGuSwapBuffers();
}

static void *psp_vram_addr(void *frame, u32 x, u32 y)
{
  return (void *)(((u32)frame | 0x44000000) + ((x + (y << 9)) << 1));
}

static void generate_display_list(float scale_x, float scale_y)
{
  u32 i;
  s32 dx, dy;
  s32 dw, dh;
  Vertex *vertices, *vertices_tmp;

  dw = (s32)(GBA_SCREEN_WIDTH  * scale_x);
  dh = (s32)(GBA_SCREEN_HEIGHT * scale_y);

  dx = (PSP_SCREEN_WIDTH  - dw) / 2;
  dy = (PSP_SCREEN_HEIGHT - dh) / 2;

  sceGuStart(GU_CALL, display_list_0);

  sceGuClear(GU_COLOR_BUFFER_BIT | GU_FAST_CLEAR_BIT);

  vertices = (Vertex *)sceGuGetMemory(VERTEX_COUNT * sizeof(Vertex));

  if (vertices != NULL)
  {
    memset(vertices, 0, VERTEX_COUNT * sizeof(Vertex));

    vertices_tmp = vertices;

    for (i = 0; (i + SLICE) < GBA_SCREEN_WIDTH; i += SLICE)
    {
      vertices_tmp[0].u = i;
      vertices_tmp[0].v = 0;
      vertices_tmp[0].x = dx + (u16)((float)i * scale_x);
      vertices_tmp[0].y = dy;

      vertices_tmp[1].u = i + SLICE;
      vertices_tmp[1].v = GBA_SCREEN_HEIGHT;
      vertices_tmp[1].x = dx + (u16)((float)(i + SLICE) * scale_x);
      vertices_tmp[1].y = dy + dh;

      vertices_tmp += 2;
    }

    if (i < GBA_SCREEN_WIDTH)
    {
      vertices_tmp[0].u = i;
      vertices_tmp[0].v = 0;
      vertices_tmp[0].x = dx + (u16)((float)i * scale_x);
      vertices_tmp[0].y = dy;

      vertices_tmp[1].u = GBA_SCREEN_WIDTH;
      vertices_tmp[1].v = GBA_SCREEN_HEIGHT;
      vertices_tmp[1].x = dx + dw;
      vertices_tmp[1].y = dy + dh;
    }

    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, VERTEX_COUNT, 0, vertices);
  }

  sceGuFinish();
}

static void bitbilt_gu(void)
{
  sceKernelDcacheWritebackAll();

  sceGuStart(GU_DIRECT, display_list);

  sceGuCallList(display_list_0);

  sceGuFinish();
  sceGuSync(0, GU_SYNC_FINISH);
}

#define NORMAL_BLEND(c0, c1) ((c0 & c1) + (((c0 ^ c1) & 0x7bde) >> 1))

static void bitbilt_sw(void)
{
  u32 x, y;
  u16 *vptr, *vptr0;
  u16 *d, *d0;

  sceKernelDcacheWritebackAll();

  sceGuStart(GU_DIRECT, display_list);

  sceGuClear(GU_COLOR_BUFFER_BIT | GU_FAST_CLEAR_BIT);

  sceGuFinish();
  sceGuSync(0, GU_SYNC_FINISH);

  vptr0 = (u16 *)psp_vram_addr(draw_frame, 60, 16);
  d0 = screen_texture;

  for (y = 0; y < (GBA_SCREEN_HEIGHT / 2); y++)
  {
    vptr = vptr0;
    d = d0;

    for (x = 0; x < (GBA_SCREEN_WIDTH / 2); x++, d += 2)
    {
      vptr[0] = d[0];
      vptr[2] = d[1];

      vptr[0 + PSP_LINE_SIZE * 2] = d[0 + GBA_LINE_SIZE];
      vptr[2 + PSP_LINE_SIZE * 2] = d[1 + GBA_LINE_SIZE];

      *++vptr = NORMAL_BLEND(d[0], d[1]);
      vptr += (PSP_LINE_SIZE - 1);

      *vptr++ = NORMAL_BLEND(d[0], d[0 + GBA_LINE_SIZE]);
      *vptr++ = NORMAL_BLEND(NORMAL_BLEND(d[0], d[0 + GBA_LINE_SIZE]), NORMAL_BLEND(d[1], d[1 + GBA_LINE_SIZE]));
      *vptr   = NORMAL_BLEND(d[1], d[1 + GBA_LINE_SIZE]);
      vptr += (PSP_LINE_SIZE - 1);

      *vptr   = NORMAL_BLEND(d[0 + GBA_LINE_SIZE], d[1 + GBA_LINE_SIZE]);
      vptr += (2 - PSP_LINE_SIZE * 2);
    }

    vptr0 += (PSP_LINE_SIZE * 3);
    d0 += (GBA_LINE_SIZE * 2);
  }
}

void video_resolution_large(void)
{
}

void video_resolution_small(void)
{
  set_gba_resolution();

  sceGuStart(GU_DIRECT, display_list);

  sceGuClearColor(0);
  sceGuClear(GU_COLOR_BUFFER_BIT | GU_FAST_CLEAR_BIT);

  sceGuTexFilter(option_screen_filter, option_screen_filter);

  sceGuFinish();
  sceGuSync(0, GU_SYNC_FINISH);
}

static void set_gba_resolution(void)
{
  switch (option_screen_scale)
  {
    case SCALED_NONE:
      generate_display_list(1.0f, 1.0f);
      update_screen = bitbilt_gu;
      break;

    case SCALED_X15_GU:
      generate_display_list(1.5f, 1.5f);
      update_screen = bitbilt_gu;
      break;

    case SCALED_X15_SW:
      update_screen = bitbilt_sw;
      break;

    case SCALED_USER:
      generate_display_list(option_screen_mag / 100.0f, option_screen_mag / 100.0f);
      update_screen = bitbilt_gu;
      break;

    case SCALED_16X9_GU:
      generate_display_list((float)PSP_SCREEN_WIDTH / (float)GBA_SCREEN_WIDTH,
                            (float)PSP_SCREEN_HEIGHT / (float)GBA_SCREEN_HEIGHT);
      update_screen = bitbilt_gu;
      break;
  }
}

void clear_screen(u32 color)
{
  u32 r8 = (color >>  0) & 0xFF;
  u32 g8 = (color >>  8) & 0xFF;
  u32 b8 = (color >> 16) & 0xFF;
  u16 color16 = ((r8 >> 3) << 0) | ((g8 >> 3) << 5) | ((b8 >> 3) << 10) | 0x8000;

  u16 *vram_ptr = (u16 *)((u32)draw_frame | 0x44000000);
  u32 pixels = PSP_LINE_SIZE * PSP_SCREEN_HEIGHT;

  for (u32 i = 0; i < pixels; i++)
  {
    vram_ptr[i] = color16;
  }
}

void clear_texture(u16 color)
{
  u32 x, y;
  u16 *p_dest, *p_dest0;

  p_dest0 = screen_texture;

  for (y = 0; y < GBA_SCREEN_HEIGHT; y++)
  {
    p_dest = p_dest0;

    for (x = 0; x < GBA_SCREEN_WIDTH; x++, p_dest++)
      *p_dest = color;

    p_dest0 += GBA_LINE_SIZE;
  }
}

u16 *copy_screen(void)
{
  u32 x, y;
  u16 *copy;
  u16 *p_src, *p_src0;
  u16 *p_dest;

  copy = (u16 *)safe_malloc(GBA_SCREEN_SIZE);

  p_src0 = screen_texture;
  p_dest = copy;

  for (y = 0; y < GBA_SCREEN_HEIGHT; y++)
  {
    p_src = p_src0;

    for (x = 0; x < GBA_SCREEN_WIDTH; x++, p_src++, p_dest++)
      *p_dest = *p_src;

    p_src0 += GBA_LINE_SIZE;
  }

  return copy;
}

void blit_to_screen(u16 *src, u16 w, u16 h, u16 dest_x, u16 dest_y)
{
  u32 x, y;
  u16 *p_src, *p_dest, *p_dest0;

  p_src   = src;
  p_dest0 = (u16 *)psp_vram_addr(draw_frame, dest_x, dest_y);

  for (y = 0; y < h; y++)
  {
    p_dest = p_dest0;

    for (x = 0; x < w; x++, p_src++, p_dest++)
      *p_dest = *p_src;

    p_dest0 += PSP_LINE_SIZE;
  }
}

void draw_box_line(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
  VertexLineCol16 *vertices;

  sceGuStart(GU_DIRECT, display_list);

  sceGuDisable(GU_TEXTURE_2D);

  vertices = (VertexLineCol16 *)sceGuGetMemory(5 * sizeof(VertexLineCol16));

  if (vertices != NULL)
  {
    memset(vertices, 0, 5 * sizeof(VertexLineCol16));

    vertices[0].x = x1;
    vertices[0].y = y1;
    vertices[0].color = color;

    vertices[1].x = x2;
    vertices[1].y = y1;
    vertices[1].color = color;

    vertices[2].x = x2;
    vertices[2].y = y2 + 1;
    vertices[2].color = color;

    vertices[3].x = x1;
    vertices[3].y = y2;
    vertices[3].color = color;

    vertices[4].x = x1;
    vertices[4].y = y1;
    vertices[4].color = color;

    sceGuDrawArray(GU_LINE_STRIP, GU_COLOR_5551 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 5, NULL, vertices);
  }

  sceGuEnable(GU_TEXTURE_2D);

  sceGuFinish();
  sceGuSync(0, GU_SYNC_FINISH);
}

void draw_box_fill(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
  VertexLineCol16 *vertices;

  sceGuStart(GU_DIRECT, display_list);

  sceGuDisable(GU_TEXTURE_2D);

  vertices = (VertexLineCol16 *)sceGuGetMemory(4 * sizeof(VertexLineCol16));

  if (vertices != NULL)
  {
    memset(vertices, 0, 4 * sizeof(VertexLineCol16));

    vertices[0].x = x1;
    vertices[0].y = y1;
    vertices[0].color = color;

    vertices[1].x = x2 + 1;
    vertices[1].y = y1;
    vertices[1].color = color;

    vertices[2].x = x1;
    vertices[2].y = y2 + 1;
    vertices[2].color = color;

    vertices[3].x = x2 + 1;
    vertices[3].y = y2 + 1;
    vertices[3].color = color;

    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_5551 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 4, NULL, vertices);
  }

  sceGuEnable(GU_TEXTURE_2D);

  sceGuFinish();
  sceGuSync(0, GU_SYNC_FINISH);
}

void draw_box_alpha(u16 x1, u16 y1, u16 x2, u16 y2, u32 color)
{
  VertexLineCol32 *vertices;

  sceGuStart(GU_DIRECT, display_list);

  sceGuDisable(GU_TEXTURE_2D);
  sceGuEnable(GU_BLEND);

  vertices = (VertexLineCol32 *)sceGuGetMemory(4 * sizeof(VertexLineCol32));

  if (vertices != NULL)
  {
    memset(vertices, 0, 4 * sizeof(VertexLineCol32));

    vertices[0].x = x1;
    vertices[0].y = y1;
    vertices[0].color = color;

    vertices[1].x = x2 + 1;
    vertices[1].y = y1;
    vertices[1].color = color;

    vertices[2].x = x1;
    vertices[2].y = y2 + 1;
    vertices[2].color = color;

    vertices[3].x = x2 + 1;
    vertices[3].y = y2 + 1;
    vertices[3].color = color;

    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 4, NULL, vertices);
  }

  sceGuDisable(GU_BLEND);
  sceGuEnable(GU_TEXTURE_2D);

  sceGuFinish();
  sceGuSync(0, GU_SYNC_FINISH);
}

void draw_hline(u16 sx, u16 ex, u16 y, u16 color)
{
  VertexLineCol16 *vertices;

  sceGuStart(GU_DIRECT, display_list);

  sceGuDisable(GU_TEXTURE_2D);

  vertices = (VertexLineCol16 *)sceGuGetMemory(2 * sizeof(VertexLineCol16));

  if (vertices != NULL)
  {
    memset(vertices, 0, 2 * sizeof(VertexLineCol16));

    vertices[0].x = sx;
    vertices[0].y = y;
    vertices[0].color = color;

    vertices[1].x = ex + 1;
    vertices[1].y = y;
    vertices[1].color = color;

    sceGuDrawArray(GU_LINES, GU_COLOR_5551 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
  }

  sceGuEnable(GU_TEXTURE_2D);

  sceGuFinish();
  sceGuSync(0, GU_SYNC_FINISH);
}

void draw_vline(u16 x, u16 sy, u16 ey, u16 color)
{
  VertexLineCol16 *vertices;

  sceGuStart(GU_DIRECT, display_list);

  sceGuDisable(GU_TEXTURE_2D);

  vertices = (VertexLineCol16 *)sceGuGetMemory(2 * sizeof(VertexLineCol16));

  if (vertices != NULL)
  {
    memset(vertices, 0, 2 * sizeof(VertexLineCol16));

    vertices[0].x = x;
    vertices[0].y = sy;
    vertices[0].color = color;

    vertices[1].x = x;
    vertices[1].y = ey + 1;
    vertices[1].color = color;

    sceGuDrawArray(GU_LINES, GU_COLOR_5551 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
  }

  sceGuEnable(GU_TEXTURE_2D);

  sceGuFinish();
  sceGuSync(0, GU_SYNC_FINISH);
}

void print_string(const char *str, s16 x, u16 y, u16 fg_color, s16 bg_color)
{
  if (x < 0) x = (PSP_SCREEN_WIDTH - (strlen(str) * FONTWIDTH)) >> 1;

  mh_print(str, x, y, fg_color, bg_color, (u16 *)((u32)draw_frame | 0x44000000), PSP_LINE_SIZE);
}

void print_string_ext(const char *str, s16 x, u16 y, u16 fg_color, s16 bg_color, void *_dest_ptr, u16 pitch)
{
  if (x < 0) x = (pitch - (strlen(str) * FONTWIDTH)) >> 1;

  mh_print(str, x, y, fg_color, bg_color, _dest_ptr, pitch);
}

void print_string_gbk(const char *str, s16 x, u16 y, u16 fg_color, s16 bg_color)
{
  if (x < 0) x = (PSP_SCREEN_WIDTH - (strlen(str) * FONTWIDTH)) >> 1;

  ch_print(str, x, y, fg_color, bg_color, (u16 *)((u32)draw_frame | 0x44000000), PSP_LINE_SIZE);
}

void print_string_ext_gbk(const char *str, s16 x, u16 y, u16 fg_color, s16 bg_color, void *_dest_ptr, u16 pitch)
{
  if (x < 0) x = (pitch - (strlen(str) * FONTWIDTH)) >> 1;

  ch_print(str, x, y, fg_color, bg_color, _dest_ptr, pitch);
}

static void load_volume_icon(int devkit_version)
{
  if (devkit_version >= 0x03050210)
  {
    u32 x, y, alpha;
    u16 *dst, *tex_volicon;

    tex_volicon = screen_texture + (GBA_LINE_SIZE * VOLICON_OFFSET);

    dst = tex_volicon + SPEEKER_X;
    for (y = 0; y < 32; y++)
    {
      for (x = 0; x < 32; x++)
      {
        if ((x & 1) != 0)
          alpha = icon_speeker[y][(x >> 1)] >> 4;
        else
          alpha = icon_speeker[y][(x >> 1)] & 0x0f;

        dst[x] = (alpha << 12) | 0x0fff;
      }
      dst += GBA_LINE_SIZE;
    }

    dst = tex_volicon + SPEEKER_SHADOW_X;
    for (y = 0; y < 32; y++)
    {
      for (x = 0; x < 32; x++)
      {
        if ((x & 1) != 0)
          alpha = icon_speeker_shadow[y][(x >> 1)] >> 4;
        else
          alpha = icon_speeker_shadow[y][(x >> 1)] & 0x0f;

        dst[x] = alpha << 12;
      }
      dst += GBA_LINE_SIZE;
    }

    dst = tex_volicon + VOLUME_BAR_X;
    for (y = 0; y < 32; y++)
    {
      for (x = 0; x < 12; x++)
      {
        if ((x & 1) != 0)
          alpha = icon_bar[y][(x >> 1)] >> 4;
        else
          alpha = icon_bar[y][(x >> 1)] & 0x0f;

        dst[x] = (alpha << 12) | 0x0fff;
      }
      dst += GBA_LINE_SIZE;
    }

    dst = tex_volicon + VOLUME_BAR_SHADOW_X;
    for (y = 0; y < 32; y++)
    {
      for (x = 0; x < 12; x++)
      {
        if ((x & 1) != 0)
          alpha = icon_bar_shadow[y][(x >> 1)] >> 4;
        else
          alpha = icon_bar_shadow[y][(x >> 1)] & 0x0f;

        dst[x] = alpha << 12;
      }
      dst += GBA_LINE_SIZE;
    }

    dst = tex_volicon + VOLUME_DOT_X;
    for (y = 0; y < 32; y++)
    {
      for (x = 0; x < 12; x++)
      {
        if ((x & 1) != 0)
          alpha = icon_dot[y][(x >> 1)] >> 4;
        else
          alpha = icon_dot[y][(x >> 1)] & 0x0f;

        dst[x] = (alpha << 12) | 0x0fff;
      }
      dst += GBA_LINE_SIZE;
    }

    dst = tex_volicon + VOLUME_DOT_SHADOW_X;
    for (y = 0; y < 32; y++)
    {
      for (x = 0; x < 12; x++)
      {
        if ((x & 1) != 0)
          alpha = icon_dot_shadow[y][(x >> 1)] >> 4;
        else
          alpha = icon_dot_shadow[y][(x >> 1)] & 0x0f;

        dst[x] = alpha << 12;
      }
      dst += GBA_LINE_SIZE;
    }
  }
}

static void draw_volume(int volume)
{
  Vertex *vertices, *vertices_tmp;

  sceKernelDcacheWritebackAll();

  sceGuStart(GU_DIRECT, display_list);

  sceGuEnable(GU_BLEND);
  sceGuTexMode(GU_PSM_4444, 0, 0, GU_FALSE);

  vertices = (Vertex *)sceGuGetMemory(2 * 31 * 2 * sizeof(Vertex));

  if (vertices != NULL)
  {
    int i, x;

    memset(vertices, 0, 2 * 31 * 2 * sizeof(Vertex));
    vertices_tmp = vertices;

    x = 24;

    vertices_tmp[0].u = SPEEKER_SHADOW_X;
    vertices_tmp[0].v = 0 + VOLICON_OFFSET;
    vertices_tmp[0].x = 3 + x;
    vertices_tmp[0].y = 3 + 230;

    vertices_tmp[1].u = SPEEKER_SHADOW_X + 32;
    vertices_tmp[1].v = 32 + VOLICON_OFFSET;
    vertices_tmp[1].x = 3 + x + 32;
    vertices_tmp[1].y = 3 + 230 + 32;

    vertices_tmp += 2;

    vertices_tmp[0].u = SPEEKER_X;
    vertices_tmp[0].v = 0 + VOLICON_OFFSET;
    vertices_tmp[0].x = x;
    vertices_tmp[0].y = 230;

    vertices_tmp[1].u = SPEEKER_X + 32;
    vertices_tmp[1].v = 32 + VOLICON_OFFSET;
    vertices_tmp[1].x = x + 32;
    vertices_tmp[1].y = 230 + 32;

    vertices_tmp += 2;

    x = 64;

    for (i = 0; i < volume; i++)
    {
      vertices_tmp[0].u = VOLUME_BAR_SHADOW_X;
      vertices_tmp[0].v = 0 + VOLICON_OFFSET;
      vertices_tmp[0].x = 3 + x;
      vertices_tmp[0].y = 3 + 230;

      vertices_tmp[1].u = VOLUME_BAR_SHADOW_X + 12;
      vertices_tmp[1].v = 32 + VOLICON_OFFSET;
      vertices_tmp[1].x = 3 + x + 12;
      vertices_tmp[1].y = 3 + 230 + 32;

      vertices_tmp += 2;

      vertices_tmp[0].u = VOLUME_BAR_X;
      vertices_tmp[0].v = 0 + VOLICON_OFFSET;
      vertices_tmp[0].x = x;
      vertices_tmp[0].y = 230;

      vertices_tmp[1].u = VOLUME_BAR_X + 12;
      vertices_tmp[1].v = 32 + VOLICON_OFFSET;
      vertices_tmp[1].x = x + 12;
      vertices_tmp[1].y = 230 + 32;

      vertices_tmp += 2;

      x += 12;
    }

    for (; i < 30; i++)
    {
      vertices_tmp[0].u = VOLUME_DOT_SHADOW_X;
      vertices_tmp[0].v = 0 + VOLICON_OFFSET;
      vertices_tmp[0].x = 3 + x;
      vertices_tmp[0].y = 3 + 230;

      vertices_tmp[1].u = VOLUME_DOT_SHADOW_X + 12;
      vertices_tmp[1].v = 32 + VOLICON_OFFSET;
      vertices_tmp[1].x = 3 + x + 12;
      vertices_tmp[1].y = 3 + 230 + 32;

      vertices_tmp += 2;

      vertices_tmp[0].u = VOLUME_DOT_X;
      vertices_tmp[0].v = 0 + VOLICON_OFFSET;
      vertices_tmp[0].x = x;
      vertices_tmp[0].y = 230;

      vertices_tmp[1].u = VOLUME_DOT_X + 12;
      vertices_tmp[1].v = 32 + VOLICON_OFFSET;
      vertices_tmp[1].x = x + 12;
      vertices_tmp[1].y = 230 + 32;

      vertices_tmp += 2;

      x += 12;
    }

    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2 * 31 * 2, 0, vertices);
  }

  sceGuDisable(GU_BLEND);
  sceGuTexMode(GU_PSM_5551, 0, 0, GU_FALSE);

  sceGuFinish();
  sceGuSync(0, GU_SYNC_FINISH);
}

int draw_volume_status(int draw)
{
  static u64 disp_end = 0;
  int volume = kuImposeGetParam(PSP_IMPOSE_MAIN_VOLUME);

  if ((volume < 0) || (volume > 30))
    return 0;

  if (get_pad_input(PSP_CTRL_VOLUP | PSP_CTRL_VOLDOWN | PSP_CTRL_NOTE) != 0)
  {
    disp_end = ticker() + (2 * 1000 * 1000);
    draw = 1;
  }

  if (disp_end != 0)
  {
    if (ticker() < disp_end)
    {
      if (draw != 0)
        draw_volume(volume);
    }
    else
    {
      disp_end = 0;
    }
  }

  return 0;
}

int draw_volume_status_null(int draw)
{
  return 0;
}

