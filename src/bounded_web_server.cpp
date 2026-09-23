#include "bounded_web_server.h"

#include <Arduino.h>
#include <errno.h>
#include <lwip/sockets.h>

namespace {

// Keep each lwIP enqueue small enough to remain comfortable even after the
// camera driver and an HTTP connection have fragmented internal/DMA-capable RAM.
constexpr size_t kMaxProgmemWriteBytes = 256;

// Do not fill the TCP send queue back-to-back. A small successful-send pacing
// interval lets Wi-Fi/lwIP transmit and process ACKs before the next enqueue.
constexpr uint32_t kSuccessPaceMs = 2;

// EAGAIN/EWOULDBLOCK/ENOMEM mean "try again after the stack drains", not a
// failed HTTP response. Bound a no-progress interval so a dead peer cannot
// block the Arduino HTTP task forever.
constexpr uint32_t kRetryDelayMs = 2;
constexpr uint32_t kMaxNoProgressMs = 3000;

bool retryableSocketError(int error) {
  return error == EAGAIN || error == EWOULDBLOCK || error == ENOMEM;
}

}  // namespace

size_t BoundedWriteWebServer::_currentClientWrite_P(PGM_P buffer, size_t length) {
  if (length == 0) return 0;

  const int socket_fd = _currentClient.fd();
  if (socket_fd < 0 || !_currentClient.connected()) return 0;

  Serial.printf(
      "NETDBG,web_p_begin,ms=%lu,bytes=%u,chunk=%u,pace_ms=%u,retry_ms=%u\n",
      static_cast<unsigned long>(millis()),
      static_cast<unsigned>(length),
      static_cast<unsigned>(kMaxProgmemWriteBytes),
      static_cast<unsigned>(kSuccessPaceMs),
      static_cast<unsigned>(kRetryDelayMs));

  size_t total = 0;
  uint32_t sends = 0;
  uint32_t retries = 0;
  uint32_t last_progress_ms = millis();
  bool failed = false;
  int last_error = 0;

  while (total < length) {
    const size_t remaining = length - total;
    const size_t want =
        remaining < kMaxProgmemWriteBytes ? remaining : kMaxProgmemWriteBytes;

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

      if ((sends & 0x07U) == 0U || total == length) {
        Serial.printf(
            "NETDBG,web_p_progress,sends=%lu,retries=%lu,sent=%u,total=%u\n",
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
            "NETDBG,web_p_stall,offset=%u,want=%u,errno=%d,retries=%lu,connected=%u\n",
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
        "NETDBG,web_p_socket_error,offset=%u,want=%u,result=%d,errno=%d,connected=%u\n",
        static_cast<unsigned>(total),
        static_cast<unsigned>(want),
        result,
        last_error,
        _currentClient.connected() ? 1U : 0U);
    failed = true;
    break;
  }

  Serial.printf(
      "NETDBG,web_p_end,result=%s,sends=%lu,retries=%lu,sent=%u,total=%u,last_errno=%d,connected=%u\n",
      (!failed && total == length) ? "OK" : "FAILED",
      static_cast<unsigned long>(sends),
      static_cast<unsigned long>(retries),
      static_cast<unsigned>(total),
      static_cast<unsigned>(length),
      last_error,
      _currentClient.connected() ? 1U : 0U);

  return total;
}
