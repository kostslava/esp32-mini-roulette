#include "game.h"
#include <Preferences.h>
#include <math.h>
#include "config.h"
#include "gfx.h"
#include "pontune_mark.h"

// Alternates white (odd) / black (even) around the wheel, with 0 between 6 and 5.
const uint8_t WHEEL_ORDER[POCKETS] = {0, 5, 2, 7, 4, 1, 8, 3, 6};

const uint8_t CHIP_PCT[CHIP_COUNT] = {1, 5, 10, 25, 50, 100};
const char *const CHIP_LABEL[CHIP_COUNT] = {"1%", "5%", "10%", "25%", "50%", "ALL"};

int32_t balance = START_BALANCE;
int32_t bets[SPOT_COUNT] = {0};
uint8_t history[HISTORY_MAX];
int historyCount = 0;
uint32_t historyVersion = 0;

namespace {
struct UndoEntry {
  uint8_t spot;
  int32_t amount;
};
constexpr int UNDO_MAX = 64;
UndoEntry undoStack[UNDO_MAX];
int undoTop = 0;

Preferences prefs;

void drawHub(float cx, float cy, float r) {
  fillDisc(cx, cy, r);
  setColor(C_BLACK);
  circle(cx, cy, r - 2);
  bitmap(lroundf(cx) - PONTUNE_MARK_W / 2, lroundf(cy) - PONTUNE_MARK_H / 2, PONTUNE_MARK, PONTUNE_MARK_W,
         PONTUNE_MARK_H);
  setColor(C_WHITE);
}
}  // namespace

// ---------- wheel drawing ----------

void drawPocket(float cx, float cy, float rIn, float rOut, float a0, float a1, int number, float textR) {
  float am = (a0 + a1) * 0.5f;
  int tx = lroundf(cx + textR * cosf(am));
  int ty = lroundf(cy + textR * sinf(am));
  char buf[4];
  snprintf(buf, sizeof(buf), "%d", number);

  switch (pocketColor(number)) {
    case PC_WHITE:
      fillWedge(cx, cy, rIn, rOut, a0, a1);
      setColor(C_BLACK);
      textCentered(tx, ty, buf);
      setColor(C_WHITE);
      break;
    case PC_BLACK:
      textCentered(tx, ty, buf);
      break;
    case PC_ZERO: {
      float inset = (rOut - rIn) * 0.12f + 1;
      float ai = (a1 - a0) * 0.08f;
      arc(cx, cy, rIn + inset, a0 + ai, a1 - ai);
      arc(cx, cy, rOut - inset, a0 + ai, a1 - ai);
      textCentered(tx, ty, buf);
      break;
    }
  }
}

void drawNumberWheel(float cx, float cy, float R, float rot) {
  const float step = 2 * PI / POCKETS;
  float rIn = R * 0.66f;
  float textR = (R + rIn) * 0.5f;
  float hubR = rIn * 0.55f;

  setFont(F_SMALL);
  for (int i = 0; i < POCKETS; i++) {
    float a0 = rot + i * step;
    drawPocket(cx, cy, rIn, R, a0, a0 + step, WHEEL_ORDER[i], textR);
    line(cx + rIn * cosf(a0), cy + rIn * sinf(a0), cx + R * cosf(a0), cy + R * sinf(a0));
  }
  circle(cx, cy, R);
  circle(cx, cy, rIn);

  for (int k = 0; k < 4; k++) {
    float a = -rot * 0.5f + k * (PI / 2);
    line(cx + hubR * cosf(a), cy + hubR * sinf(a), cx + rIn * cosf(a), cy + rIn * sinf(a));
  }
  drawHub(cx, cy, hubR);
}

void drawChipWheel(float cx, float cy, float R, float pos, int sel) {
  const int slots = 12;  // chips repeat twice around the wheel
  const float step = 2 * PI / slots;
  float rIn = R - 20;
  float textR = R - 10;
  int base = (int)floorf(pos);

  setFont(F_SMALL);
  for (int s = base - 5; s <= base + 6; s++) {
    float ac = PI - (s - pos) * step;
    float a0 = ac - step / 2, a1 = ac + step / 2;
    int tx = lroundf(cx + textR * cosf(ac));
    int ty = lroundf(cy + textR * sinf(ac));
    const char *label = CHIP_LABEL[chipMod(s)];
    if (s == sel) {
      fillWedge(cx, cy, rIn, R, a0, a1);
      setColor(C_BLACK);
      textCentered(tx, ty, label);
      setColor(C_WHITE);
    } else {
      textCentered(tx, ty, label);
    }
    line(cx + rIn * cosf(a0), cy + rIn * sinf(a0), cx + R * cosf(a0), cy + R * sinf(a0));
  }
  circle(cx, cy, R);
  circle(cx, cy, rIn);
  drawHub(cx, cy, rIn * 0.55f);
}

// ---------- chips & bets ----------

int32_t chipValue(int chipIdx) {
  if (balance <= 0) return 0;
  int pct = CHIP_PCT[chipMod(chipIdx)];
  if (pct >= 100) return balance;
  // Percentages are of the pre-spin balance, so chips stay the same size while betting.
  int32_t base = balance + totalBet();
  int32_t v = (int32_t)((int64_t)base * pct / 100);
  return v < 1 ? 1 : v;
}

int32_t totalBet() {
  int32_t t = 0;
  for (int i = 0; i < SPOT_COUNT; i++) t += bets[i];
  return t;
}

int32_t payoutFor(int n) {
  int32_t r = bets[n] * 8;
  if (n >= 1 && n <= 4) r += bets[SPOT_LOW] * 2;
  if (n >= 5) r += bets[SPOT_HIGH] * 2;
  if (n != 0) r += bets[(n & 1) ? SPOT_WHITE : SPOT_BLACK] * 2;
  return r;
}

bool placeBet(int spot, int32_t amount) {
  if (amount <= 0 || amount > balance) return false;
  balance -= amount;
  bets[spot] += amount;
  if (undoTop < UNDO_MAX) undoStack[undoTop++] = {(uint8_t)spot, amount};
  return true;
}

bool undoBet(int *spotOut, int32_t *amountOut) {
  if (undoTop == 0) return false;
  UndoEntry e = undoStack[--undoTop];
  bets[e.spot] -= e.amount;
  balance += e.amount;
  *spotOut = e.spot;
  *amountOut = e.amount;
  return true;
}

void refundAllBets() {
  balance += totalBet();
  clearBetsLost();
}

void clearBetsLost() {
  for (int i = 0; i < SPOT_COUNT; i++) bets[i] = 0;
  undoTop = 0;
}

void pushHistory(int number) {
  memmove(history + 1, history, HISTORY_MAX - 1);
  history[0] = number;
  if (historyCount < HISTORY_MAX) historyCount++;
  historyVersion++;
  prefs.putBytes("hist", history, historyCount);
}

void loadBalance() {
  prefs.begin("goober", false);
  balance = prefs.getInt("bal", START_BALANCE);
  if (balance < 0) balance = 0;
  historyCount = prefs.getBytes("hist", history, HISTORY_MAX);
  for (int i = 0; i < historyCount; i++)
    if (history[i] >= POCKETS) historyCount = i;
}

void saveBalance() { prefs.putInt("bal", balance); }
