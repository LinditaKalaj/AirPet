/*
  AirPet — derpy googly-eyed blob, DVD-bounce edition, for your tested hardware
  When the air gets bad he storms up to your face and coughs at you; if it stays
  bad he collapses and wails on the floor until it clears.

  Board: SparkFun Pro Micro, ATmega32U4 (5V, 16 MHz)
  Libraries: U8g2, Adafruit CCS811, Adafruit BusIO

  CCS811: VCC->VCC, GND->GND, SDA->2, SCL->3, WAKE->GND.
          RST and INT are unconnected. Address: 0x5A.
  OLED:   VCC->VCC, GND->GND, CLK->15, MOSI->16,
          RES->8, DC->9, CS->10.

  Disconnect USB before changing wires.
  Send c in Serial Monitor at 9600 baud for a 15-second demo cough (it does not
  change the sensor reading). The sketch prints nothing to save flash space;
  sensor status shows on the OLED instead.
  This is a VOC-responsive toy, not an AQI or safety monitor.
*/

#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_CCS811.h>

U8G2_SH1106_128X64_NONAME_1_4W_HW_SPI oled(U8G2_R0, 10, 9, 8);
Adafruit_CCS811 sensor;

// These are adjustable game settings, NOT validated health limits.
// Petty mode: perfume, snacks or a whiff of hand sanitizer will set him off.
const uint16_t COUGH_ON_PPB = 150;
const uint16_t COUGH_OFF_PPB = 100;
// The sensor gives one reading per second. 1 = react to the first bad reading;
// raise it if single spikes set him off too easily.
const uint8_t HIGH_SAMPLES_REQUIRED = 1;
const uint8_t LOW_SAMPLES_REQUIRED = 3;
const uint32_t DEMO_MS = 15000UL;  // long enough to see him collapse
const uint32_t STALE_MS = 10000UL;

bool sensorStarted = false;
bool haveReading = false;
bool readError = false;
bool vocTriggered = false;
bool demoActive = false;
uint8_t highSamples = 0;
uint8_t lowSamples = 0;
uint16_t tvoc = 0;
uint16_t eco2 = 0;
uint32_t startedAt = 0;
uint32_t lastPollAt = 0;
uint32_t lastReadingAt = 0;
uint32_t demoStartedAt = 0;

void clearTrigger() {
  vocTriggered = false;
  highSamples = 0;
  lowSamples = 0;
}

bool freshReading(uint32_t now) {
  return sensorStarted && haveReading && !readError &&
         (uint32_t)(now - lastReadingAt) < STALE_MS;
}

void updateTrigger() {
  if (tvoc >= COUGH_ON_PPB) {
    lowSamples = 0;
    if (highSamples < HIGH_SAMPLES_REQUIRED) highSamples++;
    if (highSamples >= HIGH_SAMPLES_REQUIRED) vocTriggered = true;
  } else if (tvoc <= COUGH_OFF_PPB) {
    highSamples = 0;
    if (lowSamples < LOW_SAMPLES_REQUIRED) lowSamples++;
    if (lowSamples >= LOW_SAMPLES_REQUIRED) vocTriggered = false;
  } else {
    highSamples = 0;
    lowSamples = 0;
  }
}

void pollSensor(uint32_t now) {
  // Check often so a new reading is picked up as soon as the sensor has it.
  if ((uint32_t)(now - lastPollAt) < 100UL) return;
  lastPollAt = now;

  if (!sensorStarted) return;

  // Check for a disconnected sensor before asking the library for data.
  Wire.clearWireTimeoutFlag();
  Wire.beginTransmission(0x5A);
  if (Wire.endTransmission() != 0) {
    readError = true;
  } else if (sensor.checkError()) {
    readError = true;
  } else if (sensor.available()) {
    byte error = sensor.readData();
    readError = error != 0 || Wire.getWireTimeoutFlag();

    if (!readError) {
      tvoc = sensor.getTVOC();
      eco2 = sensor.geteCO2();
      // The CCS811's calculated eCO2 output starts at 400 ppm.
      readError = eco2 < 400;
    }

    if (!readError) {
      haveReading = true;
      lastReadingAt = now;
      updateTrigger();
    }
  }

  if (Wire.getWireTimeoutFlag()) readError = true;
  if (readError) clearTrigger();
}

// Four chubby squish frames (a slow jiggle), kept in flash to save the Pro Micro's RAM.
const int8_t BLOB_SHAPES[4][32][2] PROGMEM = {
  {{28, 0}, {28, 4}, {26, 7}, {24, 10}, {21, 11}, {18, 13}, {13, 14}, {8, 15}, {0, 15}, {-8, 15}, {-13, 14}, {-18, 13}, {-21, 11}, {-24, 10}, {-26, 7}, {-28, 4}, {-28, 0}, {-27, -4}, {-26, -7}, {-23, -11}, {-20, -13}, {-16, -16}, {-11, -18}, {-5, -19}, {0, -19}, {5, -19}, {11, -18}, {16, -16}, {20, -13}, {23, -11}, {26, -7}, {27, -4}},
  {{29, 0}, {29, 4}, {27, 7}, {25, 9}, {22, 11}, {18, 12}, {14, 13}, {8, 14}, {0, 14}, {-8, 14}, {-14, 13}, {-18, 12}, {-22, 11}, {-25, 9}, {-27, 7}, {-29, 4}, {-29, 0}, {-28, -4}, {-27, -7}, {-24, -10}, {-21, -13}, {-16, -15}, {-11, -17}, {-6, -18}, {0, -18}, {6, -18}, {11, -17}, {16, -15}, {21, -13}, {24, -10}, {27, -7}, {28, -4}},
  {{28, 0}, {28, 4}, {26, 7}, {24, 10}, {21, 11}, {18, 13}, {13, 14}, {8, 15}, {0, 15}, {-8, 15}, {-13, 14}, {-18, 13}, {-21, 11}, {-24, 10}, {-26, 7}, {-28, 4}, {-28, 0}, {-27, -4}, {-26, -7}, {-23, -11}, {-20, -13}, {-16, -16}, {-11, -18}, {-5, -19}, {0, -19}, {5, -19}, {11, -18}, {16, -16}, {20, -13}, {23, -11}, {26, -7}, {27, -4}},
  {{27, 0}, {27, 4}, {25, 7}, {23, 10}, {21, 11}, {17, 13}, {13, 14}, {8, 15}, {0, 15}, {-8, 15}, {-13, 14}, {-17, 13}, {-21, 11}, {-23, 10}, {-25, 7}, {-27, 4}, {-27, 0}, {-26, -4}, {-25, -8}, {-22, -11}, {-19, -14}, {-15, -17}, {-10, -18}, {-5, -20}, {0, -20}, {5, -20}, {10, -18}, {15, -17}, {19, -14}, {22, -11}, {25, -8}, {26, -4}}
};

// ---- Size: 16 is normal; bigger means he is closer to your face. ----
uint8_t petScale = 16;
int sc(int v) { return (v * (int)petScale + (v >= 0 ? 8 : -8)) / 16; }

// Body outline. squashY / stretchX are in 16ths (16 = normal), so he can
// flatten into a sad puddle on the floor or stretch tall when he springs up.
void drawBlob(int x, int y, uint8_t shape, uint8_t squashY, uint8_t stretchX) {
  for (uint8_t i = 0; i < 32; i++) {
    uint8_t next = (i + 1) % 32;
    int dx1 = sc((int8_t)pgm_read_byte(&BLOB_SHAPES[shape][i][0])) * stretchX / 16;
    int dy1 = sc((int8_t)pgm_read_byte(&BLOB_SHAPES[shape][i][1])) * squashY / 16;
    int dx2 = sc((int8_t)pgm_read_byte(&BLOB_SHAPES[shape][next][0])) * stretchX / 16;
    int dy2 = sc((int8_t)pgm_read_byte(&BLOB_SHAPES[shape][next][1])) * squashY / 16;
    oled.drawLine(x + dx1, y + dy1, x + dx2, y + dy2);
  }
}

// A big white googly eye with a black pupil and a tiny sparkle.
// (ox, oy) places the eye on the face; (px, py) must keep the pupil inside it.
void drawGooglyEye(int x, int y, int ox, int oy, uint8_t r, uint8_t pr, int px, int py) {
  const int cx = x + sc(ox), cy = y + sc(oy);
  oled.drawDisc(cx, cy, sc(r));
  oled.setDrawColor(0);
  oled.drawDisc(cx + sc(px), cy + sc(py), sc(pr));
  oled.setDrawColor(1);
  oled.drawPixel(cx + sc(px) - 1, cy + sc(py) - 1);
}

// A black eyelid over the top of an eye. The lid's edge runs from the outer
// corner to the inner corner (offsets from the eye centre): slanted down toward
// the nose looks furious, flat looks unimpressed. Optionally adds a thick brow.
void drawLid(int x, int y, int ox, int oy, uint8_t r, int8_t outerEdge,
             int8_t innerEdge, bool innerOnRight, bool brow) {
  const int cx = x + sc(ox), cy = y + sc(oy), R = sc(r);
  const int top = cy - R - 1;
  const int leftEdge = cy + sc(innerOnRight ? outerEdge : innerEdge);
  const int rightEdge = cy + sc(innerOnRight ? innerEdge : outerEdge);
  oled.setDrawColor(0);
  for (int i = -R; i <= R; i++) {
    const int edge = leftEdge + (rightEdge - leftEdge) * (i + R) / (2 * R);
    if (edge > top) oled.drawVLine(cx + i, top, edge - top);
  }
  oled.setDrawColor(1);
  if (brow) {
    const int l = innerOnRight ? 2 : 0, rr = innerOnRight ? 0 : 2;
    oled.drawLine(cx - R + l, leftEdge - 3, cx + R - rr, rightEdge - 3);
    oled.drawLine(cx - R + l, leftEdge - 4, cx + R - rr, rightEdge - 4);
  }
}

bool onScreen(int x, int y, int margin) {
  return x - margin >= 0 && x + margin < 128 && y - margin >= 0 && y + margin < 64;
}

// The cartoon "popping vein" anger mark, floating by his head.
void drawVein(int vx, int vy, int a) {
  if (!onScreen(vx, vy, a + 1)) return;
  oled.drawHLine(vx - 1 - a, vy - 1, a);  oled.drawVLine(vx - 1, vy - 1 - a, a);
  oled.drawHLine(vx + 2, vy - 1, a);      oled.drawVLine(vx + 1, vy - 1 - a, a);
  oled.drawHLine(vx - 1 - a, vy + 1, a);  oled.drawVLine(vx - 1, vy + 2, a);
  oled.drawHLine(vx + 2, vy + 1, a);      oled.drawVLine(vx + 1, vy + 2, a);
}

// "///" blush marks on both cheeks.
void drawBlush(int x, int y, uint8_t lines) {
  for (int8_t i = 0; i < (int8_t)lines; i++) {
    oled.drawLine(x + sc(-23 + i * 3), y + sc(8), x + sc(-21 + i * 3), y + sc(6));
    oled.drawLine(x + sc(15 + i * 3), y + sc(8), x + sc(17 + i * 3), y + sc(6));
  }
}

// ---- Drool: a drip grows at the corner of his mouth, lets go, falls to
// the floor and splats into a little puddle. ----
const uint32_t DROOL_CYCLE_MS = 3600UL;
const uint32_t DROOL_START_MS = 400UL;    // drip starts forming
const uint32_t DROOL_DETACH_MS = 1400UL;  // drip lets go
const uint32_t DROOL_FALL_DIV = 8000UL;   // gravity: fall px = dt^2 / this
const int FLOOR_Y = 56;                   // just above the VOC line

uint16_t isqrt32(uint32_t n) {
  uint32_t r = 0, b = 1UL << 30;
  while (b > n) b >>= 2;
  while (b) {
    if (n >= r + b) { n -= r + b; r = (r >> 1) + b; }
    else r >>= 1;
    b >>= 2;
  }
  return (uint16_t)r;
}

// Goofy open grin with the tongue flopped out to one side.
void drawDerpMouth(int x, int y, uint32_t now, bool drool) {
  y -= 3;
  oled.drawLine(x - 6, y + 8, x - 3, y + 11);
  oled.drawHLine(x - 3, y + 11, 7);
  oled.drawLine(x + 3, y + 11, x + 6, y + 8);
  oled.drawRBox(x, y + 11, 6, 4, 1);
  oled.setDrawColor(0);
  oled.drawVLine(x + 3, y + 12, 2);
  oled.setDrawColor(1);
  // The drip, still hanging from the other corner of his mouth.
  const uint32_t c = now % DROOL_CYCLE_MS;
  if (drool && c >= DROOL_START_MS && c < DROOL_DETACH_MS) {
    oled.drawVLine(x - 4, y + 11, 1 + (c - DROOL_START_MS) * 4 / (DROOL_DETACH_MS - DROOL_START_MS));
  }
}

// ---- DVD-logo bounce: straight diagonal lines at a constant speed,
// reflecting cleanly off each edge, like the DVD screensaver. ----
// The body is about 58 px wide and 40 px tall, so its centre stays in this box
// (above the VOC line at the bottom).
const int BOUNCE_LEFT = 32;
const int BOUNCE_RIGHT = 96;
const int BOUNCE_TOP = 20;
const int BOUNCE_BOTTOM = 41;
const uint32_t BOUNCE_MS_PER_PX = 100UL;  // same on both axes, 10 px/s

// The two spans (64 and 21) share no common factor, so he only lands exactly
// in a corner once every 64 * 21 steps (about every 2 minutes 15 seconds).
const uint32_t CORNER_EVERY_STEPS = (uint32_t)(BOUNCE_RIGHT - BOUNCE_LEFT) *
                                    (uint32_t)(BOUNCE_BOTTOM - BOUNCE_TOP);
const uint32_t CELEBRATE_MS = 2400UL;  // a whole number of 300 ms hops
const uint32_t HOP_MS = 300UL;

// Position along one axis: a constant-speed run between two walls and back.
int bounceAxis(uint32_t now, int lo, int hi) {
  const uint32_t span = hi - lo;
  const uint32_t step = (now / BOUNCE_MS_PER_PX) % (2 * span);
  return lo + (int)(step < span ? step : 2 * span - step);
}

// One eye blinks, then the other follows right after.
const uint32_t BLINK_EVERY_MS = 4200UL;
const uint32_t BLINK_SHUT_MS = 140UL;
const uint32_t BLINK_LAG_MS = 200UL;

// ---- Mood ----
// CALM:   bouncing around, drooling.
// ANGRY:  on a cough trigger he stops dead, seethes, charges up to your face
//         and coughs at you like a bratty kid, over and over.
// DRAMA:  if the bad air lasts more than 6 s he gives up, collapses on the
//         floor and wails like he is dying.
// RETURN: air cleared while still angry: "hmph", shrinks back into the bounce.
// GETUP:  air cleared while on the floor: pops right up like nothing happened.
enum Mood : uint8_t { MOOD_CALM, MOOD_ANGRY, MOOD_RETURN, MOOD_DRAMA, MOOD_GETUP };
Mood mood = MOOD_CALM;
uint32_t moodSince = 0;
int moodFromX = 64, moodFromY = 31;  // where the current transition began
uint8_t moodFromScale = 16;
uint8_t moodFromSquash = 16;
uint8_t moodFromStretch = 16;
int shownX = 64, shownY = 31;        // where he was drawn last frame
uint8_t shownScale = 16;
uint8_t shownSquash = 16;
uint8_t shownStretch = 16;

const uint32_t STOP_MS = 500UL;          // freezes and fumes in place
const uint32_t RUN_MS = 700UL;           // charges toward you
const uint32_t RETURN_MS = 1000UL;       // shrinks back into the bounce
const uint32_t COUGH_CYCLE_MS = 1700UL;  // glare, inhale, KOFF, glare...
const uint32_t DRAMA_AFTER_MS = 6000UL;  // bad air this long: collapse
const uint32_t SHOCK_MS = 400UL;         // reels back, stunned
const uint32_t COLLAPSE_MS = 700UL;      // falls to the floor
const uint32_t SPRING_MS = 350UL;        // getting up: pops up tall
const uint32_t GETUP_MS = 750UL;         // ...then rejoins the bounce
const int FACE_X = 64;
const int FACE_Y = 33;
const uint8_t FACE_SCALE = 23;           // about 1.4x: right up in your face
const uint8_t FLAT_SQUASH = 11;          // lying flat on the floor
const uint8_t FLAT_STRETCH = 18;

// What he is shouting in the top-right corner.
enum Shout : uint8_t { SHOUT_NONE, SHOUT_HUH, SHOUT_CORNER, SHOUT_KOFF, SHOUT_UGH, SHOUT_DYING,
                       SHOUT_WHY, SHOUT_MOM, SHOUT_BLEH, SHOUT_SMALL_KOFF, SHOUT_OVER };
// Characters in each shout, in enum order; keep in sync with printShout().
const uint8_t SHOUT_LEN[] = {0, 5, 8, 5, 3, 9, 8, 5, 3, 6, 9};
uint8_t petShout = SHOUT_NONE;

void printShout(uint8_t s) {
  switch (s) {
    case SHOUT_HUH: oled.print(F("HUH?!")); break;
    case SHOUT_CORNER: oled.print(F("CORNER!!")); break;
    case SHOUT_KOFF: oled.print(F("KOFF!")); break;
    case SHOUT_UGH: oled.print(F("WAH")); break;
    case SHOUT_DYING: oled.print(F("I'M DYING")); break;
    case SHOUT_WHY: oled.print(F("WHY ME?!")); break;
    case SHOUT_MOM: oled.print(F("MOMMY")); break;
    case SHOUT_BLEH: oled.print(F("WAH")); break;
    case SHOUT_SMALL_KOFF: oled.print(F("*koff*")); break;
    case SHOUT_OVER: oled.print(F("IT'S OVER")); break;
  }
}

int lerpInt(int a, int b, uint16_t u) {
  return a + (int)(((int32_t)(b - a) * u) >> 8);
}

// Smooth start and stop: 0..256 over the given time.
uint16_t easeInOut(uint32_t t, uint32_t total) {
  if (t >= total) return 256;
  const uint32_t u = t * 256UL / total;
  return (uint16_t)(u * u * (768UL - 2UL * u) / 65536UL);
}

// ---- Napping: after a long calm stretch he slows to a crawl and dozes off.
// His bounce runs on its own travel clock, which slows to a sleepy drift as he
// nods off, so he picks up again from exactly where he was. ----
const uint32_t SLEEP_AFTER_MS = 150000UL; // calm this long: starts getting drowsy
const uint32_t SLOW_MS = 5000UL;          // slows from full speed to a drift
const uint32_t SLEEP_SPEED_DIV = 10;      // asleep he drifts at 1/10 speed
const uint32_t JOLT_MS = 300UL;           // jumps awake when the air turns bad
uint32_t travelOffset = 0;                // bounce time lost to naps so far
bool wokeFromNap = false;

// How much bounce time this calm stretch has lost to dozing. Speed ramps from
// full down to 1/SLEEP_SPEED_DIV, then stays there; that loses exactly
// (1 - 1/SLEEP_SPEED_DIV) of what a full stop would.
uint32_t napLoss(uint32_t calmFor) {
  if (calmFor <= SLEEP_AFTER_MS) return 0;
  const uint32_t e = calmFor - SLEEP_AFTER_MS;
  const uint32_t stop = e < SLOW_MS ? e * e / (2 * SLOW_MS) : e - SLOW_MS / 2;
  return stop - stop / SLEEP_SPEED_DIV;
}

// 0 = wide awake, 256 = fast asleep.
uint16_t drowsiness(uint32_t now) {
  if (mood != MOOD_CALM) return 0;
  const uint32_t calmFor = now - moodSince;
  if (calmFor <= SLEEP_AFTER_MS) return 0;
  return easeInOut(calmFor - SLEEP_AFTER_MS, SLOW_MS);
}

uint32_t travelTime(uint32_t now) {
  uint32_t tt = now - travelOffset;
  if (mood == MOOD_CALM) tt -= napLoss(now - moodSince);
  return tt;
}

void updateMood(uint32_t now, bool coughing) {
  const uint32_t t = now - moodSince;
  if (mood == MOOD_CALM && !coughing && t > SLEEP_AFTER_MS) {
    const uint32_t travel = travelTime(now);
    if ((travel / BOUNCE_MS_PER_PX) % CORNER_EVERY_STEPS == 0) {
      // Drifted into a corner in his sleep: wakes right up to celebrate,
      // then nods straight back off. The calm clock is set so drowsiness
      // returns the moment the party ends, and the nap is banked so his
      // bounce carries on from the same spot.
      travelOffset += napLoss(t);
      moodSince = now - travel % BOUNCE_MS_PER_PX - (SLEEP_AFTER_MS - CELEBRATE_MS);
      return;
    }
  }
  Mood next = mood;
  if (coughing) {
    if (mood == MOOD_CALM || mood == MOOD_RETURN || mood == MOOD_GETUP) next = MOOD_ANGRY;
    else if (mood == MOOD_ANGRY && t >= DRAMA_AFTER_MS) next = MOOD_DRAMA;
  } else {
    if (mood == MOOD_ANGRY) next = MOOD_RETURN;
    else if (mood == MOOD_DRAMA) next = MOOD_GETUP;
    else if (mood == MOOD_RETURN && t >= RETURN_MS) next = MOOD_CALM;
    else if (mood == MOOD_GETUP && t >= SPRING_MS + GETUP_MS) next = MOOD_CALM;
  }
  if (next == mood) return;
  if (mood == MOOD_CALM) {
    // Bank the nap so his bounce resumes from where he stopped.
    wokeFromNap = (now - moodSince) > SLEEP_AFTER_MS;
    travelOffset += napLoss(now - moodSince);
  }
  mood = next;
  moodSince = now;
  moodFromX = shownX;
  moodFromY = shownY;
  moodFromScale = shownScale;
  moodFromSquash = shownSquash;
  moodFromStretch = shownStretch;
}

// Back and forth between -amp and +amp over the given period.
int wobble(uint32_t now, uint32_t period, int amp) {
  const uint32_t step = (now % period) * (uint32_t)(4 * amp) / period;
  const int s = (int)step;
  return s < 2 * amp ? s - amp : 3 * amp - s;
}

// The falling drop and its splat, drawn in screen space so it stays behind
// when he bounces away.
void drawFallingDrool(uint32_t now) {
  const uint32_t c = now % DROOL_CYCLE_MS;
  if (c < DROOL_DETACH_MS) return;
  // Only if he was already calm and bouncing when this drop let go.
  if ((uint32_t)(now - moodSince) < c - DROOL_DETACH_MS) return;
  const uint32_t detachAt = now - (c - DROOL_DETACH_MS);
  const uint32_t tt = travelTime(detachAt);
  const int x0 = bounceAxis(tt, BOUNCE_LEFT, BOUNCE_RIGHT) - 4;
  const int y0 = bounceAxis(tt, BOUNCE_TOP, BOUNCE_BOTTOM) + 9;
  const uint32_t dt = c - DROOL_DETACH_MS;
  const uint32_t hitAt = isqrt32((uint32_t)(FLOOR_Y - y0) * DROOL_FALL_DIV);
  if (dt < hitAt) {
    const int yy = y0 + (int)(dt * dt / DROOL_FALL_DIV);
    oled.drawVLine(x0, yy - 1, 2);
    return;
  }
  const uint32_t s = dt - hitAt;
  if (s < 80UL) {
    oled.drawHLine(x0 - 1, FLOOR_Y, 3);
    oled.drawPixel(x0 - 3, FLOOR_Y - 2);
    oled.drawPixel(x0 + 3, FLOOR_Y - 2);
  } else if (s < 160UL) {
    oled.drawHLine(x0 - 2, FLOOR_Y, 5);
    oled.drawPixel(x0 - 4, FLOOR_Y - 1);
    oled.drawPixel(x0 + 4, FLOOR_Y - 1);
  } else if (c < DROOL_CYCLE_MS - 400UL) {
    oled.drawHLine(x0 - 2, FLOOR_Y, 5);  // puddle
  } else {
    oled.drawHLine(x0 - 1, FLOOR_Y, 3);  // drying up
  }
}

enum Face : uint8_t { FACE_CALM, FACE_FAULT, FACE_ANGRY, FACE_INHALE, FACE_COUGH,
                      FACE_HMPH, FACE_SHOCK, FACE_SOB, FACE_INNOCENT, FACE_CORNER };

// A googly eye with a twinkling star where the pupil should be.
void drawStarEye(int cx, int cy, int r, bool big) {
  oled.drawDisc(cx, cy, r);
  oled.setDrawColor(0);
  const int a = big ? r - 1 : r - 3;
  oled.drawHLine(cx - a, cy, 2 * a + 1);
  oled.drawVLine(cx, cy - a, 2 * a + 1);
  oled.drawBox(cx - 1, cy - 1, 3, 3);
  oled.setDrawColor(1);
}

// He hit the corner!! Star eyes, happy hops and sparkles all around.
void drawCornerFace(int x, int y, uint32_t now) {
  const bool twinkle = (now / 150UL) % 2;
  drawStarEye(x - 10, y - 4, 8, twinkle);
  drawStarEye(x + 11, y - 3, 6, !twinkle);
  drawDerpMouth(x, y, now, false);
  static const int8_t SPARKLE[4][2] PROGMEM = {{-34, -14}, {34, -12}, {-33, 10}, {34, 11}};
  for (uint8_t i = 0; i < 4; i++) {
    if (((now / 150UL) + i) % 2) continue;
    const int sx = x + (int8_t)pgm_read_byte(&SPARKLE[i][0]);
    const int sy = y + (int8_t)pgm_read_byte(&SPARKLE[i][1]);
    if (!onScreen(sx, sy, 2)) continue;
    oled.drawHLine(sx - 2, sy, 5);
    oled.drawVLine(sx, sy - 2, 5);
  }
}

void drawAngryFace(int x, int y, uint32_t now, bool inhale) {
  // Pupils glare straight at you from under furious brows.
  drawGooglyEye(x, y, -10, -4, 8, 3, 1, 2);
  drawGooglyEye(x, y, 11, -3, 6, 2, -1, 2);
  drawLid(x, y, -10, -4, 8, -6, -1, true, true);
  drawLid(x, y, 11, -3, 6, -5, -1, false, true);
  drawVein(x + sc(23), y - sc(16), (now / 200UL) % 2 ? 3 : 2);
  if (inhale) {
    // Holding his breath: lips squeezed into a tiny o.
    oled.drawCircle(x, y + sc(8), sc(2));
  } else {
    // Bratty pout.
    oled.drawLine(x - sc(6), y + sc(10), x - sc(3), y + sc(7));
    oled.drawLine(x - sc(3), y + sc(7), x + sc(3), y + sc(7));
    oled.drawLine(x + sc(3), y + sc(7), x + sc(6), y + sc(10));
  }
}

void drawCoughFace(int x, int y, uint32_t ct) {
  // Eyes squeezed shut, mouth wide open, cough clouds blasting at you.
  oled.drawLine(x - sc(16), y - sc(7), x - sc(7), y - sc(3));
  oled.drawLine(x - sc(7), y - sc(3), x - sc(16), y + sc(1));
  oled.drawLine(x + sc(16), y - sc(7), x + sc(7), y - sc(3));
  oled.drawLine(x + sc(7), y - sc(3), x + sc(16), y + sc(1));
  const int my = y + sc(7);
  oled.drawCircle(x, my, sc(5));
  oled.drawCircle(x, my, sc(4));
  static const int8_t PUFF_DIR[5][2] PROGMEM = {{-6, -1}, {6, -1}, {-4, 3}, {4, 3}, {0, 4}};
  const int r = 2 + (int)(ct / 150UL);
  for (uint8_t i = 0; i < 5; i++) {
    const int dx = (int8_t)pgm_read_byte(&PUFF_DIR[i][0]);
    const int dy = (int8_t)pgm_read_byte(&PUFF_DIR[i][1]);
    const int px = x + dx * (int)(ct / 100UL);
    const int py = my + dy * (int)(ct / 100UL);
    if (onScreen(px, py, r)) oled.drawCircle(px, py, r);
    // Spit flecks fly ahead of the cloud.
    const int sx = x + dx * (int)(ct / 60UL), sy = my + dy * (int)(ct / 60UL);
    if (onScreen(sx, sy, 0)) oled.drawPixel(sx, sy);
  }
}

void drawHmphFace(int x, int y) {
  // Unimpressed: half-shut eyes looking away, flat little mouth.
  drawGooglyEye(x, y, -10, -4, 8, 3, -4, 2);
  drawGooglyEye(x, y, 11, -3, 6, 2, -3, 2);
  drawLid(x, y, -10, -4, 8, -1, -1, true, false);
  drawLid(x, y, 11, -3, 6, -1, -1, false, false);
  oled.drawHLine(x - sc(4), y + sc(8), sc(8) + 1);
}

void drawShockFace(int x, int y) {
  // Clutching his pearls: huge eyes, pinprick pupils, gaping mouth.
  drawGooglyEye(x, y, -10, -4, 8, 1, 0, 0);
  drawGooglyEye(x, y, 11, -3, 6, 1, 0, 0);
  oled.drawCircle(x, y + sc(8), sc(3));
}

// Lying flat on the floor, sobbing and wailing. The body is flattened, so
// these offsets are tuned for the squashed shape.
void drawSobFace(int x, int y, uint32_t now) {
  // Eyes squeezed shut into > <.
  oled.drawLine(x - 18, y - 6, x - 10, y - 3);
  oled.drawLine(x - 10, y - 3, x - 18, y);
  oled.drawLine(x + 18, y - 6, x + 10, y - 3);
  oled.drawLine(x + 10, y - 3, x + 18, y);
  // Anime tear-streams pouring down both cheeks.
  const int flow = (int)((now / 60UL) % 3);
  for (int yy = y + 2; yy <= y + 9; yy++) {
    if ((yy + 3 - flow) % 3 == 0) continue;  // gaps that run downward
    oled.drawHLine(x - 15, yy, 2);
    oled.drawHLine(x + 14, yy, 2);
  }
  // Wailing mouth that stretches open and shut: "waaah... WAAAAH".
  const int h = 4 + (int)((now / 160UL) % 4);
  oled.drawRBox(x - 6, y + 1, 13, h, 2);
  oled.setDrawColor(0);
  oled.drawRBox(x - 5, y + 2, 11, h - 2, 1);
  oled.setDrawColor(1);
  // More tears squirting out sideways in arcs and splashing on the floor.
  for (uint8_t k = 0; k < 4; k++) {
    const uint32_t tt = (now + k * 150UL) % 600UL;
    const int dist = (int)(tt / 25UL);
    const int yy = y - 3 + (int)(tt * tt / 14000UL) - (int)(tt / 50UL);
    if (yy + 1 > FLOOR_Y) continue;
    if (onScreen(x - 21 - dist, yy, 1)) oled.drawBox(x - 22 - dist, yy, 2, 2);
    if (onScreen(x + 21 + dist, yy, 1)) oled.drawBox(x + 21 + dist, yy, 2, 2);
  }
  // Puddles of tears on both sides.
  oled.drawHLine(x - 50, FLOOR_Y, 8);
  oled.drawHLine(x + 43, FLOOR_Y, 8);
  // Wavy moan lines wafting up from his misery.
  const int rise = (int)((now / 70UL) % 10);
  for (int8_t side = -1; side <= 1; side += 2) {
    const int mx = x + side * 9, my = y - 17 - rise;
    if (onScreen(mx, my, 4)) {
      oled.drawLine(mx - 4, my, mx - 2, my - 2);
      oled.drawLine(mx - 2, my - 2, mx, my);
      oled.drawLine(mx, my, mx + 2, my - 2);
      oled.drawLine(mx + 2, my - 2, mx + 4, my);
    }
  }
}

// A heavy eyelid lowered to `edge` (offset from the eye centre), with a white
// line along its rim so the eye reads as closing rather than vanishing.
void drawSleepyLid(int x, int y, int ox, int oy, int r, int edge, bool innerOnRight) {
  drawLid(x, y, ox, oy, r, edge, edge, innerOnRight, false);
  const int e = edge < 0 ? -edge : edge;
  if (e >= r) return;
  const int hw = r - e * e / (2 * r);  // close enough to the circle, and small
  oled.drawHLine(x + ox - hw, y + oy + edge, 2 * hw + 1);
}

// Little z's drifting up beside his head, each one appearing only after he
// has actually fallen asleep.
void drawZs(int x, int y, uint32_t asleepFor) {
  for (uint8_t k = 0; k < 3; k++) {
    const uint32_t tt = (asleepFor + k * 700UL) % 2100UL;
    if (asleepFor < tt) continue;
    const int s = 2 + (int)(tt / 700UL);
    // Float out from beside his head, toward whichever side has more room.
    const int side = x > 64 ? -1 : 1;
    const int zx = x + side * (24 + (int)(tt / 120UL)) - (side < 0 ? s : 0);
    const int zy = y - 6 - (int)(tt / 200UL);
    if (!onScreen(zx + s / 2, zy + s / 2, s / 2 + 1)) continue;
    oled.drawHLine(zx, zy, s + 1);
    oled.drawLine(zx + s, zy, zx, zy + s);
    oled.drawHLine(zx, zy + s, s + 1);
  }
}

void drawCalmFace(int x, int y, uint32_t now, bool drool, uint16_t drowse) {
  // Each pupil wanders on its own schedule, so the eyes never quite agree.
  static const int8_t LEFT_WANDER[6][2] PROGMEM = {{-3, 2}, {-4, 0}, {-2, 3}, {0, 3}, {-3, -2}, {-4, 1}};
  static const int8_t RIGHT_WANDER[5][2] PROGMEM = {{2, -2}, {3, 0}, {1, -3}, {2, 2}, {3, -1}};
  const uint8_t lg = (now / 700UL) % 6;
  const uint8_t rg = (now / 1100UL) % 5;
  const int lpx = (int8_t)pgm_read_byte(&LEFT_WANDER[lg][0]);
  const int lpy = (int8_t)pgm_read_byte(&LEFT_WANDER[lg][1]);
  const int rpx = (int8_t)pgm_read_byte(&RIGHT_WANDER[rg][0]);
  const int rpy = (int8_t)pgm_read_byte(&RIGHT_WANDER[rg][1]);
  const uint32_t blinkT = now % BLINK_EVERY_MS;
  const bool leftShut = !drowse && blinkT < BLINK_SHUT_MS;
  const bool rightShut = !drowse && blinkT >= BLINK_LAG_MS &&
                         blinkT < BLINK_LAG_MS + BLINK_SHUT_MS;

  // Mismatched eyes: a huge one and a smaller one, set a bit crooked.
  // They blink one after the other, never together.
  if (leftShut) oled.drawHLine(x - 18, y - 4, 16);
  else drawGooglyEye(x, y, -10, -4, 8, 3, lpx, lpy);
  if (rightShut) oled.drawHLine(x + 5, y - 3, 13);
  else drawGooglyEye(x, y, 11, -3, 6, 2, rpx, rpy);
  if (drowse) {
    // Lids sink until only a sliver of eye is left.
    drawSleepyLid(x, y, -10, -4, 8, lerpInt(-9, 6, drowse), true);
    drawSleepyLid(x, y, 11, -3, 6, lerpInt(-7, 4, drowse), false);
  }
  drawDerpMouth(x, y, now, drool);
}

// How much he leans back (inhale) or lunges (cough) during the in-your-face
// cycle, in 16ths of scale, eased so he never pops between sizes.
int8_t lungeAmount(uint32_t c) {
  if (c < 600UL) return 0;
  if (c < 1000UL) return -(int8_t)(easeInOut(c - 600UL, 150UL) * 2 / 256);
  if (c < 1500UL) return (int8_t)(lerpInt(-2, 2, easeInOut(c - 1000UL, 80UL)));
  return (int8_t)(lerpInt(2, 0, easeInOut(c - 1500UL, 150UL)));
}

void drawPet(uint32_t now, bool fault) {
  const uint32_t travel = travelTime(now);
  const int bx = bounceAxis(travel, BOUNCE_LEFT, BOUNCE_RIGHT);
  const int by = bounceAxis(travel, BOUNCE_TOP, BOUNCE_BOTTOM);
  const uint32_t t = now - moodSince;
  const uint16_t drowse = drowsiness(now);
  int x = bx, y = by;
  uint8_t squash = 16, stretch = 16;
  const uint8_t shape = (now / 350UL) % 4;
  uint32_t coughT = 0;
  Face face = fault ? FACE_FAULT : FACE_CALM;
  petShout = SHOUT_NONE;
  petScale = 16;

  if (mood == MOOD_ANGRY) {
    face = FACE_ANGRY;
    if (wokeFromNap && t < JOLT_MS) {
      // Jolted awake mid-nap: leaps up with a start.
      face = FACE_SHOCK;
      petShout = SHOUT_HUH;
      const uint32_t p = t < JOLT_MS / 2 ? t : JOLT_MS - t;
      x = moodFromX;
      y = moodFromY - (int)(p * 12UL / JOLT_MS);
      petScale = moodFromScale;
    } else if (t < STOP_MS) {
      // Stops dead and trembles with rage, settling back to his normal shape.
      const uint16_t u = easeInOut(t, STOP_MS / 3);
      x = moodFromX + (((now / 60UL) % 2) ? 1 : -1);
      y = moodFromY;
      petScale = moodFromScale;
      squash = lerpInt(moodFromSquash, 16, u);
      stretch = lerpInt(moodFromStretch, 16, u);
    } else if (t < STOP_MS + RUN_MS) {
      // Stomps right up to your face, hops shrinking as he arrives.
      const uint16_t u = easeInOut(t - STOP_MS, RUN_MS);
      x = lerpInt(moodFromX, FACE_X, u);
      y = lerpInt(moodFromY, FACE_Y, u) - ((((now / 80UL) % 2) && u < 230) ? 2 : 0);
      petScale = lerpInt(moodFromScale, FACE_SCALE, u);
    } else {
      // Glare, suck in a breath, KOFF in your face, glare...
      const uint32_t c = (t - STOP_MS - RUN_MS) % COUGH_CYCLE_MS;
      const int8_t lunge = lungeAmount(c);
      x = FACE_X;
      y = FACE_Y;
      petScale = FACE_SCALE + lunge;
      if (c >= 600UL && c < 1000UL) {
        face = FACE_INHALE;
      } else if (c >= 1000UL && c < 1500UL) {
        face = FACE_COUGH;
        squash = lerpInt(16, 14, easeInOut(c - 1000UL, 80UL));
        coughT = c - 1000UL;
        x += ((now / 50UL) % 2) ? 2 : -2;
        petShout = SHOUT_KOFF;
      } else if (c >= 1500UL) {
        squash = lerpInt(14, 16, easeInOut(c - 1500UL, 150UL));
      }
    }
  } else if (mood == MOOD_DRAMA) {
    if (t < SHOCK_MS) {
      // Reels back, stunned that the air is STILL bad.
      face = FACE_SHOCK;
      const uint16_t u = easeInOut(t, SHOCK_MS);
      x = moodFromX;
      y = moodFromY;
      petScale = lerpInt(moodFromScale, 18, u);
      squash = lerpInt(moodFromSquash, 18, u);
      stretch = lerpInt(moodFromStretch, 14, u);
    } else if (t < SHOCK_MS + COLLAPSE_MS) {
      // Faints and crumples onto the floor, splatting a bit flatter on impact.
      face = FACE_SHOCK;
      const uint16_t u = easeInOut(t - SHOCK_MS, COLLAPSE_MS);
      x = lerpInt(moodFromX, 64, u);
      petScale = lerpInt(18, 16, u);
      squash = lerpInt(18, FLAT_SQUASH - 2, u);
      stretch = lerpInt(14, FLAT_STRETCH, u);
      // Keep the bottom on its way to the floor as he flattens.
      const int fromBottom = moodFromY + 15 * 18 * 18 / 256;  // scale 18, squash 18
      y = lerpInt(fromBottom, FLOOR_Y, u) - sc(15) * squash / 16;
    } else {
      // Lies there writhing and wailing like it is the end of the world.
      const uint32_t lying = t - SHOCK_MS - COLLAPSE_MS;
      face = FACE_SOB;
      // Rocking starts from the middle; the splat rebounds into heaving sobs.
      x = 64 + wobble(lying + 600UL, 2400UL, 8);
      squash = lerpInt(FLAT_SQUASH - 2, FLAT_SQUASH, easeInOut(lying, 250UL)) +
               ((lying > 250UL && (lying / 400UL) % 2) ? 1 : 0);
      stretch = FLAT_STRETCH;
      y = FLOOR_Y - 15 * squash / 16;
      static const uint8_t WAILS[] = {SHOUT_UGH, SHOUT_DYING, SHOUT_WHY, SHOUT_SMALL_KOFF,
                                      SHOUT_MOM, SHOUT_BLEH, SHOUT_OVER};
      petShout = WAILS[(lying / 1100UL) % 7];
    }
  } else if (mood == MOOD_RETURN) {
    // Huffs and shrinks back into the bounce, wherever it has got to.
    const uint16_t u = easeInOut(t, RETURN_MS);
    x = lerpInt(moodFromX, bx, u);
    y = lerpInt(moodFromY, by, u);
    petScale = lerpInt(moodFromScale, 16, u);
    squash = lerpInt(moodFromSquash, 16, u);
    stretch = lerpInt(moodFromStretch, 16, u);
    face = FACE_HMPH;
  } else if (drowse) {
    // Dozing: slow, deep breaths once he has stopped.
    if (drowse >= 256) {
      squash = 16 + wobble(t - SLEEP_AFTER_MS - SLOW_MS + 750UL, 3000UL, 1);
    }
  } else if (mood == MOOD_CALM && !fault) {
    // DVD corner hit! Only if he was already calm when he reached it, and
    // only if the party is over before he starts getting sleepy.
    const uint32_t steps = travel / BOUNCE_MS_PER_PX;
    const uint32_t sinceCorner = (steps % CORNER_EVERY_STEPS) * BOUNCE_MS_PER_PX +
                                 travel % BOUNCE_MS_PER_PX;
    if (sinceCorner < CELEBRATE_MS && t >= sinceCorner &&
        t - sinceCorner + CELEBRATE_MS <= SLEEP_AFTER_MS) {
      face = FACE_CORNER;
      petShout = SHOUT_CORNER;
      // Happy little hops that start and end on the ground, so they blend in.
      const uint32_t p = sinceCorner % HOP_MS;
      const int hop = (int)((p < HOP_MS / 2 ? p : HOP_MS - p) * 8UL / HOP_MS);
      y -= hop;
      squash = 16 + hop / 2;
    }
  } else if (mood == MOOD_GETUP) {
    // Springs up tall off the floor, then strolls back into the bounce
    // like nothing happened.
    face = FACE_INNOCENT;
    const int springY = FLOOR_Y - 15 * 20 / 16;
    if (t < SPRING_MS) {
      const uint16_t u = easeInOut(t, SPRING_MS);
      x = moodFromX;
      y = lerpInt(moodFromY, springY, u);
      petScale = lerpInt(moodFromScale, 16, u);
      squash = lerpInt(moodFromSquash, 20, u);
      stretch = lerpInt(moodFromStretch, 13, u);
    } else {
      const uint16_t u = easeInOut(t - SPRING_MS, GETUP_MS);
      x = lerpInt(moodFromX, bx, u);
      y = lerpInt(springY, by, u);
      squash = lerpInt(20, 16, u);
      stretch = lerpInt(13, 16, u);
    }
  }

  // Never let any part of him leave the screen.
  const int halfW = sc(29) * stretch / 16 + 1, top = sc(20) * squash / 16;
  const int bottom = FLOOR_Y - sc(15) * squash / 16;
  if (x < halfW) x = halfW;
  if (x > 127 - halfW) x = 127 - halfW;
  if (y > bottom) y = bottom;
  if (y < top) y = top;
  shownX = x;
  shownY = y;
  shownScale = petScale;
  shownSquash = squash;
  shownStretch = stretch;

  oled.setDrawColor(1);
  drawBlob(x, y, shape, squash, stretch);
  if (face != FACE_SOB) drawBlush(x, y, 3);

  switch (face) {
    case FACE_ANGRY: drawAngryFace(x, y, now, false); break;
    case FACE_INHALE: drawAngryFace(x, y, now, true); break;
    case FACE_COUGH: drawCoughFace(x, y, coughT); break;
    case FACE_HMPH: drawHmphFace(x, y); break;
    case FACE_SHOCK: drawShockFace(x, y); break;
    case FACE_SOB: drawSobFace(x, y, now); break;
    case FACE_INNOCENT: drawCalmFace(x, y, now, false, 0); break;
    case FACE_CORNER: drawCornerFace(x, y, now); break;
    // A sensor fault keeps his normal face (minus drool); the label says why.
    default: drawCalmFace(x, y, now, !fault, drowse); break;
  }
  if (drowse >= 256) drawZs(x, y, t - SLEEP_AFTER_MS - SLOW_MS);

  // No falling drool during a party: after a corner wake-up, drops from the
  // nap would otherwise be placed as if he had been bouncing at full speed.
  if (mood == MOOD_CALM && !fault && face != FACE_CORNER) drawFallingDrool(now);
}

void drawScreen(uint32_t now) {
  const bool fresh = freshReading(now);
  const bool fault = !sensorStarted || readError ||
                     (!fresh && (uint32_t)(now - startedAt) >= STALE_MS);
  const bool coughing = demoActive || (fresh && vocTriggered);
  const bool hasLabel = demoActive || fault;

  // Decide his mood once per frame, then freeze it for all eight strips.
  updateMood(now, coughing);
  oled.firstPage();
  do {
    drawPet(now, fault && !demoActive);

    // Text sits on black strips on top, so he can pass behind it.
    const int shoutX = 128 - 5 * SHOUT_LEN[petShout];
    oled.setDrawColor(0);
    oled.drawBox(0, 57, 128, 7);
    if (hasLabel) oled.drawBox(0, 0, 64, 9);
    if (petShout != SHOUT_NONE) oled.drawBox(shoutX - 2, 0, 130 - shoutX, 9);
    oled.setDrawColor(1);

    oled.setFont(u8g2_font_5x7_tr);
    oled.setCursor(0, 8);
    if (demoActive) oled.print(F("DEMO COUGH"));
    else if (fault) oled.print(F("SENSOR ISSUE"));
    if (petShout != SHOUT_NONE) {
      oled.setCursor(shoutX, 8);
      printShout(petShout);
    }

    oled.setCursor(0, 63);
    if (fresh) {
      oled.print(F("VOC: "));
      oled.print(tvoc);
      oled.print(F(" ppb"));
    } else oled.print(F("VOC: -- ppb"));
  } while (oled.nextPage());
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setWireTimeout(25000, true);
  oled.begin();
  oled.setContrast(100);
  sensorStarted = sensor.begin(0x5A);
  startedAt = millis();
}

void loop() {
  uint32_t now = millis();

  while (Serial.available()) {
    char command = Serial.read();
    if (command == 'c' || command == 'C') {
      demoActive = true;
      demoStartedAt = now;
    }
  }
  if (demoActive && (uint32_t)(now - demoStartedAt) >= DEMO_MS) demoActive = false;

  pollSensor(now);
  if (!freshReading(now)) clearTrigger();
  drawScreen(now);
  delay(50);
}
