#include "gps_m10q.h"

#include "board_config.h"

#include <TinyGPSPlus.h>
#include <stdlib.h>
#include <string.h>

namespace {

TinyGPSPlus gps;

uint8_t clampRate(uint8_t hz) {
  if (hz < 1) {
    return 1;
  }
  if (hz > 10) {
    return 10;
  }
  return hz;
}

void appendChecksumAndWrite(HardwareSerial *serial_port, uint8_t *packet,
                            uint8_t payload_end_index) {
  uint8_t ck_a = 0;
  uint8_t ck_b = 0;

  for (uint8_t i = 2; i < payload_end_index; ++i) {
    ck_a = static_cast<uint8_t>(ck_a + packet[i]);
    ck_b = static_cast<uint8_t>(ck_b + ck_a);
  }

  packet[payload_end_index] = ck_a;
  packet[payload_end_index + 1] = ck_b;
  serial_port->write(packet, payload_end_index + 2);
}

}  // namespace

GpsM10Q::GpsM10Q()
    : serial_(nullptr),
      configured_rate_hz_(1),
      nmea_line_length_(0),
      satellites_in_view_raw_(0),
      satellites_in_view_raw_valid_(false),
      rx_bytes_(0) {
  memset(&cache_, 0, sizeof(cache_));
  memset(nmea_line_buffer_, 0, sizeof(nmea_line_buffer_));
}

void GpsM10Q::setPort(HardwareSerial &serial_port) {
  serial_ = &serial_port;
}

void GpsM10Q::begin(uint32_t baud, uint8_t update_rate_hz) {
  if (serial_ == nullptr) {
    return;
  }

  nmea_line_length_ = 0;
  satellites_in_view_raw_ = 0;
  satellites_in_view_raw_valid_ = false;
  configured_rate_hz_ = clampRate(update_rate_hz);

  board::beginGpsSerial(baud);
  setUpdateRate(configured_rate_hz_);
}

void GpsM10Q::update() {
  if (serial_ == nullptr) {
    return;
  }

  while (serial_->available() > 0) {
    const int incoming = serial_->read();
    if (incoming >= 0) {
      const char c = static_cast<char>(incoming);
      rx_bytes_++;
      gps.encode(c);
      parseNmeaChar(c);
    }
  }

  refreshCache();
}

void GpsM10Q::setUpdateRate(uint8_t update_rate_hz) {
  configured_rate_hz_ = clampRate(update_rate_hz);
  const uint16_t measurement_rate_ms =
      static_cast<uint16_t>(1000U / configured_rate_hz_);
  sendUbxCfgRate(measurement_rate_ms);
}

const GpsFixData &GpsM10Q::latest() const {
  return cache_;
}

uint32_t GpsM10Q::rxBytes() const {
  return rx_bytes_;
}

uint32_t GpsM10Q::passedChecksum() const {
  return gps.passedChecksum();
}

uint32_t GpsM10Q::failedChecksum() const {
  return gps.failedChecksum();
}

void GpsM10Q::sendUbxCfgRate(uint16_t measurement_rate_ms) const {
  if (serial_ == nullptr) {
    return;
  }

  uint8_t packet[14];
  packet[0] = 0xB5;
  packet[1] = 0x62;
  packet[2] = 0x06;
  packet[3] = 0x08;
  packet[4] = 0x06;
  packet[5] = 0x00;
  packet[6] = static_cast<uint8_t>(measurement_rate_ms & 0xFF);
  packet[7] = static_cast<uint8_t>((measurement_rate_ms >> 8) & 0xFF);
  packet[8] = 0x01;   // navRate = 1 cycle
  packet[9] = 0x00;
  packet[10] = 0x01;  // timeRef = GPS
  packet[11] = 0x00;

  appendChecksumAndWrite(serial_, packet, 12);
}

void GpsM10Q::refreshCache() {
  cache_.location_valid = gps.location.isValid();
  cache_.altitude_valid = gps.altitude.isValid();
  cache_.speed_valid = gps.speed.isValid();
  cache_.course_valid = gps.course.isValid();
  cache_.hdop_valid = gps.hdop.isValid();
  cache_.satellites_in_view_valid = satellites_in_view_raw_valid_;
  cache_.date_valid = gps.date.isValid();
  cache_.time_valid = gps.time.isValid();

  cache_.latitude = gps.location.lat();
  cache_.longitude = gps.location.lng();
  cache_.altitude_m = gps.altitude.meters();
  cache_.speed_kmph = gps.speed.kmph();
  cache_.course_deg = gps.course.deg();
  cache_.hdop = gps.hdop.hdop();
  cache_.satellites = gps.satellites.value();
  cache_.satellites_in_view = satellites_in_view_raw_;

  cache_.year = gps.date.year();
  cache_.month = gps.date.month();
  cache_.day = gps.date.day();
  cache_.hour = gps.time.hour();
  cache_.minute = gps.time.minute();
  cache_.second = gps.time.second();
  cache_.centisecond = gps.time.centisecond();

  cache_.location_age_ms = gps.location.age();
  cache_.last_update_ms = millis();
  cache_.rx_bytes = rx_bytes_;
  cache_.passed_checksum = gps.passedChecksum();
  cache_.failed_checksum = gps.failedChecksum();
  cache_.sentences_with_fix = gps.sentencesWithFix();
}

void GpsM10Q::parseNmeaChar(char c) {
  if (c == '\r') {
    return;
  }

  if (c == '\n') {
    if (nmea_line_length_ > 0) {
      nmea_line_buffer_[nmea_line_length_] = '\0';
      parseNmeaLine();
      nmea_line_length_ = 0;
    }
    return;
  }

  if (c == '$') {
    nmea_line_length_ = 0;
  }

  if (nmea_line_length_ < (NMEA_LINE_BUFFER_SIZE - 1)) {
    nmea_line_buffer_[nmea_line_length_++] = c;
  } else {
    nmea_line_length_ = 0;
  }
}

void GpsM10Q::parseNmeaLine() {
  if (nmea_line_buffer_[0] != '$') {
    return;
  }

  char sentence[NMEA_LINE_BUFFER_SIZE];
  strncpy(sentence, nmea_line_buffer_, sizeof(sentence) - 1);
  sentence[sizeof(sentence) - 1] = '\0';

  char *ctx = nullptr;
  char *token = strtok_r(sentence, ",", &ctx);
  if (token == nullptr) {
    return;
  }

  const char *type = (token[0] == '$') ? (token + 1) : token;
  const size_t type_len = strlen(type);
  if (type_len < 3 || strcmp(type + (type_len - 3), "GSV") != 0) {
    return;
  }

  (void)strtok_r(nullptr, ",", &ctx);  // total_msgs
  (void)strtok_r(nullptr, ",", &ctx);  // msg_num
  char *satellites_in_view = strtok_r(nullptr, ",", &ctx);
  if (satellites_in_view == nullptr) {
    return;
  }

  char *end = nullptr;
  const uint32_t parsed = strtoul(satellites_in_view, &end, 10);
  if (end == satellites_in_view) {
    return;
  }

  const bool is_combined_gsv =
      (type_len >= 5 && strncmp(type, "GNGSV", 5) == 0);
  if (is_combined_gsv || !satellites_in_view_raw_valid_) {
    satellites_in_view_raw_ = parsed;
    satellites_in_view_raw_valid_ = true;
  }
}
