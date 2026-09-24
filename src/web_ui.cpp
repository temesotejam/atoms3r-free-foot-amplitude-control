#include "web_ui.h"
#include "config.h"
#include "runtime_web.h"
#include "runtime_diagnostics.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <errno.h>

static String num(float x) { return isfinite(x) ? String(x, 4) : String("null"); }
static const char* phase(ImmutableExport::Phase p) {
  switch (p) {
    case ImmutableExport::Phase::Building: return "building";
    case ImmutableExport::Phase::Ready: return "ready";
    case ImmutableExport::Phase::Error: return "error";
    default: return "empty";
  }
}
void WebUi::begin(WebServer& s, RunControlWorker& control, FootObserver& feet, ImmutableExport& exporter) {
  server_ = &s; control_ = &control; feet_ = &feet; export_ = &exporter;
  preview_buffer_ = static_cast<uint8_t*>(ps_malloc(FootObserver::kPreviewBytes));
  WiFi.onEvent([](WiFiEvent_t event) {
    RuntimeDiag::wifiEvent(static_cast<uint32_t>(event),
        event == ARDUINO_EVENT_WIFI_AP_STACONNECTED ? 1 : event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED ? -1 : 0,
        event == ARDUINO_EVENT_WIFI_AP_START ? 1 : event == ARDUINO_EVENT_WIFI_AP_STOP ? 0 : -1);
  });
  RuntimeDiag::result(WiFi.mode(WIFI_AP));
  RuntimeDiag::result(WiFi.softAP(Config::AP_SSID, Config::AP_PASS, Config::AP_CHANNEL));
  s.on("/", HTTP_GET, [this]() {
    RuntimeDiag::Scope diagnostic(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpRoot);
    server_->sendHeader("Cache-Control", "no-store");
    server_->send_P(200, "text/html; charset=utf-8", RUNTIME_HTML);
  });
  s.on("/status.json", HTTP_GET, [this]() { status(); });
  s.on("/vision/capture", HTTP_POST, [this]() { previewCapture(); });
  s.on("/vision/chunk", HTTP_GET, [this]() { previewChunk(); });
  s.on("/start-energy-control-autonomous", HTTP_POST, [this]() { command(RunControlWorker::Command::Start); });
  s.on("/clear", HTTP_POST, [this]() { command(RunControlWorker::Command::Clear); });
  s.on("/stop", HTTP_POST, [this]() {
    RuntimeDiag::Scope diagnostic(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpCommand);
    const bool ok = control_->requestStop();
    server_->send(ok ? 202 : 503, "text/plain", ok ? "stop_queued" : "controller_unavailable");
  });
  s.on("/export/prepare", HTTP_POST, [this]() {
    RuntimeDiag::Scope diagnostic(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpCommand);
    const auto st = control_->snapshot();
    const bool ok = !st.running && st.downloadable && !control_->commandState().pending && export_->prepare();
    server_->send(ok ? 202 : 409, "text/plain", ok ? "preparing" : "completed_run_required");
  });
  s.on("/export/manifest", HTTP_GET, [this]() { manifest(); });
  s.on("/export/chunk", HTTP_GET, [this]() { chunk(); });
  s.on("/download/rwlog", HTTP_GET, [this]() {
    server_->send(410, "text/plain", "Open http://192.168.4.1/ and use resumable RWLOG download.");
  });
  s.onNotFound([this]() { server_->send(404, "text/plain", "not_found"); });
  s.enableDelay(false); s.begin();
}
void WebUi::update() {
  RuntimeDiag::phase(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpPoll);
  if (server_) server_->handleClient();
  RuntimeDiag::sampleMemory();
  RuntimeDiag::beat(RuntimeDiag::Lane::Http);
  RuntimeDiag::phase(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::Wait);
}
void WebUi::command(RunControlWorker::Command cmd) {
  RuntimeDiag::Scope diagnostic(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpCommand);
  if (server_->hasArg("timing_ms")) {
    server_->send(400, "text/plain", "fixed_3ms_reload_page"); return;
  }
  const auto st = control_->snapshot();
  if (st.running || control_->commandState().pending || export_->status().phase == ImmutableExport::Phase::Building) {
    server_->send(409, "text/plain", "busy"); return;
  }
  if (cmd == RunControlWorker::Command::Start &&
      (export_->status().phase != ImmutableExport::Phase::Empty || !st.ready)) {
    server_->send(409, "text/plain", "clear_previous_run_first"); return;
  }
  if (cmd == RunControlWorker::Command::Clear && !export_->reset()) {
    server_->send(409, "text/plain", "export_preparing"); return;
  }
  const bool ok = control_->request(cmd);
  server_->send(ok ? 202 : 409, "text/plain", ok ? "command_queued" : "command_busy");
}
void WebUi::status() {
  RuntimeDiag::Scope diagnostic(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpStatus);
  const auto s = control_->snapshot(); const auto f = feet_->snapshot();
  const auto c = control_->commandState(); const auto e = export_->status();
  const auto h = control_->healthSnapshot();
  const auto camera = feet_->cameraSnapshot();
  const bool fresh = s.heartbeat_us && static_cast<uint32_t>(micros() - s.heartbeat_us) < 500000;
  RuntimeDiag::phase(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpJson);
  String json; json.reserve(3400);
  json = "{\"revision\":\"" RUNTIME_VERSION "\",\"state\":\"" + String(s.state_name) + "\"";
  json += ",\"running\":" + String(s.running ? "true" : "false");
  json += ",\"ready\":" + String(s.ready && fresh && export_->ready() && feet_->readyToStart() ? "true" : "false");
  json += ",\"controller_fresh\":" + String(fresh ? "true" : "false");
  json += ",\"run_id\":" + String(s.run_id) + ",\"samples\":" + String(s.sample_count);
  json += ",\"remaining_ms\":" + String(s.remaining_ms) + ",\"elapsed_ms\":" + String(s.measure_elapsed_ms);
  json += ",\"pitch_deg\":" + num(s.pitch_deg) + ",\"rate_dps\":" + num(s.rate_dps);
  json += ",\"target_deg\":" + num(s.target_deg);
  json += ",\"motor_mA\":" + String(s.motor_cmd_mA) + ",\"actual_mA\":" + String(s.actual_current_mA);
  json += ",\"battery_mV\":" + String(s.battery_mV);
  json += ",\"imu_ok\":" + String(s.imu_ok ? "true" : "false") + ",\"roller_ok\":" + String(s.roller_ok ? "true" : "false");
  json += ",\"led\":" + String(s.led_state) + ",\"sync_event_id\":" + String(s.sync_event_id);
  json += ",\"downloadable\":" + String(s.downloadable ? "true" : "false");
  json += ",\"last_error\":\"" + String(s.last_error) + "\"";
  json += ",\"command\":{\"submitted\":" + String(c.submitted) + ",\"completed\":" + String(c.completed);
  json += ",\"pending\":" + String(c.pending ? "true" : "false") + ",\"ok\":" + String(c.ok ? "true" : "false");
  json += ",\"result\":\"" + String(c.result) + "\"}";
  json += ",\"export_worker_ready\":" + String(export_->ready() ? "true" : "false");
  json += ",\"export_phase\":\"" + String(phase(e.phase)) + "\"";
  json += ",\"foot\":{\"available\":" + String(f.available ? "true" : "false");
  json += ",\"detector\":\"" + String(appcfg::kWhiteDetectorRevision) + "\"";
  json += ",\"zero_ready\":" + String(f.zero_ready ? "true" : "false") + ",\"zero_samples\":" + String(f.zero_samples);
  json += ",\"zero_reason\":\"" + String(footZeroReasonName(f.zero_reason)) + "\"";
  json += ",\"preview_available\":" + String(f.preview_available && preview_buffer_ ? "true" : "false");
  json += ",\"right_zero_x\":" + num(f.right_zero) + ",\"left_zero_x\":" + num(f.left_zero);
  json += ",\"right_x\":" + num(f.latest.right_x) + ",\"left_x\":" + num(f.latest.left_x);
  json += ",\"right_deg\":" + num(f.latest.right_deg) + ",\"left_deg\":" + num(f.latest.left_deg);
  json += ",\"right_scan_y\":" + num(f.latest.right_scan_y) + ",\"left_scan_y\":" + num(f.latest.left_scan_y);
  json += ",\"right_contrast\":" + num(f.latest.right_contrast) + ",\"left_contrast\":" + num(f.latest.left_contrast);
  json += ",\"right_weight\":" + num(f.latest.right_weight) + ",\"left_weight\":" + num(f.latest.left_weight);
  json += ",\"right_reason\":\"" + String(markerDetectionReasonName(f.latest.right_reason)) + "\"";
  json += ",\"left_reason\":\"" + String(markerDetectionReasonName(f.latest.left_reason)) + "\"";
  json += ",\"right_templates\":" + String(f.latest.right_templates) + ",\"left_templates\":" + String(f.latest.left_templates);
  json += ",\"right_candidates\":" + String(f.latest.right_candidates) + ",\"left_candidates\":" + String(f.latest.left_candidates);
  json += ",\"right_ambiguity\":" + num(f.latest.right_ambiguity) + ",\"left_ambiguity\":" + num(f.latest.left_ambiguity);
  json += ",\"right_valid\":" + String(f.latest.right_valid ? "true" : "false");
  json += ",\"left_valid\":" + String(f.latest.left_valid ? "true" : "false");
  json += ",\"right_in_range\":" + String(f.latest.right_in_range ? "true" : "false");
  json += ",\"left_in_range\":" + String(f.latest.left_in_range ? "true" : "false");
  json += ",\"age_ms\":" + String(f.latest.delivered_us ? (esp_timer_get_time() - f.latest.delivered_us) / 1000.0 : -1.0, 1);
  json += ",\"processing_us\":" + String(f.latest.processing_us);
  json += ",\"processing_max_us\":" + String(f.processing_max_us);
  json += ",\"sequence\":" + String(f.latest.sequence);
  json += ",\"frame_valid\":" + String(f.latest.frame_valid ? "true" : "false");
  json += ",\"frame_timestamp_valid\":" + String(f.latest.timestamp_valid ? "true" : "false");
  json += ",\"fps\":" + num(f.fps) + ",\"frames\":" + String(f.count);
  json += ",\"captured_frames\":" + String(f.captured_frames);
  json += ",\"right_marker_failures\":" + String(f.right_marker_failures);
  json += ",\"left_marker_failures\":" + String(f.left_marker_failures);
  json += ",\"failures\":" + String(f.frame_failures) + ",\"overflow\":" + String(f.overflow ? "true" : "false") + "}";
  json += ",\"upright\":{\"stable\":" + String(s.upright_stable ? "true" : "false");
  json += ",\"error_deg\":" + num(s.upright_error_deg) + ",\"accel_g\":" + num(s.accel_norm_g);
  json += ",\"gyro_dps\":" + num(s.gyro_norm_dps) + "}";
  json += ",\"camera\":{\"initialized\":" + String(camera.camera_ok ? "true" : "false");
  json += ",\"receiver_active\":" + String(camera.receiver_active ? "true" : "false");
  json += ",\"xclk_active\":" + String(camera.xclk_active ? "true" : "false");
  json += ",\"xclk_hz\":" + String(camera.xclk_hz);
  json += ",\"capture_us\":" + String(camera.last_capture_us) + ",\"max_capture_us\":" + String(camera.max_capture_us);
  json += ",\"driver_task_core\":" + String(camera.cam_task_core);
  json += ",\"driver_task_priority\":" + String(camera.cam_task_effective_priority) + "}";
  RuntimeDiag::phase(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpMemory);
  json += ",\"memory\":{\"internal_free\":" + String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  json += ",\"internal_min_free\":" + String(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
  json += ",\"internal_largest\":" + String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  json += ",\"dma_free\":" + String(heap_caps_get_free_size(MALLOC_CAP_DMA));
  json += ",\"psram_free\":" + String(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) + "}";
  RuntimeDiag::phase(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpJson);
  json += ",\"control\":{\"steps\":" + String(h.steps);
  json += ",\"sample_deadline_over\":" + String(h.sample_deadline_over);
  json += ",\"sample_deadline_max_us\":" + String(h.sample_deadline_max_us);
  json += ",\"runner_deadline_over\":" + String(h.runner_deadline_over) + "}}";
  server_->sendHeader("Cache-Control", "no-store"); server_->send(200, "application/json", json);
}
void WebUi::manifest() {
  RuntimeDiag::Scope diagnostic(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpManifest);
  const auto s = export_->status();
  char body[420];
  snprintf(body, sizeof(body),
      "{\"phase\":\"%s\",\"token\":\"%s\",\"bytes\":%lu,\"hashed_bytes\":%lu,\"crc32\":%lu,"
      "\"chunk_bytes\":4096,\"filename\":\"%s\",\"error\":\"%s\"}",
      phase(s.phase), s.token, static_cast<unsigned long>(s.bytes), static_cast<unsigned long>(s.hashed_bytes),
      static_cast<unsigned long>(s.crc), s.filename, s.error);
  server_->sendHeader("Cache-Control", "no-store"); server_->send(200, "application/json", body);
}
static bool unsignedArg(const String& text, uint32_t& value) {
  if (!text.length() || text.length() > 10) return false;
  uint64_t n = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    n = n * 10 + text[i] - '0'; if (n > UINT32_MAX) return false;
  }
  value = static_cast<uint32_t>(n); return true;
}
void WebUi::chunk() {
  RuntimeDiag::Scope diagnostic(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpChunk);
  uint32_t offset = 0, length = 0;
  if (!unsignedArg(server_->arg("offset"), offset) || !unsignedArg(server_->arg("length"), length) ||
      !export_->chunk(server_->arg("token").c_str(), offset, length, chunk_buffer_ + 16)) {
    server_->send(409, "text/plain", "export_token_or_range_mismatch"); return;
  }
  const uint32_t header[] = {export_protocol::kChunkMagic, offset, length,
      export_protocol::crc32(0, chunk_buffer_ + 16, length)};
  memcpy(chunk_buffer_, header, 16);
  server_->sendHeader("Cache-Control", "no-store");
  server_->setContentLength(length + 16);
  server_->send(200, "application/octet-stream", "");
  server_->sendContent(reinterpret_cast<const char*>(chunk_buffer_), length + 16);
}

bool WebUi::previewAllowed() const {
  return !control_->snapshot().running && !control_->commandState().pending &&
         export_->status().phase != ImmutableExport::Phase::Building;
}
static String previewMarkerJson(const WhiteMarkerObservation& m) {
  String s;
  s = "{\"valid\":" + String(m.valid ? "true" : "false");
  s += ",\"reason\":\"" + String(markerDetectionReasonName(m.reason)) + "\"";
  s += ",\"x\":" + num(m.center_x_px) + ",\"scan_y\":" + num(m.center_y_px);
  s += ",\"width\":" + String(m.bright_width_px) + ",\"contrast\":" + num(m.peak_contrast);
  s += ",\"weight\":" + num(m.weight_sum) + ",\"candidates\":" + String(m.candidate_count);
  s += ",\"ambiguity_ratio\":" + num(m.ambiguity_ratio);
  s += ",\"alternate_x\":" + num(m.alternate_x_px) + ",\"alternate_y\":" + num(m.alternate_y_px) + "}";
  return s;
}
void WebUi::previewCapture() {
  RuntimeDiag::Scope diagnostic(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpManifest);
  if (!previewAllowed()) { server_->send(409, "text/plain", "preview_requires_idle"); return; }
  FootPreviewInfo p;
  if (!preview_buffer_ || !feet_->copyPreview(preview_buffer_, p)) {
    server_->send(503, "text/plain", "preview_frame_unavailable"); return;
  }
  // HTTP owns this frozen copy until the next capture request. Subsequent
  // camera frames cannot change bytes or metadata during chunked transfer.
  if (++preview_token_ == 0) ++preview_token_;
  const uint32_t crc = export_protocol::crc32(0, preview_buffer_, FootObserver::kPreviewBytes);
  String json; json.reserve(1500);
  json = "{\"revision\":\"" RUNTIME_VERSION "\",\"format\":\"gray8\",\"width\":160,\"height\":120,";
  json += "\"source_width\":320,\"source_height\":240,\"bytes\":" + String(FootObserver::kPreviewBytes);
  json += ",\"token\":" + String(preview_token_) + ",\"crc32\":" + String(crc);
  json += ",\"sequence\":" + String(p.frame.sequence) + ",\"run_id\":" + String(p.frame.run_id);
  char timestamp[24];
  snprintf(timestamp, sizeof(timestamp), "%llu", static_cast<unsigned long long>(p.frame.frame_us));
  json += ",\"frame_us\":" + String(timestamp);
  snprintf(timestamp, sizeof(timestamp), "%llu", static_cast<unsigned long long>(p.frame.delivered_us));
  json += ",\"delivered_us\":" + String(timestamp);
  json += ",\"age_ms\":" + String((esp_timer_get_time() - p.frame.delivered_us) / 1000.0, 1);
  json += ",\"timestamp_valid\":" + String(p.frame.timestamp_valid ? "true" : "false");
  json += ",\"zero_ready\":" + String(p.frame.zero_ready ? "true" : "false");
  json += ",\"zero_reason\":\"" + String(footZeroReasonName(p.frame.zero_reason)) + "\"";
  json += ",\"zero_samples\":" + String(p.zero_samples);
  json += ",\"right_zero_x\":" + num(p.right_zero) + ",\"left_zero_x\":" + num(p.left_zero);
  json += ",\"right_deg\":" + num(p.frame.right_deg) + ",\"left_deg\":" + num(p.frame.left_deg);
  json += ",\"right_valid\":" + String(p.frame.right_valid ? "true" : "false");
  json += ",\"left_valid\":" + String(p.frame.left_valid ? "true" : "false");
  json += ",\"right\":" + previewMarkerJson(p.right) + ",\"left\":" + previewMarkerJson(p.left) + "}";
  server_->sendHeader("Cache-Control", "no-store"); server_->send(200, "application/json", json);
}
void WebUi::previewChunk() {
  RuntimeDiag::Scope diagnostic(RuntimeDiag::Lane::Http, RuntimeDiag::Phase::HttpChunk);
  uint32_t token = 0, offset = 0, length = 0;
  if (!previewAllowed() || !preview_buffer_ || !preview_token_ ||
      !unsignedArg(server_->arg("token"), token) || token != preview_token_ ||
      !unsignedArg(server_->arg("offset"), offset) || !unsignedArg(server_->arg("length"), length) ||
      !export_protocol::validRange(FootObserver::kPreviewBytes, offset, length, export_protocol::kChunkBytes)) {
    server_->send(409, "text/plain", "preview_token_or_range_mismatch"); return;
  }
  memcpy(chunk_buffer_ + 16, preview_buffer_ + offset, length);
  const uint32_t header[] = {export_protocol::kChunkMagic, offset, length,
      export_protocol::crc32(0, chunk_buffer_ + 16, length)};
  memcpy(chunk_buffer_, header, 16);
  server_->sendHeader("Cache-Control", "no-store");
  server_->setContentLength(length + 16); server_->send(200, "application/octet-stream", "");
  server_->sendContent(reinterpret_cast<const char*>(chunk_buffer_), length + 16);
}
