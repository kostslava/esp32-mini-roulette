#include <math.h>
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "screens.h"

namespace {

// The band is a slice of a huge wheel whose centre sits above the screen.
constexpr float BAND_CY = -90;
constexpr float R_IN = 110, R_OUT = 146, R_MID = 128;
constexpr float PA = 0.29f;  // angular width of one pocket
constexpr float DOWN = PI / 2;

// Result timeline (seconds after the wheel stops).
constexpr float T_ZOOM = 0.6f, T_HOLD = 1.05f, T_SLIDE = 1.5f, T_COUNT = 2.0f, T_DONE = 2.9f;

enum Phase : uint8_t { WAIT, SPINNING, SETTLE, RESULT };

Phase phase;
float pos, vel;
float target;
float t, phaseT;
// Recent encoder clicks in the current direction; a fast enough burst launches the spin at once.
constexpr int FLICK_MIN = 3;
constexpr uint32_t FLICK_WINDOW_MS = 160;
constexpr int FLICK_MAX = 8;
uint32_t flickT[FLICK_MAX];
int flickN;
int flickDir;
uint32_t lastClick;
float tickT;
int tickDir;
int lastCenter;
bool leaving;

int32_t betTotal, oldBal, newBal, net;
int number;
bool jackpot;

struct Particle {
  float x, y, vx, vy, ph;
};
constexpr int PARTICLES = 32;
Particle confetti[PARTICLES];
bool confettiOn;

float rand01() { return esp_random() / 4294967295.0f; }
int wrapPocket(int k) { return ((k % POCKETS) + POCKETS) % POCKETS; }

void launch(float speed, int dir) {
  vel = dir * speed * (0.9f + 0.2f * rand01());
  phase = SPINNING;
  flickN = 0;
}

void addClicks(int d, uint32_t now) {
  int dir = d > 0 ? 1 : -1;
  if (dir != flickDir) {
    flickN = 0;
    flickDir = dir;
  }
  for (int i = 0; i < abs(d); i++) {
    if (flickN == FLICK_MAX) {
      memmove(flickT, flickT + 1, sizeof(uint32_t) * (FLICK_MAX - 1));
      flickN--;
    }
    flickT[flickN++] = now;
  }
  lastClick = now;
}

// Launches immediately if the recent clicks are fast enough; slow turning just nudges the band.
void checkFlick(uint32_t now) {
  int drop = 0;
  while (drop < flickN && now - flickT[drop] > FLICK_WINDOW_MS) drop++;
  if (drop) {
    memmove(flickT, flickT + drop, sizeof(uint32_t) * (flickN - drop));
    flickN -= drop;
  }
  if (flickN < FLICK_MIN) return;
  float span = fmaxf(0.03f, (flickT[flickN - 1] - flickT[0]) / 1000.0f);
  float rate = (flickN - 1) / span;
  launch(clampf(6 + rate * 0.8f, 9, 32), flickDir);
}

void resolve() {
  number = WHEEL_ORDER[wrapPocket(lroundf(target))];
  int32_t ret = payoutFor(number);
  jackpot = bets[number] > 0;
  oldBal = balance;
  balance += ret;
  newBal = balance;
  net = ret - betTotal;
  clearBetsLost();
  saveBalance();
  pushHistory(number);
  phase = RESULT;
  phaseT = 0;
  confettiOn = false;
}

void spawnConfetti() {
  for (auto &p : confetti) {
    p.x = 30 + (rand01() - 0.5f) * 20;
    p.y = 12;
    p.vx = (rand01() - 0.5f) * 150;
    p.vy = -40 - rand01() * 80;
    p.ph = rand01() * 10;
  }
  confettiOn = true;
}

void finish(bool toMenu) {
  leaving = true;
  if (balance <= 0) navigate(SCR_AD);
  else navigate(toMenu ? SCR_MENU : SCR_BETS);
}

void drawPointer(float x, float baseY, float tipY, float tipDx) {
  float dir = tipY > baseY ? 1 : -1;
  setColor(C_BLACK);
  fillTri(x - 8, baseY - dir, x + 8, baseY - dir, x + tipDx, tipY + dir * 2);
  setColor(C_WHITE);
  fillTri(x - 6, baseY, x + 6, baseY, x + tipDx, tipY);
}

void drawBand(float cx, float cy) {
  setFont(F_BIG);
  int base = (int)floorf(pos);
  for (int k = base - 3; k <= base + 3; k++) {
    float ac = DOWN - (k - pos) * PA;
    if (fabsf(ac - DOWN) > 0.8f) continue;
    float a0 = ac - PA / 2, a1 = ac + PA / 2;
    drawPocket(cx, cy, R_IN, R_OUT, a0, a1, WHEEL_ORDER[wrapPocket(k)], R_MID);
    line(cx + R_IN * cosf(a0), cy + R_IN * sinf(a0), cx + R_OUT * cosf(a0), cy + R_OUT * sinf(a0));
  }
  arc(cx, cy, R_IN, DOWN - 0.8f, DOWN + 0.8f);
  arc(cx, cy, R_OUT, DOWN - 0.8f, DOWN + 0.8f);
  arc(cx, cy, R_IN - 3, DOWN - 0.8f, DOWN + 0.8f);
  arc(cx, cy, R_OUT + 3, DOWN - 0.8f, DOWN + 0.8f);
}

void roundBox(int x, int y, int w, int h) {
  fillRect(x + 2, y, w - 4, h);
  fillRect(x + 1, y + 1, w - 2, h - 2);
  fillRect(x, y + 2, w, h - 4);
}

void roundFrame(int x, int y, int w, int h) {
  line(x + 2, y, x + w - 3, y);
  line(x + 2, y + h - 1, x + w - 3, y + h - 1);
  line(x, y + 2, x, y + h - 3);
  line(x + w - 1, y + 2, x + w - 1, y + h - 3);
  line(x + 1, y + 1, x + 1, y + 1);
  line(x + w - 2, y + 1, x + w - 2, y + 1);
  line(x + 1, y + h - 2, x + 1, y + h - 2);
  line(x + w - 2, y + h - 2, x + w - 2, y + h - 2);
}

void drawResultBox(int x, int y, int w, int h) {
  setColor(C_BLACK);
  fillRect(x - 2, y - 2, w + 4, h + 4);
  setColor(C_WHITE);

  char buf[4];
  snprintf(buf, sizeof(buf), "%d", number);
  setFont(F_HUGE);
  int tx = x + w / 2, ty = y + h / 2;
  switch (pocketColor(number)) {
    case PC_WHITE:
      roundBox(x, y, w, h);
      setColor(C_BLACK);
      textCentered(tx, ty, buf);
      setColor(C_WHITE);
      break;
    case PC_BLACK:
      roundFrame(x, y, w, h);
      roundFrame(x + 1, y + 1, w - 2, h - 2);
      textCentered(tx, ty, buf);
      break;
    case PC_ZERO:
      roundFrame(x, y, w, h);
      roundFrame(x + 3, y + 3, w - 6, h - 6);
      textCentered(tx, ty, buf);
      break;
  }
}

}  // namespace

void spinEnter() {
  phase = WAIT;
  pos = (float)(esp_random() % POCKETS);
  vel = 0;
  t = phaseT = 0;
  flickN = 0;
  flickDir = 0;
  lastClick = 0;
  tickT = 0;
  tickDir = 1;
  lastCenter = lroundf(pos);
  leaving = false;
  betTotal = totalBet();
  confettiOn = false;
}

void spinUpdate(float dt) {
  t += dt;
  phaseT += dt;
  if (leaving) return;
  uint32_t now = millis();
  int d = encDelta();

  switch (phase) {
    case WAIT:
      if (d) {
        addClicks(d, now);
        pos += d * 0.25f;
      }
      checkFlick(now);
      if (phase == WAIT && now - lastClick > 150) pos = approach(pos, roundf(pos), 10, dt);
      if (phase == WAIT && btnPressed(BTN_OK)) launch(14 + rand01() * 10, rand01() < 0.5f ? -1 : 1);
      if (phase == WAIT && btnPressed(BTN_BACK)) {
        leaving = true;
        navigate(SCR_BETS);
      }
      break;

    case SPINNING: {
      float s = vel > 0 ? 1 : -1;
      vel -= s * (1.5f + 0.6f * fabsf(vel)) * dt;
      pos += vel * dt;
      if (fabsf(vel) < 1.3f) {
        target = vel > 0 ? ceilf(pos) : floorf(pos);
        phase = SETTLE;
      }
      break;
    }

    case SETTLE: {
      const float w = 7;
      vel += (w * w * (target - pos) - 2 * w * vel) * dt;
      pos += vel * dt;
      if (fabsf(target - pos) < 0.003f && fabsf(vel) < 0.05f) {
        pos = target;
        vel = 0;
        resolve();
      }
      break;
    }

    case RESULT:
      if (phaseT < T_COUNT && phaseT > 0.3f && (anyConfirmPressed() || btnPressed(BTN_BACK))) {
        phaseT = T_COUNT;
      } else if (phaseT >= T_DONE) {
        if (anyConfirmPressed()) finish(false);
        else if (btnPressed(BTN_BACK)) finish(true);
      }
      if (!confettiOn && phaseT >= T_COUNT && net > 0) spawnConfetti();
      break;
  }

  int center = (int)floorf(pos + 0.5f);
  if (center != lastCenter) {
    tickDir = center > lastCenter ? 1 : -1;
    lastCenter = center;
    tickT = 1;
  }
  tickT = fmaxf(0, tickT - dt * 7);

  if (confettiOn) {
    for (auto &p : confetti) {
      p.vy += 170 * dt;
      p.vx *= 1 - 0.8f * dt;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
    }
  }
}

void spinDraw(int ox, int oy) {
  char buf[20], money[16];
  float rt = phase == RESULT ? phaseT : 0;

  float slide = phase == RESULT ? easeInOutCubic((rt - T_SLIDE) / (T_COUNT - T_SLIDE)) : 0;
  float bandOy = -90 * slide;
  float cx = 64 + ox, cy = BAND_CY + oy + bandOy;

  if (slide < 1) {
    drawBand(cx, cy);

    if (phase == RESULT && rt < T_ZOOM && fmodf(rt, 0.2f) < 0.1f) {
      setColor(C_INVERT);
      fillWedge(cx, cy, R_IN + 1, R_OUT - 1, DOWN - PA / 2 + 0.01f, DOWN + PA / 2 - 0.01f);
      setColor(C_WHITE);
    }

    float tipDx = -tickDir * tickT * 3.5f;
    float py = oy + bandOy;
    drawPointer(cx, 8 + py, 22 + py, tipDx);
    drawPointer(cx, 63 + py, 52 + py, tipDx);

    setFont(F_SMALL);
    fmtMoney(money, sizeof(money), betTotal);
    snprintf(buf, sizeof(buf), "BET %s", money);
    text(1 + ox, 60 + lroundf(py), buf);
    fmtMoney(money, sizeof(money), phase == RESULT ? oldBal : balance);
    textRight(127 + ox, 60 + lroundf(py), money);

    if (phase == WAIT) {
      setFont(F_SMALL);
      int off = lroundf(3 * sinf(t * 6));
      text(cx - 26 - off, 5 + py, "<<");
      text(cx + 16 + off, 5 + py, ">>");
    }
  }

  if (phase != RESULT || rt < T_ZOOM) return;

  float z = easeOutBack((rt - T_ZOOM) / (T_HOLD - T_ZOOM));
  float bx = lerpf(46, 40, z), by = lerpf(20, 6, z), bw = lerpf(36, 48, z), bh = lerpf(36, 52, z);
  bx += lerpf(0, -34, slide);
  if (net <= 0 && rt > T_COUNT) bx += sinf(rt * 55) * 3 * fmaxf(0, 1 - (rt - T_COUNT) * 2.5f);
  drawResultBox(lroundf(bx) + ox, lroundf(by) + oy, lroundf(bw), lroundf(bh));

  if (rt >= T_SLIDE) {
    int tx = 93 + ox + lroundf(lerpf(70, 0, slide));
    const char *title = jackpot && net > 0 ? "JACKPOT!" : net > 0 ? "WIN!" : net == 0 ? "PUSH" : "LOST";
    setFont(jackpot && net > 0 ? F_SMALL : F_BOLD);
    textCentered(tx, 12 + oy, title);

    setFont(F_BOLD);
    fmtMoney(money, sizeof(money), net < 0 ? -net : net);
    snprintf(buf, sizeof(buf), "%s%s", net > 0 ? "+" : net < 0 ? "-" : "", money);
    textCentered(tx, 30 + oy, buf);

    float ce = easeOutCubic((rt - T_COUNT) / (T_DONE - T_COUNT));
    setFont(F_SMALL);
    fmtMoney(money, sizeof(money), (int32_t)lroundf(lerpf(oldBal, newBal, ce)));
    snprintf(buf, sizeof(buf), "BAL %s", money);
    textCentered(tx, 45 + oy, buf);

    if (rt >= T_DONE && fmodf(rt, 1.0f) < 0.65f) {
      setFont(F_SMALL);
      textCentered(tx, 57 + oy, balance > 0 ? "OK: AGAIN" : "OK: NEXT");
    }
  }

  if (confettiOn) {
    for (auto &p : confetti) {
      int x = lroundf(p.x) + ox, y = lroundf(p.y) + oy;
      if (y < 0 || y >= SCR_H || x < 0 || x >= SCR_W - 1) continue;
      if (((int)(p.ph + rt * 14)) & 1) hline(x, y, 2);
      else vline(x, y, 2);
    }
  }

  if (jackpot && net > 0 && rt > T_COUNT && rt < T_COUNT + 0.6f && fmodf(rt - T_COUNT, 0.2f) < 0.1f) {
    setColor(C_INVERT);
    fillRect(0, 0, SCR_W, SCR_H);
    setColor(C_WHITE);
  }
}
