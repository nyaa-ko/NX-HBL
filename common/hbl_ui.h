#pragma once

#define HBL_VERSION_NX "v1.5-nx"
#define HBL_BUTTONS_PER_PAGE 4

void hblUiInit(void);
void hblDrawBackground(void);
void hblDrawParticles(void);
void hblDrawEntryList(menu_s *menu);
void hblDrawFooter(void);
void hblDrawLaunchBox(void);

bool hblLaunchBoxIsOpen(void);
void hblLaunchBoxOpen(menuEntry_s *me);
void hblLaunchBoxClose(void);
void hblLaunchBoxConfirm(void);
bool hblLaunchBoxHandleInput(u64 down);

void hblNavigate(menu_s *menu, int move_item, int move_page);
