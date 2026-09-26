#include "control_latency.h"
namespace control_latency {
Profile profile;
namespace {
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Span poll_span, io_span;
uint32_t control_progress = 0, io_progress_at_start = 0;
}
void pollBegin() { portENTER_CRITICAL(&mux); poll_span.begin(micros()); portEXIT_CRITICAL(&mux); }
void pollEnd() { portENTER_CRITICAL(&mux); poll_span.end(micros()); portEXIT_CRITICAL(&mux); }
void ioBegin() {
  portENTER_CRITICAL(&mux);
  io_span.begin(micros()); io_progress_at_start = control_progress;
  portEXIT_CRITICAL(&mux);
}
void ioEnd(bool ok) {
  portENTER_CRITICAL(&mux);
  const uint32_t us = micros() - io_span.start;
  io_span.end(micros());
  // active is owned on the SAME core by the lower-priority controller. No
  // allocations or JSON here; stopped export never reads the live span state.
  if (profile.active) {
    ++profile.io_calls; if (!ok) ++profile.io_failures;
    if (control_progress != io_progress_at_start) ++profile.io_calls_with_control_progress;
    if (us > profile.io_max_us) profile.io_max_us = us;
  }
  portEXIT_CRITICAL(&mux);
}
void progress() { portENTER_CRITICAL(&mux); ++control_progress; portEXIT_CRITICAL(&mux); }
void setActive(bool active) { portENTER_CRITICAL(&mux); profile.active = active; portEXIT_CRITICAL(&mux); }
Activity activity(uint32_t* timestamp) {
  portENTER_CRITICAL(&mux);
  const uint32_t now = micros();
  const Activity out{poll_span.at(now), io_span.at(now), poll_span.count};
  if (timestamp) *timestamp = now;
  portEXIT_CRITICAL(&mux); return out;
}
void mark(Point point) {
  progress();
  if (profile.current.sequence) {
    uint32_t now; const auto a = activity(&now); profile.mark(point, now, a);
  }
}
void Profile::receive(bool measurement, uint32_t seq, uint32_t sample, uint32_t poll,
                      uint32_t now, Activity a) {
  current = Row{};
  if (!measurement) return;
  if (!epoch) epoch = now;
  current.sequence = seq; current.sample_us = sample;
  current.poll_to_sample_us = sample - poll;
  received_activity = a;
  poll_to_sample.add(current.poll_to_sample_us);
  mark(Received, now, a);
}
void Profile::mark(Point point, uint32_t now, Activity a) {
  if (!current.sequence || (current.valid & (1U << point))) return;
  current.age[point] = now - current.sample_us;
  current.valid |= 1U << point;
  age[point].add(current.age[point]);
  if (point == Ready) {
    current.polls_to_ready = a.polls - received_activity.polls;
    current.poll_overlap_to_ready_us = a.poll_us - received_activity.poll_us;
    current.io_overlap_to_ready_us = a.io_us - received_activity.io_us;
    poll_to_ready.add(current.poll_overlap_to_ready_us);
    io_to_ready.add(current.io_overlap_to_ready_us);
  }
}
void Profile::finish(uint32_t now, Activity a) {
  if (!current.sequence) return;
  mark(Done, now, a);
  current.poll_overlap_to_done_us = a.poll_us - received_activity.poll_us;
  current.io_overlap_to_done_us = a.io_us - received_activity.io_us;
  const uint32_t bucket = static_cast<uint32_t>(now - epoch) / 1000000;
  if (bucket >= kBuckets) ++outside_buckets;
  else if (!worst[bucket].sequence || current.age[Done] > worst[bucket].age[Done]) worst[bucket] = current;
  current = Row{};
}
void Profile::appendJson(PsramString& out) const {
  out += "{\"revision\":\"control_latency_04719\",\"origin\":\"host_read_complete_not_sensor_clock\"";
  out += ",\"overlap_semantics\":\"received_to_ready_or_done;poll_and_i2c_call_wall_overlap_not_cpu;io_includes_blocking_setup_and_isr;done_includes_deferred_comparison_and_log\"";
  out += ",\"sample_detail\":\"maximum_done_age_per_1s_bucket_not_all_samples\"";
  out += ",\"io_calls\":" + String(io_calls) + ",\"io_failures\":" + String(io_failures);
  out += ",\"io_calls_with_control_progress\":" + String(io_calls_with_control_progress);
  out += ",\"io_max_us\":" + String(io_max_us) + ",\"outside_buckets\":" + String(outside_buckets);
  static const char* names[] = {"received_age", "derived_age", "mekf_age", "control_ready_age", "decision_age", "done_age"};
  auto stat = [&out](const char* name, const Stat& s) {
    out += ",\"" + String(name) + "\":{\"count\":" + String(s.count);
    out += ",\"sum_us\":" + String(static_cast<double>(s.sum_us), 0);
    out += ",\"max_us\":" + String(s.max_us) + "}";
  };
  for (unsigned i = 0; i < Points; ++i) stat(names[i], age[i]);
  stat("poll_to_sample", poll_to_sample); stat("poll_overlap_to_ready", poll_to_ready);
  stat("io_overlap_to_ready", io_to_ready);
  out += ",\"fields\":\"sequence,sample_us,poll_to_sample_us,received_age,derived_age,mekf_age,control_ready_age,decision_age,done_age,polls_to_ready,poll_overlap_to_ready_us,io_overlap_to_ready_us,poll_overlap_to_done_us,io_overlap_to_done_us,valid_mask\",\"worst_samples\":[";
  bool first = true;
  for (const auto& r : worst) {
    if (!r.sequence) continue;
    if (!first) out += ",";
    first = false;
    out += "[" + String(r.sequence) + "," + String(r.sample_us) + "," + String(r.poll_to_sample_us);
    for (auto v : r.age) out += "," + String(v);
    out += "," + String(r.polls_to_ready) + "," + String(r.poll_overlap_to_ready_us);
    out += "," + String(r.io_overlap_to_ready_us) + "," + String(r.poll_overlap_to_done_us);
    out += "," + String(r.io_overlap_to_done_us) + "," + String(r.valid) + "]";
  }
  out += "]}";
}
}
