#include <math.h>
#include "config.h"
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "pontune_logo.h"
#include "screens.h"

namespace {

const char *const BRAND = "pontune.app";
constexpr float AD_S = AD_DURATION_MS / 1000.0f;
constexpr float REWARD_S = 1.8f;

float t = 0;
bool rewarded = false;
bool leaving = false;

struct Spark {
  int8_t x, y;
  float ph;
};
const Spark SPARKS[] = {{10, 30, 0.0f}, {117, 32, 1.7f}, {22, 45, 3.1f}, {106, 45, 4.4f}, {40, 9, 2.3f}};

void marchingBorder(int ox, int oy, float phase) {
  int p = (int)phase;
  int i = 0;
  auto dot = [&](int x, int y) {
    if (((i++ + p) % 6) < 3) {
      x += ox;
      y += oy;
      pixel(x, y);
    }
  };
  for (int x = 0; x < SCR_W; x++) dot(x, 0);
  for (int y = 1; y < SCR_H; y++) dot(SCR_W - 1, y);
  for (int x = SCR_W - 2; x >= 0; x--) dot(x, SCR_H - 1);
  for (int y = SCR_H - 2; y > 0; y--) dot(0, y);
}

void spark(int x, int y, float s) {
  int r = lroundf(s * 3);
  if (r <= 0) return;
  line(x - r, y, x + r, y);
  line(x, y - r, x, y + r);
}

void drawAd(int ox, int oy) {
  marchingBorder(ox, oy, t * 30);

  setFont(F_SMALL);
  fillRect(3 + ox, 3 + oy, 15, 10);
  setColor(C_BLACK);
  text(5 + ox, 8 + oy, "AD");
  setColor(C_WHITE);

  float left = fmaxf(0, AD_S - t);
  float cx = 117 + ox, cy = 10 + oy;
  circle(cx, cy, 6);
  arc(cx, cy, 8, -PI / 2, -PI / 2 + 2 * PI * fminf(1, t / AD_S));
  char n[4];
  snprintf(n, sizeof(n), "%d", (int)ceilf(left));
  setFont(F_SMALL);
  textCentered(lroundf(cx) + 1, lroundf(cy), n);

  // Logo: wipes in from the left, then bobs with a diagonal shine sweeping across it.
  int lx = 64 - PONTUNE_LOGO_W / 2 + ox;
  int ly = lroundf(17 + sinf(t * 3) * 1.5f) + oy;
  int cols = lroundf(PONTUNE_LOGO_W * easeOutCubic(t / 0.6f));
  bitmap(lx, ly, PONTUNE_LOGO, PONTUNE_LOGO_W, PONTUNE_LOGO_H, cols);
  float sweep = fmodf(t, 1.6f) / 0.7f;
  if (t > 0.6f && sweep < 1) {
    int sx = lroundf(lerpf(-PONTUNE_LOGO_H, PONTUNE_LOGO_W, sweep));
    setColor(C_BLACK);
    for (int by = 0; by < PONTUNE_LOGO_H; by++)
      for (int k = 0; k < 3; k++) {
        int bx = sx + (PONTUNE_LOGO_H - by) + k;
        if (bx >= 0 && bx < PONTUNE_LOGO_W && bitmapBit(PONTUNE_LOGO, PONTUNE_LOGO_W, bx, by)) pixel(lx + bx, ly + by);
      }
    setColor(C_WHITE);
  }

  for (auto &s : SPARKS) spark(s.x + ox, s.y + oy, fmaxf(0, sinf(t * 4 + s.ph)));

  setFont(F_SMALL);
  if (t > 0.7f) textCentered(64 + ox, 45 + oy, BRAND);

  frameRect(12 + ox, 53 + oy, 104, 6);
  fillRect(14 + ox, 55 + oy, lroundf(100 * fminf(1, t / AD_S)), 2);
}

void drawReward(int ox, int oy) {
  float rt = t - AD_S;
  float e = easeOutBack(rt / 0.5f);

  float coinY = lerpf(-12, 18, e) + oy;
  float squash = fabsf(sinf(rt * 6)) * 2;
  fillDisc(64 + ox, coinY, 9 - squash * 0.3f);
  setColor(C_BLACK);
  circle(64 + ox, coinY, 6);
  setFont(F_SMALL);
  textCentered(65 + ox, lroundf(coinY), "$");
  setColor(C_WHITE);

  char buf[16], money[12];
  fmtMoney(money, sizeof(money), START_BALANCE);
  snprintf(buf, sizeof(buf), "+%s", money);
  setFont(F_BOLD);
  textCentered(64 + ox, lroundf(lerpf(80, 42, easeOutBack((rt - 0.15f) / 0.5f))) + oy, buf);

  setFont(F_SMALL);
  if (rt > 0.6f) textCentered(64 + ox, 57 + oy, "THANKS FOR WATCHING");

  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4 + rt * 1.5f;
    float r = 14 + 4 * sinf(rt * 5 + i);
    if (rt > 0.3f) spark(64 + ox + cosf(a) * r, coinY + sinf(a) * r, 0.7f);
  }
}

}  // namespace

void adEnter() {
  t = 0;
  rewarded = false;
  leaving = false;
}

void adUpdate(float dt) {
  t += dt;
  if (leaving) return;
  if (!rewarded && t >= AD_S) {
    rewarded = true;
    refundAllBets();
    balance = START_BALANCE;
    saveBalance();
  }
  if (rewarded && (t >= AD_S + REWARD_S || (t > AD_S + 0.6f && anyConfirmPressed()))) {
    leaving = true;
    navigate(SCR_MENU);
  }
}

void adDraw(int ox, int oy) {
  if (t < AD_S) drawAd(ox, oy);
  else drawReward(ox, oy);
}
