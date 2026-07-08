#pragma once

#include <Arduino.h>

struct GpsFixData {
  bool location_valid;
  bool altitude_valid;
  bool speed_valid;
  bool course_valid;
  bool hdop_valid;
  bool satellites_in_view_valid;
  bool date_valid;
  bool time_valid;

  double latitude;
  double longitude;
  double altitude_m;
  double speed_kmph;
  double course_deg;
  double hdop;
  uint32_t satellites;
  uint32_t satellites_in_view;

  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t centisecond;

  uint32_t location_age_ms;
  uint32_t last_update_ms;

  uint32_t rx_bytes;
  uint32_t passed_checksum;
  uint32_t failed_checksum;
  uint32_t sentences_with_fix;
};

class GpsM10Q {
 public:
  GpsM10Q();

  void setPort(HardwareSerial &serial_port);
  void begin(uint32_t baud, uint8_t update_rate_hz);
  void update();
  void setUpdateRate(uint8_t update_rate_hz);

  const GpsFixData &latest() const;
  uint32_t rxBytes() const;
  uint32_t passedChecksum() const;
  uint32_t failedChecksum() const;

 private:
  void sendUbxCfgRate(uint16_t measurement_rate_ms) const;
  void refreshCache();
  void parseNmeaChar(char c);
  void parseNmeaLine();

  HardwareSerial *serial_;
  GpsFixData cache_;
  uint8_t configured_rate_hz_;

  static constexpr size_t NMEA_LINE_BUFFER_SIZE = 128;
  char nmea_line_buffer_[NMEA_LINE_BUFFER_SIZE];
  size_t nmea_line_length_;
  uint32_t satellites_in_view_raw_;
  bool satellites_in_view_raw_valid_;
  uint32_t rx_bytes_;
};
