#pragma once
#include <stdint.h>

#define RUNTIME_VERSION "0.47.18-direct-q-inverse"

namespace RuntimeDiag {
enum class Lane : uint32_t { Control, Imu, Camera, Http, Roller, Export, Count };
enum class Stage : uint32_t {
  UsbWindow, M5, Logger, Imu, Camera, Roller, Runner, Control, Feet, Export, Web, Ready
};
enum class Phase : uint32_t {
  Unseen, Wait, ControlService, ControlImu, ControlCommand, ControlRunner,
  ControlFinish, ControlIdle, Snapshot, Publish, ImuRead, ImuAudit,
  CameraCapture, CameraProcess, CameraRelease, CameraStop,
  HttpPoll, HttpRoot, HttpStatus, HttpCommand, HttpManifest, HttpChunk,
  HttpJson, HttpMemory, HttpSend, HttpClose, RollerInit, RollerIo,
  ExportMetadata, ExportCrc
};
#if defined(ARDUINO_ARCH_ESP32)
void begin();
void boot(Stage stage);
void result(bool ok);
// Single task owns each lane. These probes never log or acquire application locks.
void phase(Lane lane, Phase phase);
Phase currentPhase(Lane lane);
// Real-time owners defer the periodic stack scan during a run. Heartbeat and
// phase publication continue; the first due idle beat refreshes the watermark.
void beat(Lane lane, uint32_t detail = 0, bool allow_stack_scan = true);
void sampleMemory(); // HTTP owner only; deliberately excluded from the USB observer.
void cameraDriverTask(uint32_t requested, uint32_t allocated, bool created);
void cameraDriverStack(uint32_t free_bytes);
void cameraDriverStopped();
void wifiEvent(uint32_t event, int client_change = 0, int ap_active = -1);
void pollFallback(); // Only used if the independent observer task could not be created.
#else
inline void phase(Lane, Phase) {}
inline Phase currentPhase(Lane) { return Phase::Unseen; }
inline void beat(Lane, uint32_t = 0, bool = true) {}
inline void cameraDriverTask(uint32_t, uint32_t, bool) {}
inline void cameraDriverStack(uint32_t) {}
inline void cameraDriverStopped() {}
#endif
struct Scope {
  Lane lane; Phase previous;
  Scope(Lane l, Phase p) : lane(l), previous(currentPhase(l)) { phase(l, p); }
  ~Scope() { phase(lane, previous); }
};
}
