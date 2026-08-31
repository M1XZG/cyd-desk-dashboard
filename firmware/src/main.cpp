#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <lvgl.h>
#include <new>

#include "default_icons.h"
#include "default_startup.h"
#include "lgfx_cyd.h"
#include "live_data.h"

namespace {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 240;
constexpr size_t kSystemsCardCount = kMaximumSystemMonitors + 2;
static_assert(kSystemsCardCount == 6, "Systems grid is designed for six cards");
constexpr int kBacklightPin = 21;
constexpr int kBacklightChannel = 0;
constexpr int kSdCs = 5;

constexpr int kTouchCs = 33;
constexpr int kTouchIrq = 36;
constexpr int kTouchClock = 25;
constexpr int kTouchMosi = 32;
constexpr int kTouchMiso = 39;
constexpr const char* kSetupAccessPointSsid = "desktopdashboard-setup";
constexpr const char* kSetupAccessPointPassword = "deskdashboard";
constexpr const char* kDefaultPosixTimezone =
    "GMT0BST,M3.5.0/1,M10.5.0";
constexpr uint32_t kTileColors[] = {
    0x2563EB, 0x0891B2, 0x7C3AED, 0x0F766E, 0xB45309, 0x475569,
};

constexpr const char* kTileNames[] = {
    "Weather", "Flights", "Bambuddy", "Systems", "Calendar", "Settings",
};

constexpr const char* kTileIconPaths[] = {
    "S:/dashboard/icons/Weather.bin",
    "S:/dashboard/icons/Flights.bin",
    "S:/dashboard/icons/Bambuddy.bin",
    "S:/dashboard/icons/Systems.bin",
    "S:/dashboard/icons/Calendar.bin",
    "S:/dashboard/icons/Settings.bin",
};

constexpr const char* kTileIds[] = {
    "weather", "flights", "bambuddy", "systems", "calendar",
};

constexpr const char* kSettingsNames[] = {
    "Display", "Location", "Locale & Units", "Services", "Home Tiles", "System",
};

constexpr const char kConfigExample[] = R"json({
  "version": 1,
  "display": {
    "rotation": 1,
    "brightness_percent": 85,
    "panel_variant": "auto"
  },
  "locale": {
    "time_format": "24h",
    "date_format": "yyyy-dd-mm",
    "posix_timezone": "GMT0BST,M3.5.0/1,M10.5.0"
  },
  "location": {
    "method": "search",
    "search": "YOUR TOWN OR POSTCODE",
    "name": "",
    "country": "",
    "country_code": "",
    "latitude": null,
    "longitude": null,
    "elevation_metres": null,
    "timezone": "auto"
  },
  "weather": {
    "temperature_unit": "celsius",
    "wind_speed_unit": "kmh",
    "pressure_unit": "hpa",
    "precipitation_unit": "mm"
  },
  "flights": {
    "provider": "airplanes.live",
    "radius_km": 100,
    "maximum_aircraft": 12,
    "minimum_altitude_ft": 0
  },
  "services": {
    "bambuddy": {
      "enabled": true,
      "protocol": "http",
      "host": "192.168.1.50",
      "port": 9001,
      "api_base_path": "/api/v1",
      "printer_id": 1
    },
    "flights": {
      "enabled": true
    },
    "systems": {
      "enabled": true,
      "monitors": [
        {"enabled": false, "name": "", "type": "http", "host": "", "port": 80, "path": "/"},
        {"enabled": false, "name": "", "type": "http", "host": "", "port": 80, "path": "/"},
        {"enabled": false, "name": "", "type": "tcp", "host": "", "port": 443, "path": "/"},
        {"enabled": false, "name": "", "type": "tcp", "host": "", "port": 22, "path": "/"}
      ]
    },
    "calendar": {
      "enabled": true
    }
  },
  "refresh_seconds": {
    "weather": 600,
    "astronomy": 3600,
    "flights": 60,
    "bambuddy_visible": 30,
    "bambuddy_background": 120,
    "systems": 60
  },
  "tile_visibility": {
    "weather": true,
    "flights": true,
    "bambuddy": true,
    "systems": true,
    "calendar": true
  }
}
)json";

constexpr const char kConnectionsExample[] = R"json({
  "version": 1,
  "wifi": {
    "ssid": "YOUR_WIFI_NETWORK",
    "password": "YOUR_WIFI_PASSWORD"
  },
  "services": {
    "bambuddy_readonly_api_key": "bb_READ_STATUS_KEY_ONLY",
    "flights_api_token": ""
  },
  "device": {
    "settings_portal_password": ""
  }
}
)json";

LGFX_CYD display;
SPIClass sdSpi(VSPI);
WebServer settingsServer(80);
DNSServer setupDnsServer;
File portalUploadFile;
String portalUploadTarget;
String portalUploadError;
bool portalUploadAccepted = false;
bool portalUploadHasStorageLock = false;
SemaphoreHandle_t sdMutex = nullptr;

lv_disp_draw_buf_t drawBuffer;
lv_color_t bufferOne[kScreenWidth * 10];
lv_color_t bufferTwo[kScreenWidth * 10];
lv_disp_drv_t displayDriver;
lv_indev_drv_t inputDriver;

lv_obj_t* wifiStatusLabel = nullptr;
lv_obj_t* wifiSignalBars[4] = {};
lv_obj_t* sdStatusLabel = nullptr;
lv_obj_t* clockStatusLabel = nullptr;
lv_obj_t* brightnessLabel = nullptr;
lv_obj_t* weatherIconImage = nullptr;
lv_obj_t* weatherTemperatureLabel = nullptr;
lv_obj_t* weatherConditionLabel = nullptr;
lv_obj_t* weatherSummaryLabel = nullptr;
lv_obj_t* weatherDetailsLabel = nullptr;
lv_obj_t* weatherAstronomyLabel = nullptr;
lv_obj_t* weatherCreditLabel = nullptr;
const char* renderedWeatherIconPath = nullptr;
lv_obj_t* flightsRadar = nullptr;
lv_obj_t* flightsMessageLabel = nullptr;
lv_obj_t* bambuddyNameLabel = nullptr;
lv_obj_t* bambuddyStatusLabel = nullptr;
lv_obj_t* bambuddySignalLabel = nullptr;
lv_obj_t* bambuddyJobLabel = nullptr;
lv_obj_t* bambuddyProgressBar = nullptr;
lv_obj_t* bambuddyProgressLabel = nullptr;
lv_obj_t* bambuddyRemainingLabel = nullptr;
lv_obj_t* bambuddyNozzleLabel = nullptr;
lv_obj_t* bambuddyBedLabel = nullptr;
lv_obj_t* bambuddyLayerLabel = nullptr;
lv_obj_t* systemsNameLabels[kSystemsCardCount] = {};
lv_obj_t* systemsStateLabels[kSystemsCardCount] = {};
lv_obj_t* systemsDetailLabels[kSystemsCardCount] = {};
lv_obj_t* systemsCards[kSystemsCardCount] = {};
int8_t systemsMonitorCardIndices[kMaximumSystemMonitors] = {-1, -1, -1, -1};
bool systemMonitorEnabled[kMaximumSystemMonitors] = {};

bool sdReady = false;
bool configReady = false;
bool connectionsReady = false;
bool wifiStarted = false;
bool portalStarted = false;
bool portalRoutesConfigured = false;
bool mdnsStarted = false;
bool setupAccessPointStarted = false;
bool setupMode = false;
bool setupProvisioningAllowed = false;
bool clockConfigured = false;
bool clockSyncLogged = false;
bool portalUsesBootstrapPassword = false;
bool restartPending = false;
bool tileEnabled[] = {true, true, true, true, true};
enum class PendingNavigation : uint8_t {
  none,
  home,
  settings,
  flights,
  aircraft,
  systems,
  calendar,
};
PendingNavigation pendingNavigation = PendingNavigation::none;
uint8_t brightnessPercent = 85;
uint8_t displayRotation = 1;
String wifiSsid;
String wifiPassword;
String locationSearch;
String timeFormat = "24h";
String posixTimezone = kDefaultPosixTimezone;
String temperatureUnit = "celsius";
String windUnit = "kmh";
String pressureUnit = "hpa";
String flightsProvider = "airplanes.live";
String flightsApiToken;
bool flightsApiTokenPresent = false;
uint16_t flightsRadiusKm = 100;
uint8_t flightsMaximumAircraft = 12;
uint32_t flightsMinimumAltitudeFt = 0;
String configuredPortalPassword;
String portalPassword;
String portalCsrfToken;
String bambuddyProtocol = "http";
String bambuddyHost;
String bambuddyApiBasePath = "/api/v1";
uint16_t bambuddyPort = 9001;
uint16_t bambuddyPrinterId = 1;
bool bambuddyKeyPresent = false;
String bambuddyApiKey;
uint32_t weatherRefreshMilliseconds = 600000;
uint32_t flightsRefreshMilliseconds = 60000;
uint32_t bambuddyVisibleRefreshMilliseconds = 30000;
uint32_t bambuddyBackgroundRefreshMilliseconds = 120000;
uint32_t systemsRefreshMilliseconds = 60000;
String currentPage = "Home";
unsigned long lastStatusUpdate = 0;
unsigned long lastSerialStatus = 0;
unsigned long restartAt = 0;
unsigned long lastLiveUiUpdate = 0;
uint32_t renderedFlightsUpdate = UINT32_MAX;
LiveDataState renderedFlightsState = LiveDataState::idle;
FlightsData renderedFlightsData;
AircraftData selectedAircraft;
int16_t calendarMonthOffset = 0;
lv_point_t aircraftLinePoints[kMaximumTrackedAircraft][3][2];

struct TouchCalibration {
  bool valid = false;
  bool swapAxes = false;
  int32_t xAtLeft = 0;
  int32_t xAtRight = 0;
  int32_t yAtTop = 0;
  int32_t yAtBottom = 0;
};

TouchCalibration touchCalibration;

void setBacklight(uint8_t percent) {
  brightnessPercent = constrain(percent, 5, 100);
  const uint8_t duty = map(brightnessPercent, 0, 100, 0, 255);
  ledcWrite(kBacklightChannel, duty);

  if (currentPage == "Display" && brightnessLabel != nullptr) {
    lv_label_set_text_fmt(brightnessLabel, "%u%%", brightnessPercent);
  }
}

void displayFlush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* pixels) {
  const uint32_t width = area->x2 - area->x1 + 1;
  const uint32_t height = area->y2 - area->y1 + 1;

  display.startWrite();
  display.setAddrWindow(area->x1, area->y1, width, height);
  display.pushPixels(reinterpret_cast<uint16_t*>(pixels), width * height);
  display.endWrite();

  lv_disp_flush_ready(driver);
}

void touchDelay() {
  delayMicroseconds(1);
}

void touchWriteByte(uint8_t value) {
  for (int bit = 7; bit >= 0; --bit) {
    digitalWrite(kTouchMosi, (value >> bit) & 1);
    digitalWrite(kTouchClock, HIGH);
    touchDelay();
    digitalWrite(kTouchClock, LOW);
    touchDelay();
  }
}

uint16_t touchReadChannel(uint8_t command) {
  digitalWrite(kTouchCs, LOW);
  touchWriteByte(command);

  uint16_t value = 0;
  for (int bit = 0; bit < 16; ++bit) {
    digitalWrite(kTouchClock, HIGH);
    touchDelay();
    value = static_cast<uint16_t>((value << 1) | digitalRead(kTouchMiso));
    digitalWrite(kTouchClock, LOW);
    touchDelay();
  }

  digitalWrite(kTouchCs, HIGH);
  return static_cast<uint16_t>((value >> 3) & 0x0FFF);
}

bool readRawTouch(int& rawX, int& rawY) {
  if (digitalRead(kTouchIrq) == HIGH) {
    return false;
  }

  uint32_t xTotal = 0;
  uint32_t yTotal = 0;
  for (int sample = 0; sample < 3; ++sample) {
    xTotal += touchReadChannel(0xD0);
    yTotal += touchReadChannel(0x90);
  }
  rawX = xTotal / 3;
  rawY = yTotal / 3;

  if (rawX < 100 || rawY < 100 || rawX > 4000 || rawY > 4000) {
    return false;
  }

  return true;
}

bool mapTouchCoordinates(int rawX, int rawY, int& x, int& y) {
  if (!touchCalibration.valid) {
    return false;
  }

  const int xAxis = touchCalibration.swapAxes ? rawY : rawX;
  const int yAxis = touchCalibration.swapAxes ? rawX : rawY;
  x = map(xAxis, touchCalibration.xAtLeft, touchCalibration.xAtRight, 0, kScreenWidth - 1);
  y = map(yAxis, touchCalibration.yAtTop, touchCalibration.yAtBottom, 0, kScreenHeight - 1);
  x = constrain(x, 0, kScreenWidth - 1);
  y = constrain(y, 0, kScreenHeight - 1);
  return true;
}

bool readTouch(int& x, int& y) {
  int rawX = 0;
  int rawY = 0;
  return readRawTouch(rawX, rawY) && mapTouchCoordinates(rawX, rawY, x, y);
}

void touchRead(lv_indev_drv_t*, lv_indev_data_t* data) {
  int x = 0;
  int y = 0;
  if (readTouch(x, y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

bool loadJsonFile(const char* path, JsonDocument& document) {
  if (!SD.exists(path)) {
    Serial.printf("[SD] Missing %s\n", path);
    return false;
  }

  File file = SD.open(path, FILE_READ);
  if (!file) {
    Serial.printf("[SD] Could not open %s\n", path);
    return false;
  }

  const DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error) {
    Serial.printf("[SD] Invalid JSON in %s: %s\n", path, error.c_str());
    return false;
  }

  return true;
}

void loadConfiguration() {
  if (!sdReady) {
    return;
  }
  xSemaphoreTake(sdMutex, portMAX_DELAY);

  wifiSsid = "";
  wifiPassword = "";
  configuredPortalPassword = "";
  locationSearch = "";
  timeFormat = "24h";
  posixTimezone = kDefaultPosixTimezone;
  temperatureUnit = "celsius";
  windUnit = "kmh";
  pressureUnit = "hpa";
  flightsProvider = "airplanes.live";
  flightsRadiusKm = 100;
  flightsMaximumAircraft = 12;
  flightsMinimumAltitudeFt = 0;
  flightsApiToken = "";
  flightsApiTokenPresent = false;
  bambuddyProtocol = "http";
  bambuddyHost = "";
  bambuddyApiBasePath = "/api/v1";
  bambuddyPort = 9001;
  bambuddyPrinterId = 1;
  bambuddyKeyPresent = false;
  bambuddyApiKey = "";

  JsonDocument config;
  configReady = loadJsonFile("/dashboard/config.json", config);
  if (configReady) {
    setBacklight(config["display"]["brightness_percent"] | brightnessPercent);
    locationSearch = String(config["location"]["search"] | "");
    timeFormat = String(config["locale"]["time_format"] | "24h");
    posixTimezone =
        String(config["locale"]["posix_timezone"] | kDefaultPosixTimezone);
    temperatureUnit =
        String(config["weather"]["temperature_unit"] | "celsius");
    windUnit = String(config["weather"]["wind_speed_unit"] | "kmh");
    pressureUnit = String(config["weather"]["pressure_unit"] | "hpa");
    flightsProvider =
        String(config["flights"]["provider"] | "airplanes.live");
    flightsRadiusKm =
        constrain(config["flights"]["radius_km"] | 100, 10, 250);
    flightsMaximumAircraft =
        constrain(config["flights"]["maximum_aircraft"] | 12, 1, 20);
    flightsMinimumAltitudeFt =
        constrain(config["flights"]["minimum_altitude_ft"] | 0, 0, 60000);
    bambuddyProtocol = String(config["services"]["bambuddy"]["protocol"] | "http");
    bambuddyHost = String(config["services"]["bambuddy"]["host"] | "");
    bambuddyPort = config["services"]["bambuddy"]["port"] | 9001;
    bambuddyApiBasePath =
        String(config["services"]["bambuddy"]["api_base_path"] | "/api/v1");
    bambuddyPrinterId = config["services"]["bambuddy"]["printer_id"] | 1;
    weatherRefreshMilliseconds =
        static_cast<uint32_t>(
            constrain(config["refresh_seconds"]["weather"] | 600, 600, 86400)) *
        1000UL;
    flightsRefreshMilliseconds =
        static_cast<uint32_t>(
            constrain(config["refresh_seconds"]["flights"] | 60, 15, 900)) *
        1000UL;
    bambuddyVisibleRefreshMilliseconds =
        static_cast<uint32_t>(
            constrain(
                config["refresh_seconds"]["bambuddy_visible"] | 30,
                15,
                3600)) *
        1000UL;
    bambuddyBackgroundRefreshMilliseconds =
        static_cast<uint32_t>(
            constrain(
                config["refresh_seconds"]["bambuddy_background"] | 120,
                15,
                86400)) *
        1000UL;
    systemsRefreshMilliseconds =
        static_cast<uint32_t>(
            constrain(config["refresh_seconds"]["systems"] | 60, 15, 900)) *
        1000UL;
    for (int index = 0; index < 5; ++index) {
      tileEnabled[index] = config["tile_visibility"][kTileIds[index]] | true;
    }
  }

  Preferences tilePreferences;
  tilePreferences.begin("tiles", false);
  for (int index = 0; index < 5; ++index) {
    if (tilePreferences.isKey(kTileIds[index])) {
      tileEnabled[index] = tilePreferences.getBool(kTileIds[index], tileEnabled[index]);
    }
  }
  tilePreferences.end();

  JsonDocument connections;
  connectionsReady = loadJsonFile("/dashboard/connections.json", connections);
  if (connectionsReady) {
    wifiSsid = String(connections["wifi"]["ssid"] | "");
    wifiPassword = String(connections["wifi"]["password"] | "");
    if (wifiSsid == "YOUR_WIFI_NETWORK" &&
        wifiPassword == "YOUR_WIFI_PASSWORD") {
      wifiSsid = "";
      wifiPassword = "";
    }
    const char* bambuddyKey =
        connections["services"]["bambuddy_readonly_api_key"] | "";
    bambuddyKeyPresent = strlen(bambuddyKey) > 0;
    bambuddyApiKey = bambuddyKey;
    const char* flightsToken =
        connections["services"]["flights_api_token"] | "";
    if (strlen(flightsToken) == 0) {
      flightsToken = connections["services"]["flights_proxy_token"] | "";
    }
    flightsApiTokenPresent = strlen(flightsToken) > 0;
    flightsApiToken = flightsToken;
    configuredPortalPassword =
        String(connections["device"]["settings_portal_password"] | "");
  }
  setupProvisioningAllowed =
      !connectionsReady ||
      (wifiSsid.isEmpty() && wifiPassword.isEmpty() &&
       configuredPortalPassword.isEmpty());

  LiveDataSettings liveSettings;
  strlcpy(
      liveSettings.locationSearch,
      locationSearch.c_str(),
      sizeof(liveSettings.locationSearch));
  strlcpy(
      liveSettings.temperatureUnit,
      temperatureUnit.c_str(),
      sizeof(liveSettings.temperatureUnit));
  strlcpy(
      liveSettings.windUnit,
      windUnit.c_str(),
      sizeof(liveSettings.windUnit));
  strlcpy(
      liveSettings.pressureUnit,
      pressureUnit.c_str(),
      sizeof(liveSettings.pressureUnit));
  strlcpy(
      liveSettings.flightsProvider,
      flightsProvider.c_str(),
      sizeof(liveSettings.flightsProvider));
  strlcpy(
      liveSettings.flightsApiToken,
      flightsApiToken.c_str(),
      sizeof(liveSettings.flightsApiToken));
  liveSettings.flightsRadiusKm = flightsRadiusKm;
  liveSettings.flightsMaximumAircraft = flightsMaximumAircraft;
  liveSettings.flightsMinimumAltitudeFt = flightsMinimumAltitudeFt;
  strlcpy(
      liveSettings.bambuddyProtocol,
      bambuddyProtocol.c_str(),
      sizeof(liveSettings.bambuddyProtocol));
  strlcpy(
      liveSettings.bambuddyHost,
      bambuddyHost.c_str(),
      sizeof(liveSettings.bambuddyHost));
  strlcpy(
      liveSettings.bambuddyApiPath,
      bambuddyApiBasePath.c_str(),
      sizeof(liveSettings.bambuddyApiPath));
  strlcpy(
      liveSettings.bambuddyApiKey,
      bambuddyApiKey.c_str(),
      sizeof(liveSettings.bambuddyApiKey));
  liveSettings.bambuddyPort = bambuddyPort;
  liveSettings.bambuddyPrinterId = bambuddyPrinterId;
  for (size_t index = 0; index < kMaximumSystemMonitors; ++index) {
    JsonObjectConst monitor =
        config["services"]["systems"]["monitors"][index].as<JsonObjectConst>();
    const char* monitorName = monitor["name"] | "";
    const char* monitorHost = monitor["host"] | "";
    strlcpy(
        liveSettings.systemMonitors[index].name,
        monitorName,
        sizeof(liveSettings.systemMonitors[index].name));
    strlcpy(
        liveSettings.systemMonitors[index].type,
        monitor["type"] | "http",
        sizeof(liveSettings.systemMonitors[index].type));
    strlcpy(
        liveSettings.systemMonitors[index].host,
        monitorHost,
        sizeof(liveSettings.systemMonitors[index].host));
    strlcpy(
        liveSettings.systemMonitors[index].path,
        monitor["path"] | "/",
        sizeof(liveSettings.systemMonitors[index].path));
    liveSettings.systemMonitors[index].port =
        monitor["port"] | 80;
    liveSettings.systemMonitors[index].enabled =
        monitor["enabled"] |
        (strlen(monitorName) > 0 || strlen(monitorHost) > 0);
    systemMonitorEnabled[index] =
        liveSettings.systemMonitors[index].enabled;
  }
  liveDataConfigure(liveSettings);
  xSemaphoreGive(sdMutex);
}

void startWifi() {
  if (wifiSsid.isEmpty()) {
    Serial.println("[WIFI] No SSID configured");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  wifiStarted = true;
  Serial.printf("[WIFI] Connecting to %s\n", wifiSsid.c_str());
}

String wifiSummary() {
  if (!wifiStarted) {
    return "Wi-Fi not configured";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  return "Wi-Fi connecting";
}

String randomHexToken(size_t characters) {
  String token;
  token.reserve(characters);
  while (token.length() < characters) {
    char block[9];
    snprintf(block, sizeof(block), "%08lx", static_cast<unsigned long>(esp_random()));
    token += block;
  }
  token.toUpperCase();
  return token.substring(0, characters);
}

void initialisePortalCredentials() {
  portalCsrfToken = randomHexToken(24);
  if (!configuredPortalPassword.isEmpty()) {
    portalPassword = configuredPortalPassword;
    portalUsesBootstrapPassword = false;
    return;
  }

  Preferences preferences;
  preferences.begin("portal", false);
  if (preferences.isKey("bootstrap")) {
    portalPassword = preferences.getString("bootstrap", "");
  }
  if (portalPassword.isEmpty()) {
    portalPassword = randomHexToken(10);
    preferences.putString("bootstrap", portalPassword);
  }
  preferences.end();
  portalUsesBootstrapPassword = true;
  Serial.println("[PORTAL] Bootstrap password available on Settings > System");
}

bool requirePortalAuthentication() {
  if (settingsServer.authenticate("admin", portalPassword.c_str())) {
    return true;
  }

  settingsServer.requestAuthentication(
      BASIC_AUTH,
      "Desk Dashboard Settings",
      "Authentication required");
  return false;
}

String htmlEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 16);
  for (size_t index = 0; index < value.length(); ++index) {
    switch (value[index]) {
      case '&':
        escaped += F("&amp;");
        break;
      case '<':
        escaped += F("&lt;");
        break;
      case '>':
        escaped += F("&gt;");
        break;
      case '"':
        escaped += F("&quot;");
        break;
      case '\'':
        escaped += F("&#39;");
        break;
      default:
        escaped += value[index];
        break;
    }
  }
  return escaped;
}

String urlEncodeComponent(const String& value) {
  static constexpr char hex[] = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(value.length() * 2);
  for (size_t index = 0; index < value.length(); ++index) {
    const uint8_t character = static_cast<uint8_t>(value[index]);
    if (isalnum(character) || character == '-' || character == '_' ||
        character == '.' || character == '~') {
      encoded += static_cast<char>(character);
    } else {
      encoded += '%';
      encoded += hex[character >> 4];
      encoded += hex[character & 0x0F];
    }
  }
  return encoded;
}

bool validSdPath(String path, bool allowRoot = true) {
  path.trim();
  if (!path.startsWith("/") || path.length() > 180 ||
      path.indexOf("..") >= 0 || path.indexOf('\\') >= 0 ||
      path.indexOf("//") >= 0 || (!allowRoot && path == "/")) {
    return false;
  }
  for (size_t index = 0; index < path.length(); ++index) {
    if (static_cast<uint8_t>(path[index]) < 32) {
      return false;
    }
  }
  return true;
}

bool validUploadFilename(const String& filename) {
  if (filename.isEmpty() || filename.length() > 96 ||
      filename == "." || filename == ".." ||
      filename.indexOf('/') >= 0 || filename.indexOf('\\') >= 0) {
    return false;
  }
  for (size_t index = 0; index < filename.length(); ++index) {
    if (static_cast<uint8_t>(filename[index]) < 32) {
      return false;
    }
  }
  return true;
}

bool jpegDimensions(File& file, uint16_t& width, uint16_t& height) {
  width = 0;
  height = 0;
  if (file.size() < 4 || !file.seek(file.size() - 2) ||
      file.read() != 0xFF || file.read() != 0xD9 ||
      !file.seek(0) || file.read() != 0xFF || file.read() != 0xD8) {
    return false;
  }

  while (file.available()) {
    int prefix = file.read();
    while (prefix != 0xFF && prefix >= 0) {
      prefix = file.read();
    }
    int marker = file.read();
    while (marker == 0xFF) {
      marker = file.read();
    }
    if (marker < 0 || marker == 0xD9 || marker == 0xDA) {
      break;
    }
    if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
      continue;
    }

    const int high = file.read();
    const int low = file.read();
    if (high < 0 || low < 0) {
      return false;
    }
    const uint16_t segmentLength =
        static_cast<uint16_t>((high << 8) | low);
    if (segmentLength < 2) {
      return false;
    }
    if (marker == 0xC0) {
      if (segmentLength < 7 || file.read() < 0) {
        return false;
      }
      const int heightHigh = file.read();
      const int heightLow = file.read();
      const int widthHigh = file.read();
      const int widthLow = file.read();
      if (heightHigh < 0 || heightLow < 0 ||
          widthHigh < 0 || widthLow < 0) {
        return false;
      }
      height = static_cast<uint16_t>((heightHigh << 8) | heightLow);
      width = static_cast<uint16_t>((widthHigh << 8) | widthLow);
      return true;
    }
    if (!file.seek(file.position() + segmentLength - 2)) {
      return false;
    }
  }
  return false;
}

class ScopedSdLock {
 public:
  explicit ScopedSdLock(
      uint32_t timeoutMilliseconds = 10000,
      bool waitForever = false)
      : locked_(
            sdMutex == nullptr ||
            xSemaphoreTake(
                sdMutex,
                waitForever
                    ? portMAX_DELAY
                    : pdMS_TO_TICKS(timeoutMilliseconds)) == pdTRUE) {}

  ~ScopedSdLock() {
    if (locked_ && sdMutex != nullptr) {
      xSemaphoreGive(sdMutex);
    }
  }

  explicit operator bool() const {
    return locked_;
  }

 private:
  bool locked_;
};

String parentSdPath(const String& path) {
  if (path == "/") {
    return "/";
  }
  const int separator = path.lastIndexOf('/');
  return separator <= 0 ? "/" : path.substring(0, separator);
}

String joinSdPath(const String& directory, const String& name) {
  return directory == "/" ? "/" + name : directory + "/" + name;
}

String lvglSdPath(const char* path) {
  return path[0] == '/' ? String(path) : "/" + String(path);
}

void* lvglSdOpen(lv_fs_drv_t*, const char* path, lv_fs_mode_t mode) {
  if (!sdReady || mode != LV_FS_MODE_RD) {
    return nullptr;
  }
  ScopedSdLock lock(0, true);
  if (!lock) {
    return nullptr;
  }
  File* file = new (std::nothrow) File(SD.open(lvglSdPath(path), FILE_READ));
  if (file == nullptr) {
    return nullptr;
  }
  if (!*file || file->isDirectory()) {
    file->close();
    delete file;
    return nullptr;
  }
  return file;
}

lv_fs_res_t lvglSdClose(lv_fs_drv_t*, void* filePointer) {
  ScopedSdLock lock(0, true);
  if (!lock) {
    return LV_FS_RES_FS_ERR;
  }
  File* file = static_cast<File*>(filePointer);
  file->close();
  delete file;
  return LV_FS_RES_OK;
}

lv_fs_res_t lvglSdRead(
    lv_fs_drv_t*,
    void* filePointer,
    void* buffer,
    uint32_t bytesToRead,
    uint32_t* bytesRead) {
  ScopedSdLock lock(0, true);
  if (!lock) {
    return LV_FS_RES_FS_ERR;
  }
  File* file = static_cast<File*>(filePointer);
  *bytesRead = file->read(static_cast<uint8_t*>(buffer), bytesToRead);
  return LV_FS_RES_OK;
}

lv_fs_res_t lvglSdSeek(
    lv_fs_drv_t*,
    void* filePointer,
    uint32_t position,
    lv_fs_whence_t whence) {
  ScopedSdLock lock(0, true);
  if (!lock) {
    return LV_FS_RES_FS_ERR;
  }
  SeekMode mode = SeekSet;
  if (whence == LV_FS_SEEK_CUR) {
    mode = SeekCur;
  } else if (whence == LV_FS_SEEK_END) {
    mode = SeekEnd;
  }
  return static_cast<File*>(filePointer)->seek(position, mode)
             ? LV_FS_RES_OK
             : LV_FS_RES_FS_ERR;
}

lv_fs_res_t lvglSdTell(
    lv_fs_drv_t*,
    void* filePointer,
    uint32_t* position) {
  ScopedSdLock lock(0, true);
  if (!lock) {
    return LV_FS_RES_FS_ERR;
  }
  *position = static_cast<File*>(filePointer)->position();
  return LV_FS_RES_OK;
}

bool sdIconAvailable(const char* lvglPath) {
  if (!sdReady || strncmp(lvglPath, "S:", 2) != 0) {
    return false;
  }
  ScopedSdLock lock(0, true);
  if (!lock) {
    return false;
  }
  File file = SD.open(lvglPath + 2, FILE_READ);
  if (!file || file.isDirectory() || file.size() != 1156) {
    file.close();
    return false;
  }
  uint32_t header = 0;
  const bool valid =
      file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) ==
          sizeof(header) &&
      header == (13U | (48U << 10) | (48U << 21));
  file.close();
  return valid;
}

String selectedIf(bool selected) {
  return selected ? " selected" : "";
}

String checkedIf(bool checked) {
  return checked ? " checked" : "";
}

bool loadPortalDocuments(
    JsonDocument& config,
    JsonDocument& connections,
    String& error) {
  if (!sdReady) {
    error = "The SD card is not available.";
    return false;
  }
  if (!loadJsonFile("/dashboard/config.json", config)) {
    error = "config.json is missing or invalid.";
    return false;
  }
  if (!loadJsonFile("/dashboard/connections.json", connections)) {
    error = "connections.json is missing or invalid.";
    return false;
  }
  return true;
}

bool writeJsonStage(const char* path, JsonDocument& document, String& error) {
  if (SD.exists(path)) {
    SD.remove(path);
  }

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    error = String("Could not create ") + path;
    return false;
  }

  const size_t written = serializeJsonPretty(document, file);
  file.close();
  if (written == 0) {
    SD.remove(path);
    error = String("Could not write ") + path;
    return false;
  }

  JsonDocument verification;
  if (!loadJsonFile(path, verification)) {
    SD.remove(path);
    error = String("Validation failed for ") + path;
    return false;
  }
  return true;
}

bool writePortalTransactionMarker(
    bool hadConfig,
    bool hadConnections,
    String& error) {
  constexpr const char* markerPath = "/dashboard/portal.transaction";
  SD.remove(markerPath);
  File marker = SD.open(markerPath, FILE_WRITE);
  if (!marker) {
    error = "Could not create the configuration transaction marker.";
    return false;
  }
  const char flags[] = {
      hadConfig ? '1' : '0',
      hadConnections ? '1' : '0',
  };
  const bool written = marker.write(
      reinterpret_cast<const uint8_t*>(flags),
      sizeof(flags)) == sizeof(flags);
  marker.flush();
  marker.close();
  if (!written) {
    SD.remove(markerPath);
    error = "Could not write the configuration transaction marker.";
    return false;
  }
  marker = SD.open(markerPath, FILE_READ);
  char verification[2] = {};
  const bool verified =
      marker &&
      marker.read(reinterpret_cast<uint8_t*>(verification), 2) == 2 &&
      verification[0] == flags[0] && verification[1] == flags[1];
  marker.close();
  if (!verified) {
    SD.remove(markerPath);
    error = "Could not verify the configuration transaction marker.";
    return false;
  }
  return true;
}

bool replacePortalDocuments(
    JsonDocument& config,
    JsonDocument& connections,
    String& error) {
  constexpr const char* configLive = "/dashboard/config.json";
  constexpr const char* configStage = "/dashboard/config.portal-new.json";
  constexpr const char* configBackup = "/dashboard/config.portal-backup.json";
  constexpr const char* connectionsLive = "/dashboard/connections.json";
  constexpr const char* connectionsStage =
      "/dashboard/connections.portal-new.json";
  constexpr const char* connectionsBackup =
      "/dashboard/connections.portal-backup.json";
  constexpr const char* transactionMarker =
      "/dashboard/portal.transaction";

  if (SD.exists(transactionMarker)) {
    error = "An interrupted configuration save requires a restart.";
    return false;
  }
  if (!writeJsonStage(configStage, config, error)) {
    return false;
  }
  if (!writeJsonStage(connectionsStage, connections, error)) {
    SD.remove(configStage);
    return false;
  }

  if (SD.exists(configBackup)) {
    SD.remove(configBackup);
  }
  if (SD.exists(connectionsBackup)) {
    SD.remove(connectionsBackup);
  }

  const bool hadConfig = SD.exists(configLive);
  const bool hadConnections = SD.exists(connectionsLive);
  if (!writePortalTransactionMarker(hadConfig, hadConnections, error)) {
    SD.remove(configStage);
    SD.remove(connectionsStage);
    return false;
  }
  if (hadConfig && !SD.rename(configLive, configBackup)) {
    SD.remove(configStage);
    SD.remove(connectionsStage);
    SD.remove(transactionMarker);
    error = "Could not preserve config.json before saving.";
    return false;
  }

  if (hadConnections &&
      !SD.rename(connectionsLive, connectionsBackup)) {
    if (hadConfig) {
      SD.rename(configBackup, configLive);
    }
    SD.remove(configStage);
    SD.remove(connectionsStage);
    SD.remove(transactionMarker);
    error = "Could not preserve connections.json before saving.";
    return false;
  }

  if (!SD.rename(configStage, configLive)) {
    if (hadConfig) {
      SD.rename(configBackup, configLive);
    }
    if (hadConnections) {
      SD.rename(connectionsBackup, connectionsLive);
    }
    SD.remove(connectionsStage);
    SD.remove(transactionMarker);
    error = "Could not install the new config.json.";
    return false;
  }

  if (!SD.rename(connectionsStage, connectionsLive)) {
    SD.remove(configLive);
    if (hadConfig) {
      SD.rename(configBackup, configLive);
    }
    if (hadConnections) {
      SD.rename(connectionsBackup, connectionsLive);
    }
    SD.remove(transactionMarker);
    error = "Could not install the new connections.json.";
    return false;
  }

  if (SD.exists(transactionMarker) &&
      !SD.remove(transactionMarker)) {
    error = "Could not commit the configuration transaction.";
    return false;
  }
  SD.remove(configBackup);
  SD.remove(connectionsBackup);
  return true;
}

bool parsePortalInteger(
    const String& field,
    int minimum,
    int maximum,
    int& value,
    String& error) {
  if (!settingsServer.hasArg(field)) {
    error = "Missing field: " + field;
    return false;
  }

  String text = settingsServer.arg(field);
  text.trim();
  char* end = nullptr;
  const long parsed = strtol(text.c_str(), &end, 10);
  if (text.isEmpty() || end == nullptr || *end != '\0' ||
      parsed < minimum || parsed > maximum) {
    error = field + " must be between " + String(minimum) + " and " +
            String(maximum) + ".";
    return false;
  }

  value = static_cast<int>(parsed);
  return true;
}

bool validPortalHost(const String& host) {
  if (host.length() > 128) {
    return false;
  }
  for (size_t index = 0; index < host.length(); ++index) {
    if (isWhitespace(host[index]) || host[index] == '/') {
      return false;
    }
  }
  return true;
}

bool validPosixTimezone(const String& timezone) {
  if (timezone.isEmpty() || timezone.length() > 80) {
    return false;
  }
  for (size_t index = 0; index < timezone.length(); ++index) {
    const uint8_t character = static_cast<uint8_t>(timezone[index]);
    if (character < 33 || character > 126) {
      return false;
    }
  }
  return true;
}

void appendPortalOption(
    String& html,
    const char* value,
    const char* label,
    const String& current) {
  html += F("<option value=\"");
  html += value;
  html += '"';
  html += selectedIf(current == value);
  html += '>';
  html += label;
  html += F("</option>");
}

bool loadSetupDocuments(
    JsonDocument& config,
    JsonDocument& connections,
    String& error) {
  if (!sdReady) {
    error = "Insert a writable microSD card before setup.";
    return false;
  }
  if (!loadJsonFile("/dashboard/config.json", config) &&
      deserializeJson(config, kConfigExample)) {
    error = "Could not prepare the default configuration.";
    return false;
  }
  if (!loadJsonFile("/dashboard/connections.json", connections) &&
      deserializeJson(connections, kConnectionsExample)) {
    error = "Could not prepare the default connection settings.";
    return false;
  }
  return true;
}

void handleSetupRoot() {
  String html;
  if (!html.reserve(5000)) {
    settingsServer.send(503, "text/plain", "Not enough memory for setup.");
    return;
  }
  html += F(
      "<!doctype html><html><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>Desk Dashboard Setup</title><style>"
      ":root{color-scheme:dark;font-family:system-ui,sans-serif}"
      "body{margin:0;background:#0f172a;color:#e2e8f0}"
      "main{max-width:560px;margin:auto;padding:24px}"
      "section{background:#1e293b;border:1px solid #334155;border-radius:12px;"
      "padding:18px}p{color:#a5b4cc;line-height:1.5}"
      "label{display:flex;flex-direction:column;gap:6px;margin:14px 0;"
      "font-weight:650}input{box-sizing:border-box;width:100%;padding:11px;"
      "border-radius:7px;border:1px solid #475569;background:#0f172a;"
      "color:#f8fafc}button{padding:12px 18px;border:0;border-radius:8px;"
      "background:#2563eb;color:white;font-weight:700;cursor:pointer}"
      ".hint{font-size:.9rem}</style></head><body><main><section>"
      "<h1>Desk Dashboard setup</h1>"
      "<p>Connect the dashboard to a 2.4 GHz Wi-Fi network and create the "
      "password used by its browser portal.</p>"
      "<form method=\"post\" action=\"/setup/save\">"
      "<input type=\"hidden\" name=\"csrf\" value=\"");
  html += portalCsrfToken;
  html += F(
      "\"><label>Wi-Fi name<input name=\"wifi_ssid\" maxlength=\"32\" "
      "autocomplete=\"username\" required></label>"
      "<label>Wi-Fi password<input name=\"wifi_password\" type=\"password\" "
      "maxlength=\"64\" autocomplete=\"current-password\"></label>"
      "<label>Portal password<input name=\"portal_password\" type=\"password\" "
      "minlength=\"8\" maxlength=\"64\" autocomplete=\"new-password\" required>"
      "</label><label>POSIX timezone<input name=\"posix_timezone\" "
      "maxlength=\"80\" value=\"");
  html += htmlEscape(posixTimezone);
  html += F(
      "\" required></label><p class=\"hint\">The timezone controls the clock "
      "and Calendar, including daylight-saving changes.</p>"
      "<button type=\"submit\">Save and restart</button></form>"
      "</section></main></body></html>");
  settingsServer.send(200, "text/html; charset=utf-8", html);
}

void handleSetupSave() {
  if (!setupMode || !setupProvisioningAllowed) {
    settingsServer.send(404, "text/plain", "Setup mode is not active.");
    return;
  }
  if (settingsServer.arg("csrf") != portalCsrfToken) {
    settingsServer.send(403, "text/plain", "Invalid form token.");
    return;
  }

  String ssid = settingsServer.arg("wifi_ssid");
  const String password = settingsServer.arg("wifi_password");
  const String newPortalPassword = settingsServer.arg("portal_password");
  String timezone = settingsServer.arg("posix_timezone");
  ssid.trim();
  timezone.trim();
  if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 64 ||
      newPortalPassword.length() < 8 ||
      newPortalPassword.length() > 64 ||
      !validPosixTimezone(timezone)) {
    settingsServer.send(400, "text/plain", "One or more setup values are invalid.");
    return;
  }

  JsonDocument config;
  JsonDocument connections;
  String error;
  {
    ScopedSdLock lock;
    if (!lock) {
      settingsServer.send(503, "text/plain", "The SD card is busy. Try again.");
      return;
    }
    if (!loadSetupDocuments(config, connections, error)) {
      settingsServer.send(500, "text/plain", error);
      return;
    }
    if (String(config["location"]["search"] | "") ==
        "YOUR TOWN OR POSTCODE") {
      config["location"]["search"] = "";
    }
    if (String(config["services"]["bambuddy"]["host"] | "") ==
        "192.168.1.50") {
      config["services"]["bambuddy"]["host"] = "";
    }
    if (String(
            connections["services"]["bambuddy_readonly_api_key"] | "") ==
        "bb_READ_STATUS_KEY_ONLY") {
      connections["services"]["bambuddy_readonly_api_key"] = "";
    }
    config["locale"]["posix_timezone"] = timezone;
    connections["wifi"]["ssid"] = ssid;
    connections["wifi"]["password"] = password;
    connections["device"]["settings_portal_password"] =
        newPortalPassword;
    if (!replacePortalDocuments(config, connections, error)) {
      settingsServer.send(500, "text/plain", error);
      return;
    }
  }

  settingsServer.send(
      200,
      "text/html; charset=utf-8",
      "<!doctype html><html><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<meta http-equiv=\"refresh\" content=\"15;url=/\"></head>"
      "<body><h1>Settings saved</h1><p>The dashboard is restarting and "
      "connecting to your Wi-Fi network.</p></body></html>");
  restartPending = true;
  restartAt = millis() + 1500;
}

void handlePortalRoot() {
  if (!requirePortalAuthentication()) {
    return;
  }

  JsonDocument config;
  JsonDocument connections;
  String error;
  {
    ScopedSdLock lock;
    if (!lock) {
      settingsServer.send(503, "text/plain", "The SD card is busy.");
      return;
    }
    if (!loadPortalDocuments(config, connections, error)) {
      settingsServer.send(500, "text/plain", error);
      return;
    }
  }

  const String locationSearch = String(config["location"]["search"] | "");
  const String timeFormat = String(config["locale"]["time_format"] | "24h");
  const String dateFormat =
      String(config["locale"]["date_format"] | "yyyy-dd-mm");
  const String timezone =
      String(config["locale"]["posix_timezone"] | kDefaultPosixTimezone);
  const String temperatureUnit =
      String(config["weather"]["temperature_unit"] | "celsius");
  const String windUnit =
      String(config["weather"]["wind_speed_unit"] | "kmh");
  const String pressureUnit =
      String(config["weather"]["pressure_unit"] | "hpa");
  const String precipitationUnit =
      String(config["weather"]["precipitation_unit"] | "mm");
  const String aircraftProvider =
      String(config["flights"]["provider"] | "airplanes.live");
  const String protocol =
      String(config["services"]["bambuddy"]["protocol"] | "http");
  const String host = String(config["services"]["bambuddy"]["host"] | "");
  const String apiPath =
      String(config["services"]["bambuddy"]["api_base_path"] | "/api/v1");
  const String ssid = String(connections["wifi"]["ssid"] | "");

  String html;
  if (!html.reserve(13500)) {
    settingsServer.send(
        503,
        "text/plain",
        "Not enough memory to build the settings page. Try again.");
    return;
  }
  html += F(
      "<!doctype html><html><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>Desk Dashboard Settings</title><style>"
      ":root{color-scheme:dark;font-family:system-ui,sans-serif}"
      "body{margin:0;background:#0f172a;color:#e2e8f0}"
      "main{max-width:900px;margin:auto;padding:20px}"
      "h1{margin:0 0 4px}p{color:#94a3b8}"
      "section{background:#1e293b;border:1px solid #334155;border-radius:12px;"
      "padding:16px;margin:14px 0}"
      "details{margin:14px 0}details section{margin:0}"
      "summary{cursor:pointer;list-style:none;background:#1e293b;"
      "border:1px solid #334155;border-radius:12px;padding:16px}"
      "summary::-webkit-details-marker{display:none}"
      "summary h2{display:inline;margin:0}"
      "summary:after{content:'+';float:right;font-size:1.5rem;line-height:1}"
      "details[open] summary{border-radius:12px 12px 0 0}"
      "details[open] summary:after{content:'-'}"
      "details[open] section{border-top:0;border-radius:0 0 12px 12px}"
      ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}"
      "label{display:flex;flex-direction:column;gap:5px;font-weight:600}"
      "input,select{box-sizing:border-box;width:100%;padding:10px;border-radius:7px;"
      "border:1px solid #475569;background:#0f172a;color:#f8fafc}"
      ".check{flex-direction:row;align-items:center;font-weight:500}"
      ".check input{width:auto}.hint{font-size:.9rem;color:#94a3b8}"
      "button{padding:11px 18px;border:0;border-radius:8px;background:#2563eb;"
      "color:white;font-weight:700;cursor:pointer}.danger{background:#b91c1c}"
      ".nav{display:inline-block;margin-top:10px;color:#93c5fd}"
      ".notice{padding:12px;border-radius:8px;background:#14532d;color:#dcfce7}"
      ".warning{padding:12px;border-radius:8px;background:#713f12;color:#fef3c7}"
      "</style></head><body><main><h1>Desk Dashboard</h1>"
      "<p>Local settings portal. Blank credential fields preserve their current values.</p>");
  const String saveResult = settingsServer.arg("saved");
  if (saveResult == "1") {
    html += F(
        "<p class=\"notice\"><strong>Settings saved.</strong> The live JSON "
        "files were validated and replaced safely.</p>");
  }
  if (settingsServer.arg("startup") == "1") {
    html += F(
        "<p class=\"notice\"><strong>Startup artwork updated.</strong> "
        "Restart the device to see it.</p>");
  }
  html += F(
      "<a class=\"nav\" href=\"/files\">Browse SD card files</a>"
      "<form method=\"post\" action=\"/save\">");
  html += F("<input type=\"hidden\" name=\"csrf\" value=\"");
  html += portalCsrfToken;
  html += F("\">");

  html += F("<section><h2>Display</h2><div class=\"grid\"><label>Brightness %"
            "<input name=\"brightness\" type=\"number\" min=\"5\" max=\"100\" value=\"");
  html += String(config["display"]["brightness_percent"] | 85);
  html += F("\"></label><label>Orientation<select name=\"rotation\">");
  appendPortalOption(html, "1", "Normal", String(config["display"]["rotation"] | 1));
  appendPortalOption(html, "3", "Rotate 180 degrees", String(config["display"]["rotation"] | 1));
  html += F("</select></label></div></section>");

  html += F("<section><h2>Location and formats</h2><div class=\"grid\">"
            "<label>Town, postcode, or place<input name=\"location_search\" maxlength=\"100\" value=\"");
  html += htmlEscape(locationSearch);
  html += F("\"></label><label>Time format<select name=\"time_format\">");
  appendPortalOption(html, "24h", "24 hour", timeFormat);
  appendPortalOption(html, "12h", "12 hour", timeFormat);
  html += F("</select></label><label>Date format<select name=\"date_format\">");
  appendPortalOption(html, "yyyy-dd-mm", "YYYY-DD-MM", dateFormat);
  appendPortalOption(html, "yyyy-mm-dd", "YYYY-MM-DD", dateFormat);
  appendPortalOption(html, "dd-mm-yyyy", "DD-MM-YYYY", dateFormat);
  appendPortalOption(html, "mm-dd-yyyy", "MM-DD-YYYY", dateFormat);
  html += F("</select></label><label>POSIX timezone<input name=\"posix_timezone\" maxlength=\"80\" value=\"");
  html += htmlEscape(timezone);
  html += F("\"></label><label>Temperature<select name=\"temperature_unit\">");
  appendPortalOption(html, "celsius", "Celsius", temperatureUnit);
  appendPortalOption(html, "fahrenheit", "Fahrenheit", temperatureUnit);
  html += F("</select></label><label>Wind speed<select name=\"wind_unit\">");
  appendPortalOption(html, "kmh", "km/h", windUnit);
  appendPortalOption(html, "mph", "mph", windUnit);
  html += F("</select></label><label>Pressure<select name=\"pressure_unit\">");
  appendPortalOption(html, "hpa", "hPa", pressureUnit);
  appendPortalOption(html, "inhg", "inHg", pressureUnit);
  html += F("</select></label><label>Precipitation<select name=\"precipitation_unit\">");
  appendPortalOption(html, "mm", "Millimetres", precipitationUnit);
  appendPortalOption(html, "in", "Inches", precipitationUnit);
  html += F("</select></label></div></section>");

  html += F("<section><h2>Flights</h2><div class=\"grid\">"
            "<label>Aircraft data provider<select name=\"flights_provider\">");
  appendPortalOption(
      html,
      "airplanes.live",
      "airplanes.live (no key required)",
      aircraftProvider);
  appendPortalOption(
      html,
      "adsb.lol",
      "adsb.lol (no key required)",
      aircraftProvider);
  appendPortalOption(
      html,
      "flightradar24",
      "Flightradar24 official API",
      aircraftProvider);
  html += F("</select></label><label>Range in km<input name=\"flights_radius\" type=\"number\" min=\"10\" max=\"250\" value=\"");
  html += String(config["flights"]["radius_km"] | 100);
  html += F("\"></label><label>Maximum aircraft<input name=\"flights_maximum\" type=\"number\" min=\"1\" max=\"20\" value=\"");
  html += String(config["flights"]["maximum_aircraft"] | 12);
  html += F("\"></label><label>Minimum altitude in feet<input name=\"flights_minimum_altitude\" type=\"number\" min=\"0\" max=\"60000\" value=\"");
  html += String(config["flights"]["minimum_altitude_ft"] | 0);
  html += F("\"></label><label>Refresh interval in seconds<input name=\"flights_refresh\" type=\"number\" min=\"15\" max=\"900\" value=\"");
  html += String(config["refresh_seconds"]["flights"] | 60);
  html += F("\"></label><label>Provider API token<input name=\"flights_token\" type=\"password\" maxlength=\"256\" autocomplete=\"new-password\" placeholder=\"Leave blank to keep current\"></label></div><p class=\"hint\">Current token: ");
  html += flightsApiTokenPresent ? "configured" : "not configured";
  html += F(". A token is required only for providers such as Flightradar24.</p>"
            "<label class=\"check\"><input type=\"checkbox\" name=\"clear_flights_token\">Clear the saved Flights token</label></section>");

  html += F("<section><h2>Bambuddy</h2><div class=\"grid\">"
            "<label>Protocol<select name=\"bambuddy_protocol\">");
  appendPortalOption(html, "http", "HTTP", protocol);
  html += F("</select></label><label>Hostname or IP<input name=\"bambuddy_host\" maxlength=\"128\" value=\"");
  html += htmlEscape(host);
  html += F("\"></label><label>Port<input name=\"bambuddy_port\" type=\"number\" min=\"1\" max=\"65535\" value=\"");
  html += String(config["services"]["bambuddy"]["port"] | 9001);
  html += F("\"></label><label>API base path<input name=\"bambuddy_path\" maxlength=\"64\" value=\"");
  html += htmlEscape(apiPath);
  html += F("\"></label><label>Printer ID<input name=\"bambuddy_printer\" type=\"number\" min=\"1\" max=\"65535\" value=\"");
  html += String(config["services"]["bambuddy"]["printer_id"] | 1);
  html += F("\"></label><label>Read-only API key<input name=\"bambuddy_key\" type=\"password\" maxlength=\"256\" autocomplete=\"new-password\" placeholder=\"Leave blank to keep current\"></label></div>");
  html += F("<p class=\"hint\">Current key: ");
  html += bambuddyKeyPresent ? "configured" : "not configured";
  html += F("</p><label class=\"check\"><input type=\"checkbox\" name=\"clear_bambuddy_key\">Clear the saved Bambuddy key</label></section>");

  html += F(
      "<section><h2>Systems monitoring</h2>"
      "<p class=\"hint\">HTTP checks use plain LAN HTTP. TCP checks test whether "
      "a host and port accept a connection.</p><div class=\"grid\">"
      "<label>Refresh interval in seconds<input name=\"systems_refresh\" "
      "type=\"number\" min=\"15\" max=\"900\" value=\"");
  html += String(config["refresh_seconds"]["systems"] | 60);
  html += F("\"></label></div>");
  for (size_t index = 0; index < kMaximumSystemMonitors; ++index) {
    JsonObjectConst monitor =
        config["services"]["systems"]["monitors"][index].as<JsonObjectConst>();
    const String monitorName = String(monitor["name"] | "");
    const String monitorHost = String(monitor["host"] | "");
    const bool monitorEnabled =
        monitor["enabled"] |
        (!monitorName.isEmpty() || !monitorHost.isEmpty());
    html += F("<h3>Monitor ");
    html += String(index + 1);
    html += F("</h3><label class=\"check\"><input type=\"checkbox\" name=\"systems_enabled_");
    html += String(index);
    html += '"';
    html += checkedIf(monitorEnabled);
    html += F(">Enable and display this monitor</label><div class=\"grid\"><label>Name<input name=\"systems_name_");
    html += String(index);
    html += F("\" maxlength=\"24\" value=\"");
    html += htmlEscape(monitorName);
    html += F("\"></label><label>Type<select name=\"systems_type_");
    html += String(index);
    html += F("\">");
    appendPortalOption(
        html,
        "http",
        "HTTP endpoint",
        String(monitor["type"] | "http"));
    appendPortalOption(
        html,
        "tcp",
        "TCP port",
        String(monitor["type"] | "http"));
    html += F("</select></label><label>Hostname or IP<input name=\"systems_host_");
    html += String(index);
    html += F("\" maxlength=\"64\" value=\"");
    html += htmlEscape(monitorHost);
    html += F("\"></label><label>Port<input name=\"systems_port_");
    html += String(index);
    html += F("\" type=\"number\" min=\"1\" max=\"65535\" value=\"");
    html += String(monitor["port"] | 80);
    html += F("\"></label><label>HTTP path<input name=\"systems_path_");
    html += String(index);
    html += F("\" maxlength=\"64\" value=\"");
    html += htmlEscape(String(monitor["path"] | "/"));
    html += F("\"></label></div>");
  }
  html += F("</section>");

  html += F("<section><h2>Home tiles</h2><div class=\"grid\">");
  for (int index = 0; index < 5; ++index) {
    html += F("<label class=\"check\"><input type=\"checkbox\" name=\"tile_");
    html += kTileIds[index];
    html += '"';
    html += checkedIf(config["tile_visibility"][kTileIds[index]] | true);
    html += '>';
    html += kTileNames[index];
    html += F("</label>");
  }
  html += F("</div><p class=\"hint\">Settings always remains available.</p></section>");

  html += F("<section><h2>Wi-Fi network</h2><div class=\"grid\">"
            "<label>Wi-Fi SSID<input name=\"wifi_ssid\" maxlength=\"32\" value=\"");
  html += htmlEscape(ssid);
  html += F("\"></label><label>Wi-Fi password<input name=\"wifi_password\" type=\"password\" maxlength=\"64\" autocomplete=\"new-password\" placeholder=\"Leave blank to keep current\"></label>"
            "</div><p class=\"hint\">Wi-Fi changes take effect after restart.</p></section>"
            "<section><h2>Portal security</h2><div class=\"grid\">"
            "<label>New portal password<input name=\"portal_password\" type=\"password\" maxlength=\"64\" autocomplete=\"new-password\" placeholder=\"Leave blank to keep current\"></label></div>"
            "<p class=\"hint\">Portal username: admin. New passwords must contain at least eight characters.</p></section>");

  html += F("<button type=\"submit\">Save settings</button></form>"
            "<section><h2>Startup artwork</h2>"
            "<p class=\"hint\">Upload a 320x240 JPEG no larger than 200 KB. "
            "The image is validated before it replaces the active file.</p>"
            "<form method=\"post\" action=\"/startup/upload\" "
            "enctype=\"multipart/form-data\">"
            "<input type=\"hidden\" name=\"csrf\" value=\"");
  html += portalCsrfToken;
  html += F("\"><input name=\"startup_image\" type=\"file\" "
            "accept=\"image/jpeg\" required>"
            "<button type=\"submit\">Upload startup image</button></form>"
            "</section>"
            "<form method=\"post\" action=\"/restart\" style=\"margin-top:14px\">"
            "<input type=\"hidden\" name=\"csrf\" value=\"");
  html += portalCsrfToken;
  html += F("\"><button class=\"danger\" type=\"submit\">Restart device</button>"
            "</form><script>"
            "document.querySelectorAll('section').forEach(function(s,i){"
            "var h=s.firstElementChild;if(!h||h.tagName!=='H2')return;"
            "var d=document.createElement('details');d.open=i===0;"
            "var m=document.createElement('summary');"
            "s.parentNode.insertBefore(d,s);m.appendChild(h);"
            "d.appendChild(m);d.appendChild(s);});"
            "</script></main></body></html>");

  Serial.printf(
      "[PORTAL] Settings page=%u bytes largest-block=%u\n",
      html.length(),
      ESP.getMaxAllocHeap());
  settingsServer.send(200, "text/html; charset=utf-8", html);
}

bool validChoice(
    const String& value,
    const char* const* choices,
    size_t choiceCount) {
  for (size_t index = 0; index < choiceCount; ++index) {
    if (value == choices[index]) {
      return true;
    }
  }
  return false;
}

void updateSecretField(
    JsonDocument& connections,
    const char* jsonKey,
    const String& formField) {
  const String value = settingsServer.arg(formField);
  if (!value.isEmpty()) {
    connections["services"][jsonKey] = value;
  }
}

bool savePortalSettings(int& savedBrightness) {
  JsonDocument config;
  JsonDocument connections;
  String error;
  ScopedSdLock sdLock;
  if (!sdLock) {
    settingsServer.send(503, "text/plain", "The SD card is busy. Try again.");
    return false;
  }
  if (!loadPortalDocuments(config, connections, error)) {
    settingsServer.send(500, "text/plain", error);
    return false;
  }

  int brightness = 0;
  int rotation = 0;
  int flightsRadiusValue = 0;
  int flightsMaximumValue = 0;
  int flightsMinimumAltitudeValue = 0;
  int flightsRefreshValue = 0;
  int systemsRefreshValue = 0;
  int bambuddyPortValue = 0;
  int bambuddyPrinterValue = 0;
  if (!parsePortalInteger("brightness", 5, 100, brightness, error) ||
      !parsePortalInteger("rotation", 1, 3, rotation, error) ||
      !parsePortalInteger(
          "flights_radius",
          10,
          250,
          flightsRadiusValue,
          error) ||
      !parsePortalInteger(
          "flights_maximum",
          1,
          20,
          flightsMaximumValue,
          error) ||
      !parsePortalInteger(
          "flights_minimum_altitude",
          0,
          60000,
          flightsMinimumAltitudeValue,
          error) ||
      !parsePortalInteger(
          "flights_refresh",
          15,
          900,
          flightsRefreshValue,
          error) ||
      !parsePortalInteger(
          "systems_refresh",
          15,
          900,
          systemsRefreshValue,
          error) ||
      !parsePortalInteger(
          "bambuddy_port",
          1,
          65535,
          bambuddyPortValue,
          error) ||
      !parsePortalInteger(
          "bambuddy_printer",
          1,
          65535,
          bambuddyPrinterValue,
          error)) {
    settingsServer.send(400, "text/plain", error);
    return false;
  }
  if (rotation != 1 && rotation != 3) {
    settingsServer.send(400, "text/plain", "Rotation must be 1 or 3.");
    return false;
  }

  const String locationSearch = settingsServer.arg("location_search");
  const String protocol = settingsServer.arg("bambuddy_protocol");
  String host = settingsServer.arg("bambuddy_host");
  String apiPath = settingsServer.arg("bambuddy_path");
  const String ssid = settingsServer.arg("wifi_ssid");
  const String wifiPasswordInput = settingsServer.arg("wifi_password");
  const String portalPasswordInput = settingsServer.arg("portal_password");
  String timezone = settingsServer.arg("posix_timezone");
  host.trim();
  apiPath.trim();
  timezone.trim();

  constexpr const char* timeFormats[] = {"12h", "24h"};
  constexpr const char* dateFormats[] = {
      "yyyy-dd-mm", "yyyy-mm-dd", "dd-mm-yyyy", "mm-dd-yyyy"};
  constexpr const char* temperatureUnits[] = {"celsius", "fahrenheit"};
  constexpr const char* windUnits[] = {"kmh", "mph"};
  constexpr const char* pressureUnits[] = {"hpa", "inhg"};
  constexpr const char* precipitationUnits[] = {"mm", "in"};
  constexpr const char* protocols[] = {"http"};
  constexpr const char* aircraftProviders[] = {
      "airplanes.live", "adsb.lol", "flightradar24"};

  if (locationSearch.length() > 100 || !validPortalHost(host) ||
      apiPath.length() > 64 || !apiPath.startsWith("/") ||
      !validChoice(protocol, protocols, 1) ||
      !validChoice(
          settingsServer.arg("flights_provider"),
          aircraftProviders,
          3) ||
      !validChoice(settingsServer.arg("time_format"), timeFormats, 2) ||
      !validChoice(settingsServer.arg("date_format"), dateFormats, 4) ||
      !validPosixTimezone(timezone) ||
      !validChoice(
          settingsServer.arg("temperature_unit"),
          temperatureUnits,
          2) ||
      !validChoice(settingsServer.arg("wind_unit"), windUnits, 2) ||
      !validChoice(settingsServer.arg("pressure_unit"), pressureUnits, 2) ||
      !validChoice(
          settingsServer.arg("precipitation_unit"),
          precipitationUnits,
          2)) {
    settingsServer.send(400, "text/plain", "One or more settings are invalid.");
    return false;
  }
  if (ssid.length() > 32 || wifiPasswordInput.length() > 64 ||
      portalPasswordInput.length() > 64 ||
      (!portalPasswordInput.isEmpty() && portalPasswordInput.length() < 8)) {
    settingsServer.send(400, "text/plain", "Wi-Fi or portal credentials are invalid.");
    return false;
  }

  const uint8_t previousRotation = displayRotation;
  config["version"] = 1;
  config["display"]["brightness_percent"] = brightness;
  config["display"]["rotation"] = rotation;
  config["location"]["search"] = locationSearch;
  config["locale"]["time_format"] = settingsServer.arg("time_format");
  config["locale"]["date_format"] = settingsServer.arg("date_format");
  config["locale"]["posix_timezone"] = timezone;
  config["weather"]["temperature_unit"] =
      settingsServer.arg("temperature_unit");
  config["weather"]["wind_speed_unit"] = settingsServer.arg("wind_unit");
  config["weather"]["pressure_unit"] = settingsServer.arg("pressure_unit");
  config["weather"]["precipitation_unit"] =
      settingsServer.arg("precipitation_unit");
  config["flights"]["provider"] = settingsServer.arg("flights_provider");
  config["flights"]["radius_km"] = flightsRadiusValue;
  config["flights"]["maximum_aircraft"] = flightsMaximumValue;
  config["flights"]["minimum_altitude_ft"] = flightsMinimumAltitudeValue;
  config["services"]["flights"]["enabled"] = true;
  config["services"]["flights"].remove("proxy_url");
  config["refresh_seconds"]["flights"] = flightsRefreshValue;
  config["refresh_seconds"]["systems"] = systemsRefreshValue;
  config["services"]["bambuddy"]["enabled"] = true;
  config["services"]["bambuddy"]["protocol"] = protocol;
  config["services"]["bambuddy"]["host"] = host;
  config["services"]["bambuddy"]["port"] = bambuddyPortValue;
  config["services"]["bambuddy"]["api_base_path"] = apiPath;
  config["services"]["bambuddy"]["printer_id"] = bambuddyPrinterValue;
  config["services"].remove("bambuddy_url");
  config["services"].remove("bambuddy_printer_id");

  constexpr const char* systemMonitorTypes[] = {"http", "tcp"};
  for (size_t index = 0; index < kMaximumSystemMonitors; ++index) {
    String name =
        settingsServer.arg("systems_name_" + String(index));
    String type =
        settingsServer.arg("systems_type_" + String(index));
    String monitorHost =
        settingsServer.arg("systems_host_" + String(index));
    String monitorPath =
        settingsServer.arg("systems_path_" + String(index));
    name.trim();
    monitorHost.trim();
    monitorPath.trim();
    if (monitorPath.isEmpty()) {
      monitorPath = "/";
    }
    int monitorPort = 0;
    if (name.length() > 24 || monitorHost.length() > 64 ||
        !validPortalHost(monitorHost) || monitorPath.length() > 64 ||
        !monitorPath.startsWith("/") ||
        !validChoice(type, systemMonitorTypes, 2) ||
        !parsePortalInteger(
            "systems_port_" + String(index),
            1,
            65535,
            monitorPort,
            error)) {
      settingsServer.send(
          400,
          "text/plain",
          "One or more Systems monitors are invalid.");
      return false;
    }
    JsonObject monitor =
        config["services"]["systems"]["monitors"][index].to<JsonObject>();
    monitor["enabled"] =
        settingsServer.hasArg("systems_enabled_" + String(index));
    monitor["name"] = name;
    monitor["type"] = type;
    monitor["host"] = monitorHost;
    monitor["port"] = monitorPort;
    monitor["path"] = monitorPath;
  }

  for (int index = 0; index < 5; ++index) {
    config["tile_visibility"][kTileIds[index]] =
        settingsServer.hasArg(String("tile_") + kTileIds[index]);
  }

  connections["version"] = 1;
  connections["wifi"]["ssid"] = ssid;
  if (!wifiPasswordInput.isEmpty()) {
    connections["wifi"]["password"] = wifiPasswordInput;
  }
  if (settingsServer.hasArg("clear_bambuddy_key")) {
    connections["services"]["bambuddy_readonly_api_key"] = "";
  } else {
    updateSecretField(
        connections,
        "bambuddy_readonly_api_key",
        "bambuddy_key");
  }
  if (settingsServer.hasArg("clear_flights_token")) {
    connections["services"]["flights_api_token"] = "";
  } else {
    updateSecretField(connections, "flights_api_token", "flights_token");
  }
  connections["services"].remove("flights_proxy_token");
  connections["services"].remove("systems_readonly_token");
  connections["services"].remove("calendar_ics_url");
  connections["services"].remove("calendar_readonly_token");
  config["services"]["calendar"].remove("url");
  if (!portalPasswordInput.isEmpty()) {
    connections["device"]["settings_portal_password"] = portalPasswordInput;
  }

  if (!replacePortalDocuments(config, connections, error)) {
    Serial.printf("[PORTAL] Save failed: %s\n", error.c_str());
    settingsServer.send(500, "text/plain", error);
    return false;
  }

  Preferences tilePreferences;
  tilePreferences.begin("tiles", false);
  for (int index = 0; index < 5; ++index) {
    tilePreferences.putBool(
        kTileIds[index],
        config["tile_visibility"][kTileIds[index]] | true);
  }
  tilePreferences.end();

  Preferences displayPreferences;
  displayPreferences.begin("display", false);
  displayPreferences.putUChar("rotation", rotation);
  displayPreferences.end();
  if (previousRotation != rotation) {
    Preferences touchPreferences;
    touchPreferences.begin("touch", false);
    touchPreferences.clear();
    touchPreferences.end();
  }

  if (!portalPasswordInput.isEmpty()) {
    portalPassword = portalPasswordInput;
    portalUsesBootstrapPassword = false;
    Preferences portalPreferences;
    portalPreferences.begin("portal", false);
    portalPreferences.clear();
    portalPreferences.end();
  }

  savedBrightness = brightness;
  return true;
}

void handlePortalSave() {
  if (!requirePortalAuthentication()) {
    return;
  }
  if (settingsServer.arg("csrf") != portalCsrfToken) {
    settingsServer.send(403, "text/plain", "Invalid form token.");
    return;
  }

  int savedBrightness = 0;
  if (!savePortalSettings(savedBrightness)) {
    return;
  }

  loadConfiguration();
  clockConfigured = false;
  clockSyncLogged = false;
  setBacklight(savedBrightness);
  Serial.println("[PORTAL] Settings saved");

  settingsServer.sendHeader("Location", "/?saved=1");
  settingsServer.send(303, "text/plain", "Settings saved.");
}

void handlePortalRestart() {
  if (!requirePortalAuthentication()) {
    return;
  }
  if (settingsServer.arg("csrf") != portalCsrfToken) {
    settingsServer.send(403, "text/plain", "Invalid form token.");
    return;
  }

  settingsServer.send(
      200,
      "text/html; charset=utf-8",
      F("<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Restarting dashboard</title></head>"
        "<body style=\"font-family:sans-serif;max-width:38rem;margin:4rem auto;"
        "padding:0 1rem;background:#111827;color:#e5e7eb\">"
        "<h1>Restarting dashboard</h1>"
        "<p>The device is restarting. This page will return to the dashboard "
        "in about 15 seconds.</p>"
        "<script>setTimeout(function(){location.replace('/');},15000);</script>"
        "</body></html>"));
  restartPending = true;
  restartAt = millis() + 750;
}

void handleFileManager() {
  if (!requirePortalAuthentication()) {
    return;
  }
  if (!sdReady) {
    settingsServer.send(503, "text/plain", "The SD card is not available.");
    return;
  }
  ScopedSdLock sdLock;
  if (!sdLock) {
    settingsServer.send(503, "text/plain", "The SD card is busy. Try again.");
    return;
  }

  String path = settingsServer.arg("path");
  if (path.isEmpty()) {
    path = "/";
  }
  if (!validSdPath(path)) {
    settingsServer.send(400, "text/plain", "Invalid SD card path.");
    return;
  }
  while (path.length() > 1 && path.endsWith("/")) {
    path.remove(path.length() - 1);
  }

  File directory = SD.open(path);
  if (!directory || !directory.isDirectory()) {
    if (directory) {
      directory.close();
    }
    settingsServer.send(404, "text/plain", "Directory not found.");
    return;
  }

  String html;
  html.reserve(14000);
  html += F(
      "<!doctype html><html><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>SD Card Files</title><style>"
      ":root{color-scheme:dark;font-family:system-ui,sans-serif}"
      "body{margin:0;background:#0f172a;color:#e2e8f0}"
      "main{max-width:900px;margin:auto;padding:20px}"
      "a{color:#93c5fd}section{background:#1e293b;border:1px solid #334155;"
      "border-radius:12px;padding:16px;margin:14px 0}"
      "table{width:100%;border-collapse:collapse}td,th{padding:9px;"
      "border-bottom:1px solid #334155;text-align:left}"
      "button{padding:7px 11px;border:0;border-radius:7px;background:#2563eb;"
      "color:#fff;font-weight:700}.danger{background:#b91c1c}"
      "input{padding:9px}.inline{display:inline}.muted{color:#94a3b8}"
      "</style></head><body><main><h1>SD card files</h1><p><a href=\"/\">"
      "Settings</a> &middot; Current directory: <code>");
  html += htmlEscape(path);
  html += F("</code></p>");

  if (path != "/") {
    html += F("<p><a href=\"/files?path=");
    html += urlEncodeComponent(parentSdPath(path));
    html += F("\">&larr; Parent directory</a></p>");
  }

  html += F(
      "<section><h2>Upload a file</h2><form method=\"post\" "
      "enctype=\"multipart/form-data\" action=\"/files/upload?path=");
  html += urlEncodeComponent(path);
  html += F("&csrf=");
  html += urlEncodeComponent(portalCsrfToken);
  html += F(
      "\"><input type=\"file\" name=\"file\" required> "
      "<button type=\"submit\">Upload</button></form>"
      "<p class=\"muted\">Maximum file size: 8 MB. Existing files are not "
      "overwritten; delete them first if replacement is intentional.</p>"
      "<h2>Create a directory</h2><form method=\"post\" action=\"/files/mkdir\">"
      "<input type=\"hidden\" name=\"csrf\" value=\"");
  html += portalCsrfToken;
  html += F("\"><input type=\"hidden\" name=\"path\" value=\"");
  html += htmlEscape(path);
  html += F("\"><input name=\"name\" maxlength=\"96\" required "
            "placeholder=\"Directory name\"> "
            "<button type=\"submit\">Create</button></form></section>"
      "<section><h2>Contents</h2><table><thead><tr><th>Name</th><th>Size</th>"
      "<th>Action</th></tr></thead><tbody>");

  constexpr size_t kMaximumDisplayedEntries = 40;
  bool hasEntries = false;
  bool listingTruncated = false;
  size_t entryCount = 0;
  File entry = directory.openNextFile();
  while (entry) {
    if (entryCount >= kMaximumDisplayedEntries) {
      listingTruncated = true;
      entry.close();
      break;
    }
    ++entryCount;
    hasEntries = true;
    String entryPath = entry.name();
    if (!entryPath.startsWith("/")) {
      entryPath = joinSdPath(path, entryPath);
    }
    String displayName = entryPath;
    const int separator = displayName.lastIndexOf('/');
    if (separator >= 0) {
      displayName = displayName.substring(separator + 1);
    }

    html += F("<tr><td>");
    if (entry.isDirectory()) {
      html += F("<a href=\"/files?path=");
      html += urlEncodeComponent(entryPath);
      html += F("\">");
      html += htmlEscape(displayName);
      html += F("/</a>");
    } else {
      html += F("<a href=\"/files/download?path=");
      html += urlEncodeComponent(entryPath);
      html += F("\">");
      html += htmlEscape(displayName);
      html += F("</a>");
    }
    html += F("</td><td>");
    html += entry.isDirectory() ? "-" : String(entry.size());
    html += F("</td><td><form class=\"inline\" method=\"post\" action=\"/files/delete\" "
              "onsubmit=\"return confirm('Delete this item?')\">"
              "<input type=\"hidden\" name=\"csrf\" value=\"");
    html += portalCsrfToken;
    html += F("\"><input type=\"hidden\" name=\"path\" value=\"");
    html += htmlEscape(entryPath);
    html += F("\"><input type=\"hidden\" name=\"return_path\" value=\"");
    html += htmlEscape(path);
    html += F("\"><button class=\"danger\" type=\"submit\">Delete</button>"
              "</form></td></tr>");
    entry.close();
    entry = directory.openNextFile();
  }
  directory.close();

  if (!hasEntries) {
    html += F("<tr><td colspan=\"3\" class=\"muted\">This directory is empty.</td></tr>");
  } else if (listingTruncated) {
    html += F(
        "<tr><td colspan=\"3\" class=\"muted\">Only the first 40 entries are "
        "shown. Open a subdirectory to narrow the listing.</td></tr>");
  }
  html += F("</tbody></table></section><p class=\"muted\">Deleting "
            "config.json or connections.json can prevent services from loading "
            "after a restart.</p></main></body></html>");
  settingsServer.send(200, "text/html; charset=utf-8", html);
}

void handleFileUploadData() {
  HTTPUpload& upload = settingsServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    portalUploadAccepted = false;
    portalUploadError = "";
    portalUploadTarget = "";
    if (portalUploadFile) {
      portalUploadFile.close();
    }
    if (portalUploadHasStorageLock && sdMutex != nullptr) {
      xSemaphoreGive(sdMutex);
      portalUploadHasStorageLock = false;
    }

    if (!settingsServer.authenticate("admin", portalPassword.c_str()) ||
        settingsServer.arg("csrf") != portalCsrfToken) {
      portalUploadError = "Authentication or form token failed.";
      return;
    }
    if (sdMutex != nullptr &&
        xSemaphoreTake(sdMutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
      portalUploadError = "The SD card is busy. Try again.";
      return;
    }
    portalUploadHasStorageLock = sdMutex != nullptr;
    String directory = settingsServer.arg("path");
    while (directory.length() > 1 && directory.endsWith("/")) {
      directory.remove(directory.length() - 1);
    }
    if (!validSdPath(directory) || !validUploadFilename(upload.filename)) {
      portalUploadError = "Invalid upload path or filename.";
      if (portalUploadHasStorageLock) {
        xSemaphoreGive(sdMutex);
        portalUploadHasStorageLock = false;
      }
      return;
    }
    File directoryFile = SD.open(directory);
    const bool directoryExists =
        directoryFile && directoryFile.isDirectory();
    if (directoryFile) {
      directoryFile.close();
    }
    if (!directoryExists) {
      portalUploadError = "Upload directory does not exist.";
      if (portalUploadHasStorageLock) {
        xSemaphoreGive(sdMutex);
        portalUploadHasStorageLock = false;
      }
      return;
    }

    portalUploadTarget = joinSdPath(directory, upload.filename);
    if (SD.exists(portalUploadTarget)) {
      portalUploadError = "A file with that name already exists.";
      if (portalUploadHasStorageLock) {
        xSemaphoreGive(sdMutex);
        portalUploadHasStorageLock = false;
      }
      return;
    }
    portalUploadFile = SD.open(portalUploadTarget, FILE_WRITE);
    if (!portalUploadFile) {
      portalUploadError = "Could not create the uploaded file.";
      if (portalUploadHasStorageLock) {
        xSemaphoreGive(sdMutex);
        portalUploadHasStorageLock = false;
      }
      return;
    }
    portalUploadAccepted = true;
  } else if (upload.status == UPLOAD_FILE_WRITE && portalUploadAccepted) {
    constexpr size_t maximumUploadBytes = 8U * 1024U * 1024U;
    if (upload.totalSize + upload.currentSize > maximumUploadBytes ||
        portalUploadFile.write(upload.buf, upload.currentSize) !=
            upload.currentSize) {
      portalUploadError = "Upload exceeded 8 MB or the SD write failed.";
      portalUploadAccepted = false;
      portalUploadFile.close();
      SD.remove(portalUploadTarget);
      if (portalUploadHasStorageLock) {
        xSemaphoreGive(sdMutex);
        portalUploadHasStorageLock = false;
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (portalUploadFile) {
      portalUploadFile.close();
    }
    if (portalUploadHasStorageLock) {
      xSemaphoreGive(sdMutex);
      portalUploadHasStorageLock = false;
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (portalUploadFile) {
      portalUploadFile.close();
    }
    if (!portalUploadTarget.isEmpty()) {
      SD.remove(portalUploadTarget);
    }
    portalUploadAccepted = false;
    portalUploadError = "Upload was aborted.";
    if (portalUploadHasStorageLock) {
      xSemaphoreGive(sdMutex);
      portalUploadHasStorageLock = false;
    }
  }
}

void handleFileUploadComplete() {
  if (!requirePortalAuthentication()) {
    return;
  }
  if (settingsServer.arg("csrf") != portalCsrfToken) {
    settingsServer.send(403, "text/plain", "Invalid form token.");
    return;
  }
  if (!portalUploadAccepted || !portalUploadError.isEmpty()) {
    settingsServer.send(
        400,
        "text/plain",
        portalUploadError.isEmpty() ? "Upload failed." : portalUploadError);
    return;
  }
  String directory = settingsServer.arg("path");
  if (!validSdPath(directory)) {
    directory = "/";
  }
  settingsServer.sendHeader(
      "Location",
      "/files?path=" + urlEncodeComponent(directory));
  settingsServer.send(303, "text/plain", "Uploaded.");
}

bool installStartupImage(String& error) {
  constexpr const char* stagePath = "/dashboard/startup.portal-new.jpg";
  constexpr const char* livePath = "/dashboard/startup.jpg";
  constexpr const char* backupPath = "/dashboard/startup.portal-backup.jpg";
  File image = SD.open(stagePath, FILE_READ);
  uint16_t width = 0;
  uint16_t height = 0;
  const bool valid =
      image && image.size() <= 200U * 1024U &&
      jpegDimensions(image, width, height) &&
      width == kScreenWidth && height == kScreenHeight;
  image.close();
  if (!valid) {
    SD.remove(stagePath);
    error =
        "Startup artwork must be a 320x240 baseline JPEG no larger than 200 KB.";
    return false;
  }

  SD.remove(backupPath);
  const bool hadLiveImage = SD.exists(livePath);
  if (hadLiveImage && !SD.rename(livePath, backupPath)) {
    SD.remove(stagePath);
    error = "Could not preserve the current startup image.";
    return false;
  }
  if (!SD.rename(stagePath, livePath)) {
    if (hadLiveImage) {
      SD.rename(backupPath, livePath);
    }
    error = "Could not install the startup image.";
    return false;
  }
  return true;
}

void handleStartupUploadData() {
  constexpr const char* stagePath = "/dashboard/startup.portal-new.jpg";
  HTTPUpload& upload = settingsServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    portalUploadAccepted = false;
    portalUploadError = "";
    portalUploadTarget = stagePath;
    if (portalUploadFile) {
      portalUploadFile.close();
    }
    if (portalUploadHasStorageLock && sdMutex != nullptr) {
      xSemaphoreGive(sdMutex);
      portalUploadHasStorageLock = false;
    }
    if (!settingsServer.authenticate("admin", portalPassword.c_str()) ||
        settingsServer.arg("csrf") != portalCsrfToken) {
      portalUploadError = "Authentication or form token failed.";
      return;
    }
    if (sdMutex != nullptr &&
        xSemaphoreTake(sdMutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
      portalUploadError = "The SD card is busy. Try again.";
      return;
    }
    portalUploadHasStorageLock = sdMutex != nullptr;
    SD.remove(stagePath);
    portalUploadFile = SD.open(stagePath, FILE_WRITE);
    if (!portalUploadFile) {
      portalUploadError = "Could not create the staged startup image.";
      if (portalUploadHasStorageLock) {
        xSemaphoreGive(sdMutex);
        portalUploadHasStorageLock = false;
      }
      return;
    }
    portalUploadAccepted = true;
  } else if (upload.status == UPLOAD_FILE_WRITE && portalUploadAccepted) {
    constexpr size_t maximumBytes = 200U * 1024U;
    if (upload.totalSize + upload.currentSize > maximumBytes ||
        portalUploadFile.write(upload.buf, upload.currentSize) !=
            upload.currentSize) {
      portalUploadError = "The image exceeded 200 KB or the SD write failed.";
      portalUploadAccepted = false;
      portalUploadFile.close();
      SD.remove(stagePath);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (portalUploadFile) {
      portalUploadFile.close();
    }
    if (portalUploadAccepted &&
        !installStartupImage(portalUploadError)) {
      portalUploadAccepted = false;
    }
    if (portalUploadHasStorageLock) {
      xSemaphoreGive(sdMutex);
      portalUploadHasStorageLock = false;
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (portalUploadFile) {
      portalUploadFile.close();
    }
    SD.remove(stagePath);
    portalUploadAccepted = false;
    portalUploadError = "Upload was aborted.";
    if (portalUploadHasStorageLock) {
      xSemaphoreGive(sdMutex);
      portalUploadHasStorageLock = false;
    }
  }
}

void handleStartupUploadComplete() {
  if (!requirePortalAuthentication()) {
    return;
  }
  if (settingsServer.arg("csrf") != portalCsrfToken) {
    settingsServer.send(403, "text/plain", "Invalid form token.");
    return;
  }
  if (!portalUploadAccepted || !portalUploadError.isEmpty()) {
    settingsServer.send(
        400,
        "text/plain",
        portalUploadError.isEmpty()
            ? "Startup image upload failed."
            : portalUploadError);
    return;
  }
  settingsServer.sendHeader("Location", "/?startup=1");
  settingsServer.send(303, "text/plain", "Startup image updated.");
}

void handleFileDelete() {
  if (!requirePortalAuthentication()) {
    return;
  }
  if (settingsServer.arg("csrf") != portalCsrfToken) {
    settingsServer.send(403, "text/plain", "Invalid form token.");
    return;
  }
  ScopedSdLock sdLock;
  if (!sdLock) {
    settingsServer.send(503, "text/plain", "The SD card is busy. Try again.");
    return;
  }
  String path = settingsServer.arg("path");
  String returnPath = settingsServer.arg("return_path");
  if (!validSdPath(path, false) || !validSdPath(returnPath)) {
    settingsServer.send(400, "text/plain", "Invalid SD card path.");
    return;
  }

  File item = SD.open(path);
  if (!item) {
    settingsServer.send(404, "text/plain", "File or directory not found.");
    return;
  }
  const bool isDirectory = item.isDirectory();
  item.close();
  const bool removed =
      isDirectory ? SD.rmdir(path.c_str()) : SD.remove(path.c_str());
  if (!removed) {
    settingsServer.send(
        409,
        "text/plain",
        isDirectory ? "Directory must be empty before deletion."
                    : "Could not delete the file.");
    return;
  }
  settingsServer.sendHeader(
      "Location",
      "/files?path=" + urlEncodeComponent(returnPath));
  settingsServer.send(303, "text/plain", "Deleted.");
}

void handleDirectoryCreate() {
  if (!requirePortalAuthentication()) {
    return;
  }
  if (settingsServer.arg("csrf") != portalCsrfToken) {
    settingsServer.send(403, "text/plain", "Invalid form token.");
    return;
  }
  ScopedSdLock sdLock;
  if (!sdLock) {
    settingsServer.send(503, "text/plain", "The SD card is busy. Try again.");
    return;
  }
  String parent = settingsServer.arg("path");
  String name = settingsServer.arg("name");
  name.trim();
  if (!validSdPath(parent) || !validUploadFilename(name)) {
    settingsServer.send(400, "text/plain", "Invalid directory path or name.");
    return;
  }
  while (parent.length() > 1 && parent.endsWith("/")) {
    parent.remove(parent.length() - 1);
  }
  const String path = joinSdPath(parent, name);
  if (SD.exists(path)) {
    settingsServer.send(409, "text/plain", "That item already exists.");
    return;
  }
  if (!SD.mkdir(path.c_str())) {
    settingsServer.send(500, "text/plain", "Could not create the directory.");
    return;
  }
  settingsServer.sendHeader(
      "Location",
      "/files?path=" + urlEncodeComponent(parent));
  settingsServer.send(303, "text/plain", "Directory created.");
}

void handleFileDownload() {
  if (!requirePortalAuthentication()) {
    return;
  }
  ScopedSdLock sdLock;
  if (!sdLock) {
    settingsServer.send(503, "text/plain", "The SD card is busy. Try again.");
    return;
  }
  const String path = settingsServer.arg("path");
  if (!validSdPath(path, false)) {
    settingsServer.send(400, "text/plain", "Invalid SD card path.");
    return;
  }
  File file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) {
      file.close();
    }
    settingsServer.send(404, "text/plain", "File not found.");
    return;
  }
  String filename = path.substring(path.lastIndexOf('/') + 1);
  filename.replace("\"", "");
  settingsServer.sendHeader(
      "Content-Disposition",
      "attachment; filename=\"" + filename + "\"");
  settingsServer.streamFile(file, "application/octet-stream");
  file.close();
}

void configurePortalRoutes() {
  if (portalRoutesConfigured) {
    return;
  }

  settingsServer.on("/", HTTP_GET, []() {
    if (setupMode) {
      handleSetupRoot();
    } else {
      handlePortalRoot();
    }
  });
  settingsServer.on("/setup/save", HTTP_POST, handleSetupSave);
  settingsServer.on("/save", HTTP_POST, handlePortalSave);
  settingsServer.on("/restart", HTTP_POST, handlePortalRestart);
  settingsServer.on("/files", HTTP_GET, handleFileManager);
  settingsServer.on(
      "/files/upload",
      HTTP_POST,
      handleFileUploadComplete,
      handleFileUploadData);
  settingsServer.on(
      "/startup/upload",
      HTTP_POST,
      handleStartupUploadComplete,
      handleStartupUploadData);
  settingsServer.on("/files/delete", HTTP_POST, handleFileDelete);
  settingsServer.on("/files/mkdir", HTTP_POST, handleDirectoryCreate);
  settingsServer.on("/files/download", HTTP_GET, handleFileDownload);
  settingsServer.on("/health", HTTP_GET, []() {
    settingsServer.send(
        200,
        "application/json",
        String("{\"status\":\"ok\",\"wifi\":") +
            (WiFi.status() == WL_CONNECTED ? "true" : "false") +
            ",\"sd\":" + (sdReady ? "true" : "false") + "}");
  });
  settingsServer.onNotFound([]() {
    if (setupMode) {
      settingsServer.sendHeader(
          "Location",
          "http://" + WiFi.softAPIP().toString() + "/");
      settingsServer.send(302, "text/plain", "Open the setup page.");
    } else {
      settingsServer.send(404, "text/plain", "Not found");
    }
  });
  portalRoutesConfigured = true;
}

void startSetupAccessPoint() {
  if (setupAccessPointStarted) {
    return;
  }
  WiFi.mode(wifiStarted ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAPConfig(
      IPAddress(192, 168, 4, 1),
      IPAddress(192, 168, 4, 1),
      IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(
          kSetupAccessPointSsid,
          kSetupAccessPointPassword,
          1,
          0,
          2)) {
    Serial.println("[SETUP] Could not start setup access point");
    return;
  }
  setupMode = true;
  setupAccessPointStarted = true;
  setupDnsServer.start(53, "*", WiFi.softAPIP());
  configurePortalRoutes();
  if (!portalStarted) {
    settingsServer.begin();
    portalStarted = true;
  }
  Serial.printf(
      "[SETUP] Join %s and open http://%s/\n",
      kSetupAccessPointSsid,
      WiFi.softAPIP().toString().c_str());
}

void stopSetupAccessPoint() {
  if (!setupAccessPointStarted) {
    return;
  }
  setupDnsServer.stop();
  WiFi.softAPdisconnect(false);
  setupAccessPointStarted = false;
  setupMode = false;
  Serial.println("[SETUP] Setup access point stopped");
}

void startClockWhenReady() {
  if (clockConfigured || WiFi.status() != WL_CONNECTED) {
    return;
  }
  configTzTime(
      posixTimezone.c_str(),
      "pool.ntp.org",
      "time.google.com",
      "time.cloudflare.com");
  clockConfigured = true;
  Serial.printf("[TIME] SNTP started with timezone %s\n", posixTimezone.c_str());
}

void startPortalWhenReady() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  stopSetupAccessPoint();
  configurePortalRoutes();
  if (!portalStarted) {
    settingsServer.begin();
    portalStarted = true;
  }
  if (!mdnsStarted && MDNS.begin("desk-dashboard")) {
    mdnsStarted = true;
    Serial.printf(
        "[PORTAL] http://%s/ or http://desk-dashboard.local/\n",
        WiFi.localIP().toString().c_str());
  }
}

void styleScreen(lv_obj_t* screen) {
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x111827), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lv_color_hex(0xF8FAFC), 0);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
}

void updateStatusBar() {
  if (wifiStatusLabel == nullptr || sdStatusLabel == nullptr ||
      clockStatusLabel == nullptr) {
    return;
  }

  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  lv_obj_set_style_bg_color(
      wifiStatusLabel,
      lv_color_hex(wifiConnected ? 0x166534 : 0x991B1B),
      0);
  lv_obj_set_style_bg_color(
      sdStatusLabel,
      lv_color_hex(sdReady ? 0x166534 : 0x991B1B),
      0);

  uint8_t signalBars = 0;
  if (wifiConnected) {
    const int32_t rssi = WiFi.RSSI();
    signalBars =
        rssi >= -55 ? 4 :
        rssi >= -65 ? 3 :
        rssi >= -75 ? 2 : 1;
  }
  for (size_t index = 0; index < 4; ++index) {
    if (wifiSignalBars[index] != nullptr) {
      lv_obj_set_style_bg_color(
          wifiSignalBars[index],
          lv_color_hex(
              index < signalBars
                  ? 0x22C55E
                  : (wifiConnected ? 0x334155 : 0x7F1D1D)),
          0);
    }
  }

  char clockText[6] = "--:--";
  const time_t now = time(nullptr);
  if (now > 1700000000) {
    struct tm localTime = {};
    localtime_r(&now, &localTime);
    if (timeFormat == "12h") {
      strftime(clockText, sizeof(clockText), "%I:%M", &localTime);
    } else {
      strftime(clockText, sizeof(clockText), "%H:%M", &localTime);
    }
    if (!clockSyncLogged) {
      Serial.printf("[TIME] Clock synchronized at %s\n", clockText);
      clockSyncLogged = true;
    }
  }
  lv_label_set_text(clockStatusLabel, clockText);
}

void addStatusBar(lv_obj_t* parent) {
  lv_obj_t* bar = lv_obj_create(parent);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, kScreenWidth, 28);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x0B1220), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

  lv_obj_t* title = lv_label_create(bar);
  lv_label_set_text(title, currentPage.c_str());
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_set_width(title, 174);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);

  wifiStatusLabel = lv_label_create(bar);
  lv_label_set_text(wifiStatusLabel, "WiFi");
  lv_obj_set_style_text_font(wifiStatusLabel, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(wifiStatusLabel, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(wifiStatusLabel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(wifiStatusLabel, 4, 0);
  lv_obj_set_style_pad_hor(wifiStatusLabel, 4, 0);
  lv_obj_set_style_pad_ver(wifiStatusLabel, 2, 0);
  lv_obj_align(wifiStatusLabel, LV_ALIGN_RIGHT_MID, -94, 0);

  for (size_t index = 0; index < 4; ++index) {
    const int height = 3 + static_cast<int>(index) * 2;
    wifiSignalBars[index] = lv_obj_create(bar);
    lv_obj_remove_style_all(wifiSignalBars[index]);
    lv_obj_set_size(wifiSignalBars[index], 2, height);
    lv_obj_set_pos(
        wifiSignalBars[index],
        230 + static_cast<int>(index) * 3,
        19 - height);
    lv_obj_set_style_bg_opa(wifiSignalBars[index], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(wifiSignalBars[index], 1, 0);
  }

  sdStatusLabel = lv_label_create(bar);
  lv_label_set_text(sdStatusLabel, "SD");
  lv_obj_set_style_text_font(sdStatusLabel, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(sdStatusLabel, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(sdStatusLabel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(sdStatusLabel, 4, 0);
  lv_obj_set_style_pad_hor(sdStatusLabel, 4, 0);
  lv_obj_set_style_pad_ver(sdStatusLabel, 2, 0);
  lv_obj_align(sdStatusLabel, LV_ALIGN_RIGHT_MID, -49, 0);

  clockStatusLabel = lv_label_create(bar);
  lv_obj_set_width(clockStatusLabel, 42);
  lv_obj_set_style_text_font(clockStatusLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(clockStatusLabel, lv_color_hex(0xE2E8F0), 0);
  lv_obj_set_style_text_align(clockStatusLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(clockStatusLabel, LV_ALIGN_RIGHT_MID, -5, 0);
  updateStatusBar();
}

void showHome();
void showSettings();
void showDisplaySettings();
void showTileSettings();
void showServicesSettings();
void showSystemSettings();
void showWeather();
void showFlights();
void showBambuddy();
void showSystems();
void showStaticCalendar();

void backEvent(lv_event_t* event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    pendingNavigation = PendingNavigation::home;
  }
}

void settingsBackEvent(lv_event_t* event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    pendingNavigation = PendingNavigation::settings;
  }
}

void addBackButton(lv_obj_t* parent) {
  lv_obj_t* button = lv_btn_create(parent);
  lv_obj_set_size(button, 68, 34);
  lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, 8, -8);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x334155), 0);
  lv_obj_add_event_cb(button, backEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, "Home");
  lv_obj_center(label);
}

void addSettingsBackButton(lv_obj_t* parent) {
  lv_obj_t* button = lv_btn_create(parent);
  lv_obj_set_size(button, 68, 34);
  lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, 8, -8);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x334155), 0);
  lv_obj_add_event_cb(button, settingsBackEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, "Settings");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
  lv_obj_center(label);
}

void brightnessEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  const intptr_t change = reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
  setBacklight(static_cast<uint8_t>(constrain(static_cast<int>(brightnessPercent) + change, 5, 100)));
}

void recalibrateEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  Preferences preferences;
  preferences.begin("touch", false);
  preferences.clear();
  preferences.end();
  delay(100);
  ESP.restart();
}

void rotateDisplayEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  displayRotation = displayRotation == 1 ? 3 : 1;

  Preferences displayPreferences;
  displayPreferences.begin("display", false);
  displayPreferences.putUChar("rotation", displayRotation);
  displayPreferences.end();

  Preferences touchPreferences;
  touchPreferences.begin("touch", false);
  touchPreferences.clear();
  touchPreferences.end();

  delay(100);
  ESP.restart();
}

void showDisplaySettings() {
  currentPage = "Display";
  brightnessLabel = nullptr;
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  lv_obj_t* summary = lv_label_create(screen);
  lv_label_set_text_fmt(
      summary,
      "Display: 320x240  |  Touch: calibrated\nSD: %s  |  Config: %s\nWi-Fi: %s\nFree heap: %u bytes",
      sdReady ? "mounted" : "missing",
      configReady ? "loaded" : "not found",
      wifiSummary().c_str(),
      ESP.getFreeHeap());
  lv_obj_set_style_text_font(summary, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(summary, 12, 40);

  lv_obj_t* minusButton = lv_btn_create(screen);
  lv_obj_set_size(minusButton, 48, 36);
  lv_obj_set_pos(minusButton, 155, 139);
  lv_obj_add_event_cb(minusButton, brightnessEvent, LV_EVENT_CLICKED, reinterpret_cast<void*>(-10));
  lv_obj_t* minusLabel = lv_label_create(minusButton);
  lv_label_set_text(minusLabel, "-");
  lv_obj_center(minusLabel);

  brightnessLabel = lv_label_create(screen);
  lv_label_set_text_fmt(brightnessLabel, "%u%%", brightnessPercent);
  lv_obj_set_style_text_font(brightnessLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_pos(brightnessLabel, 213, 147);

  lv_obj_t* plusButton = lv_btn_create(screen);
  lv_obj_set_size(plusButton, 48, 36);
  lv_obj_set_pos(plusButton, 262, 139);
  lv_obj_add_event_cb(plusButton, brightnessEvent, LV_EVENT_CLICKED, reinterpret_cast<void*>(10));
  lv_obj_t* plusLabel = lv_label_create(plusButton);
  lv_label_set_text(plusLabel, "+");
  lv_obj_center(plusLabel);

  lv_obj_t* brightnessTitle = lv_label_create(screen);
  lv_label_set_text(brightnessTitle, "Brightness");
  lv_obj_set_pos(brightnessTitle, 155, 117);

  lv_obj_t* recalibrateButton = lv_btn_create(screen);
  lv_obj_set_size(recalibrateButton, 98, 34);
  lv_obj_set_pos(recalibrateButton, 82, 198);
  lv_obj_set_style_bg_color(recalibrateButton, lv_color_hex(0x334155), 0);
  lv_obj_add_event_cb(recalibrateButton, recalibrateEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* recalibrateLabel = lv_label_create(recalibrateButton);
  lv_label_set_text(recalibrateLabel, "Calibrate");
  lv_obj_center(recalibrateLabel);

  lv_obj_t* rotateButton = lv_btn_create(screen);
  lv_obj_set_size(rotateButton, 124, 34);
  lv_obj_set_pos(rotateButton, 188, 198);
  lv_obj_set_style_bg_color(rotateButton, lv_color_hex(0x334155), 0);
  lv_obj_add_event_cb(rotateButton, rotateDisplayEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* rotateLabel = lv_label_create(rotateButton);
  lv_label_set_text(rotateLabel, "Rotate 180");
  lv_obj_center(rotateLabel);

  addSettingsBackButton(screen);
}

void showSettingsPlaceholder(const char* title) {
  currentPage = title;
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  lv_obj_t* heading = lv_label_create(screen);
  lv_label_set_text(heading, title);
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_20, 0);
  lv_obj_align(heading, LV_ALIGN_CENTER, 0, -25);

  lv_obj_t* message = lv_label_create(screen);
  lv_label_set_text(message, "Settings controls will be added\nwith the matching data page.");
  lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(message, lv_color_hex(0xCBD5E1), 0);
  lv_obj_align(message, LV_ALIGN_CENTER, 0, 20);

  addSettingsBackButton(screen);
}

void showServicesSettings() {
  currentPage = "Services";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "Bambuddy");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_pos(title, 12, 40);

  lv_obj_t* details = lv_label_create(screen);
  lv_label_set_text_fmt(
      details,
      "Host: %s\nPort: %u  |  Protocol: %s\nAPI path: %s\nPrinter ID: %u\nRead-only key: %s",
      bambuddyHost.isEmpty() ? "not configured" : bambuddyHost.c_str(),
      bambuddyPort,
      bambuddyProtocol.c_str(),
      bambuddyApiBasePath.c_str(),
      bambuddyPrinterId,
      bambuddyKeyPresent ? "present" : "missing");
  lv_obj_set_style_text_font(details, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(details, 12, 68);

  lv_obj_t* note = lv_label_create(screen);
  lv_label_set_text_fmt(
      note,
      "Edit at http://%s/",
      WiFi.status() == WL_CONNECTED
          ? WiFi.localIP().toString().c_str()
          : "device-ip");
  lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(note, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_pos(note, 12, 170);

  addSettingsBackButton(screen);
}

void showSystemSettings() {
  currentPage = "System";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  lv_obj_t* details = lv_label_create(screen);
  if (WiFi.status() == WL_CONNECTED) {
    const int32_t wifiChannel = WiFi.channel();
    const int32_t wifiSignal = WiFi.RSSI();
    lv_label_set_text_fmt(
        details,
        "Settings portal\nhttp://%s/\ndesk-dashboard.local\n"
        "WiFi: channel %ld | %ld dBm\n"
        "Username: admin\nPassword: %s\nHeap: %u bytes",
        WiFi.localIP().toString().c_str(),
        static_cast<long>(wifiChannel),
        static_cast<long>(wifiSignal),
        portalUsesBootstrapPassword
            ? portalPassword.c_str()
            : "configured in connections.json",
        ESP.getFreeHeap());
  } else {
    lv_label_set_text(
        details,
        "Settings portal unavailable\nWi-Fi is not connected.");
  }
  lv_obj_set_style_text_font(details, &lv_font_montserrat_12, 0);
  lv_obj_set_pos(details, 12, 40);

  addSettingsBackButton(screen);
}

void settingsCategoryEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  const char* category = static_cast<const char*>(lv_event_get_user_data(event));
  if (strcmp(category, "Display") == 0) {
    showDisplaySettings();
  } else if (strcmp(category, "Home Tiles") == 0) {
    showTileSettings();
  } else if (strcmp(category, "Services") == 0) {
    showServicesSettings();
  } else if (strcmp(category, "System") == 0) {
    showSystemSettings();
  } else {
    showSettingsPlaceholder(category);
  }
}

void tileVisibilityEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  const int index = reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
  if (index < 0 || index >= 5) {
    return;
  }

  tileEnabled[index] = !tileEnabled[index];
  Preferences preferences;
  preferences.begin("tiles", false);
  preferences.putBool(kTileIds[index], tileEnabled[index]);
  preferences.end();
  showTileSettings();
}

void showTileSettings() {
  currentPage = "Home Tiles";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  constexpr int xPositions[] = {6, 111, 216};
  constexpr int yPositions[] = {38, 111};
  for (int index = 0; index < 5; ++index) {
    lv_obj_t* tile = lv_btn_create(screen);
    lv_obj_set_size(tile, 98, 64);
    lv_obj_set_pos(tile, xPositions[index % 3], yPositions[index / 3]);
    lv_obj_set_style_radius(tile, 8, 0);
    lv_obj_set_style_bg_color(
        tile,
        lv_color_hex(tileEnabled[index] ? kTileColors[index] : 0x374151),
        0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(tile, 0, 0);
    lv_obj_add_event_cb(
        tile,
        tileVisibilityEvent,
        LV_EVENT_CLICKED,
        reinterpret_cast<void*>(index));

    lv_obj_t* name = lv_label_create(tile);
    lv_label_set_text(name, kTileNames[index]);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
    lv_obj_align(name, LV_ALIGN_CENTER, 0, -9);

    lv_obj_t* state = lv_label_create(tile);
    lv_label_set_text(state, tileEnabled[index] ? "ON" : "OFF");
    lv_obj_set_style_text_font(state, &lv_font_montserrat_14, 0);
    lv_obj_align(state, LV_ALIGN_CENTER, 0, 12);
  }

  lv_obj_t* note = lv_label_create(screen);
  lv_label_set_text(note, "Settings always remains on the Home screen.");
  lv_obj_set_width(note, 230);
  lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(note, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_pos(note, 82, 181);

  addSettingsBackButton(screen);
}

void showSettings() {
  currentPage = "Settings";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  constexpr int xPositions[] = {6, 111, 216};
  constexpr int yPositions[] = {34, 115};
  for (int index = 0; index < 6; ++index) {
    lv_obj_t* tile = lv_btn_create(screen);
    lv_obj_set_size(tile, 98, 75);
    lv_obj_set_pos(tile, xPositions[index % 3], yPositions[index / 3]);
    lv_obj_set_style_radius(tile, 9, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(kTileColors[index]), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(tile, 0, 0);
    lv_obj_add_event_cb(
        tile,
        settingsCategoryEvent,
        LV_EVENT_CLICKED,
        const_cast<char*>(kSettingsNames[index]));

    lv_obj_t* label = lv_label_create(tile);
    lv_label_set_text(label, kSettingsNames[index]);
    lv_obj_set_width(label, 88);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_center(label);
  }

  lv_obj_t* homeButton = lv_btn_create(screen);
  lv_obj_set_size(homeButton, 76, 28);
  lv_obj_align(homeButton, LV_ALIGN_BOTTOM_MID, 0, -3);
  lv_obj_set_style_bg_color(homeButton, lv_color_hex(0x334155), 0);
  lv_obj_add_event_cb(homeButton, backEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* homeLabel = lv_label_create(homeButton);
  lv_label_set_text(homeLabel, "Home");
  lv_obj_set_style_text_font(homeLabel, &lv_font_montserrat_12, 0);
  lv_obj_center(homeLabel);
}

const char* weatherConditionIcon(const WeatherData& data) {
  if (strcmp(data.condition, "Clear") == 0) {
    return data.isDay ? "S:/dashboard/icons/Sun.bin"
                      : "S:/dashboard/icons/Moon.bin";
  }
  if (strcmp(data.condition, "Partly cloudy") == 0) {
    return data.isDay ? "S:/dashboard/icons/Weather.bin"
                      : "S:/dashboard/icons/CloudMoon.bin";
  }
  if (strcmp(data.condition, "Overcast") == 0) {
    return "S:/dashboard/icons/Cloud.bin";
  }
  if (strcmp(data.condition, "Fog") == 0) {
    return "S:/dashboard/icons/Fog.bin";
  }
  if (strstr(data.condition, "Snow") != nullptr) {
    return "S:/dashboard/icons/Snow.bin";
  }
  if (strcmp(data.condition, "Thunderstorm") == 0) {
    return "S:/dashboard/icons/Thunderstorm.bin";
  }
  if (strstr(data.condition, "Rain") != nullptr ||
      strcmp(data.condition, "Drizzle") == 0) {
    return "S:/dashboard/icons/Rain.bin";
  }
  return "S:/dashboard/icons/Weather.bin";
}

lv_color_t weatherConditionColour(const WeatherData& data) {
  if (strcmp(data.condition, "Clear") == 0) {
    return lv_color_hex(data.isDay ? 0xFBBF24 : 0xC4B5FD);
  }
  if (strcmp(data.condition, "Thunderstorm") == 0) {
    return lv_color_hex(0xC084FC);
  }
  if (strstr(data.condition, "Snow") != nullptr) {
    return lv_color_hex(0xE0F2FE);
  }
  if (strstr(data.condition, "Rain") != nullptr ||
      strcmp(data.condition, "Drizzle") == 0) {
    return lv_color_hex(0x60A5FA);
  }
  if (strcmp(data.condition, "Fog") == 0) {
    return lv_color_hex(0x94A3B8);
  }
  return lv_color_hex(0xCBD5E1);
}

void updateWeatherIcon(const WeatherData& data) {
  if (weatherIconImage == nullptr) {
    return;
  }
  const char* path = weatherConditionIcon(data);
  if (path != renderedWeatherIconPath) {
    if (!sdIconAvailable(path)) {
      path = kTileIconPaths[0];
    }
    lv_img_set_src(weatherIconImage, path);
    renderedWeatherIconPath = path;
  }
  lv_obj_set_style_img_recolor(
      weatherIconImage,
      weatherConditionColour(data),
      0);
}

void renderWeatherPage() {
  if (currentPage != "Weather" || weatherTemperatureLabel == nullptr) {
    return;
  }

  const WeatherData data = liveDataWeatherSnapshot();
  if (data.state == LiveDataState::idle ||
      data.state == LiveDataState::loading) {
    lv_label_set_text(weatherTemperatureLabel, "--");
    lv_label_set_text(weatherConditionLabel, "Loading weather...");
    lv_label_set_text(weatherSummaryLabel, "Resolving location and forecast");
    lv_label_set_text(weatherDetailsLabel, "");
    lv_label_set_text(weatherAstronomyLabel, "");
    lv_label_set_text(weatherCreditLabel, "");
    return;
  }

  if (data.state == LiveDataState::error) {
    lv_label_set_text(weatherTemperatureLabel, "--");
    lv_label_set_text(weatherConditionLabel, "Weather unavailable");
    lv_label_set_text(weatherSummaryLabel, data.error);
    lv_label_set_text(weatherDetailsLabel, "Check Settings > Location and Wi-Fi.");
    lv_label_set_text(weatherAstronomyLabel, "");
    lv_label_set_text(weatherCreditLabel, "");
    return;
  }

  updateWeatherIcon(data);

  char temperature[24];
  snprintf(
      temperature,
      sizeof(temperature),
      "%.1f %s",
      data.temperature,
      data.temperatureSuffix);
  lv_label_set_text(weatherTemperatureLabel, temperature);
  lv_label_set_text(weatherConditionLabel, data.condition);

  lv_label_set_text(weatherSummaryLabel, data.location);

  char details[160];
  snprintf(
      details,
      sizeof(details),
      "Feels %.1f %s   Humidity %u%%\nWind %.1f %s   Pressure %.1f %s\nHigh %.1f %s   Low %.1f %s",
      data.feelsLike,
      data.temperatureSuffix,
      data.humidity,
      data.windSpeed,
      data.windSuffix,
      data.pressure,
      data.pressureSuffix,
      data.high,
      data.temperatureSuffix,
      data.low,
      data.temperatureSuffix);
  lv_label_set_text(weatherDetailsLabel, details);

  char astronomy[128];
  snprintf(
      astronomy,
      sizeof(astronomy),
      "Sunrise %s   Sunset %s\n%s   %u%% illuminated",
      data.sunrise,
      data.sunset,
      data.moonPhase,
      data.moonIllumination);
  lv_label_set_text(weatherAstronomyLabel, astronomy);
  lv_label_set_text(
      weatherCreditLabel,
      strcmp(data.provider, "MET Norway") == 0
          ? "Weather: MET Norway\nSolar: sunrise-sunset.org"
          : "Weather: Open-Meteo");
}

void showWeather() {
  currentPage = "Weather";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  lv_obj_t* scrollView = lv_obj_create(screen);
  lv_obj_remove_style_all(scrollView);
  lv_obj_set_size(scrollView, 320, 212);
  lv_obj_set_pos(scrollView, 0, 28);
  lv_obj_add_flag(scrollView, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(scrollView, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(scrollView, LV_SCROLLBAR_MODE_ON);

  lv_obj_t* content = lv_obj_create(scrollView);
  lv_obj_remove_style_all(content);
  lv_obj_set_size(content, 320, 286);
  lv_obj_set_pos(content, 0, 0);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* headerPanel = lv_obj_create(content);
  lv_obj_set_size(headerPanel, 304, 62);
  lv_obj_set_pos(headerPanel, 8, 6);
  lv_obj_set_style_radius(headerPanel, 10, 0);
  lv_obj_set_style_bg_color(headerPanel, lv_color_hex(0x1E293B), 0);
  lv_obj_set_style_bg_opa(headerPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(headerPanel, 1, 0);
  lv_obj_set_style_border_color(headerPanel, lv_color_hex(0x334155), 0);
  lv_obj_set_style_text_color(headerPanel, lv_color_hex(0xF8FAFC), 0);
  lv_obj_set_style_pad_all(headerPanel, 0, 0);
  lv_obj_clear_flag(headerPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(headerPanel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* iconPanel = lv_obj_create(headerPanel);
  lv_obj_set_size(iconPanel, 50, 50);
  lv_obj_set_pos(iconPanel, 6, 6);
  lv_obj_set_style_radius(iconPanel, 9, 0);
  lv_obj_set_style_bg_color(iconPanel, lv_color_hex(0x172554), 0);
  lv_obj_set_style_bg_opa(iconPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(iconPanel, 0, 0);
  lv_obj_set_style_pad_all(iconPanel, 0, 0);
  lv_obj_clear_flag(iconPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(iconPanel, LV_OBJ_FLAG_CLICKABLE);

  weatherIconImage = lv_img_create(iconPanel);
  lv_img_set_src(weatherIconImage, kTileIconPaths[0]);
  lv_obj_set_style_img_recolor(
      weatherIconImage,
      lv_color_hex(0xCBD5E1),
      0);
  lv_obj_set_style_img_recolor_opa(weatherIconImage, LV_OPA_COVER, 0);
  lv_obj_center(weatherIconImage);
  renderedWeatherIconPath = nullptr;

  weatherTemperatureLabel = lv_label_create(headerPanel);
  lv_obj_set_width(weatherTemperatureLabel, 84);
  lv_label_set_long_mode(weatherTemperatureLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(weatherTemperatureLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_pos(weatherTemperatureLabel, 64, 6);

  weatherConditionLabel = lv_label_create(headerPanel);
  lv_obj_set_width(weatherConditionLabel, 144);
  lv_label_set_long_mode(weatherConditionLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(weatherConditionLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(
      weatherConditionLabel,
      lv_color_hex(0xE2E8F0),
      0);
  lv_obj_set_pos(weatherConditionLabel, 152, 12);

  weatherSummaryLabel = lv_label_create(headerPanel);
  lv_obj_set_width(weatherSummaryLabel, 232);
  lv_label_set_long_mode(weatherSummaryLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(weatherSummaryLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(weatherSummaryLabel, lv_color_hex(0xCBD5E1), 0);
  lv_obj_set_pos(weatherSummaryLabel, 64, 38);

  weatherDetailsLabel = lv_label_create(content);
  lv_obj_set_width(weatherDetailsLabel, 296);
  lv_obj_set_style_text_font(weatherDetailsLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(weatherDetailsLabel, 12, 77);

  weatherAstronomyLabel = lv_label_create(content);
  lv_obj_set_width(weatherAstronomyLabel, 296);
  lv_obj_set_style_text_font(weatherAstronomyLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(
      weatherAstronomyLabel,
      lv_color_hex(0xFBBF24),
      0);
  lv_obj_set_pos(weatherAstronomyLabel, 12, 142);

  lv_obj_t* scrollHint = lv_label_create(content);
  lv_label_set_text(scrollHint, "Swipe up for more");
  lv_obj_set_style_text_font(scrollHint, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(scrollHint, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_pos(scrollHint, 208, 198);

  addBackButton(content);

  weatherCreditLabel = lv_label_create(content);
  lv_obj_set_width(weatherCreditLabel, 212);
  lv_obj_set_style_text_font(weatherCreditLabel, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(
      weatherCreditLabel,
      lv_color_hex(0x94A3B8),
      0);
  lv_obj_set_style_text_align(weatherCreditLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(weatherCreditLabel, 96, 249);

  renderWeatherPage();
}

lv_color_t aircraftColour(const AircraftData& aircraft) {
  if (aircraft.verticalRate < -300.0f) {
    return lv_color_hex(0xF97316);
  }
  if (aircraft.verticalRate > 300.0f) {
    return lv_color_hex(0x22C55E);
  }
  if (aircraft.altitudeFt >= 20000.0f) {
    return lv_color_hex(0xA78BFA);
  }
  return lv_color_hex(0x38BDF8);
}

void flightsBackEvent(lv_event_t* event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    pendingNavigation = PendingNavigation::flights;
  }
}

void showAircraftDetail(const AircraftData& aircraft) {
  currentPage = "Aircraft";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  char title[80];
  snprintf(
      title,
      sizeof(title),
      "%s%s%s",
      strlen(aircraft.flight) > 0 ? aircraft.flight : aircraft.hex,
      strlen(aircraft.registration) > 0 ? " - " : "",
      aircraft.registration);
  lv_obj_t* heading = lv_label_create(screen);
  lv_label_set_text(heading, title);
  lv_obj_set_width(heading, 296);
  lv_label_set_long_mode(heading, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_18, 0);
  lv_obj_set_pos(heading, 12, 38);

  char identity[120];
  snprintf(
      identity,
      sizeof(identity),
      "%s%s%s",
      strlen(aircraft.aircraftType) > 0 ? aircraft.aircraftType : "Type unknown",
      strlen(aircraft.description) > 0 ? "  " : "",
      aircraft.description);
  lv_obj_t* identityLabel = lv_label_create(screen);
  lv_label_set_text(identityLabel, identity);
  lv_obj_set_width(identityLabel, 296);
  lv_label_set_long_mode(identityLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(identityLabel, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_pos(identityLabel, 12, 65);

  const char* trend = aircraft.verticalRate > 300.0f
                          ? "climbing"
                          : (aircraft.verticalRate < -300.0f ? "descending"
                                                            : "level");
  char details[220];
  snprintf(
      details,
      sizeof(details),
      "Altitude %.0f ft   Speed %.0f kt\nHeading %.0f deg   %s\nDistance %.1f km   Bearing %.0f deg\nRoute %s%s%s",
      aircraft.altitudeFt,
      aircraft.groundSpeedKnots,
      aircraft.track,
      aircraft.onGround ? "on ground" : trend,
      aircraft.distanceKm,
      aircraft.bearing,
      strlen(aircraft.origin) > 0 ? aircraft.origin : "---",
      strlen(aircraft.origin) > 0 || strlen(aircraft.destination) > 0
          ? " to "
          : "",
      strlen(aircraft.destination) > 0 ? aircraft.destination : "---");
  lv_obj_t* detailLabel = lv_label_create(screen);
  lv_label_set_text(detailLabel, details);
  lv_obj_set_style_text_font(detailLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(detailLabel, 12, 94);

  lv_obj_t* back = lv_btn_create(screen);
  lv_obj_set_size(back, 90, 34);
  lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 8, -8);
  lv_obj_set_style_bg_color(back, lv_color_hex(0x334155), 0);
  lv_obj_add_event_cb(back, flightsBackEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* label = lv_label_create(back);
  lv_label_set_text(label, "Flights");
  lv_obj_center(label);
}

void aircraftEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  const size_t index =
      reinterpret_cast<size_t>(lv_event_get_user_data(event));
  if (index < renderedFlightsData.aircraftCount) {
    selectedAircraft = renderedFlightsData.aircraft[index];
    pendingNavigation = PendingNavigation::aircraft;
  }
}

void addRadarCircle(lv_obj_t* parent, int diameter, int centreX, int centreY) {
  lv_obj_t* circle = lv_obj_create(parent);
  lv_obj_remove_style_all(circle);
  lv_obj_set_size(circle, diameter, diameter);
  lv_obj_set_pos(
      circle,
      centreX - diameter / 2,
      centreY - diameter / 2);
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(circle, 1, 0);
  lv_obj_set_style_border_color(circle, lv_color_hex(0x334155), 0);
  lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);
}

void renderFlightsPage() {
  if (currentPage != "Flights" || flightsRadar == nullptr) {
    return;
  }

  const FlightsData data = liveDataFlightsSnapshot();
  if (data.updatedAt == renderedFlightsUpdate &&
      data.state == renderedFlightsState) {
    return;
  }
  renderedFlightsUpdate = data.updatedAt;
  renderedFlightsState = data.state;
  renderedFlightsData = data;
  lv_obj_clean(flightsRadar);

  if (data.state == LiveDataState::idle ||
      data.state == LiveDataState::loading) {
    lv_label_set_text(flightsMessageLabel, "Loading nearby aircraft...");
    return;
  }
  if (data.state == LiveDataState::error) {
    lv_label_set_text(flightsMessageLabel, data.error);
    return;
  }

  lv_label_set_text_fmt(
      flightsMessageLabel,
      "%u nearby - %s - %u km",
      data.totalAircraft,
      data.provider,
      data.radiusKm);

  constexpr int centreX = 160;
  constexpr int centreY = 83;
  constexpr int radarRadius = 77;
  addRadarCircle(flightsRadar, radarRadius * 2, centreX, centreY);
  addRadarCircle(flightsRadar, radarRadius, centreX, centreY);

  lv_obj_t* vertical = lv_obj_create(flightsRadar);
  lv_obj_remove_style_all(vertical);
  lv_obj_set_size(vertical, 1, radarRadius * 2);
  lv_obj_set_pos(vertical, centreX, centreY - radarRadius);
  lv_obj_set_style_bg_opa(vertical, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(vertical, lv_color_hex(0x334155), 0);

  lv_obj_t* horizontal = lv_obj_create(flightsRadar);
  lv_obj_remove_style_all(horizontal);
  lv_obj_set_size(horizontal, radarRadius * 2, 1);
  lv_obj_set_pos(horizontal, centreX - radarRadius, centreY);
  lv_obj_set_style_bg_opa(horizontal, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(horizontal, lv_color_hex(0x334155), 0);

  lv_obj_t* north = lv_label_create(flightsRadar);
  lv_label_set_text(north, "N");
  lv_obj_set_style_text_color(north, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_pos(north, centreX - 4, 1);

  lv_obj_t* home = lv_obj_create(flightsRadar);
  lv_obj_remove_style_all(home);
  lv_obj_set_size(home, 7, 7);
  lv_obj_set_pos(home, centreX - 3, centreY - 3);
  lv_obj_set_style_radius(home, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(home, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(home, lv_color_hex(0xF8FAFC), 0);

  for (uint8_t index = 0; index < data.aircraftCount; ++index) {
    const AircraftData& aircraft = data.aircraft[index];
    const float radialDistance =
        min(aircraft.distanceKm / max(1.0f, static_cast<float>(data.radiusKm)),
            1.0f) *
        radarRadius;
    const float angle = aircraft.bearing * PI / 180.0f;
    const int x =
        centreX + lroundf(sinf(angle) * radialDistance) - 12;
    const int y =
        centreY - lroundf(cosf(angle) * radialDistance) - 12;

    lv_obj_t* marker = lv_btn_create(flightsRadar);
    lv_obj_set_size(marker, 25, 25);
    lv_obj_set_pos(marker, x, y);
    lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(marker, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(marker, LV_OPA_30, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(
        marker,
        aircraftColour(aircraft),
        LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(marker, 0, 0);
    lv_obj_set_style_pad_all(marker, 0, 0);
    lv_obj_add_event_cb(
        marker,
        aircraftEvent,
        LV_EVENT_CLICKED,
        reinterpret_cast<void*>(static_cast<size_t>(index)));

    const float heading = aircraft.track * PI / 180.0f;
    const float directionX = sinf(heading);
    const float directionY = -cosf(heading);
    const float perpendicularX = -directionY;
    const float perpendicularY = directionX;
    auto setPoint = [&](lv_point_t& point, float along, float across) {
      point.x = static_cast<lv_coord_t>(
          lroundf(12.0f + directionX * along + perpendicularX * across));
      point.y = static_cast<lv_coord_t>(
          lroundf(12.0f + directionY * along + perpendicularY * across));
    };
    setPoint(aircraftLinePoints[index][0][0], -8.0f, 0.0f);
    setPoint(aircraftLinePoints[index][0][1], 9.0f, 0.0f);
    setPoint(aircraftLinePoints[index][1][0], 1.0f, -7.0f);
    setPoint(aircraftLinePoints[index][1][1], 1.0f, 7.0f);
    setPoint(aircraftLinePoints[index][2][0], -6.0f, -3.5f);
    setPoint(aircraftLinePoints[index][2][1], -6.0f, 3.5f);
    for (int segment = 0; segment < 3; ++segment) {
      lv_obj_t* line = lv_line_create(marker);
      lv_line_set_points(line, aircraftLinePoints[index][segment], 2);
      lv_obj_set_style_line_width(line, segment == 0 ? 3 : 2, 0);
      lv_obj_set_style_line_rounded(line, true, 0);
      lv_obj_set_style_line_color(line, aircraftColour(aircraft), 0);
      lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    }
  }
}

void showFlights() {
  currentPage = "Flights";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  flightsRadar = lv_obj_create(screen);
  lv_obj_remove_style_all(flightsRadar);
  lv_obj_set_size(flightsRadar, 320, 166);
  lv_obj_set_pos(flightsRadar, 0, 28);
  lv_obj_clear_flag(flightsRadar, LV_OBJ_FLAG_SCROLLABLE);

  flightsMessageLabel = lv_label_create(screen);
  lv_obj_set_width(flightsMessageLabel, 224);
  lv_label_set_long_mode(flightsMessageLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(flightsMessageLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_align(flightsMessageLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(flightsMessageLabel, LV_ALIGN_BOTTOM_RIGHT, -8, -16);

  renderedFlightsUpdate = UINT32_MAX;
  renderedFlightsState = LiveDataState::idle;
  addBackButton(screen);
  const FlightsData flights = liveDataFlightsSnapshot();
  if (flights.state == LiveDataState::idle ||
      flights.state == LiveDataState::error ||
      millis() - flights.updatedAt >= flightsRefreshMilliseconds) {
    liveDataRequestFlights();
  }
  renderFlightsPage();
}

const char* friendlyPrinterState(const BambuddyData& data) {
  if (!data.connected) {
    return "Offline";
  }
  if (strcmp(data.printerState, "RUNNING") == 0) {
    return "Printing";
  }
  if (strcmp(data.printerState, "PAUSE") == 0) {
    return "Paused";
  }
  if (strlen(data.printerState) > 0) {
    return data.printerState;
  }
  return "Connected";
}

lv_color_t printerStateColour(const BambuddyData& data) {
  if (data.state == LiveDataState::error || !data.connected) {
    return lv_color_hex(0x991B1B);
  }
  if (strcmp(data.printerState, "RUNNING") == 0) {
    return lv_color_hex(0x166534);
  }
  if (strcmp(data.printerState, "PAUSE") == 0) {
    return lv_color_hex(0x92400E);
  }
  return lv_color_hex(0x0F766E);
}

lv_obj_t* createBambuddyPanel(
    lv_obj_t* parent,
    int x,
    int y,
    int width,
    int height) {
  lv_obj_t* panel = lv_obj_create(parent);
  lv_obj_set_size(panel, width, height);
  lv_obj_set_pos(panel, x, y);
  lv_obj_set_style_radius(panel, 10, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x1E293B), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x334155), 0);
  lv_obj_set_style_text_color(panel, lv_color_hex(0xF8FAFC), 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  return panel;
}

lv_obj_t* createBambuddyMetric(
    lv_obj_t* parent,
    int x,
    const char* caption) {
  lv_obj_t* panel = createBambuddyPanel(parent, x, 184, 72, 48);

  lv_obj_t* title = lv_label_create(panel);
  lv_label_set_text(title, caption);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x94A3B8), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

  lv_obj_t* value = lv_label_create(panel);
  lv_obj_set_width(value, 66);
  lv_obj_set_style_text_font(value, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(value, LV_ALIGN_BOTTOM_MID, 0, -7);
  return value;
}

void renderBambuddyPage() {
  if (currentPage != "Bambuddy" || bambuddyNameLabel == nullptr) {
    return;
  }

  const BambuddyData data = liveDataBambuddySnapshot();
  if (data.state == LiveDataState::idle ||
      data.state == LiveDataState::loading) {
    lv_label_set_text(bambuddyNameLabel, "Loading Bambuddy...");
    lv_label_set_text(bambuddyStatusLabel, "Connecting");
    lv_obj_set_style_bg_color(
        bambuddyStatusLabel,
        lv_color_hex(0x475569),
        0);
    lv_label_set_text(bambuddySignalLabel, "");
    lv_label_set_text(bambuddyJobLabel, "");
    lv_bar_set_value(bambuddyProgressBar, 0, LV_ANIM_OFF);
    lv_label_set_text(bambuddyProgressLabel, "");
    lv_label_set_text(
        bambuddyRemainingLabel,
        "Requesting read-only status");
    lv_label_set_text(bambuddyNozzleLabel, "--");
    lv_label_set_text(bambuddyBedLabel, "--");
    lv_label_set_text(bambuddyLayerLabel, "--");
    return;
  }

  if (data.state == LiveDataState::error) {
    lv_label_set_text(bambuddyNameLabel, "Bambuddy unavailable");
    lv_label_set_text(bambuddyStatusLabel, "Offline");
    lv_obj_set_style_bg_color(
        bambuddyStatusLabel,
        lv_color_hex(0x991B1B),
        0);
    lv_label_set_text(bambuddySignalLabel, "");
    lv_label_set_text(bambuddyJobLabel, data.error);
    lv_bar_set_value(bambuddyProgressBar, 0, LV_ANIM_OFF);
    lv_label_set_text(bambuddyProgressLabel, "");
    lv_label_set_text(
        bambuddyRemainingLabel,
        "Check Settings > Services");
    lv_label_set_text(bambuddyNozzleLabel, "--");
    lv_label_set_text(bambuddyBedLabel, "--");
    lv_label_set_text(bambuddyLayerLabel, "--");
    return;
  }

  lv_label_set_text(
      bambuddyNameLabel,
      strlen(data.printerName) > 0 ? data.printerName : "Bambuddy printer");
  lv_label_set_text(bambuddyStatusLabel, friendlyPrinterState(data));
  lv_obj_set_style_bg_color(
      bambuddyStatusLabel,
      printerStateColour(data),
      0);
  lv_label_set_text_fmt(
      bambuddySignalLabel,
      data.connected ? "%d dBm" : "",
      data.wifiSignal);

  const bool active =
      data.connected &&
      (strcmp(data.printerState, "RUNNING") == 0 ||
       strcmp(data.printerState, "PAUSE") == 0);
  lv_label_set_text(
      bambuddyJobLabel,
      active && strlen(data.printName) > 0
          ? data.printName
          : (data.connected ? "No active print" : "Printer is offline"));
  lv_bar_set_value(
      bambuddyProgressBar,
      static_cast<int>(lround(data.progress)),
      LV_ANIM_OFF);

  char progress[64];
  snprintf(
      progress,
      sizeof(progress),
      "%.0f%%",
      data.progress);
  lv_label_set_text(bambuddyProgressLabel, active ? progress : "");

  lv_label_set_text_fmt(
      bambuddyRemainingLabel,
      active ? "%d min remaining" : "",
      data.remainingMinutes);
  lv_label_set_text_fmt(
      bambuddyNozzleLabel,
      data.connected ? "%d / %d C" : "--",
      static_cast<int>(lroundf(data.nozzleTemperature)),
      static_cast<int>(lroundf(data.nozzleTarget)));
  lv_label_set_text_fmt(
      bambuddyBedLabel,
      data.connected ? "%d / %d C" : "--",
      static_cast<int>(lroundf(data.bedTemperature)),
      static_cast<int>(lroundf(data.bedTarget)));
  lv_label_set_text_fmt(
      bambuddyLayerLabel,
      active ? "%d / %d" : "--",
      data.currentLayer,
      data.totalLayers);
}

void showBambuddy() {
  currentPage = "Bambuddy";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  lv_obj_t* headerPanel = createBambuddyPanel(screen, 8, 34, 304, 62);

  lv_obj_t* iconPanel = lv_obj_create(headerPanel);
  lv_obj_set_size(iconPanel, 50, 50);
  lv_obj_set_pos(iconPanel, 6, 6);
  lv_obj_set_style_radius(iconPanel, 9, 0);
  lv_obj_set_style_bg_color(iconPanel, lv_color_hex(0x7C3AED), 0);
  lv_obj_set_style_bg_opa(iconPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(iconPanel, 0, 0);
  lv_obj_set_style_pad_all(iconPanel, 0, 0);
  lv_obj_clear_flag(iconPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(iconPanel, LV_OBJ_FLAG_CLICKABLE);

  if (sdIconAvailable(kTileIconPaths[2])) {
    lv_obj_t* icon = lv_img_create(iconPanel);
    lv_img_set_src(icon, kTileIconPaths[2]);
    lv_obj_set_style_img_recolor(icon, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_center(icon);
  } else {
    lv_obj_t* fallback = lv_label_create(iconPanel);
    lv_label_set_text(fallback, "B");
    lv_obj_set_style_text_font(fallback, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(fallback, lv_color_hex(0xF8FAFC), 0);
    lv_obj_center(fallback);
  }

  bambuddyNameLabel = lv_label_create(headerPanel);
  lv_obj_set_width(bambuddyNameLabel, 232);
  lv_label_set_long_mode(bambuddyNameLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(bambuddyNameLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_pos(bambuddyNameLabel, 64, 7);

  bambuddyStatusLabel = lv_label_create(headerPanel);
  lv_label_set_text(bambuddyStatusLabel, "Connecting");
  lv_obj_set_style_text_font(bambuddyStatusLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(
      bambuddyStatusLabel,
      lv_color_hex(0xF8FAFC),
      0);
  lv_obj_set_style_bg_color(bambuddyStatusLabel, lv_color_hex(0x475569), 0);
  lv_obj_set_style_bg_opa(bambuddyStatusLabel, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(bambuddyStatusLabel, 6, 0);
  lv_obj_set_style_pad_hor(bambuddyStatusLabel, 7, 0);
  lv_obj_set_style_pad_ver(bambuddyStatusLabel, 4, 0);
  lv_obj_set_pos(bambuddyStatusLabel, 64, 34);

  bambuddySignalLabel = lv_label_create(headerPanel);
  lv_obj_set_style_text_font(bambuddySignalLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(
      bambuddySignalLabel,
      lv_color_hex(0x94A3B8),
      0);
  lv_obj_align(bambuddySignalLabel, LV_ALIGN_BOTTOM_RIGHT, -8, -9);

  lv_obj_t* jobPanel = createBambuddyPanel(screen, 8, 102, 304, 47);
  lv_obj_t* jobCaption = lv_label_create(jobPanel);
  lv_label_set_text(jobCaption, "CURRENT JOB");
  lv_obj_set_style_text_font(jobCaption, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(jobCaption, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_pos(jobCaption, 8, 5);

  bambuddyJobLabel = lv_label_create(jobPanel);
  lv_obj_set_width(bambuddyJobLabel, 288);
  lv_label_set_long_mode(bambuddyJobLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(bambuddyJobLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(bambuddyJobLabel, 8, 22);

  bambuddyProgressBar = lv_bar_create(screen);
  lv_obj_set_size(bambuddyProgressBar, 304, 10);
  lv_obj_set_pos(bambuddyProgressBar, 8, 155);
  lv_bar_set_range(bambuddyProgressBar, 0, 100);
  lv_obj_set_style_radius(bambuddyProgressBar, 5, LV_PART_MAIN);
  lv_obj_set_style_radius(bambuddyProgressBar, 5, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(
      bambuddyProgressBar,
      lv_color_hex(0x334155),
      LV_PART_MAIN);
  lv_obj_set_style_bg_color(
      bambuddyProgressBar,
      lv_color_hex(0x7C3AED),
      LV_PART_INDICATOR);

  bambuddyProgressLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(bambuddyProgressLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_pos(bambuddyProgressLabel, 8, 168);

  bambuddyRemainingLabel = lv_label_create(screen);
  lv_obj_set_width(bambuddyRemainingLabel, 210);
  lv_obj_set_style_text_font(
      bambuddyRemainingLabel,
      &lv_font_montserrat_12,
      0);
  lv_obj_set_style_text_align(
      bambuddyRemainingLabel,
      LV_TEXT_ALIGN_RIGHT,
      0);
  lv_obj_align(bambuddyRemainingLabel, LV_ALIGN_TOP_RIGHT, -8, 168);

  addBackButton(screen);
  bambuddyNozzleLabel = createBambuddyMetric(screen, 84, "NOZZLE");
  bambuddyBedLabel = createBambuddyMetric(screen, 160, "BED");
  bambuddyLayerLabel = createBambuddyMetric(screen, 236, "LAYER");
  liveDataRequestBambuddy();
  renderBambuddyPage();
}

void styleSystemsCard(
    size_t index,
    bool online,
    bool configured = true) {
  if (index >= kSystemsCardCount || systemsCards[index] == nullptr) {
    return;
  }
  const uint32_t background =
      !configured ? 0x1E293B : (online ? 0x052E16 : 0x450A0A);
  const uint32_t border =
      !configured ? 0x475569 : (online ? 0x22C55E : 0xEF4444);
  lv_obj_set_style_bg_color(
      systemsCards[index],
      lv_color_hex(background),
      0);
  lv_obj_set_style_border_color(
      systemsCards[index],
      lv_color_hex(border),
      0);
}

void renderSystemsPage() {
  if (currentPage != "Systems" || systemsStateLabels[0] == nullptr) {
    return;
  }

  const SystemsData data = liveDataSystemsSnapshot();
  const bool wifiOnline = WiFi.status() == WL_CONNECTED;
  lv_label_set_text(
      systemsStateLabels[0],
      sdReady && wifiOnline ? "Healthy" : "Attention");
  lv_label_set_text_fmt(
      systemsDetailLabels[0],
      "%uK | %ddBm\nUp %lum",
      ESP.getFreeHeap() / 1024,
      wifiOnline ? WiFi.RSSI() : 0,
      millis() / 60000);
  styleSystemsCard(0, sdReady && wifiOnline);

  if (data.state == LiveDataState::idle ||
      data.state == LiveDataState::loading) {
    lv_label_set_text(systemsStateLabels[1], "Checking...");
    lv_label_set_text(systemsDetailLabels[1], "DNS and outbound TCP");
    styleSystemsCard(1, false, false);
  } else if (data.state == LiveDataState::error) {
    lv_label_set_text(systemsStateLabels[1], "Offline");
    lv_label_set_text(systemsDetailLabels[1], data.error);
    styleSystemsCard(1, false);
  } else {
    const bool internetHealthy = data.dnsOnline && data.internetOnline;
    lv_label_set_text(
        systemsStateLabels[1],
        internetHealthy ? "Online" : "Attention");
    lv_label_set_text_fmt(
        systemsDetailLabels[1],
        "DNS %s | %ums\nGW %s",
        data.dnsOnline ? "OK" : "fail",
        data.internetMilliseconds,
        data.gateway);
    styleSystemsCard(1, internetHealthy);
  }

  for (size_t index = 0; index < kMaximumSystemMonitors; ++index) {
    const int8_t mappedCardIndex = systemsMonitorCardIndices[index];
    if (mappedCardIndex < 0) {
      continue;
    }
    const size_t cardIndex = static_cast<size_t>(mappedCardIndex);
    const SystemMonitorData& monitor = data.monitors[index];
    if (monitor.configured && strlen(monitor.name) > 0) {
      lv_label_set_text(systemsNameLabels[cardIndex], monitor.name);
    }
    lv_label_set_text(
        systemsStateLabels[cardIndex],
        !monitor.configured
            ? "Not set"
            : (monitor.online ? "Online" : "Offline"));
    if (monitor.configured) {
      lv_label_set_text_fmt(
          systemsDetailLabels[cardIndex],
          "%s\n%ums",
          monitor.detail,
          monitor.responseMilliseconds);
    } else {
      lv_label_set_text(
          systemsDetailLabels[cardIndex],
          "Use web portal");
    }
    styleSystemsCard(cardIndex, monitor.online, monitor.configured);
  }
}

void showSystems() {
  currentPage = "Systems";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  constexpr int xPositions[] = {6, 111, 216};
  constexpr int yPositions[] = {34, 112};
  const SystemsData systems = liveDataSystemsSnapshot();
  for (size_t index = 0; index < kSystemsCardCount; ++index) {
    systemsCards[index] = nullptr;
    systemsNameLabels[index] = nullptr;
    systemsStateLabels[index] = nullptr;
    systemsDetailLabels[index] = nullptr;
  }
  for (size_t index = 0; index < kMaximumSystemMonitors; ++index) {
    systemsMonitorCardIndices[index] = -1;
  }

  size_t visibleCardCount = 2;
  for (size_t index = 0; index < kMaximumSystemMonitors; ++index) {
    if (systemMonitorEnabled[index]) {
      systemsMonitorCardIndices[index] =
          static_cast<int8_t>(visibleCardCount++);
    }
  }

  for (size_t index = 0; index < visibleCardCount; ++index) {
    lv_obj_t* card = lv_obj_create(screen);
    systemsCards[index] = card;
    lv_obj_set_size(card, 98, 70);
    lv_obj_set_pos(card, xPositions[index % 3], yPositions[index / 3]);
    lv_obj_set_style_radius(card, 9, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x475569), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* name = lv_label_create(card);
    systemsNameLabels[index] = name;
    if (index == 0) {
      lv_label_set_text(name, "DEVICE");
    } else if (index == 1) {
      lv_label_set_text(name, "INTERNET");
    } else {
      size_t monitorIndex = 0;
      while (monitorIndex < kMaximumSystemMonitors &&
             systemsMonitorCardIndices[monitorIndex] !=
                 static_cast<int8_t>(index)) {
        ++monitorIndex;
      }
      const char* configuredName =
          monitorIndex < kMaximumSystemMonitors
              ? systems.monitors[monitorIndex].name
              : "";
      if (strlen(configuredName) > 0) {
        lv_label_set_text(name, configuredName);
      } else {
        lv_label_set_text_fmt(
            name,
            "MONITOR %u",
            static_cast<unsigned>(monitorIndex + 1));
      }
    }
    lv_obj_set_width(name, 88);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_pos(name, 5, 4);

    systemsStateLabels[index] = lv_label_create(card);
    lv_obj_set_width(systemsStateLabels[index], 88);
    lv_label_set_long_mode(
        systemsStateLabels[index],
        LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(
        systemsStateLabels[index],
        &lv_font_montserrat_12,
        0);
    lv_obj_set_style_text_color(
        systemsStateLabels[index],
        lv_color_hex(0xF8FAFC),
        0);
    lv_obj_set_pos(systemsStateLabels[index], 5, 22);

    systemsDetailLabels[index] = lv_label_create(card);
    lv_obj_set_width(systemsDetailLabels[index], 88);
    lv_label_set_long_mode(
        systemsDetailLabels[index],
        LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(
        systemsDetailLabels[index],
        &lv_font_montserrat_10,
        0);
    lv_obj_set_style_text_color(
        systemsDetailLabels[index],
        lv_color_hex(0xCBD5E1),
        0);
    lv_obj_set_pos(systemsDetailLabels[index], 5, 41);
  }

  addBackButton(screen);
  if (systems.state == LiveDataState::idle ||
      systems.state == LiveDataState::error ||
      millis() - systems.updatedAt >= systemsRefreshMilliseconds) {
    liveDataRequestSystems();
  }
  renderSystemsPage();
}

#if 0
void renderCalendarPage() {
  if (currentPage != "Calendar" || calendarNextTitleLabel == nullptr) {
    return;
  }
  const CalendarData data = liveDataCalendarSnapshot();
  if (data.state == LiveDataState::idle ||
      data.state == LiveDataState::loading) {
    lv_label_set_text(calendarNextTitleLabel, "Checking calendar...");
    lv_label_set_text(calendarNextDetailLabel, "Fetching the iCalendar feed");
  } else if (data.state == LiveDataState::error) {
    lv_label_set_text(calendarNextTitleLabel, "Calendar unavailable");
    lv_label_set_text(calendarNextDetailLabel, data.error);
  } else if (!data.hasNextEvent) {
    lv_label_set_text(calendarNextTitleLabel, "Nothing upcoming");
    lv_label_set_text(calendarNextDetailLabel, "No events in the next 30 days");
  } else {
    lv_label_set_text(calendarNextTitleLabel, data.nextEvent.summary);
    if (strlen(data.nextEvent.location) > 0) {
      lv_label_set_text_fmt(
          calendarNextDetailLabel,
          "%s | %s",
          data.nextEvent.time,
          data.nextEvent.location);
    } else {
      lv_label_set_text(calendarNextDetailLabel, data.nextEvent.time);
    }
  }

  for (size_t index = 0; index < kMaximumCalendarEvents; ++index) {
    if (index < data.todayCount) {
      const CalendarEventData& event = data.today[index];
      if (strlen(event.location) > 0) {
        lv_label_set_text_fmt(
            calendarAgendaLabels[index],
            "%s\n%s\n%s",
            event.time,
            event.summary,
            event.location);
      } else {
        lv_label_set_text_fmt(
            calendarAgendaLabels[index],
            "%s\n%s",
            event.time,
            event.summary);
      }
    } else if (index == 0 && data.state == LiveDataState::ready) {
      lv_label_set_text(calendarAgendaLabels[index], "No events today");
    } else {
      lv_label_set_text(calendarAgendaLabels[index], "");
    }
  }
}

void showCalendar() {
  currentPage = "Calendar";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  lv_obj_t* nextCard = lv_obj_create(screen);
  lv_obj_set_size(nextCard, 308, 68);
  lv_obj_set_pos(nextCard, 6, 34);
  lv_obj_set_style_radius(nextCard, 10, 0);
  lv_obj_set_style_bg_color(nextCard, lv_color_hex(0x172554), 0);
  lv_obj_set_style_bg_opa(nextCard, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(nextCard, lv_color_hex(0x3B82F6), 0);
  lv_obj_set_style_border_width(nextCard, 1, 0);
  lv_obj_set_style_pad_all(nextCard, 0, 0);
  lv_obj_clear_flag(nextCard, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* nextHeading = lv_label_create(nextCard);
  lv_label_set_text(nextHeading, "NEXT EVENT");
  lv_obj_set_style_text_font(nextHeading, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(nextHeading, lv_color_hex(0x93C5FD), 0);
  lv_obj_set_pos(nextHeading, 8, 5);

  calendarNextTitleLabel = lv_label_create(nextCard);
  lv_obj_set_width(calendarNextTitleLabel, 290);
  lv_label_set_long_mode(calendarNextTitleLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(
      calendarNextTitleLabel,
      &lv_font_montserrat_14,
      0);
  lv_obj_set_style_text_color(
      calendarNextTitleLabel,
      lv_color_hex(0xF8FAFC),
      0);
  lv_obj_set_pos(calendarNextTitleLabel, 8, 22);

  calendarNextDetailLabel = lv_label_create(nextCard);
  lv_obj_set_width(calendarNextDetailLabel, 290);
  lv_label_set_long_mode(calendarNextDetailLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(
      calendarNextDetailLabel,
      &lv_font_montserrat_10,
      0);
  lv_obj_set_style_text_color(
      calendarNextDetailLabel,
      lv_color_hex(0xCBD5E1),
      0);
  lv_obj_set_pos(calendarNextDetailLabel, 8, 46);

  lv_obj_t* agendaHeading = lv_label_create(screen);
  lv_label_set_text(agendaHeading, "TODAY");
  lv_obj_set_style_text_font(agendaHeading, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(agendaHeading, lv_color_hex(0x94A3B8), 0);
  lv_obj_set_pos(agendaHeading, 8, 106);

  constexpr int xPositions[] = {6, 111, 216};
  for (size_t index = 0; index < kMaximumCalendarEvents; ++index) {
    lv_obj_t* card = lv_obj_create(screen);
    lv_obj_set_size(card, 98, 70);
    lv_obj_set_pos(card, xPositions[index], 121);
    lv_obj_set_style_radius(card, 9, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x475569), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    calendarAgendaLabels[index] = lv_label_create(card);
    lv_obj_set_size(calendarAgendaLabels[index], 88, 62);
    lv_label_set_long_mode(
        calendarAgendaLabels[index],
        LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(
        calendarAgendaLabels[index],
        &lv_font_montserrat_10,
        0);
    lv_obj_set_style_text_color(
        calendarAgendaLabels[index],
        lv_color_hex(0xE2E8F0),
        0);
    lv_obj_set_pos(calendarAgendaLabels[index], 5, 5);
  }

  addBackButton(screen);
  const CalendarData calendar = liveDataCalendarSnapshot();
  if (calendar.state == LiveDataState::idle ||
      calendar.state == LiveDataState::error ||
      millis() - calendar.updatedAt >= calendarRefreshMilliseconds) {
    liveDataRequestCalendar();
  }
  renderCalendarPage();
}
#endif

bool calendarLeapYear(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int calendarDaysInMonth(int year, int month) {
  static constexpr uint8_t days[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && calendarLeapYear(year)
             ? 29
             : days[month - 1];
}

int calendarWeekday(int year, int month, int day) {
  static constexpr int offsets[] = {
      0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) {
    --year;
  }
  return (
      year + year / 4 - year / 100 + year / 400 +
      offsets[month - 1] + day) %
      7;
}

void calendarPreviousEvent(lv_event_t* event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    --calendarMonthOffset;
    pendingNavigation = PendingNavigation::calendar;
  }
}

void calendarNextEvent(lv_event_t* event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    ++calendarMonthOffset;
    pendingNavigation = PendingNavigation::calendar;
  }
}

void showStaticCalendar() {
  currentPage = "Calendar";
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  const time_t localNow = time(nullptr);
  struct tm date = {};
  localtime_r(&localNow, &date);

  if (localNow < 1700000000) {
    lv_obj_t* message = lv_label_create(screen);
    lv_label_set_text(message, "Waiting for the clock to synchronize");
    lv_obj_set_style_text_color(message, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(message, LV_ALIGN_CENTER, 0, 0);
    addBackButton(screen);
    return;
  }

  static constexpr const char* months[] = {
      "January", "February", "March", "April", "May", "June",
      "July", "August", "September", "October", "November", "December"};
  static constexpr const char* weekdays[] = {
      "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  const int currentYear = date.tm_year + 1900;
  const int currentMonth = date.tm_mon + 1;
  const int today = date.tm_mday;
  const int monthIndex =
      currentYear * 12 + currentMonth - 1 + calendarMonthOffset;
  const int year = monthIndex / 12;
  const int month = monthIndex % 12 + 1;

  lv_obj_t* previousButton = lv_btn_create(screen);
  lv_obj_set_size(previousButton, 30, 27);
  lv_obj_align(previousButton, LV_ALIGN_TOP_LEFT, 10, 29);
  lv_obj_set_style_radius(previousButton, 8, 0);
  lv_obj_set_style_bg_color(previousButton, lv_color_hex(0x173A5E), 0);
  lv_obj_add_event_cb(previousButton, calendarPreviousEvent, LV_EVENT_CLICKED,
                     nullptr);
  lv_obj_t* previousLabel = lv_label_create(previousButton);
  lv_label_set_text(previousLabel, "<");
  lv_obj_set_style_text_font(previousLabel, &lv_font_montserrat_18, 0);
  lv_obj_center(previousLabel);

  lv_obj_t* nextButton = lv_btn_create(screen);
  lv_obj_set_size(nextButton, 30, 27);
  lv_obj_align(nextButton, LV_ALIGN_TOP_RIGHT, -10, 29);
  lv_obj_set_style_radius(nextButton, 8, 0);
  lv_obj_set_style_bg_color(nextButton, lv_color_hex(0x173A5E), 0);
  lv_obj_add_event_cb(nextButton, calendarNextEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* nextLabel = lv_label_create(nextButton);
  lv_label_set_text(nextLabel, ">");
  lv_obj_set_style_text_font(nextLabel, &lv_font_montserrat_18, 0);
  lv_obj_center(nextLabel);

  lv_obj_t* heading = lv_label_create(screen);
  lv_label_set_text_fmt(heading, "%s %d", months[month - 1], year);
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(heading, lv_color_hex(0xF8FAFC), 0);
  lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 34);

  constexpr int left = 9;
  constexpr int cellWidth = 43;
  for (int column = 0; column < 7; ++column) {
    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text(label, weekdays[column]);
    lv_obj_set_width(label, cellWidth);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(
        label,
        column == 0 || column == 6
            ? lv_color_hex(0x93C5FD)
            : lv_color_hex(0x94A3B8),
        0);
    lv_obj_set_pos(label, left + column * cellWidth, 59);
  }

  const int firstWeekday = calendarWeekday(year, month, 1);
  const int dayCount = calendarDaysInMonth(year, month);
  for (int day = 1; day <= dayCount; ++day) {
    const int cell = firstWeekday + day - 1;
    const int row = cell / 7;
    const int column = cell % 7;
    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text_fmt(label, "%d", day);
    lv_obj_set_size(label, cellWidth, 19);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(
        label,
        calendarMonthOffset == 0 && day == today
            ? lv_color_hex(0xFFFFFF)
            : lv_color_hex(0xE2E8F0),
        0);
    if (calendarMonthOffset == 0 && day == today) {
      lv_obj_set_style_bg_color(label, lv_color_hex(0x2563EB), 0);
      lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(label, 7, 0);
    }
    lv_obj_set_pos(
        label,
        left + column * cellWidth,
        77 + row * 19);
  }

  addBackButton(screen);
}

void showPlaceholder(const char* title) {
  currentPage = title;
  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  lv_obj_t* heading = lv_label_create(screen);
  lv_label_set_text(heading, title);
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_20, 0);
  lv_obj_align(heading, LV_ALIGN_CENTER, 0, -25);

  lv_obj_t* message = lv_label_create(screen);
  lv_label_set_text(message, "Page shell is working.\nLive data comes after hardware testing.");
  lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(message, lv_color_hex(0xCBD5E1), 0);
  lv_obj_align(message, LV_ALIGN_CENTER, 0, 20);

  addBackButton(screen);
}

void tileEvent(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  const char* tileName = static_cast<const char*>(lv_event_get_user_data(event));
  if (strcmp(tileName, "Settings") == 0) {
    showSettings();
  } else if (strcmp(tileName, "Weather") == 0) {
    showWeather();
  } else if (strcmp(tileName, "Flights") == 0) {
    pendingNavigation = PendingNavigation::flights;
  } else if (strcmp(tileName, "Bambuddy") == 0) {
    showBambuddy();
  } else if (strcmp(tileName, "Systems") == 0) {
    pendingNavigation = PendingNavigation::systems;
  } else if (strcmp(tileName, "Calendar") == 0) {
    calendarMonthOffset = 0;
    pendingNavigation = PendingNavigation::calendar;
  } else {
    showPlaceholder(tileName);
  }
}

void showHome() {
  currentPage = "Desk Dashboard";
  brightnessLabel = nullptr;

  lv_obj_t* screen = lv_scr_act();
  lv_obj_clean(screen);
  styleScreen(screen);
  addStatusBar(screen);

  constexpr int tileWidth = 98;
  constexpr int tileHeight = 84;
  constexpr int xPositions[] = {6, 111, 216};
  constexpr int yPositions[] = {34, 125};

  for (int index = 0; index < 6; ++index) {
    if (index < 5 && !tileEnabled[index]) {
      continue;
    }

    lv_obj_t* tile = lv_btn_create(screen);
    lv_obj_set_size(tile, tileWidth, tileHeight);
    lv_obj_set_pos(tile, xPositions[index % 3], yPositions[index / 3]);
    lv_obj_set_style_radius(tile, 9, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(kTileColors[index]), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(tile, 0, 0);
    lv_obj_add_event_cb(tile, tileEvent, LV_EVENT_CLICKED, const_cast<char*>(kTileNames[index]));

    if (sdIconAvailable(kTileIconPaths[index])) {
      lv_obj_t* icon = lv_img_create(tile);
      lv_img_set_src(icon, kTileIconPaths[index]);
      lv_obj_set_style_img_recolor(icon, lv_color_hex(0xF8FAFC), 0);
      lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);
      lv_obj_align(icon, LV_ALIGN_CENTER, 0, -12);
    } else {
      lv_obj_t* initial = lv_label_create(tile);
      char initialText[2] = {kTileNames[index][0], '\0'};
      lv_label_set_text(initial, initialText);
      lv_obj_set_style_text_font(initial, &lv_font_montserrat_20, 0);
      lv_obj_align(initial, LV_ALIGN_CENTER, 0, -15);
    }

    lv_obj_t* label = lv_label_create(tile);
    lv_label_set_text(label, kTileNames[index]);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -7);
  }
}

void initialiseTouch() {
  pinMode(kTouchCs, OUTPUT);
  pinMode(kTouchClock, OUTPUT);
  pinMode(kTouchMosi, OUTPUT);
  pinMode(kTouchMiso, INPUT);
  pinMode(kTouchIrq, INPUT_PULLUP);
  digitalWrite(kTouchCs, HIGH);
  digitalWrite(kTouchClock, LOW);
}

void drawCalibrationTarget(int x, int y, int step, int total) {
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(1);
  display.setCursor(8, 8);
  display.printf("Touch calibration %d/%d", step, total);
  display.setCursor(8, 22);
  display.print("Tap the centre of the target");
  display.drawCircle(x, y, 10, TFT_WHITE);
  display.drawFastHLine(x - 14, y, 29, TFT_WHITE);
  display.drawFastVLine(x, y - 14, 29, TFT_WHITE);
}

bool captureCalibrationPoint(int screenX, int screenY, int step, int& rawX, int& rawY) {
  drawCalibrationTarget(screenX, screenY, step, 5);

  while (digitalRead(kTouchIrq) == LOW) {
    delay(10);
  }
  while (digitalRead(kTouchIrq) == HIGH) {
    delay(10);
  }
  delay(35);

  uint32_t xTotal = 0;
  uint32_t yTotal = 0;
  int validSamples = 0;
  for (int sample = 0; sample < 12; ++sample) {
    int sampleX = 0;
    int sampleY = 0;
    if (readRawTouch(sampleX, sampleY)) {
      xTotal += sampleX;
      yTotal += sampleY;
      ++validSamples;
    }
    delay(5);
  }

  while (digitalRead(kTouchIrq) == LOW) {
    delay(10);
  }
  delay(120);

  if (validSamples < 6) {
    return false;
  }

  rawX = xTotal / validSamples;
  rawY = yTotal / validSamples;
  Serial.printf(
      "[TOUCH] point=%d screen=%d,%d raw=%d,%d samples=%d\n",
      step,
      screenX,
      screenY,
      rawX,
      rawY,
      validSamples);
  return true;
}

int32_t extrapolateStart(int32_t nearValue, int32_t farValue, int nearPosition, int farPosition) {
  return nearValue - ((farValue - nearValue) * nearPosition) / (farPosition - nearPosition);
}

int32_t extrapolateEnd(
    int32_t nearValue,
    int32_t farValue,
    int nearPosition,
    int farPosition,
    int screenEnd) {
  return nearValue + ((farValue - nearValue) * (screenEnd - nearPosition)) /
                         (farPosition - nearPosition);
}

void saveTouchCalibration() {
  Preferences preferences;
  preferences.begin("touch", false);
  preferences.putBool("valid", touchCalibration.valid);
  preferences.putBool("swap", touchCalibration.swapAxes);
  preferences.putInt("xleft", touchCalibration.xAtLeft);
  preferences.putInt("xright", touchCalibration.xAtRight);
  preferences.putInt("ytop", touchCalibration.yAtTop);
  preferences.putInt("ybottom", touchCalibration.yAtBottom);
  preferences.end();
}

bool loadTouchCalibration() {
  Preferences preferences;
  preferences.begin("touch", false);
  touchCalibration.valid = preferences.getBool("valid", false);
  touchCalibration.swapAxes = preferences.getBool("swap", false);
  touchCalibration.xAtLeft = preferences.getInt("xleft", 0);
  touchCalibration.xAtRight = preferences.getInt("xright", 0);
  touchCalibration.yAtTop = preferences.getInt("ytop", 0);
  touchCalibration.yAtBottom = preferences.getInt("ybottom", 0);
  preferences.end();

  return touchCalibration.valid &&
         abs(touchCalibration.xAtRight - touchCalibration.xAtLeft) > 1000 &&
         abs(touchCalibration.yAtBottom - touchCalibration.yAtTop) > 1000;
}

void runTouchCalibration() {
  constexpr int left = 24;
  constexpr int right = 295;
  constexpr int top = 44;
  constexpr int bottom = 215;

  int rawX[5] = {};
  int rawY[5] = {};
  constexpr int screenX[5] = {left, right, right, left, 160};
  constexpr int screenY[5] = {top, top, bottom, bottom, 120};

  display.setTextDatum(TL_DATUM);
  for (int point = 0; point < 5;) {
    if (captureCalibrationPoint(
            screenX[point],
            screenY[point],
            point + 1,
            rawX[point],
            rawY[point])) {
      ++point;
    }
  }

  const int32_t leftRawX = (rawX[0] + rawX[3]) / 2;
  const int32_t rightRawX = (rawX[1] + rawX[2]) / 2;
  const int32_t leftRawY = (rawY[0] + rawY[3]) / 2;
  const int32_t rightRawY = (rawY[1] + rawY[2]) / 2;
  touchCalibration.swapAxes =
      abs(rightRawY - leftRawY) > abs(rightRawX - leftRawX);

  const int32_t rawAtLeft = touchCalibration.swapAxes ? leftRawY : leftRawX;
  const int32_t rawAtRight = touchCalibration.swapAxes ? rightRawY : rightRawX;
  const int32_t topRawX = (rawX[0] + rawX[1]) / 2;
  const int32_t bottomRawX = (rawX[2] + rawX[3]) / 2;
  const int32_t topRawY = (rawY[0] + rawY[1]) / 2;
  const int32_t bottomRawY = (rawY[2] + rawY[3]) / 2;
  const int32_t rawAtTop = touchCalibration.swapAxes ? topRawX : topRawY;
  const int32_t rawAtBottom = touchCalibration.swapAxes ? bottomRawX : bottomRawY;

  touchCalibration.xAtLeft = extrapolateStart(rawAtLeft, rawAtRight, left, right);
  touchCalibration.xAtRight =
      extrapolateEnd(rawAtLeft, rawAtRight, left, right, kScreenWidth - 1);
  touchCalibration.yAtTop = extrapolateStart(rawAtTop, rawAtBottom, top, bottom);
  touchCalibration.yAtBottom =
      extrapolateEnd(rawAtTop, rawAtBottom, top, bottom, kScreenHeight - 1);
  touchCalibration.valid = true;

  int mappedCenterX = 0;
  int mappedCenterY = 0;
  mapTouchCoordinates(rawX[4], rawY[4], mappedCenterX, mappedCenterY);
  const bool centreIsValid =
      abs(mappedCenterX - 160) <= 25 && abs(mappedCenterY - 120) <= 25;

  Serial.printf(
      "[TOUCH] swap=%s x=%ld..%ld y=%ld..%ld centre=%d,%d result=%s\n",
      touchCalibration.swapAxes ? "yes" : "no",
      touchCalibration.xAtLeft,
      touchCalibration.xAtRight,
      touchCalibration.yAtTop,
      touchCalibration.yAtBottom,
      mappedCenterX,
      mappedCenterY,
      centreIsValid ? "accepted" : "retry");

  if (!centreIsValid) {
    touchCalibration.valid = false;
    runTouchCalibration();
    return;
  }

  saveTouchCalibration();
  display.fillScreen(TFT_BLACK);
}

void initialiseStorage() {
  sdSpi.begin(18, 19, 23, kSdCs);
  sdReady = SD.begin(kSdCs, sdSpi, 20000000);
  Serial.printf("[SD] %s\n", sdReady ? "Mounted" : "Not available");
}

bool createExampleIfMissing(const char* path, const char* contents) {
  if (SD.exists(path)) {
    Serial.printf("[SD] Kept existing %s\n", path);
    return true;
  }

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("[SD] Could not create %s\n", path);
    return false;
  }

  const size_t expected = strlen(contents);
  const size_t written = file.print(contents);
  file.close();
  if (written != expected) {
    Serial.printf(
        "[SD] Incomplete write for %s: %u/%u bytes\n",
        path,
        static_cast<unsigned>(written),
        static_cast<unsigned>(expected));
    SD.remove(path);
    return false;
  }

  Serial.printf("[SD] Created %s\n", path);
  return true;
}

bool createIconIfMissing(const DefaultIconAsset& asset) {
  const String path = String("/dashboard/icons/") + asset.filename;
  if (SD.exists(path)) {
    Serial.printf("[SD] Kept existing %s\n", path.c_str());
    return true;
  }
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("[SD] Could not create %s\n", path.c_str());
    return false;
  }
  const size_t written = file.write(asset.data, asset.size);
  file.close();
  if (written != asset.size) {
    SD.remove(path);
    Serial.printf(
        "[SD] Incomplete write for %s: %u/%u bytes\n",
        path.c_str(),
        static_cast<unsigned>(written),
        static_cast<unsigned>(asset.size));
    return false;
  }
  Serial.printf("[SD] Created %s\n", path.c_str());
  return true;
}

void recoverPortalTransaction() {
  constexpr const char* markerPath = "/dashboard/portal.transaction";
  if (!SD.exists(markerPath)) {
    return;
  }

  File marker = SD.open(markerPath, FILE_READ);
  char flags[2] = {};
  const bool valid =
      marker &&
      marker.read(reinterpret_cast<uint8_t*>(flags), 2) == 2 &&
      (flags[0] == '0' || flags[0] == '1') &&
      (flags[1] == '0' || flags[1] == '1');
  marker.close();

  constexpr const char* livePaths[] = {
      "/dashboard/config.json",
      "/dashboard/connections.json",
  };
  constexpr const char* stagedPaths[] = {
      "/dashboard/config.portal-new.json",
      "/dashboard/connections.portal-new.json",
  };
  constexpr const char* backupPaths[] = {
      "/dashboard/config.portal-backup.json",
      "/dashboard/connections.portal-backup.json",
  };

  if (!valid) {
    for (const char* stagedPath : stagedPaths) {
      SD.remove(stagedPath);
    }
    SD.remove(markerPath);
    Serial.println("[SD] Discarded an incomplete transaction marker");
    return;
  }

  for (size_t index = 0; index < 2; ++index) {
    const bool hadLiveFile = flags[index] == '1';
    if (hadLiveFile && SD.exists(backupPaths[index])) {
      SD.remove(livePaths[index]);
      SD.rename(backupPaths[index], livePaths[index]);
    } else if (!hadLiveFile) {
      SD.remove(livePaths[index]);
      SD.remove(backupPaths[index]);
    }
    SD.remove(stagedPaths[index]);
  }
  SD.remove(markerPath);
  Serial.println("[SD] Rolled back an interrupted portal save");
}

void recoverPortalFile(
    const char* livePath,
    const char* stagedPath,
    const char* backupPath) {
  if (SD.exists(livePath)) {
    JsonDocument liveDocument;
    if (loadJsonFile(livePath, liveDocument)) {
      if (SD.exists(stagedPath)) {
        SD.remove(stagedPath);
      }
      if (SD.exists(backupPath)) {
        SD.remove(backupPath);
      }
      return;
    }

    if (SD.exists(backupPath)) {
      SD.remove(livePath);
      if (SD.rename(backupPath, livePath)) {
        Serial.printf("[SD] Restored %s from portal backup\n", livePath);
      }
    }
    return;
  }

  if (SD.exists(backupPath) && SD.rename(backupPath, livePath)) {
    SD.remove(stagedPath);
    Serial.printf("[SD] Restored missing %s from portal backup\n", livePath);
    return;
  }

  if (SD.exists(stagedPath)) {
    JsonDocument stagedDocument;
    if (loadJsonFile(stagedPath, stagedDocument) &&
        SD.rename(stagedPath, livePath)) {
      Serial.printf("[SD] Recovered %s from validated portal staging\n", livePath);
    }
  }
}

void ensureSdScaffold() {
  if (!sdReady) {
    return;
  }

  if (!SD.exists("/dashboard") && !SD.mkdir("/dashboard")) {
    Serial.println("[SD] Could not create /dashboard");
    return;
  }
  if (!SD.exists("/dashboard/icons") && !SD.mkdir("/dashboard/icons")) {
    Serial.println("[SD] Could not create /dashboard/icons");
    return;
  }

  recoverPortalTransaction();
  recoverPortalFile(
      "/dashboard/config.json",
      "/dashboard/config.portal-new.json",
      "/dashboard/config.portal-backup.json");
  recoverPortalFile(
      "/dashboard/connections.json",
      "/dashboard/connections.portal-new.json",
      "/dashboard/connections.portal-backup.json");

  createExampleIfMissing("/dashboard/config.example.v1.json", kConfigExample);
  createExampleIfMissing(
      "/dashboard/connections.example.v1.json",
      kConnectionsExample);
  for (const DefaultIconAsset& asset : kDefaultIconAssets) {
    createIconIfMissing(asset);
  }
}

void initialiseDisplay() {
  ledcSetup(kBacklightChannel, 5000, 8);
  ledcAttachPin(kBacklightPin, kBacklightChannel);
  setBacklight(brightnessPercent);

  Preferences preferences;
  preferences.begin("display", false);
  displayRotation = preferences.getUChar("rotation", 1);
  preferences.end();
  if (displayRotation != 1 && displayRotation != 3) {
    displayRotation = 1;
  }

  display.init();
  display.setRotation(displayRotation);
  display.setColorDepth(16);
  display.setSwapBytes(true);
  display.fillScreen(TFT_BLACK);

  lv_init();
  static lv_fs_drv_t sdFileSystemDriver;
  lv_fs_drv_init(&sdFileSystemDriver);
  sdFileSystemDriver.letter = 'S';
  sdFileSystemDriver.open_cb = lvglSdOpen;
  sdFileSystemDriver.close_cb = lvglSdClose;
  sdFileSystemDriver.read_cb = lvglSdRead;
  sdFileSystemDriver.seek_cb = lvglSdSeek;
  sdFileSystemDriver.tell_cb = lvglSdTell;
  lv_fs_drv_register(&sdFileSystemDriver);

  lv_disp_draw_buf_init(&drawBuffer, bufferOne, bufferTwo, kScreenWidth * 10);

  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = kScreenWidth;
  displayDriver.ver_res = kScreenHeight;
  displayDriver.flush_cb = displayFlush;
  displayDriver.draw_buf = &drawBuffer;
  lv_disp_drv_register(&displayDriver);

  lv_indev_drv_init(&inputDriver);
  inputDriver.type = LV_INDEV_TYPE_POINTER;
  inputDriver.read_cb = touchRead;
  lv_indev_drv_register(&inputDriver);
}

bool drawStartupFile(const char* path) {
  File image = SD.open(path, FILE_READ);
  uint16_t width = 0;
  uint16_t height = 0;
  const bool valid =
      image && image.size() <= 200U * 1024U &&
      jpegDimensions(image, width, height) &&
      width == kScreenWidth && height == kScreenHeight;
  image.close();
  return valid &&
         display.drawJpgFile(
             SD,
             path,
             0,
             0,
             kScreenWidth,
             kScreenHeight);
}

void showStartupScreen() {
  bool drawn = false;
  constexpr const char* customPath = "/dashboard/startup.jpg";
  constexpr const char* backupPath =
      "/dashboard/startup.portal-backup.jpg";
  if (sdReady && SD.exists(customPath)) {
    drawn = drawStartupFile(customPath);
  }
  if (drawn && SD.exists(backupPath)) {
    SD.remove(backupPath);
  } else if (!drawn && SD.exists(backupPath)) {
    SD.remove(customPath);
    if (SD.rename(backupPath, customPath)) {
      drawn = drawStartupFile(customPath);
    }
  }
  if (!drawn) {
    display.drawJpg(
        kDefaultStartupJpeg,
        sizeof(kDefaultStartupJpeg),
        0,
        0,
        kScreenWidth,
        kScreenHeight);
  }

  display.setTextDatum(textdatum_t::bottom_center);
  display.setTextColor(0x632C);
  display.setTextSize(1);
  display.drawString("Starting...", kScreenWidth / 2, kScreenHeight - 6);
  delay(1200);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("[BOOT] ESP32 Dashboard first-run firmware");

  sdMutex = xSemaphoreCreateMutex();
  if (sdMutex == nullptr) {
    Serial.println("[BOOT] Could not create SD mutex");
    while (true) {
      delay(1000);
    }
  }
  liveDataSetStorageMutex(sdMutex);
  initialiseTouch();
  initialiseStorage();
  ensureSdScaffold();
  initialiseDisplay();
  showStartupScreen();
  liveDataBegin();

  const bool forceCalibration = digitalRead(kTouchIrq) == LOW;
  if (forceCalibration || !loadTouchCalibration()) {
    Serial.println("[TOUCH] Starting calibration");
    runTouchCalibration();
  } else {
    Serial.printf(
        "[TOUCH] Loaded calibration swap=%s x=%ld..%ld y=%ld..%ld\n",
        touchCalibration.swapAxes ? "yes" : "no",
        touchCalibration.xAtLeft,
        touchCalibration.xAtRight,
        touchCalibration.yAtTop,
        touchCalibration.yAtBottom);
  }

  loadConfiguration();
  initialisePortalCredentials();
  startWifi();
  showHome();

  Serial.printf("[BOOT] Heap free: %u bytes\n", ESP.getFreeHeap());
  Serial.println("[BOOT] Ready");
}

void loop() {
  lv_timer_handler();
  if (pendingNavigation != PendingNavigation::none) {
    const PendingNavigation navigation = pendingNavigation;
    pendingNavigation = PendingNavigation::none;
    if (navigation == PendingNavigation::home) {
      showHome();
    } else if (navigation == PendingNavigation::settings) {
      showSettings();
    } else if (navigation == PendingNavigation::flights) {
      showFlights();
    } else if (navigation == PendingNavigation::aircraft) {
      showAircraftDetail(selectedAircraft);
    } else if (navigation == PendingNavigation::systems) {
      showSystems();
    } else if (navigation == PendingNavigation::calendar) {
      showStaticCalendar();
    }
  }
  if (WiFi.status() != WL_CONNECTED &&
      !setupAccessPointStarted &&
      !wifiStarted && setupProvisioningAllowed) {
    startSetupAccessPoint();
  }
  startPortalWhenReady();
  startClockWhenReady();
  if (setupAccessPointStarted) {
    setupDnsServer.processNextRequest();
  }
  if (portalStarted) {
    settingsServer.handleClient();
  }

  if (restartPending && static_cast<int32_t>(millis() - restartAt) >= 0) {
    ESP.restart();
  }

  if (millis() - lastStatusUpdate >= 1000) {
    lastStatusUpdate = millis();
    updateStatusBar();
  }

  if (millis() - lastLiveUiUpdate >= 500) {
    lastLiveUiUpdate = millis();
    renderWeatherPage();
    renderFlightsPage();
    renderBambuddyPage();
    renderSystemsPage();

    if (WiFi.status() == WL_CONNECTED) {
      {
        const WeatherData weather = liveDataWeatherSnapshot();
        if (tileEnabled[0] && weather.state != LiveDataState::loading &&
            (weather.state == LiveDataState::idle ||
             millis() - weather.updatedAt >= weatherRefreshMilliseconds)) {
          liveDataRequestWeather();
        }
      }

      {
        const FlightsData flights = liveDataFlightsSnapshot();
        const uint32_t flightsRetry =
            flights.state == LiveDataState::error
                ? 15000UL
                : flightsRefreshMilliseconds;
        if (tileEnabled[1] && flights.state != LiveDataState::loading &&
            (flights.state == LiveDataState::idle ||
             millis() - flights.updatedAt >= flightsRetry)) {
          liveDataRequestFlights();
        }
      }

      {
        const BambuddyData bambuddy = liveDataBambuddySnapshot();
        const uint32_t bambuddyRefresh =
            currentPage == "Bambuddy"
                ? bambuddyVisibleRefreshMilliseconds
                : bambuddyBackgroundRefreshMilliseconds;
        if (tileEnabled[2] && bambuddy.state != LiveDataState::loading &&
            (bambuddy.state == LiveDataState::idle ||
             millis() - bambuddy.updatedAt >= bambuddyRefresh)) {
          liveDataRequestBambuddy();
        }
      }

      {
        const SystemsData systems = liveDataSystemsSnapshot();
        const uint32_t systemsRetry =
            systems.state == LiveDataState::error
                ? 15000UL
                : systemsRefreshMilliseconds;
        if (tileEnabled[3] && systems.state != LiveDataState::loading &&
            (systems.state == LiveDataState::idle ||
             millis() - systems.updatedAt >= systemsRetry)) {
          liveDataRequestSystems();
        }
      }

    }
  }

  if (millis() - lastSerialStatus >= 10000) {
    lastSerialStatus = millis();
    Serial.printf(
        "[STATUS] uptime=%lus heap=%u stack=%u sd=%s config=%s wifi=%s page=%s\n",
        millis() / 1000,
        ESP.getFreeHeap(),
        uxTaskGetStackHighWaterMark(nullptr),
        sdReady ? "ready" : "missing",
        configReady ? "loaded" : "fallback",
        WiFi.status() == WL_CONNECTED ? "online" : "offline",
        currentPage.c_str());
  }

  delay(5);
}
