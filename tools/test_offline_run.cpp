#include <cassert>
#include <iostream>
#include "offline_run_session.h"
#include "usb_diag_control.h"
#include "runtime_diagnostics.h"
using Session = OfflineRunSession;
struct Host {
  bool radio = true, active = false, stop_ok = true, queue_ok = true, restore_ok = true;
  int stopped = 0, queued = 0, cancelled = 0, restored = 0, ready = 0;
  Session::StartResult result = Session::StartResult::Pending;
  void stopServer() { ++stopped; }
  bool stopRadio() { return stop_ok; }
  bool radioActive() { return radio; }
  bool queueStart() { ++queued; return queue_ok; }
  Session::StartResult startResult() { return result; }
  bool runActive() { return active; }
  void cancelStart() { ++cancelled; }
  bool restoreTransport() { ++restored; return restore_ok; }
  void transportReady() { ++ready; }
};
void enter(Session& s, Host& h, uint32_t origin = 0) {
  assert(s.queue(origin)); assert(!s.queue(origin));
  s.update(origin+349,h); assert(h.stopped==0 && h.queued==0);
  s.update(origin+350,h); assert(h.stopped==1 && h.queued==0);
  s.update(origin+400,h); assert(h.queued==0); // Driver event has not arrived.
  h.radio=false; s.update(origin+450,h); assert(h.queued==1);
}
int main() {
  for (uint32_t origin : {0U, 0xffffff00U}) {
    Session s; Host h; enter(s,h,origin);
    h.result=Session::StartResult::Started; h.active=true;
    s.update(origin+500,h); assert(!s.serving() && h.restored==0);
    s.update(origin+46000,h); assert(h.restored==0); // Never resume on a display timer.
    h.active=false; s.update(origin+47000,h);
    h.restore_ok=false; s.update(origin+47020,h); assert(h.restored==1);
    s.update(origin+47500,h); assert(h.restored==1);
    h.restore_ok=true; s.update(origin+48020,h);
    assert(!s.busy() && h.ready==1 && h.queued==1);
    s.update(origin+100000,h); assert(h.stopped==1 && h.restored==2);
    assert(s.queue(origin+100001)); // A second explicitly requested run works.
  }
  { Session s; Host h; h.stop_ok=false; assert(s.queue(0)); s.update(350,h); s.update(400,h);
    assert(!s.busy() && h.queued==0 && s.error()==std::string("wifi_stop_failed")); }
  { Session s; Host h; assert(s.queue(0)); s.update(350,h); s.update(2350,h); s.update(2400,h);
    assert(!s.busy() && h.queued==0 && s.error()==std::string("wifi_stop_timeout")); }
  { Session s; Host h; h.queue_ok=false; enter(s,h); s.update(500,h); assert(!s.busy()); }
  { Session s; Host h; enter(s,h); h.result=Session::StartResult::Rejected;
    s.update(500,h); s.update(520,h); assert(!s.busy()); }
  { Session s; Host h; enter(s,h); h.result=Session::StartResult::Started;
    s.update(500,h); s.update(520,h); assert(!s.busy()); } // Immediate ESTOP/sealed run.
  { Session s; Host h; enter(s,h); s.update(5450,h); s.update(5470,h);
    assert(h.cancelled==1 && h.restored==0);
    h.active=true; h.result=Session::StartResult::Started; s.update(5490,h); assert(h.restored==0);
    h.active=false; s.update(5510,h); s.update(5530,h); assert(!s.busy()); }
  assert(!RuntimeDiag::enabled() && !RuntimeDiag::heavyAllowed());
  RuntimeDiag::setEnabled(true); assert(RuntimeDiag::heavyAllowed());
  RuntimeDiag::setRunActive(true); assert(!RuntimeDiag::heavyAllowed());
  RuntimeDiag::setEnabled(false); RuntimeDiag::setRunActive(false); assert(!RuntimeDiag::heavyAllowed());
  usb_diag::Parser parser;
  auto line=[&](const char* p) { usb_diag::Command r{}; while(*p) r=parser.feed(*p++); return r; };
  assert(line("DIAG ON\r\n")==usb_diag::Command::Enable);
  assert(line("DIAG OFF\n")==usb_diag::Command::Disable);
  assert(line("DIAG STATUS\n")==usb_diag::Command::Status);
  assert(line("DIAG ONxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n")==usb_diag::Command::Unknown);
  assert(line("DIAG ON\n")==usb_diag::Command::Enable);
  std::cout << "offline lifecycle: acknowledgement drain, AP events, start rejection/timeout, early STOP, restore retry, repeated runs, clock wrap; USB opt-in PASS\n";
}
