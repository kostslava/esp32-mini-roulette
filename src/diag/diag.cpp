#include <Arduino.h>

// Bit-banged I2C probe with hard timeouts, so a misbehaving bus can't hang it.
static int SDA_P, SCL_P;

static void rel(int p) { pinMode(p, INPUT_PULLUP); }
static void low(int p) {
  digitalWrite(p, LOW);
  pinMode(p, OUTPUT);
}
static void dly() { delayMicroseconds(10); }

static bool sclHigh() {
  rel(SCL_P);
  uint32_t t = micros();
  while (!digitalRead(SCL_P))
    if (micros() - t > 2000) return false;
  return true;
}

static bool start() {
  rel(SDA_P);
  if (!sclHigh()) return false;
  dly();
  if (!digitalRead(SDA_P)) return false;
  low(SDA_P); dly();
  low(SCL_P); dly();
  return true;
}

static void stop() {
  low(SDA_P); dly();
  sclHigh(); dly();
  rel(SDA_P); dly();
}

// returns 1 = ACK, 0 = NACK, -1 = SCL stuck low
static int writeByte(uint8_t b) {
  for (int i = 7; i >= 0; i--) {
    if (b & (1 << i)) rel(SDA_P);
    else low(SDA_P);
    dly();
    if (!sclHigh()) return -1;
    dly();
    low(SCL_P);
  }
  rel(SDA_P); dly();
  if (!sclHigh()) return -1;
  dly();
  int ack = !digitalRead(SDA_P);
  low(SCL_P); dly();
  return ack;
}

static void probe(int sda, int scl) {
  SDA_P = sda; SCL_P = scl;
  rel(sda); rel(scl);
  delay(5);
  Serial.printf("SDA=%d SCL=%d idle: sda=%d scl=%d |", sda, scl, digitalRead(sda), digitalRead(scl));
  delay(20);
  int hits = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    if (!start()) {
      Serial.printf(" bus busy at 0x%02X (sda=%d scl=%d)", a, digitalRead(sda), digitalRead(scl));
      break;
    }
    int r = writeByte(a << 1);
    stop();
    if (r == 1) {
      Serial.printf(" FOUND 0x%02X", a);
      hits++;
    } else if (r < 0) {
      Serial.printf(" SCL stuck low at 0x%02X", a);
      break;
    }
  }
  if (!hits) Serial.print(" no device");
  Serial.println();
  delay(30);
  rel(sda); rel(scl);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
}

void loop() {
  Serial.println("\n==== goober diag (bit-bang) ====");
  delay(30);
  rel(4);
  low(3);
  delay(2);
  int b = digitalRead(4);
  rel(3);
  low(4);
  delay(2);
  int a = digitalRead(3);
  rel(4);
  Serial.printf("short test: 3 driven LOW -> 4 reads %d, 4 driven LOW -> 3 reads %d (0 = SHORTED)\n", b, a);
  delay(30);
  probe(3, 4);
  probe(4, 3);
  Serial.println("done");
  delay(2000);
}
