/* xr_shared.h - between the game-thread capture (xr_capture.c) and the headset thread (xr_session.c). */
#ifndef BD_XR_SHARED_H
#define BD_XR_SHARED_H

#include <windows.h>
#include "xr.h"

extern XrLogFn g_xr_log;
extern volatile LONG g_xr_run;       /* 1 while the headset thread should keep going */
extern volatile LONG g_xr_capture;   /* 1 while the switch file is there */

/* Exe folder with a trailing backslash (not the log folder, which may be %TEMP%). */
const char *xr_exe_dir(void);

/* Size of the pictures being captured, 0 x 0 until the first one. */
void xr_frame_size(int *w, int *h);

/* Copy eye e's (0 left, 1 right) latest picture into dst (w*h*4 bytes, BGRX rows packed) if it is newer than
 * *serial and still w x h. Returns 1 when copied, and updates *serial. */
int xr_frame_take(int e, unsigned char *dst, int w, int h, unsigned long *serial);

/* xr_session.c: the headset thread's body. */
void xr_session_main(void);

#endif
