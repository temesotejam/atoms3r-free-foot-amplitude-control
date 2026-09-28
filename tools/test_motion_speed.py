#!/usr/bin/env python3
"""Run real motion/decision/pulse methods with frozen 0.47.24 and current flags.
Hardware I/O is stubbed; fixed clocks exclude scheduler/timing claims. Compare
all scalar event fields and control state, not struct padding or approximate floats.
"""
from pathlib import Path
import json,re,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
frozen=json.loads((ROOT/'tools/fixtures/motion_04724.json').read_text())
source=(ROOT/'src/experiment_runner.cpp').read_text()
def block(text,signature):
    a=text.index(signature);b=text.index('{',a)+1;depth=1
    while depth:
        depth+=(text[b]=='{')-(text[b]=='}');b+=1
    return text[a:b]
def method(name):
    m=re.search(r'^(?:void|bool|float) ExperimentRunner::'+name+r'\(',source,re.M)
    return block(source,m.group())
logger=(ROOT/'src/psram_logger.h').read_text()
audit=(ROOT/'src/solver_audit.h').read_text()
scalar=r'(?:bool|float|uint(?:8|16|32)_t|int(?:8|16|32)_t)'
def serializer(typ,body):
    fields=re.findall(r'\b'+scalar+r'\s+(\w+)\s*=',body)
    assert fields,typ
    return 'void emit(const '+typ+'& v){'+''.join('emit(v.'+f+');' for f in fields)+'}\n'
cpp=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <Arduino.h>
#include <Adafruit_AHRS.h>
#include "control_work_profile.h"
#include "control_latency.h"
#include "rate_baseline_correction.h"
#include "previous_peak_control_correction.h"
#define private public
#include "experiment_runner.h"
#undef private
static unsigned commands=0,stops=0,zero_events=0,peak_events=0,outputs=0,rejected=0;
static bool roller_ok=true,write_ok=true,quiet=false;
static PsramLogger::EnergyControlAutonomousPeakEvent last_peak;
static PsramLogger::EnergyControlAutonomousZeroCrossEvent last_zero;
template<class T> void emit(const T& v){if(quiet)return;assert(std::fwrite(&v,sizeof(v),1,stdout)==1);}
void emit(float v){uint32_t b;if(std::isnan(v))b=0x7fc00000U;else std::memcpy(&b,&v,4);emit(b);}
'''
for name in ['EnergyControlAutonomousPeakEvent','EnergyControlAutonomousZeroCrossEvent','TimingProbeEvent']:
    cpp+=serializer('PsramLogger::'+name,block(logger,'struct '+name))
cpp+=serializer('solver_audit::Record',block(audit,'struct Record'))
cpp+=r'''
bool Roller485Manager::ok()const{return roller_ok;}
bool Roller485Manager::setCurrentMa(int16_t v){++commands;emit(v);return write_ok;}
bool Roller485Manager::stop(){++stops;return true;}
RollerTelemetry Roller485Manager::telemetrySnapshot()const{
  RollerTelemetry t;t.battery_mV=7660;t.actual_current_mA=12;t.speed_rpm=-12.5f;
  t.current_valid=t.speed_valid=true;t.current_sample_time_us=host_us-700;t.speed_sample_time_us=host_us-1200;return t;
}
void PsramLogger::addEnergyControlAutonomousPeakEvent(const EnergyControlAutonomousPeakEvent& e){++peak_events;last_peak=e;emit(e);}
void PsramLogger::addEnergyControlAutonomousZeroCrossEvent(const EnergyControlAutonomousZeroCrossEvent& e){last_zero=e;++zero_events;outputs+=e.output_executed;rejected+=!e.valid;emit(e);}
void PsramLogger::addTimingProbeEvent(const TimingProbeEvent& e){emit(e);}
void ExperimentRunner::requestEmergencyStop(const char*){status_.emergency_stop=true;stopMotor();}
void state(const ExperimentRunner& r){
'''
status_fields='state emergency_stop pulse_active pulse_id current_mA_setting pulse_width_ms_setting motor_cmd_mA pulse_direction beta_model_vbat_status beta_model_vbat_mV predicted_i_goal_mA predicted_peak_current_mA predicted_beta_min current_audit_q_target_mA_s current_audit_q_pred_mA_s'.split()
cpp+=''.join('emit(r.status_.'+f+');\n' for f in status_fields)
fields=re.findall(r'\b'+scalar+r'\s+(energy_control_autonomous_\w+_)\s*=',frozen['header'])
fields+=['energy_control_autonomous_phase_','energy_control_autonomous_half_cycle_state_',
         'predicted_signed_current_end_mA_','predicted_current_end_ms_',
         'active_pulse_start_ms_','active_pulse_start_test_ms_','last_pulse_end_ms_',
         'next_pulse_start_test_ms_','timing_probe_pending_']
cpp+=''.join('emit(r.'+f+');\n' for f in fields)+'emit(r.timing_probe_event_);}\n'
cpp+=r'''
using Phase=ExperimentRunner::EnergyControlAutonomousPhase;
using Half=ExperimentRunner::EnergyControlAutonomousHalfCycleState;
static uint32_t seed=4725;
float noise(){seed=1664525U*seed+1013904223U;return float(seed>>8)/16777216.0f;}
void setup(ExperimentRunner& r,PsramLogger& log,ImuManager& imu,Roller485Manager& roller){
 r.logger_=&log;r.imu_=&imu;r.roller_=&roller;
 r.status_.state=ExperimentState::RUNNING_BATCH_SWEEP;
 r.energy_control_autonomous_mode_=true;r.energy_control_autonomous_phase_=Phase::ENERGY_CONTROL;
 r.energy_control_autonomous_half_cycle_state_=Half::WAIT_ZERO_CROSS;
 r.energy_control_autonomous_last_peak_valid_=true;r.run_start_ms_=1000;
 r.status_.beta_model_vbat_mV=7670;r.status_.roller_battery_mV=7660;
}
int main(){
 PsramLogger log;ImuManager imu;Roller485Manager roller;
 solver_audit::Buffer<128> audits;log.solver_audit_=&audits;log.sealed_=false;
 // Independent accepted crossings, all integer widths, both sides, residual
 // current signs, fresh voltage changes, clipping, invalid state and I/O failure.
 for(unsigned i=0;i<20000;++i){
  audits.clear();
  ExperimentRunner r;setup(r,log,imu,roller);host_us=14000000;
  imu.reading_.gyro_sequence=i+1;imu.reading_.last_gyro_update_us=host_us-500;
  const int side=i%2?1:-1;const float rate=side*(10+110*noise());
  r.energy_control_autonomous_target_peak_deg_=1+17*noise();
  r.energy_control_autonomous_last_peak_amplitude_deg_=2+18*noise();
  r.energy_control_autonomous_last_peak_side_=-side;
  r.energy_control_autonomous_integral_plus_mA_s_=60*noise()-30;
  r.energy_control_autonomous_integral_minus_mA_s_=60*noise()-30;
  r.predicted_signed_current_end_mA_=600*noise()-300;
  r.predicted_current_end_ms_=millis()-unsigned(400*noise());
  r.status_.beta_model_vbat_mV=5000+unsigned(4000*noise());
  r.status_.roller_battery_mV=i%19?5000+unsigned(4000*noise()):0;
  roller_ok=i%17!=0;write_ok=i%23!=0;
  if(i%31==0)r.energy_control_autonomous_last_peak_valid_=false;
  if(i%37==0)r.status_.emergency_stop=true;
  if(i%41==0)r.energy_control_autonomous_target_peak_deg_=NAN;
  if(i%43==0)r.predicted_signed_current_end_mA_=INFINITY;
  if(i%47==0)r.energy_control_autonomous_integral_plus_mA_s_=NAN;
  r.updateEnergyControlAutonomousAtZeroCross(13000,rate,-side*0.1f,side*0.1f,0.5f,13000.f);
  emit(audits.count());for(unsigned k=0;k<audits.count();++k)emit(audits.at(k));
  state(r);host_us+=r.status_.pulse_width_ms_setting*1000U;
  r.updateEnergyControlAutonomousPulse(millis());state(r);
  // Normal begin guards and start-kick model use the same production methods.
  if(i<204){
   ExperimentRunner p;setup(p,log,imu,roller);roller_ok=write_ok=true;
   const unsigned width=i%102;
   emit(p.beginEnergyControlAutonomousPulse(millis(),13000,side,width));state(p);
   p.stopActivePulse(millis());p.energy_control_autonomous_phase_=Phase::STRONG_START_KICK;
   emit(p.beginEnergyControlAutonomousStartKickPulse(millis(),Config::ENERGY_CONTROL_AUTONOMOUS_START_KICK_DIRECTION));state(p);
  }
 }
 // Consecutive MEKF samples: peak acceptance, projected central crossing,
 // history during a live pulse, end/rearm and side-integral state evolution.
 ExperimentRunner r;setup(r,log,imu,roller);roller_ok=write_ok=true;
 r.energy_control_autonomous_phase_=Phase::WAIT_FIRST_PEAK;
 r.energy_control_autonomous_half_cycle_state_=Half::WAIT_PEAK;
 r.resetEnergyControlAutonomousPeakTracker(true);host_us=2000000;r.run_start_ms_=millis();
 for(unsigned i=0;i<32000;++i){
  audits.clear();
  host_us+=2500;const float phase=i*0.037f;
  const float rate=8*0.037f/0.0025f*cosf(phase);
  imu.reading_.gyro_sequence=i+1;imu.reading_.last_gyro_update_us=host_us;
  imu.reading_.gy_dps=rate/Config::MEKF_GYRO_Y_SCALE;
  r.status_.pitch_mekf_measurement_relative_deg=8*sinf(phase);
  r.status_.pitch_mekf_detector_relative_deg=r.status_.pitch_mekf_measurement_relative_deg+rate*0.003f;
  r.updateEnergyControlAutonomousMotion(millis());
  emit(audits.count());for(unsigned k=0;k<audits.count();++k)emit(audits.at(k));
  r.updateEnergyControlAutonomousPulse(millis());state(r);
  r.timing_probe_loop_captured_=r.timing_probe_log_captured_=r.timing_probe_imu_captured_=true;
  r.maybeFinalizeTimingProbe();
 }
 assert(outputs>100 && rejected>100 && peak_events>10 && commands>100 && stops>100);
#ifdef STEERING_INTEGRATION
 quiet=true;
 ExperimentRunner steered;setup(steered,log,imu,roller);roller_ok=write_ok=true;
 steered.energy_control_autonomous_target_peak_deg_=10;
 // Establish nonzero steering through the actual cycle controller.
 for(unsigned i=0;i<20;++i){
   steered.steering_.peak(1,9,float(i)*2,i*1000+500,false,false);
   steered.steering_.peak(-1,11,float(i)*2+1,i*1000+1000,false,false);
 }
 assert(steered.steering_.state().delta_deg>0);
 steered.energy_control_autonomous_last_peak_valid_=true;
 steered.energy_control_autonomous_last_peak_amplitude_deg_=10;
 steered.energy_control_autonomous_last_peak_side_=-1;
 steered.energy_control_autonomous_last_peak_ms_=12500;
 steered.energy_control_autonomous_last_accepted_zero_cross_valid_=false;
 steered.energy_control_autonomous_zero_cross_consumed_for_peak_=false;
 steered.energy_control_autonomous_phase_=Phase::ENERGY_CONTROL;
 steered.energy_control_autonomous_half_cycle_state_=Half::WAIT_ZERO_CROSS;
 steered.updateEnergyControlAutonomousAtZeroCross(13000,60,-.1f,.1f,.5f,13000.f);
 assert(steered.energy_control_autonomous_pending_peak_);
 const float chosen=steered.steering_pending_target_deg_;
 assert(chosen>10 && chosen<=11);
 assert(steered.status_.pulse_width_ms_setting<=100);
 // A later outer update cannot reinterpret the target of an issued command.
 steered.steering_.reset();
 steered.status_.pulse_active=false;
 steered.energy_control_autonomous_phase_=Phase::ENERGY_CONTROL;
 steered.energy_control_autonomous_half_cycle_state_=Half::WAIT_PEAK;
 assert(steered.recordEnergyControlAutonomousPeak(13500,1,9,9));
 assert(last_peak.pending_command_matched && last_peak.target_peak_deg==chosen);
 assert(last_peak.peak_error_deg==chosen-9);
 std::fprintf(stderr,"Production steering target -> bounded pulse -> latched response error PASS\n");
 // Regression: shifting an 8-degree side target must not silently disable
 // the existing previous-peak residual model. Exercise the production path.
 for(float mean : {8.f,10.f,12.f})for(uint16_t run : {uint16_t(1),uint16_t(2)})
 for(int side : {-1,1})for(unsigned time : {15000U,24000U}) {
   ExperimentRunner p;setup(p,log,imu,roller);roller_ok=write_ok=true;
   p.energy_control_autonomous_target_peak_deg_=mean;
   p.steering_.reset(steering::Mode::ResponseCheck,run);
   p.steering_.peak(-1,mean,0,0,false,false);
   for(unsigned ms=1000;ms<=time;ms+=1000) {
     p.steering_.peak(1,mean,ms*.001f-.5f,ms-500,false,false);
     p.steering_.peak(-1,mean,ms*.001f,ms,false,false);
   }
   const float delta=p.steering_.state().delta_deg;
   assert(fabsf(delta)==.2f);
   p.energy_control_autonomous_last_peak_amplitude_deg_=8;
   p.energy_control_autonomous_last_peak_side_=-side;
   p.energy_control_autonomous_last_peak_ms_=time-500;
   host_us=(time+p.run_start_ms_)*1000U;
   p.updateEnergyControlAutonomousAtZeroCross(time,side*60.f,-side*.1f,side*.1f,.5f,float(time));
   assert(last_zero.valid && last_zero.physical_next_peak_side==side);
   assert(last_zero.target_peak_deg==mean+side*delta);
   const float residual=last_zero.free_next_peak_amplitude_deg-last_zero.rate_baseline_peak_deg;
   const float expected=mean==8 ? (side>0?.591392151f:-.157912422f) : 0.f;
   assert(fabsf(residual-expected)<1e-5f);
   assert(last_zero.pulse_width_ms<=100 && abs(last_zero.command_current_mA)<=300);
   const float issued=last_zero.target_peak_deg;
   p.steering_.reset();p.status_.pulse_active=false;
   p.energy_control_autonomous_phase_=Phase::ENERGY_CONTROL;
   p.energy_control_autonomous_half_cycle_state_=Half::WAIT_PEAK;
   assert(p.recordEnergyControlAutonomousPeak(time+300,side,mean,side*mean));
   assert(last_peak.pending_command_matched && last_peak.target_peak_deg==issued);
 }
 std::fprintf(stderr,"Production 8-degree mean gate with both response signs/sides; 10/12 bypass; bounded output and command latching PASS\n");
#endif

 std::fprintf(stderr,"20000 decision states + 32000 sequential samples; outputs=%u rejected=%u peaks=%u\n",outputs,rejected,peak_events);
}
'''
with tempfile.TemporaryDirectory(prefix='motion-speed-') as tmp:
    tmp=Path(tmp);results=[]
    for mode in ['reference','actual']:
        p=tmp/mode;p.mkdir()
        for f in (ROOT/'src').iterdir():
            if f.suffix in ('.h','.hpp'):shutil.copyfile(f,p/f.name)
        if mode=='reference':
            (p/'experiment_runner.h').write_text(frozen['header'])
            (p/'direct_q_solver.h').write_text(frozen['direct_q'])
            methods=frozen['methods'].values()
        else:methods=[method(n) for n in frozen['methods']]
        (p/'test.cpp').write_text(cpp+'\n'+'\n'.join(methods))
        command=['g++','-std=c++17','-Os','-ffp-contract=off','-Wall','-Wextra','-Werror',
                 '-I'+str(p),'-I'+str(ROOT/'tools/host_v46o'),'-I'+str(ROOT/'src'),str(p/'test.cpp'),
                 str(ROOT/'src/mekf6.cpp'),str(ROOT/'src/control_latency.cpp'),
                 str(ROOT/'tools/fixtures/adafruit_ahrs_2_4_0/Adafruit_AHRS_Madgwick.cpp'),
                 *(['-DSTEERING_INTEGRATION=1'] if mode!='reference' else []),
                 '-o',str(p/'test')]
        subprocess.run(command,check=True)
        results.append(subprocess.check_output([str(p/'test')]))
    assert results[0]==results[1], 'Real decision/pulse/state outputs differ from 0.47.24'
    print(f'0.47.24/current production methods: {len(results[0])} serialized bytes exact (NaNs canonicalized); scheduling/physical timing unverified PASS')
