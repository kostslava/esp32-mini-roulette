#include <math.h>
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "screens.h"

// ================= splash =================

namespace {
float splashT = 0;
bool splashLeaving = false;
const char *const TITLE = "ROULETTE";
}  // namespace

void splashEnter() {
  splashT = 0;
  splashLeaving = false;
}

void splashUpdate(float dt) {
  splashT += dt;
  if (splashLeaving) return;
  if (splashT > 2.2f || anyConfirmPressed() || btnPressed(BTN_BACK)) {
    splashLeaving = true;
    navigate(SCR_MENU);
  }
}

void splashDraw(int ox, int oy) {
  setFont(F_BOLD);
  int total = textWidth(TITLE);
  int x = 64 - total / 2 + ox;
  char c[2] = {0, 0};
  for (int i = 0; TITLE[i]; i++) {
    c[0] = TITLE[i];
    float e = easeOutBack((splashT - 0.15f - i * 0.07f) / 0.45f);
    int y = lroundf(lerpf(-14, 34, e)) + oy;
    if (splashT - 0.15f - i * 0.07f > 0) text(x, y, c);
    x += textWidth(c);
  }

  setFont(F_SMALL);
  float mt = easeOutCubic((splashT - 0.9f) / 0.4f);
  if (mt > 0) textCentered(64 + ox, lroundf(lerpf(-8, 14, mt)) + oy, "MINI");

  float lt = easeOutCubic((splashT - 1.1f) / 0.5f);
  int half = lroundf(46 * lt);
  hline(64 - half + ox, 47 + oy, half * 2);

  setFont(F_SMALL);
  if (splashT > 1.5f) textCentered(64 + ox, 56 + oy, "GOOD LUCK");
}

// ================= menu =================

namespace {
float menuRot = 0;
float menuVel = 0.5f;
float menuT = 0;
bool menuLeaving = false;
}  // namespace

float menuWheelRot() { return menuRot; }

void menuEnter() { menuLeaving = false; }

void menuUpdate(float dt) {
  menuT += dt;
  int d = encDelta();
  if (d) menuVel = clampf(menuVel + d * 1.4f, -14, 14);
  menuVel = approach(menuVel, 0.5f, 1.2f, dt);
  menuRot += menuVel * dt;

  if (!menuLeaving && anyConfirmPressed()) {
    menuLeaving = true;
    navigate(balance <= 0 ? SCR_AD : SCR_BETS);
  }
}

void menuDrawText(int ox) {
  char buf[16];
  setFont(F_SMALL);
  fmtMoney(buf, sizeof(buf), balance);
  textRight(125 + ox, 7, buf);

  setFont(F_BOLD);
  float bob = sinf(menuT * 3.0f) * 1.5f;
  textCentered(96 + ox, lroundf(31 + bob), "PLAY?");

  int half = lroundf(16 + 8 * sinf(menuT * 4.0f));
  hline(96 - half + ox, 44, half * 2);

  setFont(F_SMALL);
  if (fmodf(menuT, 1.2f) < 0.8f) textCentered(96 + ox, 56, balance > 0 ? "PRESS OK" : "BROKE!");
}

void menuDraw(int ox, int oy) {
  float cx = MENU_WHEEL_CX + ox, cy = 32 + oy;
  drawNumberWheel(cx, cy, MENU_WHEEL_R, menuRot);

  float px = cx + MENU_WHEEL_R;
  setColor(C_BLACK);
  fillTri(px - 1, cy, px + 8, cy - 7, px + 8, cy + 7);
  setColor(C_WHITE);
  fillTri(px + 1, cy, px + 7, cy - 4, px + 7, cy + 4);

  // The menu only ever slides horizontally, so the text ignores oy.
  menuDrawText(ox);
}

// ================= menu <-> bets morph =================

void drawMenuBetsMorph(float e) {
  betsDrawTable(lroundf(lerpf(-100, 0, e)), 0);
  menuDrawText(lroundf(lerpf(0, 110, e)));

  float cx = lerpf(MENU_WHEEL_CX, CHIP_WHEEL_CX, e);
  float R = lerpf(MENU_WHEEL_R, CHIP_WHEEL_R, e);
  setColor(C_BLACK);
  fillDisc(cx, 32, R + 1);
  setColor(C_WHITE);
  if (e < 0.5f) drawNumberWheel(cx, 32, R, menuRot + e * 5.0f);
  else {
    float pos = betsChipPos() - (1 - e) * 5.0f;
    drawChipWheel(cx, 32, R, pos, lroundf(pos));
  }

  betsDrawStrip(lroundf(lerpf(14, 0, e)));
}
