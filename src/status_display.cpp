#include "status_display.h"

#include "board_config.h"

#include <Wire.h>
#include <stdio.h>

namespace {

static constexpr uint8_t OLED_ADDRESS_PRIMARY = 0x3C;
static constexpr uint8_t OLED_ADDRESS_SECONDARY = 0x3D;
static constexpr uint32_t DISPLAY_UPDATE_PERIOD_MS = 250;
static constexpr uint32_t CRSF_ALIVE_MS = 700;
static constexpr uint32_t LINK_STATS_FRESH_MS = 2000;
static constexpr uint32_t GPS_FIX_FRESH_MS = 3000;

uint32_t ageOrMax(uint32_t now_ms, uint32_t timestamp_ms) {
  if (timestamp_ms == 0) {
    return UINT32_MAX;
  }
  return now_ms - timestamp_ms;
}

const char *crsfStateLabel(const crsf::ReceiverStatus &status,
                           uint32_t now_ms) {
  if (status.last_frame_ms == 0) {
    return "CRSF WAIT";
  }

  if (ageOrMax(now_ms, status.last_frame_ms) <= CRSF_ALIVE_MS) {
    return "CRSF OK";
  }

  return "CRSF LOST";
}

bool linkStatsFresh(const crsf::ReceiverStatus &status, uint32_t now_ms) {
  return status.link.valid &&
         ageOrMax(now_ms, status.link.last_update_ms) <= LINK_STATS_FRESH_MS;
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

bool gpsLocationFresh(const GpsFixData &fix) {
  return fix.location_valid && fix.location_age_ms <= GPS_FIX_FRESH_MS;
}

const char *gpsStateLabel(const GpsFixData &fix) {
  if (!fix.location_valid) {
    return "WAIT";
  }

  if (!gpsLocationFresh(fix)) {
    return "OLD";
  }

  return "FIX";
}

uint32_t cappedValue(uint32_t value, uint32_t maximum) {
  if (value > maximum) {
    return maximum;
  }

  return value;
}

void drawTextLine(U8G2 &display, uint8_t line, const char *text) {
  display.drawStr(0, static_cast<uint8_t>(line * 10), text);
}

}  // namespace

StatusDisplay::StatusDisplay()
    : display_(U8G2_R0, U8X8_PIN_NONE),
      available_(false),
      address_(0),
      last_update_ms_(0) {}

void StatusDisplay::begin() {
  board::beginI2c();

  if (probeAddress(OLED_ADDRESS_PRIMARY)) {
    address_ = OLED_ADDRESS_PRIMARY;
  } else if (probeAddress(OLED_ADDRESS_SECONDARY)) {
    address_ = OLED_ADDRESS_SECONDARY;
  } else {
    Serial.println(F("OLED VMA438 not found; display disabled"));
    return;
  }

  display_.setI2CAddress(static_cast<uint8_t>(address_ << 1));
  display_.begin();
  display_.setFont(u8g2_font_6x10_tf);
  display_.setFontPosTop();
  display_.clearBuffer();
  drawTextLine(display_, 0, "ELRS GPS Sensor");
  drawTextLine(display_, 2, "OLED online");
  display_.sendBuffer();

  available_ = true;
  Serial.print(F("OLED VMA438 found at 0x"));
  Serial.println(address_, HEX);
}

void StatusDisplay::update(const crsf::ReceiverStatus &receiver_status,
                           const GpsFixData &gps_fix, uint32_t gps_baud,
                           uint32_t gps_telemetry_frames_sent) {
  if (!available_) {
    return;
  }

  const uint32_t now_ms = millis();
  if (now_ms - last_update_ms_ < DISPLAY_UPDATE_PERIOD_MS) {
    return;
  }

  last_update_ms_ = now_ms;
  draw(receiver_status, gps_fix, gps_baud, gps_telemetry_frames_sent);
}

bool StatusDisplay::available() const {
  return available_;
}

bool StatusDisplay::probeAddress(uint8_t address) const {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void StatusDisplay::draw(const crsf::ReceiverStatus &receiver_status,
                         const GpsFixData &gps_fix, uint32_t gps_baud,
                         uint32_t gps_telemetry_frames_sent) {
  const uint32_t now_ms = millis();
  const bool have_link_stats = linkStatsFresh(receiver_status, now_ms);
  const crsf::LinkStatistics &link = receiver_status.link;
  char line[24];

  display_.clearBuffer();

  if (have_link_stats && link.uplink_fps_valid) {
    snprintf(line, sizeof(line), "%s FPS %u",
             crsfStateLabel(receiver_status, now_ms), link.uplink_fps);
  } else if (have_link_stats) {
    snprintf(line, sizeof(line), "%s RF %u",
             crsfStateLabel(receiver_status, now_ms), link.rf_profile);
  } else {
    snprintf(line, sizeof(line), "%s", crsfStateLabel(receiver_status, now_ms));
  }
  drawTextLine(display_, 0, line);

  if (have_link_stats) {
    const int16_t rssi = link.active_antenna == 1
                             ? link.uplink_rssi_ant2_dbm
                             : link.uplink_rssi_ant1_dbm;
    snprintf(line, sizeof(line), "LQ %3u%% RSSI %d",
             link.uplink_link_quality, rssi);
  } else {
    snprintf(line, sizeof(line), "LQ  --  RSSI --");
  }
  drawTextLine(display_, 1, line);

  if (have_link_stats) {
    if (link.uplink_power_mw > 0) {
      snprintf(line, sizeof(line), "SNR %d P %umW",
               link.uplink_snr_db, link.uplink_power_mw);
    } else {
      snprintf(line, sizeof(line), "SNR %d P --",
               link.uplink_snr_db);
    }
  } else {
    snprintf(line, sizeof(line), "SNR -- P --");
  }
  drawTextLine(display_, 2, line);

  snprintf(line, sizeof(line), "GPS %s S%lu T%lu",
           gpsStateLabel(gps_fix),
           static_cast<unsigned long>(bestSatelliteCount(gps_fix)),
           static_cast<unsigned long>(
               cappedValue(gps_telemetry_frames_sent, 9999)));
  drawTextLine(display_, 3, line);

  if (gps_fix.hdop_valid) {
    const uint16_t hdop_x10 =
        cappedValue(static_cast<uint32_t>(gps_fix.hdop * 10.0 + 0.5), 999);
    snprintf(line, sizeof(line), "Age%4lums HDOP%u.%u",
             static_cast<unsigned long>(
                 cappedValue(gps_fix.location_age_ms, 9999)),
             hdop_x10 / 10, hdop_x10 % 10);
  } else {
    snprintf(line, sizeof(line), "Age%4lums HDOP --",
             static_cast<unsigned long>(
                 cappedValue(gps_fix.location_age_ms, 9999)));
  }
  drawTextLine(display_, 4, line);

  const unsigned int gps_baud_khz =
      static_cast<unsigned int>(cappedValue(gps_baud / 1000U, 999));
  const unsigned int gps_checksum_ok =
      static_cast<unsigned int>(cappedValue(gps_fix.passed_checksum, 999999));
  const unsigned int gps_checksum_bad =
      static_cast<unsigned int>(cappedValue(gps_fix.failed_checksum, 9999));
  snprintf(line, sizeof(line), "B%uk O%u E%u", gps_baud_khz,
           gps_checksum_ok, gps_checksum_bad);
  drawTextLine(display_, 5, line);

  display_.sendBuffer();
}
