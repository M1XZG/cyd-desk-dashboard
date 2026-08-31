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
OtaStatus otaSnapshot();
