#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "experiment_runner.h"

class WebUi {
public:
  void begin(WebServer& server, ExperimentRunner& runner, ImuManager& imu, Roller485Manager& roller, PsramLogger& logger);
  void update();

private:
  void handleRoot();
  void handleStatus();
  void handleStart();
  void handleStartQIdent();
  void handleStartEnergyControlAutonomous();
  void handleStartZeroCross();
  void handleStartIdentification();
  void handleStartControl();
  void handleZero();
  void handleStop();
  void handleClear();
  void handleRwLog();
  String statusJson() const;

  WebServer* server_ = nullptr;
  ExperimentRunner* runner_ = nullptr;
  ImuManager* imu_ = nullptr;
  Roller485Manager* roller_ = nullptr;
  PsramLogger* logger_ = nullptr;
};

