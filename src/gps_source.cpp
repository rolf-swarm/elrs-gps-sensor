#include "gps_source.h"

#include "board_config.h"

#include <math.h>

namespace {

static constexpr uint32_t GPS_BAUDS[] = {9600, 38400, 57600, 115200};
static constexpr uint8_t GPS_UPDATE_RATE_HZ = 1;
static constexpr uint32_t MAX_LOCATION_AGE_MS = 3000;
static constexpr uint32_t BAUD_PROBE_MS = 3500;

uint16_t clampUint16FromDouble(double value) {
  if (!isfinite(value) || value <= 0.0) {
    return 0;
  }
  if (value >= 65535.0) {
    return 65535;
  }
  return static_cast<uint16_t>(lround(value));
}

int16_t clampInt16FromDouble(double value) {
  if (!isfinite(value)) {
    return 0;
  }
  if (value <= -32768.0) {
    return -32768;
  }
  if (value >= 32767.0) {
    return 32767;
  }
  return static_cast<int16_t>(lround(value));
}

uint8_t clampSatellites(uint32_t satellites) {
  if (satellites > 255) {
    return 255;
  }
  return static_cast<uint8_t>(satellites);
}

uint16_t encodeHeadingDegX100(double course_deg) {
  const uint16_t heading = clampUint16FromDouble(course_deg * 100.0);
  if (heading >= 36000) {
    return static_cast<uint16_t>(heading % 36000);
  }
  return heading;
}

uint32_t bestSatelliteCount(const GpsFixData &fix) {
  if (fix.satellites > 0) {
    return fix.satellites;
  }
  if (fix.satellites_in_view_valid) {
    return fix.satellites_in_view;
  }
  return 0;
}

}  // namespace

void GpsSource::begin() {
  gps_.setPort(board::gpsSerial());
  startBaud(0);
}

void GpsSource::update() {
  gps_.update();
  maybeAdvanceBaud();
}

bool GpsSource::read(GpsFix &fix) {
  const GpsFixData &latest_fix = gps_.latest();
  const bool fresh_location =
      latest_fix.location_valid &&
      latest_fix.location_age_ms <= MAX_LOCATION_AGE_MS;

  fix.valid = fresh_location;
  fix.latitude_deg = latest_fix.latitude;
  fix.longitude_deg = latest_fix.longitude;
  fix.ground_speed_kmh_x100 =
      latest_fix.speed_valid
          ? clampUint16FromDouble(latest_fix.speed_kmph * 100.0)
          : 0;
  fix.heading_deg_x100 =
      latest_fix.course_valid ? encodeHeadingDegX100(latest_fix.course_deg) : 0;
  fix.altitude_m =
      latest_fix.altitude_valid ? clampInt16FromDouble(latest_fix.altitude_m)
                                : 0;
  fix.satellites = clampSatellites(bestSatelliteCount(latest_fix));

  return fix.valid;
}

const GpsFixData &GpsSource::latest() const {
  return gps_.latest();
}

uint32_t GpsSource::activeBaud() const {
  return GPS_BAUDS[active_baud_index_];
}

void GpsSource::startBaud(uint8_t baud_index) {
  active_baud_index_ = baud_index;
  baud_started_ms_ = millis();
  passed_checksum_at_baud_start_ = gps_.passedChecksum();

  gps_.begin(activeBaud(), GPS_UPDATE_RATE_HZ);

  Serial.print(F("GPS serial baud "));
  Serial.println(activeBaud());
}

void GpsSource::maybeAdvanceBaud() {
  const GpsFixData &fix = gps_.latest();

  if (fix.location_valid || gps_.passedChecksum() > passed_checksum_at_baud_start_) {
    return;
  }

  if (millis() - baud_started_ms_ < BAUD_PROBE_MS) {
    return;
  }

  const uint8_t next_index =
      static_cast<uint8_t>((active_baud_index_ + 1) %
                           (sizeof(GPS_BAUDS) / sizeof(GPS_BAUDS[0])));
  startBaud(next_index);
}
