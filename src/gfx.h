#pragma once
#include <Arduino.h>

constexpr int SCR_W = 128;
constexpr int SCR_H = 64;

enum Color : uint8_t { C_BLACK = 0, C_WHITE = 1, C_INVERT = 2 };
enum FontId : uint8_t {
  F_SMALL,  // built-in 6x8
  F_BOLD,   // FreeSansBold 9pt
  F_BIG,    // FreeSansBold 12pt
  F_HUGE,   // FreeSansBold 18pt
};

void gfxBegin();
void beginFrame();
void endFrame();

float clampf(float v, float lo, float hi);
float lerpf(float a, float b, float t);
float easeOutCubic(float t);
float easeInOutCubic(float t);
float easeOutBack(float t);
// Frame-rate independent exponential approach toward target.
float approach(float cur, float target, float rate, float dt);

// Drawing state used by every primitive and text call.
void setColor(Color c);
void setFont(FontId f);

// All primitives below are safe with off-screen / negative coordinates.
void pixel(int x, int y);
void line(float x0, float y0, float x1, float y1);
void hline(int x, int y, int w);
void vline(int x, int y, int h);
void fillRect(int x, int y, int w, int h);
void frameRect(int x, int y, int w, int h);
void fillDisc(float cx, float cy, float r);
void circle(float cx, float cy, float r);
void arc(float cx, float cy, float r, float a0, float a1);
void fillTri(float x0, float y0, float x1, float y1, float x2, float y2);
void fillWedge(float cx, float cy, float rIn, float rOut, float a0, float a1);
// 1-bit bitmap (MSB-first rows, PROGMEM). Only the first `cols` columns are drawn (-1 = all).
void bitmap(int x, int y, const uint8_t *bmp, int w, int h, int cols = -1);
bool bitmapBit(const uint8_t *bmp, int w, int bx, int by);

// Text. y is the vertical middle of the text (digit/cap height).
void text(int x, int y, const char *s);
void textCentered(int x, int y, const char *s);
void textRight(int x, int y, const char *s);
int textWidth(const char *s);  // advance width in the current font
// Small-font text running top-to-bottom, rotated 90 degrees; rightX = right edge of the glyphs.
void textVertical(int rightX, int yCenter, const char *s);

void fmtMoney(char *buf, size_t n, int32_t v);  // "$1234"
