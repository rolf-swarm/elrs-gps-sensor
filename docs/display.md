# OLED Display Reference

The optional VMA438 / WPI438 OLED is a 128x64 SSD1306 I2C display. The firmware probes `0x3C` and `0x3D` at startup. If no display answers, display output is disabled and CRSF GPS telemetry continues normally.

On startup, a detected display briefly shows:

```text
ELRS GPS Sensor

OLED online
```

After that, the status screen refreshes about four times per second.

## Example Screen

```text
CRSF OK RF 2
LQ  98% RSSI -54
SNR 12 P 100mW
GPS FIX sats 10
Age 120ms HDOP0.8
B115k O123 E0
```

## Line 1: CRSF State

```text
CRSF WAIT
CRSF OK RF 2
CRSF OK FPS 500
CRSF LOST
```

`CRSF WAIT` means no valid incoming CRSF frame has been received yet.

`CRSF OK` means the receiver-to-board CRSF UART is alive. The firmware currently treats the stream as alive when a valid CRSF frame arrived within roughly the last 700 ms.

`CRSF LOST` means valid CRSF frames were seen before, but the stream is now stale.

If fresh link statistics are available, the same line also shows either `RF` profile information or `FPS` when CRSF v3 transmitter link statistics provide a packet/rate value.

## Line 2: Link Quality and RSSI

```text
LQ  98% RSSI -54
LQ  --  RSSI --
```

`LQ` is uplink link quality percent from CRSF link statistics. In ExpressLRS terms this is the transmitter-to-receiver radio link quality.

`RSSI` is uplink RSSI in dBm. When the receiver reports the active antenna, the display picks the matching RSSI value. `--` means no fresh link statistics are available.

## Line 3: SNR and Power

```text
SNR 12 P 100mW
SNR 12 P --
SNR -- P --
```

`SNR` is uplink signal-to-noise ratio in dB from CRSF link statistics.

`P` is transmitter power decoded from the CRSF power-level field. `P --` means the receiver did not provide a known power value in the latest parsed statistics.

## Line 4: GPS Fix

```text
GPS FIX sats 10
GPS WAIT sats 8
```

`GPS FIX` means the GPS location is valid and fresh enough to be sent as CRSF GPS telemetry.

`GPS WAIT` means the firmware is still receiving GPS data but does not currently have a fresh valid location. `sats` uses the best available satellite count: satellites used in the fix first, then satellites in view if that is all the GPS has reported.

## Line 5: GPS Age and HDOP

```text
Age 120ms HDOP0.8
Age9999ms HDOP --
```

`Age` is the GPS location age in milliseconds. The display caps the shown value at `9999ms` so the text stays inside the 128 px OLED width.

`HDOP` is horizontal dilution of precision. Smaller is better. `HDOP --` means the GPS has not reported a valid HDOP value.

## Line 6: GPS UART and Checksums

```text
B115k O123 E0
```

`B` is the active GPS UART baud in kbaud. For example, `B115k` means `115200`.

`O` is the number of NMEA sentences with a valid checksum.

`E` is the number of NMEA sentences with a failed checksum.

The `O` counter is capped at `999999` on the OLED. The `E` counter is capped at `9999`. The full counters still exist internally.

## Data Sources

CRSF values come from incoming receiver-to-board CRSF frames on the CRSF UART. The parser watches RC frames and link-statistics frames, including `0x14`, `0x1C`, and `0x1D`.

GPS values come from the M10Q NMEA stream. The display does not change the GPS telemetry schedule; valid fresh GPS fixes are still sent to the receiver once per second.

## Reading Quality Values

These values are practical bench/range-test hints, not absolute flight rules. ExpressLRS behavior depends on packet rate, RF mode, antennas, transmit power, environment, and how much fade margin you want.

## CRSF State

```text
CRSF OK
CRSF LOST
```

Good: `CRSF OK` is stable.

Medium: `CRSF OK` appears, but occasionally flickers to `CRSF LOST` while everything is powered and close together. That points to UART wiring, baud/protocol setup, or power noise.

Bad: `CRSF WAIT` never changes, or `CRSF LOST` stays on screen. The board is not receiving valid CRSF frames from the receiver.

## LQ

```text
LQ  98%
```

Good: `95-100%`. This is what you normally want to see nearby and during a healthy link.

Medium: `70-94%`. The link is working, but margin is reduced. During a range test this may be expected at distance.

Bad: below `70%`, especially if it drops quickly or does not recover. Treat this as a warning sign for antenna placement, RF power, RF mode, or receiver orientation.

## RSSI

```text
RSSI -54
```

RSSI is shown in dBm. Values closer to zero are stronger.

Good: about `-40` to `-75 dBm`.

Medium: about `-76` to `-90 dBm`.

Bad: weaker than about `-90 dBm`, especially together with falling `LQ` or negative `SNR`.

RSSI alone is less useful than `LQ` and `SNR`. A low RSSI can still work if the link quality is solid.

## SNR

```text
SNR 12
```

SNR is signal-to-noise ratio in dB. Higher is better.

Good: `10 dB` or more.

Medium: `3-9 dB`.

Bad: `0 dB` or below, especially if `LQ` is also falling.

## Power

```text
P 100mW
```

Good: the shown power matches what you expect from the transmitter settings or dynamic-power behavior.

Medium: power is higher than expected, but `LQ` and `SNR` are still healthy. Dynamic power may simply be adding margin.

Bad: power is high while `LQ` is poor. That usually means the link is struggling despite extra transmit power.

## GPS Fix, Satellites, Age, and HDOP

```text
GPS FIX sats 10
Age 120ms HDOP0.8
```

Good: `GPS FIX`, `8+` satellites, `Age` below `1000ms`, and `HDOP` below about `1.5`.

Medium: `GPS FIX`, `5-7` satellites, `Age` below `3000ms`, or `HDOP` around `1.5-3.0`.

Bad: `GPS WAIT`, fewer than `5` satellites, `Age` stuck high, or `HDOP` above `3.0`. Move the GPS antenna to open sky and away from noisy electronics.

## GPS Checksum Counters

```text
O123 E0
```

Good: `O` increases steadily and `E` stays at `0` or only rarely increments.

Medium: `E` increments occasionally while `O` is still increasing quickly. This can happen with marginal wiring or electrical noise.

Bad: `E` increases often, or `O` does not increase. Check GPS baud, GPS TX/RX wiring, ground, and power.

## Possible Optimizations

Use the display values together. For example, low `RSSI` with healthy `LQ` may still be fine, but low `LQ`, poor `SNR`, and high power at the same time means the RF link is running out of margin.

## RF Link Optimizations

Improve antennas first. Use a known-good receiver antenna, keep the active antenna element clear of carbon fiber, batteries, GPS modules, power wiring, and metal parts, and avoid folding or tightly bending the coax. If the transmitter antenna is removable, use the correct band antenna and keep it oriented sensibly for the test.

Increase transmit power when needed. Higher power can improve range, but it also increases current draw, heat, and possible RF noise near other electronics. If dynamic power is enabled, the displayed `P` value can help show whether ELRS is already raising power to maintain the link.

Try a lower packet rate for more range. Lower ExpressLRS packet rates generally improve link budget and penetration, while higher packet rates reduce latency. If `LQ` falls early during range testing, lowering the packet rate is often more useful than only increasing power.

Use a more range-oriented RF mode if your setup allows it. The displayed `RF` or `FPS` value helps confirm which mode/rate is currently active.

## GPS Optimizations

Place the GPS antenna with a clear sky view. Keep it away from the receiver antenna, switching regulators, high-current battery wires, ESCs, and noisy digital wiring.

Wait for a real fix before judging performance. A cold-start GPS can take time. Good signs are `GPS FIX`, increasing satellites, low `Age`, and improving `HDOP`.

If `HDOP` stays high or satellites stay low, try a different antenna orientation or location. A small position change can matter a lot indoors or near buildings.

## Wiring and Power Optimizations

Use a solid common ground between the board, RP2 receiver, GPS, OLED, and power supply.

Keep CRSF UART wires short and tidy. CRSF runs at `420000` baud, so poor grounding, long loose wires, or noisy routing can cause framing or checksum issues.

Power the OLED from `3.3 V`, not `5 V`, so its I2C pullups do not expose the board pins to 5 V.

If GPS checksum errors `E` increase often, check GPS TX/RX wiring, ground quality, power stability, and whether the GPS UART baud is being detected correctly.

If CRSF state flickers between `OK` and `LOST` at close range, check receiver serial protocol, CRSF baud, RP2 TX/RX crossing, and the physical UART wiring before tuning RF settings.

## Tunable Parameters

These are the main parameters worth tuning:

- ExpressLRS packet rate: lower rates usually give better range and link margin; higher rates give lower latency.
- ExpressLRS transmit power: higher power improves margin but costs current and heat.
- ExpressLRS dynamic power: can automatically raise power when link margin drops.
- Antenna choice and placement: often more important than a small power change.
- Board pin mapping: TinyPICO CRSF, GPS, and OLED pins are set in `platformio.ini` with `CRSF_RX_PIN`, `CRSF_TX_PIN`, `GPS_RX_PIN`, `GPS_TX_PIN`, `OLED_SDA_PIN`, and `OLED_SCL_PIN`.
- GPS baud scan list: currently `9600`, `38400`, `57600`, and `115200` in `src/gps_source.cpp`.
- GPS update rate: currently `1 Hz` in `src/gps_source.cpp`.
- CRSF GPS telemetry period: currently one GPS frame per second in `src/main.cpp`.
- OLED refresh period: currently about four updates per second in `src/status_display.cpp`.

For this project, the best first tuning path is usually: verify wiring and power, get stable `CRSF OK`, get `GPS FIX`, improve antenna placement, then adjust ELRS packet rate and transmit power during a controlled range test.
