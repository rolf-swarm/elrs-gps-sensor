# CRSF GPS Telemetry Sensor

Small PlatformIO/Arduino firmware for sending GPS coordinates from a u-blox M10Q-style GPS module into a RadioMaster RP2 ExpressLRS receiver as standard CRSF GPS telemetry.

The GPS module is read as NMEA over UART with TinyGPSPlus. The board sends a CRSF GPS frame to the RP2 once per second. With a fresh position fix it sends real coordinates; without a fix it sends zero position/speed/heading/altitude plus the current satellite count. Teensy 4.1 and TinyPICO builds are supported.

## Build

```powershell
pio run -e teensy41
pio run -e tinypico
```

## Upload

```powershell
pio run -e teensy41 -t upload
pio run -e tinypico -t upload
```

## Serial Monitor

```powershell
pio device monitor -b 115200
```

## Teensy 4.1 Wiring

```text
RP2 5V   -> stable 5 V supply
RP2 GND  -> Teensy GND and supply GND
RP2 TX   -> Teensy pin 0 / RX1
RP2 RX   -> Teensy pin 1 / TX1

GPS GND  -> Teensy GND and supply GND
GPS TX   -> Teensy pin 7 / RX2
GPS RX   -> Teensy pin 8 / TX2

OLED VCC -> Teensy 3.3 V
OLED GND -> Teensy GND
OLED SDA -> Teensy pin 18 / SDA
OLED SCL -> Teensy pin 19 / SCL
```

## TinyPICO Wiring

The TinyPICO pin assignment is in `platformio.ini` under `[env:tinypico]` and can be changed there if your layout needs different GPIOs.

```text
RP2 5V   -> stable 5 V supply
RP2 GND  -> TinyPICO GND and supply GND
RP2 TX   -> TinyPICO GPIO32 / CRSF RX
RP2 RX   -> TinyPICO GPIO33 / CRSF TX

GPS GND  -> TinyPICO GND and supply GND
GPS TX   -> TinyPICO GPIO25 / GPS RX
GPS RX   -> TinyPICO GPIO26 / GPS TX

OLED VCC -> TinyPICO 3.3 V
OLED GND -> TinyPICO GND
OLED SDA -> TinyPICO GPIO21 / SDA
OLED SCL -> TinyPICO GPIO22 / SCL
```

Receiver-side CRSF is normal full-duplex UART:

```text
Baud:        420000
Format:      8N1
Inverted:    no
Half-duplex: no
```

## UART Level Shifting

For a RadioMaster RP2, the UART pads should be treated as 3.3 V logic, so the normal connection is direct. Teensy 4.1 and TinyPICO GPIO pins are 3.3 V logic and are not 5 V tolerant.

```text
Board TX 3.3 V -> RP2 RX
RP2 TX 3.3 V   -> Board RX
```

Do not use a resistor divider on `board TX -> RP2 RX`; it would reduce the already-correct 3.3 V signal and may make the receiver miss bytes at 420000 baud.

If you ever connect a different receiver or telemetry device whose TX output is truly 5 V, protect `device TX -> Teensy RX` with a simple divider:

```text
5 V device TX -- 1 kOhm --+-- Teensy RX
                          |
                        2 kOhm
                          |
                         GND
```

That gives about 3.3 V at the Teensy from a 5 V UART high. Use the divider only for a known 5 V source. If the source is already 3.3 V, connect it directly or use a proper 3.3 V logic buffer such as an SN74LVC1G17/SN74LVC1T34 powered from 3.3 V.

## Firmware Behavior

- USB debug serial at 115200 baud.
- CRSF UART at 420000 baud.
- GPS module UART auto-scanning `9600`, `38400`, `57600`, and `115200` baud.
- CRSF GPS frame sent once per second. Without a fresh GPS fix, position/speed/heading/altitude are sent as zero while satellite count is still reported.
- Incoming CRSF frames are parsed for RC/link status.
- Optional VMA438/WPI438 SSD1306 OLED on I2C address `0x3C` or `0x3D`; if not found, the firmware keeps running without display output.

## OLED Display

When an OLED is present, the firmware first shows `ELRS GPS Sensor` and `OLED online`, then updates the status screen about four times per second.

```text
CRSF OK RF 2
LQ  98% RSSI -54
SNR 12 P 100mW
GPS FIX S10 T123
Age 120ms HDOP0.8
B115k O123 E0
```

Line meaning:

- `CRSF WAIT`, `CRSF OK`, or `CRSF LOST`: incoming CRSF state from the receiver. `OK` means a valid CRSF frame was seen recently; `LOST` means the CRSF stream went stale.
- `RF` or `FPS`: RF profile/rate information from CRSF link statistics when available.
- `LQ`: uplink link quality percent.
- `RSSI`: uplink RSSI in dBm, using the active antenna when reported.
- `SNR`: uplink signal-to-noise ratio in dB.
- `P`: transmitter power decoded from CRSF link statistics. `P --` means it was not available.
- `GPS FIX`, `GPS OLD`, or `GPS WAIT`: whether the GPS location is fresh, stale, or not valid yet.
- `S`: best available satellite count.
- `T`: GPS CRSF telemetry frames attempted, capped on the display at `9999`.
- `Age`: GPS location age in milliseconds.
- `HDOP`: GPS horizontal dilution of precision when reported.
- `B`: active GPS UART baud in kbaud.
- `O` / `E`: GPS NMEA checksum OK/error counters, capped on the display at `9999`.

See [docs/display.md](docs/display.md) for the full display reference.

Default board pins used by this firmware:

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

The USB monitor prints the active GPS baud plus RX/checksum counters while waiting for a fix:

```text
waiting for GPS fix valid=0 baud=9600 rx_bytes=1234 cksum_ok=12 cksum_bad=0 ...
```

Quick interpretation:

```text
rx_bytes stays 0       -> wiring, GPS power, or wrong Teensy RX pin
rx_bytes increases but cksum_ok stays 0 -> likely wrong baud or noisy/inverted signal
cksum_ok increases but no fix -> GPS is talking; wait for sky view/antenna/fix
```

## Analysis Helper

After EdgeTX logging, compute distance from a fixed base coordinate:

```powershell
python tools/analyze_edgetx_log.py path\to\LOG.csv --base-lat 51.296994 --base-lon 9.451982 --output analyzed.csv
```

If the script cannot auto-detect the latitude and longitude columns, pass them explicitly:

```powershell
python tools/analyze_edgetx_log.py LOG.csv --base-lat 51.296994 --base-lon 9.451982 --lat-column GPSLat --lon-column GPSLon
```
