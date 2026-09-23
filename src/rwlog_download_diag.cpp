#include "rwlog_download_diag.h"

#include <errno.h>
#include <esp_heap_caps.h>

namespace {
struct RwlogDownloadDiag {
  uint32_t attempts = 0;
  bool active = false;
  bool ok = false;
  const char* result = "never";
  const char* phase = "none";
  const char* failure_reason = "";
  uint32_t expected_bytes = 0;
  uint32_t metadata_bytes = 0;
  uint32_t sample_bytes = 0;
  uint32_t bytes_sent = 0;
  uint32_t last_request_bytes = 0;
  uint32_t last_progress_bytes = 0;
  uint32_t retry_count = 0;
  uint32_t eagain_count = 0;
  uint32_t enomem_count = 0;
  uint32_t zero_result_count = 0;
  uint32_t shrink_count = 0;
  uint32_t chunk_limit = 0;
  int last_errno = 0;
  uint32_t started_ms = 0;
  uint32_t duration_ms = 0;
  uint32_t internal_free_begin = 0;
  uint32_t dma_free_begin = 0;
  uint32_t psram_free_begin = 0;
  uint32_t internal_free_end = 0;
  uint32_t dma_free_end = 0;
  uint32_t psram_free_end = 0;
};

RwlogDownloadDiag g_diag;
portMUX_TYPE g_diag_mux = portMUX_INITIALIZER_UNLOCKED;
}

void rwlogDownloadDiagBegin(uint32_t expected_bytes, uint32_t metadata_bytes,
                            uint32_t sample_bytes) {
  const uint32_t now_ms = millis();
  const uint32_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
  const uint32_t psram_free = ESP.getFreePsram();
  portENTER_CRITICAL(&g_diag_mux);
  const uint32_t attempts = g_diag.attempts + 1;
  g_diag = RwlogDownloadDiag{};
  g_diag.attempts = attempts;
  g_diag.active = true;
  g_diag.result = "active";
  g_diag.phase = "prepare_done";
  g_diag.expected_bytes = expected_bytes;
  g_diag.metadata_bytes = metadata_bytes;
  g_diag.sample_bytes = sample_bytes;
  g_diag.started_ms = now_ms;
  g_diag.internal_free_begin = internal_free;
  g_diag.dma_free_begin = dma_free;
  g_diag.psram_free_begin = psram_free;
  portEXIT_CRITICAL(&g_diag_mux);
}

void rwlogDownloadDiagSetPhase(const char* phase) {
  portENTER_CRITICAL(&g_diag_mux);
  g_diag.phase = phase ? phase : "unknown";
  portEXIT_CRITICAL(&g_diag_mux);
}

void rwlogDownloadDiagRequest(size_t requested_bytes, size_t chunk_limit) {
  portENTER_CRITICAL(&g_diag_mux);
  g_diag.last_request_bytes = static_cast<uint32_t>(requested_bytes);
  g_diag.chunk_limit = static_cast<uint32_t>(chunk_limit);
  portEXIT_CRITICAL(&g_diag_mux);
}

void rwlogDownloadDiagProgress(size_t written_bytes) {
  portENTER_CRITICAL(&g_diag_mux);
  g_diag.last_progress_bytes = static_cast<uint32_t>(written_bytes);
  g_diag.bytes_sent += static_cast<uint32_t>(written_bytes);
  g_diag.last_errno = 0;
  portEXIT_CRITICAL(&g_diag_mux);
}

void rwlogDownloadDiagRetry(int error_code) {
  portENTER_CRITICAL(&g_diag_mux);
  ++g_diag.retry_count;
  g_diag.last_errno = error_code;
  if (error_code == EAGAIN || error_code == EWOULDBLOCK) ++g_diag.eagain_count;
  else if (error_code == ENOMEM) ++g_diag.enomem_count;
  else if (error_code == 0) ++g_diag.zero_result_count;
  portEXIT_CRITICAL(&g_diag_mux);
}

void rwlogDownloadDiagShrink(size_t new_chunk_limit) {
  portENTER_CRITICAL(&g_diag_mux);
  ++g_diag.shrink_count;
  g_diag.chunk_limit = static_cast<uint32_t>(new_chunk_limit);
  portEXIT_CRITICAL(&g_diag_mux);
}

void rwlogDownloadDiagFail(const char* reason, int error_code) {
  portENTER_CRITICAL(&g_diag_mux);
  g_diag.ok = false;
  g_diag.result = "failed";
  g_diag.failure_reason = reason ? reason : "unknown";
  g_diag.last_errno = error_code;
  portEXIT_CRITICAL(&g_diag_mux);
}

void rwlogDownloadDiagFinish(bool ok) {
  const uint32_t now_ms = millis();
  const uint32_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
  const uint32_t psram_free = ESP.getFreePsram();
  portENTER_CRITICAL(&g_diag_mux);
  g_diag.active = false;
  g_diag.ok = ok;
  if (ok) {
    g_diag.result = "ok";
    g_diag.failure_reason = "";
    g_diag.phase = "done";
    g_diag.last_errno = 0;
  }
  g_diag.duration_ms = now_ms - g_diag.started_ms;
  g_diag.internal_free_end = internal_free;
  g_diag.dma_free_end = dma_free;
  g_diag.psram_free_end = psram_free;
  portEXIT_CRITICAL(&g_diag_mux);
}

String rwlogDownloadDiagnosticsJson() {
  RwlogDownloadDiag d;
  portENTER_CRITICAL(&g_diag_mux);
  d = g_diag;
  portEXIT_CRITICAL(&g_diag_mux);

  String s;
  s.reserve(1000);
  s = "{\"revision\":\"rwlog_download_diag_v7_independent_port82_20260923\"";
  s += ",\"attempts\":" + String(d.attempts);
  s += ",\"active\":" + String(d.active ? "true" : "false");
  s += ",\"ok\":" + String(d.ok ? "true" : "false");
  s += ",\"result\":\"" + String(d.result) + "\"";
  s += ",\"phase\":\"" + String(d.phase) + "\"";
  s += ",\"socket_source_memory\":\"internal_stack_staging\"";
  s += ",\"metadata_profile\":\"autonomous_compact_v1\"";
  s += ",\"success_pace_ms\":2";
  s += ",\"failure_reason\":\"" + String(d.failure_reason) + "\"";
  s += ",\"expected_bytes\":" + String(d.expected_bytes);
  s += ",\"metadata_bytes\":" + String(d.metadata_bytes);
  s += ",\"sample_bytes\":" + String(d.sample_bytes);
  s += ",\"bytes_sent\":" + String(d.bytes_sent);
  s += ",\"last_request_bytes\":" + String(d.last_request_bytes);
  s += ",\"last_progress_bytes\":" + String(d.last_progress_bytes);
  s += ",\"chunk_limit\":" + String(d.chunk_limit);
  s += ",\"retry_count\":" + String(d.retry_count);
  s += ",\"eagain_count\":" + String(d.eagain_count);
  s += ",\"enomem_count\":" + String(d.enomem_count);
  s += ",\"zero_result_count\":" + String(d.zero_result_count);
  s += ",\"shrink_count\":" + String(d.shrink_count);
  s += ",\"last_errno\":" + String(d.last_errno);
  s += ",\"duration_ms\":" + String(d.duration_ms);
  s += ",\"internal_free_begin\":" + String(d.internal_free_begin);
  s += ",\"dma_free_begin\":" + String(d.dma_free_begin);
  s += ",\"psram_free_begin\":" + String(d.psram_free_begin);
  s += ",\"internal_free_end\":" + String(d.internal_free_end);
  s += ",\"dma_free_end\":" + String(d.dma_free_end);
  s += ",\"psram_free_end\":" + String(d.psram_free_end);
  return s + "}";
}
