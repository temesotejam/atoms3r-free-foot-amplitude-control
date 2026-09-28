#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

// BMM150 factory compensation, adapted from Bosch Sensortec BMM150_SensorAPI.
// See THIRD_PARTY_BMM150_LICENSE.txt and docs/MAGNETOMETER_04727.md.
// This corrects sensor sensitivity/temperature; it is NOT hard/soft-iron calibration.
namespace bmm150_observation {
struct Trim {
  int8_t x1=0, y1=0, x2=0, y2=0, xy2=0;
  uint8_t xy1=0;
  uint16_t z1=0, xyz1=0;
  int16_t z2=0, z3=0, z4=0;
  bool valid() const { return z1 && z2 && xyz1; }
};
// Written during synchronous boot only; immutable before task creation.
inline Trim& startupTrimStorage() { static Trim trim; return trim; }
struct RawReading {
  uint32_t sequence=0, sample_us=0;
  uint8_t aux[8]={};
};
inline uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1])<<8; }
inline int16_t i16(const uint8_t* p) { uint16_t u=u16(p); int16_t s; memcpy(&s,&u,2); return s; }
inline Trim decodeTrim(const uint8_t* a, const uint8_t* b, const uint8_t* c) {
  Trim t;
  t.x1=int8_t(a[0]); t.y1=int8_t(a[1]); t.x2=int8_t(b[2]); t.y2=int8_t(b[3]);
  t.z4=i16(b); t.z2=i16(c); t.z1=u16(c+2); t.xyz1=u16(c+4)&0x7fff;
  t.z3=i16(c+6); t.xy2=int8_t(c[8]); t.xy1=c[9]; return t;
}
struct Reading {
  uint32_t sequence=0, sample_us=0;
  int16_t raw_x=0, raw_y=0, raw_z=0;
  uint16_t rhall=0;
  bool factory_ok=false, valid=false;
  // Robot/MEKF body frame, microtesla. BMM die -> M5 (-x,+y,-z), then
  // M5 -> robot (-x,+y,-z): the two rotations cancel for this board only.
  float x_uT=NAN, y_uT=NAN, z_uT=NAN, norm_uT=NAN;
};
inline Reading compensate(const uint8_t* p, const Trim& t) {
  Reading r; r.factory_ok=t.valid();
  r.raw_x=int16_t(int16_t(int8_t(p[1]))*32 + (p[0]>>3));
  r.raw_y=int16_t(int16_t(int8_t(p[3]))*32 + (p[2]>>3));
  r.raw_z=int16_t(int16_t(int8_t(p[5]))*128 + (p[4]>>1));
  r.rhall=(uint16_t(p[7])<<6) | (p[6]>>2);
  if (!t.valid() || !r.rhall || r.raw_x==-4096 || r.raw_y==-4096 || r.raw_z==-16384) return r;
  const float a=float(t.xyz1)*16384.0f/r.rhall - 16384.0f;
  const float b=float(t.xy2)*(a*a/268435456.0f) + a*float(t.xy1)/16384.0f + 256.0f;
  r.x_uT=(r.raw_x*(b*(float(t.x2)+160.0f))/8192.0f + float(t.x1)*8.0f)/16.0f;
  r.y_uT=(r.raw_y*(b*(float(t.y2)+160.0f))/8192.0f + float(t.y1)*8.0f)/16.0f;
  const float den=float(t.z2) + float(t.z1)*float(r.rhall)/32768.0f;
  if (fabsf(den)<1.0e-6f) return r;
  r.z_uT=(((float(r.raw_z)-float(t.z4))*131072.0f - float(t.z3)*(float(r.rhall)-float(t.xyz1)))/(den*4.0f))/16.0f;
  r.norm_uT=sqrtf(r.x_uT*r.x_uT+r.y_uT*r.y_uT+r.z_uT*r.z_uT);
  r.valid=isfinite(r.x_uT)&&isfinite(r.y_uT)&&isfinite(r.z_uT)&&isfinite(r.norm_uT);
  return r;
}

// Startup only, before exclusive runtime I2C ownership. Preserve the exact hub
// configuration and power register, including on a failed trim read. No runtime
// register writes or extra data reads are introduced by this observer.
template<class Device, class Delay>
bool readFactoryTrim(Device& dev, Delay delayMs, Trim& trim, bool& restored) {
  trim=Trim{}; restored=true;
  uint8_t power=0, mode=0, addr=0;
  if (!dev.readRegister(0x7d,&power,1) || !dev.readRegister(0x4c,&mode,1) ||
      !dev.readRegister(0x4d,&addr,1)) return false;
  if (!(power&1u)) return false; // M5 begin did not identify BMM150.
  const auto idle = [&]() {
    for (unsigned i=0;i<20;++i) {
      uint8_t status=0;
      if (!dev.readRegister(0x03,&status,1)) return false;
      if (!(status&4u)) return true;
      delayMs(1);
    }
    return false;
  };
  bool ok=dev.writeRegister8(0x7d,power&~1u);
  delayMs(2);
  ok=ok && idle() && dev.writeRegister8(0x4c,0x80);
  const auto auxRead = [&](uint8_t reg,uint8_t* out,size_t n) {
    for (size_t i=0;i<n;++i) {
      if (!idle() || !dev.writeRegister8(0x4d,uint8_t(reg+i))) return false;
      delayMs(2); // allow the manual transaction to start before testing busy.
      if (!idle() || !dev.readRegister(0x04,out+i,1)) return false;
    }
    return true;
  };
  uint8_t id=0,a[2]={},b[4]={},c[10]={};
  ok=ok && auxRead(0x40,&id,1) && id==0x32 && auxRead(0x5d,a,2) &&
      auxRead(0x62,b,4) && auxRead(0x68,c,10);
  // Do not short-circuit restoration writes.
  const bool r0=idle();
  const bool r1=dev.writeRegister8(0x4d,addr);
  delayMs(2);
  const bool r2=idle();
  const bool r3=dev.writeRegister8(0x4c,mode);
  const bool r4=dev.writeRegister8(0x7d,power);
  uint8_t p=0,m=0,d=0;
  restored=r0&&r1&&r2&&r3&&r4&&dev.readRegister(0x7d,&p,1)&&
      dev.readRegister(0x4c,&m,1)&&dev.readRegister(0x4d,&d,1)&&p==power&&m==mode&&d==addr;
  if (ok && restored) trim=decodeTrim(a,b,c);
  return trim.valid();
}
}
