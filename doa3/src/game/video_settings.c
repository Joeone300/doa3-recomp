/**
 * Video settings (see video_settings.h).
 */
#include "exe_path.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

#include "video_settings.h"
#include "xbe_layout.h"   /* DOA3_XBE_ID_<version> */

extern HWND d3d8_GetHWND(void);
extern ptrdiff_t g_xbox_mem_offset;

static int g_window_mode = VIDEO_BORDERLESS;
static int g_aspect      = VIDEO_ASPECT_16_9;
static int g_scale       = 3;
static int g_stretch_4_3;
static int g_disable_fog;
int video_get_disable_fog(void) { return g_disable_fog; }
void video_set_disable_fog(int disabled) { g_disable_fog = !!disabled; }
int video_get_stretch_4_3(void) { return g_stretch_4_3; }
void video_set_stretch_4_3(int enabled) { g_stretch_4_3 = !!enabled; }
static int g_custom_width = 2560, g_custom_height = 1440;
static int g_resolution_failed;
static int g_accepted_scale = 3, g_accepted_aspect = VIDEO_ASPECT_16_9;
static int g_accepted_width = 2560, g_accepted_height = 1440;

int video_resolution_valid(int width, int height)
{
    return width >= VIDEO_RENDER_MIN && height >= VIDEO_RENDER_MIN &&
           width <= VIDEO_RENDER_MAX && height <= VIDEO_RENDER_MAX &&
           (uint64_t)width * (uint64_t)height <= VIDEO_RENDER_MAX_PIXELS;
}

int video_set_custom_resolution(int width, int height)
{
    if (!video_resolution_valid(width, height)) return 0;
    g_custom_width = width; g_custom_height = height;
    g_scale = 0;
    g_resolution_failed = 0;
    return 1;
}

void video_get_custom_resolution(int *width, int *height)
{
    *width = g_custom_width; *height = g_custom_height;
}

void video_target_size_result(int success)
{
    if (success) {
        g_accepted_scale = g_scale; g_accepted_aspect = g_aspect;
        g_accepted_width = g_custom_width; g_accepted_height = g_custom_height;
    } else {
        g_scale = g_accepted_scale; g_aspect = g_accepted_aspect;
        g_custom_width = g_accepted_width; g_custom_height = g_accepted_height;
    }
    if (!success) g_resolution_failed = 1;
}

int video_resolution_failed(void) { return g_resolution_failed; }

int video_get_window_mode(void) { return g_window_mode; }
int video_get_aspect(void)      { return g_aspect; }
int video_get_scale(void)       { return g_scale; }

static int valid_aspect(int aspect)
{
    return aspect >= 0 && aspect < VIDEO_ASPECT_COUNT ? aspect : VIDEO_ASPECT_16_9;
}

const char *video_aspect_name(int aspect)
{
    static const char *names[] = {
        "4:3 (Full Screen)", "4:3 (Anamorphic)", "16:9", "21:9", "32:9"
    };
    return names[valid_aspect(aspect)];
}

float video_display_aspect(int aspect)
{
    switch (valid_aspect(aspect)) {
    case VIDEO_ASPECT_4_3_FULL: case VIDEO_ASPECT_4_3_ANAMORPHIC: return 4.0f / 3.0f;
    case VIDEO_ASPECT_21_9: return 21.0f / 9.0f;
    case VIDEO_ASPECT_32_9: return 32.0f / 9.0f;
    default: return 16.0f / 9.0f;
    }
}

float video_projection_ratio(int aspect)
{
    switch (valid_aspect(aspect)) {
    case VIDEO_ASPECT_4_3_FULL: return 3.0f / 4.0f;
    case VIDEO_ASPECT_21_9: return 9.0f / 21.0f;
    case VIDEO_ASPECT_32_9: return 9.0f / 32.0f;
    default: return 9.0f / 16.0f;
    }
}

void video_hud_region(unsigned target_width, float *x, float *width)
{
    float w = (float)target_width;
    if (g_aspect == VIDEO_ASPECT_21_9) w *= 16.0f / 21.0f;
    else if (g_aspect == VIDEO_ASPECT_32_9) w *= 0.5f;
    /* 0.2.25: whole pixels, so the 16:9 canvas sits on the same pixel grid
     * at 21:9 and 32:9 as at 16:9 (32:9 at 1x put it 0.75 px right). */
    *x = floorf(((float)target_width - w) * 0.5f + 0.5f);
    *width = w;
}

const char *doa3_settings_ini_path(void)
{
    static char path[MAX_PATH];
    if (path[0]) return path;
    doa3_exe_file_ansi("doa3_settings.ini", path, sizeof path);   /* 1.0.2: non-ANSI folders */
    return path;
}

int video_settings_load(void)
{
    const char *p = doa3_settings_ini_path();
    int wm = (int)GetPrivateProfileIntA("Video", "WindowMode", VIDEO_BORDERLESS, p);
    int legacy = (int)GetPrivateProfileIntA("Video", "Widescreen", 1, p);
    int ar = (int)GetPrivateProfileIntA("Video", "AspectRatio",
        legacy == 0 ? VIDEO_ASPECT_4_3_ANAMORPHIC : VIDEO_ASPECT_16_9, p);
    int sc = (int)GetPrivateProfileIntA("Video", "InternalResolution", 3, p);
    g_window_mode = (wm == VIDEO_WINDOWED) ? VIDEO_WINDOWED : VIDEO_BORDERLESS;
    g_aspect      = valid_aspect(ar);
    g_scale       = (sc >= 1 && sc <= 4) ? sc : 3;
    g_custom_width = 2560; g_custom_height = 1440;
    {
        int cw = (int)GetPrivateProfileIntA("Video", "RenderWidth", 2560, p);
        int ch = (int)GetPrivateProfileIntA("Video", "RenderHeight", 1440, p);
        if (video_resolution_valid(cw, ch)) {
            g_custom_width = cw; g_custom_height = ch;
            if (GetPrivateProfileIntA("Video", "CustomResolution", 0, p) == 1)
                g_scale = 0;
        }
    }
    g_disable_fog = GetPrivateProfileIntA("Video", "DisableFog", 0, p) == 1;
    g_stretch_4_3 = GetPrivateProfileIntA("Video", "Stretch4_3", 0, p) == 1;
    g_resolution_failed = 0;
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
}

int video_settings_save(void)
{
    const char *p = doa3_settings_ini_path();
    char aspect[16], width[16], height[16];
    _snprintf_s(aspect, sizeof(aspect), _TRUNCATE, "%d", g_aspect);
    _snprintf_s(width, sizeof width, _TRUNCATE, "%d", g_custom_width);
    _snprintf_s(height, sizeof height, _TRUNCATE, "%d", g_custom_height);
    /* WindowMode: 0 = windowed, 1 = borderless fullscreen.
     * AspectRatio: Full 4:3, Anamorphic 4:3, 16:9, 21:9, 32:9 (0..4).
     * Keep Widescreen for older builds: 0 = either 4:3 mode, 1 = wider.
     * InternalResolution: 1 = 480 lines, 2 = 960, 3 = 1440, 4 = 1920. */
    return WritePrivateProfileStringA("Video", "WindowMode",
                                      g_window_mode ? "1" : "0", p) &&
           WritePrivateProfileStringA("Video", "Widescreen",
                                      g_aspect <= VIDEO_ASPECT_4_3_ANAMORPHIC ? "0" : "1", p) &&
           WritePrivateProfileStringA("Video", "AspectRatio", aspect, p) &&
           WritePrivateProfileStringA("Video", "InternalResolution",
                                      g_scale == 0 ? "3" : g_scale == 4 ? "4" : g_scale == 3 ? "3" : g_scale == 2 ? "2" : "1", p) &&
           WritePrivateProfileStringA("Video", "CustomResolution", g_scale == 0 ? "1" : "0", p) &&
           WritePrivateProfileStringA("Video", "RenderWidth", width, p) &&
           WritePrivateProfileStringA("Video", "RenderHeight", height, p) &&
           WritePrivateProfileStringA("Video", "Stretch4_3", g_stretch_4_3 ? "1" : "0", p) &&
           WritePrivateProfileStringA("Video", "DisableFog", g_disable_fog ? "1" : "0", p);
}

/* 480 lines at 1x. The translator maps every guest vertex onto whatever size
 * this target is, so a larger target rasterises the scene at that resolution
 * rather than scaling up a 480-line picture. */
static void video_base_size(int aspect, unsigned *w, unsigned *h)
{
    *h = 480;
    switch (valid_aspect(aspect)) {
    case VIDEO_ASPECT_4_3_FULL: case VIDEO_ASPECT_4_3_ANAMORPHIC: *w = 640; break;
    case VIDEO_ASPECT_21_9: *w = 1120; break;
    case VIDEO_ASPECT_32_9: *w = 1707; break;
    default: *w = 854; break;
    }
}

void video_guest_target_size(int aspect, unsigned *w, unsigned *h)
{
    if (g_scale == 0) {
        *w = (unsigned)g_custom_width; *h = (unsigned)g_custom_height;
        return;
    }
    video_base_size(aspect, w, h);
    *w *= (unsigned)g_scale;
    *h *= (unsigned)g_scale;
}

void video_set_scale(int scale)
{
    /* Applied at the next frame boundary, like the aspect. */
    g_scale = (scale >= 1 && scale <= 4) ? scale : 3;
    g_resolution_failed = 0;
}

/* XC_VIDEO dword: XGetVideoFlags returns (value >> 16) & 0x5F, and
 * XC_VIDEO_FLAGS_WIDESCREEN is bit 0 of that. */
unsigned video_xc_video_value(void)
{
    return g_aspect != VIDEO_ASPECT_4_3_FULL ? 0x00010000u : 0u;
}

void video_set_aspect(int aspect)
{
    /* The d3d8 layer notices the change at the next frame boundary and
     * rebuilds the guest target (d3d8_device.c, present path). */
    g_aspect = valid_aspect(aspect);
}

/* Apply projection data before guest startup and at native frame boundaries.
 * D3D init (3.1 sub_0017B8D0; 3.0 sub_001539C0) caches XC_VIDEO in three
 * places: a projection flag, present flags, and raw video flags. Before init
 * has set the guest present width to 720, leave those caches alone; the
 * kernel query returns the requested setting directly. */
void video_sync_guest_widescreen(void)
{
#if defined(DOA3_XBE_ID_JAPAN)
    volatile uint8_t *m = (volatile uint8_t *)(uintptr_t)g_xbox_mem_offset;
    volatile uint32_t *pp_w, *pp_fl, *vflag, *ws;
    if (!g_xbox_mem_offset) return;
    /* Retail Japanese 3.1: file 0x244C20 = VA 0x251620 in .rdata.
     * 00 00 10 3F is the complete little-endian float 9/16; write all
     * four bytes so non-exact ratios such as 9/21 retain their mantissa.
     * Projection routines (e.g. sub_00027720) read this through MEMF. */
    *(volatile float *)(m + 0x251620) = video_projection_ratio(g_aspect);
    /* 0.1.35: the game's default for its movie display option (Options; 0
     * = 4:3, 1 = 16:9) is the .data byte 0x3F456C = 0. It is read when a
     * new save is made (sub_000204B0) and by the Options Default buttons
     * (0xC2590, 0xC2786, 0xC35FA, 0xC37AA). The port's default is 16:9; a
     * saved setting is not changed. */
    m[0x3F456C] = 1;
    /* sub_0017B8D0 caches XC_VIDEO here. Preserve all unrelated flag bits. */
    pp_w = (volatile uint32_t *)(m + 0xA48C30);
    pp_fl = (volatile uint32_t *)(m + 0xA48C58);
    vflag = (volatile uint32_t *)(m + 0xA48C78);
    ws = (volatile uint32_t *)(m + 0xA48CDC);
#elif defined(DOA3_XBE_ID_USA)
    volatile uint8_t *m = (volatile uint8_t *)(uintptr_t)g_xbox_mem_offset;
    volatile uint32_t *pp_w  = (volatile uint32_t *)(m + 0x90F4A8);
    volatile uint32_t *pp_fl = (volatile uint32_t *)(m + 0x90F4D0);
    volatile uint32_t *vflag = (volatile uint32_t *)(m + 0x90F4E0);
    volatile uint32_t *ws    = (volatile uint32_t *)(m + 0x90F544);
#else
    return;
#endif
#if defined(DOA3_XBE_ID_JAPAN) || defined(DOA3_XBE_ID_USA)
    int on = g_aspect != VIDEO_ASPECT_4_3_FULL;
    if (!g_xbox_mem_offset || *pp_w != 0x2D0u) return;
    if (on) { *ws |= 1u;  *pp_fl |= 0x10u;  *vflag |= 1u; }
    else    { *ws &= ~1u; *pp_fl &= ~0x10u; *vflag &= ~1u; }
#endif
}

void video_apply_window_mode(HWND hwnd, int mode)
{
    MONITORINFO mi;
    HMONITOR mon;
    if (!hwnd) return;
    mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoA(mon, &mi)) return;

    if (mode == VIDEO_BORDERLESS) {
        RECT r = mi.rcMonitor;
        SetWindowLongPtrA(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        unsigned cw, ch;
        RECT r, wa = mi.rcWork;
        video_base_size(g_aspect, &cw, &ch);
        r.left = 0; r.top = 0; r.right = (LONG)cw; r.bottom = (LONG)ch;
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowLongPtrA(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_NOTOPMOST,
                     wa.left + ((wa.right - wa.left) - (r.right - r.left)) / 2,
                     wa.top + ((wa.bottom - wa.top) - (r.bottom - r.top)) / 2,
                     r.right - r.left, r.bottom - r.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
}

void video_set_window_mode(int mode)
{
    g_window_mode = (mode == VIDEO_WINDOWED) ? VIDEO_WINDOWED : VIDEO_BORDERLESS;
    video_apply_window_mode(d3d8_GetHWND(), g_window_mode);
}
