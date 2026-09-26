#pragma once
#include "bounded_web_server.h"
#include "offline_run_session.h"
#include "run_control_worker.h"
#include "foot_observer.h"
#include "immutable_export.h"

class WebUi {
 public:
  void begin(BoundedWriteWebServer&, RunControlWorker&, FootObserver&, ImmutableExport&);
  void update();
 private:
  friend class OfflineRunSession;
  void stopServer();
  bool stopRadio();
  bool radioActive() const;
  bool queueStart();
  OfflineRunSession::StartResult startResult() const;
  bool runActive() const;
  void cancelStart();
  bool restoreTransport();
  void transportReady();
  bool quietResponse();
  void status();
  void command(RunControlWorker::Command);
  void manifest();
  void chunk();
  void previewCapture();
  void previewChunk();
  bool previewAllowed() const;
  BoundedWriteWebServer* server_ = nullptr;
  RunControlWorker* control_ = nullptr;
  FootObserver* feet_ = nullptr;
  ImmutableExport* export_ = nullptr;
  uint8_t chunk_buffer_[export_protocol::kChunkBytes + 16] = {};
  uint8_t* preview_buffer_ = nullptr;
  uint32_t preview_token_ = 0;
  uint32_t boot_id_ = 0;
  OfflineRunSession offline_;
  uint32_t ap_active_ = 0, offline_poll_ms_ = 0, radio_retry_ms_ = 0;
  uint32_t start_command_id_ = 0;
  bool start_submitted_ = false, cancel_sent_ = false, radio_start_requested_ = false;
};
