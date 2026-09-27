#include <math.h>
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "screens.h"

namespace {

struct Rect {
  int x, y, w, h;
};

// Layout (y): status 0-7 | 0 + grid 9-39 (1-4, 5-8, then the 1-4 / 5-8 bets) | BLACK/WHITE 42-51 | history 54-63
constexpr int GRID_Y = 9, BW_Y = 42, HIST_Y = 54;
constexpr int TABLE_R = 84;  // right edge of the table

Rect spotRect(int s) {
  if (s == 0) return {0, GRID_Y, 13, 31};
  if (s < POCKETS) {
    int r = (s - 1) / 4, c = (s - 1) % 4;
    return {12 + c * 18, GRID_Y + r * 10, 19, 11};
  }
  switch (s) {
    case SPOT_LOW: return {12, GRID_Y + 20, 37, 11};
    case SPOT_HIGH: return {48, GRID_Y + 20, 37, 11};
    case SPOT_BLACK: return {0, BW_Y, 43, 10};
    default: return {42, BW_Y, 43, 10};
  }
}

const char *spotLabel(int s, char *buf) {
  switch (s) {
    case SPOT_LOW: return "1-4";
    case SPOT_HIGH: return "5-8";
    case SPOT_BLACK: return "BLACK";
    case SPOT_WHITE: return "WHITE";
    default: snprintf(buf, 4, "%d", s); return buf;
  }
}

void markerPos(int s, float *x, float *y) {
  Rect r = spotRect(s);
  *x = r.x + r.w - 3;
  *y = r.y + 2;
}

int cursor = 1;
int chipIdx = 2;  // 10%
float chipPos = 2;
float curX, curY, curW, curH;
float pop = 0;
float shake = 0;
float t = 0;
// Encoder button: tap = place bet (or accept chip), hold = open the chip selector.
constexpr uint32_t HOLD_MS = 300;
bool chipMode = false;
bool encArmed = false;   // press started on this screen
bool holdFired = false;  // this press already opened the selector
int chipBefore = 0;      // restored if the selector is cancelled with back
float wob = 0, wobVel = 0;  // chip wheel rotation wobble, in slots
float popVel = 0;
bool backArmed = false;
bool leaving = false;

// History row: a few recent results, newest on the left, slides in after each spin.
constexpr int HIST_SHOWN = 5;
constexpr int HIST_W = 13, HIST_GAP = 4;
uint32_t histSeen = 0;
float histT = 1;  // 0..1 slide-in progress

struct Fly {
  bool on;
  bool toTable;
  int spot;
  float x0, y0, x1, y1, t;
};
constexpr int FLY_MAX = 6;
Fly flies[FLY_MAX];
constexpr float FLY_DUR = 0.3f;

const char *toastMsg = nullptr;
float toastT = 0;
constexpr float TOAST_DUR = 1.3f;

float chipWheelX() { return CHIP_WHEEL_CX - pop; }

void pointerPos(float *x, float *y) {
  *x = chipWheelX() - CHIP_WHEEL_R + 10;
  *y = 32;
}

void toast(const char *msg) {
  toastMsg = msg;
  toastT = TOAST_DUR;
}

void launchFly(int spot, bool toTable) {
  for (auto &f : flies) {
    if (f.on) continue;
    float px, py, mx, my;
    pointerPos(&px, &py);
    markerPos(spot, &mx, &my);
    f = {true, toTable, spot, toTable ? px : mx, toTable ? py : my, toTable ? mx : px, toTable ? my : py, 0};
    return;
  }
}

bool spotHasLandedChip(int s) {
  if (bets[s] <= 0) return false;
  // Hide the marker while a chip for this spot is still in the air.
  for (auto &f : flies)
    if (f.on && f.toTable && f.spot == s && f.t < FLY_DUR) return false;
  return true;
}

void doPlace() {
  if (balance <= 0) {
    toast("NO CASH LEFT");
    shake = 1;
    return;
  }
  int32_t amount = chipValue(chipIdx);
  if (placeBet(cursor, amount)) launchFly(cursor, true);
  else {
    toast("NOT ENOUGH");
    shake = 1;
  }
}

void spring(float &x, float &v, float target, float k, float damp, float dt) {
  v += (k * (target - x) - damp * v) * dt;
  x += v * dt;
}

void enterChipMode() {
  chipMode = true;
  holdFired = true;
  chipBefore = chipIdx;
  wobVel = 10;
  popVel = 90;
  toast("TURN TO PICK");
}

void exitChipMode(bool accept) {
  chipMode = false;
  if (!accept) chipIdx = chipBefore;
  wobVel = -4;
  popVel = -50;
}

void snapCursor() {
  Rect r = spotRect(cursor);
  curX = r.x; curY = r.y; curW = r.w; curH = r.h;
}

// Back / OK handling on the table. Returns true if we are leaving the screen.
bool updateTableButtons() {
  if (btnPressed(BTN_BACK)) backArmed = true;
  if (backArmed && btnHeldMs(BTN_BACK) > 650) {
    backArmed = false;
    leaving = true;
    refundAllBets();
    navigate(SCR_MENU);
    return true;
  }
  if (btnReleased(BTN_BACK) && backArmed) {
    backArmed = false;
    int spot;
    int32_t amount;
    if (undoBet(&spot, &amount)) {
      launchFly(spot, false);
    } else {
      leaving = true;
      navigate(SCR_MENU);
      return true;
    }
  }

  if (btnPressed(BTN_OK)) {
    if (totalBet() > 0) {
      leaving = true;
      navigate(SCR_SPIN);
      return true;
    }
    toast("PLACE A BET!");
    shake = 1;
  }
  return false;
}

void animate(float dt) {
  Rect r = spotRect(cursor);
  curX = approach(curX, r.x, 24, dt);
  curY = approach(curY, r.y, 24, dt);
  curW = approach(curW, r.w, 24, dt);
  curH = approach(curH, r.h, 24, dt);
  chipPos = approach(chipPos, chipIdx, 16, dt);
  shake = fmaxf(0, shake - dt * 3.5f);
  if (toastT > 0) toastT -= dt;
  histT = fminf(1, histT + dt / 0.5f);
  for (auto &f : flies)
    if (f.on && (f.t += dt) >= FLY_DUR + 0.05f) f.on = false;
}

void drawChip(float x, float y) {
  fillDisc(x, y, 3);
  setColor(C_BLACK);
  fillDisc(x, y, 1);
  setColor(C_WHITE);
}

// A result as a mini pocket: white = filled, black = outlined, zero = double outline.
void drawMiniPocket(int x, int y, int n) {
  char buf[3];
  snprintf(buf, sizeof(buf), "%d", n);
  int cx = x + HIST_W / 2 + 1, cy = y + 5;
  switch (pocketColor(n)) {
    case PC_WHITE:
      fillRect(x + 1, y, HIST_W - 2, 10);
      fillRect(x, y + 1, HIST_W, 8);
      setColor(C_BLACK);
      textCentered(cx, cy, buf);
      setColor(C_WHITE);
      break;
    case PC_BLACK:
      frameRect(x, y, HIST_W, 10);
      textCentered(cx, cy, buf);
      break;
    case PC_ZERO:
      frameRect(x, y, HIST_W, 10);
      frameRect(x + 2, y + 2, HIST_W - 4, 6);
      textCentered(cx, cy, buf);
      break;
  }
}

void drawHistory(int ox, int oy) {
  setFont(F_SMALL);
  int y = HIST_Y + oy;
  if (historyCount == 0) {
    textCentered(TABLE_R / 2 + ox, y + 5, "NO SPINS YET");
    return;
  }
  // While sliding, everything starts one slot to the left and the old last entry slides off right.
  float shift = (1 - easeOutCubic(histT)) * (HIST_W + HIST_GAP);
  int shown = histT < 1 ? HIST_SHOWN + 1 : HIST_SHOWN;
  for (int i = 0; i < shown && i < historyCount; i++) {
    int x = lroundf(i * (HIST_W + HIST_GAP) - shift) + ox;
    if (x > TABLE_R) continue;
    drawMiniPocket(x, y, history[i]);
  }
}

}  // namespace

float betsChipPos() { return chipPos; }

void betsEnter() {
  for (auto &f : flies) f.on = false;
  toastT = 0;
  encArmed = holdFired = backArmed = false;
  chipMode = false;
  leaving = false;
  chipPos = chipIdx;
  pop = popVel = 0;
  wob = wobVel = 0;
  shake = 0;
  snapCursor();
  histT = (historyVersion != histSeen && historyCount > 0) ? 0 : 1;
  histSeen = historyVersion;
}

void betsUpdate(float dt) {
  t += dt;
  if (leaving) return;

  int d = encDelta();
  if (btnPressed(BTN_ENC)) {
    encArmed = true;
    holdFired = false;
  }
  // Turning while the button is down is ignored, so a slightly wobbly click still counts as a tap.
  if (d && !btnDown(BTN_ENC)) {
    if (chipMode) chipIdx += d;
    else cursor = ((cursor + d) % SPOT_COUNT + SPOT_COUNT) % SPOT_COUNT;
  }
  // One continuous motion: the wheel turns down while held; released early it springs back
  // (a tap), held to HOLD_MS it flings forward into the selector.
  bool holding = encArmed && !chipMode && !holdFired && btnDown(BTN_ENC);
  if (holding) {
    float e = easeOutCubic(btnHeldMs(BTN_ENC) / (float)HOLD_MS);
    float tw = -0.4f * e, tp = -6 * e;
    wobVel = (tw - wob) / fmaxf(dt, 0.001f);
    popVel = (tp - pop) / fmaxf(dt, 0.001f);
    wob = tw;
    pop = tp;
    if (btnHeldMs(BTN_ENC) >= HOLD_MS) enterChipMode();
  }
  if (btnReleased(BTN_ENC)) {
    if (encArmed && !holdFired) {
      if (chipMode) exitChipMode(true);
      else doPlace();
    }
    encArmed = false;
  }

  if (chipMode) {
    if (btnPressed(BTN_OK)) exitChipMode(true);
    else if (btnPressed(BTN_BACK)) exitChipMode(false);
    backArmed = false;
  } else {
    if (updateTableButtons()) return;
  }

  if (!holding || chipMode) {
    spring(wob, wobVel, 0, 260, 16, dt);
    spring(pop, popVel, chipMode ? 10 : 0, 260, 17, dt);
  }

  animate(dt);
}

void betsDrawTable(int ox, int oy) {
  char buf[16], lbl[4];

  // status bar
  setFont(F_SMALL);
  drawChip(3 + ox, 4 + oy);
  fmtMoney(buf, sizeof(buf), chipValue(chipIdx));
  text(9 + ox, 4 + oy, buf);
  int chipEnd = 9 + textWidth(buf) + 4;
  char bet[20];
  fmtMoney(buf, sizeof(buf), totalBet());
  snprintf(bet, sizeof(bet), "BET %s", buf);
  textRight(TABLE_R + 1 + ox, 4 + oy, chipEnd + textWidth(bet) > TABLE_R + 1 ? buf : bet);

  for (int s = 0; s < SPOT_COUNT; s++) {
    Rect r = spotRect(s);
    int x = r.x + ox, y = r.y + oy;
    frameRect(x, y, r.w, r.h);
    textCentered(x + r.w / 2 + 1, y + r.h / 2, spotLabel(s, lbl));
    if (spotHasLandedChip(s)) fillRect(x + r.w - 4, y + 1, 3, 3);
  }

  drawHistory(ox, oy);

  if (chipMode) return;
  float sx = sinf(t * 70) * 2.5f * shake;
  setColor(C_INVERT);
  fillRect(lroundf(curX + sx) + 1 + ox, lroundf(curY) + 1 + oy, lroundf(curW) - 2, lroundf(curH) - 2);
  setColor(C_WHITE);
}

void betsDrawStrip(int ox) {
  char buf[16];
  int x = 118 + ox;
  setColor(C_BLACK);
  fillRect(x, 0, SCR_W - x, SCR_H);
  setColor(C_WHITE);
  line(x, 0, x, SCR_H - 1);

  fmtMoney(buf, sizeof(buf), balance);
  if (x + 2 < SCR_W) textVertical(x + 8, 32, buf);
}

void betsDraw(int ox, int oy) {
  betsDrawTable(ox, oy);

  float cx = chipWheelX() + ox, cy = 32 + oy;
  setColor(C_BLACK);
  fillDisc(cx, cy, CHIP_WHEEL_R + 1);
  setColor(C_WHITE);
  drawChipWheel(cx, cy, CHIP_WHEEL_R, chipPos + wob, lroundf(chipPos));

  float px = cx - CHIP_WHEEL_R;
  setColor(C_BLACK);
  fillTri(px + 2, cy, px - 5, cy - 6, px - 5, cy + 6);
  setColor(C_WHITE);
  if (!chipMode || fmodf(t, 0.5f) < 0.3f) fillTri(px + 1, cy, px - 4, cy - 4, px - 4, cy + 4);

  for (auto &f : flies) {
    if (!f.on) continue;
    float e = easeInOutCubic(f.t / FLY_DUR);
    float x = lerpf(f.x0, f.x1, e) + ox;
    float y = lerpf(f.y0, f.y1, e) - sinf(e * PI) * 10 + oy;
    setColor(C_BLACK);
    fillDisc(x, y, 4);
    setColor(C_WHITE);
    drawChip(x, y);
  }

  betsDrawStrip(ox);

  if (toastT > 0 && toastMsg) {
    float in = easeOutCubic((TOAST_DUR - toastT) / 0.15f);
    float out = easeOutCubic(toastT / 0.15f);
    float e = fminf(in, out);
    setFont(F_SMALL);
    int w = textWidth(toastMsg) + 10, h = 11;
    int x = 43 - w / 2 + ox;
    int y = lroundf(lerpf(SCR_H + 2, 26, e)) + oy;
    setColor(C_BLACK);
    fillRect(x - 2, y - 2, w + 4, h + 4);
    setColor(C_WHITE);
    fillRect(x, y, w, h);
    setColor(C_BLACK);
    textCentered(x + w / 2, y + h / 2, toastMsg);
    setColor(C_WHITE);
  }
}
