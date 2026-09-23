#include "bounded_web_server.h"

#include <Arduino.h>
#include <errno.h>
#include <lwip/sockets.h>

namespace {

constexpr size_t kMaxWriteBytes = 256;
constexpr uint32_t kSuccessPaceMs = 2;
constexpr uint32_t kRetryDelayMs = 2;
constexpr uint32_t kMaxNoProgressMs = 3000;

bool retryableSocketError(int error) {
  return error == EAGAIN || error == EWOULDBLOCK || error == ENOMEM;
}

}  // namespace

size_t BoundedWriteWebServer::pacedWrite(
    const uint8_t* buffer, size_t length, const char* source_tag) {
  if (length == 0) return 0;

  const int socket_fd = _currentClient.fd();
  if (socket_fd < 0 || !_currentClient.connected()) return 0;

  const bool trace = length > kMaxWriteBytes;
  if (trace) {
    Serial.printf(
        "NETDBG,web_write_begin,src=%s,ms=%lu,bytes=%u,chunk=%u,pace_ms=%u,retry_ms=%u\n",
        source_tag,
        static_cast<unsigned long>(millis()),
        static_cast<unsigned>(length),
        static_cast<unsigned>(kMaxWriteBytes),
        static_cast<unsigned>(kSuccessPaceMs),
        static_cast<unsigned>(kRetryDelayMs));
  }

  size_t total = 0;
  uint32_t sends = 0;
  uint32_t retries = 0;
  uint32_t last_progress_ms = millis();
  bool failed = false;
  int last_error = 0;

  while (total < length) {
    const size_t remaining = length - total;
    const size_t want = remaining < kMaxWriteBytes ? remaining : kMaxWriteBytes;

    errno = 0;
    const int result = ::send(
        socket_fd,
        reinterpret_cast<const void*>(buffer + total),
        want,
        MSG_DONTWAIT);

    if (result > 0) {
      total += static_cast<size_t>(result);
      ++sends;
      last_progress_ms = millis();

      if (trace && ((sends & 0x07U) == 0U || total == length)) {
        Serial.printf(
            "NETDBG,web_write_progress,src=%s,sends=%lu,retries=%lu,sent=%u,total=%u\n",
            source_tag,
            static_cast<unsigned long>(sends),
            static_cast<unsigned long>(retries),
            static_cast<unsigned>(total),
            static_cast<unsigned>(length));
      }

      vTaskDelay(pdMS_TO_TICKS(kSuccessPaceMs));
      continue;
    }

    last_error = errno;
    if (result < 0 && retryableSocketError(last_error)) {
      ++retries;
      if (static_cast<uint32_t>(millis() - last_progress_ms) >
          kMaxNoProgressMs) {
        Serial.printf(
            "NETDBG,web_write_stall,src=%s,offset=%u,want=%u,errno=%d,retries=%lu,connected=%u\n",
            source_tag,
            static_cast<unsigned>(total),
            static_cast<unsigned>(want),
            last_error,
            static_cast<unsigned long>(retries),
            _currentClient.connected() ? 1U : 0U);
        failed = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
      continue;
    }

    Serial.printf(
        "NETDBG,web_write_socket_error,src=%s,offset=%u,want=%u,result=%d,errno=%d,connected=%u\n",
        source_tag,
        static_cast<unsigned>(total),
        static_cast<unsigned>(want),
        result,
        last_error,
        _currentClient.connected() ? 1U : 0U);
    failed = true;
    break;
  }

  if (trace || failed) {
    Serial.printf(
        "NETDBG,web_write_end,src=%s,result=%s,sends=%lu,retries=%lu,sent=%u,total=%u,last_errno=%d,connected=%u\n",
        source_tag,
        (!failed && total == length) ? "OK" : "FAILED",
        static_cast<unsigned long>(sends),
        static_cast<unsigned long>(retries),
        static_cast<unsigned>(total),
        static_cast<unsigned>(length),
        last_error,
        _currentClient.connected() ? 1U : 0U);
  }

  return total;
}

size_t BoundedWriteWebServer::_currentClientWrite(
    const char* buffer, size_t length) {
  return pacedWrite(
      reinterpret_cast<const uint8_t*>(buffer), length, "ram");
}

size_t BoundedWriteWebServer::_currentClientWrite_P(
    PGM_P buffer, size_t length) {
  return pacedWrite(
      reinterpret_cast<const uint8_t*>(buffer), length, "progmem");
}
