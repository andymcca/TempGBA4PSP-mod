/* tempgba_me kernel PRX - boots the Media Engine and runs its dispatcher.
 *
 * Ported from gbadhoc's psp/me/main.c (GPL-2.0), whose boot sequence and
 * cache ops follow DaedalusX64's MediaEngine PRX (itself from melib, (c)
 * mrbrown).  The suspend handling follows the approach of mcidclan's
 * psp-media-engine-custom-core (MIT).
 *
 * The PRX exports nothing: the EBOOT passes the mailbox pointer in the
 * sceKernelStartModule arguments and every later interaction goes through
 * the mailbox.  Every job the ME can run is compiled into this PRX and named
 * by ME_CMD_*, so user .text never executes on the ME.
 *
 * ME-side rules (me_dispatch and everything it calls): no syscalls, no
 * floating point, cache ops via __builtin_allegrex_cache only, and all
 * mailbox access through the uncached alias the host handed us.
 */
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspsysreg.h>
#include <pspsysevent.h>
#include <string.h>

#include "me_shared.h"

PSP_MODULE_INFO("tempgba_me", 0x1006, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

extern void me_stub(void);
extern void me_stub_end(void);

extern unsigned int me_render_run(volatile me_mbox *mb, unsigned int seq);

/* When idle, spin on a cached local and poll the mailbox only every ~50 us:
 * every uncached poll is a main-memory transaction on the shared bus. */
#define ME_IDLE_SPIN 4096u

static void me_dispatch(volatile me_mbox *mb)
{
  unsigned int seen;

  mb->version = ME_MBOX_VERSION;
  mb->magic   = ME_MBOX_MAGIC;
  seen = mb->cmd_seq;
  mb->done_seq = seen;

  for (;;)
  {
    unsigned int seq = mb->cmd_seq;
    if (seq == seen)
    {
      volatile unsigned int d = ME_IDLE_SPIN;
      while (d)
        d--;
      mb->heartbeat = mb->heartbeat + 1;
      continue;
    }
    seen = seq;
    mb->heartbeat = mb->heartbeat + 1;

    switch (mb->cmd)
    {
      case ME_CMD_RENDER:
        mb->result = me_render_run(mb, seen);
        break;

      case ME_CMD_PARK:
        mb->done_seq = seen;
        for (;;)
        {
          volatile int spin = 65536;
          while (spin--)
            ;
          if (mb->cmd_seq != seen)
            break;
          mb->heartbeat = mb->heartbeat + 1;
        }
        continue;

      case ME_CMD_NOP:
      default:
        mb->result = 0;
        break;
    }
    /* Drain the write buffer so every uncached output store reaches RAM
     * before the host can observe completion. */
    asm volatile("sync" ::: "memory");
    mb->done_seq = seen;
  }
}

/* The OS uses the ME itself (it is the media decoder), and its resume path
 * jumps to the reset vector.  Snapshot what we overwrite and put it back in
 * module_stop.  me_stub is 0x80 bytes; 0x600/0x604 hold the entry point and
 * the mailbox argument the stub reads. */
#define ME_VEC_BYTES 0x80
static unsigned char me_vec_save[ME_VEC_BYTES];
static unsigned int  me_arg_save[2];
static int me_vec_saved;

static int me_boot(volatile me_mbox *mb)
{
  unsigned int k1 = pspSdkSetK1(0);
  int stub_len = (int)((int)me_stub_end - (int)me_stub);

  if (!me_vec_saved)
  {
    memcpy(me_vec_save, (void *)0xbfc00040, ME_VEC_BYTES);
    me_arg_save[0] = _lw(0xbfc00600);
    me_arg_save[1] = _lw(0xbfc00604);
    me_vec_saved = 1;
  }
  if (stub_len > ME_VEC_BYTES)
    stub_len = ME_VEC_BYTES;

  memcpy((void *)0xbfc00040, me_stub, stub_len);
  _sw((unsigned int)me_dispatch, 0xbfc00600);
  _sw((unsigned int)mb,          0xbfc00604);
  sceKernelDcacheWritebackAll();
  sceSysregMeResetEnable();
  sceSysregMeBusClockEnable();
  sceSysregMeResetDisable();

  pspSdkSetK1(k1);
  return 0;
}

/* Suspend/resume.  The kernel's own ME driver registers a system event
 * handler ("SceMeRpc") and on suspend tries to put away an ME it no longer
 * controls, which kills the console inside the kernel's suspend.  Take that
 * handler's slot: park the ME on suspend (0x402), release it on resume
 * (0x10005).  Releasing reset restarts the ME from its vector, which still
 * holds our stub and mailbox pointer, so it reboots into the dispatcher. */
#define HW_SYS_RESET_ENABLE  (*(volatile unsigned int *)0xbc10004c)
#define ME_HW_RESET          0x14
#define SE_EV_SUSPEND        0x00000402
#define SE_EV_RESUME         0x00010005

static PspSysEventHandler    *g_me_seh;
static PspSysEventHandlerFunc g_me_seh_orig;

static int me_sysevent(int ev_id, char *ev_name, void *param, int *result)
{
  static int last_id;
  (void)ev_name; (void)param; (void)result;

  if (ev_id == SE_EV_SUSPEND && last_id != SE_EV_SUSPEND)
  {
    HW_SYS_RESET_ENABLE = ME_HW_RESET;
    asm volatile("sync");
    last_id = SE_EV_SUSPEND;
  }
  else if (ev_id == SE_EV_RESUME && last_id != SE_EV_RESUME)
  {
    HW_SYS_RESET_ENABLE = ME_HW_RESET;
    HW_SYS_RESET_ENABLE = 0;
    asm volatile("sync");
    last_id = SE_EV_RESUME;
  }
  return 0;
}

/* "MeR" anywhere in the first 16 characters; the name is not guaranteed to
 * be NUL-terminated from this context. */
static int me_name_is_merpc(const char *n)
{
  int i;
  if (!n)
    return 0;
  for (i = 0; i < 16 - 2; i++)
  {
    if (n[i] == 0)
      return 0;
    if (n[i] == 'M' && n[i + 1] == 'e' && n[i + 2] == 'R')
      return 1;
  }
  return 0;
}

static int me_sysevent_install(void)
{
  PspSysEventHandler *seh = sceKernelReferSysEventHandler();
  int walked = 0;
  while (seh && walked < 128)
  {
    walked++;
    if (me_name_is_merpc(seh->name))
    {
      g_me_seh      = seh;
      g_me_seh_orig = seh->handler;
      seh->handler  = me_sysevent;
      return walked;
    }
    seh = seh->next;
  }
  return -1 - walked;
}

static void me_sysevent_remove(void)
{
  if (g_me_seh && g_me_seh_orig)
    g_me_seh->handler = g_me_seh_orig;
  g_me_seh      = NULL;
  g_me_seh_orig = NULL;
}

/* argp[0] = mailbox pointer (uncached alias). */
int module_start(SceSize args, void *argp)
{
  volatile me_mbox *mb;
  if (args < 4 || !argp)
    return -1;
  mb = (volatile me_mbox *)(*(unsigned int *)argp);
  if (!mb)
    return -1;
  me_sysevent_install();
  return me_boot(mb);
}

int module_stop(SceSize args, void *argp)
{
  (void)args; (void)argp;
  me_sysevent_remove();
  sceSysregMeResetEnable();
  /* me_boot enabled the bus clock; leaving it on is what the kernel cannot
   * survive a later suspend with. */
  sceSysregMeBusClockDisable();
  if (me_vec_saved)
  {
    unsigned int k1 = pspSdkSetK1(0);
    memcpy((void *)0xbfc00040, me_vec_save, ME_VEC_BYTES);
    _sw(me_arg_save[0], 0xbfc00600);
    _sw(me_arg_save[1], 0xbfc00604);
    sceKernelDcacheWritebackAll();
    pspSdkSetK1(k1);
  }
  return 0;
}
