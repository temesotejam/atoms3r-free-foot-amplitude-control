#pragma once
#include <WebServer.h>
#include "run_control_worker.h"
#include "foot_observer.h"
#include "immutable_export.h"

class WebUi {
 public:
  void begin(WebServer&, RunControlWorker&, FootObserver&, ImmutableExport&);
  void update();
 private:
  void status();
  void command(RunControlWorker::Command);
  void manifest();
  void chunk();
  WebServer* server_ = nullptr;
  RunControlWorker* control_ = nullptr;
  FootObserver* feet_ = nullptr;
  ImmutableExport* export_ = nullptr;
  uint8_t chunk_buffer_[export_protocol::kChunkBytes + 16] = {};
};
