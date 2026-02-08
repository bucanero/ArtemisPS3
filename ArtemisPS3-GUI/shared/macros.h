#pragma once

#if !defined(_countof)
#define _countof(a) sizeof(a) / sizeof(*a)
#endif

#if !defined(_countof_1)
#define _countof_1(a) (_countof(a) - 1)
#endif

#if !defined(MAX_PATH)
#define MAX_PATH 260
#endif

#if !defined(MAX_LINE_LENGTH)
#define MAX_LINE_LENGTH 1024
#endif

#if !defined(MAX_ARRAY_LENGTH)
#define MAX_ARRAY_LENGTH 16
#endif

#if !defined(MAX_TITLE_IDS)
#define MAX_TITLE_IDS MAX_ARRAY_LENGTH
#endif
#if !defined(MAX_APP_VERS)
#define MAX_APP_VERS MAX_ARRAY_LENGTH
#endif

#define isZero(e) (e == 0)

#if !defined(here)
#define here() printf("%s:%s:%d here\n", __FUNCTION__, __FILE__, __LINE__)
#endif

static void print_bool_(const char* vn, const bool v)
{
#if defined(_VSHPRX)
    vsh::printf
#else
    printf
#endif
    ("%s: %s\n", vn, v ? "true" : "false");
}

#define print_bool(v) print_bool_(#v, v)
#define strncmp2(s, s2) strncmp(s, s2, _countof_1(s2))
