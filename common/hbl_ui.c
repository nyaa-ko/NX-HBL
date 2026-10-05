/*
 * Wii U Homebrew Launcher (dimok789) look & feel for the Switch.
 *
 * Every animation in here mirrors the behaviour of libgui as used by
 * HomebrewWindow / HomebrewLaunchWindow / MainWindow / Application:
 *   - linear 35 px/frame page scrolling, selection cleared while scrolling
 *   - GuiButton "grow" effect: scale 100% -> 110% in 4% steps (and back)
 *   - launch box EFFECT_FADE +10 / -10 (alpha / 255) with input disabled
 *     while the effect runs
 *   - GuiParticleImage(500, r 0..30, speed 0.2..0.8) rising bubbles
 *   - Application::fadeOut(): black overlay, +10 per frame before leaving
 *   - button_click sound on every GuiButton click
 */
#include "common.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>

extern uint8_t *folder_icon_large;
extern uint8_t *invalid_icon_large;
extern uint8_t *theme_icon_large;

#define SCREEN_W 1280
#define SCREEN_H 720
#define BTN_SRC_W 782
#define BTN_SRC_H 152
#define BTN_STEP  (BTN_SRC_H + 20)      /* image->getHeight() + 20 (unscaled) */
#define ARROW_W 100
#define ARROW_H 178
#define BOX_W 900
#define BOX_H 610
#define SMALL_BTN_W 252
#define SMALL_BTN_H 64
#define LABEL_MAX_W 350                 /* nameLabel->setMaxWidth(350, SCROLL_HORIZONTAL) */
#define TEXT_SCROLL_INITIAL_DELAY 6
#define TEXT_SCROLL_DELAY 6

typedef struct {
    float x, y;          /* libgui coordinates: origin = screen centre, +y = up */
    float radius, speed, direction, alpha;
} hblParticle;

typedef struct {
    int x, y, w, h;
} hblRect;

enum { BOX_CLOSED = 0, BOX_OPENING, BOX_OPEN, BOX_CLOSING };
enum { FADE_NONE = 0, FADE_LAUNCH, FADE_EXIT };

static hblParticle g_particles[HBL_PARTICLE_COUNT];
static int g_particles_inited = 0;
static u32 g_frame = 0;

static float g_currentLeft = 0.0f;
static float g_targetLeft = 0.0f;
static int g_listOffset = 0;
static int g_motionReady = 0;
static bool g_hasSel = false;
static float g_rowGrow[HBL_BUTTONS_PER_PAGE];
static int g_growPage = -1;

static int g_boxState = BOX_CLOSED;
static menuEntry_s *g_launchEntry = NULL;
static int g_launchSel = -1;            /* -1 none, 0 Load, 1 Back */
static int g_boxAlpha = 0;              /* 0..255 like libgui alpha*255 */
static float g_boxGrow[2] = { 1.0f, 1.0f };
static hblRect g_loadRect, g_backRect;

static int g_fadeMode = FADE_NONE;
static int g_fadeAlpha = 0;
static bool g_exitReady = false;
static menuEntry_s *g_fadeEntry = NULL;

/* ---------------------------------------------------------------- utils */

static float rand01(void) { return (float)rand() / (float)RAND_MAX; }
static float randM11(void) { return rand01() * 2.0f - 1.0f; }

static void playClick(void) { audioPlayClick(); }
static void playPadClick(void)
{
#if HBL_CLICK_ON_PAD_BUTTONS
    audioPlayClick();
#endif
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
    if (!img || dw <= 0 || dh <= 0 || amul <= 0.0f) return;
    for (y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        if (dy + y < 0 || dy + y >= SCREEN_H) continue;
        if (sy >= sh) sy = sh - 1;
        for (x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            const uint8_t *p;
            uint8_t a;
            if (dx + x < 0 || dx + x >= SCREEN_W) continue;
            if (sx >= sw) sx = sw - 1;
            p = img + ((sy * sw) + sx) * 4;
            a = (uint8_t)(p[3] * amul);
            if (a == 0) continue;
            DrawPixel(dx + x, dy + y, MakeColor(p[0], p[1], p[2], a));
        }
    }
}

static void blitRGBSquare(const uint8_t *img, int sw, int sh, int dx, int dy, int dw, int dh, float amul)
{
    int x, y, rr;
    uint8_t a = (uint8_t)(255.0f * amul);
    if (!img || dw <= 0 || dh <= 0 || a == 0) return;
    rr = dw / 7;
    if (rr < 6) rr = 6;
    for (y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        if (dy + y < 0 || dy + y >= SCREEN_H) continue;
        if (sy >= sh) sy = sh - 1;
        for (x = 0; x < dw; x++) {
            const uint8_t *p;
            int sx;
            if (dx + x < 0 || dx + x >= SCREEN_W) continue;
            if (!insideRoundRect(x, y, dw, dh, rr)) continue;
            sx = x * sw / dw;
            if (sx >= sw) sx = sw - 1;
            p = img + ((sy * sw) + sx) * 3;
            DrawPixel(dx + x, dy + y, MakeColor(p[0], p[1], p[2], a));
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

static void drawAssetCentered(AssetId id, int cx, int cy, int bw, int bh, float scale, float amul, hblRect *out)
{
    int sw, sh;
    const uint8_t *img = assetBuf(id, &sw, &sh);
    int w = (int)(bw * scale), h = (int)(bh * scale);
    int x = cx - w / 2, y = cy - h / 2;
    if (out) { out->x = x; out->y = y; out->w = w; out->h = h; }
    if (img) blitRGBA(img, sw, sh, x, y, w, h, amul);
}

static u32 textWidth(u32 font, const char *t)
{
    uint32_t w = 0, h = 0;
    GetTextDimensions(font, t, &w, &h);
    return w;
}

static int fontHalfHeight(u32 font)
{
    switch (font) {
        case interuiregular14: return 10;
        case interuiregular18: return 13;
        case interuimedium20:  return 14;
        case interuimedium30:  return 20;
        default: return 12;
    }
}

/* Draw text vertically centred on cy (libgui ALIGN_MIDDLE). */
static void drawTextMid(u32 font, int x, int cy, color_t c, const char *t)
{
    DrawText(font, x, cy - fontHalfHeight(font), c, t);
}

static void drawTextCentered(u32 font, int cx, int cy, color_t c, const char *t, u32 maxw)
{
    u32 w = textWidth(font, t);
    if (maxw && w > maxw) {
        DrawTextTruncate(font, cx - (int)maxw / 2, cy - fontHalfHeight(font), c, t, maxw, "...");
        return;
    }
    DrawText(font, cx - (int)w / 2, cy - fontHalfHeight(font), c, t);
}

/* GuiText::SCROLL_HORIZONTAL - long labels scroll character by character. */
static void drawTextScroll(u32 font, int x, int cy, color_t c, const char *t, u32 maxw)
{
    char buf[1024];
    int nchars = 0, pos, i;
    const char *p;
    u32 delay;

    if (!t || !t[0]) return;
    if (textWidth(font, t) <= maxw) {
        drawTextMid(font, x, cy, c, t);
        return;
    }
    for (p = t; *p; p++)
        if ((*p & 0xC0) != 0x80) nchars++;

    snprintf(buf, sizeof(buf), "%s    %s", t, t);
    delay = g_frame / TEXT_SCROLL_DELAY;
    if (delay < TEXT_SCROLL_INITIAL_DELAY) pos = 0;
    else pos = (int)((delay - TEXT_SCROLL_INITIAL_DELAY) % (u32)(nchars + 4));

    p = buf;
    for (i = 0; i < pos && *p; i++) {
        p++;
        while (*p && (*p & 0xC0) == 0x80) p++;
    }
    DrawTextTruncate(font, x, cy - fontHalfHeight(font), c, p, maxw, "");
}

static color_t white(float a)
{
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    return MakeColor(255, 255, 255, (uint8_t)(255.0f * a));
}

static float growStep(float cur, float target)
{
    if (cur < target) { cur += HBL_GROW_STEP; if (cur > target) cur = target; }
    else if (cur > target) { cur -= HBL_GROW_STEP; if (cur < target) cur = target; }
    return cur;
}

static const uint8_t *entryIcon(menuEntry_s *me)
{
    if (me->icon_gfx) return me->icon_gfx;
    if (me->type == ENTRY_TYPE_FOLDER) return folder_icon_large;
    if (me->type == ENTRY_TYPE_THEME) return theme_icon_large;
    return invalid_icon_large;
}

/* ---------------------------------------------------------------- state */

void hblResetMotion(void)
{
    int i;
    g_motionReady = 0;
    g_currentLeft = 0.0f;
    g_targetLeft = 0.0f;
    g_listOffset = 0;
    g_growPage = -1;
    g_hasSel = false;
    for (i = 0; i < HBL_BUTTONS_PER_PAGE; i++) g_rowGrow[i] = 1.0f;
}

bool hblHasSelection(void) { return g_hasSel; }

void hblSetSelection(menu_s *menu, int index)
{
    if (!menu || index < 0 || index >= menu->nEntries) return;
    menu->curEntry = index;
    g_hasSel = true;
}

bool hblInputLocked(void)
{
    return g_fadeMode != FADE_NONE || g_boxState == BOX_OPENING || g_boxState == BOX_CLOSING;
}

void hblRequestExit(void)
{
    if (g_fadeMode != FADE_NONE) return;
    g_fadeMode = FADE_EXIT;
    g_fadeAlpha = 0;
    g_exitReady = false;
}

bool hblExitReady(void) { return g_exitReady; }

static void initParticle(hblParticle *p, int spawnAnywhere)
{
    p->x = randM11() * SCREEN_W * 0.5f;
    p->y = spawnAnywhere ? randM11() * SCREEN_H * 0.5f : -SCREEN_H * 0.5f - 30.0f;
    p->alpha = rand01() * 0.6f + 0.05f;
    p->radius = rand01() * (HBL_PARTICLE_MAX_RADIUS - HBL_PARTICLE_MIN_RADIUS) + HBL_PARTICLE_MIN_RADIUS;
    p->speed = rand01() * (HBL_PARTICLE_MAX_SPEED - HBL_PARTICLE_MIN_SPEED) + HBL_PARTICLE_MIN_SPEED;
    p->direction = randM11();
}

void hblUiInit(void)
{
    int i;
    if (g_particles_inited) return;
    srand((unsigned)time(NULL));
    for (i = 0; i < HBL_PARTICLE_COUNT; i++)
        initParticle(&g_particles[i], 1);
    for (i = 0; i < HBL_BUTTONS_PER_PAGE; i++)
        g_rowGrow[i] = 1.0f;
    g_particles_inited = 1;
}

/* ---------------------------------------------------------------- background */

void hblDrawBackground(void)
{
    int x, y;
    /* MainWindow bgImageColor: vertices 0,1 (bottom) = 79,153,239 ; 2,3 (top) = 59,159,223 */
    for (y = 0; y < SCREEN_H; y++) {
        float t = (float)y / (float)(SCREEN_H - 1);   /* 0 = top, 1 = bottom */
        uint8_t r = (uint8_t)(59 + (79 - 59) * t);
        uint8_t g = (uint8_t)(159 + (153 - 159) * t);
        uint8_t b = (uint8_t)(223 + (239 - 223) * t);
        color_t c = MakeColor(r, g, b, 255);
        for (x = 0; x < SCREEN_W; x += 4)
            Draw4PixelsRaw(x, y, c);
    }
}

static void drawCircle(int cx, int cy, float r, uint8_t a)
{
    int y, x, ir = (int)ceilf(r);
    float r2 = r * r;
    color_t c = MakeColor(255, 255, 255, a);
    if (r < 0.5f || a == 0) return;
    for (y = -ir; y <= ir; y++) {
        float fy = (float)y;
        float w2 = r2 - fy * fy;
        int half;
        if (w2 < 0) continue;
        half = (int)sqrtf(w2);
        if (cy + y < 0 || cy + y >= SCREEN_H) continue;
        for (x = -half; x <= half; x++)
            DrawPixel(cx + x, cy + y, c);
    }
}

void hblDrawParticles(void)
{
    int i;
    hblUiInit();
    g_frame++;
    for (i = 0; i < HBL_PARTICLE_COUNT; i++) {
        hblParticle *p = &g_particles[i];

        /* GuiParticleImage::draw out of bounds checks */
        if (p->y > SCREEN_H * 0.5f + 30.0f)
            initParticle(p, 0);
        if (p->x > SCREEN_W * 0.5f + 30.0f || p->x < -SCREEN_W * 0.5f - 30.0f)
            initParticle(p, 0);

        p->direction += randM11() * 0.03f;
        p->x += p->speed * p->direction;
        p->y += p->speed;

        /* libgui circle: radius is in half-pixels of the 1280x720 target */
        drawCircle((int)(SCREEN_W * 0.5f + p->x), (int)(SCREEN_H * 0.5f - p->y),
                   p->radius * 0.5f, (uint8_t)(p->alpha * 255.0f));
    }
}

/* ---------------------------------------------------------------- list */

static int entryCenterY(int row)
{
    /* fYOffset = (h + 20) * 1.5 - (h + 20) * row, libgui +y = up */
    float fYOffset = (float)BTN_STEP * 1.5f - (float)BTN_STEP * (float)row;
    return (int)(SCREEN_H * 0.5f - fYOffset);
}

static float rowScaleFor(int page, int row)
{
    if (page == g_growPage)
        return HBL_IDLE_SCALE * g_rowGrow[row];
    return HBL_IDLE_SCALE;
}

static void entryRect(int page, int row, float scale, hblRect *r)
{
    int dw = (int)(BTN_SRC_W * scale);
    int dh = (int)(BTN_SRC_H * scale);
    int cx = SCREEN_W / 2 + (int)g_currentLeft + page * SCREEN_W;
    int cy = entryCenterY(row);
    r->x = cx - dw / 2;
    r->y = cy - dh / 2;
    r->w = dw;
    r->h = dh;
}

static void drawEntryButton(menuEntry_s *me, int page, int row, int selected)
{
    float scale = rowScaleFor(page, row);
    int sw, sh, iconSize, ix, iy, textX, cy;
    const uint8_t *img, *icon;
    char tmp[1024];
    hblRect r;

    entryRect(page, row, scale, &r);
    if (r.x + r.w < 0 || r.x > SCREEN_W)
        return;

    /* setdrawOverOnlyWhenSelected(true) + setImageOver(selectImg) */
    img = assetBuf(selected ? AssetId_hbl_button_selected : AssetId_hbl_button, &sw, &sh);
    if (img) blitRGBA(img, sw, sh, r.x, r.y, r.w, r.h, 1.0f);

    /* iconImg: ALIGN_LEFT | ALIGN_MIDDLE, x = 60 (scaled with the button).
       Switch icons are square 256x256, Wii U ones 256x96: keep them square. */
    cy = r.y + r.h / 2;
    iconSize = (int)(112.0f * scale / HBL_IDLE_SCALE);
    ix = r.x + (int)(60.0f * scale);
    iy = cy - iconSize / 2;
    icon = entryIcon(me);
    if (icon) blitRGBSquare(icon, 256, 256, ix, iy, iconSize, iconSize, 1.0f);

    textX = ix + iconSize + (int)(36.0f * scale);

    /* nameLabel (y +20) / descriptionLabel (y -20), max width 350, scrolling */
    snprintf(tmp, sizeof(tmp), "%s%s", me->starred ? "\u2605 " : "", me->name);
    drawTextScroll(interuimedium20, textX, cy - (int)(20.0f * scale / HBL_IDLE_SCALE), white(1.0f), tmp, LABEL_MAX_W);
    {
        const char *desc = me->author[0] ? me->author : (me->version[0] ? me->version : "");
        drawTextScroll(interuiregular18, textX, cy + (int)(20.0f * scale / HBL_IDLE_SCALE), white(1.0f), desc, LABEL_MAX_W);
    }
}

static void tickPageMotion(void)
{
    /* HomebrewWindow::draw : linear 35 px per frame */
    if (!g_motionReady) {
        g_currentLeft = g_targetLeft = (float)(-g_listOffset * SCREEN_W);
        g_motionReady = 1;
        return;
    }
    if (g_currentLeft < g_targetLeft) {
        g_currentLeft += HBL_PAGE_STEP_PX;
        if (g_currentLeft > g_targetLeft) g_currentLeft = g_targetLeft;
    } else if (g_currentLeft > g_targetLeft) {
        g_currentLeft -= HBL_PAGE_STEP_PX;
        if (g_currentLeft < g_targetLeft) g_currentLeft = g_targetLeft;
    }
}

static void tickGrow(int selRow)
{
    int i;
    if (g_listOffset != g_growPage) {
        g_growPage = g_listOffset;
        for (i = 0; i < HBL_BUTTONS_PER_PAGE; i++) g_rowGrow[i] = 1.0f;
    }
    for (i = 0; i < HBL_BUTTONS_PER_PAGE; i++) {
        float target = (g_hasSel && i == selRow) ? HBL_GROW_TARGET : 1.0f;
        g_rowGrow[i] = growStep(g_rowGrow[i], target);
    }
}

void hblDrawEntryList(menu_s *menu)
{
    menuEntry_s *me;
    int i, n;

    if (!menu || menu->nEntries <= 0)
        return;

    n = menu->nEntries;
    if (menu->curEntry < 0) menu->curEntry = 0;
    if (menu->curEntry >= n) menu->curEntry = n - 1;

    if (!g_motionReady) {
        /* entering a folder / returning: start on the page holding curEntry */
        g_listOffset = menu->curEntry / HBL_BUTTONS_PER_PAGE;
        g_targetLeft = (float)(-g_listOffset * SCREEN_W);
    }
    tickPageMotion();
    tickGrow(menu->curEntry - g_listOffset * HBL_BUTTONS_PER_PAGE);

    for (me = menu->firstEntry, i = 0; me; me = me->next, i++) {
        int page = i / HBL_BUTTONS_PER_PAGE;
        int row = i % HBL_BUTTONS_PER_PAGE;
        drawEntryButton(me, page, row, g_hasSel && i == menu->curEntry);
    }

    /* arrowLeftButton (x 40, ALIGN_LEFT|MIDDLE) / arrowRightButton (x -40) */
    if (n > HBL_BUTTONS_PER_PAGE) {
        if (g_listOffset > 0)
            drawAssetCentered(AssetId_hbl_arrow_left, 40 + ARROW_W / 2, SCREEN_H / 2, ARROW_W, ARROW_H, 1.0f, 1.0f, NULL);
        if ((g_listOffset + 1) * HBL_BUTTONS_PER_PAGE < n)
            drawAssetCentered(AssetId_hbl_arrow_right, SCREEN_W - 40 - ARROW_W / 2, SCREEN_H / 2, ARROW_W, ARROW_H, 1.0f, 1.0f, NULL);
    }
}

void hblDrawChrome(menu_s *menu, const char *title)
{
    color_t w = white(1.0f);
    (void)title;

#ifdef __SWITCH__
    {
        AppletType at = appletGetAppletType();
        if (at != AppletType_Application && at != AppletType_SystemApplication)
            DrawText(interuiregular14, 28, 18, MakeColor(255, 230, 80, 255), "Applet Mode");
    }
#endif

    if (menu && menu->nEntries <= 0) {
        drawTextCentered(interuimedium20, SCREEN_W / 2, SCREEN_H / 2 - 20, w, "No applications found", 0);
        drawTextCentered(interuiregular18, SCREEN_W / 2, SCREEN_H / 2 + 20, white(0.85f), "Place .nro files in sd:/switch", 0);
    }

    /* miiMakerHintText: ALIGN_BOTTOM|ALIGN_LEFT, (27, 36), size 26 */
    DrawText(interuiregular18, 27, SCREEN_H - 36 - 26, w,
             "\n\uE0E0 Load  \uE0E1 Back  \uE0E3 NetLoader  \uE0E2 Star  \uE0EF Exit");

    /* hblVersionText: ALIGN_BOTTOM|ALIGN_RIGHT, (-30, 30), size 32 */
    {
        const char *v = "\nHomebrew Launcher " HBL_VERSION_NX " by Mottyan";
        u32 tw = textWidth(interuimedium20, v);
        DrawText(interuimedium20, SCREEN_W - 30 - (int)tw, SCREEN_H - 30 - 30, w, v);
    }
}

/* ---------------------------------------------------------------- navigation */

static void setPage(menu_s *menu, int page)
{
    /* OnLeftArrowClick / OnRightArrowClick */
    g_listOffset = page;
    g_targetLeft = (float)(-page * SCREEN_W);
    g_hasSel = false;                     /* clearSelections() while scrolling */
    menu->curEntry = page * HBL_BUTTONS_PER_PAGE;
}

void hblNavigate(menu_s *menu, int move_item, int move_page)
{
    int min, max, idx;
    if (!menu || menu->nEntries <= 0 || hblInputLocked()) return;

    if (move_page) {
        int page = g_listOffset + (move_page < 0 ? -1 : 1);
        int lastPage = (menu->nEntries - 1) / HBL_BUTTONS_PER_PAGE;
        if (page < 0 || page > lastPage) return;   /* arrow button not on screen */
        playClick();                               /* arrow GuiButton click sound */
        setPage(menu, page);
        return;
    }

    if (!move_item) return;

    /* OnUpDownClick: selection is confined to the current page */
    min = g_listOffset * HBL_BUTTONS_PER_PAGE;
    max = min + HBL_BUTTONS_PER_PAGE;
    if (max > menu->nEntries) max = menu->nEntries;
    if (min >= max) return;

    if (!g_hasSel) {
        menu->curEntry = min;
        g_hasSel = true;
        return;
    }
    idx = menu->curEntry;
    if (idx < min || idx >= max) idx = min;
    if (move_item < 0 && idx > min) idx--;
    else if (move_item > 0 && idx < max - 1) idx++;
    menu->curEntry = idx;
}

/* ---------------------------------------------------------------- launch box */

bool hblLaunchBoxIsOpen(void) { return g_boxState != BOX_CLOSED; }

void hblLaunchBoxOpen(menuEntry_s *me)
{
    if (!me || g_boxState != BOX_CLOSED || g_fadeMode != FADE_NONE) return;
    g_launchEntry = me;
    g_boxState = BOX_OPENING;           /* STATE_DISABLED until the fade ends */
    g_launchSel = -1;
    g_boxAlpha = 0;
    g_boxGrow[0] = g_boxGrow[1] = 1.0f;
}

void hblLaunchBoxClose(void)
{
    if (g_boxState == BOX_OPEN || g_boxState == BOX_OPENING)
        g_boxState = BOX_CLOSING;       /* EFFECT_FADE -10 then delete */
}

static void startLaunchFade(menuEntry_s *me)
{
    g_fadeEntry = me;
    g_fadeMode = FADE_LAUNCH;
    g_fadeAlpha = 0;
}

void hblLaunchBoxConfirm(void)
{
    menuEntry_s *me = g_launchEntry;
    if (g_boxState != BOX_OPEN || !me) return;
    if (me->type == ENTRY_TYPE_FILE) {
        startLaunchFade(me);            /* HomebrewLoader -> Application::fadeOut() */
    } else {
        g_boxState = BOX_CLOSED;
        g_launchEntry = NULL;
        launchMenuEntryTask(me);
    }
}

#ifdef __SWITCH__
bool hblLaunchBoxHandleInput(u64 down)
{
    if (g_boxState != BOX_OPEN || g_fadeMode != FADE_NONE) return true;

    /* HomebrewLaunchWindow::OnDpadClick */
    if (down & HidNpadButton_Left) {
        g_launchSel = 0;
        return true;
    }
    if (down & HidNpadButton_Right) {
        g_launchSel = (g_launchSel < 0) ? 0 : 1;
        return true;
    }
    /* HomebrewLaunchWindow::OnButtonClick */
    if (down & HidNpadButton_B) {
        playPadClick();
        hblLaunchBoxClose();
        return true;
    }
    if (down & HidNpadButton_A) {
        if (g_launchSel == 0) { playPadClick(); hblLaunchBoxConfirm(); }
        else if (g_launchSel == 1) { playPadClick(); hblLaunchBoxClose(); }
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
    int x, y, cx;
    menuEntry_s *me = g_launchEntry;
    float a;
    const uint8_t *icon;

    if (g_boxState == BOX_CLOSED || !me) return;

    if (g_boxState == BOX_OPENING) {
        g_boxAlpha += HBL_FADE_IN_STEP;
        if (g_boxAlpha >= 255) { g_boxAlpha = 255; g_boxState = BOX_OPEN; }
    } else if (g_boxState == BOX_CLOSING) {
        g_boxAlpha -= HBL_FADE_OUT_STEP;
        if (g_boxAlpha <= 0) {
            g_boxAlpha = 0;
            g_boxState = BOX_CLOSED;
            g_launchEntry = NULL;
            return;
        }
    }
    a = (float)g_boxAlpha / 255.0f;

    g_boxGrow[0] = growStep(g_boxGrow[0], g_launchSel == 0 ? HBL_GROW_TARGET : 1.0f);
    g_boxGrow[1] = growStep(g_boxGrow[1], g_launchSel == 1 ? HBL_GROW_TARGET : 1.0f);

    /* launchMenuBox.png 900x610, centred, moved up by 30 */
    x = (SCREEN_W - BOX_W) / 2;
    y = (SCREEN_H - BOX_H) / 2 - HBL_LAUNCH_BOX_YOFF;
    cx = x + BOX_W / 2;
    {
        int sw, sh;
        const uint8_t *box = assetBuf(AssetId_hbl_launch_box, &sw, &sh);
        if (box) blitRGBA(box, sw, sh, x, y, BOX_W, BOX_H, a);
    }

    /* titleText size 42, centred at yOffset = h/2 - 75 */
    drawTextCentered(interuimedium30, cx, y + 75, white(a), me->name, BOX_W - 100);

    /* iconImage ALIGN_LEFT|MIDDLE x 100 */
    icon = entryIcon(me);
    if (icon) blitRGBSquare(icon, 256, 256, x + 100, y + 97, 112, 112, a);

    /* Version: / Author: at (width - 500), values at +100, 30 px apart */
    drawTextMid(interuiregular18, x + 400, y + 125, white(a), "Version:");
    DrawTextTruncate(interuiregular18, x + 500, y + 125 - fontHalfHeight(interuiregular18), white(a),
                     me->version[0] ? me->version : "", 350, "...");
    drawTextMid(interuiregular18, x + 400, y + 155, white(a), "Author:");
    DrawTextTruncate(interuiregular18, x + 500, y + 155 - fontHalfHeight(interuiregular18), white(a),
                     me->author[0] ? me->author : "", 350, "...");

    /* descriptionText size 28, ALIGN_LEFT|ALIGN_TOP at (100, 250), max width - 200 */
    {
        const char *p = me->path;
        const char *sd = strstr(p, "/switch/");
        DrawTextTruncate(interuiregular14, x + 100, y + 250, white(a), sd ? sd + 1 : p, BOX_W - 200, "...");
    }

    /* loadBtn (-200, -310) / backBtn (200, -310) from the box centre, grow effect */
    drawAssetCentered(g_launchSel == 0 ? AssetId_hbl_small_button_selected : AssetId_hbl_small_button,
                      cx - 200, y + BOX_H / 2 + 310, SMALL_BTN_W, SMALL_BTN_H, g_boxGrow[0], a, &g_loadRect);
    drawTextCentered(interuimedium20, cx - 200, y + BOX_H / 2 + 310, white(a), "Load", 0);

    drawAssetCentered(g_launchSel == 1 ? AssetId_hbl_small_button_selected : AssetId_hbl_small_button,
                      cx + 200, y + BOX_H / 2 + 310, SMALL_BTN_W, SMALL_BTN_H, g_boxGrow[1], a, &g_backRect);
    drawTextCentered(interuimedium20, cx + 200, y + BOX_H / 2 + 310, white(a), "Back", 0);
}

/* ---------------------------------------------------------------- fade out */

void hblDrawScreenFade(void)
{
    int x, y;
    color_t c;
    if (g_fadeMode == FADE_NONE) return;

    /* Application::fadeOut(): for (i = 0; i < 255; i += 10) */
    c = MakeColor(0, 0, 0, (uint8_t)(g_fadeAlpha > 255 ? 255 : g_fadeAlpha));
    for (y = 0; y < SCREEN_H; y++)
        for (x = 0; x < SCREEN_W; x++)
            DrawPixel(x, y, c);

    if (g_fadeAlpha < 255) {
        g_fadeAlpha += HBL_EXIT_FADE_STEP;
        if (g_fadeAlpha >= 255) g_fadeAlpha = 255;
        return;
    }

    if (g_fadeMode == FADE_EXIT) {
        g_exitReady = true;
        return;
    }

    /* FADE_LAUNCH: screen is black now, hand over to the loader */
    {
        menuEntry_s *me = g_fadeEntry;
        g_fadeEntry = NULL;
        g_fadeMode = FADE_NONE;
        g_fadeAlpha = 0;
        g_boxState = BOX_CLOSED;
        g_launchEntry = NULL;
        if (me) launchMenuEntryTask(me);
    }
}

/* ---------------------------------------------------------------- touch */

int hblHitTestEntry(menu_s *menu, int px, int py)
{
    menuEntry_s *me;
    int i;
    hblRect r;
    if (!menu) return -1;
    for (me = menu->firstEntry, i = 0; me; me = me->next, i++) {
        int page = i / HBL_BUTTONS_PER_PAGE;
        int row = i % HBL_BUTTONS_PER_PAGE;
        entryRect(page, row, rowScaleFor(page, row), &r);
        if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h)
            return i;
    }
    return -1;
}

int hblHitTestPageArrow(menu_s *menu, int px, int py)
{
    int n;
    int ay = SCREEN_H / 2 - ARROW_H / 2;
    if (!menu) return -1;
    n = menu->nEntries;
    if (n <= HBL_BUTTONS_PER_PAGE) return -1;
    if (g_listOffset > 0 &&
        px >= 40 && px < 40 + ARROW_W && py >= ay && py < ay + ARROW_H)
        return 0;
    if ((g_listOffset + 1) * HBL_BUTTONS_PER_PAGE < n &&
        px >= SCREEN_W - 40 - ARROW_W && px < SCREEN_W - 40 && py >= ay && py < ay + ARROW_H)
        return 1;
    return -1;
}

int hblLaunchBoxHitButton(int px, int py)
{
    if (g_boxState != BOX_OPEN) return -1;
    if (px >= g_loadRect.x && px < g_loadRect.x + g_loadRect.w &&
        py >= g_loadRect.y && py < g_loadRect.y + g_loadRect.h)
        return 0;
    if (px >= g_backRect.x && px < g_backRect.x + g_backRect.w &&
        py >= g_backRect.y && py < g_backRect.y + g_backRect.h)
        return 1;
    return -1;
}
