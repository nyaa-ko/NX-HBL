#pragma once

#define HBL_VERSION_NX "v1.5-nx"
#define HBL_BUTTONS_PER_PAGE 4

/*
 * ============================================================
 *  Wii U Homebrew Launcher (dimok789) 再現パラメータ
 *  Values copied from the original libgui / HBL sources.
 *  All values are per-frame (60 fps, same as Wii U).
 * ============================================================
 *
 * HBL_PAGE_STEP_PX     HomebrewWindow::draw : currentLeftPosition +/- 35 per frame (linear)
 * HBL_IDLE_SCALE       homebrewButtons[i].image->setScale(0.9f)
 * HBL_GROW_TARGET      GuiButton::setEffectGrow() -> EFFECT_SCALE target 110 (%)
 * HBL_GROW_STEP        GuiButton::setEffectGrow() -> EFFECT_SCALE amount 4 (% per frame)
 * HBL_FADE_IN_STEP     launchBox->setEffect(EFFECT_FADE, 10, 255)
 * HBL_FADE_OUT_STEP    element->setEffect(EFFECT_FADE, -10, 0)
 * HBL_EXIT_FADE_STEP   Application::fadeOut() : i += 10 (black overlay)
 * HBL_LAUNCH_BOX_YOFF  launchBox->setPosition(0, 30)  (libgui Y is up -> 30 px higher)
 * Particles            GuiParticleImage(w, h, 500, 0.0f, 30.0f, 0.2f, 0.8f)
 * HBL_BGM_VOLUME       bgMusic->SetVolume(50)
 * HBL_CLICK_VOLUME     button_click.mp3 at default volume
 *
 * HBL_CLICK_ON_PAD_BUTTONS
 *   0 = 100% original behaviour: the click sound only plays for touch / arrow
 *       buttons (on Wii U pressing (A) on a selected entry was silent).
 *   1 = also play the click when (A)/(B) activate a button.
 */
#define HBL_PAGE_STEP_PX         85.0f
#define HBL_IDLE_SCALE           0.90f
#define HBL_GROW_TARGET          1.10f
#define HBL_GROW_STEP            0.04f
#define HBL_FADE_IN_STEP         50
#define HBL_FADE_OUT_STEP        50
#define HBL_EXIT_FADE_STEP       50
#define HBL_LAUNCH_BOX_YOFF      30
#define HBL_PARTICLE_COUNT       500
#define HBL_PARTICLE_MIN_RADIUS  0.0f
#define HBL_PARTICLE_MAX_RADIUS  30.0f
#define HBL_PARTICLE_MIN_SPEED   2.0f
#define HBL_PARTICLE_MAX_SPEED   4.0f
#define HBL_BGM_VOLUME           0.50f
#define HBL_CLICK_VOLUME         1.00f
#define HBL_CLICK_ON_PAD_BUTTONS 1

/* audio (nx_main/nx_audio.c) */
void audioPlayClick(void);

void hblUiInit(void);
void hblDrawBackground(void);
void hblDrawParticles(void);
void hblDrawEntryList(menu_s *menu);
void hblDrawChrome(menu_s *menu, const char *title);
void hblDrawLaunchBox(void);
void hblDrawScreenFade(void);

bool hblLaunchBoxIsOpen(void);
void hblLaunchBoxOpen(menuEntry_s *me);
void hblLaunchBoxClose(void);
void hblLaunchBoxConfirm(void);
bool hblLaunchBoxHandleInput(u64 down);

/* selection model of the original: nothing is selected until a d-pad press */
bool hblHasSelection(void);
void hblSetSelection(menu_s *menu, int index);
void hblNavigate(menu_s *menu, int move_item, int move_page);

/* screen fade-out (Application::fadeOut) */
bool hblInputLocked(void);
void hblRequestExit(void);
bool hblExitReady(void);

/* Touch hit-testing in HBL layout coordinates (1280x720). */
int  hblHitTestEntry(menu_s *menu, int px, int py);       /* entry index, or -1 */
int  hblHitTestPageArrow(menu_s *menu, int px, int py);   /* 0=left 1=right -1=none */
int  hblLaunchBoxHitButton(int px, int py);               /* 0=Load 1=Back -1=none */
void hblResetMotion(void);
