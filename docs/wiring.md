# Wiring and UART Level Shifting

## Teensy 4.1 RP2 Wiring

Use receiver-side CRSF as a normal two-wire full-duplex UART.

```text
RP2 5V   -> stable 5 V supply
RP2 GND  -> Teensy GND and supply GND
RP2 TX   -> Teensy pin 0 / RX1
RP2 RX   -> Teensy pin 1 / TX1
```

```text
Baud:        420000
Format:      8N1
Inverted:    no
Half-duplex: no
```

## TinyPICO RP2 Wiring

The TinyPICO pins are configured in `[env:tinypico]` in `platformio.ini`.

```text
RP2 5V   -> stable 5 V supply
RP2 GND  -> TinyPICO GND and supply GND
RP2 TX   -> TinyPICO GPIO32 / CRSF RX
RP2 RX   -> TinyPICO GPIO33 / CRSF TX
```

## Teensy 4.1 GPS Module Wiring

The GPS module is on `Serial2`, because `Serial1` is reserved for CRSF telemetry to the RP2.

```text
GPS GND  -> Teensy GND and supply GND
GPS TX   -> Teensy pin 7 / RX2
GPS RX   -> Teensy pin 8 / TX2
```

The `GPS RX -> Teensy TX2` connection is needed for the firmware's UBX update-rate command. If you only wanted passive NMEA reading, `GPS TX -> Teensy RX2` would be the minimum receive-only wire.

The firmware scans these GPS baud rates until checksummed NMEA is seen:

```text
9600, 38400, 57600, 115200
```

The `gpstool` project defaults the M10Q to `9600`, but the scanner makes bench bring-up more forgiving if the module was changed later.

## TinyPICO GPS Module Wiring

```text
GPS GND  -> TinyPICO GND and supply GND
GPS TX   -> TinyPICO GPIO25 / GPS RX
GPS RX   -> TinyPICO GPIO26 / GPS TX
```

## Teensy 4.1 Optional OLED Wiring

The Velleman VMA438 / Whadda WPI438 OLED is a 128x64 SSD1306 I2C display. Power it from 3.3 V so the I2C pullups stay safe for the Teensy 4.1.

```text
OLED VCC -> Teensy 3.3 V
OLED GND -> Teensy GND
OLED SDA -> Teensy pin 18 / SDA
OLED SCL -> Teensy pin 19 / SCL
```

The board marking `0x78` is the shifted SSD1306 address. The firmware probes the normal 7-bit I2C addresses `0x3C` and `0x3D`. If no OLED answers, display output is disabled and GPS telemetry continues normally.

The OLED status lines are described in [display.md](display.md).

## TinyPICO Optional OLED Wiring

```text
OLED VCC -> TinyPICO 3.3 V
OLED GND -> TinyPICO GND
OLED SDA -> TinyPICO GPIO21 / SDA
OLED SCL -> TinyPICO GPIO22 / SCL
```

The TinyPICO OLED pins are also configurable through `OLED_SDA_PIN` and `OLED_SCL_PIN` in `platformio.ini`.

## Default Pins Used

```text
Teensy 4.1:
Serial1 RX pin 0 <- RP2 TX
Serial1 TX pin 1 -> RP2 RX
Serial2 RX pin 7 <- GPS TX
Serial2 TX pin 8 -> GPS RX
I2C SDA pin 18    -> OLED SDA
I2C SCL pin 19    -> OLED SCL

TinyPICO:
GPIO32 <- RP2 TX
GPIO33 -> RP2 RX
GPIO25 <- GPS TX
GPIO26 -> GPS RX
GPIO21 -> OLED SDA
GPIO22 -> OLED SCL
```

## Logic Levels

Teensy 4.1 and TinyPICO GPIO pins are 3.3 V logic and are not 5 V tolerant.

The RadioMaster RP2 UART pads should be treated as 3.3 V logic, so the normal direct wiring is correct:

```text
Board TX -> RP2 RX
RP2 TX   -> Board RX
```

## Easy Protection Options

For `board TX -> RP2 RX`, no level shifter is needed. A small series resistor, for example 220 Ohm to 1 kOhm, is okay for current limiting during mistakes, but do not add a resistor divider on this line.

For `RP2 TX -> board RX`, no level shifter is needed if the RP2 TX line measures around 3.3 V.

If you later use a different ELRS receiver or device with a known 5 V UART TX output, shift only the line going into the Teensy:

```text
5 V device TX -- 1 kOhm --+-- Teensy RX
                          |
                        2 kOhm
                          |
                         GND
```

This divides a 5 V high level to about 3.3 V. It is simple and fast enough for short 420000 baud UART wiring when built close to the Teensy input.

Only use that divider for a known 5 V TX source. If the source is already 3.3 V, the divider would reduce the signal too far. For a universal small-board solution, use a proper unidirectional 3.3 V logic buffer with 5 V tolerant input, such as SN74LVC1G17 or SN74LVC1T34 powered from 3.3 V.
