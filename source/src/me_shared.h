/* me_shared.h - the one contract between the main CPU and the Media Engine.
 *
 * Included by the EBOOT (src/me_video.c, src/video_cc.cc) and by the kernel
 * PRX that runs on the ME (prx/me).  Ported from gbadhoc's psp/me/me_mbox.h
 * (ADR-0080): a sequence-numbered mailbox in main RAM that both cores only
 * ever touch through the 0x40000000 uncached alias, and a fixed set of jobs
 * compiled into the PRX.  No function pointers cross the boundary.
 *
 * Every field is a 32-bit or 16-bit integer so the layout is identical under
 * the EBOOT's EABI flags and the PRX's.
 */
#ifndef ME_SHARED_H
#define ME_SHARED_H

#define ME_MBOX_MAGIC   0x4D454F4Bu   /* "MEOK": the ME writes this at boot */
#define ME_MBOX_VERSION 1

#define ME_CMD_NOP      0
#define ME_CMD_RENDER   1             /* arg0 = uncached me_render_desc * */
#define ME_CMD_PARK     2

/* Lines 0-159 of the LCD register block.  The renderer reads nothing above
 * BLDY (halfword 0x2A); 64 halfwords keep each line a whole 128 bytes. */
#define ME_CAP_IOREGS   64

/* What the main CPU records while a frame's visible lines run, instead of
 * drawing them: each line's LCD registers, and the affine reference point
 * that line is drawn with (a mid-frame BGxX/BGxY write reloads it, so it
 * cannot be recomputed from line 0). */
typedef struct
{
  unsigned short ioregs[160][ME_CAP_IOREGS];
  int            affine[160][4];      /* BG2X, BG3X, BG2Y, BG3Y */
} me_capture_frame;

#define ME_DESC_OAM_HIJACK 1u

typedef struct
{
  volatile unsigned int vram;         /* u8[0x18000] snapshot             */
  volatile unsigned int oam;          /* u16[0x200] snapshot              */
  volatile unsigned int palette;      /* u16[0x200] snapshot              */
  volatile unsigned int capture;      /* me_capture_frame for this frame  */
  volatile unsigned int out;          /* 16-bit output, out_pitch stride  */
  volatile unsigned int out_pitch;    /* in pixels                        */
  volatile unsigned int flags;        /* ME_DESC_*                        */
  volatile unsigned int pad;
} me_render_desc;

typedef struct
{
  /* written by the ME */
  volatile unsigned int magic;
  volatile unsigned int heartbeat;
  volatile unsigned int done_seq;
  volatile unsigned int result;
  /* written by the host; the ME acts on the cmd_seq write */
  volatile unsigned int cmd_seq;
  volatile unsigned int cmd;
  volatile unsigned int arg0;
  volatile unsigned int version;      /* written by the ME */
  volatile unsigned int input_seq;    /* the ME has copied the inputs */
  volatile unsigned int pad[7];
} me_mbox;

#define ME_UNCACHED(p) ((void *)(0x40000000u | (unsigned int)(p)))
#define ME_CACHED(a)   ((void *)((unsigned int)(a) & ~0x40000000u))

#endif /* ME_SHARED_H */
