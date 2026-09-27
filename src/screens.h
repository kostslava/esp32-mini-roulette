#pragma once

enum ScreenId : uint8_t { SCR_SPLASH, SCR_MENU, SCR_BETS, SCR_SPIN, SCR_AD };

// Starts an animated transition to another screen (implemented in main.cpp).
void navigate(ScreenId to);

void splashEnter();
void splashUpdate(float dt);
void splashDraw(int ox, int oy);

void menuEnter();
void menuUpdate(float dt);
void menuDraw(int ox, int oy);
void menuDrawText(int ox);
float menuWheelRot();

void betsEnter();
void betsUpdate(float dt);
void betsDraw(int ox, int oy);
void betsDrawTable(int ox, int oy);
void betsDrawStrip(int ox);
float betsChipPos();

void spinEnter();
void spinUpdate(float dt);
void spinDraw(int ox, int oy);

void adEnter();
void adUpdate(float dt);
void adDraw(int ox, int oy);

// Menu <-> bets morph. e = 0 shows the menu, e = 1 shows the bets table.
void drawMenuBetsMorph(float e);

// Wheel geometry shared by the menu, the morph and the bets screen.
constexpr float MENU_WHEEL_CX = 14, MENU_WHEEL_R = 44;
constexpr float CHIP_WHEEL_CX = 146, CHIP_WHEEL_R = 56;
