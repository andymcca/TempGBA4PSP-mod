/* me_video.c - host side of the "New (ME)" video renderer (see me_video.h).
 *
 * Ported from gbadhoc's psp/me_host.c and the me_rend_* pipeline in its
 * psp/main_psp.c.  The frame pipeline, all at vcount 160:
 *
 *   1. retire: the render posted one frame ago should be done; spin briefly
 *      for a late one, otherwise keep showing the previous frame.
 *   2. post:   copy vram/oam/palette into a snapshot the ME reads (so
 *              emulation resumes at once instead of waiting for the ME to
 *              copy the live arrays) and hand it this frame's capture.
 *   3. present the most recent finished frame.
 *
 * So the picture runs one frame behind the CPU renderer.  The ME writes its
 * output to two staging buffers in main RAM, uncached, and the GE textures
 * straight from them; this CPU only ever reads them after an invalidate.
 */
#include "common.h"
#include "me_video.h"

#define ME_STAGE_ROWS   (GBA_SCREEN_HEIGHT + 2)
#define ME_STAGE_BYTES  (GBA_LINE_SIZE * ME_STAGE_ROWS * 2)
#define ME_SNAP_VRAM    0x18000
#define ME_SNAP_OAM     0x400
#define ME_SNAP_PAL     0x400
#define ME_SNAP_BYTES   (ME_SNAP_VRAM + ME_SNAP_OAM + ME_SNAP_PAL)
#define ME_CAP_BYTES    ((sizeof(me_capture_frame) + 63) & ~63)

#define ME_BOOT_TRIES   50        /* x 5 ms handshake */
#define ME_RETIRE_US    9000      /* spin budget for a late render */
#define ME_LATE_LIMIT   120       /* consecutive late frames -> give up */
#define ME_STALE_LIMIT  30        /* frames with a frozen heartbeat -> dead */
#define ME_DRAIN_US     50000

me_capture_frame *me_capture_buf = NULL;

static me_mbox __attribute__((aligned(64))) mbox_storage;
static volatile me_mbox *mb;
static SceUID me_modid = -1;

/* The PRX did not come up, or the ME stopped answering: render on the CPU
 * for the rest of the session rather than retrying every frame. */
static u32 me_unavailable;
static u32 me_active;

static me_capture_frame *cap[2];
static u16 *stage[2];
static u8 *snap;
static me_render_desc *desc;

static s32 cap_cur;
static s32 stage_pending = -1;
static s32 stage_ready = -1;
static u32 late_frames;
static u32 beat_last;
static u32 beat_stale;

static void me_unload(void)
{
  if (me_modid >= 0)
  {
    int status;
    sceKernelStopModule(me_modid, 0, NULL, &status, NULL);
    sceKernelUnloadModule(me_modid);
    me_modid = -1;
  }
  mb = NULL;
}

static s32 me_load(void)
{
  char path[MAX_PATH];
  u32 arg[2];
  SceUID mod;
  s32 i;

  memset(&mbox_storage, 0, sizeof(mbox_storage));
  sceKernelDcacheWritebackInvalidateRange(&mbox_storage, sizeof(mbox_storage));

  sprintf(path, "%stempgba_me.prx", main_path);
  mod = kuKernelLoadModule(path, 0, NULL);
  if (mod < 0)
    return -1;

  arg[0] = (u32)ME_UNCACHED(&mbox_storage);
  arg[1] = 0;
  if (sceKernelStartModule(mod, sizeof(arg), arg, NULL, NULL) < 0)
  {
    sceKernelUnloadModule(mod);
    return -1;
  }
  me_modid = mod;

  for (i = 0; i < ME_BOOT_TRIES; i++)
  {
    volatile me_mbox *m = (volatile me_mbox *)ME_UNCACHED(&mbox_storage);

    if (m->magic == ME_MBOX_MAGIC && m->version == ME_MBOX_VERSION)
    {
      mb = m;
      return 0;
    }
    sceKernelDelayThread(5000);
  }

  /* module_start already patched the ME reset vector; undo it. */
  me_unload();
  return -1;
}

static s32 me_alloc(void)
{
  s32 i;

  for (i = 0; i < 2; i++)
  {
    cap[i]   = (me_capture_frame *)memalign(64, ME_CAP_BYTES);
    stage[i] = (u16 *)memalign(64, ME_STAGE_BYTES);
  }
  snap = (u8 *)memalign(64, ME_SNAP_BYTES);
  desc = (me_render_desc *)memalign(64, 64);

  if (!cap[0] || !cap[1] || !stage[0] || !stage[1] || !snap || !desc)
  {
    for (i = 0; i < 2; i++)
    {
      free(cap[i]);   cap[i]   = NULL;
      free(stage[i]); stage[i] = NULL;
    }
    free(snap); snap = NULL;
    free(desc); desc = NULL;
    return -1;
  }

  /* The ME writes the stages uncached: no dirty line of ours may ever be
   * written back over them. */
  for (i = 0; i < 2; i++)
  {
    memset(stage[i], 0, ME_STAGE_BYTES);
    sceKernelDcacheWritebackInvalidateRange(stage[i], ME_STAGE_BYTES);
  }
  return 0;
}

static u32 me_idle(void)
{
  return mb->done_seq == mb->cmd_seq;
}

static u32 me_wait_idle(u32 budget_us)
{
  u32 t0 = sceKernelGetSystemTimeLow();

  while (!me_idle())
  {
    if ((sceKernelGetSystemTimeLow() - t0) >= budget_us)
      return 0;
  }
  return 1;
}

static void me_post(s32 out)
{
  u8 *sh_vram = snap;
  u8 *sh_oam  = snap + ME_SNAP_VRAM;
  u8 *sh_pal  = sh_oam + ME_SNAP_OAM;

  memcpy(sh_vram, vram, ME_SNAP_VRAM);
  memcpy(sh_oam, oam_ram, ME_SNAP_OAM);
  memcpy(sh_pal, palette_ram, ME_SNAP_PAL);
  sceKernelDcacheWritebackRange(snap, ME_SNAP_BYTES);
  sceKernelDcacheWritebackRange(cap[cap_cur], sizeof(me_capture_frame));

  desc->vram      = (u32)sh_vram;
  desc->oam       = (u32)sh_oam;
  desc->palette   = (u32)sh_pal;
  desc->capture   = (u32)cap[cap_cur];
  desc->out       = (u32)stage[out];
  desc->out_pitch = GBA_LINE_SIZE;
  desc->flags     = option_oam_hijacking_enabled ? ME_DESC_OAM_HIJACK : 0;
  sceKernelDcacheWritebackRange(desc, sizeof(*desc));

  mb->cmd  = ME_CMD_RENDER;
  mb->arg0 = (u32)ME_UNCACHED(desc);
  mb->cmd_seq = mb->cmd_seq + 1;
}

/* Heartbeat-only liveness check, once per presented frame. */
static u32 me_dead(void)
{
  u32 beat = mb->heartbeat;

  if (beat != beat_last)
  {
    beat_last  = beat;
    beat_stale = 0;
    return 0;
  }
  return ++beat_stale >= ME_STALE_LIMIT;
}

static s32 me_video_start(void)
{
  if (me_unavailable)
    return -1;

  if ((mb == NULL && me_load() < 0) || (desc == NULL && me_alloc() < 0))
  {
    me_unavailable = 1;
    return -1;
  }

  cap_cur       = 0;
  stage_pending = -1;
  stage_ready   = -1;
  late_frames   = 0;
  beat_last     = mb->heartbeat;
  beat_stale    = 0;
  me_capture_buf = cap[0];
  me_active = 1;
  return 0;
}

/* Back to the CPU renderer.  The frame that just ran was captured rather
 * than drawn, so leave the newest ME frame in screen_texture for the
 * caller's update_screen() and for everything outside the frame loop. */
static void me_video_stop(void)
{
  if (!me_active)
    return;

  me_active = 0;
  me_capture_buf = NULL;
  psp_video_set_source(NULL);

  if (stage_pending >= 0 && me_wait_idle(ME_DRAIN_US))
    stage_ready = stage_pending;

  if (stage_ready >= 0 && option_video_renderer != VIDEO_RENDERER_OLD)
  {
    u16 *src = stage[stage_ready];
    u16 *dst = screen_texture;
    u32 y;

    sceKernelDcacheInvalidateRange(src, ME_STAGE_BYTES);
    for (y = 0; y < GBA_SCREEN_HEIGHT; y++)
    {
      memcpy(dst, src, GBA_SCREEN_WIDTH * sizeof(u16));
      src += GBA_LINE_SIZE;
      dst += GBA_LINE_SIZE;
    }
  }

  stage_pending = -1;
  stage_ready   = -1;
}

static void me_video_fail(void)
{
  me_unavailable = 1;
  me_video_stop();
}

u32 me_video_frame(void)
{
  s32 out;

  if (option_video_renderer != VIDEO_RENDERER_NEW_ME)
  {
    me_video_stop();
    return 0;
  }

  if (!me_active)
  {
    /* The CPU drew this frame; capturing starts with the next one. */
    me_video_start();
    return 0;
  }

  if (stage_pending >= 0)
  {
    if (!me_wait_idle(ME_RETIRE_US))
    {
      /* Still rendering: show the previous frame and keep filling the same
       * capture buffer; the ME is reading the other one. */
      if (++late_frames >= ME_LATE_LIMIT)
      {
        me_video_fail();
        return 0;
      }
      goto present;
    }
    stage_ready   = stage_pending;
    stage_pending = -1;
    late_frames   = 0;
  }

  out = (stage_ready == 0) ? 1 : 0;
  me_post(out);
  stage_pending = out;
  cap_cur ^= 1;
  me_capture_buf = cap[cap_cur];

present:
  if (me_dead())
  {
    me_video_fail();
    return 0;
  }

  psp_video_set_source((stage_ready >= 0) ? stage[stage_ready] : NULL);
  (*update_screen)();
  return 1;
}

void me_video_shutdown(void)
{
  me_capture_buf = NULL;
  me_active = 0;
  psp_video_set_source(NULL);
  me_unload();
}
