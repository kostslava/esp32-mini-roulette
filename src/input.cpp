#include "input.h"
#include "config.h"

namespace {

volatile int32_t encRaw = 0;
volatile uint8_t encState = 0;
int32_t encConsumed = 0;
int frameDelta = 0;
uint32_t lastMoveMs = 0;

// Index: (previous AB << 2) | current AB. Invalid transitions count as 0.
const int8_t QDEC[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void IRAM_ATTR encIsr() {
  uint8_t ab = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  encState = ((encState << 2) | ab) & 0x0F;
  encRaw += QDEC[encState];
}

struct Btn {
  uint8_t pin;
  bool raw;
  bool stable;
  uint32_t changedAt;
  uint32_t downAt;
  bool pressed;
  bool released;
};

Btn btns[BTN_COUNT] = {
    {PIN_BTN_OK},
    {PIN_BTN_BACK},
    {PIN_ENC_SW},
};

const uint32_t DEBOUNCE_MS = 35;

}  // namespace

void inputBegin() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  for (auto &b : btns) pinMode(b.pin, INPUT_PULLUP);

  encState = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encIsr, CHANGE);
}

void inputUpdate() {
  uint32_t now = millis();

  noInterrupts();
  int32_t raw = encRaw;
  interrupts();
  int32_t d = (raw - encConsumed) / ENC_STEPS_PER_DETENT;
  encConsumed += d * ENC_STEPS_PER_DETENT;
#if ENC_REVERSE
  d = -d;
#endif
  frameDelta = d;
  if (d != 0) lastMoveMs = now;

  for (auto &b : btns) {
    b.pressed = b.released = false;
    bool r = digitalRead(b.pin) == LOW;
    if (r != b.raw) {
      b.raw = r;
      b.changedAt = now;
    }
    if (b.raw != b.stable && now - b.changedAt >= DEBOUNCE_MS) {
      b.stable = b.raw;
      if (b.stable) {
        b.pressed = true;
        b.downAt = now;
      } else {
        b.released = true;
      }
    }
  }
}

int encDelta() { return frameDelta; }
uint32_t encLastMoveMs() { return lastMoveMs; }
bool btnPressed(BtnId b) { return btns[b].pressed; }
bool btnReleased(BtnId b) { return btns[b].released; }
bool btnDown(BtnId b) { return btns[b].stable; }
uint32_t btnHeldMs(BtnId b) { return btns[b].stable ? millis() - btns[b].downAt : 0; }
bool anyConfirmPressed() { return btns[BTN_OK].pressed || btns[BTN_ENC].pressed; }
