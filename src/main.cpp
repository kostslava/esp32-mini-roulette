#include <Arduino.h>
#include "config.h"
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "screens.h"

namespace {

enum TransitionKind : uint8_t { TR_MORPH, TR_UP, TR_DOWN, TR_LEFT };

ScreenId current = SCR_SPLASH;

struct {
  bool active = false;
  ScreenId from, to;
  TransitionKind kind;
  float t, dur;
} tr;

uint32_t lastMicros = 0;

void enterScreen(ScreenId s) {
  switch (s) {
    case SCR_SPLASH: splashEnter(); break;
    case SCR_MENU: menuEnter(); break;
    case SCR_BETS: betsEnter(); break;
    case SCR_SPIN: spinEnter(); break;
    case SCR_AD: adEnter(); break;
  }
}

void updateScreen(ScreenId s, float dt) {
  switch (s) {
    case SCR_SPLASH: splashUpdate(dt); break;
    case SCR_MENU: menuUpdate(dt); break;
    case SCR_BETS: betsUpdate(dt); break;
    case SCR_SPIN: spinUpdate(dt); break;
    case SCR_AD: adUpdate(dt); break;
  }
}

void drawScreen(ScreenId s, int ox, int oy) {
  switch (s) {
    case SCR_SPLASH: splashDraw(ox, oy); break;
    case SCR_MENU: menuDraw(ox, oy); break;
    case SCR_BETS: betsDraw(ox, oy); break;
    case SCR_SPIN: spinDraw(ox, oy); break;
    case SCR_AD: adDraw(ox, oy); break;
  }
}

void drawTransition() {
  float p = tr.t / tr.dur;
  float e = easeInOutCubic(p);
  switch (tr.kind) {
    case TR_MORPH:
      drawMenuBetsMorph(tr.to == SCR_BETS ? e : 1 - e);
      break;
    case TR_UP:
      drawScreen(tr.from, 0, lroundf(-SCR_H * e));
      drawScreen(tr.to, 0, lroundf(SCR_H * (1 - e)));
      break;
    case TR_DOWN:
      drawScreen(tr.from, 0, lroundf(SCR_H * e));
      drawScreen(tr.to, 0, lroundf(-SCR_H * (1 - e)));
      break;
    case TR_LEFT:
      drawScreen(tr.from, lroundf(-SCR_W * e), 0);
      drawScreen(tr.to, lroundf(SCR_W * (1 - e)), 0);
      break;
  }
}

}  // namespace

void navigate(ScreenId to) {
  if (tr.active) return;
  tr.active = true;
  tr.from = current;
  tr.to = to;
  tr.t = 0;
  if ((current == SCR_MENU && to == SCR_BETS) || (current == SCR_BETS && to == SCR_MENU)) {
    tr.kind = TR_MORPH;
    tr.dur = 0.7f;
  } else if (current == SCR_BETS && to == SCR_SPIN) {
    tr.kind = TR_UP;
    tr.dur = 0.45f;
  } else if (current == SCR_SPIN && to == SCR_BETS) {
    tr.kind = TR_DOWN;
    tr.dur = 0.45f;
  } else {
    tr.kind = TR_LEFT;
    tr.dur = 0.45f;
  }
  enterScreen(to);
}

void setup() {
  Serial.begin(115200);
  inputBegin();
  gfxBegin();
  loadBalance();
  enterScreen(SCR_SPLASH);
  lastMicros = micros();
}

void loop() {
  uint32_t now = micros();
  float dt = (now - lastMicros) / 1e6f;
  lastMicros = now;
  if (dt > 0.05f) dt = 0.05f;

  inputUpdate();

  if (tr.active) {
    tr.t += dt;
    if (tr.t >= tr.dur) {
      tr.active = false;
      current = tr.to;
    }
  } else {
    updateScreen(current, dt);
  }

  beginFrame();
  if (tr.active) drawTransition();
  else drawScreen(current, 0, 0);
  endFrame();
}
