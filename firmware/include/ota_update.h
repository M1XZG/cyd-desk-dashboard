#pragma once

#include <Arduino.h>
#include <freertos/semphr.h>

enum class OtaState : uint8_t {
  idle,
  checking,
  upToDate,
  updateAvailable,
  downloading,
  readyToRestart,
  error,
};

struct OtaStatus {
  OtaState state = OtaState::idle;
  char installedVersion[25] = {};
  char latestVersion[25] = {};
  char error[129] = {};
  uint32_t expectedBytes = 0;
  uint32_t downloadedBytes = 0;
  bool canInstall = false;
  bool reinstall = false;
};

void otaBegin(SemaphoreHandle_t networkMutex);
bool otaRequestCheck();
bool otaRequestInstall();
bool otaUploadBegin(
    const char* version,
    uint32_t size,
    const char* sha256,
    char* error,
    size_t errorSize);
bool otaUploadWrite(
    uint8_t* data,
    size_t length,
    char* error,
    size_t errorSize);
bool otaUploadFinish(char* error, size_t errorSize);
void otaUploadAbort(const char* error);
OtaStatus otaSnapshot();
