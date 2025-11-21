/* ----- ----- ----- ----- */
// platform.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/22
// Version: v1.0
/* ----- ----- ----- ----- */

#pragma once

#ifdef _WIN32
    #include <windows.h>
    #define delay_ms(x) Sleep(x)
#else
    #include <unistd.h>
    #define delay_ms(x) usleep((x) * 1000)
#endif
