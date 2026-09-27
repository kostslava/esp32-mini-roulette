#include "gfx.h"
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Wire.h>
#include <math.h>
#include "config.h"

#if OLED_SH1106
#include <Adafruit_SH110X.h>
#define PX_WHITE SH110X_WHITE
#define PX_BLACK SH110X_BLACK
#define PX_INVERSE SH110X_INVERSE
#else
#include <Adafruit_SSD1306.h>
#define PX_WHITE SSD1306_WHITE
#define PX_BLACK SSD1306_BLACK
#define PX_INVERSE SSD1306_INVERSE
#endif

namespace {
#if OLED_SH1106
Adafruit_SH1106G display(SCR_W, SCR_H, &Wire, -1, I2C_CLOCK_HZ, I2C_CLOCK_HZ);
#else
Adafruit_SSD1306 display(SCR_W, SCR_H, &Wire, -1, I2C_CLOCK_HZ, I2C_CLOCK_HZ);
#endif
uint16_t color = PX_WHITE;
int fontYOff = 0;  // cursor y = text middle + fontYOff
}  // namespace

void gfxBegin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(I2C_CLOCK_HZ);
#if OLED_SH1106
  bool ok = display.begin(OLED_I2C_ADDR, true);
#else
  bool ok = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR, true, false);
#endif
  if (!ok) Serial.println("OLED init failed");
  display.setTextWrap(false);
  display.clearDisplay();
  display.display();
  setFont(F_SMALL);
}

void beginFrame() { display.clearDisplay(); }
void endFrame() { display.display(); }

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }

float easeOutCubic(float t) {
  t = clampf(t, 0, 1) - 1;
  return t * t * t + 1;
}

float easeInOutCubic(float t) {
  t = clampf(t, 0, 1);
  return t < 0.5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2;
}

float easeOutBack(float t) {
  t = clampf(t, 0, 1);
  const float c1 = 1.70158f, c3 = c1 + 1;
  float u = t - 1;
  return 1 + c3 * u * u * u + c1 * u * u;
}

float approach(float cur, float target, float rate, float dt) {
  return target + (cur - target) * expf(-rate * dt);
}

void setColor(Color c) {
  color = c == C_BLACK ? PX_BLACK : c == C_WHITE ? PX_WHITE : PX_INVERSE;
}

void setFont(FontId f) {
  switch (f) {
    case F_SMALL: display.setFont(nullptr); break;
    case F_BOLD: display.setFont(&FreeSansBold9pt7b); break;
    case F_BIG: display.setFont(&FreeSansBold12pt7b); break;
    case F_HUGE: display.setFont(&FreeSansBold18pt7b); break;
  }
  display.setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds("0", 0, 0, &x1, &y1, &w, &h);
  fontYOff = f == F_SMALL ? -3 : lroundf(-y1 / 2.0f);
}

// ---------- clipped primitives ----------

namespace {
enum { IN = 0, LEFT = 1, RIGHT = 2, TOP = 4, BOTTOM = 8 };
constexpr float XMAX = SCR_W - 1, YMAX = SCR_H - 1;

int outCode(float x, float y) {
  int c = IN;
  if (x < 0) c |= LEFT;
  else if (x > XMAX) c |= RIGHT;
  if (y < 0) c |= TOP;
  else if (y > YMAX) c |= BOTTOM;
  return c;
}
}  // namespace

void pixel(int x, int y) {
  if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H) display.drawPixel(x, y, color);
}

void line(float x0, float y0, float x1, float y1) {
  int c0 = outCode(x0, y0), c1 = outCode(x1, y1);
  while (true) {
    if (!(c0 | c1)) break;
    if (c0 & c1) return;
    int c = c0 ? c0 : c1;
    float x, y;
    if (c & BOTTOM) {
      x = x0 + (x1 - x0) * (YMAX - y0) / (y1 - y0);
      y = YMAX;
    } else if (c & TOP) {
      x = x0 + (x1 - x0) * (0 - y0) / (y1 - y0);
      y = 0;
    } else if (c & RIGHT) {
      y = y0 + (y1 - y0) * (XMAX - x0) / (x1 - x0);
      x = XMAX;
    } else {
      y = y0 + (y1 - y0) * (0 - x0) / (x1 - x0);
      x = 0;
    }
    if (c == c0) {
      x0 = x; y0 = y; c0 = outCode(x0, y0);
    } else {
      x1 = x; y1 = y; c1 = outCode(x1, y1);
    }
  }
  display.drawLine(lroundf(x0), lroundf(y0), lroundf(x1), lroundf(y1), color);
}

void hline(int x, int y, int w) {
  if (y < 0 || y >= SCR_H || w <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (x + w > SCR_W) w = SCR_W - x;
  if (w > 0) display.drawFastHLine(x, y, w, color);
}

void vline(int x, int y, int h) {
  if (x < 0 || x >= SCR_W || h <= 0) return;
  if (y < 0) { h += y; y = 0; }
  if (y + h > SCR_H) h = SCR_H - y;
  if (h > 0) display.drawFastVLine(x, y, h, color);
}

void fillRect(int x, int y, int w, int h) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > SCR_W) w = SCR_W - x;
  if (y + h > SCR_H) h = SCR_H - y;
  if (w > 0 && h > 0) display.fillRect(x, y, w, h, color);
}

void frameRect(int x, int y, int w, int h) {
  hline(x, y, w);
  hline(x, y + h - 1, w);
  vline(x, y + 1, h - 2);
  vline(x + w - 1, y + 1, h - 2);
}

void fillDisc(float cx, float cy, float r) {
  int y0 = (int)ceilf(cy - r), y1 = (int)floorf(cy + r);
  if (y0 < 0) y0 = 0;
  if (y1 > SCR_H - 1) y1 = SCR_H - 1;
  for (int y = y0; y <= y1; y++) {
    float dy = y - cy;
    float dx = sqrtf(fmaxf(0, r * r - dy * dy));
    int xa = lroundf(cx - dx), xb = lroundf(cx + dx);
    hline(xa, y, xb - xa + 1);
  }
}

void arc(float cx, float cy, float r, float a0, float a1) {
  int n = (int)ceilf(fabsf(a1 - a0) * r / 4.0f);
  if (n < 2) n = 2;
  float px = cx + r * cosf(a0), py = cy + r * sinf(a0);
  for (int i = 1; i <= n; i++) {
    float a = a0 + (a1 - a0) * i / n;
    float x = cx + r * cosf(a), y = cy + r * sinf(a);
    line(px, py, x, y);
    px = x; py = y;
  }
}

void circle(float cx, float cy, float r) { arc(cx, cy, r, 0, 2 * PI); }

void fillTri(float x0, float y0, float x1, float y1, float x2, float y2) {
  display.fillTriangle(lroundf(x0), lroundf(y0), lroundf(x1), lroundf(y1), lroundf(x2), lroundf(y2), color);
}

void fillWedge(float cx, float cy, float rIn, float rOut, float a0, float a1) {
  int n = (int)ceilf(fabsf(a1 - a0) * rOut / 6.0f);
  if (n < 1) n = 1;
  float c = cosf(a0), s = sinf(a0);
  float ix = cx + rIn * c, iy = cy + rIn * s, ox = cx + rOut * c, oy = cy + rOut * s;
  for (int i = 1; i <= n; i++) {
    float a = a0 + (a1 - a0) * i / n;
    c = cosf(a); s = sinf(a);
    float ix2 = cx + rIn * c, iy2 = cy + rIn * s, ox2 = cx + rOut * c, oy2 = cy + rOut * s;
    fillTri(ix, iy, ox, oy, ox2, oy2);
    fillTri(ix, iy, ox2, oy2, ix2, iy2);
    ix = ix2; iy = iy2; ox = ox2; oy = oy2;
  }
}

bool bitmapBit(const uint8_t *bmp, int w, int bx, int by) {
  return pgm_read_byte(bmp + by * ((w + 7) / 8) + bx / 8) & (0x80 >> (bx & 7));
}

void bitmap(int x, int y, const uint8_t *bmp, int w, int h, int cols) {
  if (cols < 0 || cols > w) cols = w;
  for (int by = 0; by < h; by++)
    for (int bx = 0; bx < cols; bx++)
      if (bitmapBit(bmp, w, bx, by)) pixel(x + bx, y + by);
}

// ---------- text ----------

int textWidth(const char *s) {
  display.setCursor(0, -200);
  display.print(s);
  return display.getCursorX();
}

void text(int x, int y, const char *s) {
  if (x >= SCR_W || y < -20 || y > SCR_H + 20) return;
  display.setTextColor(color);
  display.setCursor(x, y + fontYOff);
  display.print(s);
}

void textCentered(int x, int y, const char *s) { text(x - textWidth(s) / 2, y, s); }
void textRight(int x, int y, const char *s) { text(x - textWidth(s), y, s); }

void textVertical(int rightX, int yCenter, const char *s) {
  setFont(F_SMALL);
  int w = textWidth(s);
  // Rotation 1 maps logical (lx, ly) to physical (SCR_W - 1 - ly, lx).
  display.setRotation(1);
  display.setTextColor(color);
  display.setCursor(yCenter - w / 2, SCR_W - 1 - rightX);
  display.print(s);
  display.setRotation(0);
}

void fmtMoney(char *buf, size_t n, int32_t v) {
  if (v < 0) snprintf(buf, n, "-$%ld", (long)-v);
  else snprintf(buf, n, "$%ld", (long)v);
}
