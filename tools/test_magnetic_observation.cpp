#include <cassert>
#include <cmath>
#include <iostream>
#include "magnetic_observation_json.h"
#include "log_sample_encoder.h"
using namespace bmm150_observation;
struct Device {
  uint8_t reg[256]={}, aux[256]={}; bool fail_trim=false;
  Device() { reg[0x7d]=15;reg[0x4c]=0x4f;reg[0x4d]=0x42;aux[0x40]=0x32;
    aux[0x68]=1;aux[0x6a]=2;aux[0x6c]=3; }
  bool readRegister(uint8_t a,uint8_t* p,size_t n) {
    if(a==4){if(fail_trim&&reg[0x4d]==0x62)return false;*p=aux[reg[0x4d]];return true;}
    for(size_t i=0;i<n;++i) { p[i]=reg[a+i]; }
    return true;
  }
  bool writeRegister8(uint8_t a,uint8_t v){reg[a]=v;return true;}
};
int main() {
  for(bool failure:{false,true}) {
    Device d;d.fail_trim=failure;Trim t;bool restored=false;
    assert(readFactoryTrim(d,[](uint32_t){},t,restored)==!failure);
    assert(restored&&d.reg[0x7d]==15&&d.reg[0x4c]==0x4f&&d.reg[0x4d]==0x42);
  }
  Device absent;absent.reg[0x7d]=14;Trim tr;bool restored;
  assert(!readFactoryTrim(absent,[](uint32_t){},tr,restored)&&restored);
  auto invalid=magneticObservationJson(RawReading{},123);
  assert(std::string(invalid.c_str()).find(":nan")==std::string::npos);
  assert(std::string(invalid.c_str()).find("\"norm_uT\":null")!=std::string::npos);
  ExperimentStatus status;auto& m=status.magnetic;
  m.sample_us=0xfffffff0u;m.sequence=99;
  for(unsigned i=0;i<8;++i) m.aux[i]=uint8_t(i*31);
  RollerTelemetry motor;LogSample row{};float beta[Config::DYNAMIC_BETA_COUNT]={};
  encodeLogSample(row,status,motor,100,0,0,beta);
  assert(sizeof(row)==274&&row.mag_sequence==99&&row.mag_sample_us==0xfffffff0u);
  assert(memcmp(row.mag_aux,m.aux,8)==0);
  auto fresh=magneticObservationJson(m,100),stale=magneticObservationJson(m,300000);
  assert(std::string(fresh.c_str()).find("\"fresh\":true")!=std::string::npos);
  assert(std::string(stale.c_str()).find("\"fresh\":false")!=std::string::npos);
  std::cout<<"magnetic observation: aux restore, absent/failure, null JSON, binary tail, timestamp wrap/stale PASS\n";
}
