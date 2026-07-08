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

The `O` and `E` counters are capped at `9999` on the OLED. The full counters still exist internally.

## Data Sources

CRSF values come from incoming receiver-to-board CRSF frames on the CRSF UART. The parser watches RC frames and link-statistics frames, including `0x14`, `0x1C`, and `0x1D`.

GPS values come from the M10Q NMEA stream. The display does not change the GPS telemetry schedule; valid fresh GPS fixes are still sent to the receiver once per second.
