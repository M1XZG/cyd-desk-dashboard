#include "ota_update.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_sntp.h>
#include <mbedtls/sha256.h>
#include <time.h>

#include "firmware_version.h"

namespace {

constexpr char kManifestUrl[] =
    "https://github.com/M1XZG/cyd-desk-dashboard/releases/latest/download/"
    "ota-manifest.json";
constexpr char kReleaseBaseUrl[] =
    "https://github.com/M1XZG/cyd-desk-dashboard/releases/download/";
constexpr size_t kMaximumManifestBytes = 1024;
constexpr uint32_t kNetworkLockTimeoutMilliseconds = 15000;
constexpr uint32_t kDownloadStallTimeoutMilliseconds = 15000;

constexpr char kGitHubCaBundle[] = R"pem(
-----BEGIN CERTIFICATE-----
MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDEL
MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl
eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT
JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMjAx
MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNVBAgT
Ck5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVUaGUg
VVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlm
aWNhdGlvbiBBdXRob3JpdHkwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQarFRaqflo
I+d61SRvU8Za2EurxtW20eZzca7dnNYMYf3boIkDuAUU7FfO7l0/4iGzzvfUinng
o4N+LZfQYcTxmdwlkWOrfzCjtHDix6EznPO/LlxTsV+zfTJ/ijTjeXmjQjBAMB0G
A1UdDgQWBBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAOBgNVHQ8BAf8EBAMCAQYwDwYD
VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjA2Z6EWCNzklwBBHU6+4WMB
zzuqQhFkoJ2UOQIReVx7Hfpkue4WQrO/isIJxOzksU0CMQDpKmFHjFJKS04YcPbW
RNZu9YO6bVi9JNlWSOrvxKJGgYhqOkbRqZtNyWHa0V1Xahg=
-----END CERTIFICATE-----
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

enum class OtaJob : uint8_t {
  check,
  install,
};

struct ReleaseManifest {
  char version[25] = {};
  char sha256[65] = {};
  uint32_t size = 0;
  bool valid = false;
};

SemaphoreHandle_t statusMutex = nullptr;
SemaphoreHandle_t sharedNetworkMutex = nullptr;
QueueHandle_t jobQueue = nullptr;
OtaStatus status;
ReleaseManifest manifest;
bool jobBusy = false;

class ScopedNetworkLock {
 public:
  ScopedNetworkLock()
      : locked_(
            sharedNetworkMutex == nullptr ||
            xSemaphoreTake(
                sharedNetworkMutex,
                pdMS_TO_TICKS(kNetworkLockTimeoutMilliseconds)) == pdTRUE) {}

  ~ScopedNetworkLock() {
    if (locked_ && sharedNetworkMutex != nullptr) {
      xSemaphoreGive(sharedNetworkMutex);
    }
  }

  explicit operator bool() const {
    return locked_;
  }

 private:
  bool locked_;
};

void setStatus(
    OtaState state,
    const char* error = nullptr,
    uint32_t downloadedBytes = 0) {
  xSemaphoreTake(statusMutex, portMAX_DELAY);
  status.state = state;
  status.downloadedBytes = downloadedBytes;
  strlcpy(status.error, error == nullptr ? "" : error, sizeof(status.error));
  xSemaphoreGive(statusMutex);
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

bool validReleaseVersion(const char* value) {
  const size_t length = strlen(value);
  if (length < 2 || length >= sizeof(manifest.version) || value[0] != 'v') {
    return false;
  }
  int dots = 0;
  bool digitRequired = true;
  for (size_t index = 1; index < length; ++index) {
    const char character = value[index];
    if (character == '.') {
      if (digitRequired || dots >= 2) {
        return false;
      }
      ++dots;
      digitRequired = true;
    } else if (isdigit(static_cast<unsigned char>(character))) {
      digitRequired = false;
    } else {
      return false;
    }
  }
  return dots == 2 && !digitRequired;
}

bool validSha256(const char* value) {
  if (strlen(value) != 64) {
    return false;
  }
  for (size_t index = 0; index < 64; ++index) {
    if (!isxdigit(static_cast<unsigned char>(value[index]))) {
      return false;
    }
  }
  return true;
}

int compareVersions(const char* left, const char* right) {
  const char* leftCursor = left[0] == 'v' ? left + 1 : left;
  const char* rightCursor = right[0] == 'v' ? right + 1 : right;
  for (int component = 0; component < 3; ++component) {
    char* leftEnd = nullptr;
    char* rightEnd = nullptr;
    const unsigned long leftValue = strtoul(leftCursor, &leftEnd, 10);
    const unsigned long rightValue = strtoul(rightCursor, &rightEnd, 10);
    if (leftEnd == leftCursor || rightEnd == rightCursor) {
      return strcmp(left, right);
    }
    if (leftValue != rightValue) {
      return leftValue < rightValue ? -1 : 1;
    }
    leftCursor = *leftEnd == '.' ? leftEnd + 1 : leftEnd;
    rightCursor = *rightEnd == '.' ? rightEnd + 1 : rightEnd;
  }
  const bool leftPrerelease = *leftCursor == '-';
  const bool rightPrerelease = *rightCursor == '-';
  if (leftPrerelease != rightPrerelease) {
    return leftPrerelease ? -1 : 1;
  }
  return strcmp(leftCursor, rightCursor);
}

bool beginRequest(
    WiFiClientSecure& client,
    HTTPClient& request,
    const String& url,
    char* error,
    size_t errorSize) {
  client.setCACert(kGitHubCaBundle);
  client.setHandshakeTimeout(8);
  request.setConnectTimeout(8000);
  request.setTimeout(15000);
  request.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  request.useHTTP10(true);
  if (!request.begin(client, url)) {
    strlcpy(error, "Could not initialize the HTTPS request", errorSize);
    return false;
  }
  const int httpStatus = request.GET();
  if (httpStatus != HTTP_CODE_OK) {
    snprintf(error, errorSize, "Update service returned HTTP %d", httpStatus);
    request.end();
    return false;
  }
  return true;
}

bool fetchManifest(char* error, size_t errorSize) {
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(error, "Wi-Fi is not connected", errorSize);
    return false;
  }
  if (!ensureClock(error, errorSize)) {
    return false;
  }

  ScopedNetworkLock networkLock;
  if (!networkLock) {
    strlcpy(error, "Another network request is still running", errorSize);
    return false;
  }

  WiFiClientSecure client;
  HTTPClient request;
  if (!beginRequest(client, request, kManifestUrl, error, errorSize)) {
    return false;
  }
  const int responseSize = request.getSize();
  if (responseSize <= 0 ||
      responseSize > static_cast<int>(kMaximumManifestBytes)) {
    strlcpy(error, "The update manifest size is invalid", errorSize);
    request.end();
    return false;
  }

  JsonDocument document;
  const DeserializationError jsonError =
      deserializeJson(document, request.getStream());
  request.end();
  if (jsonError) {
    snprintf(error, errorSize, "Update manifest JSON: %s", jsonError.c_str());
    return false;
  }

  const int schema = document["schema"] | 0;
  const char* version = document["version"] | "";
  const char* asset = document["firmware"]["asset"] | "";
  const char* sha256 = document["firmware"]["sha256"] | "";
  const uint32_t size = document["firmware"]["size"] | 0;
  if (schema != 1 || strcmp(asset, "firmware.bin") != 0 ||
      !validReleaseVersion(version) || !validSha256(sha256) ||
      size == 0 || size > ESP.getFreeSketchSpace()) {
    strlcpy(error, "The update manifest is invalid", errorSize);
    return false;
  }

  strlcpy(manifest.version, version, sizeof(manifest.version));
  strlcpy(manifest.sha256, sha256, sizeof(manifest.sha256));
  manifest.size = size;
  manifest.valid = true;

  xSemaphoreTake(statusMutex, portMAX_DELAY);
  strlcpy(status.latestVersion, manifest.version, sizeof(status.latestVersion));
  status.expectedBytes = manifest.size;
  status.downloadedBytes = 0;
  const int comparison = compareVersions(kFirmwareVersion, manifest.version);
  status.canInstall = comparison <= 0;
  status.reinstall = comparison == 0;
  status.state =
      comparison < 0 ? OtaState::updateAvailable : OtaState::upToDate;
  status.error[0] = '\0';
  xSemaphoreGive(statusMutex);
  return true;
}

void bytesToHex(const uint8_t* bytes, size_t length, char* output) {
  static constexpr char kHex[] = "0123456789abcdef";
  for (size_t index = 0; index < length; ++index) {
    output[index * 2] = kHex[bytes[index] >> 4];
    output[index * 2 + 1] = kHex[bytes[index] & 0x0F];
  }
  output[length * 2] = '\0';
}

bool installRelease(char* error, size_t errorSize) {
  if (!manifest.valid) {
    strlcpy(error, "Check for updates before installing", errorSize);
    return false;
  }
  if (compareVersions(kFirmwareVersion, manifest.version) > 0) {
    strlcpy(error, "Downgrading through OTA is not allowed", errorSize);
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(error, "Wi-Fi is not connected", errorSize);
    return false;
  }
  if (!ensureClock(error, errorSize)) {
    return false;
  }

  ScopedNetworkLock networkLock;
  if (!networkLock) {
    strlcpy(error, "Another network request is still running", errorSize);
    return false;
  }

  const String firmwareUrl =
      String(kReleaseBaseUrl) + manifest.version + "/firmware.bin";
  WiFiClientSecure client;
  HTTPClient request;
  if (!beginRequest(client, request, firmwareUrl, error, errorSize)) {
    return false;
  }
  const int contentLength = request.getSize();
  if (contentLength > 0 &&
      static_cast<uint32_t>(contentLength) != manifest.size) {
    strlcpy(error, "Firmware size does not match the release manifest", errorSize);
    request.end();
    return false;
  }
  if (!Update.begin(manifest.size, U_FLASH)) {
    snprintf(error, errorSize, "Could not prepare OTA: %s", Update.errorString());
    request.end();
    return false;
  }

  mbedtls_sha256_context shaContext;
  mbedtls_sha256_init(&shaContext);
  if (mbedtls_sha256_starts_ret(&shaContext, 0) != 0) {
    strlcpy(error, "Could not initialize SHA-256 verification", errorSize);
    Update.abort();
    request.end();
    mbedtls_sha256_free(&shaContext);
    return false;
  }

  WiFiClient* stream = request.getStreamPtr();
  uint8_t buffer[4096];
  uint32_t total = 0;
  uint32_t lastDataAt = millis();
  bool failed = false;
  while (total < manifest.size) {
    const size_t available = stream->available();
    if (available == 0) {
      if (!request.connected() ||
          millis() - lastDataAt >= kDownloadStallTimeoutMilliseconds) {
        strlcpy(error, "Firmware download ended before it was complete", errorSize);
        failed = true;
        break;
      }
      delay(10);
      continue;
    }

    const size_t toRead =
        min(sizeof(buffer), min(available, static_cast<size_t>(manifest.size - total)));
    const int count = stream->read(buffer, toRead);
    if (count <= 0) {
      delay(1);
      continue;
    }
    lastDataAt = millis();
    if (mbedtls_sha256_update_ret(&shaContext, buffer, count) != 0 ||
        Update.write(buffer, count) != static_cast<size_t>(count)) {
      snprintf(error, errorSize, "Could not write OTA: %s", Update.errorString());
      failed = true;
      break;
    }
    total += count;
    setStatus(OtaState::downloading, nullptr, total);
    delay(1);
  }
  request.end();

  uint8_t digest[32];
  char digestHex[65];
  if (!failed &&
      mbedtls_sha256_finish_ret(&shaContext, digest) == 0) {
    bytesToHex(digest, sizeof(digest), digestHex);
    if (strcasecmp(digestHex, manifest.sha256) != 0) {
      strlcpy(error, "Firmware SHA-256 verification failed", errorSize);
      failed = true;
    }
  } else if (!failed) {
    strlcpy(error, "Could not finish SHA-256 verification", errorSize);
    failed = true;
  }
  mbedtls_sha256_free(&shaContext);

  if (failed || total != manifest.size) {
    Update.abort();
    return false;
  }
  if (!Update.end()) {
    snprintf(error, errorSize, "Could not activate OTA: %s", Update.errorString());
    return false;
  }
  return true;
}

void worker(void*) {
  OtaJob job;
  while (true) {
    if (xQueueReceive(jobQueue, &job, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    char error[129] = {};
    bool successful = false;
    if (job == OtaJob::check) {
      successful = fetchManifest(error, sizeof(error));
    } else {
      setStatus(OtaState::downloading);
      successful = installRelease(error, sizeof(error));
      if (successful) {
        setStatus(OtaState::readyToRestart, nullptr, manifest.size);
      }
    }
    if (!successful) {
      setStatus(OtaState::error, error);
    }

    xSemaphoreTake(statusMutex, portMAX_DELAY);
    jobBusy = false;
    xSemaphoreGive(statusMutex);
  }
}

bool queueJob(OtaJob job) {
  if (jobQueue == nullptr || statusMutex == nullptr) {
    return false;
  }
  xSemaphoreTake(statusMutex, portMAX_DELAY);
  if (jobBusy) {
    xSemaphoreGive(statusMutex);
    return false;
  }
  if (job == OtaJob::install &&
      (!manifest.valid || !status.canInstall)) {
    xSemaphoreGive(statusMutex);
    return false;
  }
  jobBusy = true;
  if (job == OtaJob::check) {
    manifest.valid = false;
    status.state = OtaState::checking;
    status.error[0] = '\0';
    status.latestVersion[0] = '\0';
    status.canInstall = false;
    status.reinstall = false;
    status.expectedBytes = 0;
    status.downloadedBytes = 0;
  }
  xSemaphoreGive(statusMutex);

  if (xQueueSend(jobQueue, &job, 0) != pdTRUE) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    jobBusy = false;
    xSemaphoreGive(statusMutex);
    return false;
  }
  return true;
}

}  // namespace

void otaBegin(SemaphoreHandle_t networkMutex) {
  sharedNetworkMutex = networkMutex;
  statusMutex = xSemaphoreCreateMutex();
  if (statusMutex == nullptr) {
    Serial.println("[OTA] Could not create status mutex");
    return;
  }
  strlcpy(
      status.installedVersion,
      kFirmwareVersion,
      sizeof(status.installedVersion));
  jobQueue = xQueueCreate(2, sizeof(OtaJob));
  if (jobQueue == nullptr) {
    Serial.println("[OTA] Could not create worker queue");
    return;
  }
  if (xTaskCreatePinnedToCore(
          worker,
          "ota-update",
          12288,
          nullptr,
          1,
          nullptr,
          0) != pdPASS) {
    Serial.println("[OTA] Could not start worker");
    vQueueDelete(jobQueue);
    jobQueue = nullptr;
  }
}

bool otaRequestCheck() {
  return queueJob(OtaJob::check);
}

bool otaRequestInstall() {
  return queueJob(OtaJob::install);
}

OtaStatus otaSnapshot() {
  if (statusMutex == nullptr) {
    OtaStatus unavailable;
    strlcpy(
        unavailable.installedVersion,
        kFirmwareVersion,
        sizeof(unavailable.installedVersion));
    strlcpy(
        unavailable.error,
        "OTA service is unavailable",
        sizeof(unavailable.error));
    unavailable.state = OtaState::error;
    return unavailable;
  }
  OtaStatus snapshot;
  xSemaphoreTake(statusMutex, portMAX_DELAY);
  snapshot = status;
  xSemaphoreGive(statusMutex);
  return snapshot;
}
