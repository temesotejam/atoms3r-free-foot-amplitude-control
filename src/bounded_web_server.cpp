#include "bounded_web_server.h"
#include <Arduino.h>
#include <errno.h>
#include <lwip/sockets.h>

size_t BoundedWriteWebServer::pacedWrite(const uint8_t* buffer, size_t length, const char*) {
  const int fd = _currentClient.fd();
  if (fd < 0) return 0;
  size_t total = 0;
  const uint32_t begin = millis();
  // A hard deadline includes partial progress: a slow peer cannot own HTTP
  // indefinitely. The client retries an independently verified small chunk.
  while (total < length && static_cast<uint32_t>(millis() - begin) < 1200) {
    const size_t n = length - total < 1024 ? length - total : 1024;
    const int sent = ::send(fd, buffer + total, n, MSG_DONTWAIT);
    if (sent > 0) total += sent;
    else if (sent == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != ENOMEM && errno != EINTR)) break;
    vTaskDelay(1);
  }
  if (total != length) _currentClient.stop();
  return total;
}
size_t BoundedWriteWebServer::_currentClientWrite(const char* buffer, size_t length) {
  return pacedWrite(reinterpret_cast<const uint8_t*>(buffer), length, "ram");
}
size_t BoundedWriteWebServer::_currentClientWrite_P(PGM_P buffer, size_t length) {
  return pacedWrite(reinterpret_cast<const uint8_t*>(buffer), length, "flash");
}
