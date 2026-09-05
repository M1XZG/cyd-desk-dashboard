#pragma once

#include <Arduino.h>
#include <freertos/semphr.h>

enum class LiveDataState : uint8_t {
  idle,
  loading,
  ready,
  error,
};

constexpr size_t kMaximumSystemMonitors = 4;
constexpr size_t kMaximumBambuddyAmsUnits = 4;
constexpr size_t kMaximumBambuddyAmsTrays = 4;

struct SystemMonitorSettings {
  char name[25] = {};
  char type[6] = "http";
  char host[65] = {};
  char path[65] = "/";
  uint16_t port = 80;
  bool enabled = false;
};

struct LiveDataSettings {
  char locationSearch[101] = {};
  char temperatureUnit[12] = "celsius";
  char windUnit[8] = "kmh";
  char pressureUnit[8] = "hpa";
  char flightsProvider[24] = "airplanes.live";
  char flightsApiToken[257] = {};
  char bambuddyProtocol[6] = "http";
  char bambuddyHost[129] = {};
  char bambuddyApiPath[65] = "/api/v1";
  char bambuddyApiKey[257] = {};
  char timeFormat[4] = "24h";
  uint16_t flightsRadiusKm = 100;
  uint8_t flightsMaximumAircraft = 12;
  uint32_t flightsMinimumAltitudeFt = 0;
  uint16_t bambuddyPort = 9001;
  uint16_t bambuddyPrinterId = 1;
  SystemMonitorSettings systemMonitors[kMaximumSystemMonitors];
};

struct WeatherData {
  LiveDataState state = LiveDataState::idle;
  char error[97] = {};
  char location[65] = {};
  char condition[33] = {};
  char provider[17] = "Open-Meteo";
  char observedTime[17] = {};
  char sunrise[6] = {};
  char sunset[6] = {};
  char moonPhase[25] = {};
  char temperatureSuffix[4] = "C";
  char windSuffix[8] = "km/h";
  char pressureSuffix[8] = "hPa";
  float temperature = 0;
  float feelsLike = 0;
  float high = 0;
  float low = 0;
  float windSpeed = 0;
  float pressure = 0;
  float latitude = 0;
  float longitude = 0;
  uint8_t humidity = 0;
  uint8_t moonIllumination = 0;
  int32_t utcOffsetSeconds = 0;
  bool isDay = false;
  uint32_t updatedAt = 0;
};

struct BambuddyAmsTray {
  char colour[9] = {};
  char material[17] = {};
  char name[33] = {};
  uint8_t id = 0;
  uint8_t remainingPercent = 0;
  bool exists = false;
  bool loaded = false;
};

struct BambuddyAmsUnit {
  BambuddyAmsTray trays[kMaximumBambuddyAmsTrays];
  int16_t humidity = -1;
  float temperature = 0;
  uint8_t id = 0;
  uint8_t trayCount = 0;
  bool hasTemperature = false;
  bool isHighTemperature = false;
};

struct BambuddyData {
  LiveDataState state = LiveDataState::idle;
  char error[97] = {};
  char printerName[65] = {};
  char printerState[25] = {};
  char printName[97] = {};
  bool connected = false;
  float progress = 0;
  int remainingMinutes = 0;
  int currentLayer = 0;
  int totalLayers = 0;
  float nozzleTemperature = 0;
  float nozzleTarget = 0;
  float bedTemperature = 0;
  float bedTarget = 0;
  int wifiSignal = 0;
  BambuddyAmsUnit amsUnits[kMaximumBambuddyAmsUnits];
  uint8_t amsUnitCount = 0;
  bool amsExists = false;
  uint32_t updatedAt = 0;
};

struct BambuddyCameraData {
  LiveDataState state = LiveDataState::idle;
  char imagePath[49] = {};
  char error[97] = {};
  uint32_t updatedAt = 0;
};

constexpr size_t kMaximumTrackedAircraft = 20;

struct AircraftData {
  char hex[9] = {};
  char flight[13] = {};
  char registration[13] = {};
  char aircraftType[9] = {};
  char description[41] = {};
  char origin[5] = {};
  char destination[5] = {};
  float latitude = 0;
  float longitude = 0;
  float distanceKm = 0;
  float bearing = 0;
  float altitudeFt = 0;
  float groundSpeedKnots = 0;
  float track = 0;
  float verticalRate = 0;
  bool onGround = false;
};

struct FlightsData {
  LiveDataState state = LiveDataState::idle;
  char error[97] = {};
  char provider[24] = {};
  char location[65] = {};
  float centreLatitude = 0;
  float centreLongitude = 0;
  uint16_t radiusKm = 100;
  uint16_t totalAircraft = 0;
  uint8_t aircraftCount = 0;
  AircraftData aircraft[kMaximumTrackedAircraft];
  uint32_t updatedAt = 0;
};

struct AircraftPhotoData {
  LiveDataState state = LiveDataState::idle;
  char aircraftHex[9] = {};
  char photographer[65] = {};
  char imagePath[49] = {};
  char sourceLink[129] = {};
  char error[97] = {};
  uint32_t updatedAt = 0;
};

struct SystemMonitorData {
  char name[25] = {};
  char detail[33] = {};
  bool configured = false;
  bool online = false;
  uint16_t responseMilliseconds = 0;
};

struct SystemsData {
  LiveDataState state = LiveDataState::idle;
  char error[97] = {};
  char gateway[16] = {};
  bool dnsOnline = false;
  bool internetOnline = false;
  uint16_t internetMilliseconds = 0;
  SystemMonitorData monitors[kMaximumSystemMonitors];
  uint32_t updatedAt = 0;
};

void liveDataBegin();
void liveDataSetStorageMutex(SemaphoreHandle_t mutex);
void liveDataSetNetworkMutex(SemaphoreHandle_t mutex);
void liveDataConfigure(const LiveDataSettings& settings);
bool liveDataRequestWeather();
bool liveDataRequestFlights();
bool liveDataRequestAircraftPhoto(const char* aircraftHex);
bool liveDataRequestBambuddy();
bool liveDataRequestBambuddyCamera();
bool liveDataRequestSystems();
WeatherData liveDataWeatherSnapshot();
FlightsData liveDataFlightsSnapshot();
AircraftPhotoData liveDataAircraftPhotoSnapshot();
BambuddyData liveDataBambuddySnapshot();
BambuddyCameraData liveDataBambuddyCameraSnapshot();
SystemsData liveDataSystemsSnapshot();
