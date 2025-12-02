/* ----- ----- ----- ----- */
// status.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/12/02
// Update Date: 2025/12/02
// Version: v1.0
/* ----- ----- ----- ----- */

/* Return code convention:
 *   0      = success
 *   > 0    = non-fatal statuses (info/warning) that caller may want to inspect
 *   < 0    = fatal error codes
 */

typedef enum {
    ELEV_OK            = 0,   /* success */
    ELEV_DUPLICATE     = 1,   /* request already exists (non-fatal) */
    ELEV_IGNORED       = 2    /* request ignored due policy (non-fatal) */
    /* ... add other positive statuses if needed ... */
} ElevStatus_Pos;

typedef enum {
    ELEV_ERR_INVALID   = -1,  /* invalid argument (null pointer, out-of-range) */
    ELEV_ERR_FULL      = -2,  /* queue full or no resource */
    ELEV_ERR_INTERNAL  = -3   /* internal error */
    /* ... add other negative error codes ... */
} ElevStatus_Neg;
