#include "nx_touch.h"

#define TAP_MOVEMENT_GAP 20
#define VERTICAL_SWIPE_HORIZONTAL_PLAY 250
#define VERTICAL_SWIPE_MINIMUM_DISTANCE 220
#define HORIZONTAL_SWIPE_VERTICAL_PLAY 250
#define HORIZONTAL_SWIPE_MINIMUM_DISTANCE 180

#define distance(x1, y1, x2, y2) (int) sqrt(((x2 - x1) * (x2 - x1)) + ((y2 - y1) * (y2 - y1)))

struct touchInfo_s touchInfo;

void touchInit(void) {
    touchInfo.gestureInProgress = false;
    touchInfo.isTap = true;
    touchInfo.initMenuXPos = 0;
    touchInfo.initMenuIndex = 0;
    touchInfo.lastSlideSpeed = 0;
    hidInitializeTouchScreen();
}

static menuEntry_s *entryAt(menu_s *menu, int index) {
    menuEntry_s *me;
    int i;
    for (i = 0, me = menu->firstEntry; me; me = me->next, i++) {
        if (i == index) return me;
    }
    return NULL;
}

void handleTouch(menu_s* menu) {
    HidTouchScreenState touch = {0};
    hidGetTouchScreenStates(&touch, 1);

    if (touch.count == 1 && !touchInfo.gestureInProgress) {
        touchInfo.gestureInProgress = true;
        touchInfo.firstTouch = touch.touches[0];
        touchInfo.prevTouch = touch.touches[0];
        touchInfo.isTap = true;
        touchInfo.initMenuXPos = menu->xPos;
        touchInfo.initMenuIndex = menu->curEntry;
        touchInfo.lastSlideSpeed = 0;
        menu->slideSpeed = 0;
    }
    else if (touch.count >= 1 && touchInfo.gestureInProgress) {
        touchInfo.lastSlideSpeed = ((int)(touch.touches[0].x - touchInfo.prevTouch.x));
        touchInfo.prevTouch = touch.touches[0];
        if (touchInfo.isTap && (abs(touchInfo.firstTouch.x - touch.touches[0].x) > TAP_MOVEMENT_GAP || abs(touchInfo.firstTouch.y - touch.touches[0].y) > TAP_MOVEMENT_GAP)) {
            touchInfo.isTap = false;
        }
    }
    else if (touchInfo.gestureInProgress) {
        int x1 = touchInfo.firstTouch.x;
        int y1 = touchInfo.firstTouch.y;
        int x2 = touchInfo.prevTouch.x;
        int y2 = touchInfo.prevTouch.y;
        bool netloader_active = menuIsNetloaderActive();

        if (menuIsMsgBoxOpen() && !netloader_active) {
            if (touchInfo.isTap) menuCloseMsgBox();
        } else if (hblLaunchBoxIsOpen() && touchInfo.isTap && !netloader_active) {
            int hit = hblLaunchBoxHitButton(x1, y1);
            if (hit == 0) hblLaunchBoxConfirm();
            else if (hit == 1) hblLaunchBoxClose();
            else hblLaunchBoxClose();
        } else if (touchInfo.isTap && !netloader_active) {
            int arrow = hblHitTestPageArrow(menu, x1, y1);
            int idx = hblHitTestEntry(menu, x1, y1);
            if (arrow == 0) {
                hblNavigate(menu, 0, -1);
            } else if (arrow == 1) {
                hblNavigate(menu, 0, 1);
            } else if (idx >= 0) {
                menuEntry_s *me;
                if (idx != menu->curEntry) {
                    menu->curEntry = idx;
                } else {
                    me = entryAt(menu, idx);
                    if (me) {
                        if (me->type == ENTRY_TYPE_FILE)
                            hblLaunchBoxOpen(me);
                        else
                            launchMenuEntryTask(me);
                    }
                }
            }
        }
        else if (abs(x1 - x2) < VERTICAL_SWIPE_HORIZONTAL_PLAY && distance(x1, y1, x2, y2) > VERTICAL_SWIPE_MINIMUM_DISTANCE) {
            if (y1 - y2 > 0)
                hblNavigate(menu, -1, 0);
            else
                hblNavigate(menu, 1, 0);
        }
        else if (abs(y1 - y2) < HORIZONTAL_SWIPE_VERTICAL_PLAY && distance(x1, y1, x2, y2) > HORIZONTAL_SWIPE_MINIMUM_DISTANCE) {
            if (x1 - x2 > 0)
                hblNavigate(menu, 0, 1);
            else
                hblNavigate(menu, 0, -1);
        }

        touchInfo.gestureInProgress = false;
    }
}
