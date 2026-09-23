#include "host_v46o/Arduino.h"
#include <fstream>
#include <cassert>
#include <iostream>
#define private public
#include "../src/psram_logger.h"
#include "../src/foot_observer.h"
#include "../src/immutable_export.h"
#undef private
#include "../src/imu_manager.h"
#include "../src/roller485_manager.h"
ImuManager imu;
Roller485Manager roller;
RunControlWorker run_control;
FootObserver feet;
RollerTelemetry Roller485Manager::telemetrySnapshot() const {return {};}
void ImuManager::appendAcquisitionDiagnostics(PsramString& json) const {json += "{\"host_fixture\":true}";}
// Camera and RTOS are not exercised here; actual serializers/export code are.
int main(int argc,char** argv){
  assert(argc==2);
  PsramLogger logger;assert(logger.begin());
  assert(sizeof(PsramLogger)<4096);
  assert(PsramLogger::eventStorageBytes()==190144);
  logger.startRun(1,123456,300,100,1000,false,0,800,0,false,NAN,false,0,false,true);
  LogSample row{};row.motor_cmd_mA=300;row.roller_actual_current_mA=270;
  assert(logger.addSample(row));assert(!logger.rwlogDownloadable());
  for(unsigned i=0;i<256;++i){
    PsramLogger::EnergyControlAutonomousPeakEvent p{};p.peak_amplitude_deg=8;
    logger.addEnergyControlAutonomousPeakEvent(p);
    PsramLogger::EnergyControlAutonomousZeroCrossEvent z{};z.zero_cross_time_ms=i*100;
    z.pre_input_measured_current_mA=17;z.pre_input_wheel_speed_rpm=123.5;
    logger.addEnergyControlAutonomousZeroCrossEvent(z);
    logger.addTimingProbeEvent({});
  }
  for(unsigned i=0;i<128;++i)logger.addSolverAuditEvent({});
  feet.frames_=static_cast<FootFrame*>(ps_malloc(sizeof(FootFrame)*FootObserver::kCapacity));
  feet.status_.available=true;feet.status_.zero_ready=true;feet.status_.count=FootObserver::kCapacity;
  for(unsigned i=0;i<FootObserver::kCapacity;++i){
    FootFrame f{};f.sequence=i;f.run_id=1;f.frame_us=1000000+i*66667;
    f.delivered_us=f.frame_us+30000;f.log_time_us=f.frame_us-123456;
    f.right_valid=f.left_valid=true;f.right_deg=5;f.left_deg=6;
    f.frame_valid=f.timestamp_valid=f.zero_ready=true;
    f.right_scan_y=42;f.left_scan_y=184;f.right_weight=3200;f.left_weight=3100;
    f.right_contrast=200;f.left_contrast=190;
    f.right_reason=f.left_reason=MarkerDetectionReason::Detected;f.right_templates=f.left_templates=17;
    if(i==1){f.right_valid=false;f.right_deg=NAN;f.right_scan_y=NAN;f.right_reason=MarkerDetectionReason::LowContrast;}
    feet.frames_[i]=f;
  }
  logger.markMeasurementDone();assert(!logger.rwlogDownloadable());
  logger.seal();assert(logger.rwlogDownloadable());
  assert(!logger.addSample(row));logger.addTimingProbeEvent({});assert(logger.timing_probe_event_count_==256);
  ImmutableExport exportFile;assert(exportFile.begin(logger));assert(exportFile.prepare());
  assert(exportFile.prepare());assert(!exportFile.reset());
  exportFile.build();const auto status=exportFile.status();
  std::cerr<<"metadata bytes="<<exportFile.metadata_.length()<<" phase="<<int(status.phase)<<" error="<<status.error<<"\n";
  assert(status.phase==ImmutableExport::Phase::Ready);
  assert(exportFile.prepare());assert(exportFile.status().token==std::string(status.token));
  std::ofstream out(argv[1],std::ios::binary);uint8_t bytes[4096];
  for(uint32_t offset=0;offset<status.bytes;){
    const auto n=std::min<uint32_t>(4096,status.bytes-offset);
    assert(exportFile.chunk(status.token,offset,n,bytes));out.write(reinterpret_cast<char*>(bytes),n);offset+=n;
  }
  out.close();assert(!exportFile.chunk("wrong_token",0,4,bytes));
  assert(exportFile.reset());assert(!exportFile.chunk(status.token,0,4,bytes));
  std::cout<<"maximum autonomous events + 768 foot frames: metadata="<<exportFile.header_.metadata_json_size
      <<" bytes, event PSRAM="<<PsramLogger::eventStorageBytes()<<", logger object="<<sizeof(logger)<<" bytes PASS\n";
}
