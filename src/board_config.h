#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace board {

#if defined(ARDUINO_ARCH_ESP32)

#ifndef CRSF_RX_PIN
#define CRSF_RX_PIN 32
#endif

#ifndef CRSF_TX_PIN
#define CRSF_TX_PIN 33
#endif

#ifndef GPS_RX_PIN
#define GPS_RX_PIN 25
#endif

#ifndef GPS_TX_PIN
#define GPS_TX_PIN 26
#endif

#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN SDA
#endif

#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN SCL
#endif

static constexpr int CRSF_RX = CRSF_RX_PIN;
static constexpr int CRSF_TX = CRSF_TX_PIN;
static constexpr int GPS_RX = GPS_RX_PIN;
static constexpr int GPS_TX = GPS_TX_PIN;
static constexpr int OLED_SDA = OLED_SDA_PIN;
static constexpr int OLED_SCL = OLED_SCL_PIN;

#else

static constexpr int CRSF_RX = 0;
static constexpr int CRSF_TX = 1;
static constexpr int GPS_RX = 7;
static constexpr int GPS_TX = 8;
static constexpr int OLED_SDA = 18;
static constexpr int OLED_SCL = 19;

#endif

inline HardwareSerial &crsfSerial() {
  return Serial1;
}

inline HardwareSerial &gpsSerial() {
  return Serial2;
}

inline void beginCrsfSerial(uint32_t baud) {
#if defined(ARDUINO_ARCH_ESP32)
  crsfSerial().begin(baud, SERIAL_8N1, CRSF_RX, CRSF_TX);
#else
  crsfSerial().begin(baud);
#endif
}

inline void beginGpsSerial(uint32_t baud) {
#if defined(ARDUINO_ARCH_ESP32)
  gpsSerial().begin(baud, SERIAL_8N1, GPS_RX, GPS_TX);
#else
  gpsSerial().begin(baud);
#endif
}

inline void beginI2c() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(OLED_SDA, OLED_SCL);
#else
  Wire.begin();
#endif
}

inline void printPinSummary(Print &out) {
  out.print(F("CRSF/RP2 RX pin "));
  out.print(CRSF_RX);
  out.print(F(", TX pin "));
  out.println(CRSF_TX);

  out.print(F("GPS M10Q RX pin "));
  out.print(GPS_RX);
  out.print(F(", TX pin "));
  out.println(GPS_TX);

  out.print(F("OLED I2C SDA pin "));
  out.print(OLED_SDA);
  out.print(F(", SCL pin "));
  out.println(OLED_SCL);
}

}  // namespace board
