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
static void capture(void* p,RunControlSnapshot& s) {
  s.running=static_cast<Model*>(p)->running;s.state_id=s.running?3:2;host_us+=100;
}
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
  const auto audit=w.healthSnapshot();
  const auto profile_done=control_work::profile.stages[0][static_cast<uint8_t>(control_work::Stage::Owner)];
  assert(profile_done.count>0&&profile_done.max_us==850); // includes 100-us snapshot after 750-us work
  for(int i=0;i<100;++i)w.oneStep();
  assert(w.healthSnapshot().steps==audit.steps); // idle cannot alter run deadlines
  assert(control_work::profile.stages[0][static_cast<uint8_t>(control_work::Stage::Owner)].count==profile_done.count);
  assert(w.request(RunControlWorker::Command::Start));
  assert(w.requestStop());w.oneStep();assert(!w.active()&&!w.commandState().pending);
  assert(!w.commandState().ok); // STOP cancels queued START
  assert(w.request(RunControlWorker::Command::Clear));w.oneStep();assert(!w.active());
  w.beginRunAudit();w.last_step_start_us_=UINT32_MAX-200;host_us=300;
  w.recordStep(300,1,2,3);assert(w.healthSnapshot().max_period_us==501);
  w.recordSampleCompletion(false,true,0,9000,8000,true,true);
  w.recordSampleCompletion(true,false,0,9000,8000,true,true);
  assert(w.audit_.sample_completion.count==0); // same RUNNING/fresh gate
  const uint32_t stamp=UINT32_MAX-100;
  for(unsigned pulse=0;pulse<2;++pulse)for(unsigned accel=0;accel<2;++accel) {
    const uint32_t elapsed=2499+pulse*2+accel;
    w.recordSampleCompletion(true,true,stamp,stamp+elapsed,elapsed-10,pulse,accel);
    const auto& c=w.audit_.input_cohorts[pulse][accel];
    assert(c.count==1 && c.sum==elapsed && c.maximum==elapsed);
    assert(c.over==(elapsed>2500)); // equality is in budget, no tolerance
  }
  assert(w.audit_.sample_completion.count==4 && w.audit_.sample_completion.over==2);
  assert(w.audit_.sample_completion.sum==10002 && w.audit_.sample_completion.maximum==2502);
  assert(w.audit_.runner_work.sum==9962);
  const std::string json=w.diagnosticsJson().c_str();
  assert(json.find("\"runner_mean_us\":2490.500")!=std::string::npos);
  assert(json.find("\"deadline_input_cohorts\"")!=std::string::npos);
  w.beginRunAudit();
  assert(w.audit_.sample_completion.count==0);
  for(const auto& group:w.audit_.input_cohorts)for(const auto& c:group)assert(c.count==0&&c.sum==0);
  std::cout<<"permanent owner, command exclusion, STOP precedence, sealed audit and wrap PASS\n";
  std::cout<<"Completion input cohorts partition totals, preserve strict budget/wrap/gates and reset PASS\n";
}
