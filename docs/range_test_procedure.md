# Range Test Procedure

## Bench Bring-Up

1. Power the RP2 from a stable 5 V supply.
2. Connect Teensy, RP2, GPS, and supply grounds.
3. Wire Teensy TX1 to RP2 RX.
4. Wire RP2 TX to Teensy RX1.
5. Wire GPS TX to Teensy RX2.
6. Wire GPS RX to Teensy TX2.
7. Upload the firmware.
8. Open the USB serial monitor at 115200 baud.
9. Wait for `sent GPS ...` once per second. Until the GPS has a fresh fix, the monitor prints `waiting for GPS fix ...` with baud, RX byte, and checksum counters.
10. Bind the RP2 to the RadioMaster Pocket internal ELRS module.
11. On EdgeTX, delete old telemetry sensors if needed.
12. Select telemetry discovery and wait 10 to 20 seconds.

Expected sensors may include GPS coordinates, GSpd, Hdg, Alt, Sats, RSSI, LQ, SNR, RFMD, and TPWR.

If EdgeTX still shows the old fixed coordinate, power-cycle or delete/re-discover telemetry after uploading this firmware. The current firmware prints `No fixed-coordinate fallback is compiled in` at startup and does not contain the old dummy coordinate.

## Logging

Recommended EdgeTX special function:

```text
SF1: chosen switch -> SD Logs -> 1.0s -> enabled checkbox checked
```

Keep the first tests simple and repeatable:

```text
Packet rate:     50 Hz or 100 Hz
Telemetry ratio: 1:2 or 1:4
TX power:        fixed low power on bench
Dynamic power:   off
```

## Outdoor Test Notes

Record these before walking away from the base position:

```text
Base latitude:
Base longitude:
RP2 antenna orientation:
Pocket antenna orientation:
TX power:
Packet rate:
Telemetry ratio:
Weather / obvious obstructions:
```

After the test, stop logging and wait a few seconds before removing the SD card or entering USB Storage mode.
