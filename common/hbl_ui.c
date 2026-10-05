#include "common.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>

extern uint8_t *folder_icon_large;
extern uint8_t *invalid_icon_large;
extern uint8_t *theme_icon_large;

#define PARTICLE_COUNT 500
#define BTN_SRC_W 782
#define BTN_SRC_H 152
#define ARROW_W 100
#define ARROW_H 178
#define BOX_W 900
#define BOX_H 610
#define SMALL_BTN_W 252
#define SMALL_BTN_H 64
#define ICON_SLOT 108

typedef struct {
    float x, y, vx, vy, size, alpha;
} hblParticle;

typedef struct {
    int x, y, w, h;
} hblRect;

static hblParticle g_particles[PARTICLE_COUNT];
static int g_particles_inited = 0;
static float g_currentLeft = 0.0f;
static float g_targetLeft = 0.0f;
static int g_listOffset = 0;
static int g_motionReady = 0;
static float g_rowScale[HBL_BUTTONS_PER_PAGE];
static int g_scalePage = -1;

static int g_launchOpen = 0;
static menuEntry_s *g_launchEntry = NULL;
static int g_launchSel = 0; /* 0 = Load, 1 = Back */
static float g_launchAlpha = 0.0f;
static hblRect g_loadRect, g_backRect;

static int btnWAt(float scale) { return (int)(BTN_SRC_W * scale); }
static int btnHAt(float scale) { return (int)(BTN_SRC_H * scale); }

void hblResetMotion(void)
{
    g_motionReady = 0;
    g_currentLeft = 0.0f;
    g_targetLeft = 0.0f;
    g_listOffset = 0;
    g_scalePage = -1;
}

static int insideRoundRect(int px, int py, int w, int h, int r)
{
    int cx, cy;
    if (px < 0 || py < 0 || px >= w || py >= h) return 0;
    if (px >= r && px < w - r) return 1;
    if (py >= r && py < h - r) return 1;
    if (px < r && py < r) { cx = r; cy = r; }
    else if (px >= w - r && py < r) { cx = w - 1 - r; cy = r; }
    else if (px < r && py >= h - r) { cx = r; cy = h - 1 - r; }
    else { cx = w - 1 - r; cy = h - 1 - r; }
    {
        int dx = px - cx;
        int dy = py - cy;
        return dx * dx + dy * dy <= r * r;
    }
}

static void blitRGBA(const uint8_t *img, int sw, int sh, int dx, int dy, int dw, int dh, float amul)
{
    int x, y;
    if (!img || dw <= 0 || dh <= 0) return;
    for (y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        if (sy >= sh) sy = sh - 1;
        for (x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            if (sx >= sw) sx = sw - 1;
            const uint8_t *p = img + ((sy * sw) + sx) * 4;
            uint8_t a = (uint8_t)(p[3] * amul);
            if (a == 0) continue;
            DrawPixel(dx + x, dy + y, MakeColor(p[0], p[1], p[2], a));
        }
    }
}

static void blitRGBSquare(const uint8_t *img, int sw, int sh, int dx, int dy, int dw, int dh)
{
    int x, y;
    int rr;
    if (!img || dw <= 0 || dh <= 0) return;
    rr = dw / 7;
    if (rr < 6) rr = 6;
    for (y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        if (sy >= sh) sy = sh - 1;
        for (x = 0; x < dw; x++) {
            const uint8_t *p;
            int sx;
            if (!insideRoundRect(x, y, dw, dh, rr)) continue;
            sx = x * sw / dw;
            if (sx >= sw) sx = sw - 1;
            p = img + ((sy * sw) + sx) * 3;
            DrawPixel(dx + x, dy + y, MakeColor(p[0], p[1], p[2], 255));
        }
    }
}

static const uint8_t *assetBuf(AssetId id, int *w, int *h)
{
    assetsDataEntry *data = NULL;
    assetsGetData(id, &data);
    if (!data || !data->buffer) return NULL;
    if (w) *w = (int)data->imageSize[0];
    if (h) *h = (int)data->imageSize[1];
    return data->buffer;
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void hblUiInit(void)
{
    int i;
    if (g_particles_inited) return;
    srand((unsigned)time(NULL));
    for (i = 0; i < PARTICLE_COUNT; i++) {
        g_particles[i].x = (float)(rand() % 1280);
        g_particles[i].y = (float)(rand() % 720);
        g_particles[i].vx = ((rand() % 100) - 50) / 90.0f;
        g_particles[i].vy = -0.15f - (rand() % 100) / 180.0f;
        g_particles[i].size = 1.0f + (rand() % 4) * 0.6f;
        g_particles[i].alpha = 70.0f + (rand() % 140);
    }
    for (i = 0; i < HBL_BUTTONS_PER_PAGE; i++)
        g_rowScale[i] = HBL_IDLE_SCALE;
    g_particles_inited = 1;
}

void hblDrawBackground(void)
{
    int x, y;
    /* Wii U HBL corner colors: (79,153,239) top -> (59,159,223) bottom */
    for (y = 0; y < 720; y++) {
        float t = (float)y / 719.0f;
        uint8_t r = (uint8_t)(79 + (59 - 79) * t);
        uint8_t g = (uint8_t)(153 + (159 - 153) * t);
        uint8_t b = (uint8_t)(239 + (223 - 239) * t);
        color_t c = MakeColor(r, g, b, 255);
        for (x = 0; x < 1280; x += 4)
            Draw4PixelsRaw(x, y, c);
    }
}

void hblDrawParticles(void)
{
    int i, px, py, s, ox, oy;
    hblUiInit();
    for (i = 0; i < PARTICLE_COUNT; i++) {
        hblParticle *p = &g_particles[i];
        p->x += p->vx * HBL_PARTICLE_SPEED;
        p->y += p->vy * HBL_PARTICLE_SPEED;
        if (p->x < -4) p->x = 1284;
        if (p->x > 1284) p->x = -4;
        if (p->y < -4) p->y = 724;
        if (p->y > 724) p->y = -4;
        s = (int)p->size;
        {
            color_t c = MakeColor(255, 255, 255, (uint8_t)p->alpha);
            for (oy = 0; oy <= s; oy++) {
                for (ox = 0; ox <= s; ox++) {
                    px = (int)p->x + ox;
                    py = (int)p->y + oy;
                    DrawPixel(px, py, c);
                }
            }
        }
    }
}

static int entryCenterY(int row)
{
    int h = btnHAt(HBL_IDLE_SCALE);
    float step = (float)(h + 20);
    float fYOffset = step * 1.5f - step * (float)row;
    return (int)(360.0f - fYOffset);
}

static float rowScaleFor(int page, int row, int selected)
{
    (void)selected;
    if (page == g_scalePage)
        return g_rowScale[row];
    return HBL_IDLE_SCALE;
}

static void entryRect(int page, int row, float scale, hblRect *r)
{
    int dw = btnWAt(scale);
    int dh = btnHAt(scale);
    int cx = 640 + (int)g_currentLeft + page * 1280;
    int cy = entryCenterY(row);
    r->x = cx - dw / 2;
    r->y = cy - dh / 2;
    r->w = dw;
    r->h = dh;
}

static void drawWideButton(int x, int y, int w, int h, int selected)
{
    int sw, sh;
    const uint8_t *img = assetBuf(selected ? AssetId_hbl_button_selected : AssetId_hbl_button, &sw, &sh);
    if (img)
        blitRGBA(img, sw, sh, x, y, w, h, 1.0f);
    else {
        int i, j;
        color_t fill = selected ? MakeColor(255, 255, 255, 230) : MakeColor(255, 255, 255, 180);
        for (j = 0; j < h; j++)
            for (i = 0; i < w; i++)
                DrawPixel(x + i, y + j, fill);
    }
}

static void drawEntryButton(menuEntry_s *me, int page, int row, int selected)
{
    float scale = rowScaleFor(page, row, selected);
    int dw, dh, x, y;
    const uint8_t *icon;
    char tmp[1024];
    int textX, textY, iconSize, ix, iy;
    color_t white = MakeColor(255, 255, 255, 255);
    color_t nameCol = MakeColor(255, 255, 255, 255);
    hblRect r;

    entryRect(page, row, scale, &r);
    dw = r.w; dh = r.h; x = r.x; y = r.y;

    if (x + dw < 0 || x > 1280)
        return;

    drawWideButton(x, y, dw, dh, selected);

    icon = NULL;
    if (me->icon_gfx)
        icon = me->icon_gfx;
    else if (me->type == ENTRY_TYPE_FOLDER)
        icon = folder_icon_large;
    else if (me->type == ENTRY_TYPE_THEME)
        icon = theme_icon_large;
    else
        icon = invalid_icon_large;

    /* Switch NRO icons are 256x256. Draw them SQUARE — never squash to 256x96. */
    iconSize = ICON_SLOT;
    if (iconSize > dh - 16) iconSize = dh - 16;
    ix = x + 28;
    iy = y + (dh - iconSize) / 2;
    if (icon)
        blitRGBSquare(icon, 256, 256, ix, iy, iconSize, iconSize);

    textX = ix + iconSize + 22;
    textY = y + dh / 2 - 22;
    memset(tmp, 0, sizeof(tmp));
    snprintf(tmp, sizeof(tmp) - 1, "%s%s", me->starred ? "★ " : "", me->name);
    DrawTextTruncate(interuiregular18, textX, textY, nameCol, tmp, 430, "...");

    {
        const char *desc = me->author[0] ? me->author : (me->version[0] ? me->version : "");
        DrawTextTruncate(interuiregular14, textX, textY + 28, white, desc, 430, "...");
    }
}

static void tickPageMotion(int wantedOffset)
{
    float diff;
    if (!g_motionReady) {
        g_listOffset = wantedOffset;
        g_currentLeft = g_targetLeft = (float)(-g_listOffset * 1280);
        g_motionReady = 1;
        return;
    }
    if (wantedOffset != g_listOffset) {
        if (wantedOffset - g_listOffset > 1 || wantedOffset - g_listOffset < -1) {
            /* folder / menu swap: snap instead of sliding across empty pages */
            g_currentLeft = (float)(-wantedOffset * 1280);
        }
        g_listOffset = wantedOffset;
        g_targetLeft = (float)(-g_listOffset * 1280);
    }
    diff = g_targetLeft - g_currentLeft;
    if (diff > -HBL_PAGE_SNAP_PX && diff < HBL_PAGE_SNAP_PX) {
        g_currentLeft = g_targetLeft;
    } else {
        /* exponential ease-out: smooth, no linear 35px stepping */
        g_currentLeft += diff * HBL_PAGE_LERP;
    }
}

static void tickSelectScale(int page, int selRow)
{
    int i;
    if (page != g_scalePage) {
        g_scalePage = page;
        for (i = 0; i < HBL_BUTTONS_PER_PAGE; i++)
            g_rowScale[i] = (i == selRow) ? HBL_SELECT_SCALE : HBL_IDLE_SCALE;
        return;
    }
    for (i = 0; i < HBL_BUTTONS_PER_PAGE; i++) {
        float target = (i == selRow) ? HBL_SELECT_SCALE : HBL_IDLE_SCALE;
        g_rowScale[i] += (target - g_rowScale[i]) * HBL_SELECT_LERP;
    }
}

void hblDrawEntryList(menu_s *menu)
{
    menuEntry_s *me;
    int i, page, row, n;
    int sw, sh;
    const uint8_t *arr;
    int wantedOffset;

    if (!menu || menu->nEntries <= 0)
        return;

    wantedOffset = menu->curEntry / HBL_BUTTONS_PER_PAGE;
    tickPageMotion(wantedOffset);
    tickSelectScale(wantedOffset, menu->curEntry % HBL_BUTTONS_PER_PAGE);

    n = menu->nEntries;
    for (me = menu->firstEntry, i = 0; me; me = me->next, i++) {
        page = i / HBL_BUTTONS_PER_PAGE;
        row = i % HBL_BUTTONS_PER_PAGE;
        drawEntryButton(me, page, row, i == menu->curEntry);
    }

    if (n > HBL_BUTTONS_PER_PAGE) {
        if (g_listOffset > 0) {
            arr = assetBuf(AssetId_hbl_arrow_left, &sw, &sh);
            if (arr) blitRGBA(arr, sw, sh, 40, 360 - ARROW_H / 2, ARROW_W, ARROW_H, 1.0f);
        }
        if ((g_listOffset + 1) * HBL_BUTTONS_PER_PAGE < n) {
            arr = assetBuf(AssetId_hbl_arrow_right, &sw, &sh);
            if (arr) blitRGBA(arr, sw, sh, 1280 - 40 - ARROW_W, 360 - ARROW_H / 2, ARROW_W, ARROW_H, 1.0f);
        }
    }
}

void hblDrawChrome(menu_s *menu, const char *title)
{
    color_t white = MakeColor(255, 255, 255, 255);
    color_t dim = MakeColor(255, 255, 255, 210);
    char clock[16];
    char batt[16];
    uint32_t charge = 0;
    bool charging = 0;
    time_t unixTime;
    struct tm *timeStruct;

    if (!title) title = "Homebrew Launcher";
    DrawText(interuimedium20, 28, 18, white, title);

#ifdef __SWITCH__
    {
        AppletType at = appletGetAppletType();
        if (at != AppletType_Application && at != AppletType_SystemApplication) {
            DrawText(interuiregular14, 28, 44, MakeColor(255, 230, 80, 255), "Applet Mode");
        }
    }
#endif

    unixTime = time(NULL);
    timeStruct = localtime((const time_t *)&unixTime);
    if (timeStruct)
        snprintf(clock, sizeof(clock), "%02d:%02d", timeStruct->tm_hour, timeStruct->tm_min);
    else
        snprintf(clock, sizeof(clock), "--:--");
    DrawText(interuiregular18, 1280 - 96, 18, white, clock);

    if (powerGetDetails(&charge, &charging)) {
        if (charge > 100) charge = 100;
        snprintf(batt, sizeof(batt), charging ? "+%u%%" : "%u%%", (unsigned)charge);
        DrawText(interuiregular14, 1280 - 176, 20, dim, batt);
    }

    if (menu && menu->nEntries <= 0) {
        DrawText(interuimedium20, 360, 320, white, "No applications found");
        DrawText(interuiregular18, 300, 360, dim, "Place .nro files in sd:/switch");
    }

    DrawText(interuiregular14, 27, 720 - 36, white,
             "\uE0E0 Load    \uE0E1 Back    \uE0E3 NetLoader    \uE0E2 Star    \uE0EF Exit");
    DrawText(interuiregular18, 1280 - 430, 720 - 42, white, "Homebrew Launcher " HBL_VERSION_NX " by Dimok");
}

bool hblLaunchBoxIsOpen(void) { return g_launchOpen; }

void hblLaunchBoxOpen(menuEntry_s *me)
{
    g_launchEntry = me;
    g_launchOpen = 1;
    g_launchSel = 0;
    g_launchAlpha = 0.0f;
}

void hblLaunchBoxClose(void)
{
    g_launchOpen = 0;
    g_launchEntry = NULL;
    g_launchAlpha = 0.0f;
}

void hblLaunchBoxConfirm(void)
{
    menuEntry_s *me = g_launchEntry;
    hblLaunchBoxClose();
    if (me)
        launchMenuEntryTask(me);
}

#ifdef __SWITCH__
bool hblLaunchBoxHandleInput(u64 down)
{
    if (down & HidNpadButton_B) { hblLaunchBoxClose(); return true; }
    if (down & HidNpadButton_AnyLeft) { g_launchSel = 0; return true; }
    if (down & HidNpadButton_AnyRight) { g_launchSel = 1; return true; }
    if (down & HidNpadButton_A) {
        if (g_launchSel == 0) hblLaunchBoxConfirm();
        else hblLaunchBoxClose();
        return true;
    }
    return true;
}
#else
bool hblLaunchBoxHandleInput(u64 down)
{
    (void)down;
    return true;
}
#endif

void hblDrawLaunchBox(void)
{
    int sw, sh, x, y, bx, by;
    const uint8_t *box, *btn, *btnsel;
    menuEntry_s *me = g_launchEntry;
    color_t white = MakeColor(255, 255, 255, 255);
    color_t dim = MakeColor(255, 255, 255, 220);
    int iconSize = 176;
    float a;

    if (!g_launchOpen || !me) return;

    if (g_launchAlpha < 1.0f) {
        g_launchAlpha += HBL_LAUNCH_FADE;
        if (g_launchAlpha > 1.0f) g_launchAlpha = 1.0f;
    }
    a = clampf(g_launchAlpha, 0.0f, 1.0f);

    for (y = 0; y < 720; y++) {
        for (x = 0; x < 1280; x++)
            DrawPixel(x, y, MakeColor(0, 0, 0, (uint8_t)(100 * a)));
    }

    x = (1280 - BOX_W) / 2;
    y = (720 - BOX_H) / 2 + 16;
    box = assetBuf(AssetId_hbl_launch_box, &sw, &sh);
    if (box) blitRGBA(box, sw, sh, x, y, BOX_W, BOX_H, a);

    DrawTextTruncate(interuimedium30, x + 80, y + 36, white, me->name, BOX_W - 160, "...");

    if (me->icon_gfx)
        blitRGBSquare(me->icon_gfx, 256, 256, x + 90, y + 96, iconSize, iconSize);

    DrawText(interuiregular18, x + 300, y + 120, dim, "Author");
    DrawTextTruncate(interuiregular18, x + 300, y + 148, white,
                     me->author[0] ? me->author : "-", 460, "...");
    DrawText(interuiregular18, x + 300, y + 190, dim, "Version");
    DrawTextTruncate(interuiregular18, x + 300, y + 218, white,
                     me->version[0] ? me->version : "-", 460, "...");

    DrawTextTruncate(interuiregular14, x + 90, y + 300, white,
                     me->type == ENTRY_TYPE_FOLDER ? "Open this folder." : "Load this homebrew application.",
                     BOX_W - 180, "...");

    btn = assetBuf(AssetId_hbl_small_button, &sw, &sh);
    btnsel = assetBuf(AssetId_hbl_small_button_selected, &sw, &sh);
    by = y + BOX_H - 96;

    bx = x + BOX_W / 2 - 200 - SMALL_BTN_W / 2;
    g_loadRect.x = bx; g_loadRect.y = by; g_loadRect.w = SMALL_BTN_W; g_loadRect.h = SMALL_BTN_H;
    if (g_launchSel == 0 && btnsel) blitRGBA(btnsel, sw, sh, bx, by, SMALL_BTN_W, SMALL_BTN_H, 1.0f);
    else if (btn) blitRGBA(btn, sw, sh, bx, by, SMALL_BTN_W, SMALL_BTN_H, 1.0f);
    DrawText(interuiregular18, bx + 95, by + 22, white, "Load");

    bx = x + BOX_W / 2 + 200 - SMALL_BTN_W / 2;
    g_backRect.x = bx; g_backRect.y = by; g_backRect.w = SMALL_BTN_W; g_backRect.h = SMALL_BTN_H;
    if (g_launchSel == 1 && btnsel) blitRGBA(btnsel, sw, sh, bx, by, SMALL_BTN_W, SMALL_BTN_H, 1.0f);
    else if (btn) blitRGBA(btn, sw, sh, bx, by, SMALL_BTN_W, SMALL_BTN_H, 1.0f);
    DrawText(interuiregular18, bx + 95, by + 22, white, "Back");
}

void hblNavigate(menu_s *menu, int move_item, int move_page)
{
    int newEntry;
    if (!menu || menu->nEntries <= 0) return;
    newEntry = menu->curEntry + move_item + move_page * HBL_BUTTONS_PER_PAGE;
    if (newEntry < 0) newEntry = 0;
    if (newEntry >= menu->nEntries) newEntry = menu->nEntries - 1;
    menu->curEntry = newEntry;
}

int hblHitTestEntry(menu_s *menu, int px, int py)
{
    menuEntry_s *me;
    int i, page, row;
    hblRect r;
    if (!menu) return -1;
    for (me = menu->firstEntry, i = 0; me; me = me->next, i++) {
        float scale;
        page = i / HBL_BUTTONS_PER_PAGE;
        row = i % HBL_BUTTONS_PER_PAGE;
        scale = rowScaleFor(page, row, i == menu->curEntry);
        entryRect(page, row, scale, &r);
        if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h)
            return i;
    }
    return -1;
}

int hblHitTestPageArrow(menu_s *menu, int px, int py)
{
    int n;
    int ay = 360 - ARROW_H / 2;
    if (!menu) return -1;
    n = menu->nEntries;
    if (n <= HBL_BUTTONS_PER_PAGE) return -1;
    if (g_listOffset > 0 &&
        px >= 40 && px < 40 + ARROW_W && py >= ay && py < ay + ARROW_H)
        return 0;
    if ((g_listOffset + 1) * HBL_BUTTONS_PER_PAGE < n &&
        px >= 1280 - 40 - ARROW_W && px < 1280 - 40 && py >= ay && py < ay + ARROW_H)
        return 1;
    return -1;
}

int hblLaunchBoxHitButton(int px, int py)
{
    if (!g_launchOpen) return -1;
    if (px >= g_loadRect.x && px < g_loadRect.x + g_loadRect.w &&
        py >= g_loadRect.y && py < g_loadRect.y + g_loadRect.h)
        return 0;
    if (px >= g_backRect.x && px < g_backRect.x + g_backRect.w &&
        py >= g_backRect.y && py < g_backRect.y + g_backRect.h)
        return 1;
    return -1;
}
