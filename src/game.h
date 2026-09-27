#pragma once
#include <Arduino.h>

// ---------- wheel ----------
constexpr int POCKETS = 9;  // 0..8
extern const uint8_t WHEEL_ORDER[POCKETS];

enum PocketColor : uint8_t { PC_ZERO, PC_WHITE, PC_BLACK };
inline PocketColor pocketColor(int n) { return n == 0 ? PC_ZERO : ((n & 1) ? PC_WHITE : PC_BLACK); }

// One pocket of a wheel: filled for white, outlined for black, double-ruled for zero.
void drawPocket(float cx, float cy, float rIn, float rOut, float a0, float a1, int number, float textR);
// Main-menu style number wheel. rot = angle of pocket 0's leading edge.
void drawNumberWheel(float cx, float cy, float R, float rot);
// Bets-screen chip selector. pos = fractional chip index sitting under the pointer (angle PI);
// sel = slot drawn highlighted.
void drawChipWheel(float cx, float cy, float R, float pos, int sel);

// ---------- chips ----------
constexpr int CHIP_COUNT = 6;
extern const uint8_t CHIP_PCT[CHIP_COUNT];
extern const char *const CHIP_LABEL[CHIP_COUNT];
inline int chipMod(int i) { return ((i % CHIP_COUNT) + CHIP_COUNT) % CHIP_COUNT; }
int32_t chipValue(int chipIdx);  // dollar value of a chip, based on the pre-spin balance

// ---------- bets ----------
enum BetSpot : uint8_t {
  SPOT_LOW = POCKETS,  // 1-4
  SPOT_HIGH,           // 5-8
  SPOT_BLACK,     // even
  SPOT_WHITE,     // odd
  SPOT_COUNT
};

extern int32_t balance;
extern int32_t bets[SPOT_COUNT];

int32_t totalBet();
int32_t payoutFor(int number);  // total returned (stake included) for this result
bool placeBet(int spot, int32_t amount);
bool undoBet(int *spotOut, int32_t *amountOut);
void refundAllBets();
void clearBetsLost();

// ---------- history (newest first) ----------
constexpr int HISTORY_MAX = 8;
extern uint8_t history[HISTORY_MAX];
extern int historyCount;
extern uint32_t historyVersion;  // bumped on every push
void pushHistory(int number);

void loadBalance();  // also loads history
void saveBalance();
