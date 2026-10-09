/* me_render_env.h - what video_cc.cc needs from common.h when it is compiled
 * into the Media Engine PRX (ME_PRX_BUILD).  The PRX links the kernel libc,
 * so the user-side headers common.h pulls in (zlib, GU, stdio file I/O) are
 * kept out of it. */
#ifndef ME_RENDER_ENV_H
#define ME_RENDER_ENV_H

#include <psptypes.h>
#include <pspkerneltypes.h>
#include <string.h>

#define MAX_PATH (512)
#define MAX_FILE (256)

#define ADDRESS8(base, offset)  *((u8 *)((u8 *)(base) + (offset)))
#define ADDRESS16(base, offset) *((u16 *)((u8 *)(base) + (offset)))
#define ADDRESS32(base, offset) *((u32 *)((u8 *)(base) + (offset)))

#include "cpu.h"
#include "memory.h"
#include "video.h"

#endif /* ME_RENDER_ENV_H */
