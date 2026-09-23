#include "runtime_diagnostics.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "diagnostic_journal.h"
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdarg.h>

namespace RuntimeDiag {
namespace {
constexpr uint32_t kLanes = static_cast<uint32_t>(Lane::Count);
struct Probe { uint32_t phase, last_ms, beats, detail, stack_free, core; };
struct Memory { uint32_t at_ms, internal_free, internal_min, largest, dma_free, psram_free; };
struct Boot { uint32_t id, reset, stage, failures; };
struct Sample {
  uint32_t boot, at_ms, wifi_event, wifi_events, clients, ap_active;
  Probe lanes[kLanes]; Memory memory;
};
static_assert(__atomic_always_lock_free(sizeof(uint32_t), nullptr), "Diagnostic probes must be lock free");
RTC_NOINIT_ATTR volatile diagnostic_journal::Journal<Boot, 0x55444231> boot_journal;
RTC_NOINIT_ATTR volatile diagnostic_journal::Journal<Sample, 0x55445331> sample_journal;
Probe probes[kLanes]{};
Memory memory{};
uint32_t event_id = 0, event_count = 0, clients = 0, ap_active = 0;
uint32_t boot_stage = 0, boot_failures = 0;
Boot current_boot{}, previous_boot{};
Sample previous_sample{};
bool have_previous_boot = false, have_previous_sample = false;
bool observer_started = false;
uint32_t last_sample_ms = 0, last_report_ms = 0, report_started_ms = 0, dropped = 0;
char report[3072];
size_t report_length = 0, report_sent = 0;

uint32_t get(const uint32_t& value) { return __atomic_load_n(&value, __ATOMIC_RELAXED); }
void put(uint32_t& dest, uint32_t value) { __atomic_store_n(&dest, value, __ATOMIC_RELAXED); }
const char* stageName(uint32_t stage) {
  static const char* const names[] = {"usb_window", "m5", "logger", "imu", "camera", "roller",
      "runner", "control", "feet", "export", "web", "ready"};
  return stage < sizeof(names) / sizeof(names[0]) ? names[stage] : "unknown";
}
const char* phaseName(uint32_t p) {
  static const char* const names[] = {"unseen", "wait", "control_service", "control_imu", "control_command",
      "control_runner", "control_finish", "control_idle", "snapshot", "publish", "imu_read", "imu_audit",
      "camera_capture", "camera_process", "camera_release", "camera_stop", "http_poll", "http_root",
      "http_status", "http_command", "http_manifest", "http_chunk", "http_json", "http_memory", "http_send",
      "http_close", "roller_init", "roller_io", "export_metadata", "export_crc"};
  static_assert(sizeof(names) / sizeof(names[0]) == static_cast<unsigned>(Phase::ExportCrc) + 1, "phase names");
  return p < sizeof(names) / sizeof(names[0]) ? names[p] : "unknown";
}
const char* reason(uint32_t reset) {
  switch (reset) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INTERRUPT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "OTHER_WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}
Sample capture(uint32_t now) {
  Sample s{}; s.boot = current_boot.id; s.at_ms = now;
  s.wifi_event = get(event_id); s.wifi_events = get(event_count);
  s.clients = get(clients); s.ap_active = get(ap_active);
  // Deliberately independent atomic fields: no live snapshot/spinlock or
  // unbounded seqlock retry if a producer hangs midway through its update.
  for (uint32_t i = 0; i < kLanes; ++i) {
    const auto& p = probes[i]; auto& d = s.lanes[i];
    d.phase = get(p.phase); d.last_ms = get(p.last_ms); d.beats = get(p.beats);
    d.detail = get(p.detail); d.stack_free = get(p.stack_free); d.core = get(p.core);
  }
  s.memory.at_ms = get(memory.at_ms); s.memory.internal_free = get(memory.internal_free);
  s.memory.internal_min = get(memory.internal_min); s.memory.largest = get(memory.largest);
  s.memory.dma_free = get(memory.dma_free); s.memory.psram_free = get(memory.psram_free);
  return s;
}
void append(const char* format, ...) {
  if (report_length >= sizeof(report) - 1) return;
  va_list args; va_start(args, format);
  const int n = vsnprintf(report + report_length, sizeof(report) - report_length, format, args);
  va_end(args);
  if (n > 0) report_length += static_cast<size_t>(n) < sizeof(report) - report_length
      ? static_cast<size_t>(n) : sizeof(report) - report_length - 1;
}
void appendSample(const char* label, const Sample& s) {
  static const char* const names[] = {"control", "imu", "camera", "http", "roller", "export"};
  append("USBDBG,%s,boot=%u,ms=%u,ap=%u,clients=%u,wifi_event=%u,wifi_events=%u\n",
      label, s.boot, s.at_ms, s.ap_active, s.clients, s.wifi_event, s.wifi_events);
  for (uint32_t i = 0; i < kLanes; ++i) {
    const auto& p = s.lanes[i];
    append("USBDBG,%s,%s,phase=%s,seen=%u,age_ms=%u,beats=%u,detail=%u,stack_bytes=%u,core=%u\n",
        label, names[i], phaseName(p.phase), p.beats != 0, s.at_ms - p.last_ms,
        p.beats, p.detail, p.stack_free, p.core);
  }
  const auto& m = s.memory;
  append("USBDBG,%s,heap_at_ms=%u,internal=%u,min=%u,largest=%u,dma=%u,psram=%u\n",
      label, m.at_ms, m.internal_free, m.internal_min, m.largest, m.dma_free, m.psram_free);
}
void tick() {
  const uint32_t now = millis();
  if (now - last_sample_ms >= 250 || !last_sample_ms) {
    sample_journal.save(capture(now)); last_sample_ms = now;
  }
  if (report_sent < report_length && now - report_started_ms > 2000) {
    report_sent = report_length; ++dropped;
  }
  if (report_sent == report_length && (now - last_report_ms >= 1000 || !last_report_ms)) {
    report_sent = report_length = 0; report_started_ms = last_report_ms = now;
    append("\nUSBDBG,boot,version=" RUNTIME_VERSION ",id=%u,reset=%s,reset_id=%u,stage=%s,failures=0x%08x,observer=%u,observer_stack_bytes=%u,usb_dropped=%u\n",
        current_boot.id, reason(current_boot.reset), current_boot.reset, stageName(get(boot_stage)),
        get(boot_failures), observer_started, uxTaskGetStackHighWaterMark(nullptr), dropped);
    if (have_previous_boot) {
      append("USBDBG,previous_boot,id=%u,stage=%s,failures=0x%08x,rtc_sample=%u\n",
          previous_boot.id, stageName(previous_boot.stage), previous_boot.failures, have_previous_sample);
      if (have_previous_sample) appendSample("previous", previous_sample);
    } else append("USBDBG,previous_boot,unavailable=1\n");
    appendSample("live", capture(now));
  }
  // Native HWCDC has a bounded ring. Never flush or wait for a PC to connect.
  // Timeout 0 is unsafe in this core's unsigned retry loop; use 1 ms plus a
  // checked 128-byte write. Leave room for the existing one-off MEKF message.
  if (report_sent < report_length && Serial) {
    const int room = Serial.availableForWrite();
    if (room > 256) {
      const size_t left = report_length - report_sent;
      report_sent += Serial.write(reinterpret_cast<const uint8_t*>(report + report_sent), left < 128 ? left : 128);
    }
  }
}
void observer(void*) { for (;;) { tick(); vTaskDelay(pdMS_TO_TICKS(20)); } }
}

void boot(Stage stage) {
  current_boot.stage = static_cast<uint32_t>(stage);
  put(boot_stage, current_boot.stage); boot_journal.save(current_boot);
  if (stage != Stage::UsbWindow) sampleMemory();
}
void result(bool ok) {
  if (!ok) {
    current_boot.failures |= 1U << current_boot.stage;
    put(boot_failures, current_boot.failures); boot_journal.save(current_boot);
  }
}
void begin() {
  current_boot.reset = static_cast<uint32_t>(esp_reset_reason());
  // RTC contents after power-on are not evidence of the preceding power loss.
  have_previous_boot = current_boot.reset != ESP_RST_POWERON && boot_journal.latest(previous_boot) >= 0;
  have_previous_sample = have_previous_boot && sample_journal.latest(previous_sample) >= 0 &&
      previous_sample.boot == previous_boot.id;
  current_boot.id = have_previous_boot ? previous_boot.id + 1 : 1;
  boot_journal.clear(); sample_journal.clear(); boot(Stage::UsbWindow);
  observer_started = true;
  if (xTaskCreatePinnedToCore(observer, "usb_diagnostics", 6144, nullptr, 2, nullptr, 0) != pdPASS) {
    observer_started = false; result(false);
  }
}
void phase(Lane lane, Phase p) { put(probes[static_cast<uint32_t>(lane)].phase, static_cast<uint32_t>(p)); }
Phase currentPhase(Lane lane) { return static_cast<Phase>(get(probes[static_cast<uint32_t>(lane)].phase)); }
void beat(Lane lane, uint32_t detail) {
  auto& p = probes[static_cast<uint32_t>(lane)];
  const uint32_t now = millis();
  // Stack scans happen in their owner once a second, never from another task.
  static uint32_t stack_ms[kLanes]{};
  auto& last = stack_ms[static_cast<uint32_t>(lane)];
  if (now - last >= 1000 || !get(p.beats)) {
    put(p.stack_free, uxTaskGetStackHighWaterMark(nullptr)); put(p.core, xPortGetCoreID()); last = now;
  }
  put(p.detail, detail); put(p.last_ms, now); put(p.beats, get(p.beats) + 1);
}
void sampleMemory() {
  const uint32_t now = millis();
  if (now - get(memory.at_ms) < 1000) return;
  Scope diagnostic(Lane::Http, Phase::HttpMemory);
  put(memory.internal_free, heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  put(memory.internal_min, heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
  put(memory.largest, heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  put(memory.dma_free, heap_caps_get_free_size(MALLOC_CAP_DMA));
  put(memory.psram_free, heap_caps_get_free_size(MALLOC_CAP_SPIRAM)); put(memory.at_ms, now);
}
void wifiEvent(uint32_t event, int change, int active) {
  put(event_id, event); put(event_count, get(event_count) + 1);
  if (active >= 0) { put(ap_active, active); if (!active) put(clients, 0); }
  if (change > 0) put(clients, get(clients) + 1);
  else if (change < 0 && get(clients)) put(clients, get(clients) - 1);
}
void pollFallback() { if (!observer_started) tick(); }
}
#endif
