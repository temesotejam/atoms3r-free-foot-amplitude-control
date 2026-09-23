#include "host_v46o/Arduino.h"
#include <cassert>
#include <iostream>
#define private public
#include "../src/run_control_worker.h"
#undef private
struct Model {bool running=false;unsigned steps=0;RunControlWorker* worker=nullptr;};
static bool step(void* p) {
  auto& m=*static_cast<Model*>(p); ++m.steps;
  if(m.worker->takeStopRequest()) m.running=false;
  const auto command=m.worker->takeCommand();
  if(command==RunControlWorker::Command::Start){m.running=true;m.worker->beginRunAudit();m.worker->completeCommand(true,"started");}
  if(command==RunControlWorker::Command::Clear){m.running=false;m.worker->completeCommand(true,"cleared");}
  if(m.running){const auto t=micros();host_us+=750;m.worker->recordStep(t,100,600,750);}
  return m.running;
}
static void capture(void* p,RunControlSnapshot& s) {s.running=static_cast<Model*>(p)->running;}
int main(){
  RunControlWorker w;Model m;m.worker=&w;
  assert(!w.request(RunControlWorker::Command::Start));
  assert(w.begin(step,capture,&m));assert(!w.begin(step,capture,&m));
  w.oneStep();w.oneStep();assert(m.steps==2&&!w.active()); // permanent idle owner
  assert(w.request(RunControlWorker::Command::Start));
  assert(!w.request(RunControlWorker::Command::Clear)); // pending start blocks clear
  w.oneStep();assert(w.active()&&w.commandState().ok);
  assert(!w.request(RunControlWorker::Command::Start));
  assert(!w.request(RunControlWorker::Command::Clear));
  for(int i=0;i<100;++i)w.oneStep();
  assert(w.requestStop());w.oneStep();assert(!w.active());
  const auto audit=w.healthSnapshot();for(int i=0;i<100;++i)w.oneStep();
  assert(w.healthSnapshot().steps==audit.steps); // idle cannot alter run deadlines
  assert(w.request(RunControlWorker::Command::Start));
  assert(w.requestStop());w.oneStep();assert(!w.active()&&!w.commandState().pending);
  assert(!w.commandState().ok); // STOP cancels queued START
  assert(w.request(RunControlWorker::Command::Clear));w.oneStep();assert(!w.active());
  w.beginRunAudit();w.last_step_start_us_=UINT32_MAX-200;host_us=300;
  w.recordStep(300,1,2,3);assert(w.healthSnapshot().max_period_us==501);
  std::cout<<"permanent owner, command exclusion, STOP precedence, sealed audit and wrap PASS\n";
}
