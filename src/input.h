#pragma once
#include <Arduino.h>

enum BtnId : uint8_t { BTN_OK, BTN_BACK, BTN_ENC, BTN_COUNT };

void inputBegin();
void inputUpdate();  // call once per frame, before any queries

int encDelta();               // clicks turned since the last frame (signed)
uint32_t encLastMoveMs();     // millis() of the most recent click

bool btnPressed(BtnId b);     // went down this frame
bool btnReleased(BtnId b);    // went up this frame
bool btnDown(BtnId b);
uint32_t btnHeldMs(BtnId b);  // 0 if not held
bool anyConfirmPressed();     // OK or encoder push
