/* me_video.h - the "New (ME)" video renderer: the New renderer running on the
 * PSP's Media Engine (ported from gbadhoc, see src/me_shared.h).
 *
 * While it is active the core records each visible line into me_capture_buf
 * instead of drawing it, and at vcount 160 me_video_frame() hands the frame
 * to the ME and presents the previous one.  Any failure (PRX missing, ME not
 * answering, renders too late) falls back to the New renderer on the CPU for
 * the rest of the session.
 */
#ifndef ME_VIDEO_H
#define ME_VIDEO_H

#include "me_shared.h"

/* Non-NULL only while frames are being captured for the ME. */
extern me_capture_frame *me_capture_buf;

/* vcount 160 of a drawn frame.  Returns 1 if it presented the frame, 0 if
 * the caller must run update_screen() as usual. */
u32 me_video_frame(void);

/* Stop the ME and unload the PRX (puts the ME reset vector back). */
void me_video_shutdown(void);

#endif /* ME_VIDEO_H */
