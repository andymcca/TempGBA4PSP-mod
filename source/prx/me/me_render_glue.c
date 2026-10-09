/* me_render_glue.c - run the "New" scanline renderer (src/video_cc.cc) on the
 * Media Engine.  Ported from gbadhoc's psp/me/me_render_glue.cc.
 *
 *   phase 1 - SNAPSHOT: copy the host's vram/oam/palette snapshot into this
 *             PRX's own arrays (cached reads, invalidate first), then publish
 *             input_seq.
 *   phase 2 - RENDER:   replay the captured lines through update_scanline()
 *             into an ME-local buffer, then write the frame to the host's
 *             staging buffer uncached, so the GE sees it with no writeback.
 *
 * The renderer object references the core's memory by name; the definitions
 * below are the ME's working copies of them.
 */
#include <psptypes.h>
#include <string.h>

#include "me_shared.h"

#define GBA_LINE_SIZE 256

u16 palette_ram [0x200];
u16 oam_ram     [0x200];
u16 io_registers[0x400];
u8  vram        [0x18000];
u32 oam_update = 1;
u32 skip_next_frame;
u32 option_oam_hijacking_enabled;
s32 affine_reference_x[2];
s32 affine_reference_y[2];

extern u16 *screen_texture;
void update_scanline(void);

static u16 me_out[GBA_LINE_SIZE * 160] __attribute__((aligned(64)));

/* Line-aligned invalidate of a main-RAM span in the ME D-cache before a
 * cached read, so a stale line is never served. */
static void me_inv(unsigned int cached_addr, unsigned int nbytes)
{
  unsigned int a   = cached_addr & ~63u;
  unsigned int end = cached_addr + nbytes;
  for (; a < end; a += 64)
    __builtin_allegrex_cache(0x19, (int)a);
}

unsigned int me_render_run(volatile me_mbox *mb, unsigned int seq)
{
  me_render_desc *d = (me_render_desc *)ME_CACHED(mb->arg0);
  const me_capture_frame *cap;
  unsigned int sum = 0;
  unsigned int log_n, use_log, bg_mode;
  int ln;

  me_inv((unsigned int)d, sizeof(*d));
  if (!d->vram || !d->oam || !d->palette || !d->capture || !d->out ||
      !d->out_pitch)
  {
    mb->input_seq = seq;
    return 0;
  }

  /* ---- phase 1: snapshot ---------------------------------------------- */
  if (d->flags & ME_DESC_VRAM_FULL)
  {
    me_inv((unsigned int)ME_CACHED(d->vram), sizeof(vram));
    memcpy(vram, ME_CACHED(d->vram), sizeof(vram));
  }
  else
  {
    const unsigned char *src = (const unsigned char *)ME_CACHED(d->vram);
    unsigned int p;
    for (p = 0; p < ME_VRAM_PAGES; p++)
    {
      unsigned int off = p << ME_VRAM_PAGE_SHIFT;
      if (d->vram_pages[p] == 0)
        continue;
      me_inv((unsigned int)(src + off), 1u << ME_VRAM_PAGE_SHIFT);
      memcpy(vram + off, src + off, 1u << ME_VRAM_PAGE_SHIFT);
    }
  }
  me_inv((unsigned int)ME_CACHED(d->oam), sizeof(oam_ram));
  memcpy(oam_ram, ME_CACHED(d->oam), sizeof(oam_ram));
  me_inv((unsigned int)ME_CACHED(d->palette), sizeof(palette_ram));
  memcpy(palette_ram, ME_CACHED(d->palette), sizeof(palette_ram));

  cap = (const me_capture_frame *)ME_CACHED(d->capture);
  me_inv((unsigned int)cap, ME_CAP_FIXED_BYTES);
  log_n = cap->log_start[160];
  if (log_n > ME_LOG_MAX)
    log_n = ME_LOG_MAX;
  me_inv((unsigned int)cap->log, log_n * sizeof(me_log_entry));
  mb->input_seq = seq;

  /* ---- phase 2: render ------------------------------------------------ */
  skip_next_frame = 0;
  option_oam_hijacking_enabled = (d->flags & ME_DESC_OAM_HIJACK) ? 1 : 0;
  screen_texture = me_out;
  /* Palette and OAM start as line 0 saw them and follow the log; without
   * a log the vcount-160 snapshot above draws the whole frame. */
  use_log = cap->log_valid;
  if (use_log)
  {
    memcpy(palette_ram, cap->palette0, sizeof(palette_ram));
    memcpy(oam_ram, cap->oam0, sizeof(oam_ram));
  }
  /* The object lists are rebuilt every frame rather than tracked across
   * frames, and again whenever OAM or the BG mode changes. */
  oam_update = 1;
  bg_mode = cap->ioregs[0][0] & 0x07;

  for (ln = 0; ln < 160; ln++)
  {
    const int *a = cap->affine[ln];

    if (use_log)
    {
      unsigned int i   = cap->log_start[ln];
      unsigned int end = cap->log_start[ln + 1];
      if (end > log_n)
        end = log_n;
      for (; i < end; i++)
      {
        unsigned int idx = cap->log[i].index;
        if (idx & ME_LOG_OAM)
        {
          oam_ram[idx & 0x1FF] = cap->log[i].value;
          oam_update = 1;
        }
        else
          palette_ram[idx & 0x1FF] = cap->log[i].value;
      }
    }

    memcpy(io_registers, cap->ioregs[ln], ME_CAP_IOREGS * sizeof(u16));
    if ((io_registers[0] & 0x07) != bg_mode)
    {
      bg_mode = io_registers[0] & 0x07;
      oam_update = 1;
    }
    affine_reference_x[0] = a[0];
    affine_reference_x[1] = a[1];
    affine_reference_y[0] = a[2];
    affine_reference_y[1] = a[3];
    update_scanline();
  }

  /* Pitched, uncached write-out of the 240 visible pixels of each row. */
  {
    const unsigned int *src = (const unsigned int *)me_out;
    unsigned int dst    = d->out | 0x40000000u;
    unsigned int pitchb = d->out_pitch * 2;
    int row;
    for (row = 0; row < 160; row++)
    {
      unsigned int *o = (unsigned int *)(dst + (unsigned int)row * pitchb);
      unsigned int w;
      for (w = 0; w < 240 / 2; w++)
      {
        o[w] = src[w];
        sum += src[w];
      }
      src += GBA_LINE_SIZE / 2;
    }
  }
  return sum;
}
