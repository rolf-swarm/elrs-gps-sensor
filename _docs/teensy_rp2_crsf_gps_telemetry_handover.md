# Teensy + RadioMaster RP2 CRSF GPS Telemetry Handover

## Goal

Build a small Teensy firmware that sends latitude and longitude into a RadioMaster RP2 ExpressLRS receiver as standard CRSF GPS telemetry.

The full range-test chain is:

```text
Teensy sensor/GPS source
  -> CRSF GPS telemetry over UART
  -> RadioMaster RP2 ELRS receiver
  -> ELRS RF link
  -> RadioMaster Pocket ELRS / EdgeTX
  -> EdgeTX telemetry sensors
  -> SD-card CSV log
  -> Python range analysis afterwards
```

The first proof should send fixed dummy coordinates once per second. After EdgeTX discovers and logs them correctly, replace the dummy coordinates with real GPS/sensor values.

---

## Hardware

### Devices

- RadioMaster Pocket with internal ELRS and EdgeTX
- RadioMaster RP2 ELRS 2.4 GHz receiver
- Teensy 4.1
- Stable 5 V supply for RP2
- Sensor or GPS source connected to Teensy later

### Important electrical notes

- The RP2 receiver power input expects 5 V.
- RP2 UART logic should be treated as 3.3 V logic.
- Do not drive the RP2 RX pad with 5 V logic.
- Teensy 4.1 UART pins are 3.3 V logic and are suitable directly for RP2 UART.
- Teensy 4.1 GPIO/UART pins are not 5 V tolerant. Do not connect 5 V logic to Teensy RX pins.
- All grounds must be common.

---

## Wiring

Use a normal two-wire full-duplex CRSF UART between Teensy and RP2.

```text
RP2 5V   -> stable 5 V supply
RP2 GND  -> Teensy GND and supply GND
RP2 TX   -> Teensy RX1
RP2 RX   -> Teensy TX1
```

Example using Teensy 4.1 `Serial1`:

```text
Teensy 4.1 pin 1  TX1 -> RP2 RX
Teensy 4.1 pin 0  RX1 <- RP2 TX
Teensy 4.1 GND        -> RP2 GND
```

Do not use the one-wire module-bay CRSF wiring here. Receiver-side CRSF is normal two-wire UART.

---

## RP2 / ELRS Configuration

The RP2 should remain in normal receiver mode, not AirPort mode.

Recommended RP2 serial protocol:

```text
Serial protocol: CRSF
Baud: 420000
Format: 8N1
Inverted: no
Half-duplex: no
```

The Pocket model should use:

```text
Internal RF: CRSF
External RF: OFF
```

The RP2 and Pocket internal ELRS module must be bound before telemetry can reach EdgeTX.

For the first test:

```text
Binding phrase: same on Pocket internal ELRS and RP2
Model Match: off
Telemetry ratio: 1:2 or 1:4
Packet rate: 50 Hz or 100 Hz
TX power: low on bench, fixed power preferred for repeatability
Dynamic power: off for controlled range tests
```

---

## CRSF GPS Telemetry Frame

Use the standard CRSF GPS telemetry frame type.

```text
Frame type: 0x02 GPS
Address/sync byte: 0xC8, flight-controller/host side
Length byte: 0x11
CRC polynomial: 0xD5
CRC is calculated over frame type + payload only
```

Frame layout:

```text
[0]  address/sync     0xC8
[1]  length           0x11  type + 15-byte payload + crc = 17
[2]  type             0x02  GPS

Payload:
[3..6]   latitude     int32, degrees * 10,000,000, big endian
[7..10]  longitude    int32, degrees * 10,000,000, big endian
[11..12] ground speed uint16, km/h * 100, big endian
[13..14] heading      uint16, degrees * 100, big endian
[15..16] altitude     uint16, meters + 1000, big endian
[17]     satellites   uint8

[18] crc8 over bytes [2..17]
```

For a range test, latitude and longitude are the important fields. Speed, heading, and altitude can initially be fixed dummy values.

Use a realistic satellite count, for example 8 to 12, otherwise EdgeTX may treat the GPS data as invalid or less useful.

---

## Firmware Behavior

Minimum behavior for first proof:

1. Start USB debug serial at 115200 baud.
2. Start CRSF UART to RP2 at 420000 baud, 8N1.
3. Once per second, send one CRSF GPS telemetry frame.
4. Use fixed dummy coordinates initially.
5. Print each sent coordinate on USB debug serial.
6. Ignore incoming CRSF frames from RP2 at first.

Later behavior:

1. Read GPS/sensor data on Teensy.
2. Validate fix and coordinate freshness.
3. Send CRSF GPS telemetry at 1 Hz.
4. Optionally include speed, heading, altitude, and satellite count.
5. Keep telemetry rate modest; do not stream high-rate data over ELRS telemetry.

Recommended first telemetry rate:

```text
CRSF GPS frame rate: 1 Hz
EdgeTX SD log interval: 1 s
```

---

## Minimal Teensy 4.1 Arduino Firmware

This sketch sends a fixed GPS coordinate once per second.

Replace the dummy coordinate with the real sensor/GPS values after the end-to-end log test works.

```cpp
// Teensy 4.1 -> RadioMaster RP2 CRSF GPS telemetry test
//
// Teensy 4.1 Serial1 pins:
//   pin 1 = TX1
//   pin 0 = RX1
//
// Wiring:
//   RP2 RX <- Teensy 4.1 pin 1 / TX1
//   RP2 TX -> Teensy 4.1 pin 0 / RX1
//   RP2 GND <-> Teensy 4.1 GND
//   RP2 5V  <- stable 5V supply
//
// Receiver protocol:
//   CRSF, 420000 baud, 8N1, uninverted, full duplex

#include <Arduino.h>
#include <math.h>

static constexpr uint32_t CRSF_BAUD = 420000;
static constexpr uint8_t CRSF_ADDR_FLIGHT_CONTROLLER = 0xC8;
static constexpr uint8_t CRSF_FRAMETYPE_GPS = 0x02;

uint32_t lastGpsMs = 0;

// CRSF CRC8 polynomial 0xD5.
// CRC is calculated over frame type + payload, not over address or length.
uint8_t crsf_crc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0xD5;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

void put_u8(uint8_t *buf, uint8_t &idx, uint8_t v) {
  buf[idx++] = v;
}

void put_u16_be(uint8_t *buf, uint8_t &idx, uint16_t v) {
  buf[idx++] = (uint8_t)(v >> 8);
  buf[idx++] = (uint8_t)(v & 0xFF);
}

void put_i32_be(uint8_t *buf, uint8_t &idx, int32_t v) {
  uint32_t uv = (uint32_t)v;
  buf[idx++] = (uint8_t)(uv >> 24);
  buf[idx++] = (uint8_t)(uv >> 16);
  buf[idx++] = (uint8_t)(uv >> 8);
  buf[idx++] = (uint8_t)(uv & 0xFF);
}

void send_crsf_gps(
  double lat_deg,
  double lon_deg,
  uint16_t speed_kmh_x100,
  uint16_t heading_deg_x100,
  int16_t altitude_m,
  uint8_t satellites
) {
  uint8_t frame[19];
  uint8_t idx = 0;

  int32_t lat = (int32_t)llround(lat_deg * 10000000.0);
  int32_t lon = (int32_t)llround(lon_deg * 10000000.0);

  // CRSF altitude field is meters + 1000.
  int32_t alt_encoded = (int32_t)altitude_m + 1000;
  if (alt_encoded < 0) {
    alt_encoded = 0;
  }
  if (alt_encoded > 65535) {
    alt_encoded = 65535;
  }

  put_u8(frame, idx, CRSF_ADDR_FLIGHT_CONTROLLER);
  put_u8(frame, idx, 17);                 // type + 15-byte payload + crc
  put_u8(frame, idx, CRSF_FRAMETYPE_GPS);

  put_i32_be(frame, idx, lat);
  put_i32_be(frame, idx, lon);
  put_u16_be(frame, idx, speed_kmh_x100);
  put_u16_be(frame, idx, heading_deg_x100);
  put_u16_be(frame, idx, (uint16_t)alt_encoded);
  put_u8(frame, idx, satellites);

  // CRC over type + payload, i.e. frame[2] through frame[17].
  frame[idx++] = crsf_crc8(&frame[2], 16);

  Serial1.write(frame, idx);
}

void setup() {
  Serial.begin(115200);      // USB debug
  Serial1.begin(CRSF_BAUD);  // CRSF to RP2

  delay(1000);
  Serial.println("CRSF GPS telemetry test started");
}

void loop() {
  // Optional: read incoming CRSF RC/channel frames from RP2.
  // For the first proof we only drain the bytes and ignore them.
  while (Serial1.available()) {
    (void)Serial1.read();
  }

  if (millis() - lastGpsMs >= 1000) {
    lastGpsMs = millis();

    // Example position near Kassel. Replace with real GPS/sensor values later.
    const double lat = 51.296994;
    const double lon = 9.451982;

    const uint16_t speed = 0;       // km/h * 100
    const uint16_t heading = 0;     // degrees * 100
    const int16_t altitude = 200;   // meters
    const uint8_t sats = 10;        // realistic valid-looking satellite count

    send_crsf_gps(lat, lon, speed, heading, altitude, sats);

    Serial.print("sent GPS ");
    Serial.print(lat, 7);
    Serial.print(", ");
    Serial.println(lon, 7);
  }
}
```

---

## EdgeTX Telemetry Discovery

After Teensy is running, RP2 is powered, and the receiver is bound:

```text
1. On Pocket, open the range-test model.
2. Confirm Internal RF = CRSF.
3. Confirm RP2 LED indicates bound state.
4. Go to Telemetry.
5. Delete old sensors if needed.
6. Select Discover new sensors.
7. Wait 10 to 20 seconds.
```

Expected sensors may include:

```text
GPS / coordinates
GSpd
Hdg
Alt
Sats
RSSI
LQ
SNR
RFMD
TPWR
```

Exact sensor names depend on EdgeTX and ExpressLRS versions.

---

## SD Logging Setup

Recommended for range test:

```text
Special Functions:
  SF1: chosen switch -> SD Logs -> 1.0s -> enabled checkbox checked
```

Avoid using permanent `ON` logging unless wanted. A switch-controlled logger is safer.

Example procedure:

```text
1. Power Pocket.
2. Select range-test model.
3. Power RP2 + Teensy.
4. Wait until bound.
5. Verify telemetry values change / GPS exists.
6. Flip logging switch to start logging.
7. Perform range test.
8. Flip logging switch off.
9. Wait a few seconds.
10. Use USB Storage mode or power off before removing SD card.
```

Do not remove the SD card while logging.

---

## CSV Range Analysis Plan

The EdgeTX CSV log should contain timestamps, ELRS link telemetry, and GPS telemetry.

Use one fixed reference coordinate for the base/Pocket position:

```text
base_lat = latitude of Pocket / base position
base_lon = longitude of Pocket / base position
```

Then compute distance from base to each logged GPS point.

Python helper:

```python
from math import radians, sin, cos, sqrt, atan2


def distance_m(lat1, lon1, lat2, lon2):
    r_earth_m = 6371000.0
    p1 = radians(lat1)
    p2 = radians(lat2)
    dp = radians(lat2 - lat1)
    dl = radians(lon2 - lon1)

    a = sin(dp / 2.0) ** 2 + cos(p1) * cos(p2) * sin(dl / 2.0) ** 2
    return 2.0 * r_earth_m * atan2(sqrt(a), sqrt(1.0 - a))
```

Recommended plots afterwards:

```text
Distance vs RSSI dBm
Distance vs LQ
Distance vs SNR
Distance vs packet mode / RFMD
Distance vs TX power if dynamic power is enabled
Map of logged GPS path
```

For repeatable RF comparisons, use fixed TX power and fixed packet rate first.

---

## First Bring-Up Checklist

### Bench test

```text
[ ] RP2 is powered from stable 5 V.
[ ] Teensy and RP2 share GND.
[ ] Teensy TX1 is wired to RP2 RX.
[ ] Teensy RX1 is wired to RP2 TX.
[ ] Teensy USB debug prints `sent GPS ...` once per second.
[ ] Pocket and RP2 are bound.
[ ] EdgeTX telemetry discovery finds GPS-related sensors.
[ ] EdgeTX SD log contains GPS and ELRS link telemetry.
```

### Outdoor range test

```text
[ ] Base/Pocket position recorded.
[ ] RP2/Teensy/tag antenna orientation documented.
[ ] Pocket antenna orientation documented.
[ ] TX power fixed.
[ ] Packet rate fixed.
[ ] Telemetry ratio noted.
[ ] SD logging switch tested before leaving start point.
[ ] Log file copied after test over USB Storage or from SD card.
```

---

## Troubleshooting

### No GPS sensors discovered

Check:

```text
- RP2 is bound to Pocket.
- Teensy is sending frames once per second.
- Teensy TX1/RX1 are crossed correctly to RP2 RX/TX.
- Common GND exists.
- UART baud is 420000.
- RP2 serial protocol is CRSF, not AirPort.
- Satellite count is nonzero and realistic.
- CRC calculation is correct.
```

### Receiver binds but telemetry is unreliable

Check:

```text
- ELRS telemetry ratio is not too low.
- Do not send GPS frames faster than needed.
- Start with 1 Hz GPS telemetry.
- Keep RP2 antenna away from battery, wires, and ground plane effects.
- For serious range testing, consider RP1 or another receiver with external antenna instead of RP2 ceramic antenna.
```

### EdgeTX logs link values but no GPS

Likely causes:

```text
- CRSF GPS frame format error.
- Wrong byte order.
- Wrong CRC range.
- Teensy UART not reaching RP2 RX.
- RP2 Web UI serial protocol not set to CRSF.
```

---

## Recommended Project Structure

For a VS Code / PlatformIO or Arduino-style project targeting Teensy 4.1:

```text
teensy-rp2-crsf-gps/
  platformio.ini
  README.md
  src/
    main.cpp
    crsf.cpp
    crsf.h
    gps_source.cpp
    gps_source.h
  tools/
    analyze_edgetx_log.py
  docs/
    wiring.md
    range_test_procedure.md
```

Recommended PlatformIO target:

```ini
[env:teensy41]
platform = teensy
board = teensy41
framework = arduino
monitor_speed = 115200
```

Suggested module split:

```text
crsf.h/.cpp
  - CRC8
  - endian helpers
  - send_crsf_gps()

main.cpp
  - setup UARTs
  - periodic scheduler
  - USB debug prints

gps_source.h/.cpp
  - fixed dummy coordinate first
  - later parse real GPS or sensor source

tools/analyze_edgetx_log.py
  - read EdgeTX CSV
  - parse GPS columns
  - calculate distance to base
  - plot distance vs RSSI/LQ/SNR
```

---

## Implementation Priority for Codex

1. Create a compiling Teensy 4.1 project, preferably PlatformIO `board = teensy41` or Arduino IDE with Teensyduino set to Teensy 4.1.
2. Add CRSF CRC8 and GPS frame builder.
3. Send fixed GPS coordinates at 1 Hz over `Serial1` at 420000 baud.
4. Add USB debug output.
5. Verify EdgeTX sensor discovery and CSV log.
6. Add a small Python script to parse EdgeTX CSV and compute distance from a configured base coordinate.
7. Replace dummy coordinate with real GPS/sensor input.
8. Add range-test metadata to logs or a sidecar JSON/YAML file.

