#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "experiment_runner.h"
#include "foot_angle_tracker.h"

class WebUi {
public:
  bool begin(WebServer& server, ExperimentRunner& runner, ImuManager& imu, Roller485Manager& roller, PsramLogger& logger, FootAngleTracker& foot_angles);
  bool accessPointReady() const { return ap_ready_; }
  void setSubsystemsReady(bool ready) { subsystems_ready_ = ready; }
  bool subsystemsReady() const { return subsystems_ready_; }
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
  void handleFootAngleLog();
  String statusJson() const;

  WebServer* server_ = nullptr;
  ExperimentRunner* runner_ = nullptr;
  ImuManager* imu_ = nullptr;
  Roller485Manager* roller_ = nullptr;
  PsramLogger* logger_ = nullptr;
  FootAngleTracker* foot_angles_ = nullptr;
  bool ap_ready_ = false;
  bool subsystems_ready_ = false;
};

