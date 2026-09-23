#include "rwlog_diag_server_task.h"

#include <Arduino.h>
#include <WiFi.h>

#include "rwlog_download_diag.h"

namespace {

constexpr uint16_t kRwlogDiagPort = 82;
constexpr BaseType_t kRwlogDiagCore = 0;
constexpr UBaseType_t kRwlogDiagPriority = 1;
constexpr uint32_t kRwlogDiagStackBytes = 6144;
constexpr uint32_t kRequestTimeoutMs = 1200;
constexpr size_t kMaxRequestLineBytes = 192;

WiFiServer g_rwlog_diag_server(kRwlogDiagPort);
TaskHandle_t g_rwlog_diag_task = nullptr;

void sendHttp(WiFiClient& client, int code, const char* status,
              const char* content_type, const String& body) {
  char header[256];
  const int n = snprintf(
      header, sizeof(header),
      "HTTP/1.1 %d %s\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %u\r\n"
      "Connection: close\r\n"
      "Cache-Control: no-store\r\n"
      "\r\n",
      code, status, content_type, static_cast<unsigned>(body.length()));
  if (n > 0) client.write(reinterpret_cast<const uint8_t*>(header), static_cast<size_t>(n));
  client.write(reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
  client.flush();
}

void serveClient(WiFiClient client) {
  const uint32_t opened_ms = millis();
  char first_line[kMaxRequestLineBytes] = {};
  size_t used = 0;
  bool line_done = false;
  while (client.connected() && !line_done &&
         static_cast<uint32_t>(millis() - opened_ms) < kRequestTimeoutMs) {
    while (client.available() > 0) {
      const int v = client.read();
      if (v < 0) break;
      const char ch = static_cast<char>(v);
      if (ch == '\n') { line_done = true; break; }
      if (ch != '\r' && used + 1 < sizeof(first_line)) {
        first_line[used++] = ch;
        first_line[used] = '\0';
      }
    }
    if (!line_done) vTaskDelay(pdMS_TO_TICKS(2));
  }
  if (strncmp(first_line, "GET /rwlog-download-health ", 27) == 0 ||
      strncmp(first_line, "GET / ", 6) == 0) {
    sendHttp(client, 200, "OK", "application/json; charset=utf-8",
             rwlogDownloadDiagnosticsJson());
  } else {
    sendHttp(client, 404, "Not Found", "application/json; charset=utf-8",
             String("{\"error\":\"use_/rwlog-download-health\",\"port\":82}"));
  }
  client.stop();
}

void rwlogDiagTask(void*) {
  g_rwlog_diag_server.begin();
  for (;;) {
    WiFiClient incoming = g_rwlog_diag_server.available();
    if (incoming) serveClient(incoming);
    else vTaskDelay(pdMS_TO_TICKS(10));
  }
}
}  // namespace

bool rwlogDiagServerBegin() {
  if (g_rwlog_diag_task) return true;
  return xTaskCreatePinnedToCore(
      rwlogDiagTask, "rwlog_diag82", kRwlogDiagStackBytes, nullptr,
      kRwlogDiagPriority, &g_rwlog_diag_task, kRwlogDiagCore) == pdPASS;
}
