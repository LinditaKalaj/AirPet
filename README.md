# AirPet

A googly-eyed blob that lives on a small OLED screen and reacts to the air around it.

He bounces around the screen like the old DVD logo. When a CCS811 gas sensor picks up
VOCs (perfume, snacks, hand sanitizer, cooking smells), he stops, fumes, charges up to
your face and coughs at you. If the air stays bad, he collapses on the floor and wails
until it clears. If things stay calm for a while he slows down and falls asleep.

![AirPet preview](AirPet-preview.gif)

> AirPet is a toy that responds to VOCs. It is **not** an air-quality or safety monitor.
> The thresholds are tuned for fun, not health limits.

---

## Components

| # | Part | Notes |
|---|------|-------|
| 1 | **SparkFun Pro Micro**, ATmega32U4, 5V / 16 MHz | The microcontroller. Any Pro Micro clone with the same chip works. |
| 1 | **CCS811 air-quality sensor breakout** | Measures TVOC (ppb) and eCO2 (ppm) over I²C, address `0x5A`. |
| 1 | **1.3" 128×64 OLED, SH1106 driver, 7-pin SPI** | Pins: GND, VCC, CLK, MOSI, RES, DC, CS. |
| – | Jumper wires + breadboard | |
| 1 | Micro-USB cable | For power and uploading. |

## Wiring

**Unplug USB before changing any wires.**

### CCS811 sensor → Pro Micro

| CCS811 | Pro Micro |
|--------|-----------|
| VCC    | VCC       |
| GND    | GND       |
| SDA    | 2 (SDA)   |
| SCL    | 3 (SCL)   |
| WAKE   | GND (keeps the sensor awake) |
| RST    | not connected |
| INT    | not connected |

### OLED → Pro Micro

| OLED | Pro Micro |
|------|-----------|
| VCC  | VCC       |
| GND  | GND       |
| CLK  | 15 (SCK)  |
| MOSI | 16 (MOSI) |
| RES  | 8         |
| DC   | 9         |
| CS   | 10        |

## Software

- **Arduino IDE** 2.3.10
- **Board:** SparkFun Pro Micro (ATmega32U4, 5V, 16 MHz). Install SparkFun's AVR
  boards package, or pick "Arduino Leonardo" if you don't have it, since the chip is the same.

### Libraries

Install these with **Tools → Manage Libraries**:

| Library | Version tested | Used for |
|---------|----------------|----------|
| U8g2 | 2.36.19 | Drawing on the SH1106 OLED |
| Adafruit CCS811 Library | 1.1.3 | Reading the CCS811 sensor |
| Adafruit BusIO | 1.17.4 | Dependency of the CCS811 library |

`SPI.h` and `Wire.h` are built in.

> Adafruit GFX and Adafruit SSD1306 are also installed on this machine, but AirPet
> doesn't use them. This display uses the SH1106 driver, which U8g2 handles.

## Uploading

1. Open `AirPet.ino` in the Arduino IDE.
2. Select the board and its USB port.
3. Click **Upload**.

The sketch writes nothing to Serial, which saves flash space. Sensor status shows on the
OLED instead.

## What's on the screen

- **Bottom line:** current VOC reading, e.g. `VOC: 42 ppb` (`--` while there's no fresh reading). Hidden when `SHOW_VOC` is `false`.
- **Top left:** `SENSOR ISSUE` if the CCS811 isn't responding, or `DEMO COUGH` during a demo.
- **Top right:** whatever he's shouting (`HUH?!`, `KOFF!`, `I'M DYING`, `WHY ME?!`,
  `MOMMY`, `IT'S OVER`, `CORNER!!` ...).

To hide the VOC number and keep just the pet, set this near the top of `AirPet.ino` and upload again:

```cpp
const bool SHOW_VOC = false;
```

He still reacts to the air the same way. Only the number is hidden.

## How he behaves

![AirPet moods: calm, angry, shocked, collapsed, sleepy](AirPet-preview.png)

| State | What happens |
|-------|--------------|
| **Calm** | Bounces around at 10 px/s, blinking and drooling. |
| **Corner!** | About every 2 min 15 s he lands exactly in a corner and hops around celebrating. |
| **Sleepy** | After 2.5 min of calm he slows to 1/10 speed and dozes off (Zzz). |
| **Angry** | VOC reaches **150 ppb** or more: he freezes, then charges at you and coughs. |
| **Drama** | Bad air for **6 s**: he gets a shock, collapses flat and sobs. |
| **Recovery** | After **3** readings in a row at or below **100 ppb** he gets back up and goes back to bouncing. |

### Demo mode

Open **Serial Monitor at 9600 baud** and send `c`. He'll do a 15-second demo cough,
long enough for him to collapse. The real sensor reading isn't changed.

## Tuning

These constants are near the top of `AirPet.ino`:

| Constant | Default | Meaning |
|----------|---------|---------|
| `COUGH_ON_PPB` | 150 | TVOC level that sets him off |
| `COUGH_OFF_PPB` | 100 | TVOC level he calms down at |
| `HIGH_SAMPLES_REQUIRED` | 1 | Bad readings in a row before reacting (raise it if single spikes set him off) |
| `LOW_SAMPLES_REQUIRED` | 3 | Good readings in a row before recovering |
| `DEMO_MS` | 15000 | Length of the demo cough |
| `DRAMA_AFTER_MS` | 6000 | How long bad air lasts before he collapses |
| `SLEEP_AFTER_MS` | 150000 | How long he stays calm before getting sleepy |
| `SHOW_VOC` | true | Show the VOC reading at the bottom of the screen (`false` hides it) |

## Troubleshooting

- **`SENSOR ISSUE` on screen:** check the SDA/SCL wires and confirm WAKE is tied to GND.
  The CCS811 has to be at address `0x5A`.
- **Readings look odd at first:** the CCS811 needs some time to warm up, and its eCO2
  value starts at 400 ppm. Readings below that are treated as errors.
- **Blank screen:** check CLK/MOSI on pins 15/16 and RES/DC/CS on 8/9/10. Also make sure
  the display is an SH1106. An SSD1306 needs a different U8g2 constructor.
