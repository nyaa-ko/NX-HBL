#pragma once

#define HBL_VERSION_NX "v1.5-nx"
#define HBL_BUTTONS_PER_PAGE 4

/*
 * ============================================================
 *  アニメーション速度をいじる場所はここだけ
 *  Edit animation speed / easing here (the only knobs you need)
 * ============================================================
 *
 * 値は「1フレームあたり」(Switch は約 60fps) です。
 * Values are per-frame at ~60 fps.
 *
 * HBL_PAGE_LERP
 *   ページ送りが目標位置へ追いつく割合。
 *   小さいほどゆっくり滑らか / 大きいほどキビキビ。
 *   目安: 0.08 遅め,  0.14 標準,  0.22 速め,  0.32 かなり速め
 *
 * HBL_PAGE_SNAP_PX
 *   これより近づいたらページ位置をスナップ（振動防止）
 *
 * HBL_SELECT_LERP
 *   選択中バナーの拡大が追いつく割合。PAGE_LERP と同じ感覚。
 *
 * HBL_SELECT_SCALE / HBL_IDLE_SCALE
 *   選択中 / 非選択のバナー倍率。SELECT を 1.00 にすると拡大なし。
 *
 * HBL_LAUNCH_FADE
 *   起動確認ダイアログのフェードイン速度 (0.06 遅め / 0.18 速め)
 *
 * HBL_PARTICLE_SPEED
 *   背景パーティクルの速度倍率 (0.5 遅め / 1.0 標準 / 2.0 速め)
 *
 * HBL_BGM_VOLUME
 *   BGM 音量 (0.0〜1.0)。Wii U 版は約 0.50。
 */
#define HBL_PAGE_LERP        0.14f
#define HBL_PAGE_SNAP_PX     0.8f
#define HBL_SELECT_LERP      0.20f
#define HBL_SELECT_SCALE     0.96f
#define HBL_IDLE_SCALE       0.90f
#define HBL_LAUNCH_FADE      0.10f
#define HBL_PARTICLE_SPEED   1.00f
#define HBL_BGM_VOLUME       0.50f

void hblUiInit(void);
void hblDrawBackground(void);
void hblDrawParticles(void);
void hblDrawEntryList(menu_s *menu);
void hblDrawChrome(menu_s *menu, const char *title);
void hblDrawLaunchBox(void);

bool hblLaunchBoxIsOpen(void);
void hblLaunchBoxOpen(menuEntry_s *me);
void hblLaunchBoxClose(void);
void hblLaunchBoxConfirm(void);
bool hblLaunchBoxHandleInput(u64 down);

void hblNavigate(menu_s *menu, int move_item, int move_page);

/* Touch hit-testing in HBL layout coordinates (1280x720). */
int  hblHitTestEntry(menu_s *menu, int px, int py);       /* entry index, or -1 */
int  hblHitTestPageArrow(menu_s *menu, int px, int py);   /* 0=left 1=right -1=none */
int  hblLaunchBoxHitButton(int px, int py);               /* 0=Load 1=Back -1=none */
void hblResetMotion(void);
