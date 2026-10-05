#include "common.h"
#include <stdlib.h>

extern uint8_t *folder_icon_large;
extern uint8_t *invalid_icon_large;
extern uint8_t *theme_icon_large;

#define PARTICLE_COUNT 500
#define BTN_SRC_W 782
#define BTN_SRC_H 152
#define BTN_SCALE 0.90f
#define ARROW_W 100
#define ARROW_H 178
#define BOX_W 900
#define BOX_H 610
#define SMALL_BTN_W 252
#define SMALL_BTN_H 64

typedef struct {
    float x, y, vx, vy, size, alpha;
} hblParticle;

static hblParticle g_particles[PARTICLE_COUNT];
static int g_particles_inited = 0;
static float g_currentLeft = 0.0f;
static float g_targetLeft = 0.0f;
static int g_listOffset = 0;

static int g_launchOpen = 0;
static menuEntry_s *g_launchEntry = NULL;
static int g_launchSel = 0; /* 0 = Load, 1 = Back */
static float g_launchAlpha = 0.0f;

static int btnW(void) { return (int)(BTN_SRC_W * BTN_SCALE); }
static int btnH(void) { return (int)(BTN_SRC_H * BTN_SCALE); }

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

static void blitRGB(const uint8_t *img, int sw, int sh, int dx, int dy, int dw, int dh)
{
    int x, y;
    if (!img || dw <= 0 || dh <= 0) return;
    for (y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        if (sy >= sh) sy = sh - 1;
        for (x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            if (sx >= sw) sx = sw - 1;
            const uint8_t *p = img + ((sy * sw) + sx) * 3;
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

void hblUiInit(void)
{
    int i;
    if (g_particles_inited) return;
    for (i = 0; i < PARTICLE_COUNT; i++) {
        g_particles[i].x = (float)(rand() % 1280);
        g_particles[i].y = (float)(rand() % 720);
        g_particles[i].vx = ((rand() % 100) - 50) / 90.0f;
        g_particles[i].vy = -0.15f - (rand() % 100) / 180.0f;
        g_particles[i].size = 1.0f + (rand() % 4) * 0.6f;
        g_particles[i].alpha = 70.0f + (rand() % 140);
    }
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
        p->x += p->vx;
        p->y += p->vy;
        if (p->x < -4) p->x = 1284;
        if (p->x > 1284) p->x = -4;
        if (p->y < -4) p->y = 724;
        if (p->y > 724) p->y = -4;
        s = (int)p->size;
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

static int entryCenterY(int row)
{
    int h = btnH();
    float step = (float)(h + 20);
    float fYOffset = step * 1.5f - step * (float)row; /* Wii U Y-up from center */
    return (int)(360.0f - fYOffset);
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
    int w = btnW();
    int h = btnH();
    float scale = selected ? 0.96f : BTN_SCALE;
    int dw = (int)(BTN_SRC_W * scale);
    int dh = (int)(BTN_SRC_H * scale);
    int cx = 640 + (int)g_currentLeft + page * 1280;
    int x = cx - dw / 2;
    int cy = entryCenterY(row);
    int y = cy - dh / 2;
    const uint8_t *icon;
    char tmp[1024];
    int textX, textY;
    color_t white = MakeColor(255, 255, 255, 255);
    color_t nameCol = MakeColor(255, 255, 255, 255);

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

    /* HBL icon slot: 256x96 at x+60, vertically centered */
    if (icon) {
        int iw = 256, ih = 96;
        int ix = x + (int)(60 * scale / BTN_SCALE);
        int iy = y + (dh - ih) / 2;
        blitRGB(icon, 256, 256, ix, iy, iw, ih);
    }

    textX = x + (int)((256 + 80) * scale / BTN_SCALE);
    textY = y + dh / 2 - 22;
    memset(tmp, 0, sizeof(tmp));
    snprintf(tmp, sizeof(tmp) - 1, "%s%s", me->starred ? "★ " : "", me->name);
    DrawTextTruncate(interuiregular18, textX, textY, nameCol, tmp, 350, "...");

    {
        const char *desc = me->author[0] ? me->author : "";
        DrawTextTruncate(interuiregular14, textX, textY + 28, white, desc, 350, "...");
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
    if (wantedOffset != g_listOffset) {
        g_listOffset = wantedOffset;
        g_targetLeft = (float)(-g_listOffset * 1280);
    }
    if (g_currentLeft < g_targetLeft) {
        g_currentLeft += 35.0f;
        if (g_currentLeft > g_targetLeft) g_currentLeft = g_targetLeft;
    } else if (g_currentLeft > g_targetLeft) {
        g_currentLeft -= 35.0f;
        if (g_currentLeft < g_targetLeft) g_currentLeft = g_targetLeft;
    }

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

void hblDrawFooter(void)
{
    color_t white = MakeColor(255, 255, 255, 255);
    DrawText(interuiregular14, 27, 720 - 36, white, "Press \uE0EF to exit    \uE0E0 Load    \uE0E1 Back    \uE0E3 NetLoader");
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
    char line[256];

    if (!g_launchOpen || !me) return;

    if (g_launchAlpha < 1.0f) {
        g_launchAlpha += 0.08f;
        if (g_launchAlpha > 1.0f) g_launchAlpha = 1.0f;
    }

    /* dim */
    for (y = 0; y < 720; y++)
        for (x = 0; x < 1280; x++)
            DrawPixel(x, y, MakeColor(0, 0, 0, (uint8_t)(90 * g_launchAlpha)));

    x = (1280 - BOX_W) / 2;
    y = (720 - BOX_H) / 2 + 16;
    box = assetBuf(AssetId_hbl_launch_box, &sw, &sh);
    if (box) blitRGBA(box, sw, sh, x, y, BOX_W, BOX_H, g_launchAlpha);

    DrawTextTruncate(interuimedium30, x + 80, y + 40, white, me->name, BOX_W - 160, "...");

    if (me->icon_gfx)
        blitRGB(me->icon_gfx, 256, 256, x + 100, y + 100, 256, 96);

    snprintf(line, sizeof(line), "Version:");
    DrawText(interuiregular18, x + BOX_W - 420, y + 110, white, line);
    DrawTextTruncate(interuiregular18, x + BOX_W - 300, y + 110, white, me->version[0] ? me->version : "-", 260, "...");

    DrawText(interuiregular18, x + BOX_W - 420, y + 145, white, "Author:");
    DrawTextTruncate(interuiregular18, x + BOX_W - 300, y + 145, white, me->author[0] ? me->author : "-", 260, "...");

    DrawTextTruncate(interuiregular14, x + 100, y + 230, white,
                     me->type == ENTRY_TYPE_FOLDER ? "Open this folder." : "Load this homebrew application.",
                     BOX_W - 200, "...");

    btn = assetBuf(AssetId_hbl_small_button, &sw, &sh);
    btnsel = assetBuf(AssetId_hbl_small_button_selected, &sw, &sh);
    bx = x + BOX_W / 2 - 200 - SMALL_BTN_W / 2;
    by = y + BOX_H - 90;
    if (g_launchSel == 0 && btnsel) blitRGBA(btnsel, sw, sh, bx, by, SMALL_BTN_W, SMALL_BTN_H, 1.0f);
    else if (btn) blitRGBA(btn, sw, sh, bx, by, SMALL_BTN_W, SMALL_BTN_H, 1.0f);
    DrawText(interuiregular18, bx + 95, by + 22, white, "Load");

    bx = x + BOX_W / 2 + 200 - SMALL_BTN_W / 2;
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
