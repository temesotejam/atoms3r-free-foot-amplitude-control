#include "bounded_web_server.h"

#include <Arduino.h>

namespace {
constexpr size_t kMaxProgmemWriteBytes = 512;
}

size_t BoundedWriteWebServer::_currentClientWrite_P(PGM_P buffer, size_t length) {
  if (length <= kMaxProgmemWriteBytes) {
    return _currentClient.write_P(buffer, length);
  }

  Serial.printf("NETDBG,web_p_begin,ms=%lu,bytes=%u,chunk=%u\n",
                static_cast<unsigned long>(millis()),
                static_cast<unsigned>(length),
                static_cast<unsigned>(kMaxProgmemWriteBytes));

  size_t total = 0;
  uint16_t calls = 0;
  bool failed = false;

  while (total < length) {
    const size_t remaining = length - total;
    const size_t want =
        remaining < kMaxProgmemWriteBytes ? remaining : kMaxProgmemWriteBytes;
    const size_t wrote = _currentClient.write_P(buffer + total, want);
    ++calls;

    if (wrote == 0) {
      Serial.printf(
          "NETDBG,web_p_zero,calls=%u,offset=%u,want=%u,connected=%u\n",
          static_cast<unsigned>(calls),
          static_cast<unsigned>(total),
          static_cast<unsigned>(want),
          _currentClient.connected() ? 1U : 0U);
      failed = true;
      break;
    }

    total += wrote;

    if (wrote != want) {
      Serial.printf(
          "NETDBG,web_p_short,calls=%u,offset=%u,want=%u,wrote=%u,connected=%u\n",
          static_cast<unsigned>(calls),
          static_cast<unsigned>(total - wrote),
          static_cast<unsigned>(want),
          static_cast<unsigned>(wrote),
          _currentClient.connected() ? 1U : 0U);
    }

    if ((calls & 0x03U) == 0U || total == length) {
      Serial.printf("NETDBG,web_p_progress,calls=%u,sent=%u,total=%u\n",
                    static_cast<unsigned>(calls),
                    static_cast<unsigned>(total),
                    static_cast<unsigned>(length));
    }

    // Let higher-priority Wi-Fi/lwIP work run before supplying the next small
    // piece. This does not change response bytes or HTTP semantics.
    taskYIELD();
  }

  Serial.printf(
      "NETDBG,web_p_end,result=%s,calls=%u,sent=%u,total=%u,connected=%u\n",
      (!failed && total == length) ? "OK" : "FAILED",
      static_cast<unsigned>(calls),
      static_cast<unsigned>(total),
      static_cast<unsigned>(length),
      _currentClient.connected() ? 1U : 0U);

  return total;
}
