#include "live_data.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_sntp.h>
#include <math.h>
#include <time.h>

namespace {

constexpr size_t kMaximumCalendarBytes = 4U * 1024U * 1024U;

constexpr char kIsrgRootX1[] = R"pem(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)pem";

constexpr char kGtsRootR4[] = R"pem(
-----BEGIN CERTIFICATE-----
MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD
VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG
A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw
WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz
IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi
AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi
QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR
HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW
BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D
9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8
p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD
-----END CERTIFICATE-----
)pem";

constexpr char kGtsRootR1[] = R"pem(
-----BEGIN CERTIFICATE-----
MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw
CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU
MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw
MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp
Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA
A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo
27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w
Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw
TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl
qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH
szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8
Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk
MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92
wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p
aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN
VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID
AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E
FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb
C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe
QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy
h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4
7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J
ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef
MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/
Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT
6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ
0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm
2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb
bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c
-----END CERTIFICATE-----
)pem";

constexpr char kHaricaTlsRsaRoot2021[] = R"pem(
-----BEGIN CERTIFICATE-----
MIIGwDCCBKigAwIBAgIQKmCG1NTeRcleS5j7vy+/JjANBgkqhkiG9w0BAQsFADCB
pjELMAkGA1UEBhMCR1IxDzANBgNVBAcTBkF0aGVuczFEMEIGA1UEChM7SGVsbGVu
aWMgQWNhZGVtaWMgYW5kIFJlc2VhcmNoIEluc3RpdHV0aW9ucyBDZXJ0LiBBdXRo
b3JpdHkxQDA+BgNVBAMTN0hlbGxlbmljIEFjYWRlbWljIGFuZCBSZXNlYXJjaCBJ
bnN0aXR1dGlvbnMgUm9vdENBIDIwMTUwHhcNMjEwOTAyMDc0MTU1WhcNMjkwODMx
MDc0MTU0WjBsMQswCQYDVQQGEwJHUjE3MDUGA1UECgwuSGVsbGVuaWMgQWNhZGVt
aWMgYW5kIFJlc2VhcmNoIEluc3RpdHV0aW9ucyBDQTEkMCIGA1UEAwwbSEFSSUNB
IFRMUyBSU0EgUm9vdCBDQSAyMDIxMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIIC
CgKCAgEAi8Lnr2WbBWeWyQ0kudAOZPzO4iQYLIR/d1HLBBE2uF7taXGnnuQlCZdn
wUfCz5EWNmI9OAThUYL/rNK0ad0u7BGjRe5raztMv4yNpB6dEbnpOPl6DgyY4iMd
0U5j1Oe4QUT7a69r2h/TxZGIW6SJktGB5ow5WKDWaUOprZhSWG7bCvtrz2j646Re
OkVzmAfqXwJy3gyls5+uqR23HbP8ilnnbnJlrfUwlCMH84IWSzWYnFO7L8rkWtnH
jR38mJn7LKSCa/AqH44LX3FcXK5CeymJgcsDo5nKiJ4LQAlBM9vmWHr9rplwwFoP
1hOGcS92afyQ3dstbtHym/Uaa55vFYx68EsooCI4gCRsNqQ78jCR83gTz8E/Navx
HREjtUMingGStxgC5RHRgtsVAMxhN8EqfJrh0LqzUEbugqydMfj7I+IDAEhwowkm
eRVTYPM4XK046oEAYxS5M17dC9ugRQcaMwn4TbSnAqZp9MJZBYhlhVauS8vg3jx9
LRrI6fsfo2FK1ioTrXdMGhibkQ9Y2AZUxZf4qj8giqaFpnf2pvwc4u5ulDMqg1CE
CuVPhvhQRXgAgetbaOMmjcx7XFH0FCxAvhpgHXpyYR0fYy2Iqs6iRZAI/Gu+s1Aq
Wv2oSBhG1pBAkpAKhF5oMfjr7Q3THcZ9mRhVVidlLo1FxSTszuMCAwEAAaOCASEw
ggEdMA8GA1UdEwEB/wQFMAMBAf8wHwYDVR0jBBgwFoAUcRVnyMjJvXVdctA4GGqd
83EkVAswTAYIKwYBBQUHAQEEQDA+MDwGCCsGAQUFBzAChjBodHRwOi8vcmVwby5o
YXJpY2EuZ3IvY2VydHMvSGFyaWNhUm9vdENBMjAxNS5jcnQwEQYDVR0gBAowCDAG
BgRVHSAAMB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDATA6BgNVHR8EMzAx
MC+gLaArhilodHRwOi8vY3JsLmhhcmljYS5nci9IYXJpY2FSb290Q0EyMDE1LmNy
bDAdBgNVHQ4EFgQUCkgjpmCkkgoz6pNbxVfqJU29Eu4wDgYDVR0PAQH/BAQDAgGG
MA0GCSqGSIb3DQEBCwUAA4ICAQDAze/rVni3cYGP1BWkxXnIVynqHcC6Cw7kbkXo
XBZAqF9ANQAKoz2YQ+WyzrparKVilMG6p6mdWTKeLy6eQDRhFSu9DrMYr4TBYxIp
2e5ZLQBk96xbG3wSepBCkTDA0MA0NybzM0gF5nTa82BMCED8EQXliWWsgqUorXuk
7msadosX793M42llmAcj1zJkADzlyzi4gcPvIc4skfmz1OW9jxxwyaR/41VYzPVy
w7m0rsL9n3v5PCFJ1zrldYayKq9gOdwOZInNJQgPU6imXEFBZWdqdR+xdL7qHVMk
dzBRoFqnYjr2bonw87Rxk7dAsdzmUUPJTIayzDTdtU78XkOl0FivJWBXDQPiVB2Z
5NdOVPUANDnADA5octuC3YTG2ncpG9aNKM4ggAyr3imdnz5UZdioYkecv2TVJHT2
lXE5PH/ifJs8p9+WFgyzFsW97BUF1nKsZiUWvlyPY9C9PKuhI4OMeTEA99YIRXYj
XDSZq300ko5sXUVKVG2osAR4Sr3wqS5hLGNBfym7FZwsQ+/ja+fenEALAbAiEMpI
9h71lpMpIhdx7rdRznNeSOok/pVMaqvBZ0IvAq+yXb7d8C+tAND0IcUxYyyJqkE7
89RJdcGZO9JMXv4inBFIEuMAMlGPxzcroEx+ehDXyGR0asw7CooV2DCfiEJlIsKz
LcPZew==
-----END CERTIFICATE-----
)pem";

enum class NetworkJob : uint8_t {
  weather,
  flights,
  aircraftPhoto,
  bambuddy,
  systems,
};

LiveDataSettings currentSettings;
WeatherData weatherData;
FlightsData flightsData;
AircraftPhotoData aircraftPhotoData;
BambuddyData bambuddyData;
SystemsData systemsData;
QueueHandle_t jobQueue = nullptr;
SemaphoreHandle_t dataMutex = nullptr;
SemaphoreHandle_t storageMutex = nullptr;
SemaphoreHandle_t networkMutex = nullptr;
bool weatherBusy = false;
bool flightsBusy = false;
bool aircraftPhotoBusy = false;
bool bambuddyBusy = false;
bool systemsBusy = false;
char cachedLocationSearch[101] = {};
char cachedLocationName[65] = {};
float cachedLatitude = 0;
float cachedLongitude = 0;
bool cachedLocationValid = false;
uint32_t openMeteoBlockedUntil = 0;
char requestedAircraftHex[9] = {};

template <size_t Size>
void copyText(char (&destination)[Size], const char* source) {
  strlcpy(destination, source == nullptr ? "" : source, Size);
}

String urlEncode(const char* value) {
  static constexpr char hex[] = "0123456789ABCDEF";
  String encoded;
  for (const uint8_t* cursor = reinterpret_cast<const uint8_t*>(value);
       *cursor != 0;
       ++cursor) {
    const char character = static_cast<char>(*cursor);
    if (isalnum(*cursor) || character == '-' || character == '_' ||
        character == '.' || character == '~') {
      encoded += character;
    } else {
      encoded += '%';
      encoded += hex[*cursor >> 4];
      encoded += hex[*cursor & 0x0F];
    }
  }
  return encoded;
}

bool ensureClock(char* error, size_t errorSize) {
  if (time(nullptr) > 1700000000) {
    return true;
  }

  if (!esp_sntp_enabled()) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
  }
  const uint32_t deadline = millis() + 8000;
  while (time(nullptr) <= 1700000000 &&
         static_cast<int32_t>(millis() - deadline) < 0) {
    delay(100);
  }

  if (time(nullptr) <= 1700000000) {
    strlcpy(error, "Could not synchronize time for HTTPS", errorSize);
    return false;
  }
  return true;
}

class LimitedWriteStream : public Stream {
 public:
  LimitedWriteStream(File& file, size_t limit)
      : file_(file), limit_(limit) {}

  size_t write(uint8_t value) override {
    return write(&value, 1);
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    const size_t available =
        written_ < limit_ ? limit_ - written_ : 0;
    const size_t accepted = min(size, available);
    if (accepted < size) {
      exceeded_ = true;
    }
    const size_t count = accepted > 0 ? file_.write(buffer, accepted) : 0;
    written_ += count;
    return count;
  }

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override { file_.flush(); }
  size_t written() const { return written_; }
  bool exceeded() const { return exceeded_; }

 private:
  File& file_;
  size_t limit_;
  size_t written_ = 0;
  bool exceeded_ = false;
};

class LimitedReadStream : public Stream {
 public:
  LimitedReadStream(Stream& source, size_t limit)
      : source_(source), remaining_(limit) {}

  int available() override {
    return min(source_.available(), static_cast<int>(remaining_));
  }

  int read() override {
    if (remaining_ == 0) {
      return -1;
    }
    const int value = source_.read();
    if (value >= 0) {
      --remaining_;
      ++read_;
    }
    return value;
  }

  int peek() override {
    return remaining_ > 0 ? source_.peek() : -1;
  }

  void flush() override { source_.flush(); }
  size_t write(uint8_t) override { return 0; }
  size_t readCount() const { return read_; }

 private:
  Stream& source_;
  size_t remaining_;
  size_t read_ = 0;
};

bool secureGetJson(
    const String& url,
    const char* rootCertificate,
    JsonDocument& document,
    char* error,
    size_t errorSize,
    int* httpStatus = nullptr) {
  WiFiClientSecure client;
  client.setCACert(rootCertificate);
  client.setHandshakeTimeout(6);

  HTTPClient request;
  request.setConnectTimeout(6000);
  request.setTimeout(8000);
  request.useHTTP10(true);
  if (!request.begin(client, url)) {
    strlcpy(error, "Could not initialize HTTPS request", errorSize);
    return false;
  }

  const int status = request.GET();
  if (httpStatus != nullptr) {
    *httpStatus = status;
  }
  if (status != HTTP_CODE_OK) {
    snprintf(error, errorSize, "Weather service returned HTTP %d", status);
    request.end();
    return false;
  }

  const int length = request.getSize();
  if (length > 20000) {
    strlcpy(error, "Weather response was too large", errorSize);
    request.end();
    return false;
  }

  LimitedReadStream response(request.getStream(), 20001);
  const DeserializationError jsonError =
      deserializeJson(document, response);
  request.end();
  if (response.readCount() > 20000) {
    strlcpy(error, "Weather response was too large", errorSize);
    return false;
  }
  if (jsonError) {
    snprintf(error, errorSize, "Weather JSON: %s", jsonError.c_str());
    return false;
  }
  return true;
}

bool looksLikeUkPostcode(const char* search) {
  size_t alphanumeric = 0;
  for (const char* cursor = search; *cursor != 0; ++cursor) {
    if (isalnum(static_cast<unsigned char>(*cursor))) {
      ++alphanumeric;
    } else if (!isspace(static_cast<unsigned char>(*cursor))) {
      return false;
    }
  }
  return alphanumeric >= 5 && alphanumeric <= 7 &&
         isalpha(static_cast<unsigned char>(search[0]));
}

bool geocodeLocation(
    const LiveDataSettings& settings,
    float& latitude,
    float& longitude,
    char* location,
    size_t locationSize,
    char* error,
    size_t errorSize) {
  if (cachedLocationValid &&
      strcmp(cachedLocationSearch, settings.locationSearch) == 0) {
    latitude = cachedLatitude;
    longitude = cachedLongitude;
    strlcpy(location, cachedLocationName, locationSize);
    return true;
  }

  JsonDocument geocoding;
  const String geocodingUrl =
      "https://geocoding-api.open-meteo.com/v1/search?name=" +
      urlEncode(settings.locationSearch) +
      "&count=1&language=en&format=json";
  if (!secureGetJson(
          geocodingUrl,
          kIsrgRootX1,
          geocoding,
          error,
          errorSize)) {
    return false;
  }
  if (!geocoding["results"][0].isNull()) {
    latitude = geocoding["results"][0]["latitude"] | 0.0f;
    longitude = geocoding["results"][0]["longitude"] | 0.0f;
    const char* name = geocoding["results"][0]["name"] | "";
    const char* country = geocoding["results"][0]["country"] | "";
    snprintf(
        location,
        locationSize,
        "%s%s%s",
        name,
        strlen(country) > 0 ? ", " : "",
        country);
  } else {
    if (!looksLikeUkPostcode(settings.locationSearch)) {
      strlcpy(error, "Location was not found", errorSize);
      return false;
    }

    String compactPostcode;
    for (const char* cursor = settings.locationSearch; *cursor != 0; ++cursor) {
      if (!isspace(static_cast<unsigned char>(*cursor))) {
        compactPostcode += static_cast<char>(toupper(*cursor));
      }
    }

    JsonDocument postcode;
    const String postcodeUrl =
        "https://api.postcodes.io/postcodes/" +
        urlEncode(compactPostcode.c_str());
    if (!secureGetJson(
            postcodeUrl,
            kGtsRootR4,
            postcode,
            error,
            errorSize)) {
      return false;
    }
    if ((postcode["status"] | 0) != 200 || postcode["result"].isNull()) {
      strlcpy(error, "UK postcode was not found", errorSize);
      return false;
    }

    latitude = postcode["result"]["latitude"] | 0.0f;
    longitude = postcode["result"]["longitude"] | 0.0f;
    const char* district = postcode["result"]["admin_district"] | "";
    const char* postcodeName = postcode["result"]["postcode"] | "";
    snprintf(
        location,
        locationSize,
        "%s%s%s",
        district,
        strlen(district) > 0 ? " " : "",
        postcodeName);
  }

  strlcpy(
      cachedLocationSearch,
      settings.locationSearch,
      sizeof(cachedLocationSearch));
  strlcpy(cachedLocationName, location, sizeof(cachedLocationName));
  cachedLatitude = latitude;
  cachedLongitude = longitude;
  cachedLocationValid = true;
  return true;
}

const char* weatherCondition(int code) {
  if (code == 0) return "Clear";
  if (code <= 2) return "Partly cloudy";
  if (code == 3) return "Overcast";
  if (code == 45 || code == 48) return "Fog";
  if (code >= 51 && code <= 57) return "Drizzle";
  if (code >= 61 && code <= 67) return "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Rain showers";
  if (code >= 85 && code <= 86) return "Snow showers";
  if (code >= 95) return "Thunderstorm";
  return "Unknown";
}

const char* metNoWeatherCondition(const char* symbol) {
  if (symbol == nullptr) return "Unknown";
  if (strstr(symbol, "clearsky") != nullptr) return "Clear";
  if (strstr(symbol, "fair") != nullptr ||
      strstr(symbol, "partlycloudy") != nullptr) {
    return "Partly cloudy";
  }
  if (strstr(symbol, "cloudy") != nullptr) return "Overcast";
  if (strstr(symbol, "fog") != nullptr) return "Fog";
  if (strstr(symbol, "thunder") != nullptr) return "Thunderstorm";
  if (strstr(symbol, "snow") != nullptr ||
      strstr(symbol, "sleet") != nullptr) {
    return "Snow";
  }
  if (strstr(symbol, "rain") != nullptr) return "Rain";
  return "Unknown";
}

double julianDay(int year, int month, int day) {
  if (month <= 2) {
    --year;
    month += 12;
  }
  const int century = year / 100;
  const int correction = 2 - century + century / 4;
  return floor(365.25 * (year + 4716)) +
         floor(30.6001 * (month + 1)) + day + correction - 1524.5;
}

void calculateMoon(
    const char* date,
    char* phase,
    size_t phaseSize,
    uint8_t& illumination) {
  int year = 0;
  int month = 0;
  int day = 0;
  if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3) {
    strlcpy(phase, "Unknown", phaseSize);
    illumination = 0;
    return;
  }

  constexpr double synodicMonth = 29.530588853;
  double age = fmod(julianDay(year, month, day) - 2451550.1, synodicMonth);
  if (age < 0) {
    age += synodicMonth;
  }
  const double angle = age / synodicMonth * 2.0 * PI;
  illumination = static_cast<uint8_t>(
      constrain(lround((1.0 - cos(angle)) * 50.0), 0L, 100L));
  constexpr const char* phases[] = {
      "New Moon",
      "Waxing Crescent",
      "First Quarter",
      "Waxing Gibbous",
      "Full Moon",
      "Waning Gibbous",
      "Last Quarter",
      "Waning Crescent",
  };
  const int phaseIndex =
      static_cast<int>(floor(age / synodicMonth * 8.0 + 0.5)) % 8;
  strlcpy(phase, phases[phaseIndex], phaseSize);
}

bool fetchSolarData(
    float latitude,
    float longitude,
    const char* date,
    WeatherData& result) {
  JsonDocument solar;
  const String url =
      "https://api.sunrise-sunset.org/v2?lat=" +
      String(latitude, 4) + "&lng=" + String(longitude, 4) +
      "&date=" + urlEncode(date);
  char error[97] = {};
  if (!secureGetJson(
          url,
          kGtsRootR4,
          solar,
          error,
          sizeof(error))) {
    Serial.printf("[WEATHER] Solar data unavailable: %s\n", error);
    return false;
  }

  const String sunrise = String(solar["sunrise"] | "");
  const String sunset = String(solar["sunset"] | "");
  if (sunrise.length() < 16 || sunset.length() < 16) {
    Serial.println("[WEATHER] Solar response had no sunrise or sunset");
    return false;
  }

  copyText(result.sunrise, sunrise.substring(11, 16).c_str());
  copyText(result.sunset, sunset.substring(11, 16).c_str());
  copyText(result.moonPhase, solar["moon_phase"] | result.moonPhase);
  result.moonIllumination = constrain(
      static_cast<int>(round(solar["moon_illumination"] | 0.0f)),
      0,
      100);

  const char* offset = solar["utc_offset"] | "";
  int hours = 0;
  int minutes = 0;
  if (strlen(offset) == 6 &&
      sscanf(offset + 1, "%d:%d", &hours, &minutes) == 2) {
    result.utcOffsetSeconds =
        (hours * 60 + minutes) * 60 * (offset[0] == '-' ? -1 : 1);
  }
  Serial.printf(
      "[WEATHER] Solar sunrise=%s sunset=%s\n",
      result.sunrise,
      result.sunset);
  return true;
}

bool fetchMetNoWeather(
    const LiveDataSettings& settings,
    float latitude,
    float longitude,
    WeatherData& result) {
  WiFiClientSecure client;
  client.setCACert(kHaricaTlsRsaRoot2021);
  client.setHandshakeTimeout(6);

  const String url =
      "https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=" +
      String(latitude, 4) + "&lon=" + String(longitude, 4);

  HTTPClient request;
  request.setConnectTimeout(6000);
  request.setTimeout(12000);
  request.useHTTP10(true);
  if (!request.begin(client, url)) {
    copyText(result.error, "Could not initialize backup weather request");
    return false;
  }
  request.addHeader(
      "User-Agent",
      "CYD-Desk-Dashboard/1.0 github.com/M1XZG");
  request.addHeader("Accept", "application/json");

  const int status = request.GET();
  if (status != HTTP_CODE_OK && status != 203) {
    snprintf(
        result.error,
        sizeof(result.error),
        "Backup weather service returned HTTP %d",
        status);
    request.end();
    return false;
  }

  JsonDocument filter;
  filter["properties"]["timeseries"][0]["time"] = true;
  filter["properties"]["timeseries"][0]["data"]["instant"]["details"]
        ["air_temperature"] = true;
  filter["properties"]["timeseries"][0]["data"]["instant"]["details"]
        ["relative_humidity"] = true;
  filter["properties"]["timeseries"][0]["data"]["instant"]["details"]
        ["wind_speed"] = true;
  filter["properties"]["timeseries"][0]["data"]["instant"]["details"]
        ["air_pressure_at_sea_level"] = true;
  filter["properties"]["timeseries"][0]["data"]["next_1_hours"]["summary"]
        ["symbol_code"] = true;
  filter["properties"]["timeseries"][0]["data"]["next_6_hours"]["summary"]
        ["symbol_code"] = true;

  const int expectedBytes = request.getSize();
  if (expectedBytes > 200000) {
    copyText(result.error, "Backup weather response was too large");
    request.end();
    return false;
  }

  constexpr const char* temporaryPath =
      "/dashboard/weather-response.tmp";
  const bool storageLocked =
      storageMutex == nullptr ||
      xSemaphoreTake(storageMutex, pdMS_TO_TICKS(10000)) == pdTRUE;
  if (!storageLocked) {
    copyText(result.error, "SD card is busy");
    request.end();
    return false;
  }
  if (SD.exists(temporaryPath)) {
    SD.remove(temporaryPath);
  }
  File responseFile = SD.open(temporaryPath, FILE_WRITE);
  if (!responseFile) {
    if (storageMutex != nullptr) {
      xSemaphoreGive(storageMutex);
    }
    copyText(result.error, "Could not create the weather cache");
    request.end();
    return false;
  }

  LimitedWriteStream limitedOutput(responseFile, 200000);
  const int transferResult = request.writeToStream(&limitedOutput);
  const size_t bytesWritten = limitedOutput.written();
  responseFile.close();
  request.end();
  client.stop();
  if (limitedOutput.exceeded()) {
    SD.remove(temporaryPath);
    if (storageMutex != nullptr) {
      xSemaphoreGive(storageMutex);
    }
    copyText(result.error, "Backup weather response was too large");
    return false;
  }
  if (transferResult < 0 || bytesWritten == 0 ||
      (expectedBytes > 0 &&
       bytesWritten != static_cast<size_t>(expectedBytes))) {
    SD.remove(temporaryPath);
    if (storageMutex != nullptr) {
      xSemaphoreGive(storageMutex);
    }
    copyText(result.error, "Backup weather download was incomplete");
    return false;
  }

  File responseInput = SD.open(temporaryPath, FILE_READ);
  if (!responseInput) {
    SD.remove(temporaryPath);
    if (storageMutex != nullptr) {
      xSemaphoreGive(storageMutex);
    }
    copyText(result.error, "Could not read the weather cache");
    return false;
  }

  JsonDocument forecast;
  const DeserializationError jsonError = deserializeJson(
      forecast,
      responseInput,
      DeserializationOption::Filter(filter));
  responseInput.close();
  SD.remove(temporaryPath);
  if (storageMutex != nullptr) {
    xSemaphoreGive(storageMutex);
  }
  if (jsonError) {
    snprintf(
        result.error,
        sizeof(result.error),
        "Backup weather JSON: %s",
        jsonError.c_str());
    return false;
  }

  const JsonArray timeseries = forecast["properties"]["timeseries"];
  if (timeseries.isNull() || timeseries.size() == 0) {
    copyText(result.error, "Backup weather response had no observations");
    return false;
  }

  const JsonObject first = timeseries[0];
  const JsonObject details = first["data"]["instant"]["details"];
  float temperature = details["air_temperature"] | 0.0f;
  float high = temperature;
  float low = temperature;
  const size_t values = min(timeseries.size(), static_cast<size_t>(24));
  for (size_t index = 1; index < values; ++index) {
    const float candidate =
        timeseries[index]["data"]["instant"]["details"]["air_temperature"] |
        temperature;
    high = max(high, candidate);
    low = min(low, candidate);
  }

  const char* symbol =
      first["data"]["next_1_hours"]["summary"]["symbol_code"] | nullptr;
  if (symbol == nullptr) {
    symbol =
        first["data"]["next_6_hours"]["summary"]["symbol_code"] | "";
  }

  result.temperature = temperature;
  result.feelsLike = temperature;
  result.high = high;
  result.low = low;
  result.humidity = constrain(
      static_cast<int>(round(details["relative_humidity"] | 0.0f)),
      0,
      100);
  result.windSpeed = (details["wind_speed"] | 0.0f) * 3.6f;
  result.pressure = details["air_pressure_at_sea_level"] | 0.0f;
  result.isDay = strstr(symbol, "_night") == nullptr;
  copyText(result.condition, metNoWeatherCondition(symbol));

  const String observed = String(first["time"] | "");
  copyText(
      result.observedTime,
      observed.length() >= 16 ? observed.substring(0, 16).c_str() : "");
  const String day =
      observed.length() >= 10 ? observed.substring(0, 10) : "";
  calculateMoon(
      day.c_str(),
      result.moonPhase,
      sizeof(result.moonPhase),
      result.moonIllumination);
  copyText(result.sunrise, "--:--");
  copyText(result.sunset, "--:--");

  if (strcmp(settings.temperatureUnit, "fahrenheit") == 0) {
    result.temperature = result.temperature * 1.8f + 32.0f;
    result.feelsLike = result.feelsLike * 1.8f + 32.0f;
    result.high = result.high * 1.8f + 32.0f;
    result.low = result.low * 1.8f + 32.0f;
    copyText(result.temperatureSuffix, "F");
  }
  if (strcmp(settings.windUnit, "mph") == 0) {
    result.windSpeed *= 0.621371f;
    copyText(result.windSuffix, "mph");
  }
  if (strcmp(settings.pressureUnit, "inhg") == 0) {
    result.pressure *= 0.0295299830714f;
    copyText(result.pressureSuffix, "inHg");
  }

  result.state = LiveDataState::ready;
  result.updatedAt = millis();
  copyText(result.provider, "MET Norway");
  Serial.println("[WEATHER] Using MET Norway backup provider");
  return true;
}

void trimText(char* value) {
  size_t length = strlen(value);
  while (length > 0 && isspace(static_cast<unsigned char>(value[length - 1]))) {
    value[--length] = '\0';
  }
  size_t start = 0;
  while (value[start] != '\0' &&
         isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  if (start > 0) {
    memmove(value, value + start, strlen(value + start) + 1);
  }
}

float degreesToRadians(float degrees) {
  return degrees * PI / 180.0f;
}

void distanceAndBearing(
    float fromLatitude,
    float fromLongitude,
    float toLatitude,
    float toLongitude,
    float& distanceKm,
    float& bearing) {
  constexpr float earthRadiusKm = 6371.0f;
  const float fromLat = degreesToRadians(fromLatitude);
  const float toLat = degreesToRadians(toLatitude);
  const float deltaLat = degreesToRadians(toLatitude - fromLatitude);
  const float deltaLon = degreesToRadians(toLongitude - fromLongitude);
  const float a = sinf(deltaLat / 2.0f) * sinf(deltaLat / 2.0f) +
                  cosf(fromLat) * cosf(toLat) *
                      sinf(deltaLon / 2.0f) * sinf(deltaLon / 2.0f);
  distanceKm = earthRadiusKm * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));

  const float y = sinf(deltaLon) * cosf(toLat);
  const float x = cosf(fromLat) * sinf(toLat) -
                  sinf(fromLat) * cosf(toLat) * cosf(deltaLon);
  bearing = fmodf(atan2f(y, x) * 180.0f / PI + 360.0f, 360.0f);
}

void insertNearest(
    FlightsData& result,
    const AircraftData& aircraft,
    uint8_t maximumAircraft) {
  uint8_t position = result.aircraftCount;
  while (position > 0 &&
         result.aircraft[position - 1].distanceKm > aircraft.distanceKm) {
    --position;
  }

  if (position >= maximumAircraft) {
    return;
  }
  const uint8_t newCount =
      min(static_cast<uint8_t>(result.aircraftCount + 1), maximumAircraft);
  for (uint8_t index = newCount - 1; index > position; --index) {
    result.aircraft[index] = result.aircraft[index - 1];
  }
  result.aircraft[position] = aircraft;
  result.aircraftCount = newCount;
}

bool resolveFlightsLocation(
    const LiveDataSettings& settings,
    float& latitude,
    float& longitude,
    char* location,
    size_t locationSize,
    char* error,
    size_t errorSize) {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  const WeatherData cachedWeather = weatherData;
  xSemaphoreGive(dataMutex);
  if (cachedWeather.state == LiveDataState::ready) {
    latitude = cachedWeather.latitude;
    longitude = cachedWeather.longitude;
    strlcpy(location, cachedWeather.location, locationSize);
    return true;
  }
  return geocodeLocation(
      settings,
      latitude,
      longitude,
      location,
      locationSize,
      error,
      errorSize);
}

void processAircraft(
    JsonObject source,
    bool isFlightradar,
    const LiveDataSettings& settings,
    FlightsData& result,
    uint8_t maximumAircraft) {
  if (!source["lat"].is<float>() || !source["lon"].is<float>()) {
    return;
  }

  AircraftData aircraft;
  copyText(aircraft.hex, source["hex"] | "");
  const char* flight = source["flight"] | "";
  if (strlen(flight) == 0) {
    flight = source["callsign"] | "";
  }
  copyText(aircraft.flight, flight);
  copyText(
      aircraft.registration,
      isFlightradar ? (source["reg"] | "") : (source["r"] | ""));
  copyText(
      aircraft.aircraftType,
      isFlightradar ? (source["type"] | "") : (source["t"] | ""));
  copyText(aircraft.description, source["desc"] | "");
  copyText(aircraft.origin, source["orig_iata"] | "");
  copyText(aircraft.destination, source["dest_iata"] | "");
  trimText(aircraft.flight);
  aircraft.latitude = source["lat"] | 0.0f;
  aircraft.longitude = source["lon"] | 0.0f;
  aircraft.onGround =
      source["alt_baro"].is<const char*>() &&
      strcmp(source["alt_baro"] | "", "ground") == 0;
  aircraft.altitudeFt =
      isFlightradar ? (source["alt"] | 0.0f)
                    : (source["alt_baro"] | 0.0f);
  aircraft.groundSpeedKnots =
      isFlightradar ? (source["gspeed"] | 0.0f)
                    : (source["gs"] | 0.0f);
  aircraft.track = source["track"] | 0.0f;
  aircraft.verticalRate =
      isFlightradar ? (source["vspeed"] | 0.0f)
                    : (source["baro_rate"] | 0.0f);
  if (!aircraft.onGround &&
      aircraft.altitudeFt < settings.flightsMinimumAltitudeFt) {
    return;
  }

  distanceAndBearing(
      result.centreLatitude,
      result.centreLongitude,
      aircraft.latitude,
      aircraft.longitude,
      aircraft.distanceKm,
      aircraft.bearing);
  if (aircraft.distanceKm <= settings.flightsRadiusKm * 1.05f) {
    insertNearest(result, aircraft, maximumAircraft);
  }
}

bool parseAircraftFile(
    File& input,
    bool isFlightradar,
    const LiveDataSettings& settings,
    FlightsData& result,
    uint8_t maximumAircraft) {
  const char* arrayMarker = isFlightradar ? "\"data\"" : "\"ac\"";
  if (!input.find(arrayMarker) || !input.find("[")) {
    copyText(result.error, "Aircraft response has no data array");
    return false;
  }

  String objectJson;
  objectJson.reserve(3072);
  JsonDocument document;
  bool inString = false;
  bool escaped = false;
  bool oversized = false;
  int depth = 0;

  while (input.available()) {
    const char character = static_cast<char>(input.read());
    if (depth == 0) {
      if (character == ']') {
        return true;
      }
      if (character != '{') {
        continue;
      }
      objectJson = "{";
      depth = 1;
      inString = false;
      escaped = false;
      oversized = false;
      continue;
    }

    if (!oversized) {
      if (objectJson.length() < 8192) {
        objectJson += character;
      } else {
        oversized = true;
      }
    }

    if (escaped) {
      escaped = false;
      continue;
    }
    if (inString && character == '\\') {
      escaped = true;
      continue;
    }
    if (character == '"') {
      inString = !inString;
      continue;
    }
    if (inString) {
      continue;
    }
    if (character == '{') {
      ++depth;
    } else if (character == '}' && --depth == 0) {
      ++result.totalAircraft;
      if (!oversized) {
        document.clear();
        const DeserializationError error =
            deserializeJson(document, objectJson);
        if (!error) {
          processAircraft(
              document.as<JsonObject>(),
              isFlightradar,
              settings,
              result,
              maximumAircraft);
        }
      }
    }
  }

  copyText(result.error, "Aircraft cache ended unexpectedly");
  return false;
}

class ScopedStorageLock {
 public:
  explicit ScopedStorageLock(TickType_t timeout)
      : locked_(
            storageMutex == nullptr ||
            xSemaphoreTake(storageMutex, timeout) == pdTRUE) {}

  ~ScopedStorageLock() {
    if (locked_ && storageMutex != nullptr) {
      xSemaphoreGive(storageMutex);
    }
  }

  explicit operator bool() const {
    return locked_;
  }

 private:
  bool locked_;
};

void fetchFlights(const LiveDataSettings& settings) {
  FlightsData result;
  result.state = LiveDataState::error;
  result.radiusKm = settings.flightsRadiusKm;
  copyText(result.provider, settings.flightsProvider);

  if (WiFi.status() != WL_CONNECTED) {
    copyText(result.error, "Wi-Fi is offline");
  } else if (strlen(settings.locationSearch) < 2) {
    copyText(result.error, "Set a location in the settings portal");
  } else if (!ensureClock(result.error, sizeof(result.error))) {
  } else if (strcmp(settings.flightsProvider, "flightradar24") == 0 &&
             strlen(settings.flightsApiToken) == 0) {
    copyText(result.error, "Set the Flightradar24 API token");
  } else if (resolveFlightsLocation(
                 settings,
                 result.centreLatitude,
                 result.centreLongitude,
                 result.location,
                 sizeof(result.location),
                 result.error,
                 sizeof(result.error))) {
    const uint8_t maximumAircraft = constrain(
        settings.flightsMaximumAircraft,
        static_cast<uint8_t>(1),
        static_cast<uint8_t>(kMaximumTrackedAircraft));
    String url;
    const char* rootCertificate = kGtsRootR4;
    const bool isFlightradar =
        strcmp(settings.flightsProvider, "flightradar24") == 0;
    if (isFlightradar) {
      const float latitudeDelta = settings.flightsRadiusKm / 111.32f;
      const float longitudeDelta =
          settings.flightsRadiusKm /
          (111.32f *
           max(0.1f, cosf(degreesToRadians(result.centreLatitude))));
      url = "https://fr24api.flightradar24.com/api/live/flight-positions/full"
            "?bounds=" +
            String(result.centreLatitude + latitudeDelta, 5) + "," +
            String(result.centreLatitude - latitudeDelta, 5) + "," +
            String(result.centreLongitude - longitudeDelta, 5) + "," +
            String(result.centreLongitude + longitudeDelta, 5) +
            "&limit=" + String(maximumAircraft);
    } else {
      const bool useAdsbLol =
          strcmp(settings.flightsProvider, "adsb.lol") == 0;
      rootCertificate = useAdsbLol ? kIsrgRootX1 : kGtsRootR4;
      const float radiusNm = min(settings.flightsRadiusKm / 1.852f, 250.0f);
      url = String(
                useAdsbLol
                    ? "https://api.adsb.lol/v2/point/"
                    : "https://api.airplanes.live/v2/point/") +
            String(result.centreLatitude, 5) + "/" +
            String(result.centreLongitude, 5) + "/" +
            String(radiusNm, 1);
    }

    WiFiClientSecure client;
    client.setCACert(rootCertificate);
    client.setHandshakeTimeout(6);
    client.setTimeout(30000);
    HTTPClient request;
    request.setConnectTimeout(6000);
    request.setTimeout(30000);
    request.useHTTP10(true);
    request.setUserAgent("DeskDashboard/1.0");
    if (!request.begin(client, url)) {
      copyText(result.error, "Could not initialize aircraft request");
    } else {
      if (isFlightradar) {
        request.addHeader(
            "Authorization",
            "Bearer " + String(settings.flightsApiToken));
        request.addHeader("Accept-Version", "v1");
      }
      const int status = request.GET();
      Serial.printf(
          "[FLIGHTS] HTTP %d length=%d heap=%u\n",
          status,
          request.getSize(),
          ESP.getFreeHeap());
      if (status != HTTP_CODE_OK) {
        snprintf(
            result.error,
            sizeof(result.error),
            "Aircraft service returned HTTP %d",
            status);
      } else if (request.getSize() > 300000) {
        copyText(result.error, "Aircraft response was too large");
      } else {
        ScopedStorageLock storageLock(pdMS_TO_TICKS(10000));
        constexpr const char* temporaryPath =
            "/dashboard/flights-response.tmp";
        constexpr const char* cachePath =
            "/dashboard/flights-cache.json";
        if (!storageLock) {
          copyText(result.error, "SD card is busy");
        } else if (SD.exists(temporaryPath)) {
          SD.remove(temporaryPath);
        }
        File responseFile =
            storageLock ? SD.open(temporaryPath, FILE_WRITE) : File();
        if (!storageLock) {
        } else if (!responseFile) {
          copyText(result.error, "Could not create the aircraft cache");
        } else {
          const int expectedBytes = request.getSize();
          LimitedWriteStream limitedOutput(responseFile, 300000);
          const int transferResult = request.writeToStream(&limitedOutput);
          const size_t bytesWritten = limitedOutput.written();
          responseFile.close();
          request.end();
          client.stop();
          Serial.printf(
              "[FLIGHTS] Downloaded %u bytes\n",
              static_cast<unsigned>(bytesWritten));
          if (limitedOutput.exceeded()) {
            copyText(result.error, "Aircraft response was too large");
            SD.remove(temporaryPath);
          } else if (transferResult < 0 || bytesWritten == 0 ||
              (expectedBytes > 0 &&
               bytesWritten != static_cast<size_t>(expectedBytes))) {
            copyText(result.error, "Aircraft download was incomplete");
            SD.remove(temporaryPath);
          } else {
            File input = SD.open(temporaryPath, FILE_READ);
            const bool parsed = parseAircraftFile(
                input,
                isFlightradar,
                settings,
                result,
                maximumAircraft);
            input.close();
            if (!parsed) {
              SD.remove(temporaryPath);
            } else {
              if (SD.exists(cachePath)) {
                SD.remove(cachePath);
              }
              if (!SD.rename(temporaryPath, cachePath)) {
                SD.remove(temporaryPath);
              }
              result.state = LiveDataState::ready;
              result.updatedAt = millis();
            }
          }
        }
      }
      request.end();
    }
  }

  if (result.updatedAt == 0) {
    result.updatedAt = millis();
  }
  if (result.state == LiveDataState::ready) {
    Serial.printf(
        "[FLIGHTS] %s returned=%u showing=%u radius=%u km\n",
        result.provider,
        result.totalAircraft,
        result.aircraftCount,
        result.radiusKm);
  } else {
    Serial.printf("[FLIGHTS] Error: %s\n", result.error);
  }
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  flightsData = result;
  flightsBusy = false;
  xSemaphoreGive(dataMutex);
}

void fetchAircraftPhoto() {
  AircraftPhotoData result;
  result.state = LiveDataState::error;

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  copyText(result.aircraftHex, requestedAircraftHex);
  xSemaphoreGive(dataMutex);

  if (WiFi.status() != WL_CONNECTED) {
    copyText(result.error, "Wi-Fi is offline");
  } else if (strlen(result.aircraftHex) < 6) {
    copyText(result.error, "No aircraft identifier is available");
  } else if (!ensureClock(result.error, sizeof(result.error))) {
  } else {
    const String metadataUrl =
        "https://airport-data.com/api/ac_thumb.json?m=" +
        urlEncode(result.aircraftHex) + "&n=1";
    WiFiClientSecure metadataClient;
    // Airport-Data currently uses a P-384 issuer chain that exceeds the
    // no-PSRAM CYD's contiguous heap. The response contains only public photo
    // data and is constrained to exact hosts, bounded sizes, and valid JPEGs.
    metadataClient.setInsecure();
    metadataClient.setHandshakeTimeout(6);
    HTTPClient metadataRequest;
    metadataRequest.setConnectTimeout(6000);
    metadataRequest.setTimeout(15000);
    metadataRequest.setReuse(true);
    metadataRequest.setUserAgent(
        "CYD-Desk-Dashboard/1.1 "
        "(+https://github.com/M1XZG/cyd-desk-dashboard)");
    if (!metadataRequest.begin(metadataClient, metadataUrl)) {
      copyText(result.error, "Could not initialize the photo lookup");
    } else {
      const int metadataStatus = metadataRequest.GET();
      if (metadataStatus == HTTP_CODE_NOT_FOUND) {
        copyText(result.error, "No photo found for this aircraft");
      } else if (metadataStatus != HTTP_CODE_OK) {
        snprintf(
            result.error,
            sizeof(result.error),
            "Aircraft photo service returned HTTP %d",
            metadataStatus);
      } else if (metadataRequest.getSize() > 8192) {
        copyText(result.error, "Aircraft photo metadata was too large");
      } else {
        JsonDocument filter;
        filter["status"] = true;
        filter["count"] = true;
        JsonObject itemFilter = filter["data"].add<JsonObject>();
        itemFilter["image"] = true;
        itemFilter["link"] = true;
        itemFilter["photographer"] = true;
        JsonDocument document;
        const DeserializationError jsonError = deserializeJson(
            document,
            metadataRequest.getStream(),
            DeserializationOption::Filter(filter));
        if (jsonError) {
          snprintf(
              result.error,
              sizeof(result.error),
              "Aircraft photo metadata: %s",
              jsonError.c_str());
        } else if ((document["count"] | 0) < 1) {
          copyText(result.error, "No photo found for this aircraft");
        } else {
          const char* imageUrl = document["data"][0]["image"] | "";
          const char* sourceLink = document["data"][0]["link"] | "";
          const char* photographer =
              document["data"][0]["photographer"] | "";
          constexpr char imagePrefix[] =
              "https://airport-data.com/images/aircraft/thumbnails/";
          constexpr char linkPrefix[] =
              "https://airport-data.com/aircraft/photo/";
          if (strncmp(imageUrl, imagePrefix, strlen(imagePrefix)) != 0 ||
              strncmp(sourceLink, linkPrefix, strlen(linkPrefix)) != 0 ||
              strlen(imageUrl) >= 192 ||
              strlen(sourceLink) >= sizeof(result.sourceLink) ||
              strlen(photographer) >= sizeof(result.photographer)) {
            copyText(result.error, "Aircraft photo metadata was invalid");
          } else {
            String imageUrlCopy(imageUrl);
            copyText(result.photographer, photographer);
            copyText(result.sourceLink, sourceLink);
            if (!metadataRequest.setURL(imageUrlCopy)) {
              copyText(result.error, "Could not prepare the photo download");
            } else {
              const int imageStatus = metadataRequest.GET();
              const int expectedBytes = metadataRequest.getSize();
              if (imageStatus != HTTP_CODE_OK) {
                snprintf(
                    result.error,
                    sizeof(result.error),
                    "Aircraft image returned HTTP %d",
                    imageStatus);
              } else if (expectedBytes > 100U * 1024U) {
                copyText(result.error, "Aircraft image was too large");
              } else {
                ScopedStorageLock storageLock(pdMS_TO_TICKS(10000));
                constexpr char temporaryPath[] =
                    "/dashboard/aircraft-photo.tmp";
                constexpr char imagePath[] =
                    "/dashboard/aircraft-photo.jpg";
                if (!storageLock) {
                  copyText(result.error, "SD card is busy");
                } else {
                  if (SD.exists(temporaryPath)) {
                    SD.remove(temporaryPath);
                  }
                  File output = SD.open(temporaryPath, FILE_WRITE);
                  if (!output) {
                    copyText(result.error, "Could not create the aircraft image");
                  } else {
                    LimitedWriteStream limitedOutput(output, 100U * 1024U);
                    const int transferResult =
                        metadataRequest.writeToStream(&limitedOutput);
                    const size_t bytesWritten = limitedOutput.written();
                    output.close();
                    if (limitedOutput.exceeded() || transferResult < 0 ||
                        bytesWritten < 4 ||
                        (expectedBytes > 0 &&
                         bytesWritten != static_cast<size_t>(expectedBytes))) {
                      SD.remove(temporaryPath);
                      copyText(result.error, "Aircraft image download was incomplete");
                    } else {
                      File validation = SD.open(temporaryPath, FILE_READ);
                      const bool validJpeg =
                          validation &&
                          validation.read() == 0xFF &&
                          validation.read() == 0xD8 &&
                          validation.seek(validation.size() - 2) &&
                          validation.read() == 0xFF &&
                          validation.read() == 0xD9;
                      validation.close();
                      if (!validJpeg) {
                        SD.remove(temporaryPath);
                        copyText(result.error, "Aircraft image was not a valid JPEG");
                      } else {
                        if (SD.exists(imagePath)) {
                          SD.remove(imagePath);
                        }
                        if (!SD.rename(temporaryPath, imagePath)) {
                          SD.remove(temporaryPath);
                          copyText(result.error, "Could not save the aircraft image");
                        } else {
                          copyText(result.imagePath, imagePath);
                          result.state = LiveDataState::ready;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      metadataRequest.end();
      metadataClient.stop();
    }
  }

  result.updatedAt = millis();
  Serial.printf(
      "[AIRCRAFT PHOTO] %s: %s\n",
      result.aircraftHex,
      result.state == LiveDataState::ready ? result.photographer : result.error);
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  aircraftPhotoData = result;
  aircraftPhotoBusy = false;
  xSemaphoreGive(dataMutex);
}

void fetchWeather(const LiveDataSettings& settings) {
  WeatherData result;
  result.state = LiveDataState::error;

  if (WiFi.status() != WL_CONNECTED) {
    copyText(result.error, "Wi-Fi is offline");
  } else if (strlen(settings.locationSearch) < 2) {
    copyText(result.error, "Set a location in the settings portal");
  } else if (!ensureClock(result.error, sizeof(result.error))) {
  } else {
    float latitude = 0;
    float longitude = 0;
    if (geocodeLocation(
            settings,
            latitude,
            longitude,
            result.location,
            sizeof(result.location),
            result.error,
            sizeof(result.error))) {
      result.latitude = latitude;
      result.longitude = longitude;
      const bool openMeteoBlocked =
          openMeteoBlockedUntil != 0 &&
          static_cast<int32_t>(millis() - openMeteoBlockedUntil) < 0;
      int forecastStatus = 0;
      bool forecastReady = false;
      JsonDocument forecast;
      if (!openMeteoBlocked) {
        const String url =
            "https://api.open-meteo.com/v1/forecast?latitude=" +
            String(latitude, 5) + "&longitude=" + String(longitude, 5) +
            "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
            "weather_code,wind_speed_10m,surface_pressure,is_day"
            "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset"
            "&timezone=auto&forecast_days=1&temperature_unit=" +
            String(settings.temperatureUnit) + "&wind_speed_unit=" +
            String(settings.windUnit);
        forecastReady = secureGetJson(
            url,
            kIsrgRootX1,
            forecast,
            result.error,
            sizeof(result.error),
            &forecastStatus);
      }

      if (forecastReady) {
        result.temperature = forecast["current"]["temperature_2m"] | 0.0f;
        result.feelsLike =
            forecast["current"]["apparent_temperature"] | 0.0f;
        result.humidity =
            constrain(forecast["current"]["relative_humidity_2m"] | 0, 0, 100);
        result.windSpeed = forecast["current"]["wind_speed_10m"] | 0.0f;
        result.pressure = forecast["current"]["surface_pressure"] | 0.0f;
        result.isDay = (forecast["current"]["is_day"] | 0) == 1;
        result.utcOffsetSeconds =
            forecast["utc_offset_seconds"] | 0;
        const int code = forecast["current"]["weather_code"] | -1;
        copyText(result.condition, weatherCondition(code));
        copyText(
            result.observedTime,
            forecast["current"]["time"] | "");
        result.high = forecast["daily"]["temperature_2m_max"][0] | 0.0f;
        result.low = forecast["daily"]["temperature_2m_min"][0] | 0.0f;

        const String sunrise = String(forecast["daily"]["sunrise"][0] | "");
        const String sunset = String(forecast["daily"]["sunset"][0] | "");
        copyText(
            result.sunrise,
            sunrise.length() >= 16 ? sunrise.substring(11, 16).c_str() : "--:--");
        copyText(
            result.sunset,
            sunset.length() >= 16 ? sunset.substring(11, 16).c_str() : "--:--");

        const char* day = forecast["daily"]["time"][0] | "";
        calculateMoon(
            day,
            result.moonPhase,
            sizeof(result.moonPhase),
            result.moonIllumination);

        if (strcmp(settings.temperatureUnit, "fahrenheit") == 0) {
          copyText(result.temperatureSuffix, "F");
        }
        if (strcmp(settings.windUnit, "mph") == 0) {
          copyText(result.windSuffix, "mph");
        }
        if (strcmp(settings.pressureUnit, "inhg") == 0) {
          result.pressure *= 0.0295299830714f;
          copyText(result.pressureSuffix, "inHg");
        }

        result.state = LiveDataState::ready;
        result.updatedAt = millis();
        copyText(result.provider, "Open-Meteo");
        openMeteoBlockedUntil = 0;
      } else if (openMeteoBlocked || forecastStatus == 429) {
        if (forecastStatus == 429) {
          openMeteoBlockedUntil = millis() + 6UL * 60UL * 60UL * 1000UL;
        }
        if (fetchMetNoWeather(settings, latitude, longitude, result)) {
          char day[11] = {};
          strlcpy(day, result.observedTime, sizeof(day));
          fetchSolarData(latitude, longitude, day, result);
        }
      }
    }
  }

  if (result.updatedAt == 0) {
    result.updatedAt = millis();
  }
  if (result.state == LiveDataState::ready) {
    Serial.printf(
        "[WEATHER] %s %.1f %s, %s\n",
        result.location,
        result.temperature,
        result.temperatureSuffix,
        result.condition);
  } else {
    Serial.printf("[WEATHER] Error: %s\n", result.error);
  }
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  weatherData = result;
  weatherBusy = false;
  xSemaphoreGive(dataMutex);
}

void fetchBambuddy(const LiveDataSettings& settings) {
  BambuddyData result;
  result.state = LiveDataState::error;

  if (WiFi.status() != WL_CONNECTED) {
    copyText(result.error, "Wi-Fi is offline");
  } else if (strlen(settings.bambuddyHost) == 0) {
    copyText(result.error, "Set the Bambuddy host in the portal");
  } else if (strlen(settings.bambuddyApiKey) == 0) {
    copyText(result.error, "Set the Bambuddy read-only key");
  } else if (strcmp(settings.bambuddyProtocol, "http") != 0) {
    copyText(result.error, "Bambuddy HTTPS needs a configured CA");
  } else {
    String path = settings.bambuddyApiPath;
    if (path.endsWith("/")) {
      path.remove(path.length() - 1);
    }
    const String url =
        "http://" + String(settings.bambuddyHost) + ":" +
        String(settings.bambuddyPort) + path + "/printers/" +
        String(settings.bambuddyPrinterId) + "/status";

    WiFiClient client;
    HTTPClient request;
    request.setConnectTimeout(4000);
    request.setTimeout(6000);
    request.useHTTP10(true);
    if (!request.begin(client, url)) {
      copyText(result.error, "Could not initialize Bambuddy request");
    } else {
      request.addHeader("X-API-Key", settings.bambuddyApiKey);
      const int status = request.GET();
      if (status != HTTP_CODE_OK) {
        snprintf(
            result.error,
            sizeof(result.error),
            "Bambuddy returned HTTP %d",
            status);
      } else if (request.getSize() > 30000) {
        copyText(result.error, "Bambuddy response was too large");
      } else {
        JsonDocument document;
        const DeserializationError jsonError =
            deserializeJson(document, request.getStream());
        if (jsonError) {
          snprintf(
              result.error,
              sizeof(result.error),
              "Bambuddy JSON: %s",
              jsonError.c_str());
        } else {
          result.connected = document["connected"] | false;
          copyText(result.printerName, document["name"] | "Printer");
          copyText(result.printerState, document["state"] | "UNKNOWN");
          const char* printName = document["current_print"] | "";
          if (strlen(printName) == 0) {
            printName = document["subtask_name"] | "";
          }
          if (strlen(printName) == 0) {
            printName = document["gcode_file"] | "";
          }
          copyText(result.printName, printName);
          result.progress =
              constrain(document["progress"] | 0.0f, 0.0f, 100.0f);
          result.remainingMinutes = document["remaining_time"] | 0;
          result.currentLayer = document["layer_num"] | 0;
          result.totalLayers = document["total_layers"] | 0;
          result.nozzleTemperature =
              document["temperatures"]["nozzle"] | 0.0f;
          result.nozzleTarget =
              document["temperatures"]["nozzle_target"] | 0.0f;
          result.bedTemperature = document["temperatures"]["bed"] | 0.0f;
          result.bedTarget =
              document["temperatures"]["bed_target"] | 0.0f;
          result.wifiSignal = document["wifi_signal"] | 0;
          result.state = LiveDataState::ready;
          result.updatedAt = millis();
        }
      }
      request.end();
    }
  }

  if (result.updatedAt == 0) {
    result.updatedAt = millis();
  }
  if (result.state == LiveDataState::ready) {
    Serial.printf(
        "[BAMBUDDY] %s connected=%s state=%s progress=%.0f%%\n",
        result.printerName,
        result.connected ? "yes" : "no",
        result.printerState,
        result.progress);
  } else {
    Serial.printf("[BAMBUDDY] Error: %s\n", result.error);
  }
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  bambuddyData = result;
  bambuddyBusy = false;
  xSemaphoreGive(dataMutex);
}

bool checkTcpPort(
    const char* host,
    uint16_t port,
    uint16_t& responseMilliseconds) {
  WiFiClient client;
  const uint32_t startedAt = millis();
  const bool connected = client.connect(host, port, 2500);
  responseMilliseconds = static_cast<uint16_t>(
      min<uint32_t>(millis() - startedAt, UINT16_MAX));
  client.stop();
  return connected;
}

#if 0
struct IcsDate {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  bool allDay = false;
  bool valid = false;
};

struct IcsEvent {
  String summary;
  String location;
  String recurrence;
  IcsDate start;
  IcsDate end;
  uint64_t excludedDates[8] = {};
  uint8_t excludedCount = 0;
};

int64_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra =
      yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + static_cast<int>(dayOfEra) - 719468;
}

void civilFromDays(int64_t days, int& year, int& month, int& day) {
  days += 719468;
  const int era = static_cast<int>((days >= 0 ? days : days - 146096) / 146097);
  const unsigned dayOfEra = static_cast<unsigned>(days - era * 146097);
  const unsigned yearOfEra =
      (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 -
       dayOfEra / 146096) /
      365;
  year = static_cast<int>(yearOfEra) + era * 400;
  const unsigned dayOfYear =
      dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const unsigned monthPrime = (5 * dayOfYear + 2) / 153;
  day = static_cast<int>(
      dayOfYear - (153 * monthPrime + 2) / 5 + 1);
  month = static_cast<int>(monthPrime + (monthPrime < 10 ? 3 : -9));
  year += month <= 2;
}

uint64_t calendarKey(const IcsDate& value) {
  return static_cast<uint64_t>(value.year) * 100000000ULL +
         static_cast<uint64_t>(value.month) * 1000000ULL +
         static_cast<uint64_t>(value.day) * 10000ULL +
         static_cast<uint64_t>(value.hour) * 100ULL +
         value.minute;
}

IcsDate parseIcsDate(const String& rawValue, int32_t utcOffsetSeconds) {
  String value = rawValue;
  value.trim();
  IcsDate result;
  if (value.length() < 8) {
    return result;
  }
  result.year = value.substring(0, 4).toInt();
  result.month = value.substring(4, 6).toInt();
  result.day = value.substring(6, 8).toInt();
  result.allDay = value.length() == 8;
  if (!result.allDay && value.length() >= 13) {
    result.hour = value.substring(9, 11).toInt();
    result.minute = value.substring(11, 13).toInt();
  }
  result.valid =
      result.year >= 1970 && result.month >= 1 && result.month <= 12 &&
      result.day >= 1 && result.day <= 31 &&
      result.hour >= 0 && result.hour <= 23 &&
      result.minute >= 0 && result.minute <= 59;
  if (!result.valid || result.allDay || !value.endsWith("Z")) {
    return result;
  }

  int64_t localSeconds =
      daysFromCivil(result.year, result.month, result.day) * 86400LL +
      result.hour * 3600 + result.minute * 60 + utcOffsetSeconds;
  int64_t localDays = localSeconds / 86400;
  int64_t secondsInDay = localSeconds % 86400;
  if (secondsInDay < 0) {
    secondsInDay += 86400;
    --localDays;
  }
  civilFromDays(
      localDays,
      result.year,
      result.month,
      result.day);
  result.hour = static_cast<int>(secondsInDay / 3600);
  result.minute = static_cast<int>((secondsInDay % 3600) / 60);
  return result;
}

String unescapeIcsText(const String& input) {
  String output;
  output.reserve(min<size_t>(input.length(), 64));
  bool escaped = false;
  for (size_t index = 0; index < input.length() && output.length() < 64;
       ++index) {
    const char character = input[index];
    if (escaped) {
      output += (character == 'n' || character == 'N') ? ' ' : character;
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else {
      output += character;
    }
  }
  output.trim();
  return output;
}

int weekdayForDay(int64_t day) {
  int weekday = static_cast<int>((day + 3) % 7);
  return weekday < 0 ? weekday + 7 : weekday;
}

uint8_t weekdayMask(const String& byDay) {
  static constexpr const char* names[] = {
      "MO", "TU", "WE", "TH", "FR", "SA", "SU"};
  uint8_t mask = 0;
  for (int index = 0; index < 7; ++index) {
    if (byDay.indexOf(names[index]) >= 0) {
      mask |= 1U << index;
    }
  }
  return mask;
}

String recurrenceValue(const String& rule, const char* key) {
  const String prefix = String(key) + "=";
  int position = 0;
  while (position < static_cast<int>(rule.length())) {
    int end = rule.indexOf(';', position);
    if (end < 0) {
      end = rule.length();
    }
    const String part = rule.substring(position, end);
    if (part.startsWith(prefix)) {
      return part.substring(prefix.length());
    }
    position = end + 1;
  }
  return "";
}

bool recurrenceMatches(
    const IcsEvent& event,
    int64_t candidateDay,
    int32_t utcOffsetSeconds) {
  const int64_t startDay =
      daysFromCivil(event.start.year, event.start.month, event.start.day);
  if (candidateDay < startDay) {
    return false;
  }
  if (event.recurrence.isEmpty()) {
    return candidateDay == startDay;
  }

  const String frequency = recurrenceValue(event.recurrence, "FREQ");
  const int interval = max(
      1,
      static_cast<int>(
          recurrenceValue(event.recurrence, "INTERVAL").toInt()));
  const String untilValue = recurrenceValue(event.recurrence, "UNTIL");
  if (!untilValue.isEmpty()) {
    const IcsDate until = parseIcsDate(untilValue, utcOffsetSeconds);
    if (until.valid &&
        candidateDay >
            daysFromCivil(until.year, until.month, until.day)) {
      return false;
    }
  }

  int year = 0;
  int month = 0;
  int day = 0;
  civilFromDays(candidateDay, year, month, day);
  const int64_t dayDifference = candidateDay - startDay;
  const uint8_t byDay =
      weekdayMask(recurrenceValue(event.recurrence, "BYDAY"));
  if (byDay != 0 && (byDay & (1U << weekdayForDay(candidateDay))) == 0) {
    return false;
  }
  if (frequency == "DAILY") {
    return dayDifference % interval == 0;
  }
  if (frequency == "WEEKLY") {
    const int64_t startMonday =
        startDay - weekdayForDay(startDay);
    return ((candidateDay - startMonday) / 7) % interval == 0 &&
           (byDay != 0 ||
            weekdayForDay(candidateDay) == weekdayForDay(startDay));
  }
  if (frequency == "MONTHLY") {
    const int monthDifference =
        (year - event.start.year) * 12 + month - event.start.month;
    const String byMonthDay =
        recurrenceValue(event.recurrence, "BYMONTHDAY");
    const int requiredDay =
        byMonthDay.isEmpty() ? event.start.day : byMonthDay.toInt();
    return monthDifference >= 0 &&
           monthDifference % interval == 0 &&
           day == requiredDay;
  }
  if (frequency == "YEARLY") {
    return (year - event.start.year) % interval == 0 &&
           month == event.start.month && day == event.start.day;
  }
  return false;
}

bool isExcluded(const IcsEvent& event, uint64_t key) {
  for (uint8_t index = 0; index < event.excludedCount; ++index) {
    if (event.excludedDates[index] / 10000ULL == key / 10000ULL) {
      return true;
    }
  }
  return false;
}

void formatCalendarTime(
    const IcsDate& start,
    const IcsDate& end,
    const char* timeFormat,
    char* output,
    size_t outputSize) {
  if (start.allDay) {
    strlcpy(output, "All day", outputSize);
    return;
  }
  auto formatOne = [timeFormat](const IcsDate& value, char* target) {
    if (strcmp(timeFormat, "12h") == 0) {
      const int hour = value.hour % 12 == 0 ? 12 : value.hour % 12;
      snprintf(
          target,
          12,
          "%d:%02d%s",
          hour,
          value.minute,
          value.hour < 12 ? "am" : "pm");
    } else {
      snprintf(target, 12, "%02d:%02d", value.hour, value.minute);
    }
  };
  char startText[12] = {};
  char endText[12] = {};
  formatOne(start, startText);
  if (end.valid && !end.allDay) {
    formatOne(end, endText);
    snprintf(output, outputSize, "%s-%s", startText, endText);
  } else {
    strlcpy(output, startText, outputSize);
  }
}

void insertTodayEvent(CalendarData& result, const CalendarEventData& event) {
  size_t position = 0;
  while (position < result.todayCount &&
         result.today[position].startKey <= event.startKey) {
    ++position;
  }
  if (position >= kMaximumCalendarEvents) {
    return;
  }
  const size_t newCount =
      min<size_t>(result.todayCount + 1, kMaximumCalendarEvents);
  for (size_t index = newCount - 1; index > position; --index) {
    result.today[index] = result.today[index - 1];
  }
  result.today[position] = event;
  result.todayCount = static_cast<uint8_t>(newCount);
}

void addCalendarOccurrence(
    const IcsEvent& source,
    int64_t occurrenceDay,
    int64_t todayDay,
    uint64_t nowKey,
    const char* timeFormat,
    CalendarData& result) {
  CalendarEventData event;
  copyText(
      event.summary,
      source.summary.isEmpty() ? "Untitled event" : source.summary.c_str());
  copyText(event.location, source.location.c_str());
  IcsDate start = source.start;
  civilFromDays(
      occurrenceDay,
      start.year,
      start.month,
      start.day);
  IcsDate end = source.end;
  if (end.valid) {
    const int64_t durationDays =
        daysFromCivil(source.end.year, source.end.month, source.end.day) -
        daysFromCivil(source.start.year, source.start.month, source.start.day);
    civilFromDays(
        occurrenceDay + durationDays,
        end.year,
        end.month,
        end.day);
  }
  event.startKey = calendarKey(start);
  event.allDay = start.allDay;
  formatCalendarTime(
      start,
      end,
      timeFormat,
      event.time,
      sizeof(event.time));
  if (isExcluded(source, event.startKey)) {
    return;
  }
  if (occurrenceDay == todayDay) {
    insertTodayEvent(result, event);
  }
  if ((event.startKey >= nowKey ||
       (event.allDay && occurrenceDay == todayDay)) &&
      (!result.hasNextEvent ||
       event.startKey < result.nextEvent.startKey)) {
    result.nextEvent = event;
    result.hasNextEvent = true;
  }
}

void finishIcsEvent(
    const IcsEvent& event,
    int64_t todayDay,
    uint64_t nowKey,
    int32_t utcOffsetSeconds,
    const char* timeFormat,
    CalendarData& result) {
  if (!event.start.valid) {
    return;
  }
  const int64_t lastDay = todayDay + 30;
  for (int64_t day = todayDay; day <= lastDay; ++day) {
    if (recurrenceMatches(event, day, utcOffsetSeconds)) {
      addCalendarOccurrence(
          event,
          day,
          todayDay,
          nowKey,
          timeFormat,
          result);
      if (event.recurrence.isEmpty()) {
        break;
      }
    }
  }
}

bool parseCalendarStream(
    Stream& stream,
    int contentLength,
    int32_t utcOffsetSeconds,
    const char* timeFormat,
    CalendarData& result) {
  if (contentLength > static_cast<int>(kMaximumCalendarBytes)) {
    copyText(result.error, "Calendar feed is larger than 4 MB");
    return false;
  }

  const time_t localNow = time(nullptr) + utcOffsetSeconds;
  struct tm nowParts = {};
  gmtime_r(&localNow, &nowParts);
  const int64_t todayDay =
      daysFromCivil(
          nowParts.tm_year + 1900,
          nowParts.tm_mon + 1,
          nowParts.tm_mday);
  const uint64_t nowKey =
      static_cast<uint64_t>(nowParts.tm_year + 1900) * 100000000ULL +
      static_cast<uint64_t>(nowParts.tm_mon + 1) * 1000000ULL +
      static_cast<uint64_t>(nowParts.tm_mday) * 10000ULL +
      static_cast<uint64_t>(nowParts.tm_hour) * 100ULL +
      nowParts.tm_min;

  IcsEvent event;
  bool inEvent = false;
  String pendingLine;
  size_t bytesRead = 0;
  uint16_t lineCount = 0;
  stream.setTimeout(1000);
  auto processLine = [&](const String& line) {
    if (line == "BEGIN:VEVENT") {
      event = IcsEvent();
      inEvent = true;
      return;
    }
    if (line == "END:VEVENT") {
      if (inEvent) {
        finishIcsEvent(
            event,
            todayDay,
            nowKey,
            utcOffsetSeconds,
            timeFormat,
            result);
      }
      inEvent = false;
      return;
    }
    if (!inEvent) {
      return;
    }
    const int separator = line.indexOf(':');
    if (separator < 0) {
      return;
    }
    String property = line.substring(0, separator);
    property.toUpperCase();
    const String value = line.substring(separator + 1);
    if (property.startsWith("DTSTART")) {
      event.start = parseIcsDate(value, utcOffsetSeconds);
    } else if (property.startsWith("DTEND")) {
      event.end = parseIcsDate(value, utcOffsetSeconds);
    } else if (property == "SUMMARY") {
      event.summary = unescapeIcsText(value);
    } else if (property == "LOCATION") {
      event.location = unescapeIcsText(value);
    } else if (property == "RRULE") {
      event.recurrence = value;
      event.recurrence.toUpperCase();
    } else if (property.startsWith("EXDATE")) {
      int position = 0;
      while (position < static_cast<int>(value.length()) &&
             event.excludedCount < 8) {
        int end = value.indexOf(',', position);
        if (end < 0) {
          end = value.length();
        }
        const IcsDate excluded =
            parseIcsDate(value.substring(position, end), utcOffsetSeconds);
        if (excluded.valid) {
          event.excludedDates[event.excludedCount++] =
              calendarKey(excluded);
        }
        position = end + 1;
      }
    }
  };

  while (bytesRead < static_cast<size_t>(contentLength)) {
    if (!stream.available()) {
      break;
    }
    String line = stream.readStringUntil('\n');
    if (++lineCount % 32 == 0) {
      vTaskDelay(1);
    }
    bytesRead += line.length() + 1;
    if (bytesRead > kMaximumCalendarBytes) {
      copyText(result.error, "Calendar feed exceeded 4 MB");
      return false;
    }
    if (line.endsWith("\r")) {
      line.remove(line.length() - 1);
    }
    if ((line.startsWith(" ") || line.startsWith("\t")) &&
        !pendingLine.isEmpty()) {
      pendingLine += line.substring(1);
      if (pendingLine.length() > 768) {
        copyText(result.error, "Calendar contains an oversized line");
        return false;
      }
    } else {
      if (!pendingLine.isEmpty()) {
        processLine(pendingLine);
      }
      pendingLine = line;
    }
  }
  if (!pendingLine.isEmpty()) {
    processLine(pendingLine);
  }
  if (contentLength > 0 &&
      bytesRead < static_cast<size_t>(contentLength)) {
    copyText(result.error, "Calendar download was incomplete");
    return false;
  }
  return true;
}

bool readCalendarRequest(
    HTTPClient& request,
    int32_t utcOffsetSeconds,
    const char* timeFormat,
    CalendarData& result) {
  request.setConnectTimeout(6000);
  request.setTimeout(10000);
  request.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  request.useHTTP10(true);
  const int status = request.GET();
  if (status != HTTP_CODE_OK) {
    snprintf(
        result.error,
        sizeof(result.error),
        "Calendar returned HTTP %d",
        status);
    request.end();
    return false;
  }
  const int contentLength = request.getSize();
  if (contentLength > static_cast<int>(kMaximumCalendarBytes)) {
    copyText(result.error, "Calendar feed is larger than 4 MB");
    request.end();
    return false;
  }
  if (storageMutex == nullptr ||
      xSemaphoreTake(storageMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    copyText(result.error, "SD card is busy");
    request.end();
    return false;
  }

  constexpr const char* temporaryPath = "/dashboard/calendar.tmp";
  SD.remove(temporaryPath);
  File output = SD.open(temporaryPath, FILE_WRITE);
  if (!output) {
    xSemaphoreGive(storageMutex);
    copyText(result.error, "Could not create calendar cache");
    request.end();
    return false;
  }

  LimitedWriteStream limitedOutput(output, kMaximumCalendarBytes);
  const int transferResult = request.writeToStream(&limitedOutput);
  const size_t bytesWritten = limitedOutput.written();
  output.close();
  request.end();

  if (transferResult < 0 || bytesWritten == 0 ||
      (contentLength > 0 &&
       bytesWritten != static_cast<size_t>(contentLength))) {
    SD.remove(temporaryPath);
    xSemaphoreGive(storageMutex);
    copyText(result.error, "Calendar download was incomplete");
    return false;
  }
  if (limitedOutput.exceeded() ||
      (bytesWritten >= kMaximumCalendarBytes &&
      (contentLength < 0 ||
       static_cast<size_t>(contentLength) > bytesWritten))) {
    SD.remove(temporaryPath);
    xSemaphoreGive(storageMutex);
    copyText(result.error, "Calendar feed exceeded 4 MB");
    return false;
  }

  File input = SD.open(temporaryPath, FILE_READ);
  const bool parsed =
      input &&
      parseCalendarStream(
          input,
          static_cast<int>(bytesWritten),
          utcOffsetSeconds,
          timeFormat,
          result);
  input.close();
  SD.remove(temporaryPath);
  xSemaphoreGive(storageMutex);
  return parsed;
}

void fetchCalendar(const LiveDataSettings& settings) {
  CalendarData result;
  result.state = LiveDataState::error;
  if (WiFi.status() != WL_CONNECTED) {
    copyText(result.error, "Wi-Fi is offline");
  } else if (settings.calendarIcsUrl.isEmpty()) {
    copyText(result.error, "Set a calendar URL in the portal");
  } else if (!ensureClock(result.error, sizeof(result.error))) {
  } else {
    int32_t utcOffsetSeconds = 0;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    if (weatherData.state == LiveDataState::ready) {
      utcOffsetSeconds = weatherData.utcOffsetSeconds;
    }
    xSemaphoreGive(dataMutex);

    const String& url = settings.calendarIcsUrl;
    HTTPClient request;
    bool requested = false;
    if (url.startsWith("https://")) {
      WiFiClientSecure client;
      client.setCACert(
          url.indexOf("google") >= 0 ? kGtsRootR1 : kIsrgRootX1);
      client.setHandshakeTimeout(6);
      if (request.begin(client, url)) {
        requested = readCalendarRequest(
            request,
            utcOffsetSeconds,
            settings.timeFormat,
            result);
      } else {
        copyText(result.error, "Could not initialize calendar HTTPS");
      }
    } else if (url.startsWith("http://")) {
      WiFiClient client;
      if (request.begin(client, url)) {
        requested = readCalendarRequest(
            request,
            utcOffsetSeconds,
            settings.timeFormat,
            result);
      } else {
        copyText(result.error, "Could not initialize calendar HTTP");
      }
    } else {
      copyText(result.error, "Calendar URL must use HTTP or HTTPS");
    }
    if (requested) {
      result.state = LiveDataState::ready;
    }
  }

  result.updatedAt = millis();
  if (result.state == LiveDataState::ready) {
    Serial.printf(
        "[CALENDAR] today=%u next=%s\n",
        result.todayCount,
        result.hasNextEvent ? result.nextEvent.summary : "none");
  } else {
    Serial.printf("[CALENDAR] Error: %s\n", result.error);
  }
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  if (calendarData != nullptr) {
    *calendarData = result;
  }
  calendarBusy = false;
  xSemaphoreGive(dataMutex);
}

#endif

void fetchSystems(const LiveDataSettings& settings) {
  SystemsData result;
  result.state = LiveDataState::error;
  copyText(result.gateway, WiFi.gatewayIP().toString().c_str());

  for (size_t index = 0; index < kMaximumSystemMonitors; ++index) {
    const SystemMonitorSettings& monitor = settings.systemMonitors[index];
    SystemMonitorData& check = result.monitors[index];
    copyText(check.name, monitor.name);
    check.configured =
        monitor.enabled &&
        strlen(monitor.name) > 0 &&
        strlen(monitor.host) > 0;
    if (!check.configured) {
      copyText(check.detail, "Not configured");
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    copyText(result.error, "Wi-Fi is offline");
  } else {
    IPAddress resolvedAddress;
    result.dnsOnline =
        WiFi.hostByName("example.com", resolvedAddress) == 1;
    result.internetOnline =
        checkTcpPort("1.1.1.1", 443, result.internetMilliseconds);

    for (size_t index = 0; index < kMaximumSystemMonitors; ++index) {
      const SystemMonitorSettings& monitor = settings.systemMonitors[index];
      SystemMonitorData& check = result.monitors[index];
      if (!check.configured) {
        continue;
      }

      const uint32_t startedAt = millis();
      if (strcmp(monitor.type, "tcp") == 0) {
        check.online =
            checkTcpPort(
                monitor.host,
                monitor.port,
                check.responseMilliseconds);
        snprintf(
            check.detail,
            sizeof(check.detail),
            check.online ? "TCP %u open" : "TCP %u unavailable",
            monitor.port);
      } else if (strcmp(monitor.type, "http") == 0) {
        WiFiClient client;
        HTTPClient request;
        request.setConnectTimeout(2500);
        request.setTimeout(3500);
        request.useHTTP10(true);
        const String url =
            "http://" + String(monitor.host) + ":" +
            String(monitor.port) +
            (monitor.path[0] == '/' ? String(monitor.path)
                                    : "/" + String(monitor.path));
        int status = -1;
        if (request.begin(client, url)) {
          status = request.GET();
          request.end();
        }
        check.responseMilliseconds = static_cast<uint16_t>(
            min<uint32_t>(millis() - startedAt, UINT16_MAX));
        check.online = status >= 200 && status < 400;
        snprintf(
            check.detail,
            sizeof(check.detail),
            status > 0 ? "HTTP %d" : "HTTP unavailable",
            status);
      } else {
        copyText(check.detail, "Unsupported check type");
      }
    }

    result.state = LiveDataState::ready;
    result.updatedAt = millis();
  }

  if (result.updatedAt == 0) {
    result.updatedAt = millis();
  }
  Serial.printf(
      "[SYSTEMS] dns=%s internet=%s gateway=%s\n",
      result.dnsOnline ? "online" : "offline",
      result.internetOnline ? "online" : "offline",
      result.gateway);
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  systemsData = result;
  systemsBusy = false;
  xSemaphoreGive(dataMutex);
}

void networkWorker(void*) {
  NetworkJob job;
  while (true) {
    if (xQueueReceive(jobQueue, &job, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    LiveDataSettings settings;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    settings = currentSettings;
    xSemaphoreGive(dataMutex);

    if (networkMutex != nullptr) {
      xSemaphoreTake(networkMutex, portMAX_DELAY);
    }
    if (job == NetworkJob::weather) {
      fetchWeather(settings);
    } else if (job == NetworkJob::flights) {
      fetchFlights(settings);
    } else if (job == NetworkJob::aircraftPhoto) {
      fetchAircraftPhoto();
    } else if (job == NetworkJob::bambuddy) {
      fetchBambuddy(settings);
    } else {
      fetchSystems(settings);
    }
    if (networkMutex != nullptr) {
      xSemaphoreGive(networkMutex);
    }
  }
}

bool queueJob(NetworkJob job) {
  if (jobQueue == nullptr || dataMutex == nullptr) {
    return false;
  }

  xSemaphoreTake(dataMutex, portMAX_DELAY);
  bool* busy = nullptr;
  if (job == NetworkJob::weather) {
    busy = &weatherBusy;
  } else if (job == NetworkJob::flights) {
    busy = &flightsBusy;
  } else if (job == NetworkJob::aircraftPhoto) {
    busy = &aircraftPhotoBusy;
  } else if (job == NetworkJob::bambuddy) {
    busy = &bambuddyBusy;
  } else {
    busy = &systemsBusy;
  }
  if (*busy) {
    xSemaphoreGive(dataMutex);
    return false;
  }
  *busy = true;
  if (job == NetworkJob::weather) {
    weatherData.state = LiveDataState::loading;
  } else if (job == NetworkJob::flights) {
    flightsData.state = LiveDataState::loading;
  } else if (job == NetworkJob::aircraftPhoto) {
    aircraftPhotoData.state = LiveDataState::loading;
  } else if (job == NetworkJob::bambuddy) {
    bambuddyData.state = LiveDataState::loading;
  } else {
    systemsData.state = LiveDataState::loading;
  }
  xSemaphoreGive(dataMutex);

  if (xQueueSend(jobQueue, &job, 0) != pdTRUE) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    *busy = false;
    xSemaphoreGive(dataMutex);
    return false;
  }
  return true;
}

}  // namespace

void liveDataBegin() {
  dataMutex = xSemaphoreCreateMutex();
  if (dataMutex == nullptr) {
    Serial.println("[LIVE] Could not create data mutex");
    return;
  }
  jobQueue = xQueueCreate(4, sizeof(NetworkJob));
  if (jobQueue == nullptr) {
    Serial.println("[LIVE] Could not create worker queue");
    return;
  }
  const BaseType_t taskCreated = xTaskCreatePinnedToCore(
      networkWorker,
      "live-data",
      16384,
      nullptr,
      1,
      nullptr,
      0);
  if (taskCreated != pdPASS) {
    Serial.println("[LIVE] Could not start network worker");
    vQueueDelete(jobQueue);
    jobQueue = nullptr;
  }
}

void liveDataSetStorageMutex(SemaphoreHandle_t mutex) {
  storageMutex = mutex;
}

void liveDataSetNetworkMutex(SemaphoreHandle_t mutex) {
  networkMutex = mutex;
}

void liveDataConfigure(const LiveDataSettings& settings) {
  if (dataMutex == nullptr) {
    return;
  }
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  currentSettings = settings;
  xSemaphoreGive(dataMutex);
}

bool liveDataRequestWeather() {
  return queueJob(NetworkJob::weather);
}

bool liveDataRequestFlights() {
  return queueJob(NetworkJob::flights);
}

bool liveDataRequestAircraftPhoto(const char* aircraftHex) {
  if (dataMutex == nullptr || aircraftHex == nullptr ||
      strlen(aircraftHex) < 6 ||
      strlen(aircraftHex) >= sizeof(requestedAircraftHex) ||
      jobQueue == nullptr) {
    return false;
  }
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  if (aircraftPhotoBusy) {
    xSemaphoreGive(dataMutex);
    return false;
  }
  copyText(requestedAircraftHex, aircraftHex);
  aircraftPhotoBusy = true;
  aircraftPhotoData.state = LiveDataState::loading;
  xSemaphoreGive(dataMutex);
  const NetworkJob job = NetworkJob::aircraftPhoto;
  if (xQueueSend(jobQueue, &job, 0) == pdTRUE) {
    return true;
  }
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  aircraftPhotoBusy = false;
  aircraftPhotoData.state = LiveDataState::idle;
  xSemaphoreGive(dataMutex);
  return false;
}

bool liveDataRequestBambuddy() {
  return queueJob(NetworkJob::bambuddy);
}

bool liveDataRequestSystems() {
  return queueJob(NetworkJob::systems);
}

WeatherData liveDataWeatherSnapshot() {
  if (dataMutex == nullptr) {
    return weatherData;
  }
  WeatherData snapshot;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  snapshot = weatherData;
  xSemaphoreGive(dataMutex);
  return snapshot;
}

FlightsData liveDataFlightsSnapshot() {
  if (dataMutex == nullptr) {
    return flightsData;
  }
  FlightsData snapshot;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  snapshot = flightsData;
  xSemaphoreGive(dataMutex);
  return snapshot;
}

AircraftPhotoData liveDataAircraftPhotoSnapshot() {
  AircraftPhotoData snapshot;
  if (dataMutex == nullptr) {
    return snapshot;
  }
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  snapshot = aircraftPhotoData;
  xSemaphoreGive(dataMutex);
  return snapshot;
}

BambuddyData liveDataBambuddySnapshot() {
  if (dataMutex == nullptr) {
    return bambuddyData;
  }
  BambuddyData snapshot;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  snapshot = bambuddyData;
  xSemaphoreGive(dataMutex);
  return snapshot;
}

SystemsData liveDataSystemsSnapshot() {
  if (dataMutex == nullptr) {
    return systemsData;
  }
  SystemsData snapshot;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  snapshot = systemsData;
  xSemaphoreGive(dataMutex);
  return snapshot;
}
